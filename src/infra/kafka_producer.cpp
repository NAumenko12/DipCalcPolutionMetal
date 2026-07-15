#include "testdip/kafka_producer.hpp"

#include <librdkafka/rdkafka.h>
#include <librdkafka/rdkafkacpp.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace testdip {

KafkaProducer::KafkaProducer(const std::string &brokers) {
    std::string error;
    std::unique_ptr<RdKafka::Conf> config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    if (config->set("bootstrap.servers", brokers, error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("client.id", "testdip-api", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("message.timeout.ms", "5000", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    if (config->set("retry.backoff.ms", "250", error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka config error: " + error);
    }

    producer_.reset(RdKafka::Producer::create(config.get(), error));
    if (!producer_) {
        throw std::runtime_error("Kafka producer create error: " + error);
    }
}

KafkaProducer::~KafkaProducer() {
    if (producer_) {
        producer_->flush(2000);
    }
}

void KafkaProducer::publish(const std::string &topic, const std::string &key, const std::string &payload) {
    if (!producer_) {
        throw std::runtime_error("Kafka producer is not initialized");
    }

    const auto result = producer_->produce(
        topic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char *>(payload.data()),
        payload.size(),
        key.data(),
        key.size(),
        0,
        nullptr,
        nullptr);

    producer_->poll(0);

    if (result != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Kafka publish error: " + RdKafka::err2str(result));
    }

    const auto remaining = producer_->flush(5000);
    if (remaining != 0) {
        producer_->purge(RD_KAFKA_PURGE_F_QUEUE | RD_KAFKA_PURGE_F_INFLIGHT);
        producer_->poll(0);
        throw std::runtime_error("Kafka publish timeout: messages were not delivered");
    }
}

} // namespace testdip
