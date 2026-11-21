cmake_minimum_required(VERSION 3.21)

project(BakaQtClient VERSION 0.1.0 LANGUAGES CXX)

# RC（资源编译器）只有 Windows 有：把 resources/app.rc 编进去，exe 才带图标
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# 不使用 Qt Designer，故不启用 AUTOUIC（见 Docs/开发约束.md 第 3 条）

# ---------------------------------------------------------------------------
# Qt 6.11.1 mingw_64：D:/QT/6.11.1/mingw_64
#   cmake -S . -B build -G Ninja \
#     -DCMAKE_PREFIX_PATH=D:/QT/6.11.1/mingw_64 \
#     -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe
# ---------------------------------------------------------------------------
# WebSockets（QWebSocket）、Multimedia（提示音）不在 Qt 默认安装里，缺了用维护工具补装
# Sql（QSqlDatabase）是 Qt 默认安装自带的，SQLite 驱动是插件 plugins/sqldrivers/qsqlite.dll，不用另装 sqlite
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia WebSockets Sql)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# protobuf 3.20.3（静态库，用 Qt 的 MinGW 13.1 编译，装在机器级目录 D:/libs）
#   安装前缀可用 -DPB_PREFIX=... 覆盖；若机器上没有，可回退到工程内 third_party/protobuf。
# ---------------------------------------------------------------------------
if(WIN32)
    # Windows：用本机手编的静态库（MinGW 13.1 编的，ABI 与 Qt 一致）
    set(PB_PREFIX "D:/libs/protobuf-3.20.3" CACHE PATH "protobuf install prefix")
    if(NOT EXISTS "${PB_PREFIX}/bin/protoc.exe" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/bin/protoc.exe")
        set(PB_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf")
    endif()
    if(NOT EXISTS "${PB_PREFIX}/bin/protoc.exe")
        message(FATAL_ERROR "找不到 protobuf（${PB_PREFIX}）。安装方法见 Docs/开发环境.md 第 2 节。")
    endif()
    set(Protobuf_INCLUDE_DIR "${PB_PREFIX}/include")
    set(Protobuf_LIBRARIES   "${PB_PREFIX}/lib/libprotobuf.a")
    set(Protobuf_PROTOC_EXECUTABLE "${PB_PREFIX}/bin/protoc.exe")
endif()
# Linux / 其他平台：走系统包（apt install libprotobuf-dev protobuf-compiler）
find_package(Protobuf REQUIRED)

# message.proto 与后端 src/proto/message.proto 保持同步（勿手改字段号）
set(PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/message.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

set(APP_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MainWindowRequests.cpp
    src/AppConfig.h
    src/AppConfig.cpp
    src/ui/LoginPage.h
    src/ui/LoginPage.cpp
    src/ui/LoginDialog.h
    src/ui/LoginDialog.cpp
    src/ui/AutoLoginDialog.h
    src/ui/AutoLoginDialog.cpp
    src/ui/NoticeDialog.h
    src/ui/NoticeDialog.cpp
    src/ui/Spinner.h
    src/ui/Spinner.cpp
    src/ui/Assets.h
    src/ui/Avatar.h
    src/ui/Avatar.cpp
    src/ui/ContactPanel.h
    src/ui/ContactPanel.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/AccountSettingsPage.h
    src/ui/AccountSettingsPage.cpp
    src/ui/BakaSettingsPage.h
    src/ui/BakaSettingsPage.cpp
    src/model/Contact.h
    src/model/Message.h
    src/util/TimeFormat.h
    src/ui/ToolRail.h
    src/ui/ToolRail.cpp
    src/ui/SoundPlayer.h
    src/ui/SoundPlayer.cpp
    src/ui/ChatPage.h
    src/ui/ChatPage.cpp
    src/ui/Composer.h
    src/ui/Composer.cpp
    src/ui/EmojiPanel.h
    src/ui/EmojiPanel.cpp
    src/ui/HomePage.h
    src/ui/HomePage.cpp
    src/ui/MessageRow.h
    src/ui/MessageRow.cpp
    src/ui/InputDialog.h
    src/ui/InputDialog.cpp
    src/ui/MenuStyle.h
    src/ui/FileCover.h
    src/ui/FileCover.cpp
    src/ui/PendingPage.h
    src/ui/PendingPage.cpp
    src/ui/InviteDialog.h
    src/ui/InviteDialog.cpp
    src/net/PacketCodec.h
    src/net/PacketCodec.cpp
    src/net/WsCore.h
    src/net/WsCore.cpp
    src/net/ChatClient.h
    src/net/ChatClient.cpp
    src/net/TlsSetup.h
    src/net/TlsSetup.cpp
    ${PROTO_SRCS}
)

if(WIN32)
    list(APPEND APP_SOURCES resources/app.rc)   # Windows 图标资源（Linux 不需要）
endif()

qt_add_executable(baka_client ${APP_SOURCES})

# 配置文件：把 qt_client/config.json 复制到可执行目录，运行时改那里的文件即可
add_custom_command(TARGET baka_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:baka_client>/config.json"
)

# 图标：exe 用 app.rc 里的 app.ico；运行时窗口/任务栏图标用同一张图（qrc）
qt_add_resources(baka_client "appStyles"
    PREFIX "/"
    BASE resources
    FILES resources/style.qss
)

qt_add_resources(baka_client "appImages"
    PREFIX "/"
    BASE resources
    FILES resources/icons/badge-256.png
          resources/icons/settings.png
          resources/icons/download.png
          resources/icons/toggle_off.png
          resources/icons/toggle_on.png
          resources/images/logo.png
          resources/images/default_avatar.png
          resources/images/default_group_avatar.png
          resources/images/default_comm_avatar.png
          resources/images/default_background.png
)

# 提示音（路径集中在 src/ui/Assets.h）
qt_add_resources(baka_client "appAudio"
    PREFIX "/"
    BASE resources
    FILES resources/audio/baka.mp3
)

target_include_directories(baka_client PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}      # 生成 message.pb.h 的目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIR}
)

target_link_libraries(baka_client PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::WebSockets Qt6::Sql
    ${Protobuf_LIBRARIES}
)

if(WIN32)
    set_target_properties(baka_client PROPERTIES WIN32_EXECUTABLE ON)
endif()
