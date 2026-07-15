#include "testdip/domain.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool near(double actual, double expected, double epsilon = 1e-6) {
    return std::abs(actual - expected) <= epsilon;
}

void test_parse_number() {
    assert(near(testdip::parse_number("12.5"), 12.5));
    assert(near(testdip::parse_number("12,5"), 12.5));
    assert(near(testdip::parse_number("  0,001 "), 0.001));

    bool failed = false;
    try {
        (void)testdip::parse_number("abc");
    } catch (const std::invalid_argument &) {
        failed = true;
    }
    assert(failed);
}

void test_distance_km() {
    const auto same = testdip::distance_km({67.923840, 32.840962}, {67.923840, 32.840962});
    assert(near(same, 0.0));

    const auto distance = testdip::distance_km({67.923840, 32.840962}, {67.95, 32.9});
    assert(distance > 3.0);
    assert(distance < 5.0);
}

void test_concentration_at_distance() {
    const auto same_distance = testdip::concentration_at_distance(2.0, 10.0, 10.0, 0.1);
    assert(near(same_distance, 2.0));

    const auto farther = testdip::concentration_at_distance(2.0, 10.0, 20.0, 0.1);
    assert(farther > 0.0);
    assert(farther < 2.0);
}

void test_grid_generation() {
    const testdip::Location reference{
        .id = 1,
        .name = "Reference",
        .site_number = "R1",
        .distance_from_source_km = 5.0,
        .description = "",
        .latitude = 67.95,
        .longitude = 32.9,
    };
    const testdip::Metal metal{.id = 1, .name = "Lead", .symbol = "Pb", .unit = "mg/kg"};

    const auto points = testdip::calculate_concentration_field(
        {67.923840, 32.840962},
        reference,
        metal,
        1.5,
        10.0,
        20.0);

    assert(!points.empty());
    for (const auto &point : points) {
        assert(point.latitude >= -90.0);
        assert(point.latitude <= 90.0);
        assert(point.longitude >= -180.0);
        assert(point.longitude <= 180.0);
        assert(point.concentration >= 0.0);
    }
}

} // namespace

int main() {
    test_parse_number();
    test_distance_km();
    test_concentration_at_distance();
    test_grid_generation();

    std::cout << "domain tests passed\n";
    return 0;
}
