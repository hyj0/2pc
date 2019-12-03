# 2pc 优化

## 数据存储
### 数据
    //data_key1_ver3 --> {state:deleted, value:value22}
    data_key1_commit_ts --> {state:update, value:value2}
    data_key1_start_ts --> {state:update, value:value2}
    data_key1_commit_ts_old --> {state:insert, value:value2}
### 锁
    lock_key --> {}
### 事务链
    trans_commit_ts --> {type:commited, beginTrans:start_ts, transList:[id0, id1/*参与者列表*/]}
    //trans_commit_ts --> {type:prepared, beginTrans:start_ts}
    trans_start_ts --> {type:begin, commitTrans:commit_ts}

### 流程
    start_ts = getTS()
    begin(start_ts)   //  trans_start_ts --> {type:begin, }
    value = get(key, start_ts) 
    value2 += value
    update(key, value2, start_ts)   // data_key_start_st --> {state:ok, value:value}
                                    // trans_start_ts --> {type:begin, keys:[key, ] }
    commit_ts = getTS()
    commit(commit_ts)   // data_key_commit_ts --> {state:ok, value:value,  }
                        // trans_commit_ts --> {type:prepared-->commited, beginTrans:start_ts, transList:[id0, id1/*参与者列表*/]}
                        
### 访问
#### begin(start_ts)
    增加记录 trans_start_ts --> {type:begin, }
#### get(key, start_ts)
    搜索data_key_start_ts, SeekToPrev, 
    loop key, ver, value
        1, 如果ver是start_ts, 返回value
        2, 通过ver查找事务trans_ver, 然后trans_ver的事务状态是commited, 返回value, 事务状态为commited或者abort则等待
        3, 返回not found
#### update(key, value2, start_ts)
    增加记录data_key_start_st --> {state:ok, value:value}
    trans_start_ts --> {type:begin, keys:[key, ] }中keys增加key
#### commit(commit_ts)  
    prepare:参与者:写trans_commit_ts --> {type:prepared, beginTrans:start_ts, transList:[id0, id1/*参与者列表*/]},
                    trans_start_ts --> {type:begin, commitTrans:commit_ts}, 
            参与者:写data_key_commit_ts --> {state:ok, value:value,  }(*~~这里代价高~~*), 返回
        协调者:如果所有节点prepare返回成功, 则返回client成功, 异步通知transList:id0成功prepared
        参与者:transList:id0异步收集其他参与者状态, 都prepared后, 或者收到协调者通知prepared, 通知其他参与者commited
#### rollback(commit_ts)
    prepare:参与者:写trans_commit_ts --> {type:abort, beginTrans:start_ts, transList:[id0, id1/*参与者列表*/]},
                        trans_start_ts --> {type:begin, commitTrans:commit_ts}, 返回
    
### 清理数据

### 死锁检测 
    