# Baka Community

一个即时通讯 / 社区（Discord 式频道）项目：**C++17 后端 + Vue 3 网页前端**，
支持私聊、群聊、好友、文件传输与离线消息。后端提供 epoll 与 Boost.Asio 两套网络实现，
协议统一走 protobuf，浏览器打开网址即可使用。

线上地址：**https://bakacomm.world**（HTTPS 页面 + WSS 连接，由 Nginx / Cloudflare 终结 TLS，
C++ 服务本身只监听明文 `:8080`）

---

## 项目概览

仓库覆盖 IM 系统的主要链路：网络、协议、存储、业务。

- **网络层**：Linux `epoll` + 主从 Reactor 多线程模型；另有一份 Boost.Asio + Beast 的 WebSocket 实现，
  启动时由配置二选一，两条路径复用同一套业务代码。
- **协议层**：请求与响应统一用 protobuf 编码。所有消息共用一个 `Envelope`，靠 `type` 区分种类，
  新增功能不需要改帧格式。
- **业务层**：登录态、好友关系、群成员、社区权限、离线消息、文件分片等，按消息类型拆成独立 handler。
- **存储层**：MySQL 存关系数据，Redis 缓存热点数据。
- **前端**：Vue 3 单页应用，构建产物由 C++ 服务静态托管，因此对外只需要 8080 一个端口。

## 功能

| 模块 | 能力 |
|---|---|
| 账号 | 注册、UID / 邮箱登录、token 自动登录（7 天滑动过期）、改昵称 / 邮箱 / 头像 / 主题、被顶号下线 |
| 私聊 | 双向会话、回复引用、删除消息、离线消息补收 |
| 群聊 | 建群 / 拉人 / 踢人 / 改名 / 转让与身份管理、入群申请审批 |
| 社区 | Discord 式结构：社区 = 核心频道 + 若干普通频道，成员角色 owner / admin / member，频道分区 |
| 好友 | 按邮箱申请、同意 / 拒绝 / 删除、备注名、结构化好友列表 |
| 文件 | 分片上传下载（4 MiB/片）、进度、暂停 / 继续、断线重连续传、过期会话 watchdog |
| 其他 | 业务层心跳、结构化联系人 / 频道 / 成员列表、消息与文件的时间统一存 UTC |

## 架构

```
            浏览器 (Vue 3 SPA)
                   │  HTTPS / WSS
              Nginx / Cloudflare        ← 证书、静态资源、反代（deploy/nginx-chat.conf）
                   │  明文 ws
        ┌──────────▼──────────┐
        │   C++ 服务 :8080     │
        │  ┌────────────────┐  │
        │  │ 网络层（二选一） │  │  epoll 主从 Reactor  或  Boost.Asio + Beast
        │  └───────┬────────┘  │
        │   IClientTransport   │  传输抽象：业务层不关心是 TCP 还是 WS
        │  ┌───────▼────────┐  │
        │  │ 业务层 handlers │  │  account / chat / group / community / friend / file
        │  └───────┬────────┘  │
        │  ┌───────▼────────┐  │
        │  │ 数据层 repo     │  │  repo_interface 契约 + MySQL 实现
        │  └───────┬────────┘  │
        └──────────┼───────────┘
             ┌─────▼─────┐   ┌──────────┐
             │  MySQL    │   │  Redis   │
             └───────────┘   └──────────┘
```

几个关键设计：

- **传输抽象**：`IClientTransport` 抽象出"怎么发字节"，因此 epoll 与 WebSocket 两套网络层共享
  同一份 `client_session` 与 handler。
- **单一信封**：`Envelope` 承载所有字段，字段号一经发布不再改，只追加。协议定义唯一来源是
  `src/proto/message.proto`。
- **两套帧格式**：TCP binary 模式是「4 字节总长度 + 4 字节 payload 长度 + protobuf」；
  WebSocket 模式自带消息边界，只需「4 字节 payload 长度 + protobuf」，文件数据追加在帧尾。
- **时间统一 UTC**：MySQL 时区固定 `+00:00`，`last_login_time` 用 `gmtime` 生成，前端拿
  `"YYYY-MM-DD HH:MM:SS"` 后转本地时区显示。
- **连接与线程**：主 Reactor 管 accept / 读事件，业务丢进线程池；每个会话独立 strand，
  同一连接的收发不会并发。

## 技术栈

| 层 | 选型 |
|---|---|
| 语言 / 构建 | C++17、CMake、Ninja |
| 网络 | Linux epoll（主从 Reactor）／ Boost.Asio + Beast WebSocket |
| 协议 | protobuf 3（`chat_proto.Envelope`） |
| 存储 | MySQL 8（11 张表）、Redis（redis-plus-plus，已落地 C1/C2/C4/C7–C11 缓存点） |
| 前端 | Vite + Vue 3，构建产物输出到 `web/` 由后端静态托管 |

## 快速开始（本地跑起来）

```bash
# 1) 依赖：boost / protobuf / mysql-connector-c++ / hiredis（Redis 客户端经 FetchContent 自动拉取）
sudo apt install -y libboost-dev libboost-system-dev libprotobuf-dev protobuf-compiler \
                    libmysqlcppconn-dev

# 2) 配置：配置文件按「运行目录」的相对路径读取，所以先拷到构建目录
cp configure.json build/
cd build && cmake .. && cmake --build . -j8

# 3) 建库（见 sql/create_table.sql、sql/file_system.sql）
mysql -u root -p < ../sql/create_table.sql
mysql -u root -p < ../sql/file_system.sql

# 4) 启动（默认 :8080）
./server
```

前端单独构建（改动 `frontend/` 之后）：

```bash
cd frontend && npm install && npm run build   # 产出到 ../web
```

跑起来之后浏览器打开 `http://<host>:8080/` 即可（仅 `net_layer=websocket` 模式会提供页面与静态资源）。

## 配置

`configure.json`（相对运行目录）主要项：

1. `net_layer`：`binary` 或 `websocket`，**启动时读取，改完要重启**，两者不能同时跑。
2. 心跳：`use_heartbeat` / `heartbeat_interval`（业务层心跳，不是 WS 自带 ping）。
3. 连接与线程：`max_connections`、`business_threads`。
4. WebSocket：`path`、`io_threads`、`max_message_bytes`、`max_pending_bytes`、`web_root`。
5. 数据库连接池：`database.*`（`db_conn_count`）。
6. Redis：`redis.enabled/host/port/db/pool_size/timeout_ms`，关掉后回退纯 MySQL。
7. 文件存储：`file_storage.*`（分片大小、单文件上限、传输超时等）。

> 完整部署（systemd、Nginx 反代、证书、常见问题）见 [Docs/部署文档.md](Docs/部署文档.md)。

## 测试

```bash
# 单元测试（不需要 MySQL）
cmake -S . -B build -DSERVER_BUILD_TESTS=ON && cmake --build build -j8
ctest --test-dir build --output-on-failure

# 端到端冒烟（需要 MySQL，服务端以 net_layer=websocket 启动）
python3 tests/ws_chat_smoke.py 127.0.0.1 8080 /ws      # 注册/登录/好友/私聊/群聊/离线/顶号
python3 tests/community_smoke.py 127.0.0.1 8080 /ws    # 社区/频道/成员管理
python3 tests/test_group_broadcast.py 127.0.0.1 8080 /ws  # 群聊广播多端字段一致性
python3 tests/file_smoke.py 127.0.0.1 8080 /ws         # 分片上传下载、暂停续传、断线续传
```

## 文档

- [部署文档.md](Docs/部署文档.md) —— 从零部署：依赖、建库、构建、systemd、Nginx、常见问题
- [客户端接口文档.txt](Docs/客户端接口文档.txt) —— 帧格式、`Envelope` 字段总表、每个消息类型的请求/响应、系统提示文本
- [架构模型与状态机.md](Docs/架构模型与状态机.md) —— 网络 / 业务 / 文件传输模型、Session 生命周期、状态机
- [前端说明](frontend/README.md) —— Vite + Vue 3 的开发构建与 UI 功能清单
- [社区后端改造方案.md](Docs/社区后端改造方案.md) —— Discord 式社区表结构、权限与 Redis 缓存点
- [protobuf改造方案-done.md](Docs/protobuf改造方案-done.md) —— JSON → protobuf 的迁移设计与验证矩阵
- [WebSocket接入代码改动文档-done.md](Docs/WebSocket接入代码改动文档-done.md) —— WebSocket 层接入记录
- [client_session拆分方案-done.md](Docs/client_session拆分方案-done.md) —— 会话拆分与业务逻辑下沉 handler
- [数据库设计.txt](Docs/数据库设计.txt) —— 表结构、UID 分配、连接池接入
- [Redis缓存方案-done.md](Docs/Redis缓存方案-done.md) —— 缓存点现状（C1/C2/C4/C7–C11 已落地）与瓶颈分析
- [文件系统方案.md](Docs/文件系统方案.md) / [文件传输系统说明.md](Docs/文件传输系统说明.md) —— 文件体系、分片、续传、watchdog
- [压力测试方案.md](Docs/压力测试方案.md) —— WebSocket 压测方案与用例
- [开发日志.txt](build/开发日志.txt) —— 待办与历次重构记录

## 目录结构

```
.
├── src/
│   ├── main.cpp            # 入口：监听、主从 Reactor、线程池、连接池初始化
│   ├── net_core/           # epoll 网络层：epoller / client_session / session_manager / notice_service ...
│   ├── net_core_ws/        # WebSocket 网络层：ws_server / ws_session（Boost.Asio + Beast，含静态资源服务）
│   ├── net_common/         # 传输抽象 IClientTransport + 消息载体
│   ├── proto/              # message.proto（协议唯一来源）
│   ├── logic/              # 业务层：account / group / social_module + handlers（各类型消息处理器）
│   ├── db/                 # mysql 连接池 / repo_interface 契约 / repo MySQL 实现
│   └── utils/              # 线程池、配置、Redis 客户端与缓存
├── include/total.h         # 基础公共头
├── configure.json          # 运行配置
├── sql/                    # 建表与迁移脚本
├── deploy/nginx-chat.conf  # HTTPS + WSS 反代示例
├── frontend/               # Vue 3 源码（npm run build → web/）
├── web/                    # 前端构建产物（后端静态托管，勿手改）
├── tests/                  # 单元测试 + 端到端冒烟脚本 + 低阶浏览器测试页
└── build/                  # 构建输出
```

## 说明

- 仓库里同时包含后端、Vue 前端、protobuf schema、SQL 脚本与测试，`src/proto/message.proto` 是协议定义的唯一来源，
  前后端字段号必须一致。
- 服务端的 TLS 由 Nginx / Cloudflare 负责，C++ 进程只讲明文 WebSocket；这也是为什么
  客户端必须连 `wss://bakacomm.world/ws` 而不是 `wss://<ip>:8080`。
