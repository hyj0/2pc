# 2pc �Ż�

## ���ݴ洢
### ������
    //�����ڵ����� data_key1_entry --> data_key1_begin_ts1 --> data_key1_begin_ts --> data_key1_begin_ts_old
    //data_key1_ver3 --> {state:deleted, value:value22}
    data_key1_entry --> {state:update, value:value3, next:begin_ts1, cur_version:begin_ts2}
    data_key1_begin_ts1 --> {state:update, value:value3, next:begin_ts}
    data_key1_begin_ts --> {state:update, value:value2, next:begin_ts_old}
    data_key1_begin_ts_old --> {state:insert, value:value1, next:null}
    
    data_key2_entry --> {state:update, value:value3, next:begin_ts1, cur_version:begin_ts3}
    data_key2_begin_ts1 --> {state:update, value:value3, next:begin_ts}
    data_key2_begin_ts --> {state:update, value:value2, next:begin_ts_old}
    data_key2_begin_ts_old --> {state:insert, value:value1, next:null}
    
### ��
    lock_key1 --> {trans:begin_ts2}
    lock_key2 --> {trans:begin_ts3}
### ������
    //
    trans_begin_ts3 --> {state:begin, commitTrans:null,  startTrans:null,   transList:[id0, id1/*�������б�*/]}
    trans_begin_ts2 --> {state:prepared, commitTrans:commit_ts2,  startTrans:start_ts2,   transList:[id0, id1/*�������б�*/]}
    trans_begin_ts1 --> {state:commited, commitTrans:commit_ts1, startTrans:start_ts1,   transList:[id0, id1/*�������б�*/]}
    trans_begin_ts --> {state:commited, commitTrans:commit_ts, startTrans:start_ts,   transList:[id0, id1/*�������б�*/]}

### ����
    begin_ts = getTS()
    begin(begin_ts)   //  trans_begin_ts --> {state:begin, }
    value = get(key, begin_ts) 
    value2 += value
    update(key, value2, begin_ts)   // data_key_begin_ts --> {state:update, value:value}
                                    // trans_begin_ts --> {state:begin, keys:[key, ] }
    start_ts = getTS()
    prepare(start_ts)      // trans_begin_ts --> {state:prepared, startTrans:start_ts,   transList:[id0, id1/*�������б�*/]}
    //���еĲ����߶�prepare�ɹ�
    commit_ts = getTS()
    commit(commit_ts)   // trans_begin_ts --> {state:commited, startTrans:start_ts,   transList:[id0, id1/*�������б�*/]}
                        // �����
    // else 
    rollback(commit_ts)
                        
### ����
#### begin(start_ts)
    ���Ӽ�¼ trans_begin_ts --> {state:begin, }
#### get(key, begin_ts)
    ��ȡdata_key1_entry --> {state:update, value:value3, next:begin_ts1},
    loop ȡnext --> state:update, value:value3, next:begin_ts1
        1, ���next��begin_ts, ����value, �����Լ��޸ĵļ�¼
        2, ͨ��next:begin_ts1���������trans_begin_ts1, ���trans_start_tsС���������begin_ts: ����״̬��commited, ����value; ����״̬Ϊprepared��ȴ�(prepared˵�������ύ��)
            ��������commited��commit_ts������
        3, ����not found
#### update(key, value2, begin_ts)
    ����key, ����lock_key --> {trans:begin_ts}
    ��ȡdata_key_entry-->{state:update, value:value3, next:begin_ts2, cur_version:begin_ts1}
    ���¼�¼data_key_begin_ts1 --> {state:update, value:value3, next:begin_ts2}
    ����data_key_entry-->{state:update, value:value2, next:begin_ts1, cur_version:begin_ts}
    trans_begin_ts --> {state:begin, keys:[key, ] }��keys����key
#### prepared(start_ts, transList)
    ������:дredolog, ����trans_begin_ts --> {state:prepared, commitTrans:null, startTrans:start_ts, next:null,  transList:[id0, id1/*�������б�*/]}, ����
    Э����:������нڵ�prepare���سɹ�, �򷵻�client�ɹ�, �첽֪ͨtransList:id0�ɹ�commit(commit_ts)
#### commit(commit_ts)  
    ������:����trans_begin_ts --> {state:commited, commitTrans:commit_ts, startTrans:start_ts, next:null,  transList:[id0, id1/*�������б�*/]}
    �����
    ������:transList:id0�첽�ռ�����������״̬, ��prepared��, �����յ�Э����֪ͨprepared, ֪ͨ����������commited
       
#### rollback(commit_ts)
    ������: ����trans_begin_ts --> {state:aborted, commitTrans:commit_ts, startTrans:null, next:null,  transList:[id0, id1/*�������б�*/]}
    �����
    
### ��������

### ������� 

### ������鼰�����쳣����
    