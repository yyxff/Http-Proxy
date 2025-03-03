#include "CacheMaster.hpp"

#define CACHE_NUMBER 8

// singleton get instance
CacheMaster & CacheMaster::getInstance() {
    static CacheMaster instance;
    return instance;
}

CacheMaster::CacheMaster(){
    for (int i = 0; i < CACHE_NUMBER; i++){
        cacheList.push_back(new Cache());
    }
}

CacheMaster::~CacheMaster() {
    for (auto& cache : cacheList) {
        delete cache; 
    }
}

Cache & CacheMaster::selectCache(const string & url){
    return *(cacheList.at(hash<string>{}(url) % cacheList.size()));
}

int CacheMaster::selectIndex(const string & url){
    return hash<string>{}(url) % cacheList.size();
}
