#pragma once

#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "error/error.h"

namespace skillctl::output {

struct OutputOptions {
  bool jsonMode = false;
  bool quiet = false;
  bool verbose = false;
};

struct IO {
  std::ostream* out = &std::cout;
  std::ostream* err = &std::cerr;
};

void print_json_ok(const IO& io, const nlohmann::json& data);
void print_json_local_error(const IO& io, const std::string& message);
void print_json_server_error_raw(const IO& io, const nlohmann::json& raw);
void print_server_error_human(const IO& io, const skillctl::err::ErrorResponse& e);
void print_local_error_human(const IO& io, const std::string& message);

}  // namespace skillctl::output

