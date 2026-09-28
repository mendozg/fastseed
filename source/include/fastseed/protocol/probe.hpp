#pragma once
#include "version_message.hpp"
#include "wire_messages.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <string>
#include <memory>
#include <functional>
namespace fastseed::protocol {
enum class ProbeStatus { Success, ConnectionFailed, Timeout, InvalidMagic, InvalidMessage, InvalidChecksum, UnsupportedVersion, HandshakeFailed, ProtocolError, TransportUnsupported, TransportFailed };
struct ProbeResult { ProbeStatus status{ProbeStatus::ProtocolError}; std::string endpoint; std::int32_t protocol_version{}; std::uint64_t services{}; std::string user_agent; std::chrono::milliseconds connect_time{}; std::chrono::milliseconds handshake_time{}; std::string error; std::vector<AddrV2Address> discovered_addrv2; };
class PeerProbe {
public:
 explicit PeerProbe(boost::asio::io_context& io) : io_(io) {}
 void run(const std::string& host, std::uint16_t port, std::chrono::milliseconds connect_timeout, std::chrono::milliseconds handshake_timeout, std::function<void(ProbeResult)> done);
 void run_connected(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                    std::string endpoint,
                    std::chrono::milliseconds handshake_timeout,
                    std::function<void(ProbeResult)> done);
private: boost::asio::io_context& io_;
};
}
