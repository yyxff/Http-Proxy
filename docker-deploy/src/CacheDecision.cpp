#include "CacheDecision.hpp"

CacheDecision::Decision CacheDecision::makeDecision(const Request & request){
    string cacheControl = request.getHeader("Cache-Control");
    // no cache control
    if (cacheControl == ""){return CacheDecision::DIRECT;}

    Cache & cache = Cache::getInstance();
    Cache::CacheStatus cacheStatus = cache.checkStatus(request.getUrl());
    CacheEntry * entry = cache.getEntry(request.getUrl());
 
    // no entry
    if (cacheStatus == Cache::NOT_IN_CACHE){return CacheDecision::DIRECT;}

    // check every directive
    if (cacheControl.find("no-store") != string::npos){
        return CacheDecision::DIRECT;
    }
    if (cacheControl.find("no-cache") != string::npos){
        return CacheDecision::REVALIDATE;
    }
    if (cacheControl.find("only-if-cached") != string::npos){
        if (cacheStatus == Cache::IN_CACHE_VALID){
            return CacheDecision::RETURN_CACHE;
        }else{
            return CacheDecision::RETURN_504;
        }
    }
    if (cacheControl.find("max-age") != string::npos){
        return handle_max_age(cacheControl, entry);
    }
    if (cacheControl.find("min-fresh") != string::npos){
        return handle_min_fresh(cacheControl, entry);
    }
    if (cacheControl.find("max-stale") != string::npos){
        return handle_max_stale(cacheControl, entry);
    }
    if (cacheControl.find("no-transform") != string::npos){
        return CacheDecision::NO_TRANSFORM;
    }
    return CacheDecision::DIRECT;
}

CacheDecision::Decision CacheDecision::handle_max_age(const string & cacheControl, const CacheEntry * entry){
    int max_age = parseTime(cacheControl, "max-age");
    int entryAge = entry->getAge();

    // if no max stale
    if (entryAge <= max_age){
        if (cacheControl.find("min-fresh") != string::npos){
            return handle_min_fresh(cacheControl, entry);
        }
        return CacheDecision::RETURN_CACHE;
    }else{
        if (cacheControl.find("max-stale") != string::npos){
            return handle_max_stale(cacheControl, entry);
        }
        return CacheDecision::DIRECT;
    }
}

CacheDecision::Decision CacheDecision::handle_min_fresh(const string & cacheControl, const CacheEntry * entry){
    int min_fresh = parseTime(cacheControl, "min-fresh");
    int restTime = entry->getRestTime();

    // if no max stale
    if (restTime <= min_fresh){
        return CacheDecision::REVALIDATE;
    }else{
        return CacheDecision::RETURN_CACHE;
    }
}

CacheDecision::Decision CacheDecision::handle_max_stale(const string & cacheControl, const CacheEntry * entry){
    int max_stale = parseTime(cacheControl, "max-stale");
    int staleTime = entry->getStaleTime();

    // if no max stale
    if (staleTime <= max_stale){
        return CacheDecision::RETURN_CACHE;
    }else{
        return CacheDecision::REVALIDATE;
    }
}

// CacheDecision::Decision CacheDecision::handle_no_cache_contorl(const string & cacheControl, const CacheEntry * entry){
//     // Pragma
//     if (cacheControl.find("Pragma: no-cache") != string::npos) {
//         return CacheDecision::DIRECT
//     }

//     // Expires
//     if (cacheControl.find("Expires") != string::npos) {
//         return entry->isExpired() ? CacheDecision::DIRECT : CacheDecision::RETURN_CACHE;
//     }

//     // Modified
//     if (cacheControl.find("Last-Modified") != string::npos) {
//         return entry->isModifiedAfter() ? CACHEABLE : BYPASS;
//     }

//     // Direct
//     return CacheDecision::DIRECT;
// }

int CacheDecision::parseTime(const string & cacheControl, string directive){
    regex time_regex(".*?"+directive+"=(\\d+)");
    smatch time_match;
    if (regex_search(cacheControl, time_match, time_regex)){
        return stoi(time_match[1]);
    }
    return -1;
    
}
