//
// Created by hyj on 2019-12-10.
//

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <string>
#include <config.pb.h>
#include <pb2json.h>
#include "Utils.h"

using namespace std;

namespace tpc::Core {
    class Config {
    private:
        string sConfigFile_;
    public:
        Config(const string &sConfigFile_) : sConfigFile_(sConfigFile_) {}
        shared_ptr<tpc::Config::Config> getConfig();
    };
}



#endif //PROJECT_CONFIG_H
