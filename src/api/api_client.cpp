#include "api/api_client.h"

#include "coord/coord.h"
#include "util/strings.h"

#include <curl/curl.h>

#include <chrono>
#include <cctype>
#include <iostream>
#include <thread>

namespace skillctl::api {

static size_t write_to_string(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* s = static_cast<std::string*>(userdata);
  s->append(ptr, size * nmemb);
  return size * nmemb;
}

static size_t write_to_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* cb = static_cast<std::function<bool(const char*, size_t)>*>(userdata);
  size_t n = size * nmemb;
  if (!(*cb)(ptr, n)) return 0;  // abort transfer
  return n;
}

static size_t header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* contentType = static_cast<std::string*>(userdata);
  std::string line(buffer, size * nitems);
  // very small parser for Content-Type
  auto lower = line;
  for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  const std::string key = "content-type:";
  if (lower.rfind(key, 0) == 0) {
    auto v = line.substr(key.size());
    *contentType = skillctl::util::trim(v);
  }
  return size * nitems;
}

static std::string join_url(const std::string& base, const std::string& path) {
  if (base.empty()) return path;
  if (base.back() == '/' && !path.empty() && path.front() == '/') return base.substr(0, base.size() - 1) + path;
  if (base.back() != '/' && !path.empty() && path.front() != '/') return base + "/" + path;
  return base + path;
}

ApiClient::ApiClient(ClientOptions opt) : opt_(std::move(opt)) {}

std::string ApiClient::endpoint_publish() const {
  // Repo 已从公共 API 中移除，这里直接按统一路径调用后端。
  return join_url(opt_.baseUrl, "/api/v1/skills");
}

std::string ApiClient::endpoint_show(const coord::Coord& c) const {
  return join_url(opt_.baseUrl,
                  "/api/v1/skills/" + coord::encode_path_segment(c.group) + "/" +
                      coord::encode_path_segment(c.name) + "/" + coord::encode_path_segment(c.version));
}

std::string ApiClient::endpoint_get_content(const coord::Coord& c) const {
  return endpoint_show(c) + "/content";
}

std::string ApiClient::endpoint_search() const {
  return join_url(opt_.baseUrl, "/api/v1/skills/search");
}

static ApiResult classify_response(const HttpResponse& r) {
  ApiResult ar;
  ar.httpStatus = r.status;
  if (r.status >= 200 && r.status < 300) return ar;
  // attempt parse JSON error
  if (r.contentType.find("application/json") != std::string::npos ||
      r.contentType.find("+json") != std::string::npos) {
    try {
      auto j = nlohmann::json::parse(r.body);
      auto parsed = skillctl::err::parse_error_response(j);
      if (parsed) {
        ar.serverError = *parsed;
        return ar;
      }
    } catch (...) {
    }
  }
  skillctl::err::LocalError le;
  le.httpStatus = r.status;
  le.httpBodySnippet = r.body.substr(0, 512);
  le.message = "HTTP " + std::to_string(r.status);
  ar.localError = le;
  return ar;
}

static bool should_retry(long status) { return status == 429 || status == 503; }

static long backoff_ms(int attempt) {
  // attempt starts at 1
  long base = 500;
  long max = 8000;
  long v = base * (1L << (attempt - 1));
  return v > max ? max : v;
}

ApiResult ApiClient::publish_file(const std::string& filePath, const std::optional<std::string>& idemKey) {
  ApiResult out;
  CURL* curl = curl_easy_init();
  if (!curl) {
    skillctl::err::LocalError le;
    le.message = "failed to init curl";
    out.localError = le;
    return out;
  }

  std::string url = endpoint_publish();
  std::string body;
  std::string contentType;

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: skillctl/0.1.0");
  headers = curl_slist_append(headers, "Accept: application/json");
  if (opt_.token && !opt_.token->empty()) {
    headers = curl_slist_append(headers, ("Authorization: Bearer " + *opt_.token).c_str());
  }
  if (idemKey && !idemKey->empty()) {
    headers = curl_slist_append(headers, ("Idempotency-Key: " + *idemKey).c_str());
  }

  curl_mime* mime = curl_mime_init(curl);
  curl_mimepart* part = curl_mime_addpart(mime);
  curl_mime_name(part, "file");
  curl_mime_filedata(part, filePath.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt_.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (opt_.tlsInsecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &contentType);

  long status = 0;
  int attempt = 0;
  CURLcode code = CURLE_OK;
  do {
    attempt++;
    body.clear();
    contentType.clear();
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) break;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (attempt <= opt_.retries && should_retry(status)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms(attempt)));
      continue;
    }
    break;
  } while (attempt <= opt_.retries + 1);

  HttpResponse resp;
  resp.status = status;
  resp.contentType = contentType;
  resp.body = body;

  if (code != CURLE_OK) {
    skillctl::err::LocalError le;
    le.message = curl_easy_strerror(code);
    le.retryable = (code == CURLE_OPERATION_TIMEDOUT);
    // treat timeouts as retryable at mapping layer
    out.localError = le;
  } else if (status >= 200 && status < 300) {
    try {
      out.dataJson = nlohmann::json::parse(body);
    } catch (...) {
      // allow empty body / non-json success: wrap as raw text
      out.dataJson = nlohmann::json::object();
    }
  } else {
    auto classified = classify_response(resp);
    out.serverError = classified.serverError;
    out.localError = classified.localError;
    out.httpStatus = classified.httpStatus;
  }

  curl_mime_free(mime);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  out.httpStatus = status;
  return out;
}

ApiResult ApiClient::show(const coord::Coord& c) {
  ApiResult out;
  CURL* curl = curl_easy_init();
  if (!curl) {
    skillctl::err::LocalError le;
    le.message = "failed to init curl";
    out.localError = le;
    return out;
  }

  std::string url = endpoint_show(c);
  std::string body;
  std::string contentType;
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: skillctl/0.1.0");
  headers = curl_slist_append(headers, "Accept: application/json");
  if (opt_.token && !opt_.token->empty()) {
    headers = curl_slist_append(headers, ("Authorization: Bearer " + *opt_.token).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt_.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (opt_.tlsInsecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &contentType);

  long status = 0;
  int attempt = 0;
  CURLcode code = CURLE_OK;
  do {
    attempt++;
    body.clear();
    contentType.clear();
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) break;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (attempt <= opt_.retries && should_retry(status)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms(attempt)));
      continue;
    }
    break;
  } while (attempt <= opt_.retries + 1);

  if (code != CURLE_OK) {
    skillctl::err::LocalError le;
    le.message = curl_easy_strerror(code);
    le.retryable = (code == CURLE_OPERATION_TIMEDOUT);
    out.localError = le;
  } else if (status >= 200 && status < 300) {
    try {
      out.dataJson = nlohmann::json::parse(body);
    } catch (...) {
      skillctl::err::LocalError le;
      le.message = "invalid JSON response";
      le.httpStatus = status;
      out.localError = le;
    }
  } else {
    HttpResponse resp{status, contentType, body};
    auto classified = classify_response(resp);
    out.serverError = classified.serverError;
    out.localError = classified.localError;
    out.httpStatus = status;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  out.httpStatus = status;
  return out;
}

ApiResult ApiClient::search(const std::string& q, const std::optional<std::string>& group,
                            const std::optional<std::string>& name, const std::optional<std::string>& label) {
  ApiResult out;
  CURL* curl = curl_easy_init();
  if (!curl) {
    skillctl::err::LocalError le;
    le.message = "failed to init curl";
    out.localError = le;
    return out;
  }

  auto esc = [&](const std::string& s) -> std::string {
    char* p = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.size()));
    if (!p) return "";
    std::string out(p);
    curl_free(p);
    return out;
  };

  std::string url = endpoint_search();
  url += "?q=" + esc(q);
  if (group && !group->empty()) url += "&group=" + esc(*group);
  if (name && !name->empty()) url += "&name=" + esc(*name);
  if (label && !label->empty()) url += "&label=" + esc(*label);

  if (opt_.verbose) {
    std::cerr << "[skillctl] GET " << url << "\n";
  }

  std::string body;
  std::string contentType;
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: skillctl/0.1.0");
  headers = curl_slist_append(headers, "Accept: application/json");
  if (opt_.token && !opt_.token->empty()) {
    headers = curl_slist_append(headers, ("Authorization: Bearer " + *opt_.token).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt_.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (opt_.tlsInsecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &contentType);

  long status = 0;
  int attempt = 0;
  CURLcode code = CURLE_OK;
  do {
    attempt++;
    body.clear();
    contentType.clear();
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) break;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (attempt <= opt_.retries && should_retry(status)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms(attempt)));
      continue;
    }
    break;
  } while (attempt <= opt_.retries + 1);

  if (code != CURLE_OK) {
    skillctl::err::LocalError le;
    le.message = curl_easy_strerror(code);
    le.retryable = (code == CURLE_OPERATION_TIMEDOUT);
    out.localError = le;
  } else if (status >= 200 && status < 300) {
    try {
      out.dataJson = nlohmann::json::parse(body);
    } catch (...) {
      skillctl::err::LocalError le;
      le.message = "invalid JSON response";
      le.httpStatus = status;
      out.localError = le;
    }
  } else {
    HttpResponse resp{status, contentType, body};
    auto classified = classify_response(resp);
    out.serverError = classified.serverError;
    out.localError = classified.localError;
    out.httpStatus = status;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  out.httpStatus = status;
  return out;
}

ApiResult ApiClient::get_content(const coord::Coord& c, std::function<bool(const char*, size_t)> onChunk) {
  ApiResult out;
  CURL* curl = curl_easy_init();
  if (!curl) {
    skillctl::err::LocalError le;
    le.message = "failed to init curl";
    out.localError = le;
    return out;
  }

  std::string url = endpoint_get_content(c);
  if (opt_.verbose) {
    std::cerr << "[skillctl] GET " << url << "\n";
  }
  std::string contentType;
  std::string bodySnippet;  // only for error fallback

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: skillctl/0.1.0");
  if (opt_.token && !opt_.token->empty()) {
    headers = curl_slist_append(headers, ("Authorization: Bearer " + *opt_.token).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, opt_.timeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (opt_.tlsInsecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &onChunk);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &contentType);

  long status = 0;
  int attempt = 0;
  CURLcode code = CURLE_OK;
  do {
    attempt++;
    contentType.clear();
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) break;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (attempt <= opt_.retries && should_retry(status)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms(attempt)));
      continue;
    }
    break;
  } while (attempt <= opt_.retries + 1);

  if (code != CURLE_OK) {
    skillctl::err::LocalError le;
    le.message = curl_easy_strerror(code);
    le.retryable = (code == CURLE_OPERATION_TIMEDOUT);
    out.localError = le;
  } else if (status >= 200 && status < 300) {
    if (opt_.verbose) {
      std::cerr << "[skillctl] " << status << " OK\n";
    }
  } else {
    if (opt_.verbose) {
      std::cerr << "[skillctl] " << status << " (error body re-fetched for classification)\n";
    }
    // Need body to classify; re-run as capture (small) once for error.
    std::string body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    HttpResponse resp{status, contentType, body};
    auto classified = classify_response(resp);
    out.serverError = classified.serverError;
    out.localError = classified.localError;
    out.httpStatus = status;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  out.httpStatus = status;
  return out;
}

}  // namespace skillctl::api

