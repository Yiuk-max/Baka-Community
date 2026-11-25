#include "CommunitySettingsDialog.h"
#include "CommunityOverviewPage.h"
#include "CommunityChannelsPage.h"
#include "InputDialog.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

CommunitySettingsDialog::CommunitySettingsDialog(const Community &community, bool channels, QWidget *parent)
    : QDialog(parent), m_owner(community.role == QLatin1String("owner")) {
    setWindowTitle(tr("社区设置"));
    setObjectName("communitySettingsDialog");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(820, 520);
    setMinimumSize(780, 500);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *card = new QWidget;
    card->setObjectName("settingsCard");
    root->addWidget(card);
    auto *body = new QHBoxLayout(card);
    body->setContentsMargins(1, 1, 1, 1);
    body->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("settingsSidebar");
    sidebar->setMinimumWidth(180);
    sidebar->setMaximumWidth(180);
    auto *nav = new QVBoxLayout(sidebar);
    nav->setContentsMargins(12, 20, 12, 12);
    nav->setSpacing(4);
    auto *caption = new QLabel(tr("社区设置"));
    caption->setObjectName("settingsSidebarTitle");
    caption->setContentsMargins(10, 0, 0, 8);
    nav->addWidget(caption);
    body->addWidget(sidebar);
    auto *main = new QVBoxLayout;
    main->setContentsMargins(28, 18, 18, 20);
    main->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel;
    title->setObjectName("settingsTitle");
    header->addWidget(title, 1);
    auto *close = new QPushButton(QStringLiteral("×"));
    close->setObjectName("closeButton");
    close->setMinimumSize(24, 24);
    close->setMaximumSize(24, 24);
    close->setCursor(Qt::PointingHandCursor);
    close->setAutoDefault(false);
    close->setAccessibleName(tr("关闭社区设置"));
    header->addWidget(close);
    main->addLayout(header);
    connect(close, &QPushButton::clicked, this, &CommunitySettingsDialog::reject);
    m_pages = new QStackedWidget;
    m_overview = new CommunityOverviewPage(community);
    m_channels = new CommunityChannelsPage(community.id, m_owner);
    m_pages->addWidget(m_overview);
    m_pages->addWidget(m_channels);
    main->addWidget(m_pages, 1);
    auto *group = new QButtonGroup(this);
    const QStringList names{tr("概览"), tr("频道")};
    for (int i = 0; i < names.size(); ++i) {
        auto *item = new QPushButton(names[i]);
        item->setObjectName("settingsNavButton");
        item->setMinimumHeight(36);
        item->setCheckable(true);
        item->setAutoDefault(false);
        item->setCursor(Qt::PointingHandCursor);
        group->addButton(item, i);
        nav->addWidget(item);
        connect(item, &QPushButton::clicked, this, [this, title, names, i] {
            m_pages->setCurrentIndex(i);
            title->setText(names[i]);
        });
    }
    group->button(channels ? 1 : 0)->click();
    nav->addStretch();
    m_dissolve = new QPushButton(tr("解散社区"));
    m_dissolve->setObjectName("settingsLogoutButton");
    m_dissolve->setMinimumHeight(36);
    m_dissolve->setAutoDefault(false);
    m_dissolve->setEnabled(m_owner);
    m_dissolve->setCursor(Qt::PointingHandCursor);
    nav->addWidget(m_dissolve);
    connect(m_dissolve, &QPushButton::clicked, this, [this] {
        if (!m_online || !m_pending.isEmpty()) return;
        const QString name = m_overview->draft().name;
        InputDialog confirm(tr("解散「%1」？社区和频道将被删除，此操作不可撤销。").arg(name), {}, this);
        connect(this, &QDialog::finished, &confirm, &QDialog::reject);
        if (confirm.exec() == QDialog::Accepted && begin(QStringLiteral("dissolve"))) emit dissolveRequested();
    });
    m_message = new QLabel;
    m_message->setObjectName("settingsMessage");
    m_message->setWordWrap(true);
    m_message->hide();
    main->addWidget(m_message);
    body->addLayout(main, 1);
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(15000);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        finish();
        showMessage(tr("响应超时，请重新打开设置确认结果"), true);
    });
    m_messageTimer = new QTimer(this);
    m_messageTimer->setSingleShot(true);
    m_messageTimer->setInterval(3000);
    connect(m_messageTimer, &QTimer::timeout, m_message, &QWidget::hide);
    connect(m_overview, &CommunityOverviewPage::saveRequested, this, [this] {
        if (!m_overview->isDirty() || !begin(QStringLiteral("save"))) return;
        const auto value = m_overview->draft();
        emit saveRequested(value.name, value.description, value.avatarId, value.bannerId);
    });
    connect(m_overview, &CommunityOverviewPage::assetsReset, this, &CommunitySettingsDialog::assetsReset);
    connect(m_overview, &CommunityOverviewPage::imageError, this, [this](const QString &text) { showMessage(text, true); });
    connect(m_overview, &CommunityOverviewPage::uploadRequested, this, [this](const QByteArray &png, bool banner) {
        if (!begin(QStringLiteral("upload"))) return;
        m_timeout->stop(); // 上传器自己处理逐步超时。
        m_uploadPng = png;
        m_uploadBanner = banner;
        emit assetUploadRequested(png, banner);
    });
    connect(m_channels, &CommunityChannelsPage::createRequested, this, [this](const QString &name, const QString &category) {
        if (begin(QStringLiteral("create"))) emit createRequested(name, category);
    });
    connect(m_channels, &CommunityChannelsPage::modifyRequested, this,
        [this](int id, const QString &name, const QString &category) {
            if (begin(QStringLiteral("modify"))) emit modifyRequested(id, name, category);
        });
    connect(m_channels, &CommunityChannelsPage::deleteRequested, this, [this](int id) {
        if (begin(QStringLiteral("delete"))) emit deleteRequested(id);
    });
}

bool CommunitySettingsDialog::begin(const QString &operation) {
    if (!m_owner || !m_online || !m_pending.isEmpty()) return false;
    m_pending = operation;
    m_pages->setEnabled(false);
    m_dissolve->setEnabled(false);
    m_timeout->start();
    showMessage(operation == QLatin1String("upload") ? tr("上传中…") : tr("提交中…"));
    return true;
}

void CommunitySettingsDialog::finish() {
    m_timeout->stop();
    m_pending.clear();
    m_uploadPng.clear();
    m_pages->setEnabled(m_online);
    m_dissolve->setEnabled(m_owner && m_online);
}

void CommunitySettingsDialog::showMessage(const QString &text, bool error) {
    m_messageTimer->stop();
    m_message->setText(text);
    m_message->setProperty("error", error);
    m_message->style()->unpolish(m_message);
    m_message->style()->polish(m_message);
    m_message->setVisible(!text.isEmpty());
    if (!error && m_pending.isEmpty()) m_messageTimer->start();
}

void CommunitySettingsDialog::handleSystemText(const QString &text) {
    if (m_pending.isEmpty() || m_pending == QLatin1String("upload")) return;
    const QString operation = m_pending;
    const QString subject = (operation == QLatin1String("save") || operation == QLatin1String("dissolve"))
        ? QStringLiteral("community") : QStringLiteral("channel");
    const QString verb = operation == QLatin1String("save") || operation == QLatin1String("modify")
        ? QStringLiteral("updated") : operation == QLatin1String("create") ? QStringLiteral("created") : QStringLiteral("deleted");
    const QString success = (subject == QLatin1String("community") ? QStringLiteral("Community ") : QStringLiteral("Channel "))
        + verb + QStringLiteral(" successfully");
    const QString action = operation == QLatin1String("save") || operation == QLatin1String("modify")
        ? QStringLiteral("modify") : operation == QLatin1String("create") ? QStringLiteral("create") : QStringLiteral("delete");
    const bool ok = text.startsWith(success);
    const bool failed = text.startsWith(QStringLiteral("Failed to %1 %2").arg(action, subject))
        || text.startsWith(QLatin1String("You must be logged in")) || text.startsWith(QLatin1String("You are not a member"))
        || text.contains(QLatin1String("cannot be empty"));
    if (!ok && !failed) return;
    if (ok && operation == QLatin1String("save")) m_overview->saved();
    if (ok && operation == QLatin1String("create")) m_channels->created();
    if (ok && operation == QLatin1String("modify")) m_channels->modified();
    finish();
    showMessage(ok ? tr("已保存") : tr("操作失败：%1").arg(text), !ok);
    if (ok && operation == QLatin1String("dissolve")) accept();
}

void CommunitySettingsDialog::setChannels(const QList<Channel> &channels) { m_channels->setChannels(channels); }
void CommunitySettingsDialog::setAsset(int id, const QByteArray &data) { m_overview->setAsset(id, data); }
void CommunitySettingsDialog::assetUploaded(int id) {
    if (m_pending != QLatin1String("upload")) return;
    if (id <= 0) return assetFailed(tr("上传失败，请重试"));
    m_overview->applyUpload(id, m_uploadPng, m_uploadBanner);
    finish();
    showMessage(tr("图片已就绪，保存后应用"));
}
void CommunitySettingsDialog::assetFailed(const QString &message) {
    if (m_pending != QLatin1String("upload")) return;
    finish();
    showMessage(message, true);
}
void CommunitySettingsDialog::connectionLost() {
    m_online = false;
    finish();
    showMessage(tr("连接已断开，草稿已保留"), true);
}
void CommunitySettingsDialog::reject() {
    if (!m_pending.isEmpty()) { showMessage(tr("请等待当前操作完成"), true); return; }
    if (m_overview->isDirty()) {
        InputDialog confirm(tr("放弃未保存的修改？"), {}, this);
        connect(this, &QDialog::finished, &confirm, &QDialog::reject);
        if (confirm.exec() != QDialog::Accepted) return;
    }
    QDialog::reject();
}

void CommunitySettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    } else QDialog::mousePressEvent(event);
}
void CommunitySettingsDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
    } else QDialog::mouseMoveEvent(event);
}
void CommunitySettingsDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
