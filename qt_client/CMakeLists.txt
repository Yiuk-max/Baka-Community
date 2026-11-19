cmake_minimum_required(VERSION 3.21)

# RC：把 resources/app.rc 编进去，Windows 可执行文件才带图标
project(BakaQtClient VERSION 0.1.0 LANGUAGES CXX RC)

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
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)
# QWebSocket 需要 Qt WebSockets 模块（维护工具安装后自动启用）
find_package(Qt6 6.5 QUIET COMPONENTS WebSockets)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# protobuf 3.20.3（静态库，用 Qt 的 MinGW 13.1 编译，装在机器级目录 D:/libs）
#   安装前缀可用 -DPB_PREFIX=... 覆盖；若机器上没有，可回退到工程内 third_party/protobuf。
# ---------------------------------------------------------------------------
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
find_package(Protobuf REQUIRED)

# message.proto 与后端 src/proto/message.proto 保持同步（勿手改字段号）
set(PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/message.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

qt_add_executable(baka_client
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/AppConfig.h
    src/AppConfig.cpp
    resources/app.rc
    ${PROTO_SRCS}
)

# 配置文件：把 qt_client/config.json 复制到可执行目录，运行时改那里的文件即可
add_custom_command(TARGET baka_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:baka_client>/config.json"
)

# 图标：exe 用 app.rc 里的 app.ico；运行时窗口/任务栏图标用同一张图（qrc）
qt_add_resources(baka_client "appImages"
    PREFIX "/"
    BASE resources
    FILES resources/icons/badge-256.png
)

target_include_directories(baka_client PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}      # 生成 message.pb.h 的目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIR}
)

target_link_libraries(baka_client PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    ${Protobuf_LIBRARIES}
)

if(TARGET Qt6::WebSockets)
    target_link_libraries(baka_client PRIVATE Qt6::WebSockets)
    target_compile_definitions(baka_client PRIVATE BAKA_HAVE_QT_WEBSOCKETS=1)
    message(STATUS "Qt WebSockets: 已安装，客户端使用 QWebSocket")
else()
    message(STATUS "Qt WebSockets: 未安装，客户端暂用自研 QTcpSocket 实现（待安装后切换）")
endif()

set_target_properties(baka_client PROPERTIES WIN32_EXECUTABLE ON)
