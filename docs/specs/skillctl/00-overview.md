# `skillctl` v1 总体规格（Overview）

## 1. 目标与范围

### 1.1 CLI 定位

`skillctl` 是 Skill 管理平台的命令行客户端：

- 面向人：发布/检索/下载 Skill，查看元数据与版本，辅助治理（增强项）。
- 面向系统：CI/CD 以 Token 非交互方式发布与拉取。

平台负责存储/分发/治理；CLI 负责体验层与 API 封装，不承担 Skill 运行时。

### 1.2 v1（MVP）范围

必须实现：

- 鉴权与配置：`login/logout`、`token set|unset|show`、profile
- 发布：`publish`
- 拉取原文：`get`（content）
- 查看元数据：`show`
- 搜索：`search`
- CLI 自身版本：`version`
- 统一错误处理与退出码映射（对齐 `docs/error-standard.md`）

增强（MVP 后）：

- `deps`、`versions`、`deprecate`、幂等发布、下载缓存等

### 1.3 非目标（v1 不做）

- 不实现 Skill 执行/运行时。
- 不在 CLI 侧“猜测 latest”规则；如需 latest 必须由服务端能力明确支持。
- 不做跨 repo 本地聚合/过滤（`search` 只调用服务端搜索接口）。

## 2. 关键对外契约（必须遵守）

### 2.1 stdout / stderr 通道约束

- stdout：命令主要结果（表格、`--json` 输出、`get` 默认 content）。
- stderr：日志、告警、错误信息。
- 特别约束：`skillctl get` 在默认（输出到 stdout）时 **不得**混入任何额外信息（包括进度、日志、提示）。

### 2.2 `--json` 输出约束

- 成功：输出 `{ "ok": true, "data": <object|array> }`
- 失败：
  - 若服务端返回 `ErrorResponse`（见 `docs/error-standard.md`）：**原样输出**该 JSON
  - 若为本地错误（网络/超时/解析/IO）：输出最小对象
    - `{ "ok": false, "error": { "message": "..." } }`

### 2.3 鉴权与 Token 获取优先级

请求头：`Authorization: Bearer <token>`

Token 来源优先级（高 → 低）：

1. `--token`（一次性，不落盘）
2. 环境变量 `SKILLCTL_TOKEN`
3. 本地安全存储/配置文件（profile）

### 2.4 退出码（exit code）统一口径

以 `docs/error-standard.md` 第 9.2 节为准：

- `0`：成功
- `2`：用法错误/参数解析失败（含本地预校验失败）
- `10`：未认证（401）
- `11`：无权限（403）
- `12`：未找到（404/410）
- `13`：冲突（409）
- `14`：校验失败（422）
- `15`：限流/依赖不可用（429/503）
- `1`：其他错误（500 等）

## 3. 服务端 API（默认实现路径）

CLI 默认端点（后续若服务端微调，需集中在 endpoints 配置处修改）：

- 发布：`POST /api/v1/repos/{repo}/skills`
- 精确元数据：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`
- 下载原文：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/content`
- 搜索：`GET /api/v1/skills/search?q=...&group=...&name=...&label=...`

增强端点：

- 版本列表：`GET /api/v1/repos/{repo}/skills/{group}/{name}/versions`
- 依赖解析：`GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deps?transitive=true`
- 废弃标记：`POST /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deprecate`

## 4. 非功能要求（v1）

- 安全：日志与错误信息不得输出明文 token；`token show` 只显示掩码。
- 可观测：若服务端返回 `requestId`，CLI 需在错误展示中输出；`--verbose` 可打印 requestId、耗时等（不得输出敏感内容）。
- 跨平台：Windows/macOS/Linux；以单可执行或带最少动态库发布。

## 5. 验收标准（整体）

- MVP 命令均可用，并满足 stdout/stderr、`--json`、退出码契约。
- 所有服务端错误 envelope（JSON）在 `--json` 下 **不改写**，在非 `--json` 下展示 `code message` + `requestId`（若有）。
- `get` 默认输出可用于管道（例如 `skillctl get ... | some-tool`），不被日志污染。
