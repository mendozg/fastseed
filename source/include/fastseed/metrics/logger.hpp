#pragma once
#include <mutex>
#include <string_view>

namespace fastseed::metrics {
enum class LogLevel { Debug, Info, Warn, Error };
class Logger {
public:
    explicit Logger(LogLevel minimum = LogLevel::Info) : minimum_(minimum) {}
    void log(LogLevel level, std::string_view message);
private:
    LogLevel minimum_;
    std::mutex mutex_;
};
const char* level_name(LogLevel level) noexcept;
}
