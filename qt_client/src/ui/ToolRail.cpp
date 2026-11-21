#include "ToolRail.h"

#include "Assets.h"
#include "Avatar.h"
#include "MenuStyle.h"

#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

ToolRail::ToolRail(QWidget *parent) : QWidget(parent) {
    setObjectName("toolRail");
    setAttribute(Qt::WA_StyledBackground);   // QWidget 子类默认不画 QSS 背景，要显式打开
    setFixedWidth(60);                       // web 端 --rail-w

    m_layout = new QVBoxLayout(this);// 垂直布局
    m_layout->setContentsMargins(0, 12, 0, 12);// 上下间距
    m_layout->setSpacing(8);

    // 程序 logo 按钮（48x48 的圆角方块，里面 34x34 的图）
    m_logoButton = new QPushButton;
    m_logoButton->setObjectName("railLogoButton");
    m_logoButton->setFixedSize(48, 48);
    m_logoButton->setIcon(QIcon(assets::logo()));
    m_logoButton->setIconSize(QSize(34, 34));
    m_logoButton->setCursor(Qt::PointingHandCursor);    // 悬停改变鼠标图标
    m_logoButton->setToolTip(tr("首页 · 双击Baka！"));   // 悬停提示
    m_logoButton->installEventFilter(this);             // 事件过滤器获取双击信号
    m_layout->addWidget(m_logoButton, 0, Qt::AlignHCenter);//水平居中对齐

    // 分隔线：把 logo 和下面的社区图标隔开
    auto *divider = new QFrame;
    divider->setObjectName("railDivider");
    divider->setFixedSize(32, 2);
    m_layout->addWidget(divider, 0, Qt::AlignHCenter);

    // 社区图标区：占掉中间所有剩余高度，社区多了就在里面滚动（不显示滚动条，用滚轮）
    auto *scroll = new QScrollArea;
    scroll->setObjectName("railCommunityArea");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *communityList = new QWidget;
    communityList->setObjectName("railCommunityList");
    m_communityLayout = new QVBoxLayout(communityList);
    m_communityLayout->setContentsMargins(0, 0, 0, 0);
    m_communityLayout->setSpacing(8);

    // “＋”按钮：跟在最后一个社区图标下面（和 web 端一样），点开是创建 / 加入的菜单
    m_addButton = new QPushButton(QStringLiteral("＋"));
    m_addButton->setObjectName("railAddButton");
    m_addButton->setFixedSize(48, 48);
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setToolTip(tr("创建或加入社区"));
    m_communityLayout->addWidget(m_addButton, 0, Qt::AlignHCenter);

    m_communityLayout->addStretch();   // 图标插在弹簧上面，始终从顶部往下排
    scroll->setWidget(communityList);
    m_layout->addWidget(scroll, 1);

    connect(m_logoButton, &QPushButton::clicked, this, &ToolRail::homeClicked);
    connect(m_addButton, &QPushButton::clicked, this, &ToolRail::showAddMenu);
}

void ToolRail::showAddMenu() {
    QMenu menu(this);
    menus::prepare(&menu);
    const QList<QPair<QString, AddAction>> items = {
        {tr("创建社区"), AddAction::CreateCommunity},
        {tr("创建群聊"), AddAction::CreateGroup},
        {tr("申请加入社区"), AddAction::JoinCommunity},
        {tr("申请加入群聊"), AddAction::JoinGroup},
        {tr("添加好友"), AddAction::AddFriend},
    };
    for (const auto &item : items) menu.addAction(item.first)->setData(static_cast<int>(item.second));

    // 菜单出现在按钮右边
    QAction *chosen = menu.exec(m_addButton->mapToGlobal(QPoint(m_addButton->width() + 10, 0)));
    if (chosen) emit addActionTriggered(static_cast<AddAction>(chosen->data().toInt()));
}

// 社区右键菜单：web 端的“社区设置”是一整个设置页，这边还没有，所以没放
void ToolRail::showCommunityMenu(const Community &community, const QPoint &globalPos) {
    QMenu menu(this);
    menus::prepare(&menu);
    QAction *invite = menu.addAction(tr("邀请至社区"));
    menu.addSeparator();
    QAction *createChannel = menu.addAction(tr("创建频道"));
    menu.addSeparator();
    QAction *leave = menu.addAction(tr("离开社区"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == invite) emit communityActionTriggered(community, CommunityAction::Invite);
    else if (chosen == createChannel) emit communityActionTriggered(community, CommunityAction::CreateChannel);
    else if (chosen == leave) emit communityActionTriggered(community, CommunityAction::Leave);
}

void ToolRail::setCommunities(const QList<Community> &communities) {
    qDeleteAll(m_communityButtons);
    m_communityButtons.clear();

    // 还没做头像下载，所有社区先用同一张默认社区头像（圆角和 logo 按钮一致）
    const QIcon icon = avatar::roundedIcon(assets::defaultCommunityAvatar(), 48, 16);
    for (const Community &community : communities) {
        auto *button = new QPushButton;
        button->setObjectName("railCommunityButton");
        button->setFixedSize(48, 48);
        button->setIcon(icon);
        button->setIconSize(QSize(48, 48));
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(community.name);
        connect(button, &QPushButton::clicked, this,
                [this, community] { emit communityClicked(community); });
        // 右键：社区菜单
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QPushButton::customContextMenuRequested, this,
                [this, community, button](const QPoint &pos) {
            // 等这次右键事件处理完再弹菜单：菜单开着时列表可能刷新，这个按钮会被删掉
            const QPoint globalPos = button->mapToGlobal(pos);
            QTimer::singleShot(0, this, [this, community, globalPos] {
                showCommunityMenu(community, globalPos);
            });
        });

        // 插在“＋”按钮和末尾的弹簧之前
        m_communityLayout->insertWidget(m_communityLayout->count() - 2, button, 0, Qt::AlignHCenter);
        m_communityButtons.append(button);
    }
}

bool ToolRail::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_logoButton && event->type() == QEvent::MouseButtonDblClick) {
        emit logoDoubleClicked();
    }
    return QWidget::eventFilter(watched, event);
}
