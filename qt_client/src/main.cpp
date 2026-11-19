#include "MainWindow.h"

#include <QApplication>
#include <QSslSocket>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Windows 上固定使用系统 schannel TLS 后端：wss:// 无需随包分发 OpenSSL DLL。
    // （Qt 6.11 自带 qopensslbackend / qschannelbackend 两个插件，默认优先 openssl）
#ifdef Q_OS_WIN
    QSslSocket::setActiveBackend(QStringLiteral("schannel"));
#endif

    MainWindow window;
    window.show();
    return app.exec();
}
