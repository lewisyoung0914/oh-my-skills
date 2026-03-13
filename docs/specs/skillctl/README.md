# `skillctl` specs（按模块拆分）

本目录将 `docs/cli-skillctl-技术方案.md` 按可实现的模块边界拆分为若干份 `spec` 文档，供后续实现/评审/联调使用。

## 文档列表

- `00-overview.md`：总体目标、范围、非目标、对外契约（stdout/stderr、JSON 形态、退出码）
- `01-cli-command-layer.md`：命令树/参数/交互约束（含 `--json`、`--quiet/--verbose`、help）
- `02-config-and-profile.md`：配置文件、profile、合并优先级（flag/env/config/default）
- `03-security-token-storage.md`：Token 获取优先级与本地安全存储策略（Windows DPAPI 优先）
- `04-coord.md`：坐标解析与校验（`group/name:version` 与三段式参数）
- `05-frontmatter-prevalidate.md`：FrontMatter 解析与本地预校验（“早失败”）
- `06-api-client.md`：HTTP 客户端、端点、超时/重试、上传/下载（含 stream 约束）
- `07-error-and-exitcode.md`：错误解析/展示、`--json` 透传、退出码映射（对齐 `error-standard`）
- `08-output.md`：表格/JSON 输出与通道约束（stdout/stderr）
- `09-testing-and-release.md`：测试策略、打包产物、CI 发布要点

## 约定

- `spec` 仅覆盖 CLI 侧；服务端具体字段若未冻结，以 `api-client` spec 的“对齐清单”为准。
- 所有 `--json` 行为与错误 envelope 以 `docs/error-standard.md` 为最终口径。
