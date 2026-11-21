#include "MainWindow.h"

#include "net/ChatClient.h"
#include "ui/ChatPage.h"
#include "ui/ContactPanel.h"
#include "ui/HomePage.h"
#include "ui/SettingsDialog.h"
#include "ui/SoundPlayer.h"
#include "ui/ToolRail.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
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

    // 中间区域：主页和聊天页叠在一起，同一时刻只显示一个
    m_center = new QStackedWidget(central);
    m_homePage = new HomePage(m_center);
    m_chatPage = new ChatPage(m_client, m_center);
    m_center->addWidget(m_homePage);
    m_center->addWidget(m_chatPage);
    layout->addWidget(m_center, 1);   // 伸展因子1，剩余宽度全给中间

    setCentralWidget(central);

    // 双击 logo 播放音效，可以在设置 - Baka 里关掉
    connect(m_toolRail, &ToolRail::logoDoubleClicked, this, [this] {
        if (m_sound->playOnLogo()) m_sound->playBaka();
    });
    // 单击 logo：回到主页
    connect(m_toolRail, &ToolRail::homeClicked, this, &MainWindow::showHome);
    connect(m_contactPanel, &ContactPanel::settingsClicked, this, &MainWindow::openSettings);

    // 点选联系人：打开对应的聊天页
    connect(m_contactPanel, &ContactPanel::contactSelected, this, &MainWindow::openChat);
    // 在主页点好友 / 群：同样打开聊天页，并让左侧通讯录跟着选中
    connect(m_homePage, &HomePage::contactActivated, this, [this](const Contact &contact) {
        m_contactPanel->select(contact);
        openChat(contact);
    });
    connect(m_homePage, &HomePage::createGroupClicked, this, [this] {
        statusBar()->showMessage(tr("新建群聊还没做"), 5000);
    });
    auto communityNotReady = [this](const Community &community) {
        statusBar()->showMessage(tr("社区「%1」（社区页还没做）").arg(community.name), 5000);
    };
    connect(m_homePage, &HomePage::communityActivated, this, communityNotReady);
    connect(m_toolRail, &ToolRail::communityClicked, this, communityNotReady);

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
        m_homePage->clear();
        m_chatPage->clear();
        m_center->setCurrentWidget(m_homePage);
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

void MainWindow::openChat(const Contact &contact) {
    m_chatPage->openConversation(contact, m_client->uid());
    m_center->setCurrentWidget(m_chatPage);
}

void MainWindow::showHome() {
    m_contactPanel->clearSelection();
    m_center->setCurrentWidget(m_homePage);
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
        m_homePage->setContacts(contacts);
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
        m_homePage->setCommunities(communities);
        return;
    }

    // 聊天相关消息路由给 ChatPage
    if (type == QStringLiteral("history_response") ||
        type == QStringLiteral("private_chat") ||
        type == QStringLiteral("Group_Chat") ||
        type == QStringLiteral("Channel_Chat")) {
        m_chatPage->handleEnvelope(envelope);
        return;
    }

    // system 消息中与聊天有关的部分路由给 ChatPage，其余显示到状态栏
    if (type == QStringLiteral("system")) {
        const QString text = QString::fromStdString(envelope.content());
        if (text.contains(QLatin1String("Message sent.")) ||
            text.contains(QLatin1String("Failed to load chat history"))) {
            m_chatPage->handleEnvelope(envelope);
            return;
        }
        statusBar()->showMessage(text, 5000);
        return;
    }

    statusBar()->showMessage(tr("收到 %1 类型消息（暂未处理）").arg(type), 5000);
}
