# `skillctl` 测试与发布规格（Testing & Release）

## 1. 目标

确保 `skillctl` v1：

- 行为稳定可回归（参数解析、输出、退出码、错误处理）
- 跨平台可交付（Windows/macOS/Linux）
- 发布产物可追踪（版本/commit/build_time）

## 2. 测试分层

### 2.1 单元测试（必做）

覆盖模块：

- `coord`：解析与非法输入边界
- `frontmatter`：提取、YAML 解析、必填字段、requires 解析
- `config`：合并优先级、profile 选择、缺省 config 行为
- `security`：token 掩码规则（不测明文输出）、存取接口的 mock
- `error`：退出码映射、`--json` 透传策略
- `output`：stdout/stderr 路由（可用“捕获输出”方式验证）

### 2.2 集成测试（必做：mock HTTP）

使用 mock server 覆盖典型响应：

- 401 / 403 / 404 / 409 / 422 / 500 / 503 / 429
- JSON ErrorResponse 与非 JSON 错误体两类
- `get content` 的流式响应（文本）

断言：

- 退出码映射正确
- `--json` 透传不改写
- `get` stdout 不污染

### 2.3 端到端（可选）

与本地服务端联调（CI 可选），用于发现协议差异。

## 3. 构建与依赖

建议（来自技术方案）：

- CMake
- 依赖管理：vcpkg（推荐）或 Conan
- 依赖：CLI11 / libcurl / nlohmann-json / yaml-cpp / Catch2 或 GoogleTest

构建元信息注入：

- `skillctl version` 必须输出：
  - `skillctl_version`
  - `commit`
  - `build_time`

## 4. 打包与发布

产物建议：

- `skillctl-windows-amd64.exe`
- `skillctl-linux-amd64`
- `skillctl-darwin-amd64`
- `skillctl-darwin-arm64`

交付方式：

- 静态链接优先；若无法完全静态，发布包需附带必要动态库
- Releases（GitHub 或内部制品库）

## 5. 验收标准

- 在三大平台上：基础命令可执行，`--help`、`version` 正常。
- 集成测试覆盖所有关键 HTTP 状态与 `--json`/非 `--json` 输出差异。
- 发布包可直接运行，无需额外安装运行时（或在 README 中明确依赖）。
