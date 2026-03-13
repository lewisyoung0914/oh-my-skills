# `skillctl` 输出格式与通道规格（output）

## 1. 模块职责

输出模块负责：

- 表格/文本输出（默认面向人）
- `--json` 输出（机器可读）的统一封装（成功时）
- stdout/stderr 通道约束的集中治理（尤其 `get`）

## 2. 输出模式

### 2.1 JSON 模式（`--json`）

成功输出（统一 envelope）：

- `{ "ok": true, "data": <object|array> }`

失败输出：

- 服务端 `ErrorResponse`：原样输出（由错误模块处理）
- 本地错误：最小对象 `{ "ok": false, "error": { "message": "..." } }`

### 2.2 表格/文本模式（默认）

原则：

- 面向人读，尽量短
- 结构化信息用列对齐表格；单对象用 key/value 列表
- 错误信息永远输出到 stderr

## 3. stdout / stderr 规则（强制）

- stdout：主要结果
- stderr：日志、告警、错误

### 3.1 `get` 特殊规则

- 默认（无 `-o`）：
  - stdout：仅 content
  - stderr：仅错误/日志（如 `--verbose`）
- 有 `-o/--output`：
  - 文件写入成功：stdout 可输出摘要（或 `--quiet` 下不输出）
  - 文件写入失败：stderr 输出原因，并返回对应退出码

## 4. 表格列建议（v1）

### 4.1 `search`

建议列：

- `group/name`
- `latest_version`（若 API 返回）
- `description`
- `label`
- `author`
- `updated_at`

### 4.2 `show`

建议列（按后端返回取舍）：

- `repo`
- `group`
- `name`
- `version`
- `description`
- `label`
- `author`
- `status`
- `created_at`
- `size_bytes`
- `content_sha256`

### 4.3 `publish` 成功输出

建议输出：

- 坐标（repo/group/name/version）
- `content_sha256`（若返回）
- `requestId`（若返回）
- 以及一个可直接复制的下载命令示例（但必须输出到 stdout；如担心脚本解析，建议仅在非 `--json` 下输出）

## 5. `--quiet/--verbose` 约束

- `--quiet`：
  - 允许减少“成功提示/摘要”
  - 不得影响 `--json` 主体输出
  - 不得影响 `get` 默认 content 输出
- `--verbose`：
  - 仅输出到 stderr
  - 不能输出敏感信息（token 等）

## 6. 验收标准

- 任意命令在管道场景下可稳定使用（stdout 不被日志污染）。
- `--json` 输出为单一 JSON 文档，不混入表格/提示文本。
