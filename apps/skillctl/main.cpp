#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "api/api_client.h"
#include "cli/runtime.h"
#include "config/config.h"
#include "coord/coord.h"
#include "frontmatter/frontmatter.h"
#include "output/output.h"
#include "security/token_store.h"
#include "util/fs.h"
#include "util/strings.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

static std::string build_version() {
#ifdef SKILLCTL_VERSION
  return SKILLCTL_VERSION;
#else
  return "0.1.0-dev";
#endif
}

static std::string build_commit() {
#ifdef SKILLCTL_COMMIT
  return SKILLCTL_COMMIT;
#else
  return "unknown";
#endif
}

static std::string build_time() {
#ifdef SKILLCTL_BUILD_TIME
  return SKILLCTL_BUILD_TIME;
#else
  return __DATE__ " " __TIME__;
#endif
}

static int print_runtime_error(const skillctl::output::IO& io, bool jsonMode, int exitCode,
                               const std::string& message) {
  if (jsonMode) {
    skillctl::output::print_json_local_error(io, message);
  } else {
    skillctl::output::print_local_error_human(io, message);
  }
  return exitCode;
}

struct ExitWithCode : public std::runtime_error {
  int code;
  ExitWithCode(int c, const std::string& msg) : std::runtime_error(msg), code(c) {}
};

[[noreturn]] static void fail_and_exit(const skillctl::output::IO& io, bool jsonMode, int exitCode,
                                      const std::string& message) {
  (void)print_runtime_error(io, jsonMode, exitCode, message);
  throw ExitWithCode(exitCode, message);
}

static int print_api_error(const skillctl::cli::RuntimeContext& ctx, const skillctl::api::ApiResult& r) {
  if (ctx.outOpt.jsonMode) {
    if (r.serverError) {
      skillctl::output::print_json_server_error_raw(ctx.io, r.serverError->raw);
      return static_cast<int>(skillctl::err::map_http_status_to_exit(r.httpStatus, false));
    }
    if (r.localError) {
      skillctl::output::print_json_local_error(ctx.io, r.localError->message);
      auto status = r.localError->httpStatus.value_or(0);
      return static_cast<int>(skillctl::err::map_http_status_to_exit(status, r.localError->retryable));
    }
    skillctl::output::print_json_local_error(ctx.io, "unknown error");
    return 1;
  }

  if (r.serverError) {
    skillctl::output::print_server_error_human(ctx.io, *r.serverError);
    return static_cast<int>(skillctl::err::map_http_status_to_exit(r.httpStatus, false));
  }
  if (r.localError) {
    skillctl::output::print_local_error_human(ctx.io, r.localError->message);
    auto status = r.localError->httpStatus.value_or(0);
    return static_cast<int>(skillctl::err::map_http_status_to_exit(status, r.localError->retryable));
  }
  skillctl::output::print_local_error_human(ctx.io, "unknown error");
  return 1;
}

int main(int argc, char** argv) {
  CLI::App app{"skillctl - Skill 管理平台 CLI"};
  app.require_subcommand(1);

  std::string flagToken;
  bool flagVerbose = false;
  app.add_option("--token", flagToken, "一次性 token（可选，也可用 SKILLCTL_TOKEN）");
  app.add_flag("--verbose,-v", flagVerbose, "打印请求 URL 与状态码到 stderr，便于排查");
  skillctl::output::IO io;

  auto resolve_ctx = [&](const std::string& baseName) -> skillctl::cli::ResolveRuntimeResult {
    skillctl::cli::ResolveRuntimeInput in;
    in.baseName = baseName;
    if (!flagToken.empty()) in.flagToken = flagToken;
    in.flagVerbose = flagVerbose;
    return skillctl::cli::resolve_runtime(in);
  };

  // version
  auto* cmdVersion = app.add_subcommand("version", "显示 CLI 版本与构建信息");
  cmdVersion->callback([&]() {
    (*io.out) << "skillctl_version\t" << build_version() << "\n";
    (*io.out) << "commit\t" << build_commit() << "\n";
    (*io.out) << "build_time\t" << build_time() << "\n";
  });

  // base: skillctl base "base-name" "base-url" [--repo repo]
  std::string baseNameArg;
  std::string baseUrlArg;
  std::string baseRepoOpt = "hosted";
  auto* cmdBase = app.add_subcommand("base", "配置命名 base（名称 + 地址）");
  cmdBase->add_option("base-name", baseNameArg, "base 名称")->required();
  cmdBase->add_option("base-url", baseUrlArg, "服务端地址")->required();
  cmdBase->add_option("--repo", baseRepoOpt, "仓库名（默认 hosted）");
  cmdBase->callback([&]() {
    auto cfgLoad = skillctl::config::load_config_file();
    if (cfgLoad.error) throw std::runtime_error(*cfgLoad.error);
    skillctl::config::ConfigFile cfg = cfgLoad.file.value_or(skillctl::config::ConfigFile{});
    if (!cfg.bases.is_object()) cfg.bases = nlohmann::json::object();
    cfg.bases[baseNameArg] = nlohmann::json::object();
    cfg.bases[baseNameArg]["url"] = skillctl::util::trim(baseUrlArg);
    cfg.bases[baseNameArg]["repo"] = skillctl::util::trim(baseRepoOpt);
    std::string err;
    if (!skillctl::config::save_config_file(cfg, &err)) throw std::runtime_error(err);
    (*io.out) << "base " << baseNameArg << " -> " << cfg.bases[baseNameArg]["url"].get<std::string>() << "\n";
  });

  // login: skillctl login base-name
  std::string loginBaseName;
  bool loginStoreInConfig = false;
  auto* cmdLogin = app.add_subcommand("login", "写入 token（按 base 存储）");
  cmdLogin->add_option("base-name", loginBaseName, "base 名称")->required();
  cmdLogin->add_flag("--store-in-config", loginStoreInConfig, "允许将 token 明文写入配置（有泄露风险）");
  cmdLogin->callback([&]() {
    auto cfgLoad = skillctl::config::load_config_file();
    if (cfgLoad.error) throw std::runtime_error(*cfgLoad.error);
    skillctl::config::ConfigFile cfg = cfgLoad.file.value_or(skillctl::config::ConfigFile{});
    if (!cfg.profiles.contains(loginBaseName) || !cfg.profiles[loginBaseName].is_object())
      cfg.profiles[loginBaseName] = nlohmann::json::object();

    std::string token = flagToken;
    if (token.empty()) {
      (*io.err) << "请输入 token（不会回显）: " << std::flush;
      std::getline(std::cin, token);
      token = skillctl::util::trim(token);
    }
    if (token.empty()) fail_and_exit(io, false, 2, "token is empty");

    auto sec = skillctl::security::store_token_secure(loginBaseName, token);
    if (!sec.ok) {
      if (loginStoreInConfig) {
        cfg.profiles[loginBaseName]["token"] = token;
      } else {
        throw std::runtime_error("secure token store unavailable; use --store-in-config or SKILLCTL_TOKEN");
      }
    } else {
      if (cfg.profiles[loginBaseName].contains("token")) cfg.profiles[loginBaseName].erase("token");
    }
    std::string err;
    if (!skillctl::config::save_config_file(cfg, &err)) throw std::runtime_error(err);
    (*io.out) << "已登录 base=" << loginBaseName << "\n";
  });

  // logout: skillctl logout base-name
  std::string logoutBaseName;
  auto* cmdLogout = app.add_subcommand("logout", "清理该 base 的 token");
  cmdLogout->add_option("base-name", logoutBaseName, "base 名称")->required();
  cmdLogout->callback([&]() {
    (void)skillctl::security::delete_token_secure(logoutBaseName);
    auto cfgLoad = skillctl::config::load_config_file();
    if (cfgLoad.file && cfgLoad.file->profiles.contains(logoutBaseName) &&
        cfgLoad.file->profiles[logoutBaseName].is_object() &&
        cfgLoad.file->profiles[logoutBaseName].contains("token")) {
      auto cfg = *cfgLoad.file;
      cfg.profiles[logoutBaseName].erase("token");
      std::string err;
      if (!skillctl::config::save_config_file(cfg, &err)) throw std::runtime_error(err);
    }
    (*io.out) << "已清理 token base=" << logoutBaseName << "\n";
  });

  // token: skillctl token set/show/unset base-name
  auto* cmdToken = app.add_subcommand("token", "token 管理");
  std::string tokenSetBaseName;
  std::string tokenSetValue;
  bool tokenSetStoreInConfig = false;
  auto* cmdTokenSet = cmdToken->add_subcommand("set", "写入 token");
  cmdTokenSet->add_option("base-name", tokenSetBaseName, "base 名称")->required();
  cmdTokenSet->add_option("token", tokenSetValue, "token")->required();
  cmdTokenSet->add_flag("--store-in-config", tokenSetStoreInConfig, "允许明文写入配置（有泄露风险）");
  cmdTokenSet->callback([&]() {
    auto cfgLoad = skillctl::config::load_config_file();
    if (cfgLoad.error) throw std::runtime_error(*cfgLoad.error);
    skillctl::config::ConfigFile cfg = cfgLoad.file.value_or(skillctl::config::ConfigFile{});
    if (!cfg.profiles.contains(tokenSetBaseName) || !cfg.profiles[tokenSetBaseName].is_object())
      cfg.profiles[tokenSetBaseName] = nlohmann::json::object();
    auto sec = skillctl::security::store_token_secure(tokenSetBaseName, tokenSetValue);
    if (!sec.ok) {
      if (tokenSetStoreInConfig) cfg.profiles[tokenSetBaseName]["token"] = tokenSetValue;
      else throw std::runtime_error("secure token store unavailable; use --store-in-config or SKILLCTL_TOKEN");
    } else {
      if (cfg.profiles[tokenSetBaseName].contains("token")) cfg.profiles[tokenSetBaseName].erase("token");
    }
    std::string err;
    if (!skillctl::config::save_config_file(cfg, &err)) throw std::runtime_error(err);
    (*io.out) << "已写入 token base=" << tokenSetBaseName << "\n";
  });

  auto* cmdTokenUnset = cmdToken->add_subcommand("unset", "清理 token");
  std::string tokenUnsetBaseName;
  cmdTokenUnset->add_option("base-name", tokenUnsetBaseName, "base 名称")->required();
  cmdTokenUnset->callback([&]() {
    (void)skillctl::security::delete_token_secure(tokenUnsetBaseName);
    auto cfgLoad = skillctl::config::load_config_file();
    if (cfgLoad.file && cfgLoad.file->profiles.contains(tokenUnsetBaseName) &&
        cfgLoad.file->profiles[tokenUnsetBaseName].is_object() &&
        cfgLoad.file->profiles[tokenUnsetBaseName].contains("token")) {
      auto cfg = *cfgLoad.file;
      cfg.profiles[tokenUnsetBaseName].erase("token");
      std::string err;
      if (!skillctl::config::save_config_file(cfg, &err)) throw std::runtime_error(err);
    }
    (*io.out) << "已清理 token base=" << tokenUnsetBaseName << "\n";
  });

  std::string tokenShowBaseName;
  auto* cmdTokenShow = cmdToken->add_subcommand("show", "显示掩码 token");
  cmdTokenShow->add_option("base-name", tokenShowBaseName, "base 名称")->required();
  cmdTokenShow->callback([&]() {
    auto rr = resolve_ctx(tokenShowBaseName);
    if (!rr.ctx) fail_and_exit(io, false, rr.exitCode, rr.error.value_or("runtime resolve failed"));
    if (!rr.ctx->token) {
      (*io.out) << "token: (empty)\n";
      return;
    }
    (*io.out) << skillctl::util::mask_token(*rr.ctx->token) << "\n";
  });

  // publish: skillctl publish base-name path
  std::string publishBaseName;
  bool publishDryRun = false;
  std::string publishIdemKey;
  std::string publishPath;
  auto* cmdPublish = app.add_subcommand("publish", "发布 Skill");
  cmdPublish->add_option("base-name", publishBaseName, "base 名称")->required();
  cmdPublish->add_option("path", publishPath, "markdown 文件路径")->required();
  cmdPublish->add_flag("--dry-run", publishDryRun, "仅本地解析/校验，不上传");
  cmdPublish->add_option("--idempotency-key", publishIdemKey, "透传 Idempotency-Key");
  cmdPublish->callback([&]() {
    auto rr = resolve_ctx(publishBaseName);
    if (!rr.ctx) fail_and_exit(io, false, rr.exitCode, rr.error.value_or("runtime resolve failed"));
    auto ctx = *rr.ctx;

    std::string content;
    std::string ferr;
    if (!skillctl::util::read_file(publishPath, &content, &ferr)) throw std::runtime_error(ferr);
    auto fm = skillctl::frontmatter::parse_frontmatter_from_markdown(content);
    if (!fm.fm) fail_and_exit(io, false, 2, fm.error.value_or("frontmatter invalid"));

    if (publishDryRun) {
      (*io.out) << ctx.cfg.repo << "/" << fm.fm->group << "/" << fm.fm->name << ":" << fm.fm->version << "\n";
      return;
    }

    skillctl::api::ClientOptions opt;
    opt.baseUrl = ctx.cfg.baseUrl;
    opt.repo = ctx.cfg.repo;
    opt.token = ctx.token;
    opt.timeoutSeconds = ctx.cfg.timeoutSeconds;
    opt.tlsInsecure = ctx.cfg.tlsInsecure;
    opt.retries = ctx.cfg.retries;
    opt.verbose = ctx.outOpt.verbose;
    skillctl::api::ApiClient client(opt);

    auto res = client.publish_file(publishPath, publishIdemKey.empty() ? std::nullopt : std::optional<std::string>(publishIdemKey));
    if (res.serverError || res.localError || (res.httpStatus < 200 || res.httpStatus >= 300)) {
      auto code = print_api_error(ctx, res);
      throw ExitWithCode(code, "api error");
    }
    (*io.out) << "published\n";
  });

  // get: skillctl get base-name coord [--force]，默认保存到 ~/.skillctl/skills/{group}/{name}/{version}/SKILL.md
  std::string getBaseName;
  std::string getCoordStr;
  std::string getOutputPath;
  bool getForce = false;
  auto* cmdGet = app.add_subcommand("get", "下载 content 到本地缓存（默认 ~/.skillctl/skills/<group>/<name>/<version>/SKILL.md）");
  cmdGet->add_option("base-name", getBaseName, "base 名称")->required();
  cmdGet->add_option("coord", getCoordStr, "坐标 group/name:version")->required();
  cmdGet->add_option("-o,--output", getOutputPath, "指定保存路径（不指定则用默认缓存路径）");
  cmdGet->add_flag("--force", getForce, "覆盖已存在文件");
  cmdGet->callback([&]() {
    auto rr = resolve_ctx(getBaseName);
    if (!rr.ctx) fail_and_exit(io, false, rr.exitCode, rr.error.value_or("runtime resolve failed"));
    auto ctx = *rr.ctx;

    skillctl::coord::ParseError pe;
    auto c = skillctl::coord::parse_coord(getCoordStr, &pe);
    if (!c) fail_and_exit(io, false, 2, pe.message);

    std::filesystem::path outPath;
    if (getOutputPath.empty()) {
      outPath = skillctl::util::skills_cache_path(c->group, c->name, c->version);
    } else {
      outPath = std::filesystem::path(getOutputPath);
    }

    if (skillctl::util::file_exists(outPath) && !getForce) {
      fail_and_exit(io, false, 2, "output file exists, use --force to overwrite");
    }
    std::string err;
    if (!skillctl::util::ensure_parent_dir(outPath, &err)) {
      fail_and_exit(io, false, 1, "failed to create directory: " + err);
    }
    auto tmp = outPath;
    tmp += ".tmp";
    std::ofstream f(tmp, std::ios::binary);
    if (!f) fail_and_exit(io, false, 1, "failed to open output file");

    skillctl::api::ClientOptions opt;
    opt.baseUrl = ctx.cfg.baseUrl;
    opt.repo = ctx.cfg.repo;
    opt.token = ctx.token;
    opt.timeoutSeconds = ctx.cfg.timeoutSeconds;
    opt.tlsInsecure = ctx.cfg.tlsInsecure;
    opt.retries = ctx.cfg.retries;
    opt.verbose = ctx.outOpt.verbose;
    skillctl::api::ApiClient client(opt);

    auto res = client.get_content(*c, [&](const char* p, size_t n) {
      f.write(p, static_cast<std::streamsize>(n));
      return static_cast<bool>(f);
    });
    f.close();
    if (res.serverError || res.localError || (res.httpStatus < 200 || res.httpStatus >= 300)) {
      try { std::filesystem::remove(tmp); } catch (...) {}
      auto code = print_api_error(ctx, res);
      throw ExitWithCode(code, "api error");
    }
    std::filesystem::rename(tmp, outPath);
    (*io.out) << "saved to " << outPath.string() << "\n";
  });

  // show: skillctl show base-name coord
  std::string showBaseName;
  std::string showCoordStr;
  auto* cmdShow = app.add_subcommand("show", "查看元数据");
  cmdShow->add_option("base-name", showBaseName, "base 名称")->required();
  cmdShow->add_option("coord", showCoordStr, "坐标 group/name:version")->required();
  cmdShow->callback([&]() {
    auto rr = resolve_ctx(showBaseName);
    if (!rr.ctx) fail_and_exit(io, false, rr.exitCode, rr.error.value_or("runtime resolve failed"));
    auto ctx = *rr.ctx;

    skillctl::coord::ParseError pe;
    auto c = skillctl::coord::parse_coord(showCoordStr, &pe);
    if (!c) fail_and_exit(io, false, 2, pe.message);

    skillctl::api::ClientOptions opt;
    opt.baseUrl = ctx.cfg.baseUrl;
    opt.repo = ctx.cfg.repo;
    opt.token = ctx.token;
    opt.timeoutSeconds = ctx.cfg.timeoutSeconds;
    opt.tlsInsecure = ctx.cfg.tlsInsecure;
    opt.retries = ctx.cfg.retries;
    opt.verbose = ctx.outOpt.verbose;
    skillctl::api::ApiClient client(opt);

    auto res = client.show(*c);
    if (res.serverError || res.localError || (res.httpStatus < 200 || res.httpStatus >= 300)) {
      auto code = print_api_error(ctx, res);
      throw ExitWithCode(code, "api error");
    }
    (*io.out) << res.dataJson.value_or(nlohmann::json::object()).dump(2) << "\n";
  });

  // search: skillctl search base-name query [--name ...] [--group ...] [--label ...]
  std::string searchBaseName;
  std::string searchQuery;
  std::string searchGroup;
  std::string searchName;
  std::string searchLabel;
  auto* cmdSearch = app.add_subcommand("search", "搜索 Skill");
  cmdSearch->add_option("base-name", searchBaseName, "base 名称")->required();
  cmdSearch->add_option("query", searchQuery, "搜索关键字")->required();
  cmdSearch->add_option("--group", searchGroup, "按 group 过滤");
  cmdSearch->add_option("--name", searchName, "按 name 过滤");
  cmdSearch->add_option("--label", searchLabel, "按 label 过滤");
  cmdSearch->callback([&]() {
    auto rr = resolve_ctx(searchBaseName);
    if (!rr.ctx) fail_and_exit(io, false, rr.exitCode, rr.error.value_or("runtime resolve failed"));
    auto ctx = *rr.ctx;

    skillctl::api::ClientOptions opt;
    opt.baseUrl = ctx.cfg.baseUrl;
    opt.repo = ctx.cfg.repo;
    opt.token = ctx.token;
    opt.timeoutSeconds = ctx.cfg.timeoutSeconds;
    opt.tlsInsecure = ctx.cfg.tlsInsecure;
    opt.retries = ctx.cfg.retries;
    opt.verbose = ctx.outOpt.verbose;
    skillctl::api::ApiClient client(opt);

    auto res = client.search(searchQuery,
                             searchGroup.empty() ? std::nullopt : std::optional<std::string>(searchGroup),
                             searchName.empty() ? std::nullopt : std::optional<std::string>(searchName),
                             searchLabel.empty() ? std::nullopt : std::optional<std::string>(searchLabel));
    if (res.serverError || res.localError || (res.httpStatus < 200 || res.httpStatus >= 300)) {
      auto code = print_api_error(ctx, res);
      throw ExitWithCode(code, "api error");
    }
    auto data = res.dataJson.value_or(nlohmann::json::array());
    if (!data.is_array()) {
      (*io.out) << data.dump(2) << "\n";
      return;
    }
    (*io.out) << "group/name\tlatest_version\tdescription\tlabel\n";
    for (const auto& it : data) {
      if (!it.is_object()) continue;
      auto gn = it.value("group", "") + std::string("/") + it.value("name", "");
      (*io.out) << gn << "\t" << it.value("latest_version", it.value("version", "")) << "\t"
                << it.value("description", "") << "\t" << it.value("label", "") << "\n";
    }
  });

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    (*io.err) << e.what() << "\n";
    (*io.err) << app.help() << "\n";
    return 2;
  } catch (const ExitWithCode& e) {
    return e.code;
  } catch (const std::exception& e) {
    auto msg = std::string(e.what());
    if (msg.empty()) msg = "command failed";
    return print_runtime_error(io, false, 1, msg);
  }

  return 0;
}

