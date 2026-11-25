#include "TlsSetup.h"

#include <QDebug>
#include <QSslSocket>
#include <QStringList>

namespace tlssetup {

QString initialize() {
#ifdef Q_OS_WIN
    // Windows：固定用系统 schannel，wss:// 不需要 libssl / libcrypto DLL
    if (!QSslSocket::setActiveBackend(QStringLiteral("schannel"))) {
        return QStringLiteral("无法启用 schannel TLS 后端");
    }
#endif
    // Linux / 其他平台：不指定后端，Qt 会选 openssl（需要系统装 libssl3）

    // 触发一次 TLS 后端加载，确认真的可用（缺库时这里最能说明问题）
    if (!QSslSocket::supportsSsl()) {
        return QStringLiteral("TLS 不可用，可用的后端：%1")
            .arg(QSslSocket::availableBackends().join(QStringLiteral(", ")));
    }

    qInfo("[tls] %s", qPrintable(describe()));
    return {};
}

QString describe() {
    return QStringLiteral("active=%1, available=%2")
        .arg(QSslSocket::activeBackend(),
             QSslSocket::availableBackends().join(QStringLiteral(",")));
}

} // namespace tlssetup
