//
// Created by hyj on 2019-12-10.
//

#include "Config.h"

shared_ptr<tpc::Config::Config> tpc::Core::Config::getConfig() {
    shared_ptr<tpc::Config::Config> config = make_shared<tpc::Config::Config>();
    string jsonStr = tpc::Core::Utils::ReadWholeFile(sConfigFile_);
    Pb2Json::Json json;
    json = Pb2Json::Json::parse(jsonStr);
    Pb2Json::Json2Message(json, *config, true);
    return config;
}
