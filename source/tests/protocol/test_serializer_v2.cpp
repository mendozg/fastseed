#include "fastseed/protocol/serializer.hpp"
#include "test_support.hpp"
#include <limits>
using namespace fastseed::protocol;
using namespace test_support;
int main() {
    Suite t;
    Writer w;
    w.u8(0xab); w.u16(0x1234); w.be16(8226); w.u32(0x12345678); w.i32(-2); w.u64(0x1122334455667788ULL);
    t.check(w.data() == hex("ab3412202278563412feffffff8877665544332211"), "integer byte order");
    Reader r(w.data());
    t.check(r.u8()==0xab && r.u16()==0x1234 && r.be16()==8226 && r.u32()==0x12345678 && r.i32()==-2 && r.u64()==0x1122334455667788ULL, "integer decode");
    r.require_end();
    struct Case { std::uint64_t value; const char* wire; };
    const Case cases[]={{0,"00"},{252,"fc"},{253,"fdfd00"},{65535,"fdffff"},{65536,"fe00000100"},{0xffffffffULL,"feffffffff"},{0x100000000ULL,"ff0000000001000000"},{UINT64_MAX,"ffffffffffffffffff"}};
    for (auto c:cases) {
        const auto bytes=hex(c.wire); Writer a; a.compact_size(c.value);
        t.check(a.data()==bytes,"canonical CompactSize bytes");
        Reader b(bytes); t.check(b.compact_size()==c.value && !b.remaining(),"CompactSize decode");
        for (std::size_t n=0;n<bytes.size();++n) {
            Reader short_read(std::span<const std::uint8_t>(bytes).first(n));
            t.error(ProtocolErrorCode::Truncated,[&]{short_read.compact_size();});
            t.check(short_read.remaining()==n,"truncated CompactSize is transactional");
        }
    }
    for (const char* s:{"fdfc00","feffff0000","ffffffffff00000000"}) {
        auto b=hex(s); Reader a(b);
        t.error(ProtocolErrorCode::NonCanonical,[&]{a.compact_size();});
        t.check(a.remaining()==b.size(),"noncanonical rollback");
    }
    const auto one=hex("01");Reader limited(one);
    t.error(ProtocolErrorCode::MessageTooLarge,[&]{limited.compact_size(0);});
    t.check(limited.remaining()==1,"field maximum rollback");
    const auto truncated_string=hex("036162"); Reader string_reader(truncated_string);
    t.error(ProtocolErrorCode::Truncated,[&]{string_reader.varstring(10);});
    t.check(string_reader.remaining()==3,"string rollback");
    Writer strings; strings.string("");strings.string(std::string("a\0b",3));
    Reader sr(strings.data()); t.check(sr.varstring(0).empty() && sr.varstring(3)==std::string("a\0b",3),"empty and embedded NUL strings");sr.require_end();
    Reader empty({});t.check(empty.string(0).empty() && empty.bytes(0).empty(),"empty reads");
    t.error(ProtocolErrorCode::Truncated,[&]{empty.bytes(std::numeric_limits<std::size_t>::max());});
    Writer cap(3);cap.u8(7);
    t.error(ProtocolErrorCode::MessageTooLarge,[&]{cap.u32(1);});
    t.error(ProtocolErrorCode::MessageTooLarge,[&]{cap.compact_size(253);});
    t.error(ProtocolErrorCode::MessageTooLarge,[&]{cap.string("ab");});
    t.check(cap.data()==hex("07"),"bounded writes leave no partial fields");
    cap.be16(8226);t.check(cap.data()==hex("072022"),"exact writer boundary");
    Writer zero(0);zero.bytes({});t.error(ProtocolErrorCode::MessageTooLarge,[&]{zero.u8(0);});
    Writer alias;alias.bytes(one);alias.bytes(alias.data());t.check(alias.data()==hex("0101"),"self append survives reallocation");
    Reader tail(one);t.error(ProtocolErrorCode::TrailingData,[&]{tail.require_end();});
    const auto three=hex("010203");Reader fixed(three);t.error(ProtocolErrorCode::Truncated,[&]{fixed.u32();});t.check(fixed.remaining()==3,"fixed read rollback");
    t.pass("Alpha2 serializer");
}
