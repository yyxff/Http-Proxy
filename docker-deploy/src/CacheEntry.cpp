#include "Cache.hpp"

CacheEntry::CacheEntry(const string& response_line, 
    const string& response_headers, 
    const string& response_body
    ) 
:   response_line(response_line),
    response_headers(response_headers),
    response_body(response_body),
    creation_time(time(nullptr)),
    requires_revalidation(Cache::requiresRevalidation(response_headers)) {
    expires_time = Cache::parseExpiresTime(response_headers);
    etag = Cache::extractETag(response_headers);
    last_modified = Cache::extractLastModified(response_headers);
}

bool CacheEntry::isExpired() const {
return time(nullptr) > expires_time;
}

bool CacheEntry::isExpiredByAge(int maxAge) const {
return expires_time - creation_time > maxAge;
}

bool CacheEntry::needsRevalidation() const {
return requires_revalidation;
}

string CacheEntry::getFullResponse() const {
return response_headers + response_body;
}

string CacheEntry::getResponseLine() const {
return response_line;
}

string CacheEntry::getResponseHeaders() const {
return response_headers;
}

string CacheEntry::getResponseBody() const {
return response_body;
}

string CacheEntry::getETag() const {
return etag;
}

string CacheEntry::getLastModified() const {
return last_modified;
}

time_t CacheEntry::getExpiresTime() const {
return expires_time;
}

int CacheEntry::getAge() const {
    return time(nullptr) - creation_time;
}

int CacheEntry::getRestTime() const {
    return expires_time - time(nullptr);
}

int CacheEntry::getStaleTime() const {
    return - getRestTime();
}