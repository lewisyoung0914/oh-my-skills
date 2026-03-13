#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace skillctl::err {

struct FieldError {
  std::string field;
  std::string reason;
  std::string message;
};

struct ErrorResponse {
  bool success = false;
  std::string code;
  std::string message;
  std::string requestId;
  std::optional<nlohmann::json> details;
  std::optional<std::vector<FieldError>> fieldErrors;
  nlohmann::json raw;
};

struct LocalError {
  std::string message;
  std::optional<long> httpStatus;
  std::optional<std::string> httpBodySnippet;
  bool retryable = false;
};

enum class ExitCode : int {
  Ok = 0,
  Other = 1,
  Usage = 2,
  Unauthorized = 10,
  Forbidden = 11,
  NotFound = 12,
  Conflict = 13,
  Unprocessable = 14,
  Retryable = 15,
};

ExitCode map_http_status_to_exit(long status, bool is_timeout_or_network_retryable);
bool looks_like_error_response(const nlohmann::json& j);
std::optional<ErrorResponse> parse_error_response(const nlohmann::json& j);

}  // namespace skillctl::err

