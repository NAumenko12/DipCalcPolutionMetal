#include "testdip/config.hpp"

#include <cstdlib>
#include <string>

namespace testdip {
namespace {

std::string env_or_default(const char *name, std::string fallback) {
    const char *value = std::getenv(name);
    return value == nullptr || std::string(value).empty() ? std::move(fallback) : value;
}

} // namespace

AppConfig load_config() {
    AppConfig config;
    config.database_url = env_or_default(
        "TESTDIP_DATABASE_URL",
        "postgres://testdip:testdip@localhost:5432/testdip");
    config.kafka_brokers = env_or_default("TESTDIP_KAFKA_BROKERS", "localhost:9092");
    config.frontend_root = env_or_default("TESTDIP_FRONTEND_ROOT", "");
    config.http_port = std::stoi(env_or_default("TESTDIP_HTTP_PORT", "8080"));
    return config;
}

} // namespace testdip
