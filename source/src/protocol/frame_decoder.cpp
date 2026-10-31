#include "fastseed/protocol/frame_decoder.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace fastseed::protocol {
FrameDecoder::FrameDecoder(std::size_t maximum_payload, bool require_mainnet_magic)
    : maximum_payload_(maximum_payload), require_mainnet_magic_(require_mainnet_magic) {
    if (maximum_payload > limits::max_message_size) throw std::invalid_argument("frame limit exceeds protocol maximum");
}
void FrameDecoder::reset() noexcept {
    header_used_ = 0; header_ = {}; failed_ = false;
    std::vector<std::uint8_t>().swap(payload_);
}
FrameResult FrameDecoder::consume(std::span<const std::uint8_t> input) {
    if (failed_) throw ProtocolError(ProtocolErrorCode::DecoderFailed, "reset failed frame decoder before reuse");
    FrameResult result;
    try {
        if (header_used_ < bsty::HEADER_SIZE) {
            const auto count = std::min(input.size(), bsty::HEADER_SIZE - header_used_);
            std::copy_n(input.begin(), count, header_bytes_.begin() + header_used_);
            header_used_ += count; result.consumed += count; input = input.subspan(count);
            if (header_used_ < bsty::HEADER_SIZE) return result;
            header_ = decode_header(header_bytes_);
            if (require_mainnet_magic_ && header_.magic != bsty::MAINNET_MAGIC)
                throw ProtocolError(ProtocolErrorCode::InvalidMagic, "invalid BSTY magic");
            if (header_.payload_size > maximum_payload_)
                throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "frame exceeds connection limit");
            payload_.reserve(header_.payload_size); // Only after all header checks.
        }
        const auto count = std::min(input.size(), std::size_t(header_.payload_size) - payload_.size());
        payload_.insert(payload_.end(), input.begin(), input.begin() + count);
        result.consumed += count;
        if (payload_.size() == header_.payload_size) {
            if (checksum(payload_) != header_.checksum)
                throw ProtocolError(ProtocolErrorCode::InvalidChecksum, "invalid checksum");
            result.message = Message{header_, std::move(payload_)};
            reset();
        }
        return result;
    } catch (...) {
        reset(); failed_ = true; throw;
    }
}
void FrameDecoder::finish() {
    if (failed_) throw ProtocolError(ProtocolErrorCode::DecoderFailed, "failed frame decoder");
    if (header_used_) {
        reset(); failed_ = true;
        throw ProtocolError(ProtocolErrorCode::Truncated, "EOF inside a frame");
    }
}
}
