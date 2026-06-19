# PureCloak

> 基于 Chromium 的双模式工作空间级身份隔离浏览器 — 一个浏览器，双面灵魂。

<p align="center">
  <img src="chromium_src/chrome/app/theme/chromium/product_logo_128.png" alt="PureCloak" width="128">
</p>

---

## 概述

**PureCloak** 是一款基于 Chromium 深度二次开发的专业浏览器，核心能力在于**工作空间级身份隔离**。

不同于传统的多配置文件方案，PureCloak 将每个工作空间（Workspace）作为一个完全独立的浏览器实例来运行——拥有独立的 `--user-data-dir`、独立代理、独立指纹参数、独立 Chromium 子进程进程，通过 CDP 协议与主进程通信。这使得不同工作空间之间的 Cookie、LocalStorage、缓存等所有存储实现**物理级隔离**，从根本上防止账号关联。

PureCloak 支持两种工作空间模式：

| 类型 | 用途 | 隔离级别 |
|------|------|----------|
| **普通工作空间 (Normal)** | 日常纯净浏览 | 独立子进程 + 用户数据目录，与指纹空间完全隔离 |
| **指纹工作空间 (Fingerprint)** | 多账号防关联运营 | 独立子进程 + 独立代理 + WebRTC 防护 + 指纹伪装 |

在指纹工作空间中，每个工作空间可绑定独有的代理配置、User-Agent、屏幕分辨率、时区、语言环境和 WebRTC 策略。通过 CDP 注入，可以在页面加载前完成指纹修改，确保指纹信息在浏览器层面而非仅 JS 层面被篡改。

---

## 核心特性

- **🔒 进程级隔离** — 每个工作空间运行在独立 Chromium 子进程中，拥有独立 `--user-data-dir`，存储完全物理隔离
- **🛡️ 代理隧道** — 支持 SOCKS5/HTTP/HTTPS 代理，通过 `--proxy-server` 启动参数注入，支持账号认证
- **🧬 指纹伪装** — WebRTC 强制策略 (`--force-webrtc-ip-handling-policy=disable_non_proxied_udp`)、Canvas/WebGL/AudioContext 混淆、User-Agent 自定义、屏幕分辨率、时区、语言环境
- **🔌 CDP 远程控制** — 每个工作空间暴露独立 CDP 端口，兼容 Playwright/Selenium/Puppeteer 等自动化工具
- **🖥️ 图形化管理界面** — 内置 `purecloak://purecloak/` WebUI，支持工作空间的创建、编辑、启动、停止、删除
- **🔄 看护进程 (Watchdog)** — 子进程退出自动检测，状态实时同步到 WebUI
- **🌐 中文本地化** — 完整的中文界面支持
- **⚡ 极低性能损耗** — 仅在需要时启动子进程，空闲工作空间零资源占用

---

## 快速开始

### 环境要求

- Linux x86_64（目前仅支持 Linux）
- Python 3.10+
- Git
- 50GB+ 磁盘空间（Chromium 源码 + 构建缓存）
- 16GB+ 内存（推荐 32GB）

### 克隆与构建

```bash
# 1. 克隆仓库
git clone https://github.com/molandtoxx/purecloak.git
cd purecloak

# 2. 安装 Chromium 构建依赖
./chromium_src/build/install-build-deps.sh

# 3. 同步 Chromium 源码与依赖（可能需要数小时）
gclient sync -D

# 4. 配置 GN 构建参数
gn gen out/purecloak --args='
  is_debug=false
  is_purecloak=true
  is_component_build=false
  symbol_level=0
  blink_symbol_level=0
  optimize_for_size=true
  enable_nacl=false
  use_sysroot=true
'

# 5. 编译（首次约 2-4 小时）
autoninja -C out/purecloak chrome

# 6. 运行
./out/purecloak/chrome

# 或者使用一键构建脚本
./chromium_src/purecloak/branding/build.sh
```

### 配置 `is_purecloak=true`

编译时必须传入 `is_purecloak=true`，此参数通过 `purecloak.gni` 控制 PureCloak 自定义代码的编译入口。未设置此参数时，PureCloak 代码完全不会进入编译。

---

## 架构概览

```
┌────────────────────────────────────────────────────────────�-b-┐
│                      PureCloak 主进程                          │
│                                                                │
│  ┌──────────────┐  ┌────────────────┐  ┌──────────────────┐   │
│  │  Workspace    │  │  PureCloak     │  │  Anti-Detection  │   │
│  │  Manager      │  │  WebUI         │  │  Profile Applier │   │
│  │  (CRUD/存储)  │  │  (管理界面)    │  │  (指纹注入)      │   │
│  └──────┬───────┘  └────────────────┘  └────────┬─────────┘   │
│         │                                        │             │
│         │    Workspace Launcher                  │             │
│         │    ┌──────────────────────┐            │             │
│         │    │  • 子进程 spawn      │            │             │
│         │    │  • 代理注入          │──── CDP ──┼─────────────│
│         │    │  • 看护进程 (Watchdog)│            │             │
│         │    └──────────┬───────────┘            │             │
│         └───────────────┼────────────────────────┘             │
└─────────────────────────┼──────────────────────────────────────┘
                          │
              ┌───────────┴───────────┐
              │    Chromium 子进程     │
              │   (独立的浏览器实例)    │
              │   • --user-data-dir    │
              │   • --proxy-server     │
              │   • --remote-debugging │
              │   • WebRTC 策略        │
              └───────────────────────┘
```

### 模块说明

| 模块 | 路径 | 职责 |
|------|------|------|
| **Workspace Store** | `browser/profiles/` | 工作空间数据的 JSON 持久化（CRUD） |
| **Workspace Launcher** | `browser/workspace_launcher*` | 子进程管理，包含命令行参数组装、spawn、Watchdog |
| **Workspace Profile Applier** | `browser/workspace_profile_applier*` | 指纹注入（CDP Page.addInitScript + 配置文件覆盖） |
| **CDP WebSocket Client** | `content/cdp_websocket_client*` | CDP 通信客户端 |
| **Anti-Detection Engine** | `content/anti_detection/` | WebRTC 策略、Canvas/WebGL/AudioContext 混淆引擎 |
| **Storage Partition** | `content/storage_partition_controller*` | 存储分区隔离控制器 |
| **PureCloak WebUI** | `browser/ui/webui/` | `purecloak://purecloak/` 管理界面（C++ handler + HTML/JS） |
| **UI 组件** | `browser/ui/views/` | 工具栏按钮、侧面板、工作空间管理对话框 |

---

## 项目结构

```
chromium_src/
├── purecloak/                          # PureCloak 自定义代码
│   ├── BUILD.gn                        # 纯 Cloak 模块入口
│   ├── purecloak.gni                   # is_purecloak 编译开关
│   ├── purecloak_test_main.cc          # 测试入口
│   ├── branding/                       # 构建脚本
│   ├── browser/                        # 浏览器层
│   │   ├── profiles/                   # Workspace 模型与持久化
│   │   ├── ui/views/                   # 工具栏按钮、侧面板、对话框
│   │   ├── ui/webui/                   # WebUI Handler + UI
│   │   ├── purecloak_webui_registrar*  # WebUI 注册
│   │   ├── workspace_launcher*         # 子进程管理
│   │   ├── workspace_profile_applier*  # 指纹注入
│   │   └── workspace_launcher_watchdog*# 子进程看护
│   ├── content/                        # 内容层
│   │   ├── anti_detection/             # 反检测引擎
│   │   ├── cdp_websocket_client/       # CDP 通信
│   │   ├── profile_cdp_injector*       # CDP 注入
│   │   └── storage_partition_controller* # 存储隔离
│   ├── common/                         # 公共工具
│   ├── resources/                      # WebUI HTML/i18n
│   └── tests/                          # C++ 单元测试
├── ash/                                # ChromeOS API 存根头文件
├── chromeos/                           # ChromeOS 命名空间存根
└── chrome/                             # 被修改的 Chromium 源文件
    ├── app/chromium_strings.grd        # 品牌字符串
    ├── app/theme/chromium/             # 品牌图标
    └── browser/ui/                     # Profile 菜单等 UI 修改点

tests/                                  # Python + Playwright E2E 测试
docs/                                   # 设计文档 & 规范
PureCloak-PRD.md                        # 产品需求文档
```

### 侵入点说明

PureCloak 对于 Chromium 源码树的侵入非常克制——仅修改了以下文件：

- `chrome/app/chromium_strings.grd` — 替换产品名称为 PureCloak、"profiles"→"workspaces"
- `chrome/app/theme/chromium/*` — 替换品牌图标为 Lucide shield 风格
- `chrome/browser/ui/browser_command_controller.cc` — 添加 `purecloak://purecloak/` 导航入口
- `chrome/browser/ui/views/profiles/profile_menu_view.cc` — 替换 "profiles" 文字为 "workspaces"

其余所有功能通过 `chrome/browser/purecloak/` 独立模块以标准的 GN 依赖注入方式实现，不破坏 Chromium 原始代码结构。

---

## 开发指南

### 构建选项

```bash
# Debug 构建（适合开发）
gn gen out/purecloak_debug --args='is_debug=true is_purecloak=true'

# Release 构建
gn gen out/purecloak --args='is_debug=false is_purecloak=true symbol_level=0'

# 带测试的构建
gn gen out/purecloak --args='is_debug=false is_purecloak=true'
autoninja -C out/purecloak purecloak_unittests
./out/purecloak/purecloak_unittests
```

### 运行测试

```bash
# C++ 单元测试
autoninja -C out/purecloak purecloak_unittests
./out/purecloak/purecloak_unittests

# Python E2E 测试
pip install -r tests/requirements.txt
python tests/visual_e2e_test.py

# Playwright 交互式测试
npx playwright test tests/test_webui_pages.mjs
```

### 调试

每个工作空间启动时自动开启 CDP 端口（默认偏移 9333）：

```javascript
// 使用 CDP 连接到运行中的工作空间
const ws = new WebSocket('ws://127.0.0.1:9333/devtools/browser/...');
// 或通过 Playwright
const browser = await chromium.connectOverCDP('http://127.0.0.1:9333');
```

---

## 技术栈

| 层级 | 技术 |
|------|------|
| 内核 | Chromium (Blink/V8, 151.0.7892.0) |
| 后端 | C++20 (Chromium 风格) |
| 构建 | GN + Ninja |
| WebUI | 原生 HTML + JavaScript (无框架依赖，可直接在 `purecloak://` 运行) |
| CDP | Chrome DevTools Protocol (WebSocket) |
| 测试 | Google Test + Python + Playwright |
| 代理 | SOCKS5/HTTP/HTTPS 隧道 (Chromium 内置 `--proxy-server`) |

---

## 许可

PureCloak 基于 Chromium 源码二次开发，遵循 BSD-style 许可。

- PureCloak 自定义代码：BSD-style（见各文件头部 LICENSE 声明）
- Chromium 源码：BSD-style（见 `chromium_src/LICENSE`）

---

<p align="center">
  <sub>用 🛡️ 守护你的数字身份</sub>
</p>
