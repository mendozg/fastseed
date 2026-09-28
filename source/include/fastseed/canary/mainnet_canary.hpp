#pragma once
#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/constants.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fastseed::canary {
struct CanaryPolicy {
    std::size_t max_targets{16};
    std::uint16_t required_port{bsty::DEFAULT_P2P_PORT};
    std::int32_t minimum_protocol{bsty::MIN_PEER_VERSION};
    bool require_mainnet_magic{true};
    bool permit_service_zero{true};
};

struct CanaryObservation {
    std::string target;
    protocol::ProbeResult result;
    bool accepted{false};
    std::string rejection_reason;
};

class MainnetCanary {
public:
    explicit MainnetCanary(CanaryPolicy policy = {});
    bool validate_target(std::string_view host, std::uint16_t port, std::string& reason) const;
    CanaryObservation evaluate(std::string target, std::uint16_t port, protocol::ProbeResult result) const;
    std::vector<std::string> bounded_targets(const std::vector<std::string>& targets) const;
    const CanaryPolicy& policy() const noexcept { return policy_; }
private:
    CanaryPolicy policy_;
};
}
