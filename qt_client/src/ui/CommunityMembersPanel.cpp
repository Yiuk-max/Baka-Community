#include "CommunityMembersPanel.h"

#include "Avatar.h"
#include "MenuStyle.h"
#include "NoticeDialog.h"
#include "store/AvatarStore.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {

const int kAvatarSize = 32;

// 身份分组顺序和显示名（对应 proto 的 owner / admin / member）
struct RoleGroup {
    const char *role;
    const char *title;
};

const RoleGroup kGroups[] = {
    {"owner", "所有者"},
    {"admin", "管理员"},
    {"member", "成员"},
};

QString roleTitle(const QString &role) {
    if (role == QLatin1String("owner")) return QObject::tr("所有者");
    if (role == QLatin1String("admin")) return QObject::tr("管理员");
    return {};
}

// 清空一个布局里的全部控件
void clearLayout(QVBoxLayout *layout) {
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace

CommunityMembersPanel::CommunityMembersPanel(AvatarStore *avatars, QWidget *parent)
    : QWidget(parent), m_avatars(avatars) {
    setObjectName("communityMembers");
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(220);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部：标题 + 人数 + 关闭按钮
    auto *header = new QWidget;
    header->setObjectName("communityMembersHeader");
    header->setFixedHeight(48);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 8, 0);
    headerLayout->setSpacing(6);
    auto *title = new QLabel(tr("社区成员"));
    title->setObjectName("communityMembersTitle");
    headerLayout->addWidget(title);
    m_count = new QLabel(QStringLiteral("0"));
    m_count->setObjectName("communityMembersCount");
    headerLayout->addWidget(m_count);
    headerLayout->addStretch();
    auto *closeButton = new QPushButton(QStringLiteral("×"));
    closeButton->setObjectName("closeButton");
    closeButton->setFixedSize(20, 20);
    closeButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(closeButton);
    layout->addWidget(header);

    auto *scroll = new QScrollArea;
    scroll->setObjectName("communityMembersScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *container = new QWidget;
    container->setObjectName("communityMembersContainer");
    m_rows = new QVBoxLayout(container);
    m_rows->setContentsMargins(10, 10, 10, 14);
    m_rows->setSpacing(2);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    connect(closeButton, &QPushButton::clicked, this, &CommunityMembersPanel::closed);

    // 某个人的头像下好了 / 换了：原地换掉行里的图（不重建列表，避免菜单开着时被删）
    connect(m_avatars, &AvatarStore::avatarChanged, this, [this](int uid) {
        QLabel *label = m_avatarLabels.value(uid);
        if (!label) return;
        const QString path = m_avatars->path(uid);
        for (const CommunityMember &member : std::as_const(m_members)) {
            if (member.uid != uid) continue;
            label->setPixmap(avatar::user(path, kAvatarSize));
            break;
        }
    });
}

void CommunityMembersPanel::setMembers(const QList<CommunityMember> &members, int selfUid,
                                       bool isOwner) {
    m_members = members;
    m_selfUid = selfUid;
    m_isOwner = isOwner;
    for (const CommunityMember &member : m_members) {
        if (member.avatarId > 0) m_avatars->ensure(member.uid, member.avatarId);
    }
    rebuild();
}

void CommunityMembersPanel::clear() {
    m_members.clear();
    rebuild();
}

void CommunityMembersPanel::rebuild() {
    clearLayout(m_rows);
    m_avatarLabels.clear();
    m_count->setText(QString::number(m_members.size()));

    for (const RoleGroup &group : kGroups) {
        QList<CommunityMember> matched;
        for (const CommunityMember &member : m_members) {
            if (member.role == QLatin1String(group.role)) matched.append(member);
        }
        if (matched.isEmpty()) continue;

        auto *heading = new QLabel(tr("%1 — %2").arg(tr(group.title)).arg(matched.size()));
        heading->setObjectName("communityMembersGroup");
        m_rows->addSpacing(6);
        m_rows->addWidget(heading);
        for (const CommunityMember &member : std::as_const(matched)) {
            m_rows->addWidget(makeRow(member, m_isOwner));
        }
    }

    if (m_members.isEmpty()) {
        auto *empty = new QLabel(tr("还没有成员"));
        empty->setObjectName("communityMembersEmpty");
        m_rows->addWidget(empty);
    }
    m_rows->addStretch();
}

QWidget *CommunityMembersPanel::makeRow(const CommunityMember &member, bool isOwner) {
    auto *row = new QWidget;
    row->setObjectName("communityMemberRow");
    row->setAttribute(Qt::WA_StyledBackground);
    row->setFixedHeight(44);
    row->setCursor(Qt::PointingHandCursor);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(10);

    auto *avatarLabel = new QLabel;
    avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);
    avatarLabel->setPixmap(avatar::user(member.avatarPath, kAvatarSize));
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(avatarLabel);
    m_avatarLabels.insert(member.uid, avatarLabel);

    auto *name = new QLabel(member.name.isEmpty() ? QString::number(member.uid) : member.name);
    name->setObjectName("communityMemberName");
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 名字太长就截断
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *identity = new QVBoxLayout;
    identity->setContentsMargins(0, 0, 0, 0);
    identity->addWidget(name);
    auto *status = new QLabel(tr("离线"));   // Web 当前同样没有在线状态接口
    status->setObjectName("communityMemberTag");
    status->setAttribute(Qt::WA_TransparentForMouseEvents);
    identity->addWidget(status);
    layout->addLayout(identity, 1);

    // 自己带个标记，方便在一堆名字里找到自己
    if (member.uid == m_selfUid) {
        auto *self = new QLabel(tr("你"));
        self->setObjectName("communityMemberTag");
        self->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(self);
    }
    const QString tag = roleTitle(member.role);
    if (!tag.isEmpty()) {
        auto *roleLabel = new QLabel(tag);
        roleLabel->setObjectName("communityMemberTag");
        roleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(roleLabel);
    }

    Q_UNUSED(isOwner);
    // Web 点击成员打开资料；不额外给成员行增加管理右键菜单。
    row->installEventFilter(this);
    row->setProperty("memberUid", member.uid);
    return row;
}

bool CommunityMembersPanel::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() != QEvent::MouseButtonRelease) return QWidget::eventFilter(watched, event);
    if (static_cast<QMouseEvent *>(event)->button() != Qt::LeftButton) return false;

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget) return false;
    const int uid = widget->property("memberUid").toInt();
    for (const CommunityMember &member : std::as_const(m_members)) {
        if (member.uid != uid) continue;
        // 等这次右键事件处理完再弹菜单（菜单开着时列表可能重建，这个行控件会被删掉）
        QPointer<CommunityMembersPanel> self(this);
        QTimer::singleShot(0, this, [self, member] {
            if (!self) return;
            NoticeDialog profile(QObject::tr("%1\nUID %2").arg(member.name).arg(member.uid), self);
            profile.exec();
        });
        return true;
    }
    return false;
}

void CommunityMembersPanel::showRowMenu(const CommunityMember &member, const QPoint &globalPos) {
    QMenu menu(this);
    menus::prepare(&menu);
    const bool isAdmin = member.role == QLatin1String("admin");
    QAction *promote = menu.addAction(isAdmin ? tr("取消管理员") : tr("设为管理员"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("移出社区"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == promote) emit roleChangeRequested(member.uid, !isAdmin);
    else if (chosen == remove) emit removeRequested(member.uid);
}
