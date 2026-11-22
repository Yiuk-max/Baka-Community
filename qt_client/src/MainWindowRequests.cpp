// MainWindow 的一部分：设置窗口、社区页操作、邀请好友、入群 / 入社区申请的拉取与审批。
// 单独放一个文件是为了不让 MainWindow.cpp 超过 400 行；类还是同一个 MainWindow。
#include "MainWindow.h"

#include "net/AvatarUploader.h"
#include "net/ChatClient.h"
#include "store/AvatarStore.h"
#include "ui/CommunityPage.h"
#include "ui/ContactPanel.h"
#include "ui/HomePage.h"
#include "ui/InputDialog.h"
#include "ui/InviteDialog.h"
#include "ui/PendingPage.h"
#include "ui/SettingsDialog.h"
#include <QStatusBar>

#include <QRegularExpression>
#include <QStackedWidget>

// ---------------- 社区 ----------------

// 点社区图标 / 主页里的社区行：中间区域切到社区页
void MainWindow::openCommunity(const Community &community) {
    m_contactPanel->showCommunitySidebar(true);
    m_toolRail->setActiveCommunity(community.id);
    m_communityPage->openCommunity(community, m_client->uid());
    m_center->setCurrentWidget(m_communityPage);
    for (int id : {community.avatarId, community.bannerId}) {
        if (id > 0 && !m_communityImages.contains(id)) m_communityImageRequests.remove(id);
    }
    refreshCommunityAssets();
}

void MainWindow::createChannel(const Community &community) {
    QStringList v;
    if (!ask(tr("在「%1」创建频道").arg(community.name),
             {tr("频道名称"), tr("频道分区（可不填）")}, &v)) return;
    chat_proto::Envelope env;
    env.set_type("create_channel");
    env.set_community_id(community.id);
    env.set_group_name(v[0].toStdString());
    env.set_category((v[1].isEmpty() ? tr("一般") : v[1]).toStdString());
    m_client->send(env);
}

// Web 社区设置：概览草稿保存，频道即时增删；权限由服务端校验
// openCommunitySettings 的实现见 MainWindowCommunitySettings.cpp。

// 与 Web 社区菜单一致：离开与设置里的解散是独立操作
void MainWindow::leaveOrDissolve(const Community &community) {
    // Web 菜单的离开始终是 leave_community，不因身份变成删除。
    const QString tip = tr("确定离开「%1」？").arg(community.name);
    if (!ask(tip, {})) return;

    chat_proto::Envelope env;
    env.set_type("leave_community");
    env.set_community_id(community.id);
    m_client->send(env);
}

// 社区页的全部入口接线：页面只发信号，请求统一在这里发
void MainWindow::wireCommunityPage() {
    connect(m_communityPage, &CommunityPage::inviteRequested, this, [this](const Community &community) {
        inviteFriends(tr("邀请好友加入「%1」").arg(community.name), true, community.id);
    });
    connect(m_communityPage, &CommunityPage::settingsRequested, this, [this](const Community &community) {
        openCommunitySettings(community);
    });
    connect(m_communityPage, &CommunityPage::leaveRequested, this, &MainWindow::leaveOrDissolve);
    connect(m_communityPage, &CommunityPage::createChannelRequested, this, [this](const Community &community) {
        openCommunitySettings(community, true);
    });

    connect(m_communityPage, &CommunityPage::renameChannelRequested, this,
            [this](const Community &community, int channelId, const QString &name) {
        QStringList v;
        if (!ask(tr("重命名频道「#%1」").arg(name), {tr("新的频道名称")}, &v)) return;
        if (v[0].isEmpty() || v[0] == name) return;
        chat_proto::Envelope env;
        env.set_type("modify_channel");
        env.set_community_id(community.id);
        env.set_channel_id(channelId);
        env.set_group_name(v[0].toStdString());
        // 服务端会连分区一起覆盖，所以把原来的分区带回去
        env.set_category(m_communityPage->categoryOfChannel(channelId).toStdString());
        m_client->send(env);
    });
    connect(m_communityPage, &CommunityPage::deleteChannelRequested, this,
            [this](const Community &community, int channelId, const QString &name) {
        if (!ask(tr("确定删除频道「#%1」？").arg(name), {})) return;
        chat_proto::Envelope env;
        env.set_type("delete_channel");
        env.set_community_id(community.id);
        env.set_channel_id(channelId);
        m_client->send(env);
    });

    connect(m_communityPage, &CommunityPage::memberRoleChangeRequested, this,
            [this](int uid, bool promote) {
        chat_proto::Envelope env;
        env.set_type("modify_community_member_role");
        env.set_community_id(m_communityPage->community().id);
        env.set_target_user_uid(uid);
        env.set_promote(promote);
        m_client->send(env);
    });
    connect(m_communityPage, &CommunityPage::removeMemberRequested, this, [this](int uid) {
        if (!ask(tr("确定把 UID %1 移出社区？").arg(uid), {})) return;
        chat_proto::Envelope env;
        env.set_type("community_remove_member");
        env.set_community_id(m_communityPage->community().id);
        env.set_target_user_uid(uid);
        m_client->send(env);
    });
}

// 某个人的头像下好了 / 换了 / 被清除了（聊天页里的消息行由 ChatPage 自己刷新）
void MainWindow::refreshAvatar(int uid) {
    const QString path = m_avatars->path(uid);
    if (uid == m_client->uid()) m_contactPanel->setAccountAvatar(path);

    bool isFriend = false;
    for (Contact &contact : m_contacts) {
        if (contact.isGroup || contact.uid != uid) continue;
        contact.avatarPath = path;
        isFriend = true;
    }
    if (!isFriend) return;
    m_contactPanel->setContacts(m_contacts);
    m_homePage->setContacts(m_contacts);
}

// 邀请好友：弹好友列表，点哪个好友的“邀请”就发一次请求（群聊 group_add_client / 社区 community_add_member）
void MainWindow::inviteFriends(const QString &title, bool toCommunity, int targetId) {
    if (toCommunity) {
        for (const auto &community : m_communities) {
            if (community.id == targetId && community.role != QLatin1String("owner")) {
                statusBar()->showMessage(tr("只有社区所有者可以直接添加成员"), 5000);
                return;
            }
        }
    }
    QList<Contact> friends;
    for (const Contact &contact : m_contacts) {
        if (contact.isGroup) continue;
        if (toCommunity && m_communityPage->community().id == targetId
                && m_communityPage->hasMember(contact.uid)) continue;
        friends.append(contact);
    }

    InviteDialog dialog(title, friends, this);
    connect(m_client, &ChatClient::loggedOut, &dialog, &QDialog::reject);   // 被顶下线时把弹窗关掉
    connect(&dialog, &InviteDialog::inviteRequested, this, [this, toCommunity, targetId](int uid) {
        chat_proto::Envelope env;
        if (toCommunity) {
            env.set_type("community_add_member");
            env.set_community_id(targetId);
        } else {
            env.set_type("group_add_client");
            env.set_group_uid(targetId);
        }
        env.set_target_user_uid(uid);
        m_client->send(env);
    });
    dialog.exec();
}

// 每个群各问一次有没有入群申请（不是群主 / 管理员的群，服务端会回一句失败，下面会吞掉）
void MainWindow::requestGroupRequests() {
    m_homePage->pendingPage()->clearJoinRequests(false);
    for (const Contact &contact : m_contacts) {
        if (!contact.isGroup) continue;
        chat_proto::Envelope env;
        env.set_type("show_group_requests");
        env.set_group_uid(contact.uid);
        m_client->send(env);
    }
}

// 社区只有所有者能审批，所以只问自己是 owner 的社区（和 web 端一致）
void MainWindow::requestCommunityRequests() {
    m_homePage->pendingPage()->clearJoinRequests(true);
    for (const Community &community : m_communities) {
        if (community.role != QLatin1String("owner")) continue;
        chat_proto::Envelope env;
        env.set_type("show_community_requests");
        env.set_community_id(community.id);
        m_client->send(env);
    }
}

// show_group_requests / show_community_requests 的回复是文本：
//   第一行带 "... for group [id]" 或 "... for community [id]"，后面每行 "Requester UID: N, Message: 留言"
bool MainWindow::handleJoinRequestList(const QString &text) {
    static const QRegularExpression headRe(QStringLiteral(
        "(?:Pending join requests for|No pending join requests for|Failed to retrieve) "
        "(group|community)(?: join requests)?.*?\\[(\\d+)\\]"));
    static const QRegularExpression lineRe(QStringLiteral("^Requester UID: (\\d+), Message:\\s*(.*)$"));

    const QRegularExpressionMatch head = headRe.match(text);
    if (!head.hasMatch()) {
        // 没带 id 的失败回复（比如不是群主）：不是给人看的，吞掉
        return text.contains(QLatin1String("Failed to retrieve group join requests"))
            || text.contains(QLatin1String("Failed to retrieve community join requests"));
    }

    const bool isCommunity = head.captured(1) == QLatin1String("community");
    const int targetId = head.captured(2).toInt();
    QString targetName = QString::number(targetId);
    if (isCommunity) {
        for (const Community &community : m_communities)
            if (community.id == targetId) targetName = community.name;
    } else {
        for (const Contact &contact : m_contacts)
            if (contact.isGroup && contact.uid == targetId) targetName = contact.name;
    }

    QList<JoinRequest> requests;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = lineRe.match(line.trimmed());
        if (!m.hasMatch()) continue;
        JoinRequest request;
        request.isCommunity = isCommunity;
        request.targetId = targetId;
        request.targetName = targetName;
        request.uid = m.captured(1).toInt();
        request.note = m.captured(2);
        requests.append(request);
    }
    m_homePage->pendingPage()->setJoinRequests(isCommunity, targetId, requests);
    return true;
}

// “待定”页点了 接受 / 拒绝。服务端回 "Join request accepted / rejected"，refreshAfter 看到后会重新拉申请列表
void MainWindow::handleJoinDecision(const JoinRequest &request, bool accept) {
    chat_proto::Envelope env;
    if (request.isCommunity) {
        env.set_type("handle_community_join_request");
        env.set_community_id(request.targetId);
    } else {
        env.set_type("handle_join_request");
        env.set_group_uid(request.targetId);
    }
    env.set_requester_uid(request.uid);
    env.set_accept(accept);
    m_client->send(env);
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

    // 头像：选图裁剪 → 上传文件 → set_avatar → 本地缓存换成新头像
    dialog.setAvatar(m_avatars->path(m_client->uid()));
    auto *uploader = new AvatarUploader(m_client, &dialog);   // 跟着设置窗口一起销毁
    connect(&dialog, &SettingsDialog::avatarUploadRequested, uploader, &AvatarUploader::upload);
    connect(uploader, &AvatarUploader::failed, &dialog, &SettingsDialog::showAvatarError);
    connect(uploader, &AvatarUploader::finished, m_client, &ChatClient::setAvatar);   // 拿到 file.id 再绑定
    connect(m_client, &ChatClient::avatarUpdated, &dialog, [&dialog](int) {
        dialog.avatarUploadDone();   // 本地缓存由构造函数里那条连接负责刷新
    });
    connect(m_client, &ChatClient::avatarUpdateFailed, &dialog, &SettingsDialog::showAvatarError);
    connect(m_avatars, &AvatarStore::avatarChanged, &dialog, [this, &dialog](int uid) {
        if (uid == m_client->uid()) dialog.setAvatar(m_avatars->path(uid));
    });

    if (dialog.exec() == SettingsDialog::Logout) m_client->logout();
}
