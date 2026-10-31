#pragma once
#include "message.hpp"
#include "protocol_limits.hpp"
#include <optional>

namespace fastseed::protocol {
struct FrameResult {
    std::size_t consumed{};
    std::optional<Message> message;
};
// Connection-local, non-thread-safe decoder. Consume at most one frame per call;
// pass the unconsumed suffix again. No unbounded queue of decoded messages.
class FrameDecoder {
public:
    explicit FrameDecoder(std::size_t maximum_payload = limits::max_message_size,
                          bool require_mainnet_magic = true);
    FrameResult consume(std::span<const std::uint8_t> input);
    void finish(); // EOF is valid only at a frame boundary.
    void reset() noexcept;
    bool failed() const noexcept { return failed_; }
    std::size_t buffered_bytes() const noexcept { return header_used_ + payload_.size(); }
private:
    std::size_t maximum_payload_;
    bool require_mainnet_magic_;
    bool failed_{};
    std::array<std::uint8_t,24> header_bytes_{};
    std::size_t header_used_{};
    MessageHeader header_{};
    std::vector<std::uint8_t> payload_;
};
}
