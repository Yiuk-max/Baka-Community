#include "ContactPanel.h"

#include "Assets.h"
#include "Avatar.h"
#include "MenuStyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
const int kAvatarSize = 32;   // 列表和账号栏里的头像边长
const int kRowHeight = 44;    // 列表每行高度
// 账号栏里昵称能用的宽度：面板 240 - 面板左右边距 16 - 账号栏左右边距 20 - 头像 32 - 设置按钮 32 - 两处间距 20
const int kAccountNameWidth = 120;
}

ContactPanel::ContactPanel(QWidget *parent) : QWidget(parent) {
    setObjectName("contactPanel");
    setAttribute(Qt::WA_StyledBackground);   // QWidget 子类默认不画 QSS 背景，要显式打开
    setFixedWidth(240);                      

    m_userIcon = avatar::roundedIcon(assets::defaultAvatar(), kAvatarSize, kAvatarSize / 2);
    m_groupIcon = avatar::roundedIcon(assets::defaultGroupAvatar(), kAvatarSize, kAvatarSize / 2);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(0);

    // 搜索框：在已加载的联系人里按名字过滤（不发请求）
    m_search = new QLineEdit;
    m_search->setObjectName("contactSearch");
    m_search->setPlaceholderText(tr("搜索好友或群聊"));
    m_search->setFixedHeight(36);
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    layout->addSpacing(14);
    auto *heading = new QLabel(tr("私信"));
    heading->setObjectName("contactHeading");
    heading->setContentsMargins(8, 0, 0, 0);
    layout->addWidget(heading);

    layout->addSpacing(6);
    m_list = new QListWidget;
    m_list->setObjectName("contactList");
    m_list->setIconSize(QSize(kAvatarSize, kAvatarSize));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 名字太长就省略，不横向滚动
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setFocusPolicy(Qt::NoFocus);                            // 不要选中项外面的虚线框
    m_list->setSpacing(1);
    layout->addWidget(m_list, 1);

    // 底部账号栏：自己的头像 + 昵称 + 在线状态
    layout->addSpacing(8);
    auto *accountBar = new QWidget;
    accountBar->setObjectName("accountBar");
    accountBar->setFixedHeight(56);
    auto *accountLayout = new QHBoxLayout(accountBar);
    accountLayout->setContentsMargins(10, 0, 10, 0);
    accountLayout->setSpacing(10);

    auto *accountAvatar = new QLabel;
    accountAvatar->setPixmap(avatar::rounded(assets::defaultAvatar(), kAvatarSize, kAvatarSize / 2));
    accountLayout->addWidget(accountAvatar);

    auto *accountText = new QVBoxLayout;
    accountText->setSpacing(1);
    accountText->addStretch();
    m_accountName = new QLabel;
    m_accountName->setObjectName("accountName");
    accountText->addWidget(m_accountName);
    m_accountStatus = new QLabel(tr("离线"));
    m_accountStatus->setObjectName("accountStatus");
    accountText->addWidget(m_accountStatus);
    accountText->addStretch();
    accountLayout->addLayout(accountText, 1);

    // 设置按钮：设置页还没做，点了只发信号
    auto *settingsButton = new QPushButton;
    settingsButton->setIcon(QIcon(assets::settingsIcon()));
    settingsButton->setIconSize(QSize(18, 18));
    settingsButton->setObjectName("accountSettingsButton");
    settingsButton->setFixedSize(32, 32);
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setToolTip(tr("设置"));
    accountLayout->addWidget(settingsButton);
    layout->addWidget(accountBar);

    connect(settingsButton, &QPushButton::clicked, this, &ContactPanel::settingsClicked);
    connect(m_search, &QLineEdit::textChanged, this, &ContactPanel::rebuildList);
    connect(m_list, &QListWidget::currentRowChanged, this, &ContactPanel::onCurrentRowChanged);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &ContactPanel::showContextMenu);
}

void ContactPanel::showContextMenu(const QPoint &pos) {
    const QListWidgetItem *item = m_list->itemAt(pos);
    if (!item) return;
    // 先把联系人拷出来：菜单开着的时候列表可能刷新，item 会失效
    const Contact contact = m_contacts.at(item->data(Qt::UserRole).toInt());
    if (!contact.isGroup) return;

    QMenu menu(this);
    menus::prepare(&menu);
    QAction *invite = menu.addAction(tr("邀请好友"));
    if (menu.exec(m_list->viewport()->mapToGlobal(pos)) == invite) emit inviteRequested(contact);
}

void ContactPanel::setContacts(const QList<Contact> &contacts) {
    m_contacts = contacts;
    rebuildList();
}

void ContactPanel::setAccount(const QString &name, int uid) {
    // 昵称太长时用省略号收尾（QLabel 自己只会硬截断）
    m_accountName->ensurePolished();   // 先让 QSS 的字体生效，量出来的宽度才对
    m_accountName->setText(
        m_accountName->fontMetrics().elidedText(name, Qt::ElideRight, kAccountNameWidth));
    m_accountName->setToolTip(tr("%1（UID %2）").arg(name).arg(uid));
}

void ContactPanel::setOnline(bool online) {
    m_accountStatus->setText(online ? tr("在线") : tr("离线"));
}

void ContactPanel::clear() {
    m_hasSelection = false;
    m_search->clear();
    m_accountName->clear();
    setContacts({});
}

void ContactPanel::select(const Contact &contact) {
    m_hasSelection = true;
    m_selectedIsGroup = contact.isGroup;
    m_selectedUid = contact.uid;
    rebuildList();   // 重建时会按上面三个值恢复选中，且不发信号
}

void ContactPanel::clearSelection() {
    m_hasSelection = false;
    const QSignalBlocker blocker(m_list);
    m_list->setCurrentRow(-1);
}

void ContactPanel::rebuildList() {
    // 重建期间不发 currentRowChanged：这不是用户点的，不能当成“选中了某人”
    const QSignalBlocker blocker(m_list);
    m_list->clear();

    const QString keyword = m_search->text().trimmed();
    for (int i = 0; i < m_contacts.size(); ++i) {
        const Contact &contact = m_contacts.at(i);
        if (!keyword.isEmpty() && !contact.name.contains(keyword, Qt::CaseInsensitive)) continue;

        auto *item = new QListWidgetItem(contact.isGroup ? m_groupIcon : m_userIcon, contact.name);
        item->setData(Qt::UserRole, i);   // 记下它在 m_contacts 里的下标
        item->setSizeHint(QSize(0, kRowHeight));
        item->setToolTip(contact.isGroup ? tr("%1（群 %2）").arg(contact.name).arg(contact.uid)
                                         : tr("%1（UID %2）").arg(contact.name).arg(contact.uid));
        m_list->addItem(item);

        // 刷新列表 / 改搜索词之后，原来选中的那一行继续保持选中
        if (m_hasSelection && contact.isGroup == m_selectedIsGroup && contact.uid == m_selectedUid) {
            m_list->setCurrentItem(item);
        }
    }
}

void ContactPanel::onCurrentRowChanged(int row) {
    if (row < 0) return;
    const Contact &contact = m_contacts.at(m_list->item(row)->data(Qt::UserRole).toInt());
    m_hasSelection = true;
    m_selectedIsGroup = contact.isGroup;
    m_selectedUid = contact.uid;
    emit contactSelected(contact);
}
