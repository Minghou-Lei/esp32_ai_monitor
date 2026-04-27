# Structure

日期：2026-04-27

## 当前目录结构

### 根目录

- `CMakeLists.txt`：根构建入口
- `README.md`：当前几乎为空
- `sdkconfig`：当前工程配置
- `sdkconfig.old`：历史配置备份
- `.gitignore`：忽略规则
- `AGENTS.md`：项目级协作说明

### 应用目录

- `main/CMakeLists.txt`：注册 `main.c`
- `main/main.c`：当前唯一源码入口

### 文档目录

- `docs/ai-agent-monitor-proposal.md`：AI 监控终端方向方案

### 开发环境目录

- `.devcontainer/`：容器开发环境
- `.vscode/`：编辑器与调试配置

### 本地产物目录

- `build/`：本地构建输出

## 当前结构特征

- 结构极简
- 业务代码尚未拆分
- 还没有 `components/`
- 还没有 `include/`
- 还没有测试目录
- 还没有资源目录，例如字体、图片、UI 资源

## 对后续结构演进的建议

### 优先新增

- `components/board_support/`
- `components/display_service/`
- `components/touch_service/`
- `components/ui/`
- `components/network_service/`
- `components/backend_client/`
- `components/agent_state/`

### 按需新增

- `assets/`
- `partitions.csv`
- `managed_components/`（由组件管理器生成）
- `test/` 或专用验证目录

## 命名与边界建议

- 目录名采用小写加下划线风格
- 一个组件只负责一类能力
- 板级初始化与业务 UI 分离
- 网络链路与页面逻辑分离
- 协议解析与状态存储分离

## 当前结构的最大问题

如果下一个开发步骤仍然只改 `main/main.c`，这个仓库会在很早阶段失去可维护性。

因此：

- 进入显示、触摸、网络开发前，应先完成目录级拆分方案。
