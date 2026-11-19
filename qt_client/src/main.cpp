#include "AppConfig.h"
#include "MainWindow.h"
#include "net/TlsSetup.h"
#include "ui/LoginDialog.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QSslSocket>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Baka commuity"));
    app.setOrganizationName(QStringLiteral("Baka Community"));
    // 窗口/任务栏图标（与 exe 里的 app.ico 同一张图）
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/badge-256.png")));

    //客户端需要加密连接到nginx，而nginx与后端通信时不需要加密
    // Windows 用 schannel（免分发 OpenSSL DLL），其他平台由 Qt 选 openssl（见 net/TlsSetup）
    const QString tlsError = tlssetup::initialize();
    if (!tlsError.isEmpty()) {
        qCritical("[tls] %s", qPrintable(tlsError));
    }

    // QSS 只做视觉（颜色/圆角/hover），布局全部走布局管理器
    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
    // 启动时读JSON配置文件
    const appconfig::Config config = appconfig::load();

    // 登录/注册暂不接入：先直接进主界面测网络，下面三行接认证时再启用
    // 创建登录页，exec事件循环，阻塞主页，不登录不允许打开
    
    // LoginDialog loginDialog;
    // if (loginDialog.exec() != QDialog::Accepted) return 0;

    MainWindow window(config.serverUrl);
    window.show();
    return app.exec();
}
