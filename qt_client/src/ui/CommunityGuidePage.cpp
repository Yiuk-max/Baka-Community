#include "CommunityGuidePage.h"

#include "Assets.h"
#include "Avatar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

CommunityGuidePage::CommunityGuidePage(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setObjectName("communityGuideScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *page = new QWidget;
    page->setObjectName("communityGuide");
    page->setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 32);
    layout->setSpacing(0);

    m_banner = new QLabel;
    m_banner->setFixedHeight(180);
    m_banner->setScaledContents(true);
    m_banner->setPixmap(QPixmap(assets::defaultBackground()));
    layout->addWidget(m_banner);
    layout->addSpacing(16);

    m_avatar = new QLabel;
    m_avatar->setFixedSize(96, 96);
    m_avatar->setPixmap(avatar::rounded(assets::defaultCommunityAvatar(), 96, 24));
    layout->addWidget(m_avatar);
    layout->addSpacing(18);

    m_name = new QLabel;
    m_name->setObjectName("communityGuideName");
    m_name->setWordWrap(true);
    layout->addWidget(m_name);
    layout->addSpacing(8);

    m_description = new QLabel;
    m_description->setObjectName("communityGuideDesc");
    m_description->setWordWrap(true);
    layout->addWidget(m_description);
    layout->addSpacing(18);

    m_stats = new QLabel;
    m_stats->setObjectName("communityGuideStats");
    layout->addWidget(m_stats);
    layout->addSpacing(24);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    auto *invite = new QPushButton(tr("邀请好友"));
    invite->setObjectName("primaryButton");
    invite->setFixedHeight(34);
    invite->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(invite);

    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();

    scroll->setWidget(page);
    outer->addWidget(scroll);

    connect(invite, &QPushButton::clicked, this, &CommunityGuidePage::inviteRequested);
}

void CommunityGuidePage::setCommunity(const Community &community, int memberCount,
                                      int channelCount) {
    m_name->setText(community.name);
    m_description->setText(community.description.isEmpty() ? tr("欢迎来到我们的社区，一起交流分享吧。")
                                                           : community.description);
    Q_UNUSED(channelCount);
    m_stats->setText(tr("%1 位成员").arg(memberCount));
}

void CommunityGuidePage::setAssets(const QPixmap &avatar, const QPixmap &banner) {
    m_avatar->setPixmap(avatar.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_banner->setPixmap(banner);
}
