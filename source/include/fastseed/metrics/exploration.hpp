#pragma once
#include "fastseed/crawler/scheduler.hpp"
#include <string>

namespace fastseed::metrics {

// Renders scheduler exploration telemetry as Prometheus text exposition.
// The exporter is observational and never mutates scheduler state.
std::string exploration_prometheus(const crawler::ExplorationTelemetrySnapshot& snapshot);

} // namespace fastseed::metrics
