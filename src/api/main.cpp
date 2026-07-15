#include "testdip/config.hpp"

#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace testdip::api {
void register_routes();
}

namespace {

std::string resolve_frontend_root(const std::string &configured_root) {
    namespace fs = std::filesystem;

    if (!configured_root.empty()) {
        const fs::path configured(configured_root);
        if (fs::exists(configured / "dist" / "index.html")) {
            return (configured / "dist").string();
        }
        if (fs::exists(configured / "index.html")) {
            return configured_root;
        }
    }

    const fs::path current = fs::current_path();
    const fs::path candidates[] = {
        current / "frontend" / "dist",
        current.parent_path() / "frontend" / "dist",
        current / "frontend",
        current.parent_path() / "frontend",
    };

    for (const auto &candidate : candidates) {
        if (fs::exists(candidate / "index.html")) {
            return candidate.string();
        }
    }

    return configured_root.empty() ? "frontend" : configured_root;
}

} // namespace

int main() {
    const auto config = testdip::load_config();
    const auto frontend_root = resolve_frontend_root(config.frontend_root);

    testdip::api::register_routes();

    std::cout << "Serving frontend from: " << frontend_root << '\n';

    drogon::app()
        .setDocumentRoot(frontend_root)
        .addListener("0.0.0.0", config.http_port)
        .setThreadNum(4)
        .run();

    return 0;
}
