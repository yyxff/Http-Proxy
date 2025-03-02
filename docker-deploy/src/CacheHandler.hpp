#ifndef CACHEHANDLER_HPP
#define CACHEHANDLER_HPP


#include "CacheDecision.hpp"

class CacheHandler{
    public:
        bool need_to_send(CacheDecision::Decision decision);

        string build_forward_response(CacheDecision::Decision decision, CacheEntry * entry);

        // string build_forward_request(CacheDecision::Decision decision, Request & request);
};

#endif 