#pragma once

#include "fastseed/protocol/constants.hpp"
#include "fastseed/crawler/crawler.hpp"
#include "fastseed/crawler/scheduler.hpp"
#include "fastseed/db/peer_database.hpp"
#include "fastseed/db/persistence_queue.hpp"
#include "fastseed/dns/dns_engine.hpp"
#include "fastseed/dns/native_server.hpp"
#include "fastseed/federation/federation.hpp"
#include "fastseed/health/peer_health.hpp"
#include "fastseed/intelligence/network_intelligence.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fastseed::config {

struct ValidationResult {
    bool ok{false};
    std::vector<std::string> errors;
};

struct NetworkConfig {
    std::string network{"mainnet"};
    std::uint16_t p2p_port{bsty::DEFAULT_P2P_PORT};
    std::int32_t protocol_version{bsty::PROTOCOL_VERSION};
    std::int32_t min_peer_version{bsty::MIN_PEER_VERSION};
};

struct BootstrapConfig {
    std::vector<std::string> dns_seeds{
        "seeder.globalboost.info", "seeder2.globalboost.info",
        "seeder3.globalboost.info", "dnsseed.globalboost.cc"};
};

struct DnsRuntimeConfig {
    dns::DnsConfig engine{};
    dns::NativeDnsServer::Config server{};
};

struct RuntimeConfig {
    NetworkConfig network{};
    BootstrapConfig bootstrap{};
    crawler::CrawlerConfig crawler{};
    crawler::AdaptiveSchedulerConfig scheduler{};
    health::HealthConfig health{};
    db::DatabaseConfig database{};
    db::PersistenceQueue::Config persistence{};
    DnsRuntimeConfig dns{};
    intelligence::Config intelligence{};
    federation::FederationConfig federation{};
    bool network_intelligence_enabled{true};

    static RuntimeConfig defaults();
};

ValidationResult validate(const RuntimeConfig& config);
ValidationResult validate_file(const std::string& path);

// Loads a complete typed configuration. On failure, returns std::nullopt and
// leaves the supplied error list populated with human-readable diagnostics.
std::optional<RuntimeConfig> load_file(const std::string& path,
                                       std::vector<std::string>& errors);

std::string version_string();

} // namespace fastseed::config
