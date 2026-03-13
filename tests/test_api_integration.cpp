#include <catch2/catch_test_macros.hpp>

#include "api/api_client.h"

#include <httplib.h>

#include <atomic>
#include <thread>

static int pick_port() {
  // fixed port for simplicity in tests; if occupied, user can change.
  return 18080;
}

TEST_CASE("api client parses ErrorResponse and maps status", "[api]") {
  httplib::Server svr;
  svr.Get("/api/v1/skills/com.example/demo/1.0.0", [&](const httplib::Request&, httplib::Response& res) {
    res.status = 401;
    res.set_header("Content-Type", "application/json");
    res.set_content(
        R"({"success":false,"code":"AUTH.UNAUTHORIZED","message":"unauthorized","requestId":"r1","timestamp":"2026-03-12T00:00:00Z","path":"/api/v1/skills/com.example/demo/1.0.0"})",
        "application/json");
  });

  std::atomic<bool> ready{false};
  std::thread th([&]() {
    ready = true;
    svr.listen("127.0.0.1", pick_port());
  });
  while (!ready) std::this_thread::yield();

  skillctl::api::ClientOptions opt;
  opt.baseUrl = "http://127.0.0.1:" + std::to_string(pick_port());
  opt.repo = "hosted";
  opt.timeoutSeconds = 5;
  skillctl::api::ApiClient client(opt);

  skillctl::coord::Coord c{"com.example", "demo", "1.0.0"};
  auto r = client.show(c);
  REQUIRE(r.serverError.has_value());
  REQUIRE(r.serverError->code == "AUTH.UNAUTHORIZED");
  REQUIRE(r.httpStatus == 401);

  svr.stop();
  th.join();
}

TEST_CASE("api client handles non-json error body", "[api]") {
  httplib::Server svr;
  svr.Get("/api/v1/skills/search", [&](const httplib::Request&, httplib::Response& res) {
    res.status = 503;
    res.set_header("Content-Type", "text/plain");
    res.set_content("unavailable", "text/plain");
  });

  std::thread th([&]() { svr.listen("127.0.0.1", pick_port() + 1); });

  skillctl::api::ClientOptions opt;
  opt.baseUrl = "http://127.0.0.1:" + std::to_string(pick_port() + 1);
  opt.repo = "hosted";
  opt.timeoutSeconds = 5;
  opt.retries = 0;
  skillctl::api::ApiClient client(opt);

  auto r = client.search("q", std::nullopt, std::nullopt, std::nullopt);
  REQUIRE(r.localError.has_value());
  REQUIRE(r.httpStatus == 503);

  svr.stop();
  th.join();
}

