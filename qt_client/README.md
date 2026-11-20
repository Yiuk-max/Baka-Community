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
│   ├── MainWindow.h/.cpp       # 主窗口：左侧工具栏 | 左侧通讯录（占位）| 中间页面（占位）
│   ├── net/                    # WsCore（网络线程收发）/ PacketCodec（帧编解码）/ TlsSetup
│   │                           # ChatClient（业务层网络出口：心跳、登录、注册、token）
│   └── ui/                     # LoginPage（表单）/ LoginDialog（无边框登录窗口）
│                               # AutoLoginDialog（自动登录等待窗口）/ Spinner（转圈）/ NoticeDialog（通知弹窗）
│                               # ToolRail（左侧工具栏）/ SoundPlayer（提示音）/ Assets.h（静态资源路径）
├── tools/build.bat             # Windows：固定工具链的一键构建脚本
├── tools/build.sh              # Linux / macOS 构建脚本
├── Docs/
│   ├── 开发约束.md              # 【强制】C++17/Widgets/纯代码 UI/QSS 边界/模块交付说明要求
│   ├── 开发环境.md              # 工具链路径 / protobuf 安装 / TLS 后端 / 构建部署 / 踩坑
│   ├── 客户端开发说明.md         # 协议细节 / 时序 / 分页 / 架构规划
│   └── 模块记录/                # 每个模块的类关系 / API / 调用流程
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
- [x] Qt WebSockets 6.11.1 已安装，CMake 自动链接（`BAKA_HAVE_QT_WEBSOCKETS=1`）
- [x] 生产环境连通性已验证：`wss://bakacomm.world/ws` 握手 + 收发一帧 Envelope
      （TLS 固定用 Windows 自带 schannel 后端，不依赖 OpenSSL DLL）
- [x] 网络层骨干：`WsCore`（独立线程收发）+ `PacketCodec`（4B 长度编解码）+ 业务层心跳
- [x] 登录 / 注册 / token 自动登录（`ChatClient` + `LoginDialog`，token 存 QSettings）
- [x] 主窗口三栏划分 + 左侧工具栏 logo 按钮（双击播放 Baka 音效，依赖 Qt Multimedia）+ 默认头像等静态资源入库
- [x] 退出登录 / 被顶下线：清 token 回登录窗口，被顶时先弹通知（`NoticeDialog`）；断线后点登录会先重连
- [ ] 自动断线重连、请求队列
- [ ] 会话列表 + 聊天历史分页 + 实时收发
- [ ] 群聊 / 社区频道 / 好友 / 文件传输

## 相关文档

- 后端协议原始定义：`../src/proto/message.proto`
- 后端接口规范：`../Docs/客户端接口文档.txt`（帧格式/字段/系统提示文本以它为准）
- 后端历史消息实现：`../src/logic/handlers/chat_handler.cpp`、`../src/db/repo/message_repo.cpp`
