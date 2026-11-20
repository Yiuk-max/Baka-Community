#include "MainWindow.h"

#include "net/ChatClient.h"
#include "ui/ContactPanel.h"
#include "ui/SettingsDialog.h"
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

    m_contactPanel = new ContactPanel(central);
    layout->addWidget(m_contactPanel);

    // 中间页面还没做，先放一个空控件占位（到时候换成自己的 QWidget 子类）
    m_centerPage = new QWidget(central);
    m_centerPage->setObjectName("centerPage");
    layout->addWidget(m_centerPage, 1);   // 伸展因子1，剩余宽度全给中间

    setCentralWidget(central);

    // 双击 logo 播放音效，可以在设置 - Baka 里关掉
    connect(m_toolRail, &ToolRail::logoDoubleClicked, this, [this] {
        if (m_sound->playOnLogo()) m_sound->playBaka();
    });
    connect(m_contactPanel, &ContactPanel::settingsClicked, this, &MainWindow::openSettings);

    // 聊天页、社区页还没做：点了先只在状态栏提示，做出来之后在这里切换中间页面
    connect(m_contactPanel, &ContactPanel::contactSelected, this, [this](const Contact &contact) {
        statusBar()->showMessage(tr("已选中%1「%2」（聊天页还没做）")
                                     .arg(contact.isGroup ? tr("群聊") : tr("好友"), contact.name), 5000);
    });
    connect(m_toolRail, &ToolRail::communityClicked, this, [this](const Community &community) {
        statusBar()->showMessage(tr("社区「%1」（社区页还没做）").arg(community.name), 5000);
    });

    connect(m_client, &ChatClient::loginSucceeded, this, [this] {
        statusBar()->showMessage(tr("已登录 %1（UID %2）/ 服务器 %3")
                                     .arg(m_client->username())
                                     .arg(m_client->uid())
                                     .arg(m_client->serverUrl()));
        m_contactPanel->setAccount(m_client->username(), m_client->uid());
        m_contactPanel->setOnline(true);
        requestLists();
    });

    // 退出登录 / 被顶下线：上一个账号的联系人和社区不能留给下一个账号看到
    connect(m_client, &ChatClient::loggedOut, this, [this] {
        m_contactPanel->clear();
        m_toolRail->setCommunities({});
    });

    connect(m_client, &ChatClient::connected, this, [this] { m_contactPanel->setOnline(true); });

    // 在设置里改了昵称：左下角账号栏跟着变
    connect(m_client, &ChatClient::profileChanged, this, [this] {
        m_contactPanel->setAccount(m_client->username(), m_client->uid());
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
        m_contactPanel->setOnline(false);
    });

    connect(m_client, &ChatClient::heartbeatAcked, this,
            [this](const QString &content, qint64 elapsedMs) {
        m_heartbeatLabel->setText(tr("❤ 收到心跳包 %1 · %2ms · %3")
                                      .arg(content)
                                      .arg(elapsedMs)
                                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    });

    // ChatClient 没消化的业务消息都从这里出来，后面按 type 分给各页面
    connect(m_client, &ChatClient::envelopeReceived, this, &MainWindow::handleEnvelope);
}

void MainWindow::openSettings() {
    SettingsDialog dialog(m_client->username(), m_client->uid(), m_client->email(), m_sound, this);
    // 开着设置时被顶下线：先把设置窗口关掉，再让 main.cpp 那边切回登录窗口
    connect(m_client, &ChatClient::loggedOut, &dialog, &QDialog::reject);
    // 改昵称 / 邮箱：设置窗口只发信号，请求由 ChatClient 发，结果再回填给窗口
    connect(&dialog, &SettingsDialog::nameChangeRequested, m_client, &ChatClient::changeName);
    connect(&dialog, &SettingsDialog::emailChangeRequested, m_client, &ChatClient::changeEmail);
    connect(m_client, &ChatClient::profileChanged, &dialog, [this, &dialog] {
        dialog.setProfile(m_client->username(), m_client->email());
    });
    connect(m_client, &ChatClient::profileChangeFailed, &dialog, &SettingsDialog::showProfileError);
    if (dialog.exec() == SettingsDialog::Logout) m_client->logout();
}

void MainWindow::requestLists() {
    chat_proto::Envelope contacts;
    contacts.set_type("show_contacts");
    m_client->send(contacts);

    chat_proto::Envelope communities;
    communities.set_type("show_communities");
    m_client->send(communities);
}

void MainWindow::handleEnvelope(const chat_proto::Envelope &envelope) {
    const QString type = QString::fromStdString(envelope.type());

    if (type == QStringLiteral("contact_list_response")) {
        QList<Contact> contacts;
        for (const chat_proto::ContactInfo &info : envelope.contacts()) {
            Contact contact;
            contact.uid = info.uid();
            contact.name = QString::fromStdString(info.name());
            if (contact.name.isEmpty()) contact.name = QString::number(contact.uid);
            contact.isGroup = info.is_group();
            contact.avatarId = info.avatar_id();
            contacts.append(contact);
        }
        m_contactPanel->setContacts(contacts);
        return;
    }

    if (type == QStringLiteral("community_list_response")) {
        QList<Community> communities;
        for (const chat_proto::CommunityInfo &info : envelope.communities()) {
            Community community;
            community.id = info.community_id();
            community.name = QString::fromStdString(info.name());
            community.avatarId = info.avatar_id();
            communities.append(community);
        }
        m_toolRail->setCommunities(communities);
        return;
    }

    statusBar()->showMessage(tr("收到 %1 类型消息（暂未处理）").arg(type), 5000);
}
