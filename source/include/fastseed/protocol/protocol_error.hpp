#pragma once
#include <stdexcept>
#include <string>
namespace fastseed::protocol {
enum class ProtocolErrorCode { InvalidHeader, InvalidMagic, InvalidCommand, MessageTooLarge, InvalidChecksum, Truncated, InvalidVersion, NonCanonical, TrailingData, InvalidPayload, DecoderFailed };
class ProtocolError : public std::runtime_error { public: ProtocolError(ProtocolErrorCode c, const std::string& m):std::runtime_error(m),code_(c){} ProtocolErrorCode code() const noexcept{return code_;} private: ProtocolErrorCode code_; };
}
