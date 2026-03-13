#include <catch2/catch_test_macros.hpp>

#include "config/config.h"

TEST_CASE("config merge_runtime resolves base from bases map", "[config]") {
  skillctl::config::ConfigFile cfg;
  cfg.bases["dev"] = nlohmann::json::object();
  cfg.bases["dev"]["url"] = "http://127.0.0.1:8082";
  cfg.bases["dev"]["repo"] = "hosted";
  cfg.profiles["dev"] = {{"timeoutSeconds", 10}};

  auto merged = skillctl::config::merge_runtime(
      "dev", cfg,
      std::optional<int>(20), std::optional<bool>(true), std::optional<int>(2));

  REQUIRE(merged.baseUrl == "http://127.0.0.1:8082");
  REQUIRE(merged.repo == "hosted");
  REQUIRE(merged.timeoutSeconds == 20);
  REQUIRE(merged.tlsInsecure == true);
  REQUIRE(merged.retries == 2);
}

TEST_CASE("get_base_url_repo", "[config]") {
  skillctl::config::ConfigFile cfg;
  cfg.bases["a"] = {{"url", "https://a.com"}, {"repo", "r1"}};
  cfg.bases["b"] = {{"url", "https://b.com"}};  // no repo -> default hosted
  std::string u, r;
  REQUIRE(skillctl::config::get_base_url_repo(cfg, "a", &u, &r) == true);
  REQUIRE(u == "https://a.com");
  REQUIRE(r == "r1");
  REQUIRE(skillctl::config::get_base_url_repo(cfg, "b", &u, &r) == true);
  REQUIRE(u == "https://b.com");
  REQUIRE(r == "hosted");
  REQUIRE(skillctl::config::get_base_url_repo(cfg, "missing", &u, &r) == false);
}

