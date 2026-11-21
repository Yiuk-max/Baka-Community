// MainWindow 的一部分：邀请好友、入群 / 入社区申请的拉取与审批。
// 单独放一个文件是为了不让 MainWindow.cpp 超过 400 行；类还是同一个 MainWindow。
#include "MainWindow.h"

#include "net/ChatClient.h"
#include "ui/HomePage.h"
#include "ui/InviteDialog.h"
#include "ui/PendingPage.h"

#include <QRegularExpression>

// 邀请好友：弹好友列表，点哪个好友的“邀请”就发一次请求（群聊 group_add_client / 社区 community_add_member）
void MainWindow::inviteFriends(const QString &title, bool toCommunity, int targetId) {
    QList<Contact> friends;
    for (const Contact &contact : m_contacts) {
        if (!contact.isGroup) friends.append(contact);
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
