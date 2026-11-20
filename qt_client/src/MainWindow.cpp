#include "MainWindow.h"

#include "net/ChatClient.h"
#include "ui/SoundPlayer.h"
#include "ui/ToolRail.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStatusBar>
#include <QTime>
#include <QWidget>

MainWindow::MainWindow(ChatClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client) {
    setWindowTitle(tr("Baka Community"));
    resize(1100, 700);

    m_sound = new SoundPlayer(this);

    // 主界面横向分三块：左侧工具栏 | 左侧通讯录 | 中间页面，水平布局
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_toolRail = new ToolRail(central);
    layout->addWidget(m_toolRail);

    // 通讯录、中间页面还没做，先各放一个空控件占位（到时候换成各自的 QWidget 子类）
    m_contactPanel = new QWidget(central);
    m_contactPanel->setObjectName("contactPanel");
    m_contactPanel->setFixedWidth(240);   // web 端 --sidebar-w
    layout->addWidget(m_contactPanel);

    m_centerPage = new QWidget(central);
    m_centerPage->setObjectName("centerPage");
    layout->addWidget(m_centerPage, 1);   // 伸展因子1，剩余宽度全给中间

    setCentralWidget(central);

    connect(m_toolRail, &ToolRail::logoDoubleClicked, m_sound, &SoundPlayer::playBaka);
    connect(m_toolRail, &ToolRail::logoutClicked, m_client, &ChatClient::logout);

    connect(m_client, &ChatClient::loginSucceeded, this, [this] {
        statusBar()->showMessage(tr("已登录 %1（UID %2）/ 服务器 %3")
                                     .arg(m_client->username())
                                     .arg(m_client->uid())
                                     .arg(m_client->serverUrl()));
    });

    // 状态栏右下角：心跳回执
    m_heartbeatLabel = new QLabel(
        m_client->isConnected() ? tr("心跳：已连接") : tr("心跳：未连接"), this);
    statusBar()->addPermanentWidget(m_heartbeatLabel);

    // 连接状态/异常：临时信息走左侧 statusBar 普通消息
    connect(m_client, &ChatClient::statusMessage, this, [this](const QString &text) {
        statusBar()->showMessage(text, 5000);
    });

    connect(m_client, &ChatClient::disconnected, this, [this](const QString &reason) {
        m_heartbeatLabel->setText(tr("心跳：断开（%1）").arg(reason));
    });

    connect(m_client, &ChatClient::heartbeatAcked, this,
            [this](const QString &content, qint64 elapsedMs) {
        m_heartbeatLabel->setText(tr("❤ 收到心跳包 %1 · %2ms · %3")
                                      .arg(content)
                                      .arg(elapsedMs)
                                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    });

    // ChatClient 没消化的业务消息都从这里出来，后面按 type 分给各页面
    connect(m_client, &ChatClient::envelopeReceived, this,
            [this](const chat_proto::Envelope &envelope) {
        statusBar()->showMessage(
            tr("收到 %1 类型消息（暂未处理）").arg(QString::fromStdString(envelope.type())), 5000);
    });
}
