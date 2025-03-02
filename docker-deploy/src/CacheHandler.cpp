#include "CacheHandler.hpp"

bool CacheHandler::need_to_send(CacheDecision::Decision decision){
    if (decision == CacheDecision::DIRECT ||
        decision == CacheDecision::REVALIDATE ||
        decision == CacheDecision::NO_TRANSFORM){
            return true;
        }
    return false;
}

string CacheHandler::build_forward_response(CacheDecision::Decision decision, CacheEntry * entry){
    switch(decision){
        case CacheDecision::RETURN_CACHE:{
            return entry->getFullResponse();
        }
        case CacheDecision::RETURN_304:{
            return "HTTP/1.1 304 Not Modified\r\n\r\n";
        }
        case CacheDecision::RETURN_504:{
            return "HTTP/1.1 504 Gateway Timeout\r\n\r\n";
        }
    }
    return "";
}

// string CacheHandler::build_forward_request(CacheDecision::Decision decision, Request & request){
//     switch(decision){
//         case CacheDecision::DIRECT:{
//             // return entry->getFullResponse();
//         }
//         case CacheDecision::REVALIDATE:{
//             return "HTTP/1.1 304 Not Modified\r\n\r\n";
//         }
//         case CacheDecision::NO_TRANSFORM:{
//             return request.get;
//         }
//     }
// }