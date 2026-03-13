#include "config/config.h"

#include "util/fs.h"
#include "util/strings.h"

#include <cstdlib>

namespace skillctl::config {

static std::optional<int> parse_int_env(const char* v) {
  if (!v || !*v) return std::nullopt;
  try {
    return std::stoi(std::string(v));
  } catch (...) {
    return std::nullopt;
  }
}

LoadResult load_config_file() {
  LoadResult r;
  auto path = util::config_path();
  std::string content;
  std::string err;
  if (!util::file_exists(path)) return r;
  if (!util::read_file(path, &content, &err)) {
    r.error = "failed to read config: " + err;
    return r;
  }
  try {
    auto j = nlohmann::json::parse(content);
    ConfigFile cfg;
    if (j.contains("currentProfile") && j["currentProfile"].is_string())
      cfg.currentProfile = j["currentProfile"].get<std::string>();
    if (j.contains("profiles") && j["profiles"].is_object()) cfg.profiles = j["profiles"];
    if (j.contains("bases") && j["bases"].is_object()) cfg.bases = j["bases"];
    r.file = std::move(cfg);
    return r;
  } catch (const std::exception& e) {
    r.error = std::string("invalid config json: ") + e.what();
    return r;
  }
}

bool save_config_file(const ConfigFile& cfg, std::string* err) {
  nlohmann::json j;
  j["currentProfile"] = cfg.currentProfile;
  j["profiles"] = cfg.profiles;
  j["bases"] = cfg.bases;
  auto path = util::config_path();
  auto dumped = j.dump(2);
  return util::write_file_atomic(path, dumped + "\n", err);
}

std::string resolve_profile_name(const std::optional<std::string>& flagProfile,
                                 const std::optional<ConfigFile>& cfg) {
  if (flagProfile && !util::trim(*flagProfile).empty()) return util::trim(*flagProfile);
  if (cfg) return cfg->currentProfile.empty() ? "default" : cfg->currentProfile;
  return "default";
}

static std::optional<nlohmann::json> profile_obj(const ConfigFile& cfg, const std::string& profile) {
  if (!cfg.profiles.is_object()) return std::nullopt;
  if (!cfg.profiles.contains(profile)) return std::nullopt;
  auto p = cfg.profiles[profile];
  if (!p.is_object()) return std::nullopt;
  return p;
}

std::optional<std::string> config_base_url(const ConfigFile& cfg, const std::string& profile) {
  auto p = profile_obj(cfg, profile);
  if (!p) return std::nullopt;
  if (p->contains("baseUrl") && (*p)["baseUrl"].is_string()) return (*p)["baseUrl"].get<std::string>();
  return std::nullopt;
}

std::optional<std::string> config_default_repo(const ConfigFile& cfg, const std::string& profile) {
  auto p = profile_obj(cfg, profile);
  if (!p) return std::nullopt;
  if (p->contains("defaultRepo") && (*p)["defaultRepo"].is_string())
    return (*p)["defaultRepo"].get<std::string>();
  return std::nullopt;
}

std::optional<int> config_timeout(const ConfigFile& cfg, const std::string& profile) {
  auto p = profile_obj(cfg, profile);
  if (!p) return std::nullopt;
  if (p->contains("timeoutSeconds") && (*p)["timeoutSeconds"].is_number_integer())
    return (*p)["timeoutSeconds"].get<int>();
  return std::nullopt;
}

std::optional<bool> config_insecure(const ConfigFile& cfg, const std::string& profile) {
  auto p = profile_obj(cfg, profile);
  if (!p) return std::nullopt;
  if (p->contains("tlsInsecure") && (*p)["tlsInsecure"].is_boolean())
    return (*p)["tlsInsecure"].get<bool>();
  return std::nullopt;
}

std::optional<int> config_retries(const ConfigFile& cfg, const std::string& profile) {
  auto p = profile_obj(cfg, profile);
  if (!p) return std::nullopt;
  if (p->contains("retries") && (*p)["retries"].is_number_integer()) return (*p)["retries"].get<int>();
  return std::nullopt;
}

bool get_base_url_repo(const ConfigFile& cfg, const std::string& baseName, std::string* outUrl,
                       std::string* outRepo) {
  if (!cfg.bases.is_object() || !cfg.bases.contains(baseName)) return false;
  auto b = cfg.bases[baseName];
  if (!b.is_object() || !b.contains("url") || !b["url"].is_string()) return false;
  *outUrl = util::trim(b["url"].get<std::string>());
  if (b.contains("repo") && b["repo"].is_string())
    *outRepo = util::trim(b["repo"].get<std::string>());
  else
    *outRepo = "hosted";
  return !outUrl->empty();
}

ResolvedConfig merge_runtime(const std::string& baseName, const std::optional<ConfigFile>& cfg,
                             const std::optional<int>& flagTimeout,
                             const std::optional<bool>& flagInsecure,
                             const std::optional<int>& flagRetries) {
  ResolvedConfig out;
  out.profileName = baseName;  // token is keyed by base name
  out.baseUrl.clear();
  out.repo.clear();
  if (cfg) {
    std::string u, r;
    if (get_base_url_repo(*cfg, baseName, &u, &r)) {
      out.baseUrl = u;
      out.repo = r;
    }
  }
  auto cfgTimeout = cfg ? config_timeout(*cfg, baseName) : std::nullopt;
  auto cfgInsecure = cfg ? config_insecure(*cfg, baseName) : std::nullopt;
  auto cfgRetries = cfg ? config_retries(*cfg, baseName) : std::nullopt;
  out.timeoutSeconds = flagTimeout.value_or(cfgTimeout.value_or(30));
  out.tlsInsecure = flagInsecure.value_or(cfgInsecure.value_or(false));
  out.retries = flagRetries.value_or(cfgRetries.value_or(0));
  return out;
}

}  // namespace skillctl::config

