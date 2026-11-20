#include "SettingsDialog.h"

#include "AccountSettingsPage.h"
#include "BakaSettingsPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const QString &username, int uid, const QString &email,
                               SoundPlayer *sound, QWidget *parent)
    : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角），和 LoginDialog 同一套做法
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // 窗口大小跟着卡片走

    auto *card = new QWidget(this);
    card->setObjectName("settingsCard");
    card->setFixedSize(760, 520);
    root->addWidget(card);

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(1, 1, 1, 1);   // 留出卡片 1px 的边框
    cardLayout->setSpacing(0);

    // ---- 左边：分类导航 ----
    auto *sidebar = new QWidget;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setFixedWidth(200);
    auto *navLayout = new QVBoxLayout(sidebar);
    navLayout->setContentsMargins(12, 20, 12, 12);
    navLayout->setSpacing(4);

    auto *sidebarTitle = new QLabel(tr("用户设置"));
    sidebarTitle->setObjectName("settingsSidebarTitle");
    sidebarTitle->setContentsMargins(10, 0, 0, 8);
    navLayout->addWidget(sidebarTitle);
    cardLayout->addWidget(sidebar);

    // ---- 右边：标题栏 + 页面 ----
    auto *main = new QVBoxLayout;
    main->setContentsMargins(28, 18, 18, 24);
    main->setSpacing(16);

    auto *header = new QHBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName("settingsTitle");
    header->addWidget(m_title, 1);
    auto *closeButton = new QPushButton(tr("×"));
    closeButton->setObjectName("closeButton");   // 和登录窗口的 × 同一个样式
    closeButton->setFixedSize(24, 24);
    closeButton->setCursor(Qt::PointingHandCursor);
    header->addWidget(closeButton);
    main->addLayout(header);

    m_pages = new QStackedWidget;
    main->addWidget(m_pages, 1);
    cardLayout->addLayout(main, 1);

    // 分类：要加新的设置页，在这里加一行
    m_accountPage = new AccountSettingsPage(username, uid, email);
    addPage(navLayout, tr("账户"), m_accountPage);
    addPage(navLayout, tr("Baka"), new BakaSettingsPage(sound));

    // 账户页不碰网络，修改请求经这里转出去
    connect(m_accountPage, &AccountSettingsPage::nameChangeRequested, this,
            &SettingsDialog::nameChangeRequested);
    connect(m_accountPage, &AccountSettingsPage::emailChangeRequested, this,
            &SettingsDialog::emailChangeRequested);

    navLayout->addStretch();
    auto *logoutButton = new QPushButton(tr("登出"));
    logoutButton->setObjectName("settingsLogoutButton");
    logoutButton->setFixedHeight(36);
    logoutButton->setCursor(Qt::PointingHandCursor);
    navLayout->addWidget(logoutButton);

    connect(closeButton, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(logoutButton, &QPushButton::clicked, this, [this] { done(Logout); });
}

void SettingsDialog::setProfile(const QString &username, const QString &email) {
    m_accountPage->setProfile(username, email);
}

void SettingsDialog::showProfileError(const QString &message) {
    m_accountPage->showError(message);
}

// ---- 拖动：按钮、输入框会自己吃掉鼠标事件，所以只有按在空白处才会走到这里 ----
void SettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SettingsDialog::addPage(QVBoxLayout *navLayout, const QString &name, QWidget *page) {
    auto *button = new QPushButton(name);
    button->setObjectName("settingsNavButton");
    button->setFixedHeight(36);
    button->setCheckable(true);
    button->setAutoExclusive(true);   // 同一栏里只有一个处于选中
    button->setCursor(Qt::PointingHandCursor);
    navLayout->addWidget(button);
    m_pages->addWidget(page);

    connect(button, &QPushButton::clicked, this, [this, name, page] {
        m_pages->setCurrentWidget(page);
        m_title->setText(name);
    });

    // 第一个加进来的分类默认选中
    if (m_pages->count() == 1) {
        button->setChecked(true);
        m_title->setText(name);
    }
}
