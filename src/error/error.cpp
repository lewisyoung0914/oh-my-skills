#include "error/error.h"

namespace skillctl::err {

ExitCode map_http_status_to_exit(long status, bool is_timeout_or_network_retryable) {
  switch (status) {
    case 401:
      return ExitCode::Unauthorized;
    case 403:
      return ExitCode::Forbidden;
    case 404:
    case 410:
      return ExitCode::NotFound;
    case 409:
      return ExitCode::Conflict;
    case 422:
      return ExitCode::Unprocessable;
    case 429:
    case 503:
      return ExitCode::Retryable;
    default:
      break;
  }
  if (status <= 0 && is_timeout_or_network_retryable) return ExitCode::Retryable;
  return ExitCode::Other;
}

bool looks_like_error_response(const nlohmann::json& j) {
  if (!j.is_object()) return false;
  auto it = j.find("success");
  if (it == j.end() || !it->is_boolean()) return false;
  if (it->get<bool>() != false) return false;
  return j.contains("code") && j.contains("message");
}

std::optional<ErrorResponse> parse_error_response(const nlohmann::json& j) {
  if (!looks_like_error_response(j)) return std::nullopt;
  ErrorResponse e;
  e.raw = j;
  e.success = false;
  if (j.contains("code") && j["code"].is_string()) e.code = j["code"].get<std::string>();
  if (j.contains("message") && j["message"].is_string())
    e.message = j["message"].get<std::string>();
  if (j.contains("requestId") && j["requestId"].is_string())
    e.requestId = j["requestId"].get<std::string>();
  if (j.contains("details")) e.details = j["details"];

  if (j.contains("fieldErrors") && j["fieldErrors"].is_array()) {
    std::vector<FieldError> fes;
    for (const auto& fe : j["fieldErrors"]) {
      if (!fe.is_object()) continue;
      FieldError one;
      if (fe.contains("field") && fe["field"].is_string()) one.field = fe["field"].get<std::string>();
      if (fe.contains("reason") && fe["reason"].is_string()) one.reason = fe["reason"].get<std::string>();
      if (fe.contains("message") && fe["message"].is_string()) one.message = fe["message"].get<std::string>();
      fes.push_back(std::move(one));
    }
    e.fieldErrors = std::move(fes);
  }
  return e;
}

}  // namespace skillctl::err

