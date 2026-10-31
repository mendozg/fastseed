cmake_minimum_required(VERSION 3.20)
project(BSTYFastSeed VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
option(FASTSEED_BUILD_FUZZ "Build libFuzzer target when supported" OFF)
option(FASTSEED_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
find_package(Boost 1.74 REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
add_library(fastseed
  src/protocol/serializer.cpp
  src/protocol/version_message.cpp
  src/protocol/message.cpp
  src/protocol/parser.cpp
  src/protocol/sha256.cpp
  src/protocol/probe.cpp
  src/protocol/wire_messages.cpp
  src/protocol/addrv2.cpp
  src/protocol/conformance.cpp
  src/crawler/candidate.cpp
  src/crawler/crawler.cpp
  src/crawler/scheduler.cpp
  src/crawler/adaptive_feedback.cpp
  src/health/peer_health.cpp
  src/db/peer_database.cpp
  src/db/persistence_queue.cpp
  src/dns/dns_engine.cpp
  src/dns/native_server.cpp
  src/dns/rate_limiter.cpp
  src/federation/federation.cpp
  src/metrics/metrics.cpp
  src/metrics/exploration.cpp
  src/metrics/logger.cpp
  src/config/runtime_config.cpp
  src/transport/transport.cpp
  src/intelligence/network_intelligence.cpp)
target_include_directories(fastseed PUBLIC include)
target_link_libraries(fastseed PUBLIC OpenSSL::Crypto SQLite::SQLite3 Threads::Threads)
target_include_directories(fastseed SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_compile_definitions(fastseed PUBLIC BOOST_ERROR_CODE_HEADER_ONLY BOOST_SYSTEM_NO_DEPRECATED)
target_compile_options(fastseed PRIVATE -Wall -Wextra -Wpedantic)
if(FASTSEED_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(fastseed PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(fastseed PUBLIC -fsanitize=address,undefined)
endif()
add_executable(bsty-fastseed src/main.cpp)
target_link_libraries(bsty-fastseed PRIVATE fastseed)
enable_testing()
add_executable(phase1_tests tests/test_phase1.cpp)
target_link_libraries(phase1_tests PRIVATE fastseed)
add_executable(phase11_tests tests/protocol/test_phase11.cpp)
add_executable(phase12_tests tests/protocol/test_phase12.cpp)
target_link_libraries(phase11_tests PRIVATE fastseed)
target_link_libraries(phase12_tests PRIVATE fastseed)
add_test(NAME phase1_tests COMMAND phase1_tests)
add_test(NAME phase11_tests COMMAND phase11_tests)
add_test(NAME phase12_tests COMMAND phase12_tests)
add_executable(phase13_tests tests/protocol/test_phase13.cpp)
target_link_libraries(phase13_tests PRIVATE fastseed)
add_test(NAME phase13_tests COMMAND phase13_tests)
add_executable(phase13_interop_tests tests/integration/test_phase13_interop.cpp)
target_link_libraries(phase13_interop_tests PRIVATE fastseed)
add_test(NAME phase13_interop_tests COMMAND phase13_interop_tests)

add_executable(phase2_tests tests/crawler/test_phase2.cpp)
target_link_libraries(phase2_tests PRIVATE fastseed)
add_test(NAME phase2_tests COMMAND phase2_tests)
add_executable(phase3_tests tests/health/test_phase3.cpp)
target_link_libraries(phase3_tests PRIVATE fastseed)
add_test(NAME phase3_tests COMMAND phase3_tests)
add_executable(phase4_tests tests/db/test_phase4.cpp)
target_link_libraries(phase4_tests PRIVATE fastseed)
add_test(NAME phase4_tests COMMAND phase4_tests)
add_executable(phase5_tests tests/dns/test_phase5.cpp)
target_link_libraries(phase5_tests PRIVATE fastseed)
add_test(NAME phase5_tests COMMAND phase5_tests)
add_executable(phase6_tests tests/dns/test_phase6.cpp)
target_link_libraries(phase6_tests PRIVATE fastseed)
add_test(NAME phase6_tests COMMAND phase6_tests)
add_executable(phase7_tests tests/federation/test_phase7.cpp)
target_link_libraries(phase7_tests PRIVATE fastseed)
add_test(NAME phase7_tests COMMAND phase7_tests)
if(FASTSEED_BUILD_FUZZ AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_executable(protocol_parser_fuzz fuzz/protocol_parser_fuzz.cpp)
  target_link_libraries(protocol_parser_fuzz PRIVATE fastseed)
  target_compile_options(protocol_parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(protocol_parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

add_executable(phase8_tests tests/observability/test_phase8.cpp)
target_link_libraries(phase8_tests PRIVATE fastseed)
add_test(NAME phase8_tests COMMAND phase8_tests)

add_executable(phase9_tests tests/release/test_phase9.cpp)
target_link_libraries(phase9_tests PRIVATE fastseed)
add_test(NAME phase9_tests COMMAND phase9_tests)

add_executable(phase101_tests tests/protocol/test_phase101.cpp)
target_link_libraries(phase101_tests PRIVATE fastseed)
add_test(NAME phase101_tests COMMAND phase101_tests)

add_executable(phase101_probe_tests tests/integration/test_phase101_probe.cpp)
target_link_libraries(phase101_probe_tests PRIVATE fastseed)
add_test(NAME phase101_probe_tests COMMAND phase101_probe_tests)

add_executable(phase102_tests tests/protocol/test_phase102.cpp)
target_link_libraries(phase102_tests PRIVATE fastseed)
add_test(NAME phase102_tests COMMAND phase102_tests)

add_executable(phase103_tests tests/crawler/test_phase103.cpp)
target_link_libraries(phase103_tests PRIVATE fastseed)
add_test(NAME phase103_tests COMMAND phase103_tests)

add_executable(phase104_addrv2_exchange_tests tests/integration/test_phase104_addrv2_exchange.cpp)
target_link_libraries(phase104_addrv2_exchange_tests PRIVATE fastseed)
add_test(NAME phase104_addrv2_exchange_tests COMMAND phase104_addrv2_exchange_tests)

add_executable(phase105_persistence_tests tests/db/test_phase105_persistence.cpp)
target_link_libraries(phase105_persistence_tests PRIVATE fastseed)
add_test(NAME phase105_persistence_tests COMMAND phase105_persistence_tests)

add_executable(phase106_transport_tests tests/transport/test_phase106_transport.cpp)
target_link_libraries(phase106_transport_tests PRIVATE fastseed)
add_test(NAME phase106_transport_tests COMMAND phase106_transport_tests)
add_executable(phase106_socks_tests tests/transport/test_phase106_socks.cpp)
target_link_libraries(phase106_socks_tests PRIVATE fastseed)
add_test(NAME phase106_socks_tests COMMAND phase106_socks_tests)
add_executable(phase106_sam_tests tests/transport/test_phase106_sam.cpp)
target_link_libraries(phase106_sam_tests PRIVATE fastseed)
add_test(NAME phase106_sam_tests COMMAND phase106_sam_tests)

add_executable(phase106_router_tests tests/transport/test_phase106_router.cpp)
target_link_libraries(phase106_router_tests PRIVATE fastseed)
add_test(NAME phase106_router_tests COMMAND phase106_router_tests)
add_executable(phase107_transport_crawler_tests tests/crawler/test_phase107_transport_crawler.cpp)
target_link_libraries(phase107_transport_crawler_tests PRIVATE fastseed)
add_test(NAME phase107_transport_crawler_tests COMMAND phase107_transport_crawler_tests)
add_executable(phase107_router_tests tests/transport/test_phase107_router.cpp)
target_link_libraries(phase107_router_tests PRIVATE fastseed)
add_test(NAME phase107_router_tests COMMAND phase107_router_tests)

add_executable(phase108_health_transport_tests tests/health/test_phase108_transport_health.cpp)
target_link_libraries(phase108_health_transport_tests PRIVATE fastseed)
add_test(NAME phase108_health_transport_tests COMMAND phase108_health_transport_tests)
add_executable(phase108_db_transport_tests tests/db/test_phase108_transport_persistence.cpp)
target_link_libraries(phase108_db_transport_tests PRIVATE fastseed)
add_test(NAME phase108_db_transport_tests COMMAND phase108_db_transport_tests)

add_executable(phase109_scheduler_tests tests/crawler/test_phase109_scheduler.cpp)
target_link_libraries(phase109_scheduler_tests PRIVATE fastseed)
add_test(NAME phase109_scheduler_tests COMMAND phase109_scheduler_tests)

add_executable(phase1011_feedback_tests tests/crawler/test_phase1011_feedback.cpp)
target_link_libraries(phase1011_feedback_tests PRIVATE fastseed)
add_test(NAME phase1011_feedback_tests COMMAND phase1011_feedback_tests)

add_executable(phase1010_adaptive_scheduler_tests tests/crawler/test_phase1010_adaptive.cpp)
target_link_libraries(phase1010_adaptive_scheduler_tests PRIVATE fastseed)
add_test(NAME phase1010_adaptive_scheduler_tests COMMAND phase1010_adaptive_scheduler_tests)

add_executable(phase1012_crawler_feedback_tests tests/crawler/test_phase1012_crawler_feedback.cpp)
target_link_libraries(phase1012_crawler_feedback_tests PRIVATE fastseed)
add_test(NAME phase1012_crawler_feedback_tests COMMAND phase1012_crawler_feedback_tests)

add_executable(phase1014_integrity_tests tests/db/test_phase1014_integrity.cpp)
target_link_libraries(phase1014_integrity_tests PRIVATE fastseed)
add_test(NAME phase1014_integrity_tests COMMAND phase1014_integrity_tests)

add_executable(phase1015_decay_tests tests/crawler/test_phase1015_decay.cpp)
target_link_libraries(phase1015_decay_tests PRIVATE fastseed)
add_test(NAME phase1015_decay_tests COMMAND phase1015_decay_tests)

add_executable(phase1016_confidence_tests tests/crawler/test_phase1016_confidence.cpp)
target_link_libraries(phase1016_confidence_tests PRIVATE fastseed)
add_test(NAME phase1016_confidence_tests COMMAND phase1016_confidence_tests)

add_executable(phase1017_confidence_scheduler_tests tests/crawler/test_phase1017_confidence_scheduler.cpp)
target_link_libraries(phase1017_confidence_scheduler_tests PRIVATE fastseed)
add_test(NAME phase1017_confidence_scheduler_tests COMMAND phase1017_confidence_scheduler_tests)

add_executable(phase1018_exploration_tests tests/crawler/test_phase1018_exploration.cpp)
target_link_libraries(phase1018_exploration_tests PRIVATE fastseed)
add_test(NAME phase1018_exploration_tests COMMAND phase1018_exploration_tests)

add_executable(phase1019_fairness_tests tests/crawler/test_phase1019_fairness.cpp)
target_link_libraries(phase1019_fairness_tests PRIVATE fastseed)
add_test(NAME phase1019_fairness_tests COMMAND phase1019_fairness_tests)

add_executable(phase1020_exploration_telemetry_tests tests/crawler/test_phase1020_exploration_telemetry.cpp)
target_link_libraries(phase1020_exploration_telemetry_tests PRIVATE fastseed)
add_test(NAME phase1020_exploration_telemetry_tests COMMAND phase1020_exploration_telemetry_tests)

add_executable(phase1022_observability_tests tests/observability/test_phase1022_observability.cpp)
target_link_libraries(phase1022_observability_tests PRIVATE fastseed)
add_test(NAME phase1022_observability_tests COMMAND phase1022_observability_tests)

add_executable(phase1023_integration_gate_tests tests/integration/test_phase1023_integration_gate.cpp)
target_link_libraries(phase1023_integration_gate_tests PRIVATE fastseed)
add_test(NAME phase1023_integration_gate_tests COMMAND phase1023_integration_gate_tests)

add_executable(phase1024_longrun_tests tests/integration/test_phase1024_longrun.cpp)
target_link_libraries(phase1024_longrun_tests PRIVATE fastseed)
add_test(NAME phase1024_longrun_tests COMMAND phase1024_longrun_tests)

add_executable(phase1025_failure_injection_tests tests/integration/test_phase1025_failure_injection.cpp)
target_link_libraries(phase1025_failure_injection_tests PRIVATE fastseed)
add_test(NAME phase1025_failure_injection_tests COMMAND phase1025_failure_injection_tests)

add_executable(phase1026_security_tests tests/integration/test_phase1026_security.cpp)
target_link_libraries(phase1026_security_tests PRIVATE fastseed)
add_test(NAME phase1026_security_tests COMMAND phase1026_security_tests)

add_executable(phase1028_federation_tests tests/federation/test_phase1028.cpp)
target_link_libraries(phase1028_federation_tests PRIVATE fastseed)
add_test(NAME phase1028_federation_tests COMMAND phase1028_federation_tests)

add_library(fastseed_canary src/canary/mainnet_canary.cpp)
target_include_directories(fastseed_canary PUBLIC include)
target_link_libraries(fastseed_canary PUBLIC fastseed)

add_executable(phase1029_mainnet_canary_tests tests/integration/test_phase1029_canary.cpp)
target_link_libraries(phase1029_mainnet_canary_tests PRIVATE fastseed_canary)
add_test(NAME phase1029_mainnet_canary_tests COMMAND phase1029_mainnet_canary_tests)

add_executable(phase11_native_dns_tests tests/dns/test_phase11_native_dns.cpp)
target_link_libraries(phase11_native_dns_tests PRIVATE fastseed)
add_test(NAME phase11_native_dns_tests COMMAND phase11_native_dns_tests)

add_executable(phase12_intelligence_tests tests/intelligence/test_phase12.cpp)
target_link_libraries(phase12_intelligence_tests PRIVATE fastseed)
add_test(NAME phase12_intelligence_tests COMMAND phase12_intelligence_tests)

include(GNUInstallDirs)
install(TARGETS bsty-fastseed RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY config/ DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/bsty-fastseed)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
add_executable(phase13_release_tests tests/integration/test_phase13_release.cpp)
target_link_libraries(phase13_release_tests PRIVATE fastseed_canary)
add_test(NAME phase13_release_tests COMMAND phase13_release_tests)

add_executable(config_tests tests/config/test_config.cpp)
target_link_libraries(config_tests PRIVATE fastseed)
add_test(NAME config_tests COMMAND config_tests)

add_executable(dns_compat_tests tests/dns/test_v101_compat.cpp)
target_link_libraries(dns_compat_tests PRIVATE fastseed)
add_test(NAME dns_compat_tests COMMAND dns_compat_tests)
set_tests_properties(dns_compat_tests PROPERTIES TIMEOUT 30)

add_executable(core_foundation_tests tests/core/test_foundation.cpp)
target_include_directories(core_foundation_tests PRIVATE include)
target_compile_options(core_foundation_tests PRIVATE -Wall -Wextra -Wpedantic)
if(FASTSEED_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(core_foundation_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(core_foundation_tests PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME core_foundation_tests COMMAND core_foundation_tests)

# Configuration tests read this relative to their build-directory working directory.
configure_file(config/fastseed.production.toml config/fastseed.production.toml COPYONLY)
# Existing suites use assert(); keep those checks active in Release builds too.
get_property(fastseed_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(t IN LISTS fastseed_targets)
  if(t MATCHES "_tests$")
    target_compile_options(${t} PRIVATE -UNDEBUG)
  endif()
endforeach()

set_tests_properties(phase101_probe_tests PROPERTIES TIMEOUT 60)
