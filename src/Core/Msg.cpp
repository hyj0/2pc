//
// Created by hyj on 2019-12-10.
//
#include "Utils.h"
#include <string>
#include <Log.h>
#include <poll.h>
#include <co_routine.h>
#include "Msg.h"
#include "Network.h"

#define __MSG_JSON 1

using namespace std;
//返回msgType, 失败返回<0
int tpc::Core::Msg::ReadOneMsg(int fd) {
    //todo:优化两次co_poll 超时时间设置
    struct pollfd pf = { 0 };
    pf.fd = fd;
    pf.events = (POLLIN|POLLERR|POLLHUP);
    co_poll( co_get_epoll_ct(),&pf, 1, 3*60*1000);

    string str = tpc::Core::Network::ReadBuff(fd, sizeof(Msghead));
    if (str.length() <= 0) {
        return -1;
    }
    Msghead *msghead = (Msghead *)str.c_str();
    int msgLen = msghead->msgLen;
    string strMsg = tpc::Core::Network::ReadBuff(fd, msgLen);
    msg.Clear();
#if __MSG_JSON
    Pb2Json::Json  json;
    json = Pb2Json::Json::parse(strMsg);
    Pb2Json::Json2Message(json, msg, true);
#else
    int ret = msg.ParseFromString(strMsg);
    if (ret < 0) {
        return ret;
    }
#endif
    return msg.msg_type();
}

int tpc::Core::Msg::SendMsg(int fd, tpc::Network::Msg &msg) {
    int ret;
    string outStr;
#if __MSG_JSON
    outStr = tpc::Core::Utils::Msg2JsonStr(msg);
//    LOG_COUT << "resMsg:" << outStr << LOG_ENDL;
#else
    ret = msg.SerializeToString(&outStr);
    if (ret != 0) {
        LOG_COUT << "SerializeToString err ret=" << ret << LOG_ENDL_ERR;
        LOG_COUT << "json=" << tpc::Core::Utils::Msg2JsonStr(msg) << LOG_ENDL;
        return ret;
    }
#endif

    tpc::Core::Msghead msghead;
    msghead.msgLen = outStr.size();
    char *buff = new char[sizeof(Msghead) + outStr.size()];
    memcpy(buff, &msghead, sizeof(Msghead));
    memcpy(buff+ sizeof(Msghead), outStr.c_str(), outStr.size());
    ret = tpc::Core::Network::SendBuff(fd, buff, sizeof(Msghead) + outStr.size());
    delete[](buff);
    if (ret != 0) {
        LOG_COUT << "SendBuff err ret=" << ret << LOG_ENDL_ERR;
        return ret;
    }
    return 0;
}

int tpc::Core::Msg::ReadOneMsg(int fd, tpc::Network::Msg &msg) {
    tpc::Core::Msg msg1;
    int ret = msg1.ReadOneMsg(fd);
    if (ret < 0) {
        return ret;
    }
    msg = msg1.getMsg1();
    return ret;
}
