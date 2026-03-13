#pragma once

#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "coord/coord.h"
#include "error/error.h"
#include "util/http.h"

namespace skillctl::api {

struct ClientOptions {
  std::string baseUrl;
  std::string repo;
  std::optional<std::string> token;
  int timeoutSeconds = 30;
  bool tlsInsecure = false;
  int retries = 0;
  bool verbose = false;
};

struct HttpResponse {
  long status = 0;
  std::string contentType;
  std::string body;
};

struct ApiResult {
  std::optional<nlohmann::json> dataJson;
  std::optional<std::string> dataText;  // for content download
  std::optional<err::ErrorResponse> serverError;
  std::optional<err::LocalError> localError;
  long httpStatus = 0;
};

class ApiClient {
 public:
  explicit ApiClient(ClientOptions opt);

  ApiResult publish_file(const std::string& filePath, const std::optional<std::string>& idemKey);
  ApiResult show(const coord::Coord& c);
  ApiResult search(const std::string& q, const std::optional<std::string>& group,
                   const std::optional<std::string>& name, const std::optional<std::string>& label);
  ApiResult get_content(const coord::Coord& c, std::function<bool(const char*, size_t)> onChunk);

 private:
  ClientOptions opt_;
  util::CurlGlobal curlGlobal_;

  std::string endpoint_publish() const;
  std::string endpoint_show(const coord::Coord& c) const;
  std::string endpoint_get_content(const coord::Coord& c) const;
  std::string endpoint_search() const;
};

}  // namespace skillctl::api

