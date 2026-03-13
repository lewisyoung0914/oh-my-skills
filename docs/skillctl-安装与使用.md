## skillctl CLI：安装与使用

本文面向 **CLI 使用者**，覆盖 `skillctl` 的安装、登录鉴权、常用命令与脚本化（`--json` / 退出码）约定。

> 规格与契约来源：
>
> - `docs/specs/skillctl/`（模块化 spec）
> - `docs/error-standard.md`（错误 envelope 与退出码）

---

## 安装

### 方式 A：下载预编译（二进制）

若你们发布了 release 产物（建议命名）：

- Windows：`skillctl-windows-amd64.exe`
- Linux：`skillctl-linux-amd64`
- macOS：`skillctl-darwin-amd64` / `skillctl-darwin-arm64`

安装建议：

- Windows：将 `skillctl.exe` 放到某个目录（如 `C:\tools\skillctl\`），并把该目录加入 `PATH`
- Linux/macOS：将二进制放到 `/usr/local/bin/skillctl` 并 `chmod +x`

验证：

```bash
skillctl version
skillctl --help
```

### 方式 B：从源码构建（vcpkg + CMake）

前置：

- CMake 3.24+
- C++20 编译器（Windows：MSVC 2022；Linux/macOS：clang/gcc）
- vcpkg（建议使用官方安装方式）

Windows PowerShell 示例：

```powershell
$env:VCPKG_ROOT="C:\dev\vcpkg"

cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_FEATURE_FLAGS=manifests

cmake --build build --config Release

.\build\Release\skillctl.exe version
```

---

## 快速上手

### 1) 登录（写入 baseUrl + token）

```bash
skillctl login --base-url https://skill.example.com --repo hosted
```

- token 默认写入 **系统安全存储**（Windows 优先 DPAPI 加密文件方式）
- 若当前平台无法安全存储：需显式允许明文落盘

```bash
skillctl login --base-url https://skill.example.com --repo hosted --store-in-config
```

### 2) 查看 token（仅掩码）

```bash
skillctl token show
skillctl token show --json
```

> 安全约束：任何情况下（含 `--verbose` / `--json`）都 **不会输出明文 token**。

### 3) 发布（publish）

```bash
skillctl publish ./SKILL.md --repo hosted
skillctl publish ./SKILL.md --repo hosted --dry-run
skillctl publish ./SKILL.md --repo hosted --json
```

发布前会做本地“早失败”校验（FrontMatter / 坐标 / requires 格式），最终校验以服务端为准。

### 4) 下载原文（get）

默认保存到本地缓存目录，无需指定路径与文件名：

```bash
skillctl get <base-name> com.example/my-skill:1.0.0
```

文件会保存到：`~/.skillctl/skills/com.example/my-skill/1.0.0/SKILL.md`（Windows 为 `%USERPROFILE%\.skillctl\skills\...`）。

覆盖已存在文件：

```bash
skillctl get <base-name> com.example/my-skill:1.0.0 --force
```

如需保存到其它路径，可使用可选的 `-o`：

```bash
skillctl get <base-name> com.example/my-skill:1.0.0 -o ./custom/SKILL.md
```

### 5) 查看元数据（show）

```bash
skillctl show com.example/my-skill:1.0.0 --repo hosted
skillctl show com.example/my-skill:1.0.0 --repo hosted --json
```

### 6) 搜索（search）

```bash
skillctl search "rag"
skillctl search "rag" --group com.example --label prompt
skillctl search "rag" --json
```

---

## 全局参数（常用）

这些参数对所有命令可用：

- `--base-url <url>`：服务端地址（也可用环境变量 `SKILLCTL_BASE_URL`）
- `--repo <repo>`：默认仓库（也可用 `SKILLCTL_REPO`）
- `--token <token>`：一次性 token（不落盘；也可用 `SKILLCTL_TOKEN`）
- `--timeout <seconds>`：默认 30（也可用 `SKILLCTL_TIMEOUT`）
- `--insecure`：跳过 TLS 校验（也可用 `SKILLCTL_INSECURE=1`）
- `--profile <name>`：profile 名（默认 `default`）
- `--retries <n>`：仅对 429/503 重试
- `--json`：机器可读 JSON 输出
- `--quiet`：减少成功提示（不影响 `--json` 主体；不影响 `get` 默认 content 输出）
- `--verbose`：调试信息输出到 stderr（不会输出敏感信息）

配置合并优先级（高 → 低）：

1. flag
2. env
3. config（profile）
4. default

---

## 配置文件位置

默认配置文件：

- Windows：`%APPDATA%\skillctl\config.json`
- macOS：`~/Library/Application Support/skillctl/config.json`
- Linux：`~/.config/skillctl/config.json`

> token 的安全存储与配置文件是分离的：安全存储成功时，配置文件不会出现明文 token 字段。

---

## `--json` 输出约定（脚本化）

成功：

```json
{ "ok": true, "data": { } }
```

失败：

- **服务端返回** `docs/error-standard.md` 的 `ErrorResponse`：`skillctl` 会 **原样输出**（不包裹、不改字段）
- **本地错误**（网络/超时/IO/解析）：最小对象

```json
{ "ok": false, "error": { "message": "..." } }
```

---

## 退出码（exit code）

对齐 `docs/error-standard.md`：

- `0`：成功
- `2`：用法错误/参数解析失败（含本地预校验失败）
- `10`：未认证（401）
- `11`：无权限（403）
- `12`：未找到（404/410）
- `13`：冲突（409）
- `14`：校验失败（422）
- `15`：限流/依赖不可用/超时可重试（429/503/timeout）
- `1`：其他错误

---

## 常见问题（FAQ）

### 为什么 `publish` 直接报本地校验失败？

`publish` 会在本地解析 Markdown 顶部 FrontMatter（`--- ... ---`）并检查必填字段：

- `group` / `name` / `version` / `description`

以及 `requires`（若存在）必须是合法坐标数组。

### 我不想落盘 token

使用一次性 token 或环境变量：

```bash
skillctl publish ./SKILL.md --base-url ... --repo ... --token "$TOKEN"
SKILLCTL_TOKEN="$TOKEN" skillctl show com.example/a:1.0.0 --repo hosted
```

