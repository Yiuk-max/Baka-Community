#include "PendingPage.h"

#include "Assets.h"
#include "Avatar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
const int kAvatarSize = 32;
const int kRowHeight = 56;

// 一行申请：头像 + 两行文字 + 两个按钮。按钮样式复用设置页的灰色小按钮和主按钮
QWidget *makeRow(const QString &avatarPath, const QString &title, const QString &note,
                 const QString &rejectText, QPushButton **rejectButton, QPushButton **acceptButton) {
    auto *row = new QWidget;
    row->setFixedHeight(kRowHeight);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 0, 14, 0);
    layout->setSpacing(12);

    auto *avatarLabel = new QLabel;
    avatarLabel->setPixmap(avatar::rounded(avatarPath, kAvatarSize, kAvatarSize / 2));
    layout->addWidget(avatarLabel);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addStretch();
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("homeRowName");
    titleLabel->setTextFormat(Qt::PlainText);
    text->addWidget(titleLabel);
    auto *noteLabel = new QLabel(note);
    noteLabel->setObjectName("homeHint");
    noteLabel->setTextFormat(Qt::PlainText);
    noteLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 太长就截断
    text->addWidget(noteLabel);
    text->addStretch();
    layout->addLayout(text, 1);

    *rejectButton = new QPushButton(rejectText);
    (*rejectButton)->setObjectName("settingsRowButton");
    *acceptButton = new QPushButton(QObject::tr("接受"));
    (*acceptButton)->setObjectName("primaryButton");
    for (QPushButton *button : {*rejectButton, *acceptButton}) {
        button->setFixedSize(64, 32);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(button);
    }
    return row;
}

void clearRows(QVBoxLayout *rows) {
    while (QLayoutItem *item = rows->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}
}

PendingPage::PendingPage(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // 滚动区和里面的容器沿用主页“全部”页的 objectName，样式不用另写
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("homeScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *container = new QWidget;
    container->setObjectName("homeContainer");
    auto *column = new QVBoxLayout(container);
    column->setContentsMargins(20, 16, 20, 28);
    column->setSpacing(0);

    m_friendSection = addSection(column, tr("没有待处理的好友申请"));
    column->addSpacing(24);
    m_groupSection = addSection(column, tr("没有待处理的群聊申请"));
    column->addSpacing(24);
    m_communitySection = addSection(column, tr("没有待处理的社区申请"));
    column->addStretch();

    scroll->setWidget(container);
    outer->addWidget(scroll);
    rebuild();
}

PendingPage::Section PendingPage::addSection(QVBoxLayout *column, const QString &emptyText) {
    Section section;
    section.heading = new QLabel;
    section.heading->setObjectName("homeHeading");
    column->addWidget(section.heading);
    column->addSpacing(8);
    section.rows = new QVBoxLayout;
    section.rows->setSpacing(0);
    column->addLayout(section.rows);
    section.empty = new QLabel(emptyText);
    section.empty->setObjectName("homeEmpty");
    column->addWidget(section.empty);
    return section;
}

void PendingPage::setFriendRequests(const QList<FriendRequest> &requests) {
    m_friends = requests;
    rebuild();
}

void PendingPage::setJoinRequests(bool isCommunity, int targetId, const QList<JoinRequest> &requests) {
    // 先去掉这个群 / 社区原来的申请，再放进新的
    for (int i = m_joins.size() - 1; i >= 0; --i) {
        if (m_joins[i].isCommunity == isCommunity && m_joins[i].targetId == targetId) m_joins.removeAt(i);
    }
    m_joins.append(requests);
    rebuild();
}

void PendingPage::clearJoinRequests(bool isCommunity) {
    for (int i = m_joins.size() - 1; i >= 0; --i) {
        if (m_joins[i].isCommunity == isCommunity) m_joins.removeAt(i);
    }
    rebuild();
}

void PendingPage::clear() {
    m_friends.clear();
    m_joins.clear();
    rebuild();
}

void PendingPage::rebuild() {
    clearRows(m_friendSection.rows);
    clearRows(m_groupSection.rows);
    clearRows(m_communitySection.rows);

    QPushButton *reject = nullptr;
    QPushButton *accept = nullptr;
    for (const FriendRequest &request : m_friends) {
        QWidget *row = makeRow(assets::defaultAvatar(),
                               tr("%1  UID %2").arg(request.name).arg(request.uid),
                               request.note.isEmpty() ? tr("请求添加你为好友") : request.note,
                               tr("忽略"), &reject, &accept);
        const int uid = request.uid;
        connect(reject, &QPushButton::clicked, this, [this, uid] { emit friendRequestHandled(uid, false); });
        connect(accept, &QPushButton::clicked, this, [this, uid] { emit friendRequestHandled(uid, true); });
        m_friendSection.rows->addWidget(row);
    }

    int groupCount = 0;
    int communityCount = 0;
    for (const JoinRequest &request : m_joins) {
        QString note = request.isCommunity ? tr("UID %1 申请加入社区").arg(request.uid)
                                           : tr("UID %1 申请加入群聊").arg(request.uid);
        if (!request.note.isEmpty()) note += QStringLiteral(" · ") + request.note;
        QWidget *row = makeRow(request.isCommunity ? assets::defaultCommunityAvatar()
                                                   : assets::defaultGroupAvatar(),
                               request.targetName, note, tr("拒绝"), &reject, &accept);
        connect(reject, &QPushButton::clicked, this, [this, request] { emit joinRequestHandled(request, false); });
        connect(accept, &QPushButton::clicked, this, [this, request] { emit joinRequestHandled(request, true); });
        (request.isCommunity ? m_communitySection : m_groupSection).rows->addWidget(row);
        ++(request.isCommunity ? communityCount : groupCount);
    }

    m_friendSection.heading->setText(tr("好友申请 — %1").arg(m_friends.size()));
    m_friendSection.empty->setVisible(m_friends.isEmpty());
    m_groupSection.heading->setText(tr("群聊申请 — %1").arg(groupCount));
    m_groupSection.empty->setVisible(groupCount == 0);
    m_communitySection.heading->setText(tr("社区申请 — %1").arg(communityCount));
    m_communitySection.empty->setVisible(communityCount == 0);
    emit countChanged(m_friends.size() + m_joins.size());
}
