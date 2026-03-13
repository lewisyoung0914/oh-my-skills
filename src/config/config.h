#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace skillctl::config {

struct ProfileConfig {
  std::optional<std::string> baseUrl;
  std::optional<std::string> defaultRepo;
  std::optional<int> timeoutSeconds;
  std::optional<bool> tlsInsecure;
  std::optional<int> retries;
  std::optional<std::string> tokenPlaintext;  // only when explicitly allowed
};

struct ConfigFile {
  std::string currentProfile = "default";
  nlohmann::json profiles = nlohmann::json::object();  // profileName -> object
  nlohmann::json bases = nlohmann::json::object();      // baseName -> { "url": "...", "repo": "..." }
};

struct ResolvedConfig {
  std::string profileName = "default";
  std::string baseUrl;
  std::string repo;
  int timeoutSeconds = 30;
  bool tlsInsecure = false;
  int retries = 0;
};

struct LoadResult {
  std::optional<ConfigFile> file;
  std::optional<std::string> error;
};

LoadResult load_config_file();
bool save_config_file(const ConfigFile& cfg, std::string* err);

std::string resolve_profile_name(const std::optional<std::string>& flagProfile,
                                 const std::optional<ConfigFile>& cfg);

ResolvedConfig merge_runtime(const std::string& baseName, const std::optional<ConfigFile>& cfg,
                             const std::optional<int>& flagTimeout,
                             const std::optional<bool>& flagInsecure,
                             const std::optional<int>& flagRetries);

bool get_base_url_repo(const ConfigFile& cfg, const std::string& baseName, std::string* outUrl,
                       std::string* outRepo);

// Helpers (profile-based, for login/token legacy)
std::optional<std::string> config_base_url(const ConfigFile& cfg, const std::string& profile);
std::optional<std::string> config_default_repo(const ConfigFile& cfg, const std::string& profile);
std::optional<int> config_timeout(const ConfigFile& cfg, const std::string& profile);
std::optional<bool> config_insecure(const ConfigFile& cfg, const std::string& profile);
std::optional<int> config_retries(const ConfigFile& cfg, const std::string& profile);

}  // namespace skillctl::config

