#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/parser.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <boost/asio/steady_timer.hpp>
#include <algorithm>
#include <array>
#include <memory>
#include <sstream>

namespace fastseed::protocol {
namespace {
std::string endpoint_string(const std::string& host, std::uint16_t port) {
    if (host.find(':') != std::string::npos && !(host.size() >= 2 && host.front() == '[' && host.back() == ']'))
        return "[" + host + "]:" + std::to_string(port);
    return host + ":" + std::to_string(port);
}

std::vector<std::uint8_t> make_empty_message(std::string_view command) {
    MessageHeader h;
    h.magic = bsty::MAINNET_MAGIC;
    std::copy(command.begin(), command.end(), h.command.begin());
    h.payload_size = 0;
    h.checksum = checksum({});
    return encode_header(h);
}
}

void PeerProbe::run(const std::string& host, std::uint16_t port,
                    std::chrono::milliseconds ct,
                    std::chrono::milliseconds ht,
                    std::function<void(ProbeResult)> done) {
    struct State : std::enable_shared_from_this<State> {
        boost::asio::ip::tcp::resolver resolver;
        std::shared_ptr<boost::asio::ip::tcp::socket> sock;
        boost::asio::steady_timer timer;
        ProbeResult r;
        std::function<void(ProbeResult)> done;
        std::chrono::steady_clock::time_point started{};
        bool finished = false;
        bool exchange_wait = false;
        std::array<std::uint8_t, 24> hb{};
        std::vector<std::uint8_t> payload;
        std::chrono::milliseconds handshake_timeout{};
        std::chrono::milliseconds addr_exchange_timeout{750};

        State(boost::asio::io_context& i, std::function<void(ProbeResult)> d)
            : resolver(i), sock(std::make_shared<boost::asio::ip::tcp::socket>(i)), timer(i), done(std::move(d)) {}

        void finish(ProbeStatus s, std::string e = {}) {
            if (finished) return;
            finished = true;
            boost::system::error_code ignored;
            timer.cancel(ignored);
            sock->cancel(ignored);
            r.status = s;
            r.error = std::move(e);
            if (done) {
                auto cb = std::move(done);
                cb(std::move(r));
            }
        }

        void arm_timer(std::chrono::milliseconds d) {
            auto self = shared_from_this();
            timer.expires_after(d);
            timer.async_wait([self](boost::system::error_code ec) {
                if (ec || self->finished) return;
                if (self->exchange_wait) {
                    self->finish(ProbeStatus::Success);
                } else {
                    self->finish(ProbeStatus::Timeout, "operation timeout");
                }
            });
        }

        void connect(const std::string& host, std::uint16_t port,
                     std::chrono::milliseconds ct, std::chrono::milliseconds ht) {
            started = std::chrono::steady_clock::now();
            handshake_timeout = ht;
            r.endpoint = endpoint_string(host, port);
            arm_timer(ct);
            auto self = shared_from_this();
            resolver.async_resolve(host, std::to_string(port),
                [self](auto ec, auto res) {
                    if (self->finished) return;
                    if (ec) {
                        self->finish(ProbeStatus::ConnectionFailed, ec.message());
                        return;
                    }
                    boost::asio::async_connect(*self->sock, res,
                        [self](auto ec, auto) {
                            if (self->finished) return;
                            if (ec) {
                                self->finish(ProbeStatus::ConnectionFailed, ec.message());
                                return;
                            }
                            self->r.connect_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - self->started);
                            self->arm_timer(self->handshake_timeout);
                            self->send_version();
                        });
                });
        }

        void send_version() {
            NetworkAddress recv{0, sock->remote_endpoint().address(), sock->remote_endpoint().port()};
            NetworkAddress from{0, sock->local_endpoint().address(), sock->local_endpoint().port()};
            auto vm = encode_version(make_version(recv, from));
            MessageHeader h;
            h.magic = bsty::MAINNET_MAGIC;
            std::string c = "version";
            std::copy(c.begin(), c.end(), h.command.begin());
            h.payload_size = static_cast<std::uint32_t>(vm.size());
            h.checksum = checksum(vm);
            auto out = encode_header(h);
            out.insert(out.end(), vm.begin(), vm.end());
            auto self = shared_from_this();
            boost::asio::async_write(*sock, boost::asio::buffer(out),
                [self](auto ec, auto) {
                    if (ec) {
                        self->finish(ProbeStatus::HandshakeFailed, ec.message());
                        return;
                    }
                    self->read_version();
                });
        }

        void read_version() {
            auto self = shared_from_this();
            boost::asio::async_read(*sock, boost::asio::buffer(hb),
                [self](auto ec, auto) {
                    if (ec) {
                        self->finish(ProbeStatus::HandshakeFailed, ec.message());
                        return;
                    }
                    try {
                        auto h = decode_header(self->hb);
                        if (h.magic != bsty::MAINNET_MAGIC) {
                            self->finish(ProbeStatus::InvalidMagic, "wrong BSTY magic");
                            return;
                        }
                        if (command_string(h) != "version") {
                            self->finish(ProbeStatus::InvalidMessage, "expected version");
                            return;
                        }
                        self->payload.resize(h.payload_size);
                        boost::asio::async_read(*self->sock, boost::asio::buffer(self->payload),
                            [self, h](auto ec, auto) {
                                if (ec) {
                                    self->finish(ProbeStatus::HandshakeFailed, ec.message());
                                    return;
                                }
                                if (checksum(self->payload) != h.checksum) {
                                    self->finish(ProbeStatus::InvalidChecksum, "bad checksum");
                                    return;
                                }
                                try {
                                    auto v = decode_version(self->payload);
                                    if (v.version < bsty::MIN_PEER_VERSION) {
                                        self->finish(ProbeStatus::UnsupportedVersion, "peer protocol too old");
                                        return;
                                    }
                                    self->r.protocol_version = v.version;
                                    self->r.services = v.services;
                                    self->r.user_agent = v.user_agent;
                                    self->send_sendaddrv2();
                                } catch (const std::exception& e) {
                                    self->finish(ProbeStatus::InvalidMessage, e.what());
                                }
                            });
                    } catch (const ProtocolError& e) {
                        self->finish(ProbeStatus::InvalidMessage, e.what());
                    } catch (const std::exception& e) {
                        self->finish(ProbeStatus::InvalidMessage, e.what());
                    }
                });
        }

        void send_sendaddrv2() {
            auto out = make_empty_message("sendaddrv2");
            auto self = shared_from_this();
            boost::asio::async_write(*sock, boost::asio::buffer(out),
                [self](auto ec, auto) {
                    if (ec) {
                        self->finish(ProbeStatus::HandshakeFailed, ec.message());
                        return;
                    }
                    self->send_verack();
                });
        }

        void send_verack() {
            auto out = make_empty_message("verack");
            auto self = shared_from_this();
            boost::asio::async_write(*sock, boost::asio::buffer(out),
                [self](auto ec, auto) {
                    if (ec) {
                        self->finish(ProbeStatus::HandshakeFailed, ec.message());
                        return;
                    }
                    self->read_verack();
                });
        }

        void read_verack() {
            auto self = shared_from_this();
            auto buf = std::make_shared<std::array<std::uint8_t, 24>>();
            boost::asio::async_read(*sock, boost::asio::buffer(*buf),
                [self, buf](auto ec, auto) {
                    if (ec) {
                        self->finish(ProbeStatus::HandshakeFailed, ec.message());
                        return;
                    }
                    try {
                        auto h = decode_header(*buf);
                        if (h.magic != bsty::MAINNET_MAGIC) {
                            self->finish(ProbeStatus::InvalidMagic, "wrong magic");
                            return;
                        }
                        if (command_string(h) != "verack" || h.payload_size != 0) {
                            self->finish(ProbeStatus::InvalidMessage, "expected empty verack");
                            return;
                        }
                        if (h.checksum != checksum({})) {
                            self->finish(ProbeStatus::InvalidChecksum, "bad checksum");
                            return;
                        }
                        self->r.handshake_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - self->started);
                        self->send_getaddr();
                    } catch (const std::exception& e) {
                        self->finish(ProbeStatus::HandshakeFailed, e.what());
                    }
                });
        }

        void send_getaddr() {
            auto out = make_empty_message("getaddr");
            auto self = shared_from_this();
            boost::asio::async_write(*sock, boost::asio::buffer(out),
                [self](auto ec, auto) {
                    if (ec) {
                        // Handshake itself succeeded; an inability to request peer exchange
                        // should not turn a validated BSTY peer into a failed probe.
                        self->finish(ProbeStatus::Success);
                        return;
                    }
                    self->exchange_wait = true;
                    self->arm_timer(self->addr_exchange_timeout);
                    self->read_exchange_message();
                });
        }

        void read_exchange_message() {
            auto self = shared_from_this();
            auto buf = std::make_shared<std::array<std::uint8_t, 24>>();
            boost::asio::async_read(*sock, boost::asio::buffer(*buf),
                [self, buf](auto ec, auto) {
                    if (self->finished) return;
                    if (ec) {
                        // EOF/reset after a successful handshake is a valid end to the
                        // optional discovery window. Preserve the successful probe result.
                        if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
                            ec == boost::asio::error::operation_aborted) {
                            self->finish(ProbeStatus::Success);
                        } else {
                            self->finish(ProbeStatus::Success);
                        }
                        return;
                    }
                    try {
                        auto h = decode_header(*buf);
                        if (h.magic != bsty::MAINNET_MAGIC) {
                            self->finish(ProbeStatus::ProtocolError, "invalid magic during peer exchange");
                            return;
                        }
                        const auto command = command_string(h);
                        if (h.payload_size > limits::max_message_size) {
                            self->finish(ProbeStatus::ProtocolError, "exchange message too large");
                            return;
                        }
                        auto payload = std::make_shared<std::vector<std::uint8_t>>(h.payload_size);
                        boost::asio::async_read(*self->sock, boost::asio::buffer(*payload),
                            [self, h, command, payload](auto ec, auto) {
                                if (self->finished) return;
                                if (ec) {
                                    self->finish(ProbeStatus::Success);
                                    return;
                                }
                                if (checksum(*payload) != h.checksum) {
                                    self->finish(ProbeStatus::ProtocolError, "bad checksum during peer exchange");
                                    return;
                                }
                                try {
                                    if (command == "addrv2") {
                                        auto entries = decode_addrv2(*payload);
                                        self->r.discovered_addrv2.insert(self->r.discovered_addrv2.end(),
                                            std::make_move_iterator(entries.begin()),
                                            std::make_move_iterator(entries.end()));
                                    }
                                    // Ignore unrelated post-handshake messages and continue
                                    // until the bounded exchange window expires.
                                    self->read_exchange_message();
                                } catch (const std::exception& e) {
                                    self->finish(ProbeStatus::ProtocolError, e.what());
                                }
                            });
                    } catch (const std::exception& e) {
                        self->finish(ProbeStatus::ProtocolError, e.what());
                    }
                });
        }
    };

    auto s = std::make_shared<State>(io_, std::move(done));
    s->connect(host, port, ct, ht);
}

void PeerProbe::run_connected(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                              std::string endpoint,
                              std::chrono::milliseconds ht,
                              std::function<void(ProbeResult)> done) {
    struct ConnectedState : std::enable_shared_from_this<ConnectedState> {
        std::shared_ptr<boost::asio::ip::tcp::socket> sock;
        boost::asio::steady_timer timer;
        ProbeResult r;
        std::function<void(ProbeResult)> done;
        std::chrono::steady_clock::time_point started{std::chrono::steady_clock::now()};
        bool finished{false};
        bool exchange_wait{false};
        std::array<std::uint8_t,24> hb{};
        std::vector<std::uint8_t> payload;
        std::chrono::milliseconds addr_exchange_timeout{750};
        ConnectedState(boost::asio::io_context& io, std::shared_ptr<boost::asio::ip::tcp::socket> s,
                       std::string ep, std::function<void(ProbeResult)> d)
            : sock(std::move(s)), timer(io), done(std::move(d)) { r.endpoint=std::move(ep); }
        void finish(ProbeStatus st, std::string e={}) {
            if(finished) return; finished=true; boost::system::error_code x; timer.cancel(x);
            r.status=st; r.error=std::move(e); if(done){auto cb=std::move(done); cb(std::move(r));}
        }
        void arm(std::chrono::milliseconds d){auto self=shared_from_this(); timer.expires_after(d); timer.async_wait([self](auto ec){if(ec||self->finished)return; if(self->exchange_wait) self->finish(ProbeStatus::Success); else self->finish(ProbeStatus::Timeout,"operation timeout");});}
        std::vector<std::uint8_t> empty(std::string_view command){MessageHeader h;h.magic=bsty::MAINNET_MAGIC;std::copy(command.begin(),command.end(),h.command.begin());h.payload_size=0;h.checksum=checksum({});return encode_header(h);}
        void start(){ if(!sock || !sock->is_open()){finish(ProbeStatus::ConnectionFailed,"transport returned a closed socket");return;} r.connect_time=std::chrono::milliseconds(0); arm(std::chrono::milliseconds(2500)); send_version(); }
        void send_version(){
            NetworkAddress recv{0,sock->remote_endpoint().address(),sock->remote_endpoint().port()}; NetworkAddress from{0,sock->local_endpoint().address(),sock->local_endpoint().port()};
            auto vm=encode_version(make_version(recv,from)); MessageHeader h;h.magic=bsty::MAINNET_MAGIC;std::string c="version";std::copy(c.begin(),c.end(),h.command.begin());h.payload_size=static_cast<std::uint32_t>(vm.size());h.checksum=checksum(vm);auto out=encode_header(h);out.insert(out.end(),vm.begin(),vm.end());
            auto self=shared_from_this();boost::asio::async_write(*sock,boost::asio::buffer(out),[self](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}self->read_version();});
        }
        void read_version(){auto self=shared_from_this();boost::asio::async_read(*sock,boost::asio::buffer(hb),[self](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}try{auto h=decode_header(self->hb);if(h.magic!=bsty::MAINNET_MAGIC){self->finish(ProbeStatus::InvalidMagic,"wrong BSTY magic");return;}if(command_string(h)!="version"){self->finish(ProbeStatus::InvalidMessage,"expected version");return;}if(h.payload_size>limits::max_version_payload){self->finish(ProbeStatus::InvalidMessage,"version payload too large");return;}self->payload.resize(h.payload_size);boost::asio::async_read(*self->sock,boost::asio::buffer(self->payload),[self,h](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}if(checksum(self->payload)!=h.checksum){self->finish(ProbeStatus::InvalidChecksum,"bad checksum");return;}try{auto v=decode_version(self->payload);if(v.version<bsty::MIN_PEER_VERSION){self->finish(ProbeStatus::UnsupportedVersion,"peer protocol too old");return;}self->r.protocol_version=v.version;self->r.services=v.services;self->r.user_agent=v.user_agent;self->send_sendaddrv2();}catch(const std::exception&e){self->finish(ProbeStatus::InvalidMessage,e.what());}});}catch(const std::exception&e){self->finish(ProbeStatus::InvalidMessage,e.what());}});}
        void send_sendaddrv2(){auto out=empty("sendaddrv2");auto self=shared_from_this();boost::asio::async_write(*sock,boost::asio::buffer(out),[self](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}self->send_verack();});}
        void send_verack(){auto out=empty("verack");auto self=shared_from_this();boost::asio::async_write(*sock,boost::asio::buffer(out),[self](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}self->read_verack();});}
        void read_verack(){auto self=shared_from_this();auto b=std::make_shared<std::array<std::uint8_t,24>>();boost::asio::async_read(*sock,boost::asio::buffer(*b),[self,b](auto ec,auto){if(ec){self->finish(ProbeStatus::HandshakeFailed,ec.message());return;}try{auto h=decode_header(*b);if(h.magic!=bsty::MAINNET_MAGIC){self->finish(ProbeStatus::InvalidMagic,"wrong magic");return;}if(command_string(h)!="verack"||h.payload_size!=0){self->finish(ProbeStatus::InvalidMessage,"expected empty verack");return;}if(h.checksum!=checksum({})){self->finish(ProbeStatus::InvalidChecksum,"bad checksum");return;}self->r.handshake_time=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-self->started);self->send_getaddr();}catch(const std::exception&e){self->finish(ProbeStatus::HandshakeFailed,e.what());}});}
        void send_getaddr(){auto out=empty("getaddr");auto self=shared_from_this();boost::asio::async_write(*sock,boost::asio::buffer(out),[self](auto ec,auto){if(ec){self->finish(ProbeStatus::Success);return;}self->exchange_wait=true;self->arm(self->addr_exchange_timeout);self->read_exchange();});}
        void read_exchange(){auto self=shared_from_this();auto b=std::make_shared<std::array<std::uint8_t,24>>();boost::asio::async_read(*sock,boost::asio::buffer(*b),[self,b](auto ec,auto){if(self->finished)return;if(ec){self->finish(ProbeStatus::Success);return;}try{auto h=decode_header(*b);if(h.magic!=bsty::MAINNET_MAGIC){self->finish(ProbeStatus::ProtocolError,"invalid magic during peer exchange");return;}if(h.payload_size>limits::max_message_size){self->finish(ProbeStatus::ProtocolError,"exchange message too large");return;}auto p=std::make_shared<std::vector<std::uint8_t>>(h.payload_size);boost::asio::async_read(*self->sock,boost::asio::buffer(*p),[self,h,p](auto ec,auto){if(self->finished)return;if(ec){self->finish(ProbeStatus::Success);return;}if(checksum(*p)!=h.checksum){self->finish(ProbeStatus::ProtocolError,"bad checksum during peer exchange");return;}try{if(command_string(h)=="addrv2"){auto entries=decode_addrv2(*p);self->r.discovered_addrv2.insert(self->r.discovered_addrv2.end(),std::make_move_iterator(entries.begin()),std::make_move_iterator(entries.end()));}self->read_exchange();}catch(const std::exception&e){self->finish(ProbeStatus::ProtocolError,e.what());}});}catch(const std::exception&e){self->finish(ProbeStatus::ProtocolError,e.what());}});}
    };
    if(!socket){ProbeResult r;r.status=ProbeStatus::ConnectionFailed;r.endpoint=std::move(endpoint);r.error="null transport socket";boost::asio::post(io_,[done=std::move(done),r=std::move(r)]()mutable{done(std::move(r));});return;}
    auto s=std::make_shared<ConnectedState>(io_,std::move(socket),std::move(endpoint),std::move(done));
    s->arm(ht); s->send_version();
}
}
