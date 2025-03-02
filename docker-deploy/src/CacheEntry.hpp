#ifndef CACHEENTRY_HPP
#define CACHEENTRY_HPP

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
        string cacheKey;
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
        bool isExpiredByAge(int maxAge) const;
        bool needsRevalidation() const;
        string getFullResponse() const;
        string getResponseLine() const;
        string getResponseHeaders() const;
        string getResponseBody() const;
        string getETag() const;
        string getLastModified() const;
        time_t getExpiresTime() const;
        int getAge() const;
        int getRestTime() const;
        int getStaleTime() const;
};
    

#endif