#include "AppConfig.h"
#include "MainWindow.h"
#include "net/ChatClient.h"
#include "net/TlsSetup.h"
#include "ui/AutoLoginDialog.h"
#include "ui/LoginDialog.h"
#include "ui/NoticeDialog.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QIcon>

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

    // 本地有 token：先不弹登录窗口，显示转圈的等待窗口；验证失败 / 连不上再换成登录窗口
    bool loggedIn = false;
    if (client.hasSavedToken()) {
        AutoLoginDialog autoLogin;
        QObject::connect(&client, &ChatClient::loginSucceeded, &autoLogin, &AutoLoginDialog::accept);
        // 失败原因已经由上面的 authFailed → showError 写进登录窗口了，这里只负责切换
        QObject::connect(&client, &ChatClient::authFailed, &autoLogin,
                         &AutoLoginDialog::fallbackToLogin);
        // 根本没连上服务器时不会有 authFailed，要单独处理
        QObject::connect(&client, &ChatClient::disconnected, &autoLogin,
                         [&loginDialog, &autoLogin](const QString &reason) {
            loginDialog.showError(QObject::tr("无法连接服务器：%1").arg(reason));
            autoLogin.fallbackToLogin();
        });
        const int result = autoLogin.exec();
        if (result == QDialog::Rejected) return 0;   // 等待时用户自己关掉了窗口
        loggedIn = result == QDialog::Accepted;
    }
    if (!loggedIn && loginDialog.exec() != QDialog::Accepted) return 0;

    // 退出登录 / 被顶下线：收起主窗口回到登录窗口（ChatClient 那边已经清掉了 token）。
    // 用排队连接：这里面要 exec() 对话框，不能卡在 ChatClient 发信号的那次调用里
    QObject::connect(&client, &ChatClient::loggedOut, &app, [&app, &window, &loginDialog](bool kicked) {
        // 主窗口收起后暂时没有可见窗口，先关掉“最后一个窗口关闭就退出”，免得对话框一关程序就没了
        app.setQuitOnLastWindowClosed(false);
        window.hide();
        if (kicked) {
            NoticeDialog notice(QObject::tr("你的账号已在其他位置登录，\n当前设备已下线。"));
            notice.exec();
        }
        loginDialog.resetForm();
        const bool loggedInAgain = loginDialog.exec() == QDialog::Accepted;
        app.setQuitOnLastWindowClosed(true);
        if (loggedInAgain) {
            window.show();
        } else {
            app.quit();   // 在登录窗口点了 ×
        }
    }, Qt::QueuedConnection);

    window.show();
    return app.exec();
}
