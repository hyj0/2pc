//
// Created by hyj on 2019-12-09.
//
#include <rpc.pb.h>
#include <pb2json.h>
#include "Log.h"
#include <fstream>
#include <storage.pb.h>

#include <string>
#include <co_routine_inner.h>
#include <Network.h>
#include <unistd.h>
#include <Msg.h>
#include <Utils.h>

using namespace std;

void printUsage() {
    cout << "usage:" << endl;
    cout << "begin" << endl;
    cout << "get key" << endl;
    cout << "get_for_update key" << endl;
    cout << "delete key" << endl;
    cout << "update key data" << endl;
    cout << "insert key data" << endl;
    cout << "commit" << endl;
    cout << "rollback" << endl ;
    cout << "hash key size" << endl;
    cout << endl;
}

void *mainCoroutine(void *arg)
{
    char **argv = static_cast<char **>(arg);
    string host = argv[1];
    int port = atoi(argv[2]);
    int fd = tpc::Core::Network::Connect(host, port);
    if (fd < 0) {
        LOG_COUT << "Connect err ret=" << fd << LOG_ENDL_ERR;
        exit(-1);
        return NULL;
    }
    while (true) {
        int ret;

        struct pollfd pf[2];
        memset(pf, 0, sizeof(pf));
        pf[0].fd = 0;
        pf[0].events = (POLLIN | POLLERR | POLLHUP);
        pf[1].fd = fd;
        pf[1].events = (POLLIN | POLLERR | POLLHUP);
        ret = co_poll(co_get_epoll_ct(), pf, 2, 1000);
        if (ret == 0) {
            continue;
        }

        if (pf[1].revents) {
            tpc::Core::Msg msgCl;
            ret = msgCl.ReadOneMsg(fd);
            if (ret < 0) {
                LOG_COUT << "ReadOneMsg ret=" << ret << endl;
                close(fd);
                LOG_COUT << "reconnect" << LOG_ENDL;
                fd = tpc::Core::Network::Connect(host, port);
                if (fd < 0) {
                    LOG_COUT << "Connect err ret=" << fd << LOG_ENDL_ERR;
                    exit(-1);
                }
                continue;
            }
            tpc::Network::Msg resMsg = msgCl.getMsg();
            string resStr = tpc::Core::Utils::Msg2JsonStr(resMsg);
            cout << resStr << endl;
            continue;
        }

        char buff[1000];
        memset(buff, 0, 1000);
        int n = read(0, buff, 1000);
        if (n < 0) {
            LOG_COUT << " read err fd=" << 0 << LOG_ENDL_ERR;
            continue;
        }
        char args[4][100];
        n = sscanf(buff, "%s %s %s %s", args[0], args[1], args[2], args[3]);
        if (n <= 0) {
            printUsage();
            continue;
        }
        tpc::Network::Msg reqMsg;
        reqMsg.set_msg_type(tpc::Network::MsgType::MSG_Type_Cli_Request);
        tpc::Network::CliReq *cliReq = reqMsg.mutable_cli_request();
        if ((strcmp(args[0], "begin") == 0 || strcmp(args[0], "b") == 0) && n == 1) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Begin);
        } else if ((strcmp(args[0], "commit") == 0 || strcmp(args[0], "c") == 0) && n == 1) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Commit);
        } else if ((strcmp(args[0], "rollback") == 0 || strcmp(args[0], "r") == 0) && n == 1) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Rollback);
        } else if ((strcmp(args[0], "get") == 0 || strcmp(args[0], "g") == 0) && n == 2) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Get);
            cliReq->set_key(args[1]);
        } else if ((strcmp(args[0], "get_for_update") == 0 || strcmp(args[0], "gf") == 0) && n == 2) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Get);
            cliReq->set_key(args[1]);
            cliReq->set_get_for_update(1);
        } else if ((strcmp(args[0], "update") == 0 || strcmp(args[0], "u") == 0) && n == 3) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Update);
            cliReq->set_key(args[1]);
            cliReq->set_value(args[2]);
        } else if ((strcmp(args[0], "insert") == 0 || strcmp(args[0], "i") == 0) && n == 3) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Insert);
            cliReq->set_key(args[1]);
            cliReq->set_value(args[2]);
        } else if ((strcmp(args[0], "delete") == 0 || strcmp(args[0], "d") == 0) && n == 2) {
            cliReq->set_request_type(tpc::Network::RequestType::Req_Type_Delete);
            cliReq->set_key(args[1]);
        } else if (strcmp(args[0], "hash") == 0 && n == 3) {
            cout << tpc::Core::Utils::GetHash(args[1], atol(args[2]))<<endl;
            continue;
        } else {
            printUsage();
            continue;
        }

        ret = tpc::Core::Msg::SendMsg(fd, reqMsg);
        if (ret != 0) {
            LOG_COUT << "sendMsg err ret=" << ret << LOG_ENDL_ERR;
            continue;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        LOG_COUT << "usage:" << argv[0] << " ip port" << LOG_ENDL;
        return -1;
    }

    stCoRoutine_t *ctx = NULL;
    co_create(&ctx, NULL, mainCoroutine, argv);
    co_resume(ctx);

    co_eventloop(co_get_epoll_ct(), NULL, NULL);
}

