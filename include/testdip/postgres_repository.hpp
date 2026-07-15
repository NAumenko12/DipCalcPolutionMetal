#pragma once

#include "testdip/domain.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace testdip {

struct SampleQuery {
    std::int64_t location_id{};
    std::int64_t metal_id{};
    int year{};
    int limit{100};
    int offset{};
};

struct SampleStatisticsQuery {
    std::int64_t location_id{};
    std::int64_t metal_id{};
    int year_from{};
    int year_to{};
};

struct SamplePage {
    std::vector<Sample> items;
    std::int64_t total{};
    int limit{};
    int offset{};
};

class PostgresRepository {
public:
    explicit PostgresRepository(std::string connection_string);

    std::vector<Location> list_locations() const;
    std::vector<Metal> list_metals() const;
    SamplePage list_samples_for_location(const SampleQuery &query) const;
    std::vector<SampleStatistic> list_sample_statistics(const SampleStatisticsQuery &query) const;
    Location create_location(const Location &location) const;
    Metal create_metal(const Metal &metal) const;
    Sample create_sample(const Sample &sample) const;
    std::optional<Location> update_location(std::int64_t id, const Location &location) const;
    std::optional<Metal> update_metal(std::int64_t id, const Metal &metal) const;
    bool delete_location(std::int64_t id) const;
    bool delete_metal(std::int64_t id) const;
    std::optional<Sample> update_sample(std::int64_t id, const Sample &sample) const;
    bool delete_sample(std::int64_t id) const;
    ConcentrationJob create_concentration_job(const ConcentrationJob &job) const;
    std::vector<ConcentrationJob> list_concentration_jobs(int limit = 100) const;
    std::optional<ConcentrationJob> get_concentration_job(const std::string &id) const;
    void mark_concentration_job_failed(const std::string &id, const std::string &message) const;
    void mark_concentration_job_processing(const std::string &id) const;
    void complete_concentration_job(const std::string &id, const std::vector<GridPoint> &points) const;
    std::vector<GridPoint> list_concentration_grid_points(const std::string &id) const;
    std::optional<Location> get_location(std::int64_t id) const;
    std::optional<Metal> get_metal(std::int64_t id) const;
    std::optional<double> get_metal_concentration(std::int64_t location_id, std::int64_t metal_id, int year) const;

private:
    std::string connection_string_;
};

} // namespace testdip
