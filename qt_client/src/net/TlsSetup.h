#pragma once

// TLS 后端选择（跨平台方案，按平台挑后端）
//   Windows -> schannel（系统 TLS 栈，不用随包分发 OpenSSL DLL）
//   其他平台 -> 不强制，Qt 自动用 openssl（Linux 发行版自带 libssl3 / libcrypto3）
// Qt 6 的 TLS 是运行时插件（plugins/tls/qopensslbackend、qschannelbackend），
// 编译期不需要链接 OpenSSL，所以这里只做“选择 + 自检”。
#include <QString>

namespace tlssetup {

// 初始化 TLS 后端；成功返回空，失败返回错误说明
QString initialize();

// 描述当前状态，用于日志/排查
//   "active=schannel, available=cert-only,openssl,schannel"
QString describe();

}
