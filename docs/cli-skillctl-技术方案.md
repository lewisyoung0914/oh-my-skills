# `skillctl`（CLI）技术方案（基于《Skill 管理平台需求文档》）

## 1. 范围与目标

### 1.1 CLI 在系统中的定位

`skillctl` 是 Skill 管理平台的命令行客户端，面向两类场景：

- **人使用**：发布/检索/下载 Skill，查看版本与依赖树，执行标记废弃等操作。
- **系统集成**：CI/CD 或自动化工具通过 Token 进行无交互发布与拉取。

平台“只管存储、分发、治理”，CLI 只做**用户体验层**与**API 封装**，不承担 Skill 运行时。

### 1.2 MVP 目标（对应需求文档 12.1）

- Hosted 仓库下的 **发布**（解析校验由服务端完成；CLI 做本地预校验 + 上传）
- **搜索**（字段/关键字）
- **下载 content**（按坐标精确拉取）
- **Token 鉴权**（`Authorization: Bearer <token>`）
- **基础可观测性**：标准化错误码映射、可选 debug 日志

### 1.3 增强目标（对应需求文档 12.2）

- 依赖树展示（`deps --tree`），循环/缺失提示（以服务端 `/deps` 输出为准）
- 版本“最新”解析（`--version latest` 或 `--latest`）
- 断点续传/大文件优化（若后端支持 Range/ETag）
- 多仓库（proxy/group）适配：CLI 以 repo 名称透传即可

## 2. 我领取的 CLI 任务清单（MVP → 增强）

### 2.1 MVP 任务（必须完成）

- **T1 命令与参数规范落地**
  - 以 `docs/cli-skillctl-规范.md` 为准，落地命令树、参数、输出格式（人读/机器读）
  - 统一坐标输入格式：`group/name:version` 与三段式参数兼容
- **T2 配置与鉴权**
  - 支持 `skillctl login` / `skillctl logout`
  - 支持 `skillctl token set|unset|show`
  - 本地安全存储策略（见 6.2）
- **T3 API Client 与错误处理**
  - 统一 HTTP 客户端、重试策略、超时、代理、TLS 校验开关
  - 对齐 `docs/error-standard.md`：错误展示、`--json` 错误原样输出、退出码映射（见 4.3 / 5.1）
- **T4 发布（publish）**
  - `skillctl publish <file|dir> --repo <repo>`
  - 支持上传 `SKILL.md` 或任意文件名；multipart 优先，fallback 纯文本 body
  - 409 冲突时给出明确提示（坐标已存在，不可覆盖）
  - 可选：本地解析 FrontMatter 做“早失败”（格式/必填字段）
- **T5 拉取（get）**
  - `skillctl get <coord> --repo <repo> [-o <path>]`
  - 默认输出到 stdout（文本流）；`-o` 写文件
  - 输出到 stdout 时不得混入任何额外信息（日志到 stderr）
- **T6 元数据（show）**
  - `skillctl show <coord> --repo <repo>`（仅元数据，不含 content）
- **T7 搜索（search）**
  - `skillctl search <query> [--group ... --name ... --label ...]`
  - 输出表格 + `--json` 输出
- **T8 版本信息（version）**
  - `skillctl version` 输出 CLI 自身版本/commit/build_time
- **T9 文档与示例**
  - `skillctl --help`、README/使用示例、常见错误 FAQ
- **T10 自动化测试与发布产物**
  - 命令行单测（参数解析、输出、退出码）
  - HTTP 交互测试（使用 mock server）
  - 产物：Windows/macOS/Linux 单文件或压缩包（方案见 7.3）

### 2.2 增强任务（排期在 MVP 后）

- **E1 依赖解析（deps）**
  - `skillctl deps <coord> --repo <repo> [--tree] [--transitive]`
- **E2 版本列表（versions）**
  - `skillctl versions <group/name> --repo <repo>`
- **E3 废弃标记（deprecate）**
  - `skillctl deprecate <coord> --repo <repo> [--reason ...] [--replacement ...]`
- **E4 幂等发布（Idempotency-Key）**
  - `skillctl publish ... --idempotency-key <key>` 或自动生成并可复用
- **E5 下载缓存**
  - 本地缓存目录与校验（sha256 / ETag）

## 3. CLI 命令设计

### 3.1 顶层约定

- **可执行文件名**：`skillctl`
- **全局参数**：
  - `--base-url <url>`：服务端地址（优先级高于配置文件）
  - `--repo <repo>`：默认仓库（可写入配置）
  - `--token <token>`：一次性覆盖本地 token（不落盘，适合 CI）
  - `--json`：JSON 输出（机器可读）
  - `--quiet` / `--verbose`：控制输出
  - `--timeout <seconds>`：HTTP 超时
  - `--insecure`：跳过 TLS 校验（仅用于内网调试，默认关闭）
  - `--profile <name>`：配置 profile（默认 `default`）
- **坐标输入**：
  - 形式 A：`com.example/my-skill:1.0.0`
  - 形式 B：`--group com.example --name my-skill --version 1.0.0`

- **环境变量（等价 flag）**：
  - `SKILLCTL_BASE_URL`、`SKILLCTL_REPO`、`SKILLCTL_TOKEN`、`SKILLCTL_TIMEOUT`、`SKILLCTL_INSECURE`

### 3.2 子命令（MVP）

#### 3.2.1 `skillctl login`

用途：将 token 写入本地安全存储/配置。

- 交互式：
  - 提示输入 `base-url`（可选）与 token
  - 支持 `--base-url`、`--token` 直接传参（便于脚本）
  - 支持 `--profile <name>`（默认 `default`）
  - 支持 `--store-in-config`（允许 token 明文落盘；默认禁止）
  - 说明：token 优先写入系统安全存储（Windows DPAPI / Credential Manager）；若不可用且未 `--store-in-config`，提示用户改用环境变量或 `--token`

#### 3.2.2 `skillctl logout`

用途：清理当前 profile 的 token（与可选 base-url）。

- 支持 `--profile <name>`（默认 `default`）
  - 行为：删除安全存储中的 token；不影响其他 profile

#### 3.2.3 `skillctl token set|unset|show`

- `token set <token>`：写入 token
- `token unset`：清除 token
- `token show`：显示 token 的掩码版本（避免泄露）
  - 约束：任何情况下不得输出明文 token（含 `--verbose`）

#### 3.2.4 `skillctl publish`

```
skillctl publish ./SKILL.md --repo hosted
skillctl publish ./path/to/skill.md --repo hosted --json
```

行为：

- 本地可选校验：FrontMatter YAML、必填字段、坐标合法性（与服务端规则对齐：`[a-zA-Z0-9_.-]`）
- 支持 `--dry-run`：仅本地校验不上传
- 支持 `--idempotency-key <key>`：透传 `Idempotency-Key`（可选）
- 可选：`--allow-snapshot` 仅影响本地预校验；最终以服务端仓库策略为准
- 上传：
  - 首选 multipart：字段 `file=@SKILL.md`
  - 备选：纯文本 body（Content-Type `text/markdown` 或 `text/plain`）
- 返回成功时打印：
  - 坐标、repo、content sha256（若返回）、requestId（若返回）、下载命令示例
  - `--json` 成功输出：`{ "ok": true, "data": { "repo": "...", "group": "...", "name": "...", "version": "...", "content_sha256"?: "...", "requestId"?: "..." } }`

#### 3.2.5 `skillctl get`

```
skillctl get com.example/my-skill:1.0.0 --repo hosted
skillctl get com.example/my-skill:1.0.0 --repo hosted -o .\SKILL.md
```

行为：

- 默认 `stdout` 输出 content（方便管道）
- `-o` 写文件时：
  - 若文件已存在：默认拒绝（除非 `--force`）
  - stdout 仅输出摘要（或在 `--quiet` 下不输出）；日志到 stderr
 - API：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/content`
 - 约束：content 为纯文本/流式下载；发生错误时按错误标准展示（stderr 打印 `code message` 与 `requestId`；`--json` 时原样输出服务端 `ErrorResponse`）

#### 3.2.6 `skillctl show`

用途：获取元数据（不含 content）。

- API：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`
- 输出建议（表格模式）：`repo/group/name/version/description/label/author/status/created_at/size_bytes/content_sha256`（按后端返回取舍）

#### 3.2.7 `skillctl search`

```
skillctl search "rag" --label prompt
skillctl search "rag" --json
```

输出（表格模式）建议列：

- `group/name`
- `latest_version`（若 API 返回）
- `description`
- `label`
- `author`
- `updated_at`

说明：`search` 仅查询 Skill 服务端提供的全局搜索（`GET /api/v1/skills/search...`），CLI 不做本地 repo 过滤与聚合。

#### 3.2.8 `skillctl version`

用途：显示 CLI 版本与构建信息（`skillctl_version/commit/build_time`）。

### 3.3 子命令（增强）

- `skillctl deps com.example/my-skill:1.0.0 --tree --transitive`
- `skillctl versions com.example/my-skill`
- `skillctl deprecate com.example/my-skill:1.0.0 --reason ... --replacement com.example/other:2.0.0`
（增强命令暂不在本技术方案 v1 实现范围内）

## 4. 与服务端 API 的对接约定

> 说明：需求文档给的是“建议 API”。CLI 以这些路径为默认实现，并在代码中集中管理 endpoints，便于后续服务端微调。

### 4.1 API 列表（需求文档 7.2）

- 发布：`POST /api/v1/repos/{repo}/skills`
- 精确元数据：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`
- 下载原文：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/content`
- 搜索：`GET /api/v1/skills/search?q=...&group=...&name=...&label=...`
- 版本列表：`GET /api/v1/repos/{repo}/skills/{group}/{name}/versions`
- 依赖解析：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deps?transitive=true`
- 标记废弃：`POST /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deprecate`

### 4.2 认证头

- `Authorization: Bearer <token>`

CLI 获取 token 的优先级（高 → 低）：

1. `--token`
2. 环境变量 `SKILLCTL_TOKEN`
3. 本地安全存储/配置文件中的 token

### 4.3 错误码与 CLI 退出码映射

以 `docs/error-standard.md` 第 9.2 节为准（CLI v1 与平台统一口径）：

- `0`：成功
- `2`：用法错误/参数解析失败（含本地预校验失败）
- `10`：未认证（401）
- `11`：无权限（403）
- `12`：未找到（404/410）
- `13`：冲突（409）
- `14`：校验失败（422）
- `15`：限流/依赖不可用（429/503）
- `1`：其他错误（500 等）

## 5. 数据与输出格式

### 5.1 JSON 输出（`--json`）

对齐 `docs/cli-skillctl-规范.md` 与 `docs/error-standard.md`：

- **成功**：输出
  - `{ "ok": true, "data": <object|array> }`，其中 `data.requestId` 若服务端返回则填充
- **失败（服务端返回 ErrorResponse）**：在 `--json` 下 **原样输出服务端 `ErrorResponse`**（即包含 `success=false/code/message/requestId/...` 的结构）
- **失败（本地错误：网络/超时/解析/IO）**：直接抛出对应异常并在最外层统一处理：
  - 非 `--json`：stderr 输出可读错误信息（如超时原因），并返回对应退出码
  - `--json`：输出最小对象 `{ "ok": false, "error": { "message": "..." } }`（不引入额外 `code`）

### 5.2 表格输出（默认）

- 面向人读，尽量短；错误信息输出到 stderr。
- `get` 默认输出 content 到 stdout（**不混入日志**），除非显式 `--verbose` 且输出到 stderr。

### 5.3 输出通道约束（stdout/stderr）

对齐 `docs/cli-skillctl-规范.md`：

- stdout：命令的主要结果（列表表格、`--json` 输出、`get` 的 content）
- stderr：日志、告警与错误信息（避免污染管道）

## 6. 配置与安全

### 6.1 配置项

推荐配置项：

- `base_url`
- `default_repo`
- `token_source`（可选：config/env/flag，用于诊断展示，不参与决策）
- `timeout_seconds`
- `tls_insecure`（默认 false）
- `retries`（默认 0 或 1；仅对 429/503 生效）
- `profile`（默认 `default`）

#### 6.1.1 配置文件位置（v1 建议）

为便于跨平台与可迁移，v1 采用“单文件 + 多 profile”：

- Windows：`%APPDATA%\skillctl\config.json`
- macOS：`~/Library/Application Support/skillctl/config.json`
- Linux：`~/.config/skillctl/config.json`

结构建议（示例）：

```json
{
  "currentProfile": "default",
  "profiles": {
    "default": { "baseUrl": "https://skill.example.com", "defaultRepo": "hosted", "timeoutSeconds": 30, "retries": 0 },
    "staging": { "baseUrl": "https://skill-stg.example.com", "defaultRepo": "hosted" }
  }
}
```

### 6.2 Token 本地存储策略（Windows 优先）

在 Windows 上优先使用系统凭据存储（Credential Manager / DPAPI）存 token；
若实现成本过高或跨平台统一困难，则退化为：

- 仅在配置文件中保存 token（**默认不启用**，需用户显式同意，如 `skillctl login --store-in-config`）
- 否则建议用户用环境变量/CI secret 注入（`SKILLCTL_TOKEN`）

无论哪种方式：

- `token show` 只显示掩码
- 日志与错误信息不得回显完整 token

## 7. 实现方案（工程层）

### 7.1 语言与框架建议

本项目 CLI **开发语言确定为 C++**，以跨平台（Windows/macOS/Linux）与可控的单可执行交付为目标。

建议技术选型（偏向成熟、易打包、跨平台）：

- **命令行解析**：`CLI11`（推荐）或 `cxxopts`
- **HTTP Client**：`libcurl`（推荐，TLS/代理/超时/重试易做）
- **JSON**：`nlohmann/json`
- **YAML / FrontMatter**：`yaml-cpp`（解析 FrontMatter YAML；正文无需解析）
- **表格输出**：轻量自研（对齐列宽）或引入 `tabulate`（可选）
- **测试**：`Catch2` 或 `GoogleTest`
- **构建**：`CMake`
- **依赖管理**：`vcpkg`（推荐，Windows 体验更好）或 `Conan`

### 7.2 代码结构建议

建议目录（CMake 工程）：

- `apps/skillctl/`：可执行入口（`main.cpp`）
- `src/cli/`：命令定义、参数校验、help 文案
- `src/api/`：服务端 API client（curl 封装、请求/响应模型、错误映射）
- `src/config/`：配置加载与合并（flag/env/config）
- `src/output/`：表格/JSON 输出
- `src/coord/`：坐标解析与校验
- `src/frontmatter/`：FrontMatter 解析（提取 YAML 头）
- `src/security/`
  - `windows_dpapi/`：Windows token 加密存储（可选，见 6.2）
  - `plaintext/`：退化实现（显式开启才落盘）
- `tests/`：单元/集成测试
- `third_party/`：仅在不使用包管理器时使用（尽量避免）

### 7.3 关键模块设计（C++ 落地细化）

#### 7.3.1 `coord`（坐标解析）

支持两种输入并统一为结构体：

- `group/name:version`
- `group name version`（来自三段参数）

解析规则：

- 必须包含 `/` 与 `:` 分隔（在单字符串模式下）
- `group`/`name` 需匹配正则：`^[A-Za-z0-9_.-]+$`
- `version`：v1 不强制 semver，但可提供 `--strict-semver` 做本地预校验（可选）

#### 7.3.2 `frontmatter`（本地预校验）

仅用于“早失败”，最终以服务端校验为准：

- 提取 Markdown 顶部 `---` 与下一段 `---` 之间的 YAML
- 用 `yaml-cpp` 解析并校验必填字段：`group/name/version/description`
- 失败时走本地 `exit code=2`（参数/本地校验失败）并输出明确字段错误

#### 7.3.3 `api`（HTTP 客户端）

使用 `libcurl` 封装 `ApiClient`：

- 统一设置：
  - `Authorization: Bearer <token>`
  - `User-Agent: skillctl/<version>`
  - `Accept: application/json`（除 `get content` 外）
- 超时：
  - connect timeout（建议 5s）
  - total timeout（默认 30s，可配置）
- 重试：
  - 仅当 HTTP 为 429/503（或服务端 `code` 属于可重试白名单）时重试
  - 指数退避：\(base=500ms, max=8s\)，最多 `retries` 次
  - 约束：对 409/422 不重试（用户输入/策略冲突）

上传（publish）：

- 首选 `multipart/form-data`：
  - 字段名：`file`
  - 文件名：原文件名
- 备选（若服务端支持）：`text/markdown` 纯文本 body
- 可选 Header：`Idempotency-Key`

下载（get content）：

- 作为 stream 写到 stdout 或文件
- 若输出到 stdout：不得打印额外信息（避免污染管道）

#### 7.3.4 `error`（错误解析与展示）

对齐 `docs/error-standard.md`：

- 若响应 `Content-Type` 为 JSON 且包含 `success=false`：
  - 解析字段：`code/message/requestId/details/fieldErrors`
  - 非 `--json`：打印 `code message`，并单独打印 `requestId`
  - `--json`：原样输出该 JSON
- 若非 JSON（例如 5xx HTML/文本）：
  - 非 `--json`：打印简短错误 + 状态码
  - `--json`：输出 `{ "ok": false, "error": { "message": "HTTP <status>: <text>" } }`（不引入额外 `code`）
 - 安全约束：`details/message` 不得包含明文 token、密码、签名 URL、原文完整内容（最多放摘要/坐标）

#### 7.3.5 `config`（配置合并）

- 合并顺序：flag > env > config > default
- profile 支持：`--profile <name>`（建议加入全局参数）
- 写入：`login` 负责创建/更新 profile

#### 7.3.6 `security`（token 存储）

Windows：

- DPAPI `CryptProtectData/CryptUnprotectData` 加密后落盘（推荐；避免依赖 Credential Manager UI）
- 文件权限：仅当前用户可读（尽量设置 ACL；至少避免写到公共目录）

跨平台退化：

- 仅当用户显式 `--store-in-config` 才允许明文写入 config

### 7.3 打包与发布

- GitHub Releases / 内部制品库（后续由平台自身托管也可）
- 产物：
  - `skillctl-windows-amd64.exe`
  - `skillctl-linux-amd64`
  - `skillctl-darwin-amd64` / `skillctl-darwin-arm64`
- 版本号：
  - 与 CLI 自身 semver
  - `skillctl version` 输出 build 信息（commit、build time）

建议交付方式：

- **静态链接优先**（尤其是 Windows），减少运行时依赖；若 libcurl 等无法完全静态，则在发布包内附带所需动态库。
- **CI 构建矩阵**：Windows（MSVC）、Linux（gcc/clang）、macOS（clang）
- **可重复构建**：锁定依赖版本（vcpkg baseline / conan.lock）

## 8. 测试策略

- **单元测试**
  - 坐标解析、FrontMatter 解析（若做本地预校验）、参数组合、退出码映射
- **集成测试（mock HTTP）**
  - 覆盖 401/404/409/422/500 的行为与输出
- **端到端测试（可选）**
  - 与本地启动的服务端联调（CI 可选）

## 9. 风险与对齐项

- **API 细节未冻结**：需求文档给出“建议 API”，实际字段/返回体需与后端对齐；CLI 通过集中 `endpoints` 与 `error model` 降低变更成本。
- **Token 安全存储实现**：跨平台 keychain/credential 需要额外依赖与适配；MVP 允许先以环境变量与一次性 `--token` 为主。
- **版本“最新”规则**：若后端未提供 `latest`/`stable` 标记或排序规则，CLI 不做“猜测”，仅透传服务端能力或要求显式版本。

## 10. CLI v1 建议命令集（便于排期）

> 说明：更完整的“命令树/参数/退出码/输出”以 `docs/cli-skillctl-规范.md` 为准；本节用于技术方案排期与后端接口对齐。

- **必须（MVP）**：`login`、`token set|unset|show`、`publish`、`get`、`show`、`search`、`version`
- **必须（MVP）**：`logout`
- **增强**：`versions`、`deps`、`deprecate`、`doctor/ping`

