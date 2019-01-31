#include <chrono>
#include <algorithm>

#include <folly/dynamic.h>
#include <folly/json.h>

#include "NetworkState.h"


using namespace std::chrono;


NetworkState::NetworkState(std::shared_ptr<MasternodeConfig> config) {
    config_ = config;
    httpClient_ = std::make_unique<httplib::Client>(
        config_->gateway_address.c_str(),
        config->gateway_port,
        config_->gateway_poll_interval /* timeout in seconds */
    );
}

NetworkState::~NetworkState() {
    fs.shutdown();
}

std::vector<std::string> NetworkState::getEdgeNodes() {
    return edgeNodes_.copy();
}

// void NetworkState::parseStateUpdate(std::string body) {
//     // todo: add heartbeat check, non-empty content check, lowercase hostname/eth address
//     folly::dynamic parsed = folly::parseJson(body);
//     auto lockedList = edgeNodes_.wlock();
//     lockedList->clear();
//     auto nodeMap = parsed["response"]["node_data_map"];
//     for (auto& value : nodeMap.values()) {
//         try {
//             std::string ip = value["ip_address"]["data"].getString();
//             std::string port = value["content_port"]["data"].getString();
//             lockedList->push_back(std::string(ip + ":" + port));
//         } catch (const std::exception& e) {
//             LOG(ERROR) << "Caught exception when parsing network state: " << e.what() << "\n";
//         }
//     }
// }

void NetworkState::parseStateUpdate(std::string body) {
    

    folly::dynamic state = folly::parseJson(body); // full json state
    auto lockedList = edgeNodes_.wlock();
    lockedList->clear();
    auto nodeMap = state["response"]["node_data_map"]; // map of content nodes
    int64_t time = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count(); // current time in ms
    for (auto& pair : nodeMap.items()) {
        std::string nodeAddress = pair.first.getString(); // eth address of content node
        std::transform(nodeAddress.begin(), nodeAddress.end(),
            nodeAddress.begin(), ::tolower); // lower case eth address of content node
        auto& value = pair.second;
        try {
            std::string ip = value["ip_address"]["data"].getString();
            std::string port = value["content_port"]["data"].getString();
            int64_t heartbeat = value["heartbeat"]["data"].asInt();
            bool hasNoContent = value["disk_content"]["data"].empty();
            if (hasNoContent || (time - heartbeat) > (2 * 1000 * 60)) {
                continue;
            }
            lockedList->push_back(std::string(nodeAddress + ":" + port));
        } catch (const std::exception& e) {
            LOG(ERROR) << "Caught exception when parsing network state: " << e.what() << "\n";
        }
    }
}

void NetworkState::beginPollingGateway() {
    fs.addFunction([&] {
        LOG(INFO) << "Fetching network state from gateway...\n";
        // Make a GET request to the Gladius network gateway
        // to fetch state
        auto res = httpClient_->Get("/api/p2p/state");
        if (res && res->status == 200) {
            LOG(INFO) << "Received network state from gateway\n";
            // parse the JSON into state structs/classes
            parseStateUpdate(res->body);
        }
    }, std::chrono::seconds(config_->gateway_poll_interval), "GatewayPoller");
    fs.setSteady(true);
    fs.start();
    LOG(INFO) << "Started network state polling thread...\n";
}
