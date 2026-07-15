#include "testdip/config.hpp"
#include "testdip/domain.hpp"
#include "testdip/kafka_producer.hpp"
#include "testdip/postgres_repository.hpp"

#include <json/json.h>
#include <librdkafka/rdkafkacpp.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace {

std::atomic_bool running{true};

void handle_signal(int) {
    running = false;
}

Json::Value parse_json(const std::string &payload) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream input(payload);

    if (!Json::parseFromStream(builder, input, &root, &errors)) {
        throw std::runtime_error("Invalid Kafka payload JSON: " + errors);
    }

    return root;
}

std::string string_field(const Json::Value &payload, const char *name) {
    if (!payload.isMember(name) || !payload[name].isString()) {
        throw std::runtime_error(std::string("Missing string field: ") + name);
    }

    return payload[name].asString();
}

std::int64_t int64_field(const Json::Value &payload, const char *name) {
    if (!payload.isMember(name) || !payload[name].isInt64()) {
        throw std::runtime_error(std::string("Missing int64 field: ") + name);
    }

    return payload[name].asInt64();
}

int int_field(const Json::Value &payload, const char *name) {
    if (!payload.isMember(name) || !payload[name].isInt()) {
        throw std::runtime_error(std::string("Missing int field: ") + name);
    }

    return payload[name].asInt();
}

double double_field(const Json::Value &payload, const char *name, double fallback) {
    return payload.isMember(name) && payload[name].isNumeric() ? payload[name].asDouble() : fallback;
}

std::string write_json(const Json::Value &payload) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, payload);
}

std::string completed_payload_for(
    const std::string &job_id,
    std::int64_t reference_location_id,
    std::int64_t metal_id,
    int year,
    std::size_t point_count,
    const std::string &status,
    const std::string &error_message = {}) {
    Json::Value payload;
    payload["eventType"] = "concentration.calculate.completed";
    payload["jobId"] = job_id;
    payload["referenceLocationId"] = Json::Int64(reference_location_id);
    payload["metalId"] = Json::Int64(metal_id);
    payload["year"] = year;
    payload["pointCount"] = Json::UInt64(point_count);
    payload["status"] = status;
    payload["errorMessage"] = error_message;
    return write_json(payload);
}

std::unique_ptr<RdKafka::KafkaConsumer> create_consumer(const std::string &brokers) {
    std::string error;
    std::unique_ptr<RdKafka::Conf> config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    if (config->set("bootstrap.servers", brokers, error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("group.id", "testdip-concentration-worker", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("enable.auto.commit", "false", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("auto.offset.reset", "earliest", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    std::unique_ptr<RdKafka::KafkaConsumer> consumer(RdKafka::KafkaConsumer::create(config.get(), error));
    if (!consumer) {
        throw std::runtime_error("Kafka consumer create error: " + error);
    }

    const auto topic_error = consumer->subscribe({"concentration.calculate.requested"});
    if (topic_error != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Kafka subscribe error: " + RdKafka::err2str(topic_error));
    }

    return consumer;
}

void process_message(
    testdip::PostgresRepository &repository,
    testdip::KafkaProducer &producer,
    const std::string &payload_text) {
    const auto payload = parse_json(payload_text);

    const auto job_id = string_field(payload, "jobId");
    const auto reference_location_id = int64_field(payload, "referenceLocationId");
    const auto metal_id = int64_field(payload, "metalId");
    const auto year = int_field(payload, "year");
    const auto grid_step_km = double_field(payload, "gridStepKm", 0.7);
    const auto area_size_km = double_field(payload, "areaSizeKm", 200.0);

    try {
        repository.mark_concentration_job_processing(job_id);

        const auto location = repository.get_location(reference_location_id);
        if (!location.has_value()) {
            throw std::runtime_error("Reference location not found");
        }

        const auto metal = repository.get_metal(metal_id);
        if (!metal.has_value()) {
            throw std::runtime_error("Metal not found");
        }

        const auto concentration = repository.get_metal_concentration(reference_location_id, metal_id, year);
        if (!concentration.has_value()) {
            throw std::runtime_error("Reference concentration not found");
        }

        const auto grid = testdip::calculate_concentration_field(
            testdip::GeoPoint{67.923840, 32.840962},
            *location,
            *metal,
            *concentration,
            grid_step_km,
            area_size_km);

        repository.complete_concentration_job(job_id, grid);
        producer.publish(
            "concentration.calculate.completed",
            job_id,
            completed_payload_for(
                job_id,
                reference_location_id,
                metal_id,
                year,
                grid.size(),
                "completed"));
        std::cout << "completed job " << job_id << " points=" << grid.size() << "\n";
    } catch (const std::exception &error) {
        repository.mark_concentration_job_failed(job_id, error.what());
        producer.publish(
            "concentration.calculate.completed",
            job_id,
            completed_payload_for(
                job_id,
                reference_location_id,
                metal_id,
                year,
                0,
                "failed",
                error.what()));
        std::cerr << "failed job " << job_id << ": " << error.what() << "\n";
    }
}

} // namespace

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const auto config = testdip::load_config();
    testdip::PostgresRepository repository(config.database_url);
    testdip::KafkaProducer producer(config.kafka_brokers);
    auto consumer = create_consumer(config.kafka_brokers);

    std::cout << "TESTDIP worker started, kafka=" << config.kafka_brokers << "\n";

    while (running) {
        std::unique_ptr<RdKafka::Message> message(consumer->consume(1000));

        switch (message->err()) {
        case RdKafka::ERR_NO_ERROR: {
            const std::string payload(
                static_cast<const char *>(message->payload()),
                message->len());
            process_message(repository, producer, payload);
            consumer->commitSync(message.get());
            break;
        }
        case RdKafka::ERR__TIMED_OUT:
            break;
        default:
            std::cerr << "Kafka consume error: " << message->errstr() << "\n";
            break;
        }
    }

    consumer->close();
    RdKafka::wait_destroyed(5000);

    return 0;
}
