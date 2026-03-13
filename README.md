# oh-my-skills

> A minimal **Skill artifact management platform** (similar to Nexus for JARs), plus a C++ CLI client `skillctl` for publishing, searching and downloading skills.

[简体中文说明](./README-zh_CN.md)

---

## Overview

`oh-my-skills` is an internal/experimental project for managing reusable **Skills** defined as Markdown files with FrontMatter metadata.

The platform focuses on:

- **Artifact management** for skills (`group/name:version`)
- **Stable APIs** for publishing/searching/downloading skills
- **A cross‑platform CLI** (`skillctl`) written in C++20
- **Unified error handling** across backend, web and CLI

The runtime for executing skills is **out of scope** – this repo only handles storage, distribution, and governance.

---

## Repository Structure

Key directories and files:

- `apps/skillctl/` – CLI entrypoint (`main.cpp`)
- `src/` – core libraries used by the CLI
  - `api/` – HTTP client for the Skill platform APIs
  - `cli/` – runtime context, config merging, token resolution
  - `config/` – config file schema and helpers
  - `coord/` – coordinate parsing (`group/name:version`)
  - `error/` – unified error model and mapping to exit codes
  - `frontmatter/` – FrontMatter parsing from Markdown
  - `output/` – human & JSON output helpers
  - `security/` – token storage (e.g. DPAPI on Windows)
  - `util/` – filesystem, strings, HTTP helpers
- `tests/` – unit and integration tests
- `docs/`
  - `skill-管理平台-需求文档.md` – main product requirements (Chinese)
  - `cli-skillctl-规范.md` – CLI spec (command tree, parameters, exit codes)
  - `cli-skillctl-技术方案.md` – technical design for the CLI
  - `skillctl-安装与使用.md` – install & usage guide for CLI
  - `error-standard.md` – unified error envelope and exit code mapping

---

## `skillctl` CLI at a Glance

`skillctl` is a C++20 CLI that wraps the HTTP APIs exposed by the skill management platform.

Some example commands (actual details in `docs/cli-skillctl-规范.md`):

```bash
# Show CLI version
skillctl version

# Configure a base (named backend endpoint)
skillctl base dev http://127.0.0.1:8082

# Login & store token for a base
skillctl login dev

# Publish a skill (Markdown with FrontMatter)
skillctl publish dev ./SKILL.md

# Download content, auto-saved to ~/.skillctl/skills/{group}/{name}/{version}/SKILL.md
skillctl get dev com.example/my-skill:1.0.0

# Show metadata only
skillctl show dev com.example/my-skill:1.0.0

# Search skills
skillctl search dev "rag"
```

For the full CLI contract (parameters, exit codes, JSON output), see:

- `docs/cli-skillctl-规范.md`
- `docs/skillctl-安装与使用.md`

---

## Building from Source

The CLI is implemented in **C++20** and uses **CMake + vcpkg** for dependency management.

### Prerequisites

- CMake **3.24+**
- C++20 compiler  
  - Windows: MSVC 2022  
  - Linux/macOS: clang or gcc
- vcpkg (installed via the official docs)

### Build (Windows PowerShell example)

```powershell
# 1) Set VCPKG_ROOT (example)
$env:VCPKG_ROOT="C:\dev\vcpkg"

# 2) Configure + build
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_FEATURE_FLAGS=manifests

cmake --build build --config Release

# 3) Run CLI
.\build\Release\skillctl.exe version
```

On Linux/macOS, use the same `cmake -S . -B build` invocation with a suitable toolchain, then:

```bash
cmake --build build --config Release
./build/skillctl version
```

---

## Running Tests

There are basic tests for coordinates, error handling, config merging, and frontmatter parsing:

```bash
ctest --test-dir build
# or platform‑specific test runner if configured
```

---

## Documentation

- **Requirements**: `docs/skill-管理平台-需求文档.md`
- **CLI spec**: `docs/cli-skillctl-规范.md`
- **CLI design**: `docs/cli-skillctl-技术方案.md`
- **CLI install & usage**: `docs/skillctl-安装与使用.md`
- **Error envelope**: `docs/error-standard.md`

If you only want to *use* the CLI, start with:

- `docs/skillctl-安装与使用.md`

---

## Contributing

This repo is currently experimental/internal. If you want to:

- add new CLI commands (e.g. deps, deprecate),
- extend the error model,
- or refine the UX for `skillctl`,

please align first with:

- `docs/skill-管理平台-需求文档.md`
- `docs/cli-skillctl-规范.md`
- `docs/error-standard.md`

Then open a PR or proposal.

---

## License

This project is currently **internal only**. A public license may be added later.

## oh-my-skills

当前仓库包含 `skillctl`（Skill 管理平台 CLI）v1 的规格说明与实现。

## skillctl（CLI）

实现规格见：`docs/specs/skillctl/`（按模块拆分、按顺序实现）。

使用文档见：`docs/skillctl-安装与使用.md`

### 构建（vcpkg + CMake）

本项目采用 vcpkg manifest 管理依赖。

前置要求：

- CMake 3.24+
- C++20 编译器（Windows: MSVC 2022；Linux/macOS: clang/gcc）
- vcpkg（建议使用官方文档方式安装）

构建示例（Windows PowerShell）：

```powershell
# 1) 设置 VCPKG_ROOT（示例）
$env:VCPKG_ROOT="C:\dev\vcpkg"

# 2) 配置 + 构建（vcpkg toolchain）
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_FEATURE_FLAGS=manifests

cmake --build build --config Release

# 3) 运行
.\build\Release\skillctl.exe version
```

### 测试

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

