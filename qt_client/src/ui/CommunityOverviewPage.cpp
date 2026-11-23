#include "CommunityOverviewPage.h"
#include "Assets.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QPushButton *action(const QString &text, const char *style = "settingsRowButton") {
    auto *button = new QPushButton(text);
    button->setObjectName(style);
    button->setMinimumSize(64, 32);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);
    return button;
}
QWidget *imageRow(const QString &text, QLabel *&preview, QPushButton *&change, bool banner) {
    auto *row = new QWidget;
    row->setObjectName("settingsRow");
    row->setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 10, 12, 10);
    layout->setSpacing(12);
    auto *label = new QLabel(text);
    label->setObjectName("settingsRowName");
    label->setMinimumWidth(56);
    layout->addWidget(label);
    preview = new QLabel;
    preview->setMinimumSize(banner ? QSize(100, 56) : QSize(56, 56));
    preview->setMaximumSize(preview->minimumSize());
    preview->setAlignment(Qt::AlignCenter);
    layout->addWidget(preview);
    layout->addStretch();
    change = action(QObject::tr("更换"));
    layout->addWidget(change);
    return row;
}
}

CommunityOverviewPage::CommunityOverviewPage(const Community &community, QWidget *parent)
    : QWidget(parent), m_saved(community), m_avatarId(community.avatarId), m_bannerId(community.bannerId) {
    const bool owner = community.role == QLatin1String("owner");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto *images = new QHBoxLayout;
    images->setSpacing(8);
    QPushButton *avatarButton;
    QPushButton *bannerButton;
    images->addWidget(imageRow(tr("头像"), m_avatar, avatarButton, false), 1);
    images->addWidget(imageRow(tr("背景"), m_banner, bannerButton, true), 1);
    avatarButton->setEnabled(owner);
    bannerButton->setEnabled(owner);
    connect(avatarButton, &QPushButton::clicked, this, [this] { chooseAsset(false); });
    connect(bannerButton, &QPushButton::clicked, this, [this] { chooseAsset(true); });
    layout->addLayout(images);
    layout->addSpacing(4);
    auto *nameLabel = new QLabel(tr("社区名称"));
    nameLabel->setObjectName("settingsRowName");
    layout->addWidget(nameLabel);
    m_name = new QLineEdit(community.name);
    m_name->setObjectName("settingsInput");
    m_name->setMinimumHeight(36);
    m_name->setMaxLength(qMax<qsizetype>(60, community.name.size()));
    m_name->setReadOnly(!owner);
    layout->addWidget(m_name);
    auto *descriptionHeader = new QHBoxLayout;
    auto *descriptionLabel = new QLabel(tr("社区简介"));
    descriptionLabel->setObjectName("settingsRowName");
    descriptionHeader->addWidget(descriptionLabel);
    m_count = new QLabel;
    m_count->setObjectName("settingsRowName");
    descriptionHeader->addStretch();
    descriptionHeader->addWidget(m_count);
    layout->addLayout(descriptionHeader);
    m_description = new QPlainTextEdit(community.description);
    m_description->setObjectName("settingsInput");
    m_description->setMinimumHeight(90);
    m_description->setMaximumHeight(110);
    m_description->setPlaceholderText(tr("介绍一下这个社区"));
    m_description->setReadOnly(!owner);
    layout->addWidget(m_description);
    auto *id = new QLabel(tr("社区 ID：%1").arg(community.id));
    id->setObjectName("settingsRowName");
    id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(id);
    layout->addStretch();
    m_saveBar = new QWidget;
    auto *footer = new QHBoxLayout(m_saveBar);
    footer->setContentsMargins(0, 0, 0, 0);
    footer->addStretch();
    auto *resetButton = action(tr("取消"));
    resetButton->setObjectName("settingsRowButton");
    resetButton->setProperty("action", "resetProfile");
    m_save = action(tr("保存"), "primaryButton");
    m_save->setProperty("action", "saveProfile");
    footer->addWidget(resetButton);
    footer->addWidget(m_save);
    layout->addWidget(m_saveBar);
    m_avatarPixmap = QPixmap(assets::defaultCommunityAvatar());
    m_bannerPixmap = QPixmap(assets::defaultBackground());
    m_savedAvatar = m_avatarPixmap;
    m_savedBanner = m_bannerPixmap;
    refreshPreview();
    connect(m_name, &QLineEdit::textChanged, this, &CommunityOverviewPage::updateActions);
    connect(m_description, &QPlainTextEdit::textChanged, this, &CommunityOverviewPage::updateActions);
    connect(resetButton, &QPushButton::clicked, this, &CommunityOverviewPage::reset);
    connect(m_save, &QPushButton::clicked, this, &CommunityOverviewPage::saveRequested);
    connect(m_name, &QLineEdit::returnPressed, m_save, &QPushButton::click);
    updateActions();
}

Community CommunityOverviewPage::draft() const {
    Community result = m_saved;
    result.name = m_name->text().trimmed();
    result.description = m_description->toPlainText().trimmed();
    result.avatarId = m_avatarId;
    result.bannerId = m_bannerId;
    return result;
}

bool CommunityOverviewPage::isDirty() const {
    const auto value = draft();
    return value.name != m_saved.name || value.description != m_saved.description
        || value.avatarId != m_saved.avatarId || value.bannerId != m_saved.bannerId;
}

void CommunityOverviewPage::updateActions() {
    const int length = m_description->toPlainText().size();
    m_count->setText(tr("%1 / 300").arg(length));
    m_saveBar->setVisible(isDirty() && m_saved.role == QLatin1String("owner"));
    m_save->setEnabled(isDirty() && !draft().name.isEmpty()
        && length <= qMax<qsizetype>(300, m_saved.description.size()));
}

void CommunityOverviewPage::saved() {
    m_saved = draft();
    m_savedAvatar = m_avatarPixmap;
    m_savedBanner = m_bannerPixmap;
    updateActions();
}

void CommunityOverviewPage::reset() {
    m_name->setText(m_saved.name);
    m_description->setPlainText(m_saved.description);
    m_avatarId = m_saved.avatarId;
    m_bannerId = m_saved.bannerId;
    m_avatarPixmap = m_savedAvatar;
    m_bannerPixmap = m_savedBanner;
    refreshPreview();
    emit assetsReset(m_avatarId, m_bannerId);
    updateActions();
}
