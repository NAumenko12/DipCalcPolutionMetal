#include "testdip/config.hpp"
#include "testdip/domain.hpp"
#include "testdip/kafka_producer.hpp"
#include "testdip/postgres_repository.hpp"

#include <drogon/drogon.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

namespace testdip::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;

namespace {

HttpResponsePtr json_response(Json::Value body, drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

HttpResponsePtr error_response(const std::string &message, drogon::HttpStatusCode status) {
    Json::Value body;
    body["error"] = message;
    return json_response(body, status);
}

bool has_non_empty_string(const Json::Value &body, const char *field) {
    return body.isMember(field) && body[field].isString() && !body[field].asString().empty();
}

std::string optional_string(const Json::Value &body, const char *field) {
    return body.isMember(field) && body[field].isString() ? body[field].asString() : std::string{};
}

double optional_double(const Json::Value &body, const char *field, double fallback = 0.0) {
    return body.isMember(field) && body[field].isNumeric() ? body[field].asDouble() : fallback;
}

int optional_int(const Json::Value &body, const char *field, int fallback = 0) {
    return body.isMember(field) && body[field].isInt() ? body[field].asInt() : fallback;
}

Json::Value to_json(const Location &location) {
    Json::Value item;
    item["id"] = Json::Int64(location.id);
    item["name"] = location.name;
    item["siteNumber"] = location.site_number;
    item["distanceFromSourceKm"] = location.distance_from_source_km;
    item["description"] = location.description;
    item["latitude"] = location.latitude;
    item["longitude"] = location.longitude;
    return item;
}

Json::Value to_json(const Metal &metal) {
    Json::Value item;
    item["id"] = Json::Int64(metal.id);
    item["name"] = metal.name;
    item["symbol"] = metal.symbol;
    item["unit"] = metal.unit;
    return item;
}

Json::Value to_json(const Sample &sample) {
    Json::Value item;
    item["id"] = Json::Int64(sample.id);
    item["locationId"] = Json::Int64(sample.location_id);
    item["metalId"] = Json::Int64(sample.metal_id);
    item["position"] = sample.position;
    item["repetition"] = sample.repetition;
    item["value"] = sample.value;
    item["samplingDate"] = sample.sampling_date;
    item["analyticsNumber"] = sample.analytics_number;
    return item;
}

Json::Value to_json(const ConcentrationJob &job) {
    Json::Value item;
    item["id"] = job.id;
    item["referenceLocationId"] = Json::Int64(job.reference_location_id);
    item["metalId"] = Json::Int64(job.metal_id);
    item["year"] = job.sample_year;
    item["gridStepKm"] = job.grid_step_km;
    item["areaSizeKm"] = job.area_size_km;
    item["status"] = job.status;
    item["errorMessage"] = job.error_message;
    item["createdAt"] = job.created_at;
    item["completedAt"] = job.completed_at;
    return item;
}

Json::Value to_json(const GridPoint &point) {
    Json::Value item;
    item["latitude"] = point.latitude;
    item["longitude"] = point.longitude;
    item["concentration"] = point.concentration;
    return item;
}

Json::Value to_json(const SampleStatistic &statistic) {
    Json::Value item;
    item["year"] = statistic.year;
    item["locationId"] = Json::Int64(statistic.location_id);
    item["metalId"] = Json::Int64(statistic.metal_id);
    item["locationName"] = statistic.location_name;
    item["siteNumber"] = statistic.site_number;
    item["metalName"] = statistic.metal_name;
    item["metalSymbol"] = statistic.metal_symbol;
    item["averageValue"] = statistic.average_value;
    item["minValue"] = statistic.min_value;
    item["maxValue"] = statistic.max_value;
    item["sampleCount"] = Json::Int64(statistic.sample_count);
    return item;
}

template <typename T>
Json::Value array_response(const std::vector<T> &items) {
    Json::Value body;
    body["items"] = Json::arrayValue;
    body["count"] = Json::UInt64(items.size());

    for (const auto &item : items) {
        body["items"].append(to_json(item));
    }

    return body;
}

int query_int(const HttpRequestPtr &request, const std::string &name, int fallback) {
    const auto value = request->getParameter(name);
    if (value.empty()) {
        return fallback;
    }

    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str()) {
        return fallback;
    }

    return static_cast<int>(parsed);
}

Json::Value sample_page_response(const SamplePage &page) {
    Json::Value body;
    body["items"] = Json::arrayValue;
    body["count"] = Json::UInt64(page.items.size());
    body["total"] = Json::Int64(page.total);
    body["limit"] = page.limit;
    body["offset"] = page.offset;

    for (const auto &item : page.items) {
        body["items"].append(to_json(item));
    }

    return body;
}

bool validate_location_json(const Json::Value &body, std::string &error) {
    if (!has_non_empty_string(body, "name") || !has_non_empty_string(body, "siteNumber")) {
        error = "Fields name and siteNumber are required";
        return false;
    }

    return true;
}

bool validate_sample_json(const Json::Value &body, std::string &error) {
    if (!body.isMember("metalId") || !body["metalId"].isInt64()) {
        error = "Field metalId is required";
        return false;
    }

    if (!body.isMember("value") || !body["value"].isNumeric()) {
        error = "Field value is required";
        return false;
    }

    if (!has_non_empty_string(body, "samplingDate")) {
        error = "Field samplingDate is required in YYYY-MM-DD format";
        return false;
    }

    return true;
}

bool validate_metal_json(const Json::Value &body, std::string &error) {
    if (!has_non_empty_string(body, "name") || !has_non_empty_string(body, "symbol")) {
        error = "Fields name and symbol are required";
        return false;
    }

    return true;
}

Location location_from_json(const Json::Value &body) {
    return Location{
        .name = body["name"].asString(),
        .site_number = body["siteNumber"].asString(),
        .distance_from_source_km = optional_double(body, "distanceFromSourceKm"),
        .description = optional_string(body, "description"),
        .latitude = optional_double(body, "latitude"),
        .longitude = optional_double(body, "longitude"),
    };
}

Metal metal_from_json(const Json::Value &body) {
    return Metal{
        .name = body["name"].asString(),
        .symbol = body["symbol"].asString(),
        .unit = optional_string(body, "unit"),
    };
}

Sample sample_from_json(const Json::Value &body, std::int64_t location_id) {
    return Sample{
        .location_id = location_id,
        .metal_id = body["metalId"].asInt64(),
        .position = optional_string(body, "position"),
        .repetition = optional_int(body, "repetition", 1),
        .value = body["value"].asDouble(),
        .sampling_date = body["samplingDate"].asString(),
        .analytics_number = optional_string(body, "analyticsNumber"),
    };
}

bool validate_concentration_job_json(const Json::Value &body, std::string &error) {
    if (!body.isMember("referenceLocationId") || !body["referenceLocationId"].isInt64()) {
        error = "Field referenceLocationId is required";
        return false;
    }

    if (!body.isMember("metalId") || !body["metalId"].isInt64()) {
        error = "Field metalId is required";
        return false;
    }

    if (!body.isMember("year") || !body["year"].isInt()) {
        error = "Field year is required";
        return false;
    }

    return true;
}

ConcentrationJob concentration_job_from_json(const Json::Value &body) {
    return ConcentrationJob{
        .reference_location_id = body["referenceLocationId"].asInt64(),
        .metal_id = body["metalId"].asInt64(),
        .sample_year = body["year"].asInt(),
        .grid_step_km = optional_double(body, "gridStepKm", 0.7),
        .area_size_km = optional_double(body, "areaSizeKm", 200.0),
    };
}

std::string kafka_payload_for(const ConcentrationJob &job) {
    Json::Value payload;
    payload["eventType"] = "concentration.calculate.requested";
    payload["jobId"] = job.id;
    payload["referenceLocationId"] = Json::Int64(job.reference_location_id);
    payload["metalId"] = Json::Int64(job.metal_id);
    payload["year"] = job.sample_year;
    payload["gridStepKm"] = job.grid_step_km;
    payload["areaSizeKm"] = job.area_size_km;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, payload);
}

} // namespace

void register_routes() {
    const auto config = load_config();
    auto repository = std::make_shared<PostgresRepository>(config.database_url);
    auto kafka_producer = std::make_shared<KafkaProducer>(config.kafka_brokers);

    drogon::app().registerHandler(
        "/health",
        [](const HttpRequestPtr &, std::function<void(const HttpResponsePtr &)> &&callback) {
            Json::Value body;
            body["status"] = "ok";
            callback(json_response(body));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/locations",
        [repository](const HttpRequestPtr &, std::function<void(const HttpResponsePtr &)> &&callback) {
            try {
                callback(json_response(array_response(repository->list_locations())));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/locations",
        [repository](const HttpRequestPtr &request, std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_location_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                callback(json_response(to_json(repository->create_location(location_from_json(*body))), drogon::k201Created));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/locations/{1}",
        [repository](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t location_id) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_location_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                const auto updated = repository->update_location(location_id, location_from_json(*body));
                if (!updated.has_value()) {
                    callback(error_response("Location not found", drogon::k404NotFound));
                    return;
                }

                callback(json_response(to_json(*updated)));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Put});

    drogon::app().registerHandler(
        "/api/locations/{1}",
        [repository](
            const HttpRequestPtr &,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t location_id) {
            try {
                if (!repository->delete_location(location_id)) {
                    callback(error_response("Location not found", drogon::k404NotFound));
                    return;
                }

                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k204NoContent);
                callback(response);
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Delete});

    drogon::app().registerHandler(
        "/api/metals",
        [repository](const HttpRequestPtr &, std::function<void(const HttpResponsePtr &)> &&callback) {
            try {
                callback(json_response(array_response(repository->list_metals())));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/metals",
        [repository](const HttpRequestPtr &request, std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_metal_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                callback(json_response(to_json(repository->create_metal(metal_from_json(*body))), drogon::k201Created));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/metals/{1}",
        [repository](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t metal_id) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_metal_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                const auto updated = repository->update_metal(metal_id, metal_from_json(*body));
                if (!updated.has_value()) {
                    callback(error_response("Metal not found", drogon::k404NotFound));
                    return;
                }

                callback(json_response(to_json(*updated)));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Put});

    drogon::app().registerHandler(
        "/api/metals/{1}",
        [repository](
            const HttpRequestPtr &,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t metal_id) {
            try {
                if (!repository->delete_metal(metal_id)) {
                    callback(error_response("Metal not found", drogon::k404NotFound));
                    return;
                }

                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k204NoContent);
                callback(response);
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Delete});

    drogon::app().registerHandler(
        "/api/statistics/samples",
        [repository](const HttpRequestPtr &request, std::function<void(const HttpResponsePtr &)> &&callback) {
            try {
                SampleStatisticsQuery query;
                query.location_id = std::max(query_int(request, "locationId", 0), 0);
                query.metal_id = std::max(query_int(request, "metalId", 0), 0);
                query.year_from = std::max(query_int(request, "yearFrom", 0), 0);
                query.year_to = std::max(query_int(request, "yearTo", 0), 0);

                callback(json_response(array_response(repository->list_sample_statistics(query))));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/locations/{1}/samples",
        [repository](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t location_id) {
            try {
                SampleQuery query;
                query.location_id = location_id;
                query.metal_id = std::max(query_int(request, "metalId", 0), 0);
                query.year = std::max(query_int(request, "year", 0), 0);
                query.limit = std::clamp(query_int(request, "limit", 100), 1, 500);
                query.offset = std::max(query_int(request, "offset", 0), 0);

                callback(json_response(sample_page_response(repository->list_samples_for_location(query))));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/locations/{1}/samples",
        [repository](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t location_id) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_sample_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                callback(json_response(to_json(repository->create_sample(sample_from_json(*body, location_id))), drogon::k201Created));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/samples/{1}",
        [repository](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t sample_id) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            if (!body->isMember("locationId") || !(*body)["locationId"].isInt64()) {
                callback(error_response("Field locationId is required", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_sample_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                const auto updated = repository->update_sample(
                    sample_id,
                    sample_from_json(*body, (*body)["locationId"].asInt64()));
                if (!updated.has_value()) {
                    callback(error_response("Sample not found", drogon::k404NotFound));
                    return;
                }

                callback(json_response(to_json(*updated)));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Put});

    drogon::app().registerHandler(
        "/api/samples/{1}",
        [repository](
            const HttpRequestPtr &,
            std::function<void(const HttpResponsePtr &)> &&callback,
            std::int64_t sample_id) {
            try {
                if (!repository->delete_sample(sample_id)) {
                    callback(error_response("Sample not found", drogon::k404NotFound));
                    return;
                }

                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k204NoContent);
                callback(response);
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Delete});

    drogon::app().registerHandler(
        "/api/calculations/concentration",
        [repository, kafka_producer](
            const HttpRequestPtr &request,
            std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (body == nullptr) {
                callback(error_response("Expected JSON request body", drogon::k400BadRequest));
                return;
            }

            std::string validation_error;
            if (!validate_concentration_job_json(*body, validation_error)) {
                callback(error_response(validation_error, drogon::k400BadRequest));
                return;
            }

            try {
                const auto job = repository->create_concentration_job(concentration_job_from_json(*body));
                try {
                    kafka_producer->publish(
                        "concentration.calculate.requested",
                        job.id,
                        kafka_payload_for(job));
                } catch (const std::exception &publish_error) {
                    repository->mark_concentration_job_failed(job.id, publish_error.what());
                    Json::Value error_body = to_json(job);
                    error_body["status"] = "publish_failed";
                    error_body["error"] = publish_error.what();
                    callback(json_response(error_body, drogon::k503ServiceUnavailable));
                    return;
                }

                callback(json_response(to_json(job), drogon::k202Accepted));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k503ServiceUnavailable));
            }
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/calculations",
        [repository](const HttpRequestPtr &request, std::function<void(const HttpResponsePtr &)> &&callback) {
            try {
                callback(json_response(array_response(repository->list_concentration_jobs(query_int(request, "limit", 100)))));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/calculations/{1}/points",
        [repository](
            const HttpRequestPtr &,
            std::function<void(const HttpResponsePtr &)> &&callback,
            const std::string &job_id) {
            try {
                const auto job = repository->get_concentration_job(job_id);
                if (!job.has_value()) {
                    callback(error_response("Calculation job not found", drogon::k404NotFound));
                    return;
                }

                callback(json_response(array_response(repository->list_concentration_grid_points(job_id))));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/calculations/preview",
        [](const HttpRequestPtr &, std::function<void(const HttpResponsePtr &)> &&callback) {
            Location location{
                .id = 1,
                .name = "Reference point",
                .site_number = "demo",
                .distance_from_source_km = 10.0,
                .description = "Demo calculation",
                .latitude = 67.95,
                .longitude = 32.9,
            };
            Metal metal{.id = 1, .name = "Lead", .symbol = "Pb", .unit = "mg/m3"};

            const auto grid = calculate_concentration_field(
                GeoPoint{67.923840, 32.840962},
                location,
                metal,
                1.2,
                5.0,
                20.0);

            Json::Value points(Json::arrayValue);
            for (const auto &point : grid) {
                Json::Value item;
                item["latitude"] = point.latitude;
                item["longitude"] = point.longitude;
                item["concentration"] = point.concentration;
                points.append(item);
            }

            Json::Value body;
            body["items"] = points;
            body["count"] = static_cast<Json::UInt64>(grid.size());
            callback(json_response(body));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/calculations/{1}",
        [repository](
            const HttpRequestPtr &,
            std::function<void(const HttpResponsePtr &)> &&callback,
            const std::string &job_id) {
            try {
                const auto job = repository->get_concentration_job(job_id);
                if (!job.has_value()) {
                    callback(error_response("Calculation job not found", drogon::k404NotFound));
                    return;
                }

                callback(json_response(to_json(*job)));
            } catch (const std::exception &error) {
                callback(error_response(error.what(), drogon::k500InternalServerError));
            }
        },
        {drogon::Get});
}

} // namespace testdip::api
