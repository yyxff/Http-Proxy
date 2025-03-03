#ifndef CACHEMASTER_HPP
#define CACHEMASTER_HPP

#include "Cache.hpp"
using namespace std;


class CacheMaster{
    public:
        static CacheMaster & getInstance();

        Cache & selectCache(const string & url);

        int selectIndex(const string & url);

        ~CacheMaster();
    private:
        CacheMaster();

        vector<Cache*> cacheList;
};



#endif