#include "frontmatter/frontmatter.h"

#include "util/strings.h"

#include <yaml-cpp/yaml.h>

namespace skillctl::frontmatter {

static bool starts_with_line0_triple_dash(const std::string& s) {
  // allow UTF-8 BOM? keep simple for v1: trim leading spaces not allowed
  return s.rfind("---\n", 0) == 0 || s == "---" || s.rfind("---\r\n", 0) == 0;
}

static ParseResult err(std::string msg) {
  ParseResult r;
  r.error = std::move(msg);
  return r;
}

ParseResult parse_frontmatter_from_markdown(const std::string& markdown) {
  // Must be at document start.
  if (markdown.size() < 3) return err("frontmatter missing: document too short");
  if (!starts_with_line0_triple_dash(markdown)) return err("frontmatter missing: first line must be '---'");

  // Find end delimiter line "\n---\n" or "\r\n---\r\n"
  size_t pos = 0;
  // consume first line
  auto first_end = markdown.find('\n');
  if (first_end == std::string::npos) return err("frontmatter not closed");
  pos = first_end + 1;

  size_t end = std::string::npos;
  while (pos < markdown.size()) {
    auto line_end = markdown.find('\n', pos);
    auto line = markdown.substr(pos, (line_end == std::string::npos) ? std::string::npos : (line_end - pos));
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line == "---") {
      end = pos;  // start of closing delimiter line
      break;
    }
    if (line_end == std::string::npos) break;
    pos = line_end + 1;
  }
  if (end == std::string::npos) return err("frontmatter not closed");

  std::string yaml_text = markdown.substr(first_end + 1, end - (first_end + 1));
  YAML::Node node;
  try {
    node = YAML::Load(yaml_text);
  } catch (const std::exception& e) {
    return err(std::string("invalid YAML: ") + e.what());
  }
  if (!node || !node.IsMap()) return err("invalid YAML: expected map");

  auto get_str = [&](const char* key) -> std::optional<std::string> {
    auto v = node[key];
    if (!v) return std::nullopt;
    if (!v.IsScalar()) return std::nullopt;
    auto s = util::trim(v.as<std::string>());
    if (s.empty()) return std::nullopt;
    return s;
  };

  auto group = get_str("group");
  auto name = get_str("name");
  auto version = get_str("version");
  auto description = get_str("description");
  if (!group) return err("frontmatter field required: group");
  if (!name) return err("frontmatter field required: name");
  if (!version) return err("frontmatter field required: version");
  if (!description) return err("frontmatter field required: description");

  coord::ParseError ce;
  if (!coord::from_parts(*group, *name, *version, &ce)) {
    return err(std::string("invalid coord in frontmatter: ") + ce.message);
  }

  FrontMatter fm;
  fm.group = *group;
  fm.name = *name;
  fm.version = *version;
  fm.description = *description;
  fm.label = get_str("label");

  if (node["metadata"] && node["metadata"].IsMap()) {
    auto a = node["metadata"]["author"];
    if (a && a.IsScalar()) {
      auto s = util::trim(a.as<std::string>());
      if (!s.empty()) fm.author = s;
    }
  }

  if (node["requires"]) {
    if (!node["requires"].IsSequence()) return err("frontmatter requires must be an array of strings");
    for (const auto& it : node["requires"]) {
      if (!it.IsScalar()) return err("frontmatter requires must be an array of strings");
      auto dep = util::trim(it.as<std::string>());
      coord::ParseError depErr;
      auto parsed = coord::parse_coord(dep, &depErr);
      if (!parsed) return err(std::string("invalid requires coord: ") + depErr.message);
      fm.dependencies.push_back(*parsed);
    }
  }

  ParseResult r;
  r.fm = std::move(fm);
  return r;
}

}  // namespace skillctl::frontmatter

