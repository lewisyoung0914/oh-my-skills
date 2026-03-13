#pragma once

#include <optional>
#include <string>

namespace skillctl::coord {

struct Coord {
  std::string group;
  std::string name;
  std::string version;
};

struct ParseError {
  std::string message;
};

std::optional<Coord> parse_coord(const std::string& s, ParseError* err);
std::optional<Coord> from_parts(const std::string& group, const std::string& name,
                                const std::string& version, ParseError* err);
bool validate_group_or_name(const std::string& s);
bool validate_version(const std::string& s);
std::string encode_path_segment(const std::string& s);  // conservative: throws on unsafe

}  // namespace skillctl::coord

