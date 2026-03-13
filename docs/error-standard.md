## 前后端通用异常标准（统一口径）

本规范定义 Skill 管理平台（后端 API / Web 管理端 / CLI）统一的错误响应结构、HTTP 状态码映射、错误码（code）体系、OpenAPI 表达方式，以及三端参考实现要点（后端/前端/CLI）。

> 适用范围：除“下载原文内容”这类纯文本/流式响应外，所有 API 错误都必须遵循本文的 JSON 错误结构。

---

## 1. 设计目标

- **稳定可编程**：前端/CLI 只依赖 `code` 做逻辑分支；`message` 仅用于展示与日志。
- **可观测可追踪**：每个错误都携带 `requestId`，方便定位日志与审计。
- **可定位到字段**：表单/参数校验失败必须返回结构化 `fieldErrors`。
- **与 HTTP 语义一致**：HTTP 状态码表达错误大类，`code` 表达细分原因。

---

## 2. 统一错误响应结构（Error Envelope）

所有非 2xx 的 JSON API，响应体必须满足以下结构（字段含义见下）：

```json
{
  "success": false,
  "code": "SKILL.CONFLICT_GAV",
  "message": "Artifact already exists for (group,name,version) in repo hosted",
  "requestId": "01J123ABC...",
  "timestamp": "2026-03-12T10:12:30.123Z",
  "path": "/api/v1/repos/hosted/skills",
  "details": {
    "repo": "hosted",
    "group": "com.example",
    "name": "my-skill",
    "version": "1.0.0",
    "existingId": "12345"
  },
  "fieldErrors": [
    { "field": "group", "reason": "INVALID_FORMAT", "message": "Only [a-zA-Z0-9_.-] allowed" }
  ]
}
```

### 2.1 字段定义

- **success**：固定为 `false`。
- **code**：稳定错误码，格式见第 4 节（必须）。
- **message**：面向人类的简短说明（必须）。
- **requestId**：请求链路 ID（必须）。后端生成并在日志中打印；若网关已生成可透传。
- **timestamp**：ISO8601 时间戳（必须）。
- **path**：请求 path（必须）。
- **details**：可选。用于携带上下文（不要放敏感信息）。
- **fieldErrors**：可选。参数/字段级校验错误列表（建议用于 422）。

### 2.2 `fieldErrors` 规范

`fieldErrors` 仅用于“字段/参数校验未通过”，例如 FrontMatter 缺失、坐标格式错误、策略不允许 SNAPSHOT 等。结构：

```json
{ "field": "version", "reason": "NOT_ALLOWED", "message": "Snapshot versions are not allowed in this repo" }
```

- **field**：字段名（如 `group` / `name` / `version` / `content` / `requires[2]` 等）。
- **reason**：枚举值（见第 5 节）。
- **message**：可展示给用户的提示语。

---

## 3. HTTP 状态码与平台口径

| HTTP | 何时使用 | 例子 |
|---:|---|---|
| 400 | 请求体/格式错误 | JSON 解析失败、multipart 缺少文件 |
| 401 | 未认证/Token 无效 | 未带 Token、Token 过期/撤销 |
| 403 | 已认证但无权限 | RBAC / Scope 不满足 |
| 404 | 资源不存在 | repo 不存在、skill/version 不存在 |
| 409 | 资源冲突 | 发布同一 `(repo,group,name,version)` 冲突；尝试覆盖 immutable |
| 413 | 载荷过大 | 单文件超出限制 |
| 415 | 不支持的类型 | Content-Type 不支持 |
| 422 | 业务校验失败 | FrontMatter 缺失/无效、坐标不合法、依赖坐标不合法 |
| 429 | 限流（可选） | 频控/配额触发 |
| 500 | 未预期错误 | 代码异常、未知错误 |
| 503 | 依赖不可用 | 对象存储/索引不可用 |

> 建议：**校验类错误优先用 422**；**唯一性/不可变冲突用 409**。

---

## 4. 错误码（code）命名规则

- 格式：`<DOMAIN>.<REASON>`，全大写，使用点分层级。
- `DOMAIN` 参考：`AUTH`、`REPO`、`SKILL`、`AUDIT`、`STORAGE`、`INDEX`、`INTERNAL`。
- `REASON` 要表达可编程原因，避免仅用自然语言。

示例：
- `AUTH.UNAUTHORIZED`
- `REPO.NOT_FOUND`
- `SKILL.CONFLICT_GAV`
- `SKILL.FRONTMATTER_INVALID_YAML`

---

## 5. 统一枚举：字段校验原因（fieldErrors.reason）

建议固定以下枚举（全大写）：

- `REQUIRED`：必填缺失
- `INVALID_FORMAT`：格式/正则不合法
- `OUT_OF_RANGE`：长度/范围不合法
- `NOT_ALLOWED`：策略不允许（如不允许 SNAPSHOT）
- `CONFLICT`：字段级冲突（如与现有资源冲突，但不一定是 409）

---

## 6. MVP 必备错误码清单（建议）

### 6.1 认证与权限

- `AUTH.UNAUTHORIZED` (401)
- `AUTH.TOKEN_EXPIRED` (401)
- `AUTH.TOKEN_REVOKED` (401)
- `AUTH.FORBIDDEN` (403)
- `AUTH.INSUFFICIENT_SCOPE` (403)

### 6.2 仓库

- `REPO.NOT_FOUND` (404)
- `REPO.READONLY` (403 或 422，视策略：proxy/group 禁止发布可用 403)
- `REPO.QUOTA_EXCEEDED` (422)
- `REPO.FILE_TOO_LARGE` (413)

### 6.3 Skill 发布/校验

- `SKILL.FRONTMATTER_MISSING` (422)
- `SKILL.FRONTMATTER_INVALID_YAML` (422)
- `SKILL.INVALID_COORDINATE` (422)
- `SKILL.CONFLICT_GAV` (409) —— 对应需求 R1
- `SKILL.IMMUTABLE` (409) —— 对应需求 R2
- `SKILL.DEPENDENCY_INVALID` (422)
- `SKILL.DEPENDENCY_MISSING` (422，启用强校验时)
- `SKILL.DEPENDENCY_CYCLE` (422，增强)

### 6.4 资源状态

- `SKILL.NOT_FOUND` (404)
- `SKILL.VERSION_NOT_FOUND` (404)
- `SKILL.DELETED` (410 可选；或 404 + details.status=deleted)

### 6.5 系统

- `STORAGE.UNAVAILABLE` (503)
- `INDEX.UNAVAILABLE` (503，增强：引入全文检索时)
- `INTERNAL.ERROR` (500，兜底)

---

## 7. OpenAPI 表达（Schema 与复用方式）

- 定义统一的 `ErrorResponse` schema（见 `docs/openapi-error.yaml`）。
- 所有非 2xx 响应引用同一 schema：
  - `400/401/403/404/409/413/415/422/429/500/503` → `ErrorResponse`
- `fieldErrors` 仅在 `422` 常见，但 schema 中可选字段以支持通用性。

---

## 8. 前端处理规范（Web）

### 8.1 统一错误展示策略

- **表单页**（发布/仓库编辑/Token 创建等）：
  - 若存在 `fieldErrors`：按 `field` 映射到表单控件展示。
  - 否则：展示 `message`（toast 或 inline alert）。
- **列表/详情页**：
  - `404`：引导返回列表或选择仓库。
  - `403`：提示无权限并引导申请权限或切换账号。
- **可追踪**：UI 中提供 `requestId` 的复制入口。

### 8.2 统一重试策略

- **仅**对 `503`/`429`（或 `code` 明确可重试）做指数退避重试。
- 对 `409/422` 不重试（属于用户输入/策略冲突）。

---

## 9. CLI 处理规范

### 9.1 输出格式

- 默认：输出 `code` + `message`，并在下一行输出 `requestId`（若存在）。
- `--json`：原样输出整个 `ErrorResponse` JSON（便于脚本处理）。

### 9.2 退出码（exit code）建议

- `0`：成功
- `2`：用法错误/参数解析失败
- `10`：未认证（401）
- `11`：无权限（403）
- `12`：未找到（404/410）
- `13`：冲突（409）
- `14`：校验失败（422）
- `15`：限流/超时可重试（429/503）
- `1`：其他错误（500 等）

---

## 10. 安全约束（必须遵守）

- `details` **禁止**包含：Token、密码、敏感密钥、原文完整内容（可放摘要/坐标）。
- `message` 不应暴露内部堆栈、SQL、对象存储签名 URL 等。

