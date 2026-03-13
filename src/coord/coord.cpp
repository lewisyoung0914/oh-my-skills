#include "coord/coord.h"

#include "util/strings.h"

#include <regex>
#include <stdexcept>

namespace skillctl::coord {

static const std::regex kGroupNameRe("^[A-Za-z0-9_.-]+$");
static const std::regex kVersionRe("^[A-Za-z0-9_.+:-]+$");

bool validate_group_or_name(const std::string& s) { return std::regex_match(s, kGroupNameRe); }
bool validate_version(const std::string& s) { return std::regex_match(s, kVersionRe); }

std::optional<Coord> from_parts(const std::string& group, const std::string& name,
                                const std::string& version, ParseError* err) {
  auto g = util::trim(group);
  auto n = util::trim(name);
  auto v = util::trim(version);
  if (g.empty() || n.empty() || v.empty()) {
    if (err) err->message = "group/name/version must be non-empty";
    return std::nullopt;
  }
  if (!validate_group_or_name(g)) {
    if (err) err->message = "invalid group: only [A-Za-z0-9_.-] allowed";
    return std::nullopt;
  }
  if (!validate_group_or_name(n)) {
    if (err) err->message = "invalid name: only [A-Za-z0-9_.-] allowed";
    return std::nullopt;
  }
  if (!validate_version(v)) {
    if (err) err->message = "invalid version";
    return std::nullopt;
  }
  return Coord{g, n, v};
}

std::optional<Coord> parse_coord(const std::string& s, ParseError* err) {
  auto input = util::trim(s);
  if (input.empty()) {
    if (err) err->message = "coord is empty, expected group/name:version";
    return std::nullopt;
  }
  auto colon = input.rfind(':');
  if (colon == std::string::npos) {
    if (err) err->message = "invalid coord, missing ':' (expected group/name:version)";
    return std::nullopt;
  }
  auto before = input.substr(0, colon);
  auto version = input.substr(colon + 1);
  auto slash = before.find('/');
  if (slash == std::string::npos) {
    if (err) err->message = "invalid coord, missing '/' (expected group/name:version)";
    return std::nullopt;
  }
  auto group = before.substr(0, slash);
  auto name = before.substr(slash + 1);
  return from_parts(group, name, version, err);
}

std::string encode_path_segment(const std::string& s) {
  // Spec: prefer conservative validation over encoding to avoid server mismatch.
  // Reject any segment with path separators or traversal/control characters.
  for (unsigned char ch : s) {
    if (ch == '/' || ch == '\\') throw std::runtime_error("unsafe path segment");
    if (ch < 0x20 || ch == 0x7f) throw std::runtime_error("unsafe control character");
  }
  if (s == "." || s == "..") throw std::runtime_error("unsafe path segment");
  return s;
}

}  // namespace skillctl::coord

