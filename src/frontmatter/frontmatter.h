#pragma once

#include <optional>
#include <string>
#include <vector>

#include "coord/coord.h"

namespace skillctl::frontmatter {

struct FrontMatter {
  std::string group;
  std::string name;
  std::string version;
  std::string description;
  std::optional<std::string> label;
  std::optional<std::string> author;
  // Dependencies declared in frontmatter `requires` field.
  std::vector<coord::Coord> dependencies;
};

struct ParseResult {
  std::optional<FrontMatter> fm;
  std::optional<std::string> error;
};

ParseResult parse_frontmatter_from_markdown(const std::string& markdown);

}  // namespace skillctl::frontmatter

