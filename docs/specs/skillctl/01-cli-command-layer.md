# `skillctl` 命令层规格（CLI Command Layer）

## 1. 模块职责

命令层负责：

- 命令树定义、参数解析、help 文案
- 将 flag/env/config 合并后的运行参数下发到各子模块（api/config/security/output/error）
- 做“本地可判定”的用法错误与预校验（例如参数缺失、坐标格式不合法），并返回退出码 `2`
- 确保 stdout/stderr、`--json` 行为符合统一契约（见 `00-overview.md`）

## 2. 命令树（v1）

MVP：

- `skillctl version`
- `skillctl login`
- `skillctl logout`
- `skillctl token set|unset|show`
- `skillctl publish <file|dir>`
- `skillctl get <coord>`
- `skillctl show <coord>`
- `skillctl search <query>`

增强（先预留命令入口，不强制实现）：

- `skillctl versions <group/name>`
- `skillctl deps <coord>`
- `skillctl deprecate <coord>`

## 3. 全局参数（所有命令可用）

> 规范以 `docs/cli-skillctl-规范.md` 为准，本 spec 补充实现约束。

- `--base-url <url>`
- `--repo <repo>`
- `--token <token>`：一次性覆盖，不落盘
- `--json`
- `--quiet`：减少 stdout 输出（不影响 `--json` 主体）；致命错误仍输出到 stderr
- `--verbose`：输出调试信息到 stderr（不得输出明文 token）
- `--timeout <seconds>`：默认 30
- `--insecure`：跳过 TLS 校验，默认 false
- `--profile <name>`：默认 `default`
- （可选）`--retries <n>`：仅对 429/503 生效

环境变量等价项：

- `SKILLCTL_BASE_URL`
- `SKILLCTL_REPO`
- `SKILLCTL_TOKEN`
- `SKILLCTL_TIMEOUT`
- `SKILLCTL_INSECURE`

## 4. 参数组合与校验（本地用法错误 → exit=2）

### 4.1 通用校验

- `--timeout` 必须为正整数
- `--base-url` 必须为合法 URL（至少能被 URL 解析库接受）
- `--repo` 必须非空
- `--json` 与表格输出互斥：`--json` 时 stdout 只输出 JSON
- `--quiet` 不得影响 `get` 默认输出 content 的行为（即 `get` 默认 stdout 仍是 content）

### 4.2 `get` 特别约束

- `get` 默认 stdout 输出 content；stderr 才能输出日志/错误
- `-o/--output` 写文件时：
  - 若目标已存在且未 `--force`：视为本地错误（exit=2）或使用更贴近 IO 的退出码（v1 建议统一 exit=2）
  - 若成功写入：stdout 可输出摘要（或 `--quiet` 下不输出）

### 4.3 `token show` 安全约束

- 任意情况下不得输出明文 token（含 `--verbose`）
- 建议掩码规则：保留前 4 位与后 4 位，中间用 `...`（不足长度则整体 `****`）

## 5. 运行时依赖注入（模块间边界）

命令层应将以下运行参数统一封装为 `RuntimeContext`（或等价结构）传递：

- `baseUrl`
- `repo`（当前命令显式 repo 或默认 repo）
- `token`（按优先级解析后的最终 token，可为空）
- `timeoutSeconds`
- `tlsInsecure`
- `retries`
- `jsonMode`、`quiet`、`verbose`
- `profileName`

## 6. 验收标准

- 每个命令在 `--help` 中清晰展示：用法、必选参数、示例、全局参数。
- 任意用法错误返回 exit=2，并在非 `--json` 下给出可读提示；在 `--json` 下输出最小错误对象（见 `00-overview.md`）。
- `get` 默认内容输出不被污染（无日志/提示混入 stdout）。
