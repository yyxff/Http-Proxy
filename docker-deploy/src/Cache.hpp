#ifndef CACHE_HPP
#define CACHE_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <map>
#include <vector>

#include "Logger.hpp"
#include "CacheEntry.hpp"
#include "Response.hpp"

using namespace std;

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

    static Cache & getInstance();
    
    Cache(size_t max_size = 10 * 1024 * 1024); // Default 10MB cache
    
    CacheStatus checkStatus(const string& url);

    CacheStatus checkExpiredByAge(const string& url, int age);

    void addToCache(const string& url, 
                    const string& response_line, 
                    const string& response_headers, 
                    const string& response_body);
    CacheEntry* getEntry(const string& url);
    void removeEntry(const string& url);
    size_t getCurrentSize() const;
    
    // Parse cache control headers to determine if the response is cacheable
    static bool isCacheable(const string& response_line, const string& response_headers);
    static bool isCacheable(const Response & response);
    static time_t parseExpiresTime(const string& response_headers);
    static bool requiresRevalidation(const string& response_headers);
    static string extractETag(const string& response_headers);
    static time_t extractLastModified(const string& response_headers);


};

#endif
