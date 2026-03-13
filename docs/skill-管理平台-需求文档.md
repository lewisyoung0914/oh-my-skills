# Skill 管理平台需求文档（类似 Nexus 的制品仓库）

## 1. 背景与目标

### 1.1 背景

组织内部/社区会沉淀大量可复用的 Skill（以 Markdown + FrontMatter 形式定义），需要像 Nexus/Artifactory 管理 Java 包一样，提供统一的**上传、检索、版本管理、依赖解析、权限与审计**能力，支撑自动化拉取与运行。

### 1.2 目标

- 建立一个“Skill 仓库管理平台”（下称平台），实现 Skill 的**制品化管理**：可发布、可搜索、可回滚、可追溯、可依赖。
- 对 `group + name + version` 做**唯一性约束**（同一坐标只能存在一个制品）。
- 提供 **Web 管理端** + **CLI/HTTP API**，便于人和系统集成。

### 1.3 非目标（后续迭代）

- 不负责 Skill 的执行沙箱/运行时编排（平台只管“存储、分发、治理”）。
- 不强制限定 Skill 的具体执行引擎（Claude/其他 agent 运行器由外部系统实现）。

## 2. 术语与坐标体系

- **Skill**：一个带 FrontMatter 的 Markdown 文档。
- **坐标（GAV-like）**：`group`（如 `com.example`）+ `name`（如 `my-skill-name`）+ `version`（如 `1.2.3`）。
- **唯一性**：平台必须保证 **同一 `(group, name, version)` 只能发布一次**。
- **Artifact（制品）**：Skill 的“制品对象”，包含原始内容、解析出的元数据、校验信息与附件。
- **Repository（仓库）**：逻辑隔离空间，类似 Nexus 的 hosted/proxy/group。
  - **Hosted 仓库**：本平台存储与发布。
  - **Proxy 仓库（可选）**：代理外部 Skill 源。
  - **Group 仓库（可选）**：聚合多个 hosted/proxy，统一对外。

## 3. Skill 内容规范（输入格式）

平台需要支持并解析以下内容结构（示例）：

```markdown
---
name: my-skill-name
group: com.example
version: skill version
description: A clear description of what this skill does and when to use it.
label: "label type"
metadata:
    author: skill author
requires: ["com.example/dependency-skill:version"]
---

# What is My Skill Name

Detailed description of the skill's purpose and capabilities.

## When to Use This Skill

- Use case 1
- Use case 2
- Use case 3

## Instructions

[Detailed instructions for Claude on how to execute this skill]

## Examples

[Real-world examples showing the skill in action]
```

### 3.1 需提取并存储的字段

- `name`（必填）
- `group`（必填）
- `version`（必填）
- `description`（必填）
- `label`（可选，用于分类/展示）
- `metadata.author`（可选）
- `requires`（可选，依赖列表，形如 `com.example/dependency-skill:version`）

### 3.2 解析与校验规则

- FrontMatter 必须存在且为 YAML。
- `group/name/version` 必须通过格式校验（建议）：
  - `group`：建议仅允许 `[a-zA-Z0-9_.-]`（类似 Java 包风格）。
  - `name`：建议 `[a-zA-Z0-9_.-]`，不允许空格。
  - `version`：建议兼容 SemVer（`x.y.z`）并允许后缀（`-rc1`、`+build`）；如允许任意字符串版本，需定义排序策略。
- **版本类型判定（强制）**：
  - **Release 版本**：`version` 不包含 `SNAPSHOT`（建议大小写不敏感匹配，如 `/snapshot/i`）。
  - **Snapshot 版本**：`version` 包含 `SNAPSHOT`（如 `1.2.3-SNAPSHOT`）。
- **唯一性约束（强制）**：
  - **GAV 全局唯一**：数据库层面建立唯一索引，保证平台内 `(group, name, version)` 唯一（不区分仓库）。
  - **Release 不可覆盖**：Release 版本若同 GAV 已存在，发布必须失败并返回冲突（409）。
  - **Snapshot 可覆盖**：Snapshot 版本允许重复发布（覆盖“最新指针”），但底层内容应保留多份副本（见第 8 节数据模型与存储口径）。
- `requires` 解析：
  - 支持 `group/name:version`。
  - 校验依赖坐标合法；是否强校验“依赖必须存在”由仓库策略控制。

### 3.3 不可变性（推荐）

- **Release**：已发布的 `(group, name, version)` 内容默认**不可覆盖**（immutable）。若要更新，必须发布新版本。
- **Snapshot**：允许覆盖发布，但平台必须保留历史内容副本以便审计与排障；对外读取/下载默认永远返回“最新副本”。

## 4. 用户角色与权限模型

### 4.1 角色（默认）

- **系统管理员**：全局配置、仓库管理、用户/组织管理、审计查看。
- **仓库管理员**：管理某仓库权限、策略、配额、清理规则。
- **发布者（Publisher）**：向指定仓库发布/删除（如允许）/标记。
- **消费者（Reader）**：检索与下载。
- **审计员（Auditor）**：只读审计日志与报表。

### 4.2 权限粒度（RBAC + Scope）

- Scope：全局 / 仓库 / 组（Group/Namespace）
- 权限项（建议）：
  - `repo.read`、`repo.publish`、`repo.delete`、`repo.admin`
  - `skill.deprecate`（标记废弃）、`skill.promote`（提升渠道/阶段）
  - `security.view_audit`、`security.manage_tokens`

### 4.3 认证方式

- Web：账号密码 + SSO（OIDC/SAML，二期可选）
- API/CLI：Access Token（可设置过期、scope、只读/可发布）

## 5. 总体业务流程

### 5.1 发布（Publish）

1. 发布者选择目标仓库。
2. 上传 Skill 文档（如 `SKILL.md` 或任意文件名）。
3. 平台解析 FrontMatter → 校验 → 存储原文 → 生成制品记录。
4. 坐标已存在时的处理（必须按版本类型区分）：
   - **Release**：若同 GAV 已存在 → 返回冲突（HTTP 409），阻止覆盖。
   - **Snapshot**：允许重复发布 → 更新“最新副本指针”，并保留一个新的内容副本（路径带时间戳）。
5. 生成可下载 URL、校验和、依赖关系索引、审计日志（Snapshot 覆盖发布也必须记录审计）。

### 5.2 检索与下载（Consume）

- Web 搜索：按 `group/name/version/label/author/description` 搜索。
- CLI/API：
  - 按 GAV 精确拉取。
  - 对于 **Snapshot** 版本：平台对外读取/下载默认**永远返回最新副本**。
  - 可选增强：提供“拉取指定历史副本”的能力（如按 build 时间戳或 blobId）。

### 5.3 版本管理

- 查看版本列表、发布信息（可选：release notes）。
- 状态标记：`latest`、`stable`、`deprecated`（废弃原因与替代坐标）。
- 清理：按策略自动清理旧版本（保留数/保留时间/保留被依赖版本）。

### 5.4 依赖解析

- 以某个 Skill 为入口，解析 `requires` 形成依赖树。
- 检测循环依赖、缺失依赖、版本冲突（规则可配置）。
- 提供“锁定文件/解析结果导出”（可选）。

## 6. 功能需求（Web 管理端）

### 6.1 首页与概览

- 展示：仓库数量、Skill 总量、近 7 天发布量、失败发布、热门下载。
- 快捷入口：发布、搜索、仓库管理、Token 管理。

### 6.2 Skill 搜索与列表

- 搜索条件：`group`、`name`、`version`、`label`、`author`、关键字（标题/描述/正文全文）。
- 筛选：仓库、状态（正常/废弃/删除）、时间范围。
- 排序：最近发布、下载量、版本号（按语义版本）。
- 列表项：坐标、描述、标签、最新版本、下载量、更新时间、状态。

### 6.3 Skill 详情页（版本维度）

- 基本信息：`group/name/version`、描述、作者、标签、发布时间、发布者。
- 内容预览：渲染 Markdown（含目录）+ 原文查看。
- 依赖信息：requires 列表 + 依赖树 + 缺失/循环提示。
- 下载入口：下载原始文件、复制 API/CLI 拉取命令。
- 操作：标记废弃/取消废弃、设置推荐替代、（可选）删除/软删除。
- 审计：该版本的发布/下载/操作记录。

### 6.4 发布页面

- 上传方式：拖拽/选择文件、粘贴文本、API Token 发布指引。
- 校验反馈：FrontMatter 校验、唯一性冲突提示、依赖校验结果。
- 发布策略提示：是否允许 SNAPSHOT/预发布版本、是否强校验依赖存在。

### 6.5 仓库管理

- 创建/编辑仓库（hosted/proxy/group）。
- 仓库策略：
  - 是否允许删除。
  - 是否 immutable（默认是）。
  - 版本策略：允许 SNAPSHOT、预发布。
  - 配额：存储上限、单文件大小上限。
  - 清理策略：保留最新 N 个/保留最近 T 天/保留被依赖版本。
- 访问控制：仓库级读/写权限配置（用户/组）。

### 6.6 用户、组织与 Token 管理

- 用户/组管理（可选：对接 LDAP/OIDC）。
- Token：创建、吊销、设置过期时间、设置 scope、查看最近使用时间/IP。

### 6.7 审计与报表

- 审计日志：发布、删除、废弃标记、权限变更、Token 操作、下载事件（可配置采样）。
- 报表：发布排行、下载排行、失败发布原因分布、缺失依赖 Top。

## 7. 功能需求（API/CLI）

### 7.1 API 设计原则

- RESTful + JSON（下载原文用文本流）。
- 全部 API 支持 Token 鉴权：`Authorization: Bearer <token>`。
- 幂等性：发布接口支持 `Idempotency-Key`（可选）。
- 错误码统一：400/401/403/404/409/422/500。

### 7.2 关键 API（建议）

- 发布
  - `POST /api/v1/repos/{repo}/skills`：上传内容（multipart 或 body 纯文本）。
  - 若 `(group,name,version)` 已存在：
    - **Release**：**409 Conflict**。
    - **Snapshot**：允许覆盖发布（推荐：首次发布返回 201，覆盖发布返回 200）。
- 精确获取
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}`：获取元数据。
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/content`：下载原文（**永远返回最新副本**；Release 只有一个副本）。
- 搜索
  - `GET /api/v1/skills/search?q=...&group=...&name=...&label=...`
- 版本列表
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/versions`
- 解析依赖
  - `GET /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deps?transitive=true`
- 标记废弃
  - `POST /api/v1/repos/{repo}/skills/{group}/{name}/{version}/deprecate`

### 7.3 CLI（建议能力）

- `skillctl login` / `skillctl token set`
- `skillctl publish ./SKILL.md --repo hosted`
- `skillctl get com.example my-skill-name 1.0.0 --repo hosted`
- `skillctl search "rag" --label prompt`
- `skillctl deps com.example/my-skill-name:1.0.0 --tree`

## 8. 数据模型（概念设计）

### 8.1 核心对象

- `repositories`
- `skills`
  - `id`
  - `group`、`name`、`version`
  - `description`、`label`、`author`
  - `is_snapshot`（是否为 snapshot 版本）
  - `current_blob_id`（指向最新内容副本；Release 也可用但始终不变）
  - `status`（active/deprecated/deleted）
  - `created_at`、`created_by`
- `skill_blobs`（内容副本表，支持 Snapshot 多副本；Release 仅 1 条）
  - `id`
  - `skill_id`
  - `content_location`（对象存储路径；Snapshot 路径应带时间戳/构建号以保证多副本共存）
  - `content_sha256`
  - `size_bytes`
  - `created_at`、`created_by`
  - `request_id`（便于审计/排障链路追踪）
- `skill_dependencies`
  - `skill_id`
  - `required_group`、`required_name`、`required_version`
  - `required_skill_id`（可为空：缺失依赖）
- `downloads`（可选，量大可做聚合表）
- `audit_logs`
- `tokens`、`users`、`groups`、`permissions`

### 8.2 关键约束

- **唯一索引（GAV 全局唯一）**：`(group, name, version)`。
- `content_sha256`：用于完整性校验与去重（可选）。

## 9. 非功能需求

### 9.1 性能与容量（可按实际调整）

- 搜索：常用查询 < 300ms（P95）。
- 下载：支持断点续传（可选）。
- 存储：对象存储（S3/MinIO/Azure Blob）优先，DB 只存元数据。
- 索引：全文检索建议 Elasticsearch/OpenSearch；轻量可用 PostgreSQL/SQLite FTS。

### 9.2 安全

- Token 最小权限、可过期、可撤销。
- 审计日志不可篡改（至少追加写）。
- 防止恶意内容：上传大小限制、内容扫描（可选）。
- 依赖混淆防护：可配置“只允许特定 group 前缀发布/引用”。

### 9.3 可用性与一致性

- 发布强一致：成功返回即保证可读。
- 防重复发布：唯一索引 + 事务。
- 灾备：元数据备份 + 对象存储版本化（可选）。

### 9.4 可运维性

- 指标：QPS、错误率、发布失败原因、存储用量、索引延迟。
- 日志：结构化日志（含 request_id）。
- 告警：存储逼近配额、索引积压、500 激增。

## 10. 关键业务规则清单（口径）

- **R1（唯一性）**：GAV（`group,name,version`）在平台内全局唯一。
- **R1.1（Release 冲突）**：任何发布请求若同 GAV 的 **Release** 版本已存在，必须失败并返回 409。
- **R1.2（Snapshot 覆盖）**：同 GAV 的 **Snapshot** 版本允许重复发布；数据库中仅“最新指针”覆盖更新，但底层内容必须保留多份副本（路径带时间戳/构建号）。
- **R2（不可覆盖）**：默认不允许覆盖已发布 **Release** 版本；如需允许，仅仓库管理员可开启“允许 redeploy”，且必须审计记录。
- **R2.1（读取口径）**：对外读取/下载（`GET .../content`）永远返回“最新副本”（Release 唯一副本；Snapshot 返回当前最新副本）。
- **R3（依赖解析）**：`requires` 做坐标解析；是否强校验依赖存在由仓库策略控制。
- **R4（删除）**：默认仅允许“软删除/隐藏”，保留审计与可追溯；硬删除需系统管理员。
- **R5（废弃）**：废弃不影响下载，但在搜索与详情强提示，并允许填写替代坐标。

## 11. 页面清单（信息架构）

- 登录/SSO 回调
- 首页概览
- Skill 搜索列表
- Skill 详情（版本维度）
- 组件页（`group/name` 聚合页，展示版本列表）
- 发布页
- 仓库列表/仓库详情/权限策略
- 用户/组/权限
- Token 管理
- 审计日志
- 系统设置（存储、索引、代理源、清理任务）

## 12. 里程碑建议（MVP → 增强）

### 12.1 MVP（最小可用）

- Hosted 仓库
- 发布（含解析校验 + 唯一性约束）
- 搜索（字段匹配即可）
- 下载 content
- 基础 RBAC + Token
- 审计（发布/删除/权限变更）

### 12.2 增强

- 全文检索、依赖树、循环检测
- 清理策略与定时任务
- Proxy/Group 仓库
- 下载统计与报表
- SSO/LDAP

