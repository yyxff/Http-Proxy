#include "CacheDecision.hpp"

CacheDecision::Decision CacheDecision::makeDecision(const Request & request){
    string cacheControl = request.getHeader("Cache-Control");
    Cache & cache = Cache::getInstance();
    Cache::CacheStatus cacheStatus = cache.checkStatus(request.getUrl());
    CacheEntry * entry = cache.getEntry(request.getUrl());
    if (entry == NULL){return CacheDecision::DIRECT;}

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
    if (cacheControl.find("no-store") != string::npos){
        return CacheDecision::DIRECT;
    }
    if (cacheControl.find("no-store") != string::npos){
        return CacheDecision::DIRECT;
    }
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

int CacheDecision::parseTime(const string & cacheControl, string directive){
    regex time_regex(".*?"+directive+"=(\\d+)");
    smatch time_match;
    if (regex_search(cacheControl, time_match, time_regex)){
        return stoi(time_match[1]);
    }
    return -1;
    
}
