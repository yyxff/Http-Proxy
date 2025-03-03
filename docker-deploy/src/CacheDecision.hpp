#ifndef CACHEDECISION_HPP
#define CACHEDECISION_HPP


#include "Cache.hpp"
#include "Request.hpp"
#include "CacheMaster.hpp"
#include <regex>
#include <algorithm>
#include <ctime>
#include <string>
#include <iomanip>
#include <sstream>
using namespace std;
class CacheDecision{
    public:
        
        enum Decision{
            DIRECT,
            REVALIDATE,
            RETURN_CACHE,
            RETURN_504,
            RETURN_304,
            NO_TRANSFORM
        };

        Decision makeDecision(const Request & request);


    private:
    
        static inline Logger & logger = Logger::getInstance();

        Decision handle_max_age(const string & cacheControl, const CacheEntry * entry, int id);
        Decision handle_min_fresh(const string & cacheControl, const CacheEntry * entry, int id);
        Decision handle_max_stale(const string & cacheControl, const CacheEntry * entry, int id);

        int parseTime(const string & cacheControl, string directive);
        string timeToStr(time_t time);

};

#endif