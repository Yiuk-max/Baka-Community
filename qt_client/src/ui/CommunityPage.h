#pragma once

// 社区页（中间区域）：左边频道栏，右边“服务器指南 / 频道聊天”，可按需展开右侧成员面板。
// 频道聊天直接复用 ChatPage（Contact.isChannel = true）；页面只发信号，改名 / 建频道 / 解散等请求由 MainWindow 发。
#include "model/Contact.h"

#include <QList>
#include <QWidget>

namespace chat_proto { class Envelope; }
class AvatarStore;
class ChatClient;
class ChatPage;
class CommunityChannelList;
class CommunityGuidePage;
class CommunityMembersPanel;
class QLabel;
class QPushButton;
class QStackedWidget;

class CommunityPage : public QWidget {
    Q_OBJECT
public:
    explicit CommunityPage(ChatClient *client, AvatarStore *avatars, QWidget *parent = nullptr);

    QWidget *sidebar() const { return m_sidebar; }
    QList<Channel> channels() const { return m_channels; }
    bool hasMember(int uid) const {
        for (const auto &member : m_members) if (member.uid == uid) return true;
        return false;
    }
    // 打开一个社区：拉频道 / 成员，默认打开第一个频道
    void openCommunity(const Community &community, int selfUid);
    // 社区列表刷新时更新名称 / 简介 / 我的身份（同一个社区）
    void updateCommunity(const Community &community);
    const Community &community() const { return m_community; }
    QString categoryOfChannel(int channelId) const;   // 重命名频道时要把原分区带回去
    void clear();
    void setAssets(const QPixmap &avatar, const QPixmap &banner);

    void handleEnvelope(const chat_proto::Envelope &envelope);   // 频道列表 / 成员列表 / 频道消息
    bool handleSystemText(const QString &text);                 // 频道 / 成员变动回执；请求由页面自己补发

signals:
    void channelsChanged();
    void inviteRequested(const Community &community);
    void settingsRequested(const Community &community);
    void leaveRequested(const Community &community);
    void createChannelRequested(const Community &community);
    void deleteChannelRequested(const Community &community, int channelId, const QString &name);
    void renameChannelRequested(const Community &community, int channelId, const QString &name);
    void memberRoleChangeRequested(int uid, bool promote);
    void removeMemberRequested(int uid);

private:
    bool isOwner() const;
    QWidget *buildSidebar();
    void rebuildChannels();          // 频道数据变了：刷新列表 + 统计
    void openChannel(int channelId);
    void showGuide();
    void toggleMembers();
    void showCommunityMenu(const QPoint &position);
    void requestChannels();
    void requestMembers();

    ChatClient   *m_client = nullptr;
    AvatarStore  *m_avatars = nullptr;
    Community     m_community;
    int           m_selfUid = 0;
    QList<Channel> m_channels;
    QList<CommunityMember> m_members;
    int           m_currentChannelId = 0;   // 正在聊的频道（重建列表后要恢复高亮）；0 = 服务器指南
    QList<int>    m_pendingChannels;   // 已发出但还没回的 show_community_channels（按顺序对回执）
    QList<int>    m_pendingMembers;

    QWidget      *m_sidebar = nullptr;
    QLabel       *m_sidebarAvatar = nullptr;
    QLabel       *m_banner = nullptr;
    QLabel       *m_topic = nullptr;
    QLabel       *m_channelTitle = nullptr;
    QPushButton  *m_editGuide = nullptr;
    bool          m_waitingFirstChannel = false;
    QPushButton  *m_sidebarName = nullptr;
    QPushButton  *m_settingsButton = nullptr;
    QPushButton  *m_addChannelButton = nullptr;
    QPushButton  *m_membersButton = nullptr;
    QPushButton  *m_guideButton = nullptr;
    CommunityChannelList  *m_channelList = nullptr;
    CommunityGuidePage    *m_guidePage = nullptr;
    CommunityMembersPanel *m_membersPanel = nullptr;

    QStackedWidget *m_content = nullptr;   // 0 = 服务器指南，1 = 频道聊天
    ChatPage     *m_chat = nullptr;
};
