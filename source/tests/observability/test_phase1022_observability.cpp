#include "fastseed/metrics/exploration.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace fastseed;
    crawler::ExplorationTelemetrySnapshot s{};
    s.scheduling_cycles = 10; s.exploration_budget = 20; s.exploration_used = 18;
    s.transport[0] = {10,12,9,1,0};
    s.transport[1] = {6,8,5,2,1};
    s.transport[2] = {4,4,4,0,0};
    const auto text = metrics::exploration_prometheus(s);
    assert(text.find("# TYPE fastseed_exploration_cycles_total counter") != std::string::npos);
    assert(text.find("fastseed_exploration_budget_total 20") != std::string::npos);
    assert(text.find("fastseed_exploration_selected_total{transport=\"tor\"} 5") != std::string::npos);
    assert(text.find("fastseed_exploration_starvation_windows_total{transport=\"tor\"} 1") != std::string::npos);
    assert(text.find("{transport=\"10.") == std::string::npos);
    std::cout << "phase1022 observability tests: PASS\n";
}
