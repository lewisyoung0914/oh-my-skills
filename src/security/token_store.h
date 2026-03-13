#pragma once

#include <optional>
#include <string>

namespace skillctl::security {

struct StoreResult {
  bool ok = false;
  std::optional<std::string> error;
};

struct LoadTokenResult {
  std::optional<std::string> token;
  std::optional<std::string> error;
};

// Secure storage (Windows DPAPI file) when available.
StoreResult store_token_secure(const std::string& profile, const std::string& token);
LoadTokenResult load_token_secure(const std::string& profile);
StoreResult delete_token_secure(const std::string& profile);

}  // namespace skillctl::security

