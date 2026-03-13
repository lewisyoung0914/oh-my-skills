#include <catch2/catch_test_macros.hpp>

#include "coord/coord.h"

TEST_CASE("coord parse valid", "[coord]") {
  skillctl::coord::ParseError err;
  auto c = skillctl::coord::parse_coord("com.example/my-skill:1.0.0", &err);
  REQUIRE(c.has_value());
  REQUIRE(c->group == "com.example");
  REQUIRE(c->name == "my-skill");
  REQUIRE(c->version == "1.0.0");
}

TEST_CASE("coord parse invalid", "[coord]") {
  skillctl::coord::ParseError err;
  auto c = skillctl::coord::parse_coord("bad", &err);
  REQUIRE(!c.has_value());
  REQUIRE(!err.message.empty());
}

