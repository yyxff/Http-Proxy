#include "Cache.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <ctime>

// singleton get instance
// Cache & Cache::getInstance() {
//     static Cache instance;
//     return instance;
// }

Cache::Cache(size_t max_size) : max_size(max_size), current_size(0) {}

Cache::CacheStatus Cache::checkStatus(const string& url) {
    lock_guard<mutex> lock(cache_mutex);
    
    auto it = cache_map.find(url);
    if (it == cache_map.end()) {
        return NOT_IN_CACHE;
    }
    
    const CacheEntry& entry = it->second;
    
    if (entry.isExpired()) {
        return IN_CACHE_EXPIRED;
    }
    
    if (entry.needsRevalidation()) {
        return IN_CACHE_NEEDS_VALIDATION;
    }
    
    return IN_CACHE_VALID;
}

Cache::CacheStatus Cache::checkExpiredByAge(const string& url, int age) {
    lock_guard<mutex> lock(cache_mutex);
    auto it = cache_map.find(url);
    if (it == cache_map.end()) {
        return NOT_IN_CACHE;
    }
    const CacheEntry& entry = it->second;
    if (entry.isExpiredByAge(age)){
        return IN_CACHE_EXPIRED;
    }
    return IN_CACHE_VALID;

}

void Cache::addToCache(const string& url, 
                       const string& response_line, 
                       const string& response_headers, 
                       const string& response_body) {
    if (!isCacheable(response_line, response_headers)) {
        logger.debug("Response not cacheable for URL: " + url);
        return;
    }
    
    lock_guard<mutex> lock(cache_mutex);
    
    // calculate the size of the new entry
    size_t entry_size = response_line.size() + response_headers.size() + response_body.size();
    
    // if the entry is too large, do not cache
    if (entry_size > max_size) {
        logger.warning("Response too large to cache: " + url + " (" + to_string(entry_size) + " bytes)");
        return;
    }
    
    // if the entry already exists, remove the old entry
    auto it = cache_map.find(url);
    if (it != cache_map.end()) {
        current_size -= (it->second.getResponseLine().size() + 
                        it->second.getResponseHeaders().size() + 
                        it->second.getResponseBody().size());
        cache_map.erase(it);
    }
    
    // ensure there is enough space
    while (current_size + entry_size > max_size && !cache_map.empty()) {
        evictOldestEntry();
    }
    
    // create and add the new entry
    CacheEntry entry(response_line, response_headers, response_body);
    
    // use insert instead of operator[]
    // cache_map.insert(std::make_pair(url, entry));
    cache_map.insert_or_assign(url, entry);

    // or use emplace
    // cache_map.emplace(url, entry);
    
    current_size += entry_size;
    
    // update the expiry time map
    updateExpiryMap(url, entry.getExpiresTime());
    
    time_t expires_time = entry.getExpiresTime();
    logger.debug("Added to cache: " + url + " (expires: " + 
                string(ctime(&expires_time)) + ")");
    logger.debug("now cache has "+to_string(cache_map.size())+" entries");
}

CacheEntry* Cache::getEntry(const string& url) {
    lock_guard<mutex> lock(cache_mutex);
    
    auto it = cache_map.find(url);
    if (it != cache_map.end()) {
        return &(it->second);
    }
    
    return nullptr;
}

void Cache::removeEntry(const string& url) {
    lock_guard<mutex> lock(cache_mutex);
    
    auto it = cache_map.find(url);
    if (it != cache_map.end()) {
        current_size -= (it->second.getResponseLine().size() + 
                        it->second.getResponseHeaders().size() + 
                        it->second.getResponseBody().size());
        cache_map.erase(it);
        logger.debug("Removed from cache: " + url);
    }
}

size_t Cache::getCurrentSize() const {
    // do not use lock, because this is a simple getter method
    return current_size;
}

void Cache::evictOldestEntry() {
    if (cache_map.empty()) {
        return;
    }
    
    // simple strategy: find the entry with the earliest expiry time
    time_t oldest_time = time(nullptr) + 3600*24*365; // one year later
    string oldest_url;
    
    for (const auto& pair : cache_map) {
        if (pair.second.getExpiresTime() < oldest_time) {
            oldest_time = pair.second.getExpiresTime();
            oldest_url = pair.first;
        }
    }
    
    if (!oldest_url.empty()) {
        logger.info("(no-id): NOTE evicted " + oldest_url + " from cache");
        removeEntry(oldest_url);
    }
}

void Cache::removeExpiredEntries() {
    lock_guard<mutex> lock(cache_mutex);
    
    time_t now = time(nullptr);
    vector<string> to_remove;
    
    for (const auto& pair : cache_map) {
        if (pair.second.getExpiresTime() <= now) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& url : to_remove) {
        removeEntry(url);
    }
}

void Cache::updateExpiryMap(const string& url, time_t expires_time) {
    expiry_map[expires_time] = url;
}

bool Cache::isCacheable(const string& response_line, const string& response_headers) {

    if (response_line.find("200") == string::npos) {
        return false;
    }
    
    if (response_headers.find("Cache-Control: no-store") != string::npos ||
        response_headers.find("Cache-Control: private") != string::npos) {
        return false;
    }
    
    return true;
}

bool Cache::isCacheable(const Response & response) {

    if (response.getResult() != 200) {
        logger.debug("not 200");
        return false;
    }

    if (response.getHeader("Cache-Control") == ""){
        logger.debug("has no cache control in response");
        return true;
    }
    
    if (response.getHeader("Cache-Control").find("no-store") != std::string::npos||
        response.getHeader("Cache-Control").find("private") != std::string::npos) {
        logger.debug("cache not allowed");
        return false;
    }
    
    return true;
}

time_t Cache::parseExpiresTime(const string& response_headers) {
    time_t now = time(nullptr);
    time_t expires = now + 3600; // default 1 hour later
    // time_t expires = now + 20; // default 20 seconds later
    
    // try to parse from Cache-Control: max-age
    regex max_age_regex("Cache-Control:.*?max-age=(\\d+)");
    smatch max_age_match;
    if (regex_search(response_headers, max_age_match, max_age_regex)) {
        int max_age = stoi(max_age_match[1]);
        expires = now + max_age;
        return expires;
    }
    
    // try to parse from Expires header
    regex expires_regex("Expires: (.+)");
    smatch expires_match;
    if (regex_search(response_headers, expires_match, expires_regex)) {
        string expires_str = expires_match[1];
        
        // parse HTTP date format
        struct tm tm = {};
        strptime(expires_str.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        time_t expires_time = mktime(&tm);
        
        // adjust to GMT
        expires_time -= timezone;
        
        if (expires_time > now) {
            expires = expires_time;
        }
    }
    
    return expires;
}

bool Cache::requiresRevalidation(const string& response_headers) {
    return response_headers.find("Cache-Control: must-revalidate") != string::npos ||
           response_headers.find("Cache-Control: no-cache") != string::npos;
}

string Cache::extractETag(const string& response_headers) {
    regex etag_regex("ETag: \"?([^\"\r\n]+)\"?", std::regex_constants::icase);
    smatch etag_match;
    if (regex_search(response_headers, etag_match, etag_regex)) {
        return etag_match[1];
    }
    return "";
}

time_t Cache::extractLastModified(const string& response_headers) {
    regex last_modified_regex("Last-Modified: (.+)");
    smatch last_modified_match;
    if (regex_search(response_headers, last_modified_match, last_modified_regex)) {
        string last_modified_str =  last_modified_match[1];
        // parse HTTP date format
        struct tm tm = {};
        strptime(last_modified_str.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        time_t last_modified_time = mktime(&tm);
        
        // adjust to GMT
        last_modified_time -= timezone;
        
        if (last_modified_time > time(nullptr)) {
            return time(nullptr);;
        }
        return last_modified_time;
    }
    return time(nullptr);;
}
