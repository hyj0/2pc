# 2pc �Ż�

## ���ݴ洢
### ����
    //data_key1_ver3 --> {state:deleted, value:value22}
    data_key1_commit_ts --> {state:update, value:value2}
    data_key1_start_ts --> {state:update, value:value2}
    data_key1_commit_ts_old --> {state:insert, value:value2}
### ��
    lock_key --> {}
### ������
    trans_commit_ts --> {type:commited, beginTrans:start_ts, transList:[id0, id1/*�������б�*/]}
    //trans_commit_ts --> {type:prepared, beginTrans:start_ts}
    trans_start_ts --> {type:begin, commitTrans:commit_ts}

### ����
    start_ts = getTS()
    begin(start_ts)   //  trans_start_ts --> {type:begin, }
    value = get(key, start_ts) 
    value2 += value
    update(key, value2, start_ts)   // data_key_start_st --> {state:ok, value:value}
                                    // trans_start_ts --> {type:begin, keys:[key, ] }
    commit_ts = getTS()
    commit(commit_ts)   // data_key_commit_ts --> {state:ok, value:value,  }
                        // trans_commit_ts --> {type:prepared-->commited, beginTrans:start_ts, transList:[id0, id1/*�������б�*/]}
                        
### ����
#### begin(start_ts)
    ���Ӽ�¼ trans_start_ts --> {type:begin, }
#### get(key, start_ts)
    ����data_key_start_ts, SeekToPrev, 
    loop key, ver, value
        1, ���ver��start_ts, ����value
        2, ͨ��ver��������trans_ver, Ȼ��trans_ver������״̬��commited, ����value, ����״̬Ϊcommited����abort��ȴ�
        3, ����not found
#### update(key, value2, start_ts)
    ���Ӽ�¼data_key_start_st --> {state:ok, value:value}
    trans_start_ts --> {type:begin, keys:[key, ] }��keys����key
#### commit(commit_ts)  
    prepare:������:дtrans_commit_ts --> {type:prepared, beginTrans:start_ts, transList:[id0, id1/*�������б�*/]},
                    trans_start_ts --> {type:begin, commitTrans:commit_ts}, 
            ������:дdata_key_commit_ts --> {state:ok, value:value,  }(*~~������۸�~~*), ����
        Э����:������нڵ�prepare���سɹ�, �򷵻�client�ɹ�, �첽֪ͨtransList:id0�ɹ�prepared
        ������:transList:id0�첽�ռ�����������״̬, ��prepared��, �����յ�Э����֪ͨprepared, ֪ͨ����������commited
#### rollback(commit_ts)
    prepare:������:дtrans_commit_ts --> {type:abort, beginTrans:start_ts, transList:[id0, id1/*�������б�*/]},
                        trans_start_ts --> {type:begin, commitTrans:commit_ts}, ����
    
### ��������

### ������� 
    