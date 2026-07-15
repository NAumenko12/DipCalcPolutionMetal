#pragma once

#include <string>

namespace testdip {

struct AppConfig {
    std::string database_url;
    std::string kafka_brokers;
    std::string frontend_root;
    int http_port{8080};
};

AppConfig load_config();

} // namespace testdip
