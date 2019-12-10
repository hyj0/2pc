//
// Created by hyj on 2019-12-09.
//
#include <pb2json.h>
#include "Log.h"
#include <Network.h>
#include <sys/socket.h>
#include <Config.h>
#include <co_routine_inner.h>
#include <stack>
#include <Msg.h>
#include <unistd.h>
#include <netinet/in.h>

using namespace std;

int g_listen_fd;
shared_ptr<tpc::Config::Config> g_config;
int g_nHashId = 0;
tpc::Config::Host g_self_host;

struct task_t
{
    stCoRoutine_t *co;
    int fd;
};
static stack<task_t*> g_readwrite;


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
        for (;;) {
            struct pollfd pf = {0};
            pf.fd = fd;
            pf.events = (POLLIN | POLLERR | POLLHUP);
            ret = co_poll(co_get_epoll_ct(), &pf, 1, 1000);
            if (ret == 0) {
                continue;
            }

            int msgType = msg.ReadOneMsg(fd);
            if (msgType < 0) {
                LOG_COUT << "fd=" << fd << " read err ret=" << msgType << LOG_ENDL_ERR;
                break;
            }

            tpc::Network::Msg reqMsg = msg.getMsg();
            if (msgType == tpc::Network::MsgType::MSG_Type_Rpc_Request) {
                LOG_COUT << "fd=" << fd << " reqMsg:" << tpc::Core::Utils::Msg2JsonStr(reqMsg) << LOG_ENDL;
            } else {
                LOG_COUT << "err type=" << msgType << LOG_ENDL_ERR;
                break;
            }

            tpc::Network::Msg resMsg;
            resMsg.set_msg_type(tpc::Network::MsgType::MSG_Type_Rpc_Response);
            tpc::Network::RpcRes *rpcRes = resMsg.mutable_rpc_response();
            rpcRes->set_result(0);
            ret = msg.SendMsg(fd, resMsg);
            if (ret < 0) {
                LOG_COUT << "send msg err ret=" << ret << LOG_ENDL_ERR;
                break;
            }
        }
        close(fd);

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
        LOG_COUT << "usage:" << argv[0] << " hash_id config.json" << LOG_ENDL;
        return -1;
    }

    int nHashId = atol(argv[1]);
    //get config
    tpc::Core::Config config(argv[2]);
    g_config = config.getConfig();
    for (int i = 0; i < g_config->host_size(); ++i) {
        if (nHashId == g_config->host(i).hash_id()) {
            g_self_host = g_config->host(i);
            break;
        }
        if (i == g_config->host_size()-1) {
            LOG_COUT << "ÅäÖÃÖÐÃ»ÓÐhash_id="<< nHashId << endl;
            return -1;
        }
    }
    g_nHashId = nHashId;

    //socket 
    g_listen_fd = tpc::Core::Network::CreateTcpSocket(g_self_host.port(), "", true);
    if (g_listen_fd < 0) {
        LOG_COUT << "CreateTcpSocket fd=" << g_listen_fd << LOG_ENDL_ERR;
        return g_listen_fd;
    }
    listen( g_listen_fd,1024 );
    if(g_listen_fd==-1){
        printf("Port %d is in use\n", g_listen_fd);
        return -1;
    }
    printf("listen %d :%d\n",g_listen_fd, g_self_host.port());
    tpc::Core::Network::SetNonBlock( g_listen_fd );

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

