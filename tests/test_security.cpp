#include <catch2/catch_test_macros.hpp>

#include "util/strings.h"

TEST_CASE("token mask never reveals", "[security]") {
  REQUIRE(skillctl::util::mask_token("short") == "****");
  REQUIRE(skillctl::util::mask_token("abcdefghij") == "abcd...ghij");
}

