#pragma once
#include "fastseed/protocol/protocol_error.hpp"
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace test_support {
struct Suite {
    unsigned checks{};
    void check(bool ok, const char* label) {
        ++checks;
        if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
    }
    template<class F> void error(fastseed::protocol::ProtocolErrorCode code, F f) {
        try { f(); }
        catch (const fastseed::protocol::ProtocolError& e) { check(e.code() == code, e.what()); return; }
        catch (const std::exception& e) { check(false, e.what()); }
        check(false, "expected protocol error");
    }
    void pass(const char* name) { std::cout << name << ": " << checks << " checks PASS\n"; }
};
inline std::vector<std::uint8_t> hex(std::string_view text) {
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        std::abort();
    };
    if (text.size() % 2) std::abort();
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < text.size(); i += 2) out.push_back(static_cast<std::uint8_t>((digit(text[i]) << 4) | digit(text[i+1])));
    return out;
}
}
