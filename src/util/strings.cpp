#include "util/strings.h"

#include <algorithm>
#include <cctype>

namespace skillctl::util {

std::string trim(std::string s) {
  auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

std::string mask_token(const std::string& token) {
  if (token.size() < 10) return "****";
  return token.substr(0, 4) + "..." + token.substr(token.size() - 4);
}

}  // namespace skillctl::util

