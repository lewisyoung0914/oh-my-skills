#include "output/output.h"

namespace skillctl::output {

void print_json_ok(const IO& io, const nlohmann::json& data) {
  nlohmann::json j;
  j["ok"] = true;
  j["data"] = data;
  (*io.out) << j.dump() << "\n";
}

void print_json_local_error(const IO& io, const std::string& message) {
  nlohmann::json j;
  j["ok"] = false;
  j["error"] = {{"message", message}};
  (*io.out) << j.dump() << "\n";
}

void print_json_server_error_raw(const IO& io, const nlohmann::json& raw) {
  (*io.out) << raw.dump() << "\n";
}

void print_server_error_human(const IO& io, const skillctl::err::ErrorResponse& e) {
  (*io.err) << e.code << " " << e.message << "\n";
  if (!e.requestId.empty()) (*io.err) << "requestId: " << e.requestId << "\n";
  if (e.fieldErrors && !e.fieldErrors->empty()) {
    for (const auto& fe : *e.fieldErrors) {
      (*io.err) << "- " << fe.field;
      if (!fe.reason.empty()) (*io.err) << ": " << fe.reason;
      if (!fe.message.empty()) (*io.err) << " " << fe.message;
      (*io.err) << "\n";
    }
  }
}

void print_local_error_human(const IO& io, const std::string& message) {
  (*io.err) << "ERROR " << message << "\n";
}

}  // namespace skillctl::output

