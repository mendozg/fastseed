#include "fastseed/metrics/metrics.hpp"
#include <sstream>

namespace fastseed::metrics {
Counter& Registry::counter(std::string_view name) {
    std::lock_guard lock(mutex_); auto key=std::string(name); auto it=counters_.find(key);
    if(it==counters_.end()) it=counters_.emplace(key,std::make_unique<Counter>()).first;
    return *it->second;
}
Gauge& Registry::gauge(std::string_view name) {
    std::lock_guard lock(mutex_); auto key=std::string(name); auto it=gauges_.find(key);
    if(it==gauges_.end()) it=gauges_.emplace(key,std::make_unique<Gauge>()).first;
    return *it->second;
}
std::string Registry::prometheus() const {
    std::lock_guard lock(mutex_); std::ostringstream out;
    for(const auto& [name,c]:counters_) out<<name<<" "<<c->value()<<"\n";
    for(const auto& [name,g]:gauges_) out<<name<<" "<<g->value()<<"\n";
    return out.str();
}
void Registry::clear(){std::lock_guard lock(mutex_);counters_.clear();gauges_.clear();}
}
