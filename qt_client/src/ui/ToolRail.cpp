#include "ToolRail.h"

#include "Assets.h"

#include <QEvent>
#include <QIcon>
#include <QPushButton>
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

    m_layout->addStretch();   // 以后的按钮插在弹簧上面

    // 退出登录按钮，固定在工具栏最底部（以后有设置页了可以挪进去）
    m_logoutButton = new QPushButton(tr("退出"));
    m_logoutButton->setObjectName("railLogoutButton");
    m_logoutButton->setFixedSize(48, 48);
    m_logoutButton->setCursor(Qt::PointingHandCursor);
    m_logoutButton->setToolTip(tr("退出登录"));
    m_layout->addWidget(m_logoutButton, 0, Qt::AlignHCenter);

    connect(m_logoButton, &QPushButton::clicked, this, &ToolRail::homeClicked);
    connect(m_logoutButton, &QPushButton::clicked, this, &ToolRail::logoutClicked);
}

bool ToolRail::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_logoButton && event->type() == QEvent::MouseButtonDblClick) {
        emit logoDoubleClicked();
    }
    return QWidget::eventFilter(watched, event);
}
