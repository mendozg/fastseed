#include "fastseed/canary/mainnet_canary.hpp"
#include <algorithm>

namespace fastseed::canary {
MainnetCanary::MainnetCanary(CanaryPolicy policy) : policy_(policy) {}

bool MainnetCanary::validate_target(std::string_view host, std::uint16_t port, std::string& reason) const {
    if (host.empty()) { reason = "empty target"; return false; }
    if (host.size() > 255) { reason = "target too long"; return false; }
    if (port != policy_.required_port) { reason = "non-mainnet port"; return false; }
    reason.clear(); return true;
}

CanaryObservation MainnetCanary::evaluate(std::string target, std::uint16_t port, protocol::ProbeResult result) const {
    CanaryObservation o{std::move(target), std::move(result), false, {}};
    std::string reason;
    if (!validate_target(o.target, port, reason)) { o.rejection_reason = reason; return o; }
    if (o.result.status != protocol::ProbeStatus::Success) {
        o.rejection_reason = "probe did not validate BSTY handshake"; return o;
    }
    if (o.result.protocol_version < policy_.minimum_protocol) {
        o.rejection_reason = "peer protocol below minimum"; return o;
    }
    if (!policy_.permit_service_zero && o.result.services == 0) {
        o.rejection_reason = "peer advertises no services"; return o;
    }
    o.accepted = true; return o;
}

std::vector<std::string> MainnetCanary::bounded_targets(const std::vector<std::string>& targets) const {
    std::vector<std::string> out;
    out.reserve(std::min(policy_.max_targets, targets.size()));
    for (const auto& t : targets) {
        if (out.size() >= policy_.max_targets) break;
        if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    return out;
}
}
