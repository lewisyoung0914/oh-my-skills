#include "cli/runtime.h"

#include "security/token_store.h"
#include "util/fs.h"
#include "util/strings.h"

#include <cstdlib>

namespace skillctl::cli {

static std::optional<std::string> getenv_str(const char* k) {
  if (const char* v = std::getenv(k); v && *v) return std::string(v);
  return std::nullopt;
}

static std::optional<int> getenv_int(const char* k) {
  auto s = getenv_str(k);
  if (!s) return std::nullopt;
  try {
    return std::stoi(*s);
  } catch (...) {
    return std::nullopt;
  }
}

static std::optional<bool> getenv_bool(const char* k) {
  auto s = getenv_str(k);
  if (!s) return std::nullopt;
  if (*s == "1") return true;
  if (*s == "0") return false;
  if (util::iequals(*s, "true")) return true;
  if (util::iequals(*s, "false")) return false;
  return std::nullopt;
}

ResolveRuntimeResult resolve_runtime(const ResolveRuntimeInput& in) {
  ResolveRuntimeResult r;

  // load config (best effort for read commands; hard fail only when we need to write)
  auto cfgLoad = config::load_config_file();
  if (cfgLoad.error) {
    // treat as local error; caller decides how to display
    r.error = *cfgLoad.error;
    r.exitCode = 1;
    return r;
  }

  std::string baseName = util::trim(in.baseName);
  if (baseName.empty()) {
    r.error = "base name is required";
    r.exitCode = 2;
    return r;
  }

  auto merged = config::merge_runtime(baseName, cfgLoad.file, in.flagTimeout, in.flagInsecure, in.flagRetries);

  if (merged.baseUrl.empty()) {
    r.error = "base '" + baseName + "' not found; add it with: skillctl base \"" + baseName + "\" <url>";
    r.exitCode = 2;
    return r;
  }
  if (merged.timeoutSeconds <= 0) {
    r.error = "timeout must be positive";
    r.exitCode = 2;
    return r;
  }

  auto envToken = getenv_str("SKILLCTL_TOKEN");

  RuntimeContext ctx;
  ctx.cfg = std::move(merged);
  ctx.outOpt = output::OutputOptions{false, false, in.flagVerbose.value_or(false)};
  ctx.io = output::IO{};

  if (in.flagToken && !in.flagToken->empty()) {
    ctx.token = *in.flagToken;
    ctx.tokenSource = RuntimeContext::TokenSource::Flag;
  } else if (envToken && !envToken->empty()) {
    ctx.token = *envToken;
    ctx.tokenSource = RuntimeContext::TokenSource::Env;
  } else {
    auto sec = security::load_token_secure(baseName);
    if (sec.token) {
      ctx.token = *sec.token;
      ctx.tokenSource = RuntimeContext::TokenSource::SecureStore;
    } else if (cfgLoad.file) {
      auto p = cfgLoad.file->profiles.contains(baseName) ? cfgLoad.file->profiles[baseName] : nlohmann::json();
      if (p.is_object() && p.contains("token") && p["token"].is_string()) {
        ctx.token = p["token"].get<std::string>();
        ctx.tokenSource = RuntimeContext::TokenSource::ConfigPlaintext;
      }
    }
  }

  r.ctx = std::move(ctx);
  return r;
}

}  // namespace skillctl::cli

