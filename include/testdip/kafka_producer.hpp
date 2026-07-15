#pragma once

#include <memory>
#include <string>

namespace RdKafka {
class Producer;
}

namespace testdip {

class KafkaProducer {
public:
    explicit KafkaProducer(const std::string &brokers);
    ~KafkaProducer();

    KafkaProducer(const KafkaProducer &) = delete;
    KafkaProducer &operator=(const KafkaProducer &) = delete;

    void publish(const std::string &topic, const std::string &key, const std::string &payload);

private:
    std::unique_ptr<RdKafka::Producer> producer_;
};

} // namespace testdip

