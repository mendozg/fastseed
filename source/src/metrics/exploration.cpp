#include "fastseed/metrics/exploration.hpp"
#include <array>
#include <sstream>

namespace fastseed::metrics {

std::string exploration_prometheus(const crawler::ExplorationTelemetrySnapshot& s) {
    static constexpr std::array<const char*,3> names{"direct","tor","i2p"};
    std::ostringstream out;
    out << "# HELP fastseed_exploration_cycles_total Number of scheduler cycles that allocated exploration capacity.\n"
        << "# TYPE fastseed_exploration_cycles_total counter\n"
        << "fastseed_exploration_cycles_total " << s.scheduling_cycles << "\n"
        << "# HELP fastseed_exploration_budget_total Total exploration slots allocated.\n"
        << "# TYPE fastseed_exploration_budget_total counter\n"
        << "fastseed_exploration_budget_total " << s.exploration_budget << "\n"
        << "# HELP fastseed_exploration_selected_total Total exploration probes selected.\n"
        << "# TYPE fastseed_exploration_selected_total counter\n"
        << "fastseed_exploration_selected_total " << s.exploration_used << "\n";
    out << "# HELP fastseed_exploration_budget_allocated_total Exploration budget opportunities assigned to a transport.\n"
        << "# TYPE fastseed_exploration_budget_allocated_total counter\n";
    for (std::size_t i=0;i<3;++i) out << "fastseed_exploration_budget_allocated_total{transport=\"" << names[i] << "\"} " << s.transport[i].budget_allocated << "\n";
    out << "# HELP fastseed_exploration_opportunities_total Low-confidence exploration opportunities observed.\n"
        << "# TYPE fastseed_exploration_opportunities_total counter\n";
    for (std::size_t i=0;i<3;++i) out << "fastseed_exploration_opportunities_total{transport=\"" << names[i] << "\"} " << s.transport[i].opportunities << "\n";
    out << "# HELP fastseed_exploration_selected_total Exploration probes selected by transport.\n"
        << "# TYPE fastseed_exploration_selected_total counter\n";
    for (std::size_t i=0;i<3;++i) out << "fastseed_exploration_selected_total{transport=\"" << names[i] << "\"} " << s.transport[i].probes_selected << "\n";
    out << "# HELP fastseed_exploration_rejected_total Exploration candidates rejected by transport admission limits.\n"
        << "# TYPE fastseed_exploration_rejected_total counter\n";
    for (std::size_t i=0;i<3;++i) out << "fastseed_exploration_rejected_total{transport=\"" << names[i] << "\"} " << s.transport[i].quota_rejections << "\n";
    out << "# HELP fastseed_exploration_starvation_windows_total Transport exploration windows with eligible work but no selected probe.\n"
        << "# TYPE fastseed_exploration_starvation_windows_total counter\n";
    for (std::size_t i=0;i<3;++i) out << "fastseed_exploration_starvation_windows_total{transport=\"" << names[i] << "\"} " << s.transport[i].starvation_windows << "\n";
    return out.str();
}

} // namespace fastseed::metrics
