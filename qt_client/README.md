# Baka Community — Qt 客户端

Baka Community 聊天系统的**桌面客户端**（Qt 6.11.1 + Widgets + C++17；主力平台 Windows / MinGW 13.1，Linux 用 `tools/build.sh`）。

> 开发约束（强制）：[`Docs/开发约束.md`](Docs/开发约束.md) —— C++17 + Widgets + 纯 C++ 建 UI + 不用 `.ui` + 每页一个 QWidget + Signal/Slot。
> 版本库：本工程是后端仓库 `Baka-Community` 内的子目录 `qt_client/`，与 `src/`（后端）、`frontend/`（web）同库。
> 仓库：`https://github.com/Yiuk-max/Baka-Community`（原 `-----server`，已改名）。详见 `Docs/开发环境.md` 第 6 节。

后端不本地运行，直接连**生产环境**：

| 项 | 值 |
|---|---|
| WebSocket 端点 | `wss://bakacomm.world/ws` |
| 网络层 | `net_layer=websocket`（binary/epoll 模式仅 Linux，客户端不用） |
| 编码 | protobuf `chat_proto.Envelope` |
| 帧格式 | WS binary 帧，payload = `4 字节 payload_len（大端） + protobuf` |

> 协议已实测：TLS + WS 握手成功，binary 帧的 4 字节长度前缀与 protobuf 双向收发都验证过，
> 服务端返回 `Envelope{type:"system", content:"You must be logged in to view your chat list."}`。

## 目录结构

```
qt_client/
├── CMakeLists.txt              # 工程入口（Qt6 + protobuf + proto 自动生成）
├── config.json                 # 运行配置（server_url），构建时复制到 exe 同目录
├── proto/message.proto         # 协议唯一来源，从 ../src/proto/message.proto 同步
├── resources/                  # style.qss（只放视觉样式）、icons/、images/（logo、默认头像等）、audio/baka.mp3、app.rc
├── src/
│   ├── main.cpp                # 入口：TLS / QSS / 配置 → ChatClient → 登录窗口 → 主窗口
│   ├── AppConfig.h/.cpp        # 读 config.json
│   ├── MainWindow.h/.cpp       # 主窗口：左侧工具栏 | 左侧通讯录 | 中间页面（主页 / 聊天页）
│   ├── MainWindowRequests.cpp  # 还是 MainWindow 类：邀请好友、入群 / 入社区申请的拉取和审批
│   ├── model/                  # Contact.h（好友 / 群 / 社区 / 申请）、Message.h（一条消息）
│   ├── net/                    # WsCore（网络线程收发）/ PacketCodec（帧编解码）/ TlsSetup
│   │                           # ChatClient（业务层网络出口：心跳、登录、注册、token、改昵称邮箱）
│   ├── ui/                     # LoginPage（表单）/ LoginDialog（无边框登录窗口）
│   │                           # AutoLoginDialog（自动登录等待窗口）/ Spinner（转圈）/ NoticeDialog（通知弹窗）
│   │                           # ToolRail（左侧工具栏、“＋”菜单、社区右键菜单）/ ContactPanel（左侧通讯录）
│   │                           # HomePage（主页）/ PendingPage（“待定”页：好友 / 入群 / 入社区申请）
│   │                           # ChatPage（聊天页）/ MessageRow（一条消息）/ Composer（输入框）/ EmojiPanel（表情面板）
│   │                           # FileCover（文件封面等附件外观）/ InputDialog（输入 / 确认弹窗）/ InviteDialog（邀请好友）
│   │                           # SettingsDialog（设置窗口）/ AccountSettingsPage / BakaSettingsPage（设置里的两页）
│   │                           # Avatar（头像裁圆角）/ SoundPlayer（提示音）/ MenuStyle.h（菜单外观）/ Assets.h（静态资源路径）
│   └── util/                   # TimeFormat.h（UTC 时间转本地显示）
├── tools/build.bat             # Windows：固定工具链的一键构建脚本
├── tools/build.sh              # Linux / macOS 构建脚本
├── Docs/
│   ├── 开发约束.md              # 【强制】C++17/Widgets/纯代码 UI/QSS 边界/协作规则
│   ├── 开发环境.md              # 工具链路径 / protobuf 安装 / TLS 后端 / 构建部署 / 踩坑
│   └── 客户端开发说明.md         # 协议细节 / 时序 / 分页 / 架构规划
└── build/                      # 本工程构建产物（不进版本库）
```

protobuf 3.20.3 装在**机器级目录** `D:/libs/protobuf-3.20.3`（不在仓库内，删工程不受影响），
详见 `Docs/开发环境.md` 第 2 节。

## 快速构建

在**仓库根目录**（`D:\bakacomm_client\-----server`）下操作：

```bat
qt_client\tools\build.bat            :: 配置 + 编译
qt_client\build\baka_client.exe      :: 运行（需要 Qt bin 在 PATH，或先 build.bat deploy）
```

手动构建（等价）：

```bash
export PATH="/d/QT/Tools/CMake_64/bin:/d/QT/Tools/Ninja:/d/QT/Tools/mingw1310_64/bin:$PATH"
cmake -S ./qt_client -B ./qt_client/build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=D:/QT/6.11.1/mingw_64 \
  -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe
cmake --build ./qt_client/build -j4
```

## 当前状态

- [x] 工具链打通（Qt 6.11.1 + MinGW 13.1 + CMake 3.30.5 + Ninja 1.12.1 + Qt Creator 19.0.2）
- [x] protobuf 3.20.3 静态库（Qt 的 MinGW 13.1 编译，装在 `D:/libs`）
- [x] `message.proto` 自动生成并在构建中使用（protoc 3.20.3）
- [x] Qt WebSockets 6.11.1 已安装，CMake 里是必需组件（缺了 configure 阶段直接报错）
- [x] 生产环境连通性已验证：`wss://bakacomm.world/ws` 握手 + 收发一帧 Envelope
      （TLS 固定用 Windows 自带 schannel 后端，不依赖 OpenSSL DLL）
- [x] 网络层骨干：`WsCore`（独立线程收发）+ `PacketCodec`（4B 长度编解码）+ 业务层心跳
- [x] 登录 / 注册 / token 自动登录（`ChatClient` + `LoginDialog`，token 存 QSettings）
- [x] 主窗口三栏划分 + 左侧工具栏 logo 按钮（双击播放 Baka 音效，依赖 Qt Multimedia）+ 默认头像等静态资源入库
- [x] 被顶下线：清 token 回登录窗口，先弹通知（`NoticeDialog`）；断线后点登录会先重连
- [x] 设置窗口：账户（可改用户名、邮箱）、Baka（音量、双击 LOGO 播放），登出入口在设置窗口左下角
- [x] 左侧工具栏（社区图标）+ 左侧通讯录（搜索、好友 / 群列表、账号栏），登录后拉 `show_contacts` / `show_communities`
- [x] 主页（好友 / 群 / 社区列表）+ “待定”页（好友申请、入群申请、入社区申请的审批）
- [x] 私聊 / 群聊：历史分页（上滑加载）、实时收发、乐观消息、回复、撤回、表情面板
- [x] 工具栏“＋”菜单（创建社区 / 创建群聊 / 申请加入社区 / 申请加入群聊 / 添加好友）
- [x] 社区右键菜单（邀请至社区 / 创建频道 / 离开社区）、通讯录右键群聊“邀请好友”（从好友列表点选）
- [x] 文件消息显示封面 / 占位图（只是显示，不能下载和预览）
- [ ] 社区页：频道列表、成员列表、频道聊天（现在点社区只在状态栏提示）
- [ ] 文件上传 / 下载 / 图片预览（`file_*` 协议）、真实头像下载（现在全是默认图）
- [ ] 自动断线重连、请求队列
- [ ] 未读角标、连续同人消息合并显示
- [ ] 通讯录右键：删除好友、设置备注、退群、改群名
- [ ] 社区设置、`verify_email`、`change_theme`

明确不做：消息的表情反应；web 端标 `notDeveloped` 的功能（转发、标记未读、举报）。

## 相关文档

- 后端协议原始定义：`../src/proto/message.proto`
- 后端接口规范：`../Docs/客户端接口文档.txt`（帧格式/字段/系统提示文本以它为准）
- 后端历史消息实现：`../src/logic/handlers/chat_handler.cpp`、`../src/db/repo/message_repo.cpp`
