# `skillctl` 错误处理与退出码规格（error）

## 1. 模块职责

错误模块负责：

- 将服务端 `ErrorResponse`（`docs/error-standard.md`）解析为可展示结构
- 在非 `--json` 模式下输出稳定、简洁的人类可读错误
- 统一 HTTP/错误码到 CLI 退出码映射
- 本地错误（网络/IO/解析/超时）在 `--json` 下输出最小错误对象

## 2. 服务端错误结构（权威来源）

服务端 JSON 错误 envelope 以 `docs/error-standard.md` 为准，关键字段：

- `success=false`
- `code`
- `message`
- `requestId`
- `details?`
- `fieldErrors?`

## 3. `--json` 行为（强制）

- 若服务端返回 `ErrorResponse`：**原样输出**（不得改字段名、不得包裹）
- 若为本地错误：
  - 输出 `{ "ok": false, "error": { "message": "..." } }`
  - 不引入平台 `code`（避免与服务端 code 冲突）

## 4. 非 `--json` 展示格式（建议）

stderr 输出至少包含：

- 第一行：`<code> <message>`（若是本地错误则 `ERROR <message>`）
- 第二行（若存在）：`requestId: <id>`
- 若存在 `fieldErrors`：可逐行输出 `- <field>: <reason> <message>`（不要求完全一致，但需可读）

不得输出：

- 明文 token、密码、签名 URL、原文完整内容（最多摘要/坐标）

## 5. 退出码映射（强制对齐）

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

映射规则：

- 若拿到 HTTP status：按 status 大类映射
- 若只有本地错误（无 HTTP status）：
  - 超时/可重试网络错误：建议用 `15` 或 `1`（v1 建议：超时归类为“可重试” → `15`）
  - 其他本地 IO/解析：`1`

## 6. 典型场景要求

- 409 publish 冲突：必须提示“坐标已存在/不可覆盖”
- 401/403：提示鉴权失败/无权限，并提示使用 `login` 或检查 token
- 404/410：提示资源不存在（repo 或 skill/version）
- 422：若存在 `fieldErrors`，必须让用户能定位到具体字段（尤其 FrontMatter 缺失/非法）

## 7. 验收标准

- `--json` 下：服务端错误 envelope 字段完全保留且可被脚本稳定解析。
- 非 `--json` 下：至少输出 `code message` 与 `requestId`（若存在）。
- 退出码严格符合平台口径；本地用法错误固定为 `2`。
