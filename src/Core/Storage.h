//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_STORAGE_H
#define PROJECT_STORAGE_H

#include <pb2json.h>
#include <string>
#include <rocksdb/db.h>
#include <co_routine_inner.h>

using namespace std;

class PrepareTask {
public:
    stCoRoutine_t *co;
    string begin_ts;
};

namespace tpc::Core {
    class Storage {
    public:
        rocksdb::DB* db;
    public:
        stack<PrepareTask *> *prepareTaskStack;
        int self_hash_id;

        Storage() {}
        virtual ~Storage();
        int init(string dbPath);

        int addTrans(string begin_ts);

        static string makeTrans(string ts) {
            return string("trans_"+ts);
        }
        static void parseTrans(string key, string &begin_ts) {
            begin_ts = key.substr(strlen("trans_"));
        }
        static string makeLock(string key) {
            return string("lock_"+key);
        }
        static string makeDataEntry(string key){
            return string("data_") + key + "_entry";
        }
        static string makeData(string key, string begin_ts) {
            return string("data_"+key + "_" + begin_ts) ;
        }

        int addLock(string key, string begin_ts);

        int addData(string key, string value, string begin_ts, tpc::Network::RequestType type);

        void startUpClean();

        void startCleanTrans(string begin_ts);

        int updateTransPrepared(string begin_ts, tpc::Network::RpcReq *rpcReq);

        int updateTransSelfCommitTs(string begin_ts, string self_commit_ts);

        int ReadData(string begin_ts, string key, string &value, string &retBeginTs, string &retStartTs,
                             string &retCommitTs);
    };
}



#endif //PROJECT_STORAGE_H
