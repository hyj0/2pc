//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_STORAGE_H
#define PROJECT_STORAGE_H

#include <pb2json.h>
#include <string>
#include <rocksdb/db.h>

using namespace std;

namespace tpc::Core {
    class Storage {
    private:
        rocksdb::DB* db;
    public:
        Storage() {}
        virtual ~Storage();
        int init(string dbPath);

        int addTrans(string begin_ts);

        static string makeTrans(string ts);

        static string makeLock(string key) {
            return string("lock_"+key);
        }
        static string makeDataEntry(string key){
            return key + "_entry";
        }
        static string makeData(string key, string begin_ts) {
            return string("data_"+key + "_" + begin_ts) ;
        }

        int addLock(string key, string begin_ts);

        int addData(string key, string value, string begin_ts, tpc::Network::RequestType type);

        void startUpClean();
    };
}



#endif //PROJECT_STORAGE_H
