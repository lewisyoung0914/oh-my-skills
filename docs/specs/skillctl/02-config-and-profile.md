# `skillctl` 配置与 Profile 规格（Config & Profile）

## 1. 模块职责

配置模块负责：

- 配置文件读写（单文件、多 profile）
- profile 切换与默认 profile 解析
- 合并优先级（flag > env > config > default）
- 为 `login/logout` 等命令提供更新能力（写入 baseUrl/defaultRepo/timeout/retries 等）

## 2. 配置项与默认值（v1）

建议配置项（profile 内）：

- `baseUrl`：默认空（必须由 flag/env/config 任一提供）
- `defaultRepo`：默认空（建议在首次 `login` 时引导设置）
- `timeoutSeconds`：默认 30
- `tlsInsecure`：默认 false
- `retries`：默认 0（或 1；需与 CLI 规范统一）

顶层：

- `currentProfile`：默认 `default`
- `profiles`：map，key 为 profile 名称

`tokenSource`（诊断项）可选：用于展示 token 来自 flag/env/store/config；不参与决策。

## 3. 配置文件位置（v1 建议）

以技术方案为准：

- Windows：`%APPDATA%\skillctl\config.json`
- macOS：`~/Library/Application Support/skillctl/config.json`
- Linux：`~/.config/skillctl/config.json`

若运行环境无法写入上述目录（权限/只读），CLI 应返回明确错误（非 `--json` 输出可读信息，exit=1 或 2；v1 建议写入失败归类为本地错误 → exit=1）。

在 `--json` 下：

- 本地读写/解析错误输出最小错误对象 `{ "ok": false, "error": { "message": "..." } }`

## 4. 配置文件结构（示例）

```json
{
  "currentProfile": "default",
  "profiles": {
    "default": {
      "baseUrl": "https://skill.example.com",
      "defaultRepo": "hosted",
      "timeoutSeconds": 30,
      "tlsInsecure": false,
      "retries": 0
    }
  }
}
```

## 5. 合并优先级与解析规则

### 5.1 优先级

1. flag（如 `--base-url`、`--repo`、`--timeout`、`--insecure`、`--retries`）
2. 环境变量（`SKILLCTL_*`）
3. 配置文件（当前 profile）
4. 默认值

### 5.2 `--profile` 解析

- 未显式传入 `--profile`：
  - 优先读配置文件的 `currentProfile`
  - 若配置文件不存在：使用 `default`
- 显式 `--profile <name>`：
  - 若 config 存在该 profile：使用
  - 若不存在：
    - 读操作命令（如 `get/show/search`）：允许继续（只使用 flag/env/default）；但要在 `--verbose` 下提示“profile 不存在”
    - 写操作命令（如 `login`）：创建该 profile

## 6. `login/logout` 对 config 的影响

### 6.1 `login`

- 将 `baseUrl`（如提供）写入 profile
- 可选将 `defaultRepo` 写入 profile（若命令提供 `--repo` 或交互输入）
- 允许更新 `timeoutSeconds/tlsInsecure/retries`（若命令提供相应 flag）
- 设置 `currentProfile` 为该 profile（建议）

Token 写入规则见 `03-security-token-storage.md`。

### 6.2 `logout`

- 不强制删除 profile 配置（避免误删 baseUrl 等）
- 清理 token（安全存储/明文 config）后：
  - 允许保留 `baseUrl/defaultRepo` 以便下次登录复用
  - 或提供可选 flag（未来增强）让用户一并清理（v1 可不做）

## 7. 验收标准

- 合并优先级严格符合规范：flag/env 必须可覆盖 config。
- 缺省无 config 时命令可运行（纯 flag/env 驱动）。
- config 读写错误有清晰提示且不泄露敏感信息（尤其 token）。
