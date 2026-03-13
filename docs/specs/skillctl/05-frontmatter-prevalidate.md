# `skillctl` FrontMatter 解析与本地预校验规格（frontmatter）

## 1. 模块职责

FrontMatter 模块用于发布前“早失败”，减少无效上传：

- 从 Markdown 顶部提取 FrontMatter YAML 区块
- 解析 YAML 并校验必填字段与基本格式
- 输出结构化结果（供 `publish` 命令打印坐标、构造请求等）

> 最终校验以服务端为准；本地预校验不应引入与服务端相冲突的强约束（除非明确在规范中固定）。

## 2. 输入与输出

输入：

- 一个 markdown 文件内容（文本）

输出（概念）：

- `frontmatterRaw: string`
- `docBody: string`（可选；CLI v1 不需要解析正文）
- 解析出的字段：
  - `group: string`
  - `name: string`
  - `version: string`
  - `description: string`
  - `label?: string`
  - `metadata.author?: string`
  - `requires?: string[]`

## 3. 提取规则

- FrontMatter 位于文档开头：
  - 第一行必须是 `---`
  - 直到下一行单独 `---` 结束
- 若不存在 FrontMatter：视为本地预校验失败（exit=2），提示必须包含 FrontMatter
- 若 `---` 未闭合：视为本地预校验失败（exit=2）

## 4. YAML 解析与字段校验（v1）

### 4.1 YAML 合法性

- YAML 解析失败：本地预校验失败（exit=2），提示 “invalid YAML”

### 4.2 必填字段

依据技术方案与需求文档的必填字段：

- `group`
- `name`
- `version`
- `description`

任一缺失/为空：本地预校验失败（exit=2），提示具体字段。

### 4.3 基本格式

- `group/name` 的字符集校验复用 `coord` 模块规则（见 `04-coord.md`）
- `requires`（若存在）：
  - 必须为字符串数组
  - 每个元素必须是合法坐标（复用 `coord` 的形式 A 解析）
  - v1 不强制校验依赖是否存在（除非服务端策略要求；此处仅做格式校验）

### 4.4 目录发布（可选）

`publish <dir>` 是否支持需在实现前定：

- v1 建议先只支持单文件（更可控）
- 若支持目录：
  - 递归查找 `SKILL.md` 或 `*.md`
  - 每个文件独立做本地预校验
  - 失败策略：任一失败则整体失败（exit=2），并打印失败列表

## 5. 与 `publish` 的交互约束

- `--dry-run`：
  - 仅做提取 + 解析 + 校验
  - 不发起网络请求
  - 成功时可输出解析出的坐标与摘要信息（stdout），错误输出到 stderr

## 6. 错误与退出码

- 本地预校验失败统一 exit=2（用法/校验失败）
- `--json` 下输出最小错误对象（见 `00-overview.md`），不引入额外 `code`

## 7. 验收标准

- 覆盖：无 FrontMatter、未闭合、YAML 语法错误、字段缺失、requires 类型错误、requires 坐标不合法。
- `--dry-run` 在任意情况下不产生网络请求。
