#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <map>
#include <vector>

#include "Logger.hpp"

using namespace std;

class CacheEntry {

private:
    string response_line;
    string response_headers;
    string response_body;
    time_t creation_time;
    time_t expires_time;
    bool requires_revalidation;
    string etag;
    string last_modified;
    
public:
    CacheEntry(const string& response_line, 
               const string& response_headers, 
               const string& response_body);
    
    bool isExpired() const;
    bool needsRevalidation() const;
    string getFullResponse() const;
    string getResponseLine() const;
    string getResponseHeaders() const;
    string getResponseBody() const;
    string getETag() const;
    string getLastModified() const;
    time_t getExpiresTime() const;
};

class Cache {
private:
    unordered_map<string, CacheEntry> cache_map;
    map<time_t, string> expiry_map;
    mutex cache_mutex;
    size_t max_size;
    size_t current_size;
    static inline Logger & logger = Logger::getInstance();
    
    void evictOldestEntry();
    void removeExpiredEntries();
    void updateExpiryMap(const string& url, time_t expires_time);
    
public:
    enum CacheStatus {
        NOT_IN_CACHE,
        IN_CACHE_VALID,
        IN_CACHE_EXPIRED,
        IN_CACHE_NEEDS_VALIDATION
    };
    
    Cache(size_t max_size = 10 * 1024 * 1024); // Default 10MB cache
    
    CacheStatus checkStatus(const string& url);
    void addToCache(const string& url, 
                    const string& response_line, 
                    const string& response_headers, 
                    const string& response_body);
    CacheEntry* getEntry(const string& url);
    void removeEntry(const string& url);
    size_t getCurrentSize() const;
    
    // Parse cache control headers to determine if the response is cacheable
    static bool isCacheable(const string& response_line, const string& response_headers);
    static time_t parseExpiresTime(const string& response_headers);
    static bool requiresRevalidation(const string& response_headers);
    static string extractETag(const string& response_headers);
    static string extractLastModified(const string& response_headers);
};

#endif
