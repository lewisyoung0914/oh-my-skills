# `skillctl` CLI 规范（v1 草案）

> 本文定义 Skill 管理平台命令行工具 `skillctl` 的命令树、参数、配置、输出与错误处理规范，供 CLI/后端/Web 三端对齐。
>
> - 适用范围：`skillctl` v1（MVP + 可选增强）
> - 错误结构规范：遵循 `docs/error-standard.md`

## 1. 基本约定

### 1.1 名词与坐标

- **坐标（coord）**：`group/name:version`
  - `group`：仅允许 `[a-zA-Z0-9_.-]`
  - `name`：仅允许 `[a-zA-Z0-9_.-]`
  - `version`：建议 semver（允许 `-rc1`、`+build` 后缀）
- **repo**：仓库名字符串（如 `hosted`）

### 1.2 命令输出通道

- **stdout**：命令的主要结果（如列表表格、JSON 文本、`get` 成功时的 `saved to <path>` 提示）
- **stderr**：日志、告警、错误信息（避免污染管道）

### 1.3 全局参数（所有命令可用）

- `--token <token>`：一次性 token（不落盘，适合 CI）

### 1.4 环境变量

- `SKILLCTL_TOKEN`：等价于 `--token`

### 1.5 配置模型与优先级

- 通过 **base**（命名基础配置）管理服务端地址：`skillctl base <base-name> <base-url>`
- CLI 命令使用 `base-name` 作为第一个位置参数，例如：`skillctl search dev "测试"`
- token 解析优先级（高 → 低）：
  1. CLI flag `--token`
  2. 环境变量 `SKILLCTL_TOKEN`
  3. 配置文件中该 base 下明文 token（仅在显式允许时）

## 2. 命令树（v1）

### 2.1 MVP 命令（必须实现）

- `skillctl version`
- `skillctl base <base-name> <base-url>`
- `skillctl login <base-name>`
- `skillctl logout <base-name>`
- `skillctl token set|unset|show <base-name>`
- `skillctl publish <base-name> <file|dir>`
- `skillctl get <base-name> <coord>`
- `skillctl show <base-name> <coord>`
- `skillctl search <base-name> <query>`

### 2.2 增强命令（按后端能力启用）

- `skillctl versions <base-name> <group/name>`
- `skillctl deps <base-name> <coord>`
- `skillctl deprecate <base-name> <coord>`
- `skillctl undeprecate <base-name> <coord>`
- `skillctl doctor` / `skillctl ping`

## 3. 命令规范（详细）

> 说明：若后端暂未实现某 API，CLI 需给出清晰错误：`INTERNAL.ERROR` 或更具体的 `code`（建议后端返回 404/501 类语义；CLI 仅展示）。

### 3.1 `skillctl version`

用途：显示 CLI 版本与构建信息。

- 输出（表格/文本）：
  - `skillctl_version`
  - `commit`
  - `build_time`

### 3.2 `skillctl login <base-name>`

用途：为指定 base 写入 token 到本地凭据存储。

- 参数：
  - `<base-name>`：已通过 `skillctl base` 配置的名称
  - `--token <token>`（可选；未提供则交互式输入）
  - `--store-in-config`（可选；允许将 token 明文写入配置文件；默认禁止）
- 行为：
  - token 优先写入系统安全存储（Windows DPAPI / Credential Manager）
  - 若无法使用安全存储且未 `--store-in-config`，提示用户使用环境变量/`--token`

### 3.3 `skillctl logout`

用途：清理当前 profile 的 token 与可选 base-url。

- 参数：
  - `--profile <name>`（可选）
- 行为：
  - 删除安全存储中的 token
  - 不删除其他 profile

### 3.4 `skillctl token set|unset|show`

- `token set <token>`：写入 token（同 `login` 的存储规则）
- `token unset`：清除 token
- `token show`：输出掩码 token（例如 `abcd...wxyz`）

### 3.5 `skillctl publish <file|dir>`

用途：发布 Skill 制品到指定 repo。

- 参数：
  - `--repo <repo>`（必选或使用默认 repo）
  - `--dry-run`：仅本地解析 FrontMatter 与坐标校验，不上传
  - `--idempotency-key <key>`：透传 `Idempotency-Key`（可选）
  - `--allow-snapshot`（可选）：仅影响本地预校验；最终以服务端策略为准
- 输入：
  - `<file>`：一个 markdown 文件（如 `SKILL.md`）
  - `<dir>`：目录发布（规则：递归查找 `SKILL.md` 或 `*.md`，v1 建议先只支持单文件；若支持目录需在实现说明中明确）
- API：
  - `POST /api/v1/repos/{repo}/skills`
- 成功输出（表格/文本）：
  - `repo`, `group`, `name`, `version`, `content_sha256?`, `requestId?`（若后端返回）
- 常见失败：
  - 409：`SKILL.CONFLICT_GAV` / `SKILL.IMMUTABLE`
  - 422：`SKILL.FRONTMATTER_*` / `SKILL.INVALID_COORDINATE` / `SKILL.DEPENDENCY_*`

### 3.6 `skillctl get <coord>`

用途：下载原文 content 到本地，用户无需指定保存路径与文件名。

- 默认保存路径：`~/.skillctl/skills/{group}/{name}/{version}/SKILL.md`（Windows 为 `%USERPROFILE%\.skillctl\skills\...`）
- 参数：
  - `-o, --output <path>`：可选，指定保存路径（不指定则使用上述默认路径）
  - `--force`：覆盖已存在文件
- API：
  - `GET /api/v1/skills/{group}/{name}/{version}/content`
- 输出：
  - 成功时在 stdout 打印一行 `saved to <path>`；内容写入默认或 `-o` 指定路径

### 3.7 `skillctl show <coord>`

用途：获取元数据（不含 content）。

- 参数：
  - `--repo <repo>`
- API：
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`
- 输出：
  - 表格/JSON（含 description/label/author/requires/sha256/size/status/created_at 等，取决于后端返回）

### 3.8 `skillctl search <query>`

用途：搜索 Skill。

- 参数：
  - `--group <group>`（可选）
  - `--name <name>`（可选）
  - `--label <label>`（可选）
  - `--author <author>`（可选，若后端支持）
  - `--limit <n>`（可选）
- API：
  - `GET /api/v1/skills/search?q=...&group=...&name=...&label=...`

### 3.9 `skillctl versions <group/name>`（增强）

- API：
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/versions`

### 3.10 `skillctl deps <coord>`（增强）

- 参数：
  - `--transitive`（默认 true 或 false 需统一；建议默认 true）
  - `--tree`：树形展示
- API：
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deps?transitive=true`

### 3.11 `skillctl deprecate|undeprecate <coord>`（增强）

- `deprecate` 参数：
  - `--reason <text>`（可选）
  - `--replacement <coord>`（可选）
- API：
  - `POST /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deprecate`
  - `POST /api/v1/repos/{repo}/skills/{group}/{name}/{version}/undeprecate`（若后端提供）

## 4. 输出格式规范

### 4.1 `--json` 模式

#### 成功响应

CLI 成功时输出：

```json
{ "ok": true, "data": { "requestId": "..." } }
```

#### 失败响应

CLI 失败时：

- 若服务端返回了 `docs/error-standard.md` 的 `ErrorResponse`，则 **原样输出**（或包一层 `ok=false` 但必须保留原字段；v1 推荐“原样输出”以便脚本兼容）。
- 若是网络/解析等本地错误，CLI 需构造等价结构：
  - v1 处理：直接抛出异常并由最外层统一捕获
    - 非 `--json`：stderr 输出可读说明
    - `--json`：输出 `{ "ok": false, "error": { "message": "..." } }`（不引入额外 `code`）

### 4.2 表格/文本模式

- 成功：打印关键信息（避免冗长）
- 失败：至少打印
  - `code` + `message`
  - `requestId`（若存在）

## 5. 错误处理与退出码（对齐 `docs/error-standard.md`）

### 5.1 HTTP 状态码 → 退出码

以 `docs/error-standard.md` 第 9.2 节为准（v1 采用同一口径）：

- `0`：成功
- `2`：用法错误/参数解析失败
- `10`：未认证（401）
- `11`：无权限（403）
- `12`：未找到（404/410）
- `13`：冲突（409）
- `14`：校验失败（422）
- `15`：限流/依赖不可用（429/503）
- `1`：其他错误（500 等）

### 5.2 重试策略

- 仅对 `429/503`（或明确可重试 `code`）做指数退避重试（可通过 `--retries <n>` 控制，默认 0 或 1）
- 对 `409/422` **不重试**

## 6. 安全要求（必须遵守）

- CLI **不得**在日志/错误中输出明文 token、密码、敏感 URL（如带签名的对象存储 URL）
- `--verbose` 仅输出请求方法、path、状态码、requestId、耗时；body 需脱敏或不输出

## 7. 与后端 API 的对齐清单（实现前置）

为保证 CLI 可实现，后端需明确：

- `publish` 接口支持的上传形式：multipart 字段名（建议 `file`）与纯文本 body 的 Content-Type
- `show`/`search` 的返回字段（至少包含 `group/name/version/description`）
- `deps` 返回结构（是否包含缺失/循环信息、如何表达）
- `undeprecate` 是否存在独立接口
- `requestId` 响应头/字段的统一承载方式（建议在错误 envelope 与成功响应头同时提供）

