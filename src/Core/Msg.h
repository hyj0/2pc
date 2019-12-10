//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_MSG_H
#define PROJECT_MSG_H

#include <rpc.pb.h>

namespace tpc::Core {
    struct Msghead {
        unsigned int msgLen;
    };
    class Msg {
    private:
        tpc::Network::Msg msg;
    public:
        Msg() {}
        virtual ~Msg() {}
        //·µ»ØmsgType, Ê§°Ü·µ»Ø<0
        int ReadOneMsg(int fd);
        static int SendMsg(int fd, tpc::Network::Msg &msg);

        const tpc::Network::Msg &getMsg() const {
            return msg;
        }

        tpc::Network::Msg getMsg1() const {
            return msg;
        }
        //·µ»ØmsgType, Ê§°Ü·µ»Ø<0
        static int ReadOneMsg(int fd, tpc::Network::Msg &msg);
    };
}



#endif //PROJECT_MSG_H
