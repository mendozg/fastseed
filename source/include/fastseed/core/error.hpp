#pragma once

#include <cstdint>
#include <string_view>

namespace fastseed::core {

// Internal error vocabulary. Values are not a wire-format contract.
enum class ErrorCode : std::uint8_t {
    none,
    invalid_argument,
    out_of_range,
    malformed_input,
    unsupported_version,
    resource_limit,
    io_failure
};

[[nodiscard]] constexpr std::string_view error_message(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::none: return "no error";
    case ErrorCode::invalid_argument: return "invalid argument";
    case ErrorCode::out_of_range: return "value out of range";
    case ErrorCode::malformed_input: return "malformed input";
    case ErrorCode::unsupported_version: return "unsupported version";
    case ErrorCode::resource_limit: return "resource limit reached";
    case ErrorCode::io_failure: return "I/O failure";
    }
    return "unknown error";
}

} // namespace fastseed::core
