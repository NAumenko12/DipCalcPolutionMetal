#include "testdip/domain.hpp"

#include <cmath>
#include <numbers>
#include <string_view>

namespace testdip {
namespace {

constexpr double earth_radius_km = 6371.0;
constexpr double plant_latitude = 67.923840;
constexpr double plant_longitude = 32.840962;

double radians(double degrees) {
    return degrees * std::numbers::pi / 180.0;
}

} // namespace

double distance_km(GeoPoint first, GeoPoint second) {
    const double lat1 = radians(first.latitude);
    const double lon1 = radians(first.longitude);
    const double lat2 = radians(second.latitude);
    const double lon2 = radians(second.longitude);

    const double d_lat = lat2 - lat1;
    const double d_lon = lon2 - lon1;

    const double a = std::sin(d_lat / 2.0) * std::sin(d_lat / 2.0) +
        std::cos(lat1) * std::cos(lat2) *
            std::sin(d_lon / 2.0) * std::sin(d_lon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return earth_radius_km * c;
}

double alpha_for_metal(std::string_view metal_name_or_symbol) {
    if (metal_name_or_symbol == "Pb" || metal_name_or_symbol == "Lead") {
        return 2.0;
    }
    if (metal_name_or_symbol == "Cd" || metal_name_or_symbol == "Cadmium") {
        return 1.8;
    }
    if (metal_name_or_symbol == "Hg" || metal_name_or_symbol == "Mercury") {
        return 1.5;
    }
    if (metal_name_or_symbol == "As" || metal_name_or_symbol == "Arsenic") {
        return 1.7;
    }
    if (metal_name_or_symbol == "Ni" || metal_name_or_symbol == "Nickel") {
        return 1.9;
    }

    return 1.8;
}

double concentration_at_distance(double c0, double r0, double r, double alpha) {
    return c0 * std::pow(r0 / r, alpha) *
        std::exp(-((r - r0) * (r - r0)) / (2.0 * r0 * r0));
}

std::vector<GridPoint> calculate_concentration_field(
    GeoPoint source_point,
    Location reference_point,
    Metal metal,
    double reference_concentration,
    double grid_step_km,
    double area_size_km) {
    std::vector<GridPoint> points;

    if (reference_point.distance_from_source_km <= 0.0 ||
        reference_concentration <= 0.0 ||
        grid_step_km <= 0.0 ||
        area_size_km <= 0.0) {
        return points;
    }

    const double alpha = alpha_for_metal(metal.symbol.empty() ? metal.name : metal.symbol);
    const double lat_step = grid_step_km / 110.574;
    const double lon_step = grid_step_km /
        (111.320 * std::cos(radians(source_point.latitude)));

    for (double lat = source_point.latitude - area_size_km / 110.574;
         lat <= source_point.latitude + area_size_km / 110.574;
         lat += lat_step) {
        const double lon_radius = area_size_km / (111.320 * std::cos(radians(lat)));

        for (double lon = source_point.longitude - lon_radius;
             lon <= source_point.longitude + lon_radius;
             lon += lon_step) {
            const double r = distance_km(source_point, GeoPoint{lat, lon});
            if (r < 0.1) {
                continue;
            }

            points.push_back(GridPoint{
                .latitude = lat,
                .longitude = lon,
                .concentration = concentration_at_distance(
                    reference_concentration,
                    reference_point.distance_from_source_km,
                    r,
                    alpha),
            });
        }
    }

    return points;
}

GeoPoint default_source_point() {
    return GeoPoint{plant_latitude, plant_longitude};
}

} // namespace testdip

