//
// Created by hyj on 2019-12-09.
//
#include <pb2json.h> //pb2json.h要放在第一行
#include <unistd.h>
#include <sys/socket.h>
#include <co_routine_inner.h>
#include <netinet/in.h>
#include <Utils.h>
#include <Config.h>
#include "Log.h"
#include "Core/Network.h"
#include "config.pb.h"
#include <stack>
#include <Msg.h>


int g_listen_fd;
shared_ptr<tpc::Config::Config> g_config;

struct task_t
{
    stCoRoutine_t *co;
    int fd;
};
static stack<task_t*> g_readwrite;

class StorageCl {
public:
    int fd;
    int stat;// 0--init  1--has begin
    tpc::Config::Host host;

    StorageCl() {
        fd = -1;
        stat = 0;
    }
    ~StorageCl() {
        if (fd > 0) {
            close(fd);
        }
        stat = 0;
    }
    int getFd(string begin_ts) {
        if (fd < 0) {
            fd = tpc::Core::Network::Connect(host.host(), host.port());
            if (fd < 0) {
                return fd;
            }
        }
        if (stat == 0) {
            tpc::Network::Msg reqMsg;
            reqMsg.set_msg_type(tpc::Network::MsgType::MSG_Type_Rpc_Request);
            tpc::Network::RpcReq *rpcReq = reqMsg.mutable_rpc_request();
            rpcReq->set_request_type(tpc::Network::Req_Type_Begin);
            rpcReq->set_begin_ts(begin_ts);
            int ret = tpc::Core::Msg::SendMsg(fd, reqMsg);
            if (ret != 0) {
                LOG_COUT << "send msg err ret=" << ret << LOG_ENDL_ERR;
                return ret;
            }
            tpc::Core::Msg msg;
            ret = msg.ReadOneMsg(fd);
            if (ret < 0) {
                return ret;
            }
            if (ret != tpc::Network::MsgType::MSG_Type_Rpc_Response) {
                return ret;
            }
            tpc::Network::Msg resMsg = msg.getMsg();
            if (resMsg.mutable_rpc_response()->result() != 0) {
                LOG_COUT << "begin err ret=" << resMsg.mutable_rpc_response()->result()
                << " msg=" << resMsg.mutable_rpc_response()->err_msg() << LOG_ENDL;
                return -11;
            }
            stat = 1;
        }
        return fd;
    }

};

static void *readwrite_routine( void *arg )
{
    co_enable_hook_sys();

    task_t *co = (task_t*)arg;

    for(;;)
    {
        if( -1 == co->fd )
        {
            g_readwrite.push( co );
            co_yield_ct();
            continue;
        }

        int fd = co->fd;
        co->fd = -1;

        tpc::Core::Msg msg;
        int ret;
        //本地结构
        string begin_ts;
        string start_ts;
        StorageCl *storageCl = new StorageCl[g_config->host_size()];
        for (int i = 0; i < g_config->host_size(); ++i) {
            storageCl[i].host = g_config->host(i);
        }
        int kill_trans = 0;
        for (;;) {
            struct pollfd pf[100] = {0};
            memset(pf, 0, sizeof(pf));
            pf[g_config->host_size()].fd = fd;
            pf[g_config->host_size()].events = (POLLIN | POLLERR | POLLHUP);
            for (int i = 0; i < g_config->host_size(); ++i) {
                pf[i].fd = storageCl[i].fd;
                pf[i].events = (POLLIN | POLLERR | POLLHUP);
            }
            ret = co_poll(co_get_epoll_ct(), pf, g_config->host_size()+1, 1000);
            if (ret == 0) {
                continue;
            }
            //storage的socket异常
            for (int j = 0; j < g_config->host_size(); ++j) {
                if (pf[j].fd > 0) {
                    if (pf[j].revents) {
                        kill_trans = 1;
                        LOG_COUT << "storage fd err fd=" << pf[j].fd << LOG_ENDL;
                        break;
                    }
                } else {
                    continue;
                }
            }
            if (kill_trans) {
                break;
            }

            int msgType = msg.ReadOneMsg(fd);
            if (msgType < 0) {
                LOG_COUT << "fd=" << fd << " read err ret=" << msgType << LOG_ENDL_ERR;
                break;
            }

            string errMsg;
            int retCode = 0;
            string ret_key;
            string ret_value;
            
            tpc::Network::Msg cli_resMsg;
            cli_resMsg.set_msg_type(tpc::Network::MsgType::MSG_Type_Cli_Response);
            tpc::Network::CliRes *cliRes = cli_resMsg.mutable_cli_response();
            
            tpc::Network::Msg cli_reqMsg = msg.getMsg();
            if (msgType == tpc::Network::MsgType::MSG_Type_Cli_Request) {
                LOG_COUT << "fd=" << fd << " cli_reqMsg:" << tpc::Core::Utils::Msg2JsonStr(cli_reqMsg) << LOG_ENDL;
                tpc::Network::CliReq *cliReq = cli_reqMsg.mutable_cli_request();
                if (cliReq->request_type() == tpc::Network::RequestType::Req_Type_Begin) {
                    if (begin_ts.length() != 0) {
                        errMsg = "has begin trans!";
                        retCode = 1;
                        goto Respone;
                    }
                    begin_ts = tpc::Core::Utils::GetTS();
                } else if (cliReq->request_type() == tpc::Network::RequestType::Req_Type_Commit) {
                    if (begin_ts.length() == 0) {
                        errMsg = "has no begin trans!";
                        retCode = 1;
                        goto Respone;
                    }
                    start_ts = tpc::Core::Utils::GetTS();
                    //prepare
                    //协调者:如果所有节点prepare返回成功, 则返回client成功, 异步通知transList:id0成功commit(commit_ts)
                    //    todo:这里client先知道提交成功, 如果client又马上begin(begin_ts), 而参与者还没有commit(commit_ts), 这里begin_ts有可能大于commit_ts, 会读不到数据
                    //    -->参与者返回前commit_ts = getTs(),这个commit_ts各个参与者相互比较,最大的是最终的commit_ts.

                    //commit(commit_ts) 可选!!!


                } else if (cliReq->request_type() == tpc::Network::RequestType::Req_Type_Rollback) {
                    if (begin_ts.length() == 0) {
                        errMsg = "has no begin trans!";
                        retCode = 1;
                        goto Respone;
                    }
                } else if (cliReq->request_type() == tpc::Network::RequestType::Req_Type_Get
                    ||cliReq->request_type() == tpc::Network::RequestType::Req_Type_Update
                    ||cliReq->request_type() == tpc::Network::RequestType::Req_Type_Delete
                    ||cliReq->request_type() == tpc::Network::RequestType::Req_Type_Insert) {
                    if (begin_ts.length() == 0) {
                        errMsg = "has no begin trans!";
                        retCode = 1;
                        goto Respone;
                    }
                    int hash = tpc::Core::Utils::GetHash(cliReq->key(), g_config->host_size());
                    int sfd = storageCl[hash].getFd(begin_ts);
                    if (sfd < 0) {
                        errMsg = "get storage socket err";
                        retCode = 1;
                        kill_trans = 1;
                        goto Respone;
                    }
                    tpc::Network::Msg rpc_reqMsg;
                    rpc_reqMsg.set_msg_type(tpc::Network::MsgType::MSG_Type_Rpc_Request);
                    tpc::Network::RpcReq *rpcReq = rpc_reqMsg.mutable_rpc_request();
                    rpcReq->set_request_type(cliReq->request_type());
                    rpcReq->set_begin_ts(begin_ts);
                    rpcReq->set_key(cliReq->key());
                    rpcReq->set_value(cliReq->value());
                    ret = tpc::Core::Msg::SendMsg(sfd, rpc_reqMsg);
                    if (ret != 0) {
                        errMsg = "SendMsg storage  err";
                        retCode = 1;
                        kill_trans = 1;
                        goto Respone;
                    }
                    tpc::Network::Msg resMsg;
                    ret = tpc::Core::Msg::ReadOneMsg(sfd, resMsg);
                    if (ret < 0) {
                        errMsg = "ReadOneMsg storage  err";
                        retCode = 1;
                        kill_trans = 1;
                        goto Respone;
                    }
                    tpc::Network::RpcRes *rpcRes = resMsg.mutable_rpc_response();
                    retCode = rpcRes->result();
                    errMsg = rpcRes->err_msg();
                    cliRes->set_key(rpcRes->key());
                    cliRes->set_value(rpcRes->value());

                }
            } else {
                LOG_COUT << "err type=" << msgType << LOG_ENDL_ERR;
                break;
            }

            Respone:
            cliRes->set_result(retCode);
            cliRes->set_err_msg(errMsg);
            cliRes->set_begin_ts(begin_ts);
            ret = msg.SendMsg(fd, cli_resMsg);
            if (ret < 0) {
                LOG_COUT << "send msg err ret=" << ret << LOG_ENDL_ERR;
                break;
            }
            if (kill_trans) {
                break;
            }
        }
        close(fd);
        delete [] storageCl;
    }
    return 0;
}

static void *accept_routine( void * )
{
    co_enable_hook_sys();
//    printf("accept_routine\n");
    fflush(stdout);
    for(;;)
    {
        //printf("pid %ld g_readwrite.size %ld\n",getpid(),g_readwrite.size());
        if( g_readwrite.empty() )
        {
            printf("empty\n"); //sleep
            struct pollfd pf = { 0 };
            pf.fd = -1;
            poll( &pf,1,1000);

            continue;

        }
        struct sockaddr_in addr; //maybe sockaddr_un;
        memset( &addr,0,sizeof(addr) );
        socklen_t len = sizeof(addr);
        int co_accept(int fd, struct sockaddr *addr, socklen_t *len );
        int fd = co_accept(g_listen_fd, (struct sockaddr *)&addr, &len);
        if( fd < 0 )
        {
            struct pollfd pf = { 0 };
            pf.fd = g_listen_fd;
            pf.events = (POLLIN|POLLERR|POLLHUP);
            co_poll( co_get_epoll_ct(),&pf,1,1000 );
            continue;
        }
        if( g_readwrite.empty() )
        {
            close( fd );
            continue;
        }
        tpc::Core::Network::SetNonBlock( fd );
        task_t *co = g_readwrite.top();
        co->fd = fd;
        g_readwrite.pop();
        co_resume( co->co );
    }
    return 0;
}



int main(int argc, char **argv) {
    if (argc != 3) {
        LOG_COUT << "usage:" << argv[0] << " port config.json" << LOG_ENDL;
        return -1;
    }
    int nListenPort = atol(argv[1]);
    g_listen_fd = tpc::Core::Network::CreateTcpSocket(nListenPort, "", true);
    if (g_listen_fd < 0) {
        LOG_COUT << "CreateTcpSocket fd=" << g_listen_fd << LOG_ENDL_ERR;
        return g_listen_fd;
    }
    listen( g_listen_fd,1024 );
    if(g_listen_fd==-1){
        printf("Port %d is in use\n", g_listen_fd);
        return -1;
    }
    printf("listen %d :%d\n",g_listen_fd, nListenPort);
    tpc::Core::Network::SetNonBlock( g_listen_fd );

    //get config
    tpc::Core::Config config(argv[2]);
    g_config = config.getConfig();

    // make coroutine
    for (int i = 0; i < 100; i++) {
        task_t *task = (task_t *) calloc(1, sizeof(task_t));
        task->fd = -1;

        co_create(&(task->co), NULL, readwrite_routine, task);
        co_resume(task->co);
    }
    stCoRoutine_t *accept_co = NULL;
    co_create(&accept_co, NULL, accept_routine, 0);
    co_resume(accept_co);

    co_eventloop(co_get_epoll_ct(), 0, 0);
    return 0;
}

