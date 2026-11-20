#include "ToolRail.h"

#include "Assets.h"
#include "Avatar.h"

#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QPushButton>
#include <QScrollArea>
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
    m_communityLayout->addStretch();   // 图标插在弹簧上面，始终从顶部往下排
    scroll->setWidget(communityList);
    m_layout->addWidget(scroll, 1);

    connect(m_logoButton, &QPushButton::clicked, this, &ToolRail::homeClicked);
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

        // 插在末尾的弹簧之前
        m_communityLayout->insertWidget(m_communityLayout->count() - 1, button, 0, Qt::AlignHCenter);
        m_communityButtons.append(button);
    }
}

bool ToolRail::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_logoButton && event->type() == QEvent::MouseButtonDblClick) {
        emit logoDoubleClicked();
    }
    return QWidget::eventFilter(watched, event);
}
