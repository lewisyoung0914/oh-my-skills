# `skillctl` HTTP API Client 规格（api）

## 1. 模块职责

API Client 模块负责：

- 统一 HTTP 请求封装（baseUrl、headers、超时、代理、TLS、重试）
- 端点集中管理（便于后端路径调整）
- publish/upload 与 get/download 的流式处理
- 将响应解析为：成功数据、服务端 ErrorResponse、本地错误

## 2. 基础约定

### 2.1 Base URL 与路径拼接

- `baseUrl` 来自 flag/env/config 合并结果
- 路径拼接需保证不会出现双斜杠、不会丢失 path segment
- `{repo}/{group}/{name}/{version}` 必须经过 path segment 安全处理（见 `04-coord.md`）

### 2.2 通用请求头

- `Authorization: Bearer <token>`（若 token 非空）
- `User-Agent: skillctl/<version>`
- `Accept: application/json`（除 `get content` 外）
- `Idempotency-Key: <key>`（可选，仅 publish）

### 2.3 TLS 与代理

- 默认校验证书
- `--insecure` 时跳过 TLS 校验（仅调试）
- 代理：遵循系统/库默认（如 libcurl 环境变量）或显式配置（v1 可不做专门配置项）

## 3. 超时与重试

### 3.1 超时

- connect timeout：建议 5s（可常量或可配置）
- total timeout：默认 30s（可配置）

### 3.2 重试（仅对可重试类）

- 仅对 HTTP `429/503`（或明确可重试的服务端 `code` 白名单）重试
- 指数退避：base=500ms，max=8s
- 次数：`retries`（默认 0 或 1）
- 不重试：`409/422`（用户输入/策略冲突）

## 4. 端点清单（v1）

### 4.1 发布（publish）

- `POST /api/v1/repos/{repo}/skills`

上传形式：

- 首选：`multipart/form-data`
  - 字段名：`file`
  - filename：原文件名
- 备选（若后端支持）：纯文本 body
  - `Content-Type: text/markdown` 或 `text/plain`

成功响应：

- 允许返回 JSON（建议包含 `repo/group/name/version`、`content_sha256?`、`requestId?`）
- 若后端以 header 携带 `requestId`，CLI 可注入到 `data.requestId`

### 4.2 获取元数据（show）

- `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`
- `Accept: application/json`

### 4.3 下载原文（get content）

- `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/content`
- `Accept` 可为 `text/plain` 或不设置
- 必须支持流式读取

约束：

- 输出到 stdout 时：API Client 不得写任何进度/日志到 stdout（日志应上送到命令层并输出到 stderr）

### 4.4 搜索（search）

- `GET /api/v1/skills/search?q=...&group=...&name=...&label=...`
- query 参数需 URL encode

## 5. 响应分类与解析

### 5.1 成功（2xx）

- `--json`：命令层将其包裹为 `{ ok: true, data: ... }` 输出（或直接输出 data，但需与规范统一）
- 非 `--json`：交由 output 模块格式化表格/文本

### 5.2 服务端错误（非 2xx 且 JSON ErrorResponse）

判定条件：

- `Content-Type` 为 JSON（含 `application/json`），且 body 可解析并包含 `success=false`

处理：

- `--json`：原样输出该 ErrorResponse
- 非 `--json`：交由错误模块打印 `code message` + `requestId`

### 5.3 非 JSON 错误（例如 HTML/纯文本）

- `--json`：输出 `{ "ok": false, "error": { "message": "HTTP <status>: <text>" } }`
- 非 `--json`：stderr 输出简短错误（含 status）

## 6. 大文件与断点续传（增强预留）

若后端支持 Range/ETag，可在增强版本加入：

- Range 下载续传
- ETag/sha256 校验
- 本地缓存（见增强任务）

## 7. 与后端 API 对齐清单（实现前置）

为保证 CLI 可实现，后端需明确：

- publish 上传：multipart 字段名（建议 `file`）与纯文本 body 是否支持、支持的 `Content-Type`
- `requestId` 承载方式（响应头或 body 字段），与错误 envelope 的一致性
- `show/search` 的稳定字段（至少 `group/name/version/description`）
- `get content` 的 Content-Type 与编码（UTF-8 约定）

## 8. 验收标准

- 超时、重试仅对 429/503 生效且遵循退避策略。
- 对 409/422 不重试。
- `get content` 支持流式写入 stdout/文件且不污染 stdout。
