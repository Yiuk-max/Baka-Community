#include "MainWindow.h"

#include "ui/LoginPage.h"

#include <QStatusBar>
#include <QWidget>

MainWindow::MainWindow(const QString &serverUrl, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Baka Community"));
    resize(1100, 700);
    //创建登录页
    auto *loginPage = new LoginPage(this);
    setCentralWidget(loginPage);

    // 骨架阶段把读到的配置显示出来，方便确认 config.json 生效
    statusBar()->showMessage(tr("Qt %1 / 服务器 %2").arg(QT_VERSION_STR, serverUrl));

    // 网络层还没接：先把请求回显在状态栏
    connect(loginPage, &LoginPage::loginRequested, this,
            [this](const QString &identity, const QString &) {
                statusBar()->showMessage(tr("登录请求：%1（网络层待接入）").arg(identity), 5000);
            });
    connect(loginPage, &LoginPage::registerRequested, this,
            [this](const QString &nickname, const QString &email, const QString &) {
                statusBar()->showMessage(tr("注册请求：%1 / %2（网络层待接入）").arg(nickname, email), 5000);
            });
}
