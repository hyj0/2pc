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
            if (trans.state() == tpc::Network::TransState::TransStateRollback
                || trans.state() == tpc::Network::TransState::TransStateCommited) {
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
        LOG_COUT << "start deal TransStatePrepared trans=" << valueJson << LOG_ENDL;

        PrepareTask *co = prepareTaskStack->top();
        co->begin_ts = begin_ts;
        prepareTaskStack->pop();
        co_resume(co->co);
    }
}

int tpc::Core::Storage::updateTransPrepared(string begin_ts, tpc::Network::RpcReq *rpcReq) {
    string key = tpc::Core::Storage::makeTrans(begin_ts);
    string valueJson;
    rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, &valueJson);
    if (!status.ok()) {
        LOG_COUT << "can not found trans=" << key << LOG_ENDL;
        return -1;
    }
    tpc::Storage::Trans trans;
    tpc::Core::Utils::JsonStr2Msg(valueJson, trans);
    if (trans.state() != tpc::Network::TransState::TransStateBegin) {
        LOG_COUT << "trans state err!! " << key << "-->" << valueJson << LOG_ENDL;
        return -2;
    }
    trans.set_state(tpc::Network::TransState::TransStatePrepared);
    trans.set_start_trans(rpcReq->start_ts());
    for (int i = 0; i < rpcReq->trans_list_size(); ++i) {
        tpc::Storage::TransList *transList = trans.add_trans_list();
        Config::Host *host = transList->mutable_host();
        host->CopyFrom(rpcReq->trans_list(i));
        if (host->hash_id() == self_hash_id) {
            //设置自身为TransStatePrepared
            transList->set_state(tpc::Network::TransState::TransStatePrepared);
        }
    }
    status = db->Put(rocksdb::WriteOptions(), key, tpc::Core::Utils::Msg2JsonStr(trans));
    if (!status.ok()) {
        LOG_COUT << "update trans err ret=" << status.code()
        << " "<<key << "-->" << tpc::Core::Utils::Msg2JsonStr(trans) << LOG_ENDL;
        return -3;
    }
    return 0;
}

int tpc::Core::Storage::ReadData(string begin_ts, string key, string &value, string &retBeginTs, string &retStartTs,
                                 string &retCommitTs) {
    // 读取data_key1_entry --> {state:update, value:value3, next:begin_ts1, cur_version:begin_ts},
    //     如果cur_version == begin_ts, 返回
    //    loop 取next --> state:update, value:value3, next:begin_ts1
    //        0, 读取begin_ts1对应的trans
    //        1, 如果next是begin_ts, 返回value, 这是自己修改的记录, 返回
    //        2, 通过next:begin_ts1查找事务的trans_begin_ts1, 如果trans_start_ts小于输入参数begin_ts: 事务状态是commited, 返回value; 事务状态为prepared则等待(prepared说明事务提交中)
    //            返回最大的commited的commit_ts的数据
    //        3, 返回not found
    string loopBeginTs;
    while (1) {
        string datakey;
        if (loopBeginTs.length() == 0) {
            datakey = tpc::Core::Storage::makeDataEntry(key);
        } else {
            datakey = tpc::Core::Storage::makeData(key, loopBeginTs);
        }
        rocksdb::Status status;
        string valueJson;
        status = db->Get(rocksdb::ReadOptions(), datakey, &valueJson);
        if (status.ok()) {
            tpc::Storage::Data data;
            tpc::Core::Utils::JsonStr2Msg(valueJson, data);
            if (data.cur_version() == begin_ts) {
                //当前事务写入的, 状态应该是begin
                value = data.value();
                retBeginTs = data.cur_version();
                if (data.state() == tpc::Network::RequestType::Req_Type_Delete) {
                    return 33;//deleted
                }
                return 0;
            } else {
                //非当前事务写入
                string transKey = tpc::Core::Storage::makeTrans(data.cur_version());
                string transJson = "";
                status = db->Get(rocksdb::ReadOptions(), transKey, &transJson);
                if (!status.ok()) {
                    LOG_COUT << "can not found trans=" << transKey << " ret="<< status.code() << LOG_ENDL;
                    return -1;
                }
                tpc::Storage::Trans trans;
                tpc::Core::Utils::JsonStr2Msg(transJson, trans);
                if (trans.state() == tpc::Network::TransState::TransStateBegin) {
                    //事务正在运行中
                    LOG_COUT << "trans doing !! " << transKey << "-->" << transJson << " "<< datakey << "-->" << valueJson << LOG_ENDL;
                } else if (trans.state() == tpc::Network::TransState::TransStatePrepared){
                    if (begin_ts > trans.start_trans()) {
                        //todo:begin_ts比start_ts大,应该需要等待, 目前直接返回,让client重试
                        //todo:优化:如果self_commit_ts>begin_ts, 可以直接不管, 取下一条, 因为最终的commit_ts一定大于等于self_commit_ts.
                        retBeginTs = data.cur_version();
                        retStartTs = trans.start_trans();
                        retCommitTs = trans.commit_trans();
                        return 66;
                    } else {
                        ;
                    }
                } else if (trans.state() == tpc::Network::TransState::TransStateCommited) {
                    if (begin_ts > trans.commit_trans()) {
                        //比begin_ts小的最大版本数据
                        value = data.value();
                        retBeginTs = data.cur_version();
                        retStartTs = trans.start_trans();
                        retCommitTs = trans.commit_trans();
                        if (data.state() == tpc::Network::RequestType::Req_Type_Delete) {
                            return 33;//deleted
                        }
                        return 0;
                    } else {
                        ;//continue
                    }
                } else if (trans.state() == tpc::Network::TransState::TransStateRollback) {
                    ;//continue
                }

                //循环
                loopBeginTs = data.next();
                if (loopBeginTs.length() == 0) {
                    //next为空
                    return 99;
                }
                continue;
            }
        } else if (status.code() == rocksdb::Status::kNotFound) {
            //Not found
            return  99;
        } else {
            LOG_COUT << "db Get err!! key="<<datakey << " ret="<<status.code() << LOG_ENDL;
            assert(0);
        }
    }
    return 0;
}
