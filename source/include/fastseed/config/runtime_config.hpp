#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace fastseed::config {
struct ValidationResult { bool ok{false}; std::vector<std::string> errors; };
ValidationResult validate_file(const std::string& path);
std::string version_string();
}
