#include "fastseed/protocol/parser.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size){
    fastseed::protocol::Parser parser;
    try { (void)parser.parse(std::span<const std::uint8_t>(data,size)); } catch (...) {}
    return 0;
}
