# 2pc 优化

## 数据存储
### 数据链
    //类似于单链表 data_key1_entry --> data_key1_begin_ts1 --> data_key1_begin_ts --> data_key1_begin_ts_old
    //data_key1_ver3 --> {state:deleted, value:value22}
    data_key1_entry --> {state:update, value:value3, next:begin_ts1, cur_version:begin_ts2}
    data_key1_begin_ts1 --> {state:update, value:value3, next:begin_ts}
    data_key1_begin_ts --> {state:update, value:value2, next:begin_ts_old}
    data_key1_begin_ts_old --> {state:insert, value:value1, next:null}
    
    data_key2_entry --> {state:update, value:value3, next:begin_ts1, cur_version:begin_ts3}
    data_key2_begin_ts1 --> {state:update, value:value3, next:begin_ts}
    data_key2_begin_ts --> {state:update, value:value2, next:begin_ts_old}
    data_key2_begin_ts_old --> {state:insert, value:value1, next:null}
    
### 锁
    lock_key1 --> {trans:begin_ts2}
    lock_key2 --> {trans:begin_ts3}
### 事务链
    //
    trans_begin_ts3 --> {state:begin, commitTrans:null,  startTrans:null,   transList:[id0, id1/*参与者列表*/]}
    trans_begin_ts2 --> {state:prepared, commitTrans:commit_ts2,  startTrans:start_ts2,   transList:[id0, id1/*参与者列表*/]}
    trans_begin_ts1 --> {state:commited, commitTrans:commit_ts1, startTrans:start_ts1,   transList:[id0, id1/*参与者列表*/]}
    trans_begin_ts --> {state:commited, commitTrans:commit_ts, startTrans:start_ts,   transList:[id0, id1/*参与者列表*/]}

### 流程
    begin_ts = getTS()
    begin(begin_ts)   //  trans_begin_ts --> {state:begin, }
    value = get(key, begin_ts) 
    value2 += value
    update(key, value2, begin_ts)   // data_key_begin_ts --> {state:update, value:value}
                                    // trans_begin_ts --> {state:begin, keys:[key, ] }
    start_ts = getTS()
    prepare(start_ts)      // trans_begin_ts --> {state:prepared, startTrans:start_ts,   transList:[id0, id1/*参与者列表*/]}
    //所有的参与者都prepare成功
    commit_ts = getTS()
    commit(commit_ts)   // trans_begin_ts --> {state:commited, startTrans:start_ts,   transList:[id0, id1/*参与者列表*/]}
                        // 清除锁
    // else 
    rollback(commit_ts)
                        
### 访问
#### begin(start_ts)
    增加记录 trans_begin_ts --> {state:begin, }
#### get(key, begin_ts)
    读取data_key1_entry --> {state:update, value:value3, next:begin_ts1},
    loop 取next --> state:update, value:value3, next:begin_ts1
        1, 如果next是begin_ts, 返回value, 这是自己修改的记录
        2, 通过next:begin_ts1查找事务的trans_begin_ts1, 如果trans_start_ts小于输入参数begin_ts: 事务状态是commited, 返回value; 事务状态为prepared则等待(prepared说明事务提交中)
            返回最大的commited的commit_ts的数据
        3, 返回not found
#### update(key, value2, begin_ts)
    加锁key, 增加lock_key --> {trans:begin_ts}
    读取data_key_entry-->{state:update, value:value3, next:begin_ts2, cur_version:begin_ts1}
    更新记录data_key_begin_ts1 --> {state:update, value:value3, next:begin_ts2}
    更新data_key_entry-->{state:update, value:value2, next:begin_ts1, cur_version:begin_ts}
    trans_begin_ts --> {state:begin, keys:[key, ] }中keys增加key
#### prepared(start_ts, transList)
    参与者:写redolog, 更新trans_begin_ts --> {state:prepared, commitTrans:null, startTrans:start_ts, next:null,  transList:[id0, id1/*参与者列表*/]}, 返回
    协调者:如果所有节点prepare返回成功, 则返回client成功, 异步通知transList:id0成功commit(commit_ts)
#### commit(commit_ts)  
    参与者:更新trans_begin_ts --> {state:commited, commitTrans:commit_ts, startTrans:start_ts, next:null,  transList:[id0, id1/*参与者列表*/]}
    清除锁
    参与者:transList:id0异步收集其他参与者状态, 都prepared后, 或者收到协调者通知prepared, 通知其他参与者commited
       
#### rollback(commit_ts)
    参与者: 更新trans_begin_ts --> {state:aborted, commitTrans:commit_ts, startTrans:null, next:null,  transList:[id0, id1/*参与者列表*/]}
    清除锁
    
### 清理数据

### 死锁检测 

### 启动检查及处理异常事务
    