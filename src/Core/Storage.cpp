//
// Created by hyj on 2019-12-10.
//
#include <pb2json.h>
#include <storage.pb.h>
#include <Log.h>
#include "Storage.h"
#include "Utils.h"


tpc::Core::Storage::~Storage() {
    delete db;
}

int tpc::Core::Storage::init(string dbPath) {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 1 << 30;
    rocksdb::Status status = rocksdb::DB::Open(options, dbPath, &db);
    if(!status.ok()) {
        return -1;
    }
    return 0;
}

int tpc::Core::Storage::addTrans(string begin_ts) {
    string key = this->makeTrans(begin_ts);
    string valueJson;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &valueJson);
    if (status.code() != rocksdb::Status::Code::kNotFound) {
        return -1;
    }
    tpc::Storage::Trans trans;
    trans.set_state(tpc::Network::TransState::TransStateBegin);
    status = db->Put(rocksdb::WriteOptions(), key, tpc::Core::Utils::Msg2JsonStr(trans));
    if (!status.ok()) {
        return -2;
    }
    return 0;
}


int tpc::Core::Storage::addLock(string key, string begin_ts) {
    string key0 = tpc::Core::Storage::makeLock(key);
    rocksdb::Status status;
    string valueJson;
    status = db->Get(rocksdb::ReadOptions(), key0, &valueJson);
    if (status.ok()) {
        tpc::Storage::Lock lock;
        tpc::Core::Utils::JsonStr2Msg(valueJson, lock);
        if (lock.trans() != begin_ts) {
            //其他事务已加锁 todo:判断事务状态 清理
            status = db->Get(rocksdb::ReadOptions(), makeTrans(lock.trans()), &valueJson);
            if (!status.ok()) {
                LOG_COUT << "get trans err trans=" << makeTrans(lock.trans()) << LOG_ENDL_ERR;
                return -11;
            }
            tpc::Storage::Trans trans;
            tpc::Core::Utils::JsonStr2Msg(valueJson, trans);
            if (trans.state() == tpc::Network::TransState::TransStateRollback) {
                //rollback状态,直接覆盖
                tpc::Storage::Lock lock;
                lock.set_trans(begin_ts);
                //todo:注意并发
                status = db->Put(rocksdb::WriteOptions(), key0, tpc::Core::Utils::Msg2JsonStr(lock));
                if (!status.ok()) {
                    return -2;
                } else {
                    return 0;
                }
            }
            return -1;
        } else {
            //当前事务已加锁
            return 0;
        }
    } else if (status.code() == rocksdb::Status::Code::kNotFound) {
        tpc::Storage::Lock lock;
        lock.set_trans(begin_ts);
        //todo:注意并发
        status = db->Put(rocksdb::WriteOptions(), key0, tpc::Core::Utils::Msg2JsonStr(lock));
        if (!status.ok()) {
            return -2;
        } else {
            return 0;
        }
    }
    return 0;
}

int tpc::Core::Storage::addData(string key, string value, string begin_ts, tpc::Network::RequestType type) {
    string keyEntry = tpc::Core::Storage::makeDataEntry(key);
    rocksdb::Status status;
    string valueJson;
    status = db->Get(rocksdb::ReadOptions(), keyEntry, &valueJson);
    if (status.ok()) {
        tpc::Storage::Data data;
        tpc::Core::Utils::JsonStr2Msg(valueJson, data);
        if (data.cur_version() == begin_ts) {
            //当前事务写入的
            data.set_state(type);
            data.set_value(value);
            status = db->Put(rocksdb::WriteOptions(), keyEntry, tpc::Core::Utils::Msg2JsonStr(data));
            if (!status.ok()) {
                return -1;
            } else {
                return 0;
            }
        } else {
            //不是当前事务写入
            //todo:应该要判断data.cur_version对应的begin_ts,进行清理. .

            //写入以前的data
            string key1 = makeData(key, data.cur_version());
            status = db->Put(rocksdb::WriteOptions(), key1, tpc::Core::Utils::Msg2JsonStr(data));
            if (!status.ok()) {
                return -2;
            }
            //更新entry
            tpc::Storage::Data dataEntry;
            dataEntry.set_state(type);
            dataEntry.set_value(value);
            dataEntry.set_next(data.cur_version());
            dataEntry.set_cur_version(begin_ts);
            status = db->Put(rocksdb::WriteOptions(), keyEntry, tpc::Core::Utils::Msg2JsonStr(dataEntry));
            if (!status.ok()) {
                return -2;
            } else {
                return 0;
            }
        }
    } else {
        //写入口
        tpc::Storage::Data data;
        data.set_state(type);
        data.set_value(value);
        data.set_next("");
        data.set_cur_version(begin_ts);
        status = db->Put(rocksdb::WriteOptions(), keyEntry, tpc::Core::Utils::Msg2JsonStr(data));
        if (!status.ok()) {
            return -3;
        } else {
            return 0;
        }
    }
    return 0;
}

void tpc::Core::Storage::startUpClean() {
    auto iter = db->NewIterator(rocksdb::ReadOptions());
    string prefix = tpc::Core::Storage::makeTrans("");
    for (iter->Seek(prefix); iter->Valid() && iter->key().starts_with(prefix); iter->Next()) {
        // do something
//        LOG_COUT << iter->key().ToString() << "->" << iter->value().ToString() << LOG_ENDL;
        tpc::Storage::Trans trans;
        tpc::Core::Utils::JsonStr2Msg(iter->value().ToString(), trans);
        if (trans.state() == tpc::Network::TransState::TransStateBegin) {
            LOG_COUT << iter->key().ToString() << "-->" << "TransStateRollback" << LOG_ENDL;
            trans.set_state(tpc::Network::TransState::TransStateRollback);
            rocksdb::Status status= db->Put(rocksdb::WriteOptions(), iter->key(), tpc::Core::Utils::Msg2JsonStr(trans));
            if (!status.ok()) {
                LOG_COUT << "put err" << LOG_ENDL_ERR;
                assert(0);
            }
        } else if (trans.state() == tpc::Network::TransState::TransStatePrepared) {
            //todo:prepared的事务异常处理
            string begin_ts;
            tpc::Core::Storage::parseTrans(iter->key().ToString(), begin_ts);
            startCleanTrans(begin_ts);
        }
    }
    delete iter;
}

void tpc::Core::Storage::startCleanTrans(string begin_ts) {
    string key = tpc::Core::Storage::makeTrans(begin_ts);
    string valueJson;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &valueJson);
    if (!status.ok()) {
        LOG_COUT << "can not found trans=" << key << LOG_ENDL;
        return;
    }
    tpc::Storage::Trans trans;
    tpc::Core::Utils::JsonStr2Msg(valueJson, trans);
    if (trans.state() == tpc::Network::TransState::TransStateBegin) {
        LOG_COUT << key << "-->" << "TransStateRollback" << LOG_ENDL;
        trans.set_state(tpc::Network::TransState::TransStateRollback);
        rocksdb::Status status= db->Put(rocksdb::WriteOptions(), key, tpc::Core::Utils::Msg2JsonStr(trans));
        if (!status.ok()) {
            LOG_COUT << "put err" << LOG_ENDL_ERR;
            assert(0);
        }
    } else if (trans.state() == tpc::Network::TransState::TransStatePrepared) {
        //todo:prepared的事务异常处理
        LOG_COUT << "start deal trans=" << valueJson << LOG_ENDL;

    }
}
