#include "fastseed/protocol/probe.hpp"
#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/version_message.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <exception>
#include <iostream>
#include <thread>

using namespace fastseed::protocol;
using namespace fastseed::bsty;
using boost::asio::ip::tcp;
using Bytes=std::vector<std::uint8_t>;

static Message read_message(tcp::socket& sock) {
    std::array<std::uint8_t,24> header{};
    boost::asio::read(sock,boost::asio::buffer(header));
    auto h=decode_header(header);
    Bytes payload(h.payload_size);
    boost::asio::read(sock,boost::asio::buffer(payload));
    assert(checksum(payload)==h.checksum);
    Bytes wire(header.begin(),header.end());wire.insert(wire.end(),payload.begin(),payload.end());
    return decode_message(wire);
}

static void scenario(const char* label,bool connected,const Bytes& reply,
                     ProbeStatus expected,bool fragmented=false,bool check_error=false) {
    boost::asio::io_context server_io;
    tcp::acceptor acceptor(server_io,{boost::asio::ip::address_v4::loopback(),0});
    const auto port=acceptor.local_endpoint().port();
    std::exception_ptr server_error;
    std::thread server([&]{
        try {
            tcp::socket sock(server_io);acceptor.accept(sock);
            const auto client_version=read_message(sock);
            assert(command_string(client_version.header)=="version");
            const auto v=decode_version(client_version.payload);assert(v.version==PROTOCOL_VERSION);
            const auto version=encode_message("version",encode_version(make_version(v.addr_recv,v.addr_from)));
            boost::asio::write(sock,boost::asio::buffer(version));
            const auto capability=read_message(sock);
            assert(command_string(capability.header)=="sendaddrv2" && capability.payload.empty());
            const auto ack=read_message(sock);
            assert(command_string(ack.header)=="verack" && ack.payload.empty());
            boost::system::error_code ec;
            if(fragmented) {
                for(std::size_t i=0;i<reply.size();++i) {
                    boost::asio::write(sock,boost::asio::buffer(reply.data()+i,1),ec);
                    if(ec) break;
                }
            } else if(!reply.empty()) boost::asio::write(sock,boost::asio::buffer(reply),ec);
            // Successful clients must actually finish the handshake and request peers.
            if(expected==ProbeStatus::Success) {
                assert(!ec);
                const auto request=read_message(sock);assert(command_string(request.header)=="getaddr");
            }
        } catch(...) {server_error=std::current_exception();}
    });
    boost::asio::io_context io;PeerProbe probe(io);ProbeResult result;bool completed=false;
    auto done=[&](ProbeResult r){result=std::move(r);completed=true;};
    if(connected) {
        auto socket=std::make_shared<tcp::socket>(io);
        socket->connect({boost::asio::ip::address_v4::loopback(),port});
        probe.run_connected(socket,"loopback",std::chrono::milliseconds(2500),done);
    } else probe.run("127.0.0.1",port,std::chrono::milliseconds(1500),std::chrono::milliseconds(2500),done);
    io.run();server.join();
    if(server_error)std::rethrow_exception(server_error);
    assert(completed);
    if(result.status!=expected)std::cerr<<label<<" connected="<<connected<<" error="<<result.error<<"\n";
    assert(result.status==expected);assert(result.protocol_version==PROTOCOL_VERSION);
    if(check_error)assert(result.error.find("received ping (8 bytes)")!=std::string::npos);
}
static Bytes concat(std::initializer_list<Bytes> parts) {
    Bytes out;for(const auto& p:parts)out.insert(out.end(),p.begin(),p.end());return out;
}
int main() {
    const auto ack=encode_message("verack",{});
    const auto addrv2=encode_message("sendaddrv2",{});
    const auto wtxid=encode_message("wtxidrelay",{});
    auto badsum=addrv2;badsum[20]^=1;
    auto badmagic=addrv2;badmagic[0]^=1;
    Bytes flood;for(int n=0;n<16;++n)flood.insert(flood.end(),addrv2.begin(),addrv2.end());
    flood.insert(flood.end(),ack.begin(),ack.end());
    for(bool connected:{false,true}) {
        scenario("plain verack",connected,ack,ProbeStatus::Success);
        scenario("addrv2 negotiation",connected,concat({addrv2,ack}),ProbeStatus::Success);
        scenario("both capabilities",connected,concat({wtxid,addrv2,ack}),ProbeStatus::Success);
        scenario("reverse capabilities",connected,concat({addrv2,wtxid,ack}),ProbeStatus::Success);
        scenario("fragmented messages",connected,concat({wtxid,addrv2,ack}),ProbeStatus::Success,true);
        scenario("capability is not verack",connected,addrv2,ProbeStatus::HandshakeFailed);
        scenario("no verack",connected,{},ProbeStatus::HandshakeFailed);
        scenario("capability payload",connected,encode_message("sendaddrv2",Bytes{1}),ProbeStatus::InvalidMessage);
        scenario("verack payload",connected,encode_message("verack",Bytes{1}),ProbeStatus::InvalidMessage);
        scenario("bad checksum",connected,concat({badsum,ack}),ProbeStatus::InvalidChecksum);
        scenario("bad magic",connected,concat({badmagic,ack}),ProbeStatus::InvalidMagic);
        scenario("unexpected command diagnosis",connected,encode_message("ping",Bytes(8)),ProbeStatus::InvalidMessage,false,true);
        scenario("bounded negotiation",connected,flood,ProbeStatus::InvalidMessage);
    }
    std::cout<<"Probe handshake interoperability: 26 direct/connected scenarios PASS\n";
}
