cmake_minimum_required(VERSION 3.22)

project(testdip_cpp_web LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka++)

add_library(testdip-domain
    src/domain/concentration_calculator.cpp
    src/domain/number_parser.cpp
)

target_include_directories(testdip-domain PUBLIC include)

enable_testing()

add_executable(testdip-domain-tests
    tests/domain_tests.cpp
)

target_link_libraries(testdip-domain-tests
    PRIVATE
        testdip-domain
)

add_test(NAME testdip-domain-tests COMMAND testdip-domain-tests)

add_executable(testdip-api
    src/api/main.cpp
    src/api/routes.cpp
    src/infra/config.cpp
    src/infra/kafka_producer.cpp
    src/infra/postgres_repository.cpp
)

target_link_libraries(testdip-api
    PRIVATE
        testdip-domain
        Drogon::Drogon
        PkgConfig::PQXX
        PkgConfig::RDKAFKA
)

if(APPLE AND EXISTS "/opt/homebrew/include/pqxx")
    target_include_directories(testdip-api PRIVATE /opt/homebrew/include)
    target_link_directories(testdip-api PRIVATE /opt/homebrew/lib)
endif()

add_executable(testdip-worker
    src/worker/main.cpp
    src/infra/config.cpp
    src/infra/kafka_producer.cpp
    src/infra/postgres_repository.cpp
)

target_link_libraries(testdip-worker
    PRIVATE
        testdip-domain
        PkgConfig::JSONCPP
        PkgConfig::PQXX
        PkgConfig::RDKAFKA
)

if(APPLE AND EXISTS "/opt/homebrew/include/pqxx")
    target_include_directories(testdip-worker PRIVATE /opt/homebrew/include)
    target_link_directories(testdip-worker PRIVATE /opt/homebrew/lib)
endif()
