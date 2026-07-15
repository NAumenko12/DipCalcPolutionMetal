#include "testdip/domain.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>

namespace testdip {

double parse_number(std::string_view input) {
    std::string cleaned;
    cleaned.reserve(input.size());

    for (const char value : input) {
        if ((value >= '0' && value <= '9') || value == '.' || value == ',') {
            cleaned.push_back(value == ',' ? '.' : value);
        }
    }

    if (cleaned.empty()) {
        return 0.0;
    }

    char *end = nullptr;
    const double parsed = std::strtod(cleaned.c_str(), &end);

    if (end == cleaned.c_str()) {
        return 0.0;
    }

    return std::round(parsed * 1000.0) / 1000.0;
}

} // namespace testdip
