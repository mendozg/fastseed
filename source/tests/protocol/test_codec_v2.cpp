#include "fastseed/protocol/frame_decoder.hpp"
#include "fastseed/protocol/constants.hpp"
#include "known_vectors.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <random>
using namespace fastseed::protocol;
using namespace test_support;
int main() {
    Suite t;
    struct Case {const char* name; std::string_view payload, wire;};
    const Case cases[]={
        {"verack",vectors::verack_payload,vectors::verack_wire},
        {"sendaddrv2",vectors::sendaddrv2_payload,vectors::sendaddrv2_wire},
        {"wtxidrelay",vectors::wtxidrelay_payload,vectors::wtxidrelay_wire},
        {"getaddr",vectors::getaddr_payload,vectors::getaddr_wire},
        {"ping",vectors::ping_payload,vectors::ping_wire},
        {"pong",vectors::pong_payload,vectors::pong_wire},
        {"version",vectors::version_payload,vectors::version_wire},
        {"addr",vectors::addr_payload,vectors::addr_wire},
        {"addrv2",vectors::addrv2_payload,vectors::addrv2_wire}};
    for(auto c:cases) {
        const auto wire=hex(c.wire),payload=hex(c.payload);
        t.check(encode_message(c.name,payload)==wire,"independent frame encode");
        auto m=decode_message(wire);
        t.check(command_string(m.header)==c.name && m.payload==payload,"independent frame decode");
        t.check(encode_header(decode_header(std::span<const std::uint8_t>(wire).first(24)))==std::vector<std::uint8_t>(wire.begin(),wire.begin()+24),"header round trip");
        for(std::size_t split=0;split<=wire.size();++split) {
            FrameDecoder d;
            auto a=d.consume(std::span<const std::uint8_t>(wire).first(split));
            t.check(a.consumed==split,"split consumption");
            if(split<wire.size()) {
                t.check(!a.message,"incomplete frame is withheld");
                a=d.consume(std::span<const std::uint8_t>(wire).subspan(split));
                t.check(a.consumed==wire.size()-split,"remaining consumption");
            }
            t.check(a.message && a.message->payload==payload && command_string(a.message->header)==c.name,"all split points decode");
            t.check(!d.failed() && d.buffered_bytes()==0,"completed decoder resets");d.finish();
        }
    }
    auto ping=hex(vectors::ping_wire),verack=hex(vectors::verack_wire);
    auto joined=ping;joined.insert(joined.end(),verack.begin(),verack.end());
    FrameDecoder d;auto first=d.consume(joined);
    t.check(first.consumed==ping.size() && first.message && first.message->payload==hex(vectors::ping_payload),"coalesced first frame");
    auto second=d.consume(std::span<const std::uint8_t>(joined).subspan(first.consumed));
    t.check(second.consumed==verack.size() && second.message && second.message->payload.empty(),"coalesced second frame");d.finish();
    std::mt19937 random(142);
    for(unsigned run=0;run<32;++run) {
        d.reset();std::size_t position=0;unsigned delivered=0;
        while(position<joined.size()) {
            const auto size=std::min<std::size_t>(1+random()%13,joined.size()-position);
            auto result=d.consume(std::span<const std::uint8_t>(joined).subspan(position,size));
            t.check(result.consumed>0 && result.consumed<=size,"chunked forward progress");
            position+=result.consumed;
            if(result.message) ++delivered;
        }
        t.check(delivered==2,"chunked frame count");d.finish();
    }
    for(std::size_t size=1;size<ping.size();++size) {
        d.reset();d.consume(std::span<const std::uint8_t>(ping).first(size));
        t.error(ProtocolErrorCode::Truncated,[&]{d.finish();});
        t.check(d.failed() && d.buffered_bytes()==0,"partial EOF fails and releases data");
        t.error(ProtocolErrorCode::DecoderFailed,[&]{d.consume(verack);});
    }
    d.reset();t.check(!d.consume({}).message,"empty input");d.finish();
    auto bad=verack;bad[0]^=1;
    t.error(ProtocolErrorCode::InvalidMagic,[&]{decode_message(bad);});
    t.check(decode_message(bad,false).payload.empty(),"explicit non-mainnet mode");
    FrameDecoder permissive(limits::max_message_size,false);t.check(permissive.consume(bad).message.has_value(),"stream explicit non-mainnet mode");
    t.error(ProtocolErrorCode::InvalidMagic,[&]{d.consume(bad);});
    t.error(ProtocolErrorCode::DecoderFailed,[&]{d.consume(verack);});d.reset();
    t.check(d.consume(verack).message.has_value(),"reset permits reuse");
    for(auto byte:{0,0x1f,0x7f,0x80,0xff}) {
        bad=verack;bad[4]=static_cast<std::uint8_t>(byte);
        t.error(ProtocolErrorCode::InvalidCommand,[&]{decode_message(bad);});
        t.error(ProtocolErrorCode::InvalidCommand,[&]{decode_header(std::span<const std::uint8_t>(bad).first(24));});
    }
    bad=verack;bad[15]='x';t.error(ProtocolErrorCode::InvalidCommand,[&]{decode_message(bad);});
    bad=verack;std::fill(bad.begin()+4,bad.begin()+16,0);t.error(ProtocolErrorCode::InvalidCommand,[&]{decode_message(bad);});
    t.error(ProtocolErrorCode::InvalidCommand,[&]{encode_message("",{});});
    t.error(ProtocolErrorCode::InvalidCommand,[&]{encode_message("1234567890123",{});});
    t.error(ProtocolErrorCode::InvalidCommand,[&]{encode_message("bad\n",{});});
    t.error(ProtocolErrorCode::InvalidCommand,[&]{encode_message(std::string_view("bad\0cmd",7),{});});
    auto unknown=encode_message("123456789012",{});t.check(command_string(decode_message(unknown).header)=="123456789012","unknown full-width command retained");
    bad=ping;bad.back()^=1;t.error(ProtocolErrorCode::InvalidChecksum,[&]{decode_message(bad);});
    d.reset();t.error(ProtocolErrorCode::InvalidChecksum,[&]{d.consume(bad);});t.check(d.failed(),"checksum failure latches");
    bad=verack;std::fill(bad.begin()+16,bad.begin()+20,0xff);
    d.reset();t.error(ProtocolErrorCode::MessageTooLarge,[&]{d.consume(bad);});t.check(d.buffered_bytes()==0,"huge header fails before payload buffering");
    FrameDecoder small(7);t.error(ProtocolErrorCode::MessageTooLarge,[&]{small.consume(std::span<const std::uint8_t>(ping).first(24));});
    FrameDecoder zero(0);t.check(zero.consume(verack).message.has_value(),"zero cap allows empty messages");
    for(std::size_t n=0;n<ping.size();++n) t.error(ProtocolErrorCode::Truncated,[&]{decode_message(std::span<const std::uint8_t>(ping).first(n));});
    t.error(ProtocolErrorCode::TrailingData,[&]{decode_message(joined);});
    std::vector<std::uint8_t> maximum(limits::max_message_size,0x55);
    auto large=encode_message("future",maximum);FrameDecoder bounded;
    t.check(bounded.consume(large).message->payload==maximum,"maximum payload accepted");
    maximum.push_back(0);t.error(ProtocolErrorCode::MessageTooLarge,[&]{encode_message("future",maximum);});
    t.pass("Alpha2 frame codec");
}
