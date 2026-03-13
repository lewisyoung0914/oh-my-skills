#include <catch2/catch_test_macros.hpp>

#include "output/output.h"

#include <sstream>

TEST_CASE("json ok envelope", "[output]") {
  std::stringstream out;
  std::stringstream err;
  skillctl::output::IO io{&out, &err};
  skillctl::output::print_json_ok(io, nlohmann::json{{"a", 1}});
  auto s = out.str();
  REQUIRE(s.find("\"ok\":true") != std::string::npos);
  REQUIRE(err.str().empty());
}

