#include "AppConfig.h"
#include "MainWindow.h"
#include "net/ChatClient.h"
#include "net/TlsSetup.h"
#include "ui/LoginDialog.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QSslSocket>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Baka Community"));
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

    // 网络出口：登录窗口和主窗口共用同一条连接，所以放在两者之前创建
    ChatClient client(config.serverUrl);

    // 主窗口先建好但不显示：登录成功后服务端会紧跟着推好友申请、离线消息，得有人接着
    MainWindow window(&client);
    client.start();

    // 创建登录页，exec事件循环，阻塞主页，不登录不允许打开
    LoginDialog loginDialog;
    QObject::connect(&loginDialog, &LoginDialog::loginRequested, &client, &ChatClient::login);
    QObject::connect(&loginDialog, &LoginDialog::registerRequested, &client,
                     &ChatClient::registerAccount);
    QObject::connect(&client, &ChatClient::authStarted, &loginDialog,
                     [&loginDialog] { loginDialog.setBusy(true); });
    QObject::connect(&client, &ChatClient::authFailed, &loginDialog, &LoginDialog::showError);
    QObject::connect(&client, &ChatClient::registerSucceeded, &loginDialog,
                     &LoginDialog::showRegistered);
    // 本地有 token 时会自动登录，成功后同样走这里直接关掉登录窗口
    QObject::connect(&client, &ChatClient::loginSucceeded, &loginDialog, &LoginDialog::accept);
    if (loginDialog.exec() != QDialog::Accepted) return 0;

    window.show();
    return app.exec();
}
