#include "fastseed/metrics/logger.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>
namespace fastseed::metrics {
const char* level_name(LogLevel l) noexcept { switch(l){case LogLevel::Debug:return "DEBUG";case LogLevel::Info:return "INFO";case LogLevel::Warn:return "WARN";case LogLevel::Error:return "ERROR";} return "UNKNOWN"; }
void Logger::log(LogLevel level,std::string_view message){if(static_cast<int>(level)<static_cast<int>(minimum_))return;std::lock_guard lock(mutex_);auto now=std::chrono::system_clock::now();auto t=std::chrono::system_clock::to_time_t(now);std::tm tm{};
#if defined(_WIN32)
    _gmtime_s(&tm,&t);
#else
    gmtime_r(&t,&tm);
#endif
std::cerr<<"{\"ts\":\""<<std::put_time(&tm,"%Y-%m-%dT%H:%M:%SZ")<<"\",\"level\":\""<<level_name(level)<<"\",\"msg\":\"";for(char c:message){if(c=='\\'||c=='\"')std::cerr<<'\\';std::cerr<<c;}std::cerr<<"\"}\n";}
}
