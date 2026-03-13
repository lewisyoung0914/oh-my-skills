#pragma once

#include <optional>
#include <string>

#include "config/config.h"
#include "output/output.h"

namespace skillctl::cli {

struct RuntimeContext {
  // merged config
  config::ResolvedConfig cfg;

  // auth token (after priority resolution)
  std::optional<std::string> token;
  enum class TokenSource { None, Flag, Env, SecureStore, ConfigPlaintext };
  TokenSource tokenSource = TokenSource::None;

  // output
  output::IO io;
  output::OutputOptions outOpt;
};

struct ResolveRuntimeInput {
  std::string baseName;  // required: name from skillctl base "name" "url"
  std::optional<int> flagTimeout;
  std::optional<bool> flagInsecure;
  std::optional<int> flagRetries;
  std::optional<std::string> flagToken;
  std::optional<bool> flagVerbose;
};

struct ResolveRuntimeResult {
  std::optional<RuntimeContext> ctx;
  std::optional<std::string> error;
  int exitCode = 1;
};

ResolveRuntimeResult resolve_runtime(const ResolveRuntimeInput& in);

}  // namespace skillctl::cli

