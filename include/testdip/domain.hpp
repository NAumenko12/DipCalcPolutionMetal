#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace testdip {

struct GeoPoint {
    double latitude{};
    double longitude{};
};

struct Location {
    std::int64_t id{};
    std::string name;
    std::string site_number;
    double distance_from_source_km{};
    std::string description;
    double latitude{};
    double longitude{};
};

struct Metal {
    std::int64_t id{};
    std::string name;
    std::string symbol;
    std::string unit;
};

struct Sample {
    std::int64_t id{};
    std::int64_t location_id{};
    std::int64_t metal_id{};
    std::string position;
    int repetition{1};
    double value{};
    std::string sampling_date;
    std::string analytics_number;
};

struct GridPoint {
    double latitude{};
    double longitude{};
    double concentration{};
};

struct ConcentrationJob {
    std::string id;
    std::int64_t reference_location_id{};
    std::int64_t metal_id{};
    int sample_year{};
    double grid_step_km{0.7};
    double area_size_km{200.0};
    std::string status;
    std::string error_message;
    std::string created_at;
    std::string completed_at;
};

struct SampleStatistic {
    int year{};
    std::int64_t location_id{};
    std::int64_t metal_id{};
    std::string location_name;
    std::string site_number;
    std::string metal_name;
    std::string metal_symbol;
    double average_value{};
    double min_value{};
    double max_value{};
    std::int64_t sample_count{};
};

double parse_number(std::string_view input);
double distance_km(GeoPoint first, GeoPoint second);
double alpha_for_metal(std::string_view metal_name_or_symbol);
double concentration_at_distance(double c0, double r0, double r, double alpha);

std::vector<GridPoint> calculate_concentration_field(
    GeoPoint source_point,
    Location reference_point,
    Metal metal,
    double reference_concentration,
    double grid_step_km,
    double area_size_km);

} // namespace testdip
