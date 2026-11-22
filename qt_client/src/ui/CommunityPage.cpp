#include "CommunityPage.h"

#include "Assets.h"
#include "Avatar.h"
#include "ChatPage.h"
#include "CommunityChannelList.h"
#include "CommunityGuidePage.h"
#include "CommunityMembersPanel.h"
#include "net/ChatClient.h"
#include "store/AvatarStore.h"
#include "message.pb.h"

#include "MenuStyle.h"
#include <QMenu>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <utility>

CommunityPage::CommunityPage(ChatClient *client, AvatarStore *avatars, MediaStore *media, QWidget *parent)
    : QWidget(parent), m_client(client), m_avatars(avatars) {
    setObjectName("communityPage");
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_sidebar = buildSidebar();   // ContactPanel 原位切换侧栏，账号栏不移动
    auto *main = new QVBoxLayout;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);
    auto *header = new QWidget;
    header->setObjectName("communityChatHeader");
    header->setFixedHeight(48);
    auto *bar = new QHBoxLayout(header);
    bar->setContentsMargins(16, 0, 16, 0);
    m_channelTitle = new QLabel(tr("暂无频道"));
    m_channelTitle->setObjectName("settingsTitle");
    bar->addWidget(m_channelTitle);
    m_topic = new QLabel;
    m_topic->setObjectName("communityGuideDesc");
    m_topic->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_topic, 1);
    m_editGuide = new QPushButton(tr("编辑"));
    m_editGuide->setObjectName("communityIconButton");
    bar->addWidget(m_editGuide);
    m_membersButton = new QPushButton(tr("成员"));
    m_membersButton->setObjectName("communityIconButton");
    m_membersButton->setCheckable(true);
    bar->addWidget(m_membersButton);
    connect(m_membersButton, &QPushButton::clicked, this, &CommunityPage::toggleMembers);
    connect(m_editGuide, &QPushButton::clicked, this, [this] { emit settingsRequested(m_community); });
    main->addWidget(header);

    m_content = new QStackedWidget(this);
    m_guidePage = new CommunityGuidePage;
    m_content->addWidget(m_guidePage);
    m_chat = new ChatPage(m_client, m_avatars, media, this);
    m_content->addWidget(m_chat);
    m_chat->setHeaderVisible(false);
    main->addWidget(m_content, 1);
    layout->addLayout(main, 1);

    m_membersPanel = new CommunityMembersPanel(m_avatars, this);
    m_membersPanel->hide();
    layout->addWidget(m_membersPanel);

    // 服务器指南页上的按钮 → 页面自己的信号（请求由 MainWindow 发）
    connect(m_guidePage, &CommunityGuidePage::inviteRequested, this, [this] {
        if (m_community.id > 0) emit inviteRequested(m_community);
    });
    connect(m_guidePage, &CommunityGuidePage::settingsRequested, this, [this] {
        if (m_community.id > 0) emit settingsRequested(m_community);
    });
    connect(m_guidePage, &CommunityGuidePage::leaveRequested, this, [this] {
        if (m_community.id > 0) emit leaveRequested(m_community);
    });
    connect(m_membersPanel, &CommunityMembersPanel::closed, this, &CommunityPage::toggleMembers);
    connect(m_membersPanel, &CommunityMembersPanel::roleChangeRequested, this,
            &CommunityPage::memberRoleChangeRequested);
    connect(m_membersPanel, &CommunityMembersPanel::removeRequested, this,
            &CommunityPage::removeMemberRequested);
}

QWidget *CommunityPage::buildSidebar() {
    auto *sidebar = new QWidget;
    sidebar->setObjectName("communitySidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ---- 顶部：社区头像 + 名称 + 新建频道 / 设置 ----
    auto *header = new QWidget;
    header->setObjectName("communitySidebarHeader");
    header->setFixedHeight(56);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 8, 0);
    headerLayout->setSpacing(8);

    m_sidebarAvatar = new QLabel;
    m_sidebarAvatar->setFixedSize(36, 36);
    m_sidebarAvatar->setPixmap(avatar::rounded(assets::defaultCommunityAvatar(), 36, 11));
    headerLayout->addWidget(m_sidebarAvatar);

    m_sidebarName = new QPushButton;
    m_sidebarName->setObjectName("communitySidebarTitleButton");
    m_sidebarName->setCursor(Qt::PointingHandCursor);
    m_sidebarName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerLayout->addWidget(m_sidebarName, 1);

    m_addChannelButton = new QPushButton(tr("邀请"));
    m_addChannelButton->setObjectName("communityIconButton");
    m_addChannelButton->setFixedSize(42, 28);
    m_addChannelButton->setCursor(Qt::PointingHandCursor);
    m_addChannelButton->setToolTip(tr("邀请至社区"));
    headerLayout->addWidget(m_addChannelButton);

    m_settingsButton = new QPushButton(QStringLiteral("v"));
    m_settingsButton->setObjectName("communityIconButton");
    m_settingsButton->setFixedSize(22, 22);
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip(tr("社区设置"));
    headerLayout->addWidget(m_settingsButton);
    layout->addWidget(header);
    m_banner = new QLabel;
    m_banner->setFixedHeight(87);
    m_banner->setScaledContents(true);
    m_banner->setPixmap(QPixmap(assets::defaultBackground()));
    layout->addWidget(m_banner);

    // Web 侧栏只有服务器指南，成员入口放在聊天顶部
    auto *nav = new QVBoxLayout;
    nav->setContentsMargins(8, 6, 8, 6);
    nav->setSpacing(2);

    m_guideButton = new QPushButton(tr("服务器指南"));
    m_guideButton->setObjectName("communityNavRow");
    m_guideButton->setCheckable(true);
    m_guideButton->setChecked(true);
    m_guideButton->setFixedHeight(32);
    m_guideButton->setCursor(Qt::PointingHandCursor);
    nav->addWidget(m_guideButton);
    layout->addLayout(nav);

    // ---- 频道列表 ----
    m_channelList = new CommunityChannelList;
    layout->addWidget(m_channelList, 1);

    connect(m_guideButton, &QPushButton::clicked, this, &CommunityPage::showGuide);
    connect(m_channelList, &CommunityChannelList::channelActivated, this,
            &CommunityPage::openChannel);
    connect(m_channelList, &CommunityChannelList::renameRequested, this,
            [this](int channelId, const QString &name) {
        if (m_community.id > 0) emit renameChannelRequested(m_community, channelId, name);
    });
    connect(m_channelList, &CommunityChannelList::deleteRequested, this,
            [this](int channelId, const QString &name) {
        if (m_community.id > 0) emit deleteChannelRequested(m_community, channelId, name);
    });
    connect(m_addChannelButton, &QPushButton::clicked, this, [this] {
        if (m_community.id > 0) emit inviteRequested(m_community);
    });
    connect(m_settingsButton, &QPushButton::clicked, this, [this] {
        showCommunityMenu(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
    });
    connect(m_sidebarName, &QPushButton::clicked, this, [this] {
        showCommunityMenu(m_sidebarName->mapToGlobal(QPoint(0, m_sidebarName->height())));
    });
    m_sidebarName->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sidebarName, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showCommunityMenu(m_sidebarName->mapToGlobal(pos));
    });
    connect(m_channelList, &CommunityChannelList::createRequested, this, [this] {
        emit createChannelRequested(m_community);
    });
    return sidebar;
}

bool CommunityPage::isOwner() const {
    return m_community.role == QLatin1String("owner");
}

QString CommunityPage::categoryOfChannel(int channelId) const {
    for (const Channel &channel : m_channels) {
        if (channel.id == channelId)
            return channel.category.trimmed().isEmpty() ? tr("一般") : channel.category.trimmed();
    }
    return tr("一般");
}

void CommunityPage::openCommunity(const Community &community, int selfUid) {
    m_community = community;
    m_selfUid = selfUid;
    m_currentChannelId = 0;
    m_channels.clear();
    m_members.clear();
    m_chat->clear();

    m_sidebarName->setText(community.name);
    m_settingsButton->show();
    m_membersButton->setText(tr("成员"));
    m_membersPanel->clear();
    rebuildChannels();
    showGuide();
    m_waitingFirstChannel = true;
    m_channelTitle->setText(tr("暂无频道"));
    m_content->setCurrentWidget(m_chat);
    m_editGuide->hide();
    m_membersButton->show();
    m_guideButton->setChecked(false);

    requestChannels();
    requestMembers();
}

void CommunityPage::updateCommunity(const Community &community) {
    if (community.id != m_community.id) return;
    m_community = community;
    m_sidebarName->setText(community.name);
    m_settingsButton->show();
    m_topic->setText(community.description);
    rebuildChannels();   // 身份可能变了（升 / 降管理员），列表要跟着换右键菜单
}

void CommunityPage::clear() {
    m_community = {};
    m_currentChannelId = 0;
    m_channels.clear();
    m_members.clear();
    m_pendingChannels.clear();
    m_pendingMembers.clear();
    m_chat->clear();
    m_membersPanel->clear();
    m_membersPanel->hide();
    m_sidebarName->setText({});
    m_settingsButton->hide();
    m_membersButton->setChecked(false);
    rebuildChannels();
    showGuide();
}

void CommunityPage::rebuildChannels() {
    // 正在聊的频道被删了：聊天页清空，退回服务器指南
    if (m_currentChannelId > 0) {
        bool exists = false;
        for (const Channel &channel : std::as_const(m_channels)) {
            if (channel.id == m_currentChannelId) { exists = true; break; }
        }
        if (!exists) {
            m_chat->clear();
            m_currentChannelId = 0;
            if (!m_channels.isEmpty()) openChannel(m_channels.first().id);
            else {
                m_chat->clear();
                m_channelTitle->setText(tr("暂无频道"));
                m_content->setCurrentWidget(m_chat);
                m_guideButton->setChecked(false);
            }
        }
    }
    m_channelList->setChannels(m_channels, isOwner());
    m_channelList->setActive(m_currentChannelId);   // 列表重建后把正在聊的频道继续高亮
    m_guidePage->setCommunity(m_community, m_members.size(), m_channels.size());
}

void CommunityPage::openChannel(int channelId) {
    for (const Channel &channel : std::as_const(m_channels)) {
        if (channel.id != channelId) continue;
        Contact contact;
        contact.uid = channel.id;
        contact.name = channel.name;
        contact.isChannel = true;
        m_chat->openConversation(contact, m_selfUid);
        m_content->setCurrentWidget(m_chat);
        m_currentChannelId = channelId;
        m_channelList->setActive(channelId);
        m_channelTitle->setText(QStringLiteral("# %1").arg(channel.name));
        m_topic->setText(m_community.description);
        m_editGuide->hide();
        m_membersButton->show();
        m_waitingFirstChannel = false;
        m_guideButton->setChecked(false);
        return;
    }
}

void CommunityPage::showGuide() {
    m_content->setCurrentWidget(m_guidePage);
    m_channelTitle->setText(tr("服务器指南"));
    m_topic->clear();
    m_editGuide->show();
    m_membersButton->hide();
    m_waitingFirstChannel = false;
    m_currentChannelId = 0;
    m_channelList->setActive(-1);
    m_guideButton->setChecked(true);
}

void CommunityPage::setAssets(const QPixmap &avatar, const QPixmap &banner) {
    m_sidebarAvatar->setPixmap(avatar.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_banner->setPixmap(banner);
    m_guidePage->setAssets(avatar, banner);
}

void CommunityPage::toggleMembers() {
    const bool show = !m_membersPanel->isVisible();
    m_membersPanel->setVisible(show);
    m_membersButton->setChecked(show);
}

void CommunityPage::requestChannels() {
    if (m_community.id <= 0) return;
    m_pendingChannels.append(m_community.id);
    chat_proto::Envelope env;
    env.set_type("show_community_channels");
    env.set_community_id(m_community.id);
    m_client->send(env);
}

void CommunityPage::requestMembers() {
    if (m_community.id <= 0) return;
    m_pendingMembers.append(m_community.id);
    chat_proto::Envelope env;
    env.set_type("show_community_members");
    env.set_community_id(m_community.id);
    m_client->send(env);
}

void CommunityPage::handleEnvelope(const chat_proto::Envelope &envelope) {
    const QString type = QString::fromStdString(envelope.type());

    if (type == QStringLiteral("channel_list_response")) {
        // 回执里没有 community_id（空列表时）就按请求顺序对
        const int requested = m_pendingChannels.isEmpty() ? 0 : m_pendingChannels.takeFirst();
        int communityId = requested;
        if (envelope.channels_size() > 0) communityId = envelope.channels(0).community_id();
        if (communityId != m_community.id) return;

        m_channels.clear();
        for (const chat_proto::ChannelInfo &info : envelope.channels()) {
            Channel channel;
            channel.id = info.channel_id();
            channel.communityId = info.community_id();
            channel.name = QString::fromStdString(info.name());
            channel.category = QString::fromStdString(info.category());
            m_channels.append(channel);
        }
        const bool selectFirst = m_waitingFirstChannel;
        rebuildChannels();
        if (selectFirst && !m_channels.isEmpty()) {
            m_waitingFirstChannel = false;
            openChannel(m_channels.first().id);
        }
        emit channelsChanged();
        return;
    }

    if (type == QStringLiteral("community_member_list_response")) {
        const int requested = m_pendingMembers.isEmpty() ? 0 : m_pendingMembers.takeFirst();
        if (requested == 0 || requested != m_community.id) return;

        m_members.clear();
        for (const chat_proto::MemberInfo &info : envelope.members()) {
            CommunityMember member;
            member.uid = info.user_uid();
            member.name = QString::fromStdString(info.nickname());
            if (member.name.isEmpty()) member.name = QString::number(member.uid);
            member.role = QString::fromStdString(info.role());
            member.avatarId = info.avatar_id();
            member.avatarPath = member.avatarId > 0 ? m_avatars->path(member.uid) : QString();
            m_members.append(member);
        }
        m_membersPanel->setMembers(m_members, m_selfUid, isOwner());
        m_membersButton->setText(tr("成员 · %1").arg(m_members.size()));
        m_guidePage->setCommunity(m_community, m_members.size(), m_channels.size());
        return;
    }

    // 聊天相关（历史 / 频道推送 / 撤回）交给频道聊天页
    if (type == QStringLiteral("history_response") || type == QStringLiteral("Channel_Chat")
            || type == QStringLiteral("delete_message")) {
        m_chat->handleEnvelope(envelope);
    }
}


