#include "HomePage.h"

#include "Assets.h"
#include "Avatar.h"
#include "PendingPage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
const int kAvatarSize = 32;   // 每行头像边长（web 端 .friends-avatar）
const int kRowHeight = 56;    // 每行高度（web 端 .friends-group-row）

// 清空一个“行”布局里的全部控件
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

HomePage::HomePage(QWidget *parent) : QWidget(parent) {
    setObjectName("homePage");
    setAttribute(Qt::WA_StyledBackground);   // QWidget 子类默认不画 QSS 背景，要显式打开

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pages = new QStackedWidget(this);
    layout->addWidget(buildHeader());
    layout->addWidget(m_pages, 1);

    m_pages->addWidget(buildAllPage());

    // “待定”页是独立的 PendingPage；有待处理的申请时，标签上带上数量
    m_pendingPage = new PendingPage(this);
    m_pages->addWidget(m_pendingPage);
    connect(m_pendingPage, &PendingPage::countChanged, this, [this](int count) {
        m_pendingTab->setText(count == 0 ? tr("待定") : tr("待定 · %1").arg(count));
    });
}

QWidget *HomePage::buildHeader() {
    auto *header = new QFrame(this);
    header->setObjectName("homeHeader");
    header->setFixedHeight(48);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(16);

    auto *title = new QLabel(tr("好友"));
    title->setObjectName("homeTitle");
    layout->addWidget(title);

    auto *divider = new QFrame;
    divider->setObjectName("homeHeaderDivider");
    divider->setFixedSize(1, 24);
    layout->addWidget(divider);

    // 全部 / 待定：互斥的两个标签，按下标切 m_pages
    auto *tabs = new QHBoxLayout;
    tabs->setSpacing(2);
    auto *group = new QButtonGroup(this);
    const QStringList names = {tr("全部"), tr("待定")};
    for (int i = 0; i < names.size(); ++i) {
        auto *tab = new QPushButton(names.at(i));
        tab->setObjectName("homeTab");
        tab->setCheckable(true);
        tab->setChecked(i == 0);
        tab->setFixedHeight(32);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setFocusPolicy(Qt::NoFocus);
        group->addButton(tab, i);
        tabs->addWidget(tab);
        if (i == 1) m_pendingTab = tab;
    }
    layout->addLayout(tabs);
    layout->addStretch();

    connect(group, &QButtonGroup::idClicked, m_pages, &QStackedWidget::setCurrentIndex);
    return header;
}

QWidget *HomePage::buildAllPage() {
    auto *scroll = new QScrollArea;
    scroll->setObjectName("homeScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *container = new QWidget;
    container->setObjectName("homeContainer");
    auto *column = new QVBoxLayout(container);
    column->setContentsMargins(20, 16, 20, 28);
    column->setSpacing(0);

    // 搜索框：只在已加载的好友里按名字过滤（不发请求）
    m_search = new QLineEdit;
    m_search->setObjectName("homeSearch");
    m_search->setPlaceholderText(tr("搜索好友"));
    m_search->setFixedHeight(40);
    m_search->setClearButtonEnabled(true);
    column->addWidget(m_search);
    column->addSpacing(18);

    // “全部 — N” + “＋ 新建群聊”
    auto *headingRow = new QHBoxLayout;
    m_friendsHeading = new QLabel;
    m_friendsHeading->setObjectName("homeHeading");
    headingRow->addWidget(m_friendsHeading);
    headingRow->addStretch();
    auto *createGroup = new QPushButton(tr("＋ 新建群聊"));
    createGroup->setObjectName("homeLinkButton");
    createGroup->setFixedHeight(32);
    createGroup->setCursor(Qt::PointingHandCursor);
    createGroup->setFocusPolicy(Qt::NoFocus);
    headingRow->addWidget(createGroup);
    column->addLayout(headingRow);
    column->addSpacing(8);

    m_friendRows = new QVBoxLayout;
    m_friendRows->setSpacing(0);
    column->addLayout(m_friendRows);
    m_friendsEmpty = new QLabel(tr("没有找到好友"));
    m_friendsEmpty->setObjectName("homeEmpty");
    column->addWidget(m_friendsEmpty);

    column->addSpacing(24);
    m_groupRows = addSection(column, tr("群聊"), tr("你的群聊通讯录"));

    column->addSpacing(24);
    m_communityRows = addSection(column, tr("社区"), tr("你加入的社区"));
    m_communitiesEmpty = new QLabel(tr("还没有加入社区。"));
    m_communitiesEmpty->setObjectName("homeEmpty");
    column->addWidget(m_communitiesEmpty);

    column->addStretch();
    scroll->setWidget(container);

    connect(m_search, &QLineEdit::textChanged, this, &HomePage::rebuildContacts);
    connect(createGroup, &QPushButton::clicked, this, &HomePage::createGroupClicked);

    rebuildContacts();
    rebuildCommunities();
    return scroll;
}


QVBoxLayout *HomePage::addSection(QVBoxLayout *column, const QString &title, const QString &hint) {
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("homeSubHeading");
    column->addWidget(titleLabel);
    column->addSpacing(4);
    auto *hintLabel = new QLabel(hint);
    hintLabel->setObjectName("homeHint");
    column->addWidget(hintLabel);
    column->addSpacing(8);

    auto *rows = new QVBoxLayout;
    rows->setSpacing(0);
    column->addLayout(rows);
    return rows;
}

// 一行：头像 + 名字（+ 右侧的 ›）。整行是一个按钮，悬停变色和点击都由按钮自己负责
QPushButton *HomePage::makeRow(const QPixmap &avatarPixmap, const QString &name, bool showArrow) {
    auto *row = new QPushButton;
    row->setObjectName("homeRow");
    row->setFixedHeight(kRowHeight);
    row->setCursor(Qt::PointingHandCursor);
    row->setFocusPolicy(Qt::NoFocus);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 0, 14, 0);
    layout->setSpacing(12);

    auto *avatarLabel = new QLabel;
    avatarLabel->setPixmap(avatarPixmap);
    avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents);   // 鼠标事件交给整行按钮
    layout->addWidget(avatarLabel);

    auto *nameLabel = new QLabel(name);
    nameLabel->setObjectName("homeRowName");
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 名字太长就截断，不把窗口撑宽
    layout->addWidget(nameLabel, 1);

    if (showArrow) {
        auto *arrow = new QLabel(QStringLiteral("›"));
        arrow->setObjectName("homeRowArrow");
        arrow->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(arrow);
    }
    return row;
}

void HomePage::setContacts(const QList<Contact> &contacts) {
    m_contacts = contacts;
    rebuildContacts();
}

void HomePage::setCommunities(const QList<Community> &communities) {
    m_communities = communities;
    rebuildCommunities();
}

void HomePage::clear() {
    m_search->clear();
    setContacts({});
    setCommunities({});
    m_pendingPage->clear();
}

void HomePage::rebuildContacts() {
    clearRows(m_friendRows);
    clearRows(m_groupRows);

    const QString keyword = m_search->text().trimmed();
    int friendCount = 0;
    for (const Contact &contact : m_contacts) {
        // 搜索只过滤好友，群聊一直全部显示（和 web 端一致）
        if (!contact.isGroup && !keyword.isEmpty()
                && !contact.name.contains(keyword, Qt::CaseInsensitive)) continue;

        // 好友：有本地头像文件就用它，没有是默认头像；群还没有头像
        QPushButton *row = contact.isGroup
            ? makeRow(avatar::rounded(assets::defaultGroupAvatar(), kAvatarSize, kAvatarSize / 2), contact.name, true)
            : makeRow(avatar::user(contact.avatarPath, kAvatarSize), contact.name, false);
        connect(row, &QPushButton::clicked, this, [this, contact] { emit contactActivated(contact); });
        if (contact.isGroup) {
            m_groupRows->addWidget(row);
        } else {
            m_friendRows->addWidget(row);
            ++friendCount;
        }
    }
    m_friendsHeading->setText(tr("全部 — %1").arg(friendCount));
    m_friendsEmpty->setVisible(friendCount == 0);
}

void HomePage::rebuildCommunities() {
    clearRows(m_communityRows);
    for (const Community &community : m_communities) {
        QPushButton *row = makeRow(
            avatar::rounded(assets::defaultCommunityAvatar(), kAvatarSize, kAvatarSize / 2), community.name, true);
        connect(row, &QPushButton::clicked, this,
                [this, community] { emit communityActivated(community); });
        m_communityRows->addWidget(row);
    }
    m_communitiesEmpty->setVisible(m_communities.isEmpty());
}
