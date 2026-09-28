#pragma once
#include <cstddef>
namespace fastseed::protocol::limits {
inline constexpr std::size_t max_message_size = 4 * 1024 * 1024;
inline constexpr std::size_t max_user_agent_size = 256;
inline constexpr std::size_t max_version_payload = 1024;
inline constexpr std::size_t max_command_size = 12;
inline constexpr std::size_t max_host_size = 255;
inline constexpr std::size_t max_candidate_address_size = 512;
inline constexpr std::size_t max_dns_source_size = 255;
}
