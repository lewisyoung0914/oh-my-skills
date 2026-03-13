#include <catch2/catch_test_macros.hpp>

#include "error/error.h"

TEST_CASE("error response detection", "[error]") {
  nlohmann::json j = {{"success", false}, {"code", "AUTH.UNAUTHORIZED"}, {"message", "nope"}, {"requestId", "r1"}};
  REQUIRE(skillctl::err::looks_like_error_response(j));
  auto parsed = skillctl::err::parse_error_response(j);
  REQUIRE(parsed.has_value());
  REQUIRE(parsed->code == "AUTH.UNAUTHORIZED");
}

TEST_CASE("exit code mapping", "[error]") {
  REQUIRE(static_cast<int>(skillctl::err::map_http_status_to_exit(401, false)) == 10);
  REQUIRE(static_cast<int>(skillctl::err::map_http_status_to_exit(422, false)) == 14);
  REQUIRE(static_cast<int>(skillctl::err::map_http_status_to_exit(0, true)) == 15);
}

