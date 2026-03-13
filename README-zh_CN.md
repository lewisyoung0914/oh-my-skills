# oh-my-skills

> 一个类似 Nexus 的 **Skill 制品管理平台**，以及配套的 C++ CLI 工具 `skillctl`，用于发布、搜索和下载 Skill。

[English README](./README.md)

---

## 项目概览

`oh-my-skills` 用来管理以 **Markdown + FrontMatter** 定义的可复用 Skill。

平台主要关注：

- 按坐标 `group/name:version` 管理 Skill 制品
- 提供稳定的 **HTTP API** 供 Web / CLI / 其他系统调用
- 提供跨平台的 C++ CLI：`skillctl`
- 统一前后端与 CLI 的 **错误结构与退出码**

Skill 的**运行时/执行引擎不在本仓库内**，这里只负责“存储、分发、治理”。

---

## 仓库结构

主要目录与文件：

- `apps/skillctl/` – CLI 入口（`main.cpp`）
- `src/` – CLI 所使用的核心库
  - `api/` – 调用 Skill 平台 HTTP API 的客户端
  - `cli/` – 运行时上下文、配置合并、token 解析
  - `config/` – 配置文件结构与工具
  - `coord/` – 坐标解析（`group/name:version`）
  - `error/` – 统一错误模型与退出码映射
  - `frontmatter/` – 从 Markdown 中解析 FrontMatter
  - `output/` – 人类可读与 JSON 输出
  - `security/` – token 本地安全存储（如 Windows DPAPI）
  - `util/` – 文件、字符串、HTTP 辅助工具
- `tests/` – 单测与集成测试
- `docs/`
  - `skill-管理平台-需求文档.md` – 平台需求文档
  - `cli-skillctl-规范.md` – CLI 规范（命令树、参数、退出码）
  - `cli-skillctl-技术方案.md` – CLI 技术方案
  - `skillctl-安装与使用.md` – CLI 安装与使用说明
  - `error-standard.md` – 统一错误结构与退出码规范

---

## `skillctl` CLI 概览

`skillctl` 是一个使用 C++20 实现的命令行客户端，封装了 Skill 平台的 HTTP API。

示例命令（完整细节以 `docs/cli-skillctl-规范.md` 为准）：

```bash
# 查看 CLI 版本
skillctl version

# 配置一个 base（命名的后端地址）
skillctl base dev http://127.0.0.1:8082

# 登录并保存 dev 对应的 token
skillctl login dev

# 发布一个 Skill（带 FrontMatter 的 Markdown）
skillctl publish dev ./SKILL.md

# 下载 content，默认保存到 ~/.skillctl/skills/{group}/{name}/{version}/SKILL.md
skillctl get dev com.example/my-skill:1.0.0

# 仅查看元数据
skillctl show dev com.example/my-skill:1.0.0

# 搜索 Skill
skillctl search dev "rag"
```

更详细的用法、退出码与 `--json` 行为，请参考：

- `docs/cli-skillctl-规范.md`
- `docs/skillctl-安装与使用.md`

---

## 从源码构建

CLI 使用 **C++20 + CMake + vcpkg**。

### 前置条件

- CMake **3.24+**
- C++20 编译器  
  - Windows：MSVC 2022  
  - Linux/macOS：clang 或 gcc
- vcpkg（建议按官方文档安装）

### 构建示例（Windows PowerShell）

```powershell
# 1) 设置 VCPKG_ROOT（示例）
$env:VCPKG_ROOT="C:\dev\vcpkg"

# 2) 配置 + 构建
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_FEATURE_FLAGS=manifests

cmake --build build --config Release

# 3) 运行 CLI
.\build\Release\skillctl.exe version
```

在 Linux/macOS 上，命令类似：

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_FEATURE_FLAGS=manifests

cmake --build build --config Release
./build/skillctl version
```

---

## 运行测试

仓库包含坐标解析、错误处理、配置合并、FrontMatter 解析等测试用例：

```bash
ctest --test-dir build
# 或使用 CI 中配置的测试命令
```

---

## 文档索引

- **平台需求**：`docs/skill-管理平台-需求文档.md`
- **CLI 规范**：`docs/cli-skillctl-规范.md`
- **CLI 技术方案**：`docs/cli-skillctl-技术方案.md`
- **CLI 安装使用**：`docs/skillctl-安装与使用.md`
- **错误规范**：`docs/error-standard.md`

如果你只是要**使用 CLI**，建议直接从：

- `docs/skillctl-安装与使用.md`

开始阅读。

---

## 参与贡献

当前项目仍处于内部/实验阶段。如需：

- 增加新的 CLI 命令（例如依赖树、废弃标记）
- 扩展错误模型
- 改进 `skillctl` 的交互体验

请先对齐以下文档中的约束与假设：

- `docs/skill-管理平台-需求文档.md`
- `docs/cli-skillctl-规范.md`
- `docs/error-standard.md`

再以 PR 或设计文档的形式提交。

---

## 许可证

目前该项目仅用于内部实验，尚未对外发布正式开源许可证。

