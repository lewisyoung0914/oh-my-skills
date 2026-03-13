# `skillctl` Token 安全与本地存储规格（Security / Token Storage）

## 1. 模块职责

安全模块负责：

- Token 获取优先级与脱敏展示
- 本地安全存储（Windows 优先使用 DPAPI）
- 明文落盘的退化策略（必须显式授权）
- 防泄露约束：日志、错误、调试输出的脱敏

## 2. Token 获取优先级（高 → 低）

1. `--token`（一次性覆盖，不落盘）
2. 环境变量 `SKILLCTL_TOKEN`
3. 本地安全存储 / 配置文件（profile 绑定）

> 任一请求若最终 token 为空：服务端可能返回 401；CLI 仍应发起请求并按错误标准处理（或在命令层对“必须鉴权”的命令做前置提示，但不强制阻断）。

## 3. 本地存储策略

### 3.1 Windows（推荐实现）

优先使用 Windows DPAPI：

- 加密：`CryptProtectData`
- 解密：`CryptUnprotectData`
- 加密输出与 profile 绑定（建议将 profile 名称作为附加熵/上下文）

存储介质：

- 推荐：加密后写入独立 token 文件（例如与 `config.json` 同目录，文件名含 profile）
- 或：加密后写入 config（独立字段），但仍需确保“token 不可明文可见”

权限：

- 文件必须仅当前用户可读（尽量设置 ACL；至少避免写到公共目录）

### 3.2 跨平台退化（v1 允许）

若无法实现系统级安全存储：

- 默认 **不允许** token 明文写入 config
- 仅当用户显式指定 `--store-in-config` 时才允许明文写入（并在命令输出中明确告知风险）
- 否则引导用户使用环境变量/CI secret 或 `--token`

## 4. `login` / `token set` 行为

### 4.1 写入规则

- 尝试写入安全存储（Windows DPAPI）
- 若不可用：
  - 若有 `--store-in-config`：明文写入 config（profile 绑定）
  - 否则：拒绝落盘，提示改用环境变量/`--token`

### 4.2 `token unset` / `logout`

- 清除当前 profile 的 token（安全存储与可能存在的 config 明文字段）
- 不影响其他 profile

## 5. `token show` 脱敏规则（强制）

- 任意情况下不得输出明文 token
- 建议规则：
  - 长度 >= 10：显示 `前4...后4`
  - 长度 < 10：显示 `****`
- `--json` 下也不得输出明文 token（建议输出 `{ "ok": true, "data": { "tokenMasked": "abcd...wxyz" } }`）

## 6. 日志/错误脱敏（强制）

不得在以下位置出现明文 token：

- stderr 日志（含 `--verbose`）
- 错误信息（含本地异常 message）
- 任何 JSON 输出（无论成功/失败）

`--verbose` 允许输出：

- HTTP method、path（不含 query 中的敏感信息）、status、requestId、耗时
- 允许输出 header 名称，但 header 值必须脱敏（`Authorization` 永远不输出）

## 7. 验收标准

- 在 Windows 上：token 可写入、可读取、可删除；配置迁移不泄露 token。
- 任意模式下：不会在输出中出现明文 token（含异常堆栈/调试日志）。
- `--store-in-config` 未显式开启时：不会产生 token 明文字段落盘。
