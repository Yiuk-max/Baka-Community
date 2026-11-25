// CommunityPage 的文本回执处理，与界面构建分开放置。
#include "CommunityPage.h"
#include "ChatPage.h"
#include <QRegularExpression>
#include <QTimer>
#include "MenuStyle.h"
#include <QMenu>

void CommunityPage::showCommunityMenu(const QPoint &position) {
    const Community target = m_community;
    QMenu menu(this);
    menus::prepare(&menu);
    auto *invite = menu.addAction(tr("邀请至社区"));
    auto *settings = menu.addAction(tr("社区设置"));
    menu.addSeparator();
    auto *create = menu.addAction(tr("创建频道"));
    menu.addSeparator();
    auto *leave = menu.addAction(tr("离开社区"));
    QAction *chosen = menu.exec(position);
    if (chosen == invite) emit inviteRequested(target);
    else if (chosen == settings) emit settingsRequested(target);
    else if (chosen == create) emit createChannelRequested(target);
    else if (chosen == leave) emit leaveRequested(target);
}

bool CommunityPage::handleSystemText(const QString &text) {
    if (m_chat->handleSystemText(text)) return true;
    if (m_community.id <= 0) return false;
    if (text.contains(QLatin1String("Failed to load channels"))) {
        if (!m_pendingChannels.isEmpty()) m_pendingChannels.takeFirst();
    } else if (text.contains(QLatin1String("Failed to load members"))) {
        if (!m_pendingMembers.isEmpty()) m_pendingMembers.takeFirst();
    }
    // 频道 / 成员变动：稍等一下再拉一次（服务端先落库）
    static const QRegularExpression channelChanged(
        QStringLiteral("Channel created successfully|Channel deleted successfully|Channel updated successfully"));
    static const QRegularExpression membersChanged(QStringLiteral(
        "Role of UID \\[\\d+\\] updated successfully|Member \\[\\d+\\] removed from community"
        "|added to community \\[\\d+\\]|You have been added to community \\[\\d+\\]"
        "|Join request accepted"));
    if (channelChanged.match(text).hasMatch()) {
        QTimer::singleShot(200, this, [this] { if (m_community.id > 0) requestChannels(); });
    } else if (membersChanged.match(text).hasMatch()) {
        QTimer::singleShot(200, this, [this] { if (m_community.id > 0) requestMembers(); });
    }
    return false;
}
