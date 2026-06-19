🔴 产品需求文档 (PRD)

项目名称： Purecloak - 双模式工作空间级身份隔离浏览器
文档版本： v6.0 (最终定稿)
技术基座： 基于 Chromium 与 CloakBrowser 深度二次开发
核心目标： 融合“纯净浏览”与“多账号防关联”两种模式，通过工作空间类型一键切换。在指纹模式下，每个标签页归属于一个 Profile，不同 Profile 之间实现完全的存储与身份隔离，同一 Profile 的标签页共享会话，完美对应“一个 Profile 一个账号”的业务需求。

---

1. 产品定位与核心痛点

· 产品定位： 一个浏览器，双面灵魂。既是纯净无杂质的原生级浏览器，又可瞬间切换为每个 Profile 对应独立代理、独立指纹、独立账号的专业多账号管理终端。
· 核心痛点解决：
  1. 工作与日常彻底分离： 无需安装多个浏览器，一个 Purecloak 通过工作空间类型，将日常浏览与多账号运营完全隔离。
  2. 身份模型清晰化： 强制 “一个 Profile = 一个代理 + 一套指纹 + 一个账号”，杜绝混乱的多账号混用，从根源上防止账号关联。
  3. 按需分配隔离粒度： 不同 Profile 的标签页之间存储绝对物理隔离；同一 Profile 下的多个标签页自然共享会话，既安全又高效。
  4. 继承 CloakBrowser 生态： 完全复用 CloakBrowser 成熟的 Profile 管理界面与指纹伪装技术，学习成本为零。
  5. 全生态自动化兼容： 原生去特征化，完美兼容 Playwright、OpenCode、OpenClaw 等工具的标签页级精确控制。

---

2. 核心架构：双模式工作空间与 Profile 模型

2.1 工作空间 (Workspace) 类型

创建 Workspace 时必须选择类型，后续不可变更，该类型决定了其下所有标签页的行为模式：

类型 描述 依赖 Profile 标签页存储行为
普通工作空间 等同于纯净的 Chromium 浏览器，无任何防关联特性，用于日常上网。 否 所有标签页共享浏览器默认 Cookie 和存储，与 Chrome 完全一致。
指纹工作空间 多账号防关联模式，必须基于 Profile 使用，是真正的“多开”工作区。 是 不同 Profile 的标签页之间强制物理隔离；同一 Profile 内的标签页共享存储。

2.2 指纹工作空间内资源层次结构

```
Workspace "欧美店铺" (类型: 指纹)
│
├── Profile "买家号-美国站"                 【对应账号 A】
│   ├── 指纹预设: Win10_Chrome120
│   ├── 代理配置: socks5://us-proxy:1080
│   └── 标签页默认名称: "Amazon US"
│        ├── Tab 1 (URL: amazon.com)   ──┐
│        └── Tab 2 (URL: amazon.com/gp) ──┘ 共享同一套 Cookie、登录状态
│
├── Profile "买家号-德国站"                 【对应账号 B】
│   ├── 指纹预设: Win10_Chrome120
│   ├── 代理配置: socks5://de-proxy:1080
│   └── Tab 3 (URL: amazon.de)   ← 与上面 Profile 的标签页 Cookie 完全隔离
│
└── Profile "卖家号-全球"                   【对应账号 C】
    ├── 指纹预设: macOS_Safari16
    ├── 代理配置: socks5://global-proxy:1080
    └── Tab 4 (URL: sellercentral.amazon.com)  ← 同样完全隔离
```

核心原则：

· Profile = 账号容器： 一个 Profile 设计上对应且仅对应一个线上账号，其绑定的代理与指纹固定不变。
· 隔离边界在 Profile： 不同 Profile 的标签页之间，Cookie、LocalStorage、Cache 等所有存储绝对物理隔离（通过独立的 StoragePartition 实现）。
· 会话共享在 Profile 内： 同一个 Profile 打开的多个标签页，自然地共享登录状态和本地数据，就像使用普通浏览器登录一个账号后打开多个页面一样。

---

3. 用户操作流程

3.1 创建工作空间

1. 用户点击侧边栏“新建工作空间”。
2. 输入工作空间名称。
3. 选择类型：
   · 普通工作空间： 创建成功后即可立即新建标签页，直接输入网址浏览，无需任何额外配置。
   · 指纹工作空间： 创建成功后，系统会提示“请先创建 Profile 以开始使用”，此时标签页列表为空，无法新建标签页，直到至少创建了一个 Profile。

3.2 在指纹工作空间内新建标签页（完整流程）

1. 用户切换到一个指纹工作空间。
2. 点击“新建标签页”按钮，系统弹出 Profile 选择面板。
3. 若工作空间内尚无 Profile，面板唯一选项是“创建首个 Profile”，引导用户创建。
4. 创建 Profile 界面（完全沿用 CloakBrowser 风格）：
   · 名称： 例如“美国站买家号”。
   · 指纹配置： 从预设库选择或自定义（Canvas、WebGL、AudioContext、字体等）。
   · 代理配置： 填写代理类型、IP、端口、账号、密码。
   · 标签页默认标题： 输入“Amazon US”，后续打开的标签页将自动以此命名。
   · 归属工作空间： 自动填充为当前工作空间。
5. 保存 Profile 后，它在 Profile 选择面板中显示。用户选择一个 Profile，点击“打开标签页”。
6. 内核执行：
   · 为该标签页绑定此 Profile 的指纹混淆器和代理隧道。
   · 检查当前工作空间内是否已有同 Profile 的标签页组，若已有则将该 Tab 加入该组，共享 StoragePartition；若没有，则创建全新的 StoragePartition。
   · 标签页标题自动设置为 Profile 预设的“标签页默认标题”。
7. 页面加载，用户可登录对应的账号。

3.3 普通工作空间的日常浏览

· 点击新建标签页，无需任何配置，即刻打开原生 Chromium 标签页。
· 所有标签页在同一浏览器默认的存储环境中运行，共享书签、历史记录、Cookie，与 Chrome 体验完全一致。
· 隔离保证： 普通工作空间的标签页与任何指纹工作空间的标签页处于不同的存储上下文中，数据永远隔离。

---

4. 核心功能需求 (Features)

4.1 双模式工作空间与 Profile 管理

· 【类型锁定】： 工作空间类型一旦创建不可修改，从设计上避免误操作。
· 【视觉标识】： 垂直标签栏中，普通工作空间为无色图标，指纹工作空间为蓝色盾牌图标，活动 Profile 的标签页带有该 Profile 的颜色标记。
· 【Profile 管理入口】： 指纹工作空间顶部常驻“管理 Profile”按钮，可增、删、改 Profile，所有修改即时对所有使用该 Profile 的标签页生效。
· 【标签页手动重命名】： 用户可双击标签页标题临时修改名称，不影响 Profile 默认值。

4.2 内核级 Profile 隔离（核心技术）

· Profile 级 StoragePartition 分配：
  · 修改 content/browser/web_contents/web_contents_impl.cc 等核心源码。
  · 以 Profile ID 为粒度管理 StoragePartition。同 Profile 的所有标签页共享一个 StoragePartition；不同 Profile 强制使用不同 StoragePartition，实现账号间绝对物理隔离。
· Profile 级代理路由：
  · 修改 Chromium 网络栈。标签页发起请求时，内核根据其所属 Profile 的代理配置，直接在 C++ 层完成 SOCKS5/HTTP 代理隧道分流，不走任何 JS 扩展。
· Profile 级 Cloak 指纹集成：
  · 将 CloakBrowser 的 C++ 指纹混淆补丁完整移植到内核。
  · 网页读取指纹时，引擎根据当前标签页所属 Profile 的固定种子，生成确定、稳定、可重复的混淆结果，确保同一账号登录环境始终如一。

4.3 性能与资源优化

· 普通模式零开销： 普通工作空间的标签页直接使用原生 Chromium 存储，无任何额外资源隔离成本。
· 指纹模式智能冻结：
  · 同一 Profile 下的所有标签页作为一个组进行管理。当该组的所有标签页都切入后台超过 10 分钟，内核将冻结其渲染进程，释放 CPU 与内存。
  · 会话保持： 冻结不会清除 StoragePartition 中的 Cookie 与 Storage，切回时自动恢复登录状态。
· 单浏览器实例架构： 所有工作空间共享浏览器主进程、网络进程和 GPU 进程，仅对必要的存储上下文进行隔离。相比“一个账号一个浏览器实例”的传统方案，同等账号规模下内存占用降低 70% 以上。

4.4 全生态自动化兼容

· Tab 与 Profile 级 CDP 控制： 通过扩展 Chrome DevTools Protocol，允许自动化工具通过 tab_id 或 Profile 名称精准连接和控制特定账号的标签页。
· 简洁高效的 CLI：
  ```bash
  # 在指定工作空间的指定 Profile 下打开新标签页
  purecloak --action=new-tab \
            --workspace="欧美店铺" \
            --profile="美国站买家号" \
            --url="https://amazon.com"
  
  # 在普通工作空间中打开网页
  purecloak --action=new-tab \
            --workspace="日常浏览" \
            --url="https://google.com"
  ```
· 反侦测硬防御： 全局抹除 navigator.webdriver 等自动化特征，修复无头模式破绽，确保通过 Pixelscan、CreepJS 等所有主流检测。

---

5. 非功能性需求与编译策略

· 性能指标：
  · 普通模式冷启动速度与原生 Chromium 持平（≤ 500ms）。
  · 指纹模式下冷启动 ≤ 800ms。
  · 20 个使用不同 Profile 的活跃标签页，总内存占用 ≤ 1.8GB。
· 稳定性： 单个 Profile 的渲染进程崩溃，仅影响该 Profile 下的所有标签页，不会波及其他 Profile 的标签页或普通工作空间的标签页。
· 编译精简： 基于 Chromium + CloakBrowser 源码，通过 args.gn 严格剔除 nacl、remoting、print_preview、extensions 等非核心服务，确保内核极致纯净、高效启动。

---

这份 PRD 完整、精确地反映了我们所有沟通的最终结论，彻底落实了 “一个 Profile 对应一个代理、一个 Workspace、一个账号” 的业务模型，并清晰区分了普通与指纹两种工作空间的行为边界，是 Purecloak 进入研发阶段的唯一权威蓝图。
