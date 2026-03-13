#include "util/fs.h"

#include <cstdlib>
#include <fstream>
#include <random>

namespace skillctl::util {
namespace fs = std::filesystem;

static fs::path appdata_dir() {
#ifdef _WIN32
  if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
    return fs::path(appdata);
  }
  if (const char* userprofile = std::getenv("USERPROFILE"); userprofile && *userprofile) {
    return fs::path(userprofile) / "AppData" / "Roaming";
  }
  return fs::current_path();
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    return fs::path(xdg);
  }
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".config";
  }
  return fs::current_path();
#endif
}

static fs::path skillctl_home_dir() {
#ifdef _WIN32
  if (const char* userprofile = std::getenv("USERPROFILE"); userprofile && *userprofile) {
    return fs::path(userprofile) / ".skillctl";
  }
  return fs::current_path() / ".skillctl";
#else
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".skillctl";
  }
  return fs::current_path() / ".skillctl";
#endif
}

fs::path skills_cache_path(const std::string& group, const std::string& name,
                           const std::string& version) {
  return skillctl_home_dir() / "skills" / group / name / version / "SKILL.md";
}

fs::path config_path() {
#ifdef __APPLE__
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / "Library" / "Application Support" / "skillctl" / "config.json";
  }
  return appdata_dir() / "skillctl" / "config.json";
#elif defined(_WIN32)
  return appdata_dir() / "skillctl" / "config.json";
#else
  return appdata_dir() / "skillctl" / "config.json";
#endif
}

fs::path token_path_for_profile(const std::string& profile) {
  auto dir = config_path().parent_path();
  return dir / ("token." + profile + ".bin");
}

bool ensure_parent_dir(const fs::path& file_path, std::string* err) {
  try {
    auto dir = file_path.parent_path();
    if (dir.empty()) return true;
    fs::create_directories(dir);
    return true;
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
}

static std::string random_suffix() {
  static std::random_device rd;
  static std::mt19937_64 gen(rd());
  static std::uniform_int_distribution<uint64_t> dis;
  auto v = dis(gen);
  return std::to_string(v);
}

bool write_file_atomic(const fs::path& path, const std::string& content, std::string* err) {
  if (!ensure_parent_dir(path, err)) return false;
  auto tmp = path;
  tmp += ".tmp." + random_suffix();
  try {
    {
      std::ofstream out(tmp, std::ios::binary);
      if (!out) {
        if (err) *err = "failed to open temp file for write";
        return false;
      }
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      out.close();
      if (!out) {
        if (err) *err = "failed to write temp file";
        return false;
      }
    }
    fs::rename(tmp, path);
    return true;
  } catch (const std::exception& e) {
    try {
      if (fs::exists(tmp)) fs::remove(tmp);
    } catch (...) {
    }
    if (err) *err = e.what();
    return false;
  }
}

bool read_file(const fs::path& path, std::string* out, std::string* err) {
  try {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      if (err) *err = "failed to open file";
      return false;
    }
    std::string buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (out) *out = std::move(buf);
    return true;
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
}

bool file_exists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

}  // namespace skillctl::util

