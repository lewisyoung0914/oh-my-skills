#include <catch2/catch_test_macros.hpp>

#include "frontmatter/frontmatter.h"

TEST_CASE("frontmatter requires fields", "[frontmatter]") {
  auto r = skillctl::frontmatter::parse_frontmatter_from_markdown("# no frontmatter\n");
  REQUIRE(r.fm == std::nullopt);
  REQUIRE(r.error.has_value());
}

TEST_CASE("frontmatter parse valid", "[frontmatter]") {
  std::string md =
      "---\n"
      "group: com.example\n"
      "name: demo\n"
      "version: 1.0.0\n"
      "description: hello\n"
      "requires:\n"
      "  - com.example/dep:0.1.0\n"
      "---\n"
      "# body\n";
  auto r = skillctl::frontmatter::parse_frontmatter_from_markdown(md);
  REQUIRE(r.error == std::nullopt);
  REQUIRE(r.fm.has_value());
  REQUIRE(r.fm->group == "com.example");
  REQUIRE(r.fm->dependencies.size() == 1);
}

