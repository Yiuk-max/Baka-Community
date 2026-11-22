#include "MainWindow.h"

#include "net/ChatClient.h"
#include "net/FileDownloader.h"
#include "store/AvatarStore.h"
#include "ui/ChatPage.h"
#include "ui/CommunityPage.h"
#include "ui/ContactPanel.h"
#include "ui/HomePage.h"
#include "ui/InputDialog.h"
#include "ui/PendingPage.h"
#include "ui/SoundPlayer.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTime>
#include <QTimer>
#include <QWidget>
MainWindow::MainWindow(ChatClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client) {
    setWindowTitle(tr("Baka Community"));
    resize(1100, 700);

    m_sound = new SoundPlayer(this);
    m_avatars = new AvatarStore(new FileDownloader(m_client, this), this);
    connect(m_avatars, &AvatarStore::avatarChanged, this, &MainWindow::refreshAvatar);
    initializeAttachments();

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
    m_chatPage = new ChatPage(m_client, m_avatars, m_media, m_center);
    m_communityPage = new CommunityPage(m_client, m_avatars, m_media, m_center);
    m_center->addWidget(m_homePage);
    m_center->addWidget(m_chatPage);
    m_center->addWidget(m_communityPage);
    m_contactPanel->setCommunitySidebar(m_communityPage->sidebar());
    layout->addWidget(m_center, 1);   // 伸展因子1，剩余宽度全给中间

    setCentralWidget(wrapAttachments(central));

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
    connect(m_homePage, &HomePage::createGroupClicked, this, &MainWindow::createGroup);
    // “待定”页处理好友申请：发 accept_friend / reject_friend，稍后重新拉申请列表和通讯录
    connect(m_homePage->pendingPage(), &PendingPage::friendRequestHandled, this, [this](int uid, bool accept) {
        chat_proto::Envelope env;
        env.set_type(accept ? "accept_friend" : "reject_friend");
        env.set_sender_uid(uid);
        m_client->send(env);
        refreshAfter(QStringLiteral("Failed to handle friend request"));   // 借用同一条刷新路径
    });
    // “待定”页审批入群 / 入社区申请
    connect(m_homePage->pendingPage(), &PendingPage::joinRequestHandled, this, &MainWindow::handleJoinDecision);
    // 通讯录右键群聊 → 邀请好友：弹好友列表
    connect(m_contactPanel, &ContactPanel::inviteRequested, this, [this](const Contact &group) {
        inviteFriends(tr("邀请好友加入「%1」").arg(group.name), false, group.uid);
    });
    connect(m_toolRail, &ToolRail::addActionTriggered, this, &MainWindow::handleAddAction);
    connect(m_toolRail, &ToolRail::communityActionTriggered, this, &MainWindow::handleCommunityAction);
    // 点社区图标 / 主页里的社区行：打开社区页
    connect(m_homePage, &HomePage::communityActivated, this, &MainWindow::openCommunity);
    connect(m_toolRail, &ToolRail::communityClicked, this, &MainWindow::openCommunity);
    wireCommunityPage();      // 社区页的入口 / 频道 / 成员操作（实现见 MainWindowRequests.cpp）
    wireCommunityAssets();
    connect(m_client, &ChatClient::loginSucceeded, this, [this] {
        statusBar()->showMessage(tr("已登录 %1（UID %2）/ 服务器 %3")
                                     .arg(m_client->username())
                                     .arg(m_client->uid())
                                     .arg(m_client->serverUrl()));
        m_contactPanel->setAccount(m_client->username(), m_client->uid());
        m_contactPanel->setOnline(true);
        // 切到这个账号的本地文件夹，先核对自己的头像
        m_avatars->setUser(m_client->uid());
        m_avatars->ensure(m_client->uid(), m_client->avatarId());
        m_contactPanel->setAccountAvatar(m_avatars->path(m_client->uid()));
        requestLists();
    });

    // 头像换成新的：把本地缓存指向新的 file.id（下好后 avatarChanged 会刷新界面）
    connect(m_client, &ChatClient::avatarUpdated, this, [this](int fileId) {
        m_avatars->ensure(m_client->uid(), fileId);
    });

    // 退出登录 / 被顶下线：上一个账号的联系人和社区不能留给下一个账号看到
    connect(m_client, &ChatClient::loggedOut, this, [this] {
        m_contactPanel->clear();
        m_toolRail->setCommunities({});
        m_homePage->clear();
        m_contacts.clear();
        m_communities.clear();
        m_communityImages.clear();
        m_communityImageRequests.clear();
        m_chatPage->clear();
        m_communityPage->clear();
        m_avatars->setUser(0);
        showHome();
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

void MainWindow::request(const char *type) {
    chat_proto::Envelope env;
    env.set_type(type);
    m_client->send(env);
}

void MainWindow::requestLists() {
    request("show_contacts");
    request("show_communities");
    request("show_friend_requests");
}

bool MainWindow::ask(const QString &title, const QStringList &labels, QStringList *values) {
    InputDialog dialog(title, labels, this);
    connect(m_client, &ChatClient::loggedOut, &dialog, &QDialog::reject);   // 被顶下线时把弹窗关掉
    if (dialog.exec() != QDialog::Accepted) return false;
    if (values) {
        values->clear();
        for (int i = 0; i < labels.size(); ++i) values->append(dialog.value(i));
    }
    return true;
}

void MainWindow::createGroup() {
    QStringList v;
    if (!ask(tr("创建群聊"), {tr("群聊名称")}, &v)) return;
    chat_proto::Envelope env;
    env.set_type("create_group");
    env.set_group_name(v[0].toStdString());
    m_client->send(env);
}

// 工具栏“＋”菜单。请求发出去之后，成功 / 失败都由服务端的 system 文本告知（显示在状态栏）
void MainWindow::handleAddAction(ToolRail::AddAction action) {
    chat_proto::Envelope env;
    QStringList v;
    switch (action) {
    case ToolRail::AddAction::CreateGroup:
        createGroup();
        return;
    case ToolRail::AddAction::CreateCommunity:
        if (!ask(tr("创建社区"), {tr("社区名称"), tr("社区简介（可不填）")}, &v)) return;
        env.set_type("create_community");
        env.set_group_name(v[0].toStdString());
        env.set_description(v[1].toStdString());
        break;
    case ToolRail::AddAction::JoinCommunity:
        if (!ask(tr("申请加入社区"), {tr("社区 ID"), tr("申请留言（可不填）")}, &v)) return;
        env.set_type("join_community");
        env.set_community_id(v[0].toInt());
        env.set_apply_message(v[1].toStdString());
        break;
    case ToolRail::AddAction::JoinGroup:
        if (!ask(tr("申请加入群聊"), {tr("群聊 UID")}, &v)) return;
        env.set_type("send_join_group");
        env.set_group_uid(v[0].toInt());
        break;
    case ToolRail::AddAction::AddFriend:
        if (!ask(tr("添加好友"), {tr("对方邮箱"), tr("验证消息（可不填）")}, &v)) return;
        env.set_type("add_friend");
        env.set_email(v[0].toStdString());
        env.set_apply_message(v[1].toStdString());
        break;
    }
    // 社区 ID / 群 UID 必须是正整数
    if ((action == ToolRail::AddAction::JoinCommunity || action == ToolRail::AddAction::JoinGroup)
            && v[0].toInt() <= 0) {
        statusBar()->showMessage(tr("ID 必须是数字"), 5000);
        return;
    }
    m_client->send(env);
}

void MainWindow::handleCommunityAction(const Community &community, ToolRail::CommunityAction action) {
    switch (action) {
    case ToolRail::CommunityAction::Invite:
        inviteFriends(tr("邀请好友加入「%1」").arg(community.name), true, community.id);
        return;
    case ToolRail::CommunityAction::CreateChannel:
        openCommunitySettings(community, true);
        return;
    case ToolRail::CommunityAction::Settings:
        openCommunitySettings(community);
        return;
    case ToolRail::CommunityAction::Leave:
        leaveOrDissolve(community);
        return;
    }
}

// show_friend_requests 的回复是一段文本，每行 "[昵称] (UID): 留言"
bool MainWindow::handleFriendRequestList(const QString &text) {
    static const QRegularExpression headRe(QStringLiteral(
        "^\\[.*?\\] \\(\\d+\\):|No pending friend requests|Failed to load friend requests"));
    static const QRegularExpression lineRe(QStringLiteral("^\\[(.*?)\\] \\((\\d+)\\)(?::\\s*(.*))?$"));
    if (!headRe.match(text).hasMatch()) return false;

    QList<FriendRequest> requests;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = lineRe.match(line.trimmed());
        if (m.hasMatch()) requests.append({m.captured(2).toInt(), m.captured(1), m.captured(3)});
    }
    m_homePage->pendingPage()->setFriendRequests(requests);
    return true;
}

// 服务端没有“列表变了”的专门推送，只能看 system 文本里的关键字（和 web 端 backendClient.js 同一组）
void MainWindow::refreshAfter(const QString &text) {
    static const QRegularExpression friendsRe(QStringLiteral(
        "wants to be friend with you|You are now friends with|rejected your friend request"
        "|removed from .*friend list|Failed to handle friend request"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression groupsRe(QStringLiteral(
        "Group created|Group deleted|Group name updated|added to group|removed from group"));
    static const QRegularExpression communitiesRe(QStringLiteral(
        "Community created|Community deleted|Community updated|added to community|left community|removed from community"));

    const bool friends = friendsRe.match(text).hasMatch();
    const bool contacts = friends || groupsRe.match(text).hasMatch();
    const bool communities = communitiesRe.match(text).hasMatch();
    // 审批完一条申请：重新拉一遍申请列表
    if (text.contains(QLatin1String("Join request accepted"))
            || text.contains(QLatin1String("Join request rejected"))) {
        QTimer::singleShot(150, this, [this] {
            if (m_client->uid() == 0) return;
            requestGroupRequests();
            requestCommunityRequests();
        });
    }
    if (!contacts && !communities) return;
    // 稍等一下再拉，让服务端先把这次改动落库
    QTimer::singleShot(200, this, [this, friends, contacts, communities] {
        if (m_client->uid() == 0) return;   // 这期间已经退出登录
        if (friends) request("show_friend_requests");
        if (contacts) request("show_contacts");
        if (communities) request("show_communities");
    });
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
            // 好友头像：本地的 file.id 和这里给的不一样（或本地没有）就重新下载。群没有头像
            if (!contact.isGroup) {
                m_avatars->ensure(contact.uid, contact.avatarId);
                contact.avatarPath = m_avatars->path(contact.uid);   // 还没下好时是空的，下好后 refreshAvatar 会补上
            }
            contacts.append(contact);
        }
        m_contactPanel->setContacts(contacts);
        m_homePage->setContacts(contacts);
        m_contacts = contacts;
        requestGroupRequests();
        return;
    }

    if (type == QStringLiteral("community_list_response")) {
        QList<Community> communities;
        for (const chat_proto::CommunityInfo &info : envelope.communities()) {
            Community community;
            community.id = info.community_id();
            community.name = QString::fromStdString(info.name());
            community.description = QString::fromStdString(info.description());
            community.avatarId = info.avatar_id();
            community.bannerId = info.banner_id();
            community.role = QString::fromStdString(info.my_role());
            communities.append(community);
        }
        m_toolRail->setCommunities(communities);
        m_homePage->setCommunities(communities);
        m_communities = communities;
        // 开着的社区页跟着刷新；如果这个社区已经不在了（解散 / 被移除），退回主页
        const int openId = m_communityPage->community().id;
        if (openId > 0) {
            bool stillThere = false;
            for (const Community &community : std::as_const(communities)) {
                if (community.id != openId) continue;
                m_communityPage->updateCommunity(community);
                stillThere = true;
            }
            if (!stillThere) {
                m_communityPage->clear();
                showHome();
            }
        }
        refreshCommunityAssets();
        requestCommunityRequests();
        return;
    }

    // 社区页自己的列表 / 成员回复
    if (type == QStringLiteral("channel_list_response") ||
        type == QStringLiteral("community_member_list_response")) {
        m_communityPage->handleEnvelope(envelope);
        return;
    }

    // 聊天相关消息路由给 ChatPage（私聊 / 群）和社区页里的频道聊天页
    if (type == QStringLiteral("history_response") ||
        type == QStringLiteral("private_chat") ||
        type == QStringLiteral("Group_Chat") ||
        type == QStringLiteral("Channel_Chat") ||
        type == QStringLiteral("delete_message")) {
        m_chatPage->handleEnvelope(envelope);
        m_communityPage->handleEnvelope(envelope);
        return;
    }

    // system 文本：先给两个聊天页（发送确认 / 撤回），再看是不是申请列表，其余显示到状态栏
    if (type == QStringLiteral("system")) {
        const QString text = QString::fromStdString(envelope.content()).trimmed();
        if (m_chatPage->handleSystemText(text)) return;
        if (m_communityPage->handleSystemText(text)) return;
        if (handleFriendRequestList(text)) return;
        if (handleJoinRequestList(text)) return;
        refreshAfter(text);
        statusBar()->showMessage(text, 5000);
        return;
    }

    statusBar()->showMessage(tr("收到 %1 类型消息（暂未处理）").arg(type), 5000);
}
