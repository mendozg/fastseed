#include "fastseed/protocol/message.hpp"
#include <openssl/sha.h>
namespace fastseed::protocol { std::array<std::uint8_t,4> checksum(std::span<const std::uint8_t> payload){ unsigned char a[SHA256_DIGEST_LENGTH],b[SHA256_DIGEST_LENGTH]; SHA256(payload.data(),payload.size(),a); SHA256(a,sizeof(a),b); return {b[0],b[1],b[2],b[3]}; } }
