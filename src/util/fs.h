#pragma once

#include <filesystem>
#include <string>

namespace skillctl::util {

std::filesystem::path config_path();
std::filesystem::path token_path_for_profile(const std::string& profile);

/** 默认技能缓存路径：~/.skillctl/skills/{group}/{name}/{version}/SKILL.md */
std::filesystem::path skills_cache_path(const std::string& group, const std::string& name,
                                        const std::string& version);


bool ensure_parent_dir(const std::filesystem::path& file_path, std::string* err);
bool write_file_atomic(const std::filesystem::path& path, const std::string& content,
                       std::string* err);
bool read_file(const std::filesystem::path& path, std::string* out, std::string* err);
bool file_exists(const std::filesystem::path& path);

}  // namespace skillctl::util

