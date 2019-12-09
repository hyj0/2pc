//
// Created by hyj on 2019-12-09.
//
#include <rpc.pb.h>
#include <pb2json.h>
#include "Log.h"
#include <fstream>

int main() {
    tpc::Config::Config config;
    config.set_self_hash_id(1);
    tpc::Config::Host *host = config.add_host();
    host->set_hash_id(1);
    host->set_host("127.0.0.1");
    host->set_port(9801);

    config.add_host()->set_port(98002);

    config.add_host()->set_hash_id(3);

    Pb2Json::Json json;
    Pb2Json::Message2Json(config, json, true);

//    LOG_COUT << json << LOG_ENDL;

    ifstream infile("config.json");
    if (!infile) {
        LOG_COUT << "open config file" << LOG_ENDL_ERR;
        return -1;
    }
    string jsonStr;
    while (!infile.eof()) {
        char buff[1000];
        memset(buff, 0, sizeof(buff));
        infile.read(buff, 1000);
        jsonStr += buff;
    }
//    LOG_COUT << jsonStr << LOG_ENDL;
    Pb2Json::Json json1;
    json1 = json1.parse(jsonStr);
    LOG_COUT << json1 << LOG_ENDL;
    tpc::Config::Config config1;
    Pb2Json::Json2Message(json1, config1, true);

    Pb2Json::Json json2;
    Pb2Json::Message2Json(config1, json2, true);

    //todo:Pb2Json½âÎörepeated¶ªÊ§!!!  {"self_hash_id":1}
    LOG_COUT << json2 << LOG_ENDL;
    int len = json1["host"].size();
    for (int i = 0; i < len; ++i) {
        LOG_COUT << json1["host"][i] << LOG_ENDL;
    }
}

