#include "AppConfig.h"
#include "MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QSslSocket>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Baka commuity"));
    app.setOrganizationName(QStringLiteral("Baka Community"));
    // 窗口/任务栏图标（与 exe 里的 app.ico 同一张图）
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/badge-256.png")));

    // Windows 上固定使用系统 schannel TLS 后端：wss:// 无需随包分发 OpenSSL DLL。
    // （Qt 6.11 自带 qopensslbackend / qschannelbackend 两个插件，默认优先 openssl）
    //客户端需要加密连接到nginx，而nginx与后端通信时不需要加密
    #ifdef Q_OS_WIN
    QSslSocket::setActiveBackend(QStringLiteral("schannel"));
#endif

    // 启动时读JSON配置文件
    const appconfig::Config config = appconfig::load();

    MainWindow window(config.serverUrl);
    window.show();
    return app.exec();
}
