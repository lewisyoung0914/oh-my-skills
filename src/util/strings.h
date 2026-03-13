#pragma once

#include <string>

namespace skillctl::util {

std::string trim(std::string s);
bool iequals(const std::string& a, const std::string& b);
std::string mask_token(const std::string& token);

}  // namespace skillctl::util

