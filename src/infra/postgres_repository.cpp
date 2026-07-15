#include "testdip/postgres_repository.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace testdip {
namespace {

template <typename Row>
std::string text_or_empty(const Row &row, const char *column_name) {
    const auto field = row[column_name];
    return field.is_null() ? std::string{} : field.template as<std::string>();
}

template <typename Row>
double double_or_zero(const Row &row, const char *column_name) {
    const auto field = row[column_name];
    return field.is_null() ? 0.0 : field.template as<double>();
}

template <typename Row>
int int_or_default(const Row &row, const char *column_name, int fallback) {
    const auto field = row[column_name];
    return field.is_null() ? fallback : field.template as<int>();
}

template <typename Row>
std::int64_t int64_or_zero(const Row &row, const char *column_name) {
    const auto field = row[column_name];
    return field.is_null() ? 0 : field.template as<std::int64_t>();
}

std::string generate_uuid() {
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<std::uint64_t> distribution;

    const auto high = distribution(generator);
    const auto low = distribution(generator);

    std::ostringstream output;
    output << std::hex << std::setfill('0')
           << std::setw(8) << ((high >> 32) & 0xffffffff) << "-"
           << std::setw(4) << ((high >> 16) & 0xffff) << "-"
           << std::setw(4) << (((high & 0x0fff) | 0x4000)) << "-"
           << std::setw(4) << (((low >> 48) & 0x3fff) | 0x8000) << "-"
           << std::setw(12) << (low & 0xffffffffffffULL);

    return output.str();
}

template <typename Row>
ConcentrationJob concentration_job_from_row(const Row &row) {
    return ConcentrationJob{
        .id = text_or_empty(row, "id"),
        .reference_location_id = int64_or_zero(row, "reference_location_id"),
        .metal_id = int64_or_zero(row, "metal_id"),
        .sample_year = int_or_default(row, "sample_year", 0),
        .grid_step_km = double_or_zero(row, "grid_step_km"),
        .area_size_km = double_or_zero(row, "area_size_km"),
        .status = text_or_empty(row, "status"),
        .error_message = text_or_empty(row, "error_message"),
        .created_at = text_or_empty(row, "created_at"),
        .completed_at = text_or_empty(row, "completed_at"),
    };
}

} // namespace

PostgresRepository::PostgresRepository(std::string connection_string)
    : connection_string_(std::move(connection_string)) {}

std::vector<Location> PostgresRepository::list_locations() const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec(R"SQL(
        SELECT
            id,
            name,
            site_number,
            distance_from_source_km,
            description,
            latitude,
            longitude
        FROM locations
        ORDER BY id
    )SQL");

    std::vector<Location> locations;
    locations.reserve(rows.size());

    for (const auto &row : rows) {
        locations.push_back(Location{
            .id = row["id"].as<std::int64_t>(),
            .name = text_or_empty(row, "name"),
            .site_number = text_or_empty(row, "site_number"),
            .distance_from_source_km = double_or_zero(row, "distance_from_source_km"),
            .description = text_or_empty(row, "description"),
            .latitude = double_or_zero(row, "latitude"),
            .longitude = double_or_zero(row, "longitude"),
        });
    }

    return locations;
}

std::vector<Metal> PostgresRepository::list_metals() const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec(R"SQL(
        SELECT id, name, symbol, unit
        FROM metals
        ORDER BY id
    )SQL");

    std::vector<Metal> metals;
    metals.reserve(rows.size());

    for (const auto &row : rows) {
        metals.push_back(Metal{
            .id = row["id"].as<std::int64_t>(),
            .name = text_or_empty(row, "name"),
            .symbol = text_or_empty(row, "symbol"),
            .unit = text_or_empty(row, "unit"),
        });
    }

    return metals;
}

SamplePage PostgresRepository::list_samples_for_location(const SampleQuery &query) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto total_rows = transaction.exec_params(R"SQL(
        SELECT COUNT(*) AS total
        FROM samples
        WHERE location_id = $1
          AND ($2 = 0 OR metal_id = $2)
          AND ($3 = 0 OR EXTRACT(YEAR FROM sampling_date)::INT = $3)
    )SQL",
        query.location_id,
        query.metal_id,
        query.year);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT
            id,
            location_id,
            metal_id,
            position,
            repetition,
            COALESCE(value, 0) AS value,
            sampling_date::TEXT AS sampling_date,
            analytics_number
        FROM samples
        WHERE location_id = $1
          AND ($2 = 0 OR metal_id = $2)
          AND ($3 = 0 OR EXTRACT(YEAR FROM sampling_date)::INT = $3)
        ORDER BY sampling_date, id
        LIMIT $4 OFFSET $5
    )SQL",
        query.location_id,
        query.metal_id,
        query.year,
        query.limit,
        query.offset);

    std::vector<Sample> samples;
    samples.reserve(rows.size());

    for (const auto &row : rows) {
        samples.push_back(Sample{
            .id = row["id"].as<std::int64_t>(),
            .location_id = row["location_id"].as<std::int64_t>(),
            .metal_id = row["metal_id"].as<std::int64_t>(),
            .position = text_or_empty(row, "position"),
            .repetition = int_or_default(row, "repetition", 1),
            .value = double_or_zero(row, "value"),
            .sampling_date = text_or_empty(row, "sampling_date"),
            .analytics_number = text_or_empty(row, "analytics_number"),
        });
    }

    return SamplePage{
        .items = std::move(samples),
        .total = total_rows[0]["total"].as<std::int64_t>(),
        .limit = query.limit,
        .offset = query.offset,
    };
}

std::vector<SampleStatistic> PostgresRepository::list_sample_statistics(const SampleStatisticsQuery &query) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT
            EXTRACT(YEAR FROM s.sampling_date)::INT AS sample_year,
            s.location_id,
            s.metal_id,
            l.name AS location_name,
            l.site_number,
            m.name AS metal_name,
            m.symbol AS metal_symbol,
            AVG(s.value)::DOUBLE PRECISION AS average_value,
            MIN(s.value)::DOUBLE PRECISION AS min_value,
            MAX(s.value)::DOUBLE PRECISION AS max_value,
            COUNT(*)::BIGINT AS sample_count
        FROM samples s
        JOIN locations l ON l.id = s.location_id
        JOIN metals m ON m.id = s.metal_id
        WHERE s.value IS NOT NULL
          AND ($1 = 0 OR s.location_id = $1)
          AND ($2 = 0 OR s.metal_id = $2)
          AND ($3 = 0 OR EXTRACT(YEAR FROM s.sampling_date)::INT >= $3)
          AND ($4 = 0 OR EXTRACT(YEAR FROM s.sampling_date)::INT <= $4)
        GROUP BY
            sample_year,
            s.location_id,
            s.metal_id,
            l.name,
            l.site_number,
            m.name,
            m.symbol
        ORDER BY sample_year, l.site_number, m.symbol
        LIMIT 1000
    )SQL",
        query.location_id,
        query.metal_id,
        query.year_from,
        query.year_to);

    std::vector<SampleStatistic> statistics;
    statistics.reserve(rows.size());

    for (const auto &row : rows) {
        statistics.push_back(SampleStatistic{
            .year = int_or_default(row, "sample_year", 0),
            .location_id = int64_or_zero(row, "location_id"),
            .metal_id = int64_or_zero(row, "metal_id"),
            .location_name = text_or_empty(row, "location_name"),
            .site_number = text_or_empty(row, "site_number"),
            .metal_name = text_or_empty(row, "metal_name"),
            .metal_symbol = text_or_empty(row, "metal_symbol"),
            .average_value = double_or_zero(row, "average_value"),
            .min_value = double_or_zero(row, "min_value"),
            .max_value = double_or_zero(row, "max_value"),
            .sample_count = int64_or_zero(row, "sample_count"),
        });
    }

    return statistics;
}

Location PostgresRepository::create_location(const Location &location) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        INSERT INTO locations (
            name,
            site_number,
            distance_from_source_km,
            description,
            latitude,
            longitude
        )
        VALUES ($1, $2, $3, $4, $5, $6)
        RETURNING
            id,
            name,
            site_number,
            distance_from_source_km,
            description,
            latitude,
            longitude
    )SQL",
        location.name,
        location.site_number,
        location.distance_from_source_km,
        location.description,
        location.latitude,
        location.longitude);

    transaction.commit();

    const auto row = rows[0];
    return Location{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .site_number = text_or_empty(row, "site_number"),
        .distance_from_source_km = double_or_zero(row, "distance_from_source_km"),
        .description = text_or_empty(row, "description"),
        .latitude = double_or_zero(row, "latitude"),
        .longitude = double_or_zero(row, "longitude"),
    };
}

Metal PostgresRepository::create_metal(const Metal &metal) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        INSERT INTO metals (name, symbol, unit)
        VALUES ($1, $2, NULLIF($3, ''))
        RETURNING id, name, symbol, unit
    )SQL",
        metal.name,
        metal.symbol,
        metal.unit);

    transaction.commit();

    const auto row = rows[0];
    return Metal{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .symbol = text_or_empty(row, "symbol"),
        .unit = text_or_empty(row, "unit"),
    };
}

Sample PostgresRepository::create_sample(const Sample &sample) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        INSERT INTO samples (
            metal_id,
            location_id,
            position,
            repetition,
            value,
            sampling_date,
            analytics_number
        )
        VALUES ($1, $2, $3, $4, $5, $6::DATE, NULLIF($7, ''))
        RETURNING
            id,
            location_id,
            metal_id,
            position,
            repetition,
            COALESCE(value, 0) AS value,
            sampling_date::TEXT AS sampling_date,
            analytics_number
    )SQL",
        sample.metal_id,
        sample.location_id,
        sample.position,
        sample.repetition,
        sample.value,
        sample.sampling_date,
        sample.analytics_number);

    transaction.commit();

    const auto row = rows[0];
    return Sample{
        .id = row["id"].as<std::int64_t>(),
        .location_id = row["location_id"].as<std::int64_t>(),
        .metal_id = row["metal_id"].as<std::int64_t>(),
        .position = text_or_empty(row, "position"),
        .repetition = int_or_default(row, "repetition", 1),
        .value = double_or_zero(row, "value"),
        .sampling_date = text_or_empty(row, "sampling_date"),
        .analytics_number = text_or_empty(row, "analytics_number"),
    };
}

std::optional<Location> PostgresRepository::update_location(std::int64_t id, const Location &location) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        UPDATE locations
        SET
            name = $2,
            site_number = $3,
            distance_from_source_km = $4,
            description = $5,
            latitude = $6,
            longitude = $7
        WHERE id = $1
        RETURNING
            id,
            name,
            site_number,
            distance_from_source_km,
            description,
            latitude,
            longitude
    )SQL",
        id,
        location.name,
        location.site_number,
        location.distance_from_source_km,
        location.description,
        location.latitude,
        location.longitude);

    transaction.commit();

    if (rows.empty()) {
        return std::nullopt;
    }

    const auto row = rows[0];
    return Location{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .site_number = text_or_empty(row, "site_number"),
        .distance_from_source_km = double_or_zero(row, "distance_from_source_km"),
        .description = text_or_empty(row, "description"),
        .latitude = double_or_zero(row, "latitude"),
        .longitude = double_or_zero(row, "longitude"),
    };
}

std::optional<Metal> PostgresRepository::update_metal(std::int64_t id, const Metal &metal) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        UPDATE metals
        SET
            name = $2,
            symbol = $3,
            unit = NULLIF($4, '')
        WHERE id = $1
        RETURNING id, name, symbol, unit
    )SQL",
        id,
        metal.name,
        metal.symbol,
        metal.unit);

    transaction.commit();

    if (rows.empty()) {
        return std::nullopt;
    }

    const auto row = rows[0];
    return Metal{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .symbol = text_or_empty(row, "symbol"),
        .unit = text_or_empty(row, "unit"),
    };
}

bool PostgresRepository::delete_location(std::int64_t id) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        DELETE FROM locations
        WHERE id = $1
        RETURNING id
    )SQL",
        id);

    transaction.commit();
    return !rows.empty();
}

bool PostgresRepository::delete_metal(std::int64_t id) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        DELETE FROM metals
        WHERE id = $1
        RETURNING id
    )SQL",
        id);

    transaction.commit();
    return !rows.empty();
}

std::optional<Sample> PostgresRepository::update_sample(std::int64_t id, const Sample &sample) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        UPDATE samples
        SET
            metal_id = $2,
            location_id = $3,
            position = $4,
            repetition = $5,
            value = $6,
            sampling_date = $7::DATE,
            analytics_number = NULLIF($8, '')
        WHERE id = $1
        RETURNING
            id,
            location_id,
            metal_id,
            position,
            repetition,
            COALESCE(value, 0) AS value,
            sampling_date::TEXT AS sampling_date,
            analytics_number
    )SQL",
        id,
        sample.metal_id,
        sample.location_id,
        sample.position,
        sample.repetition,
        sample.value,
        sample.sampling_date,
        sample.analytics_number);

    transaction.commit();

    if (rows.empty()) {
        return std::nullopt;
    }

    const auto row = rows[0];
    return Sample{
        .id = row["id"].as<std::int64_t>(),
        .location_id = row["location_id"].as<std::int64_t>(),
        .metal_id = row["metal_id"].as<std::int64_t>(),
        .position = text_or_empty(row, "position"),
        .repetition = int_or_default(row, "repetition", 1),
        .value = double_or_zero(row, "value"),
        .sampling_date = text_or_empty(row, "sampling_date"),
        .analytics_number = text_or_empty(row, "analytics_number"),
    };
}

bool PostgresRepository::delete_sample(std::int64_t id) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        DELETE FROM samples
        WHERE id = $1
        RETURNING id
    )SQL",
        id);

    transaction.commit();
    return !rows.empty();
}

ConcentrationJob PostgresRepository::create_concentration_job(const ConcentrationJob &job) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    const auto id = job.id.empty() ? generate_uuid() : job.id;
    const auto rows = transaction.exec_params(R"SQL(
        INSERT INTO concentration_jobs (
            id,
            reference_location_id,
            metal_id,
            sample_year,
            grid_step_km,
            area_size_km,
            status
        )
        VALUES ($1::UUID, $2, $3, $4, $5, $6, 'queued')
        RETURNING
            id::TEXT AS id,
            reference_location_id,
            metal_id,
            sample_year,
            grid_step_km,
            area_size_km,
            status,
            error_message,
            created_at::TEXT AS created_at,
            completed_at::TEXT AS completed_at
    )SQL",
        id,
        job.reference_location_id,
        job.metal_id,
        job.sample_year,
        job.grid_step_km,
        job.area_size_km);

    transaction.commit();
    return concentration_job_from_row(rows[0]);
}

std::vector<ConcentrationJob> PostgresRepository::list_concentration_jobs(int limit) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT
            id::TEXT AS id,
            reference_location_id,
            metal_id,
            sample_year,
            grid_step_km,
            area_size_km,
            status,
            error_message,
            created_at::TEXT AS created_at,
            completed_at::TEXT AS completed_at
        FROM concentration_jobs
        ORDER BY created_at DESC
        LIMIT $1
    )SQL",
        std::clamp(limit, 1, 500));

    std::vector<ConcentrationJob> jobs;
    jobs.reserve(rows.size());

    for (const auto &row : rows) {
        jobs.push_back(concentration_job_from_row(row));
    }

    return jobs;
}

std::optional<ConcentrationJob> PostgresRepository::get_concentration_job(const std::string &id) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT
            id::TEXT AS id,
            reference_location_id,
            metal_id,
            sample_year,
            grid_step_km,
            area_size_km,
            status,
            error_message,
            created_at::TEXT AS created_at,
            completed_at::TEXT AS completed_at
        FROM concentration_jobs
        WHERE id = $1::UUID
    )SQL",
        id);

    if (rows.empty()) {
        return std::nullopt;
    }

    return concentration_job_from_row(rows[0]);
}

void PostgresRepository::mark_concentration_job_failed(const std::string &id, const std::string &message) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    transaction.exec_params(R"SQL(
        UPDATE concentration_jobs
        SET
            status = 'publish_failed',
            error_message = $2,
            completed_at = now()
        WHERE id = $1::UUID
    )SQL",
        id,
        message);

    transaction.commit();
}

void PostgresRepository::mark_concentration_job_processing(const std::string &id) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    transaction.exec_params(R"SQL(
        UPDATE concentration_jobs
        SET
            status = 'processing',
            error_message = NULL,
            completed_at = NULL
        WHERE id = $1::UUID
    )SQL",
        id);

    transaction.commit();
}

void PostgresRepository::complete_concentration_job(const std::string &id, const std::vector<GridPoint> &points) const {
    pqxx::connection connection(connection_string_);
    pqxx::work transaction(connection);

    transaction.exec_params("DELETE FROM concentration_grid_points WHERE job_id = $1::UUID", id);

    for (const auto &point : points) {
        transaction.exec_params(R"SQL(
            INSERT INTO concentration_grid_points (
                job_id,
                latitude,
                longitude,
                concentration
            )
            VALUES ($1::UUID, $2, $3, $4)
        )SQL",
            id,
            point.latitude,
            point.longitude,
            point.concentration);
    }

    transaction.exec_params(R"SQL(
        UPDATE concentration_jobs
        SET
            status = 'completed',
            error_message = NULL,
            completed_at = now()
        WHERE id = $1::UUID
    )SQL",
        id);

    transaction.commit();
}

std::vector<GridPoint> PostgresRepository::list_concentration_grid_points(const std::string &id) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT latitude, longitude, concentration
        FROM concentration_grid_points
        WHERE job_id = $1::UUID
        ORDER BY latitude, longitude
    )SQL",
        id);

    std::vector<GridPoint> points;
    points.reserve(rows.size());

    for (const auto &row : rows) {
        points.push_back(GridPoint{
            .latitude = row["latitude"].as<double>(),
            .longitude = row["longitude"].as<double>(),
            .concentration = row["concentration"].as<double>(),
        });
    }

    return points;
}

std::optional<Location> PostgresRepository::get_location(std::int64_t id) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT
            id,
            name,
            site_number,
            distance_from_source_km,
            description,
            latitude,
            longitude
        FROM locations
        WHERE id = $1
    )SQL",
        id);

    if (rows.empty()) {
        return std::nullopt;
    }

    const auto row = rows[0];
    return Location{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .site_number = text_or_empty(row, "site_number"),
        .distance_from_source_km = double_or_zero(row, "distance_from_source_km"),
        .description = text_or_empty(row, "description"),
        .latitude = double_or_zero(row, "latitude"),
        .longitude = double_or_zero(row, "longitude"),
    };
}

std::optional<Metal> PostgresRepository::get_metal(std::int64_t id) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT id, name, symbol, unit
        FROM metals
        WHERE id = $1
    )SQL",
        id);

    if (rows.empty()) {
        return std::nullopt;
    }

    const auto row = rows[0];
    return Metal{
        .id = row["id"].as<std::int64_t>(),
        .name = text_or_empty(row, "name"),
        .symbol = text_or_empty(row, "symbol"),
        .unit = text_or_empty(row, "unit"),
    };
}

std::optional<double> PostgresRepository::get_metal_concentration(
    std::int64_t location_id,
    std::int64_t metal_id,
    int year) const {
    pqxx::connection connection(connection_string_);
    pqxx::read_transaction transaction(connection);

    const auto rows = transaction.exec_params(R"SQL(
        SELECT AVG(value)::DOUBLE PRECISION AS concentration
        FROM samples
        WHERE location_id = $1
          AND metal_id = $2
          AND EXTRACT(YEAR FROM sampling_date)::INT = $3
          AND value IS NOT NULL
    )SQL",
        location_id,
        metal_id,
        year);

    if (rows.empty() || rows[0]["concentration"].is_null()) {
        return std::nullopt;
    }

    return rows[0]["concentration"].as<double>();
}

} // namespace testdip
