#include "CommunityChannelsPage.h"
#include "InputDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
QPushButton *action(const QString &text, const char *style = "settingsRowButton") {
    auto *button = new QPushButton(text);
    button->setObjectName(style);
    button->setMinimumSize(48, 30);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);
    return button;
}
}

CommunityChannelsPage::CommunityChannelsPage(int communityId, bool owner, QWidget *parent)
    : QWidget(parent), m_communityId(communityId), m_owner(owner) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *form = new QHBoxLayout;
    m_newChannel = new QLineEdit;
    m_newChannel->setObjectName("settingsInput");
    m_newChannel->setPlaceholderText(tr("新频道名称"));
    m_newChannel->setMaxLength(60);
    m_newChannel->setMinimumHeight(36);
    form->addWidget(m_newChannel, 1);
    m_create = action(tr("新建"), "primaryButton");
    m_create->setMinimumSize(64, 36);
    form->addWidget(m_create);
    layout->addLayout(form);
    m_newChannel->setEnabled(owner);
    m_create->setEnabled(false);
    connect(m_newChannel, &QLineEdit::textChanged, this, [this] {
        m_create->setEnabled(m_owner && !m_newChannel->text().trimmed().isEmpty());
    });
    connect(m_create, &QPushButton::clicked, this, [this] {
        if (m_owner && !m_newChannel->text().trimmed().isEmpty())
            emit createRequested(m_newChannel->text().trimmed(), tr("一般"));
    });
    connect(m_newChannel, &QLineEdit::returnPressed, m_create, &QPushButton::click);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("communitySettingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *container = new QWidget;
    container->setObjectName("communitySettingsContainer");
    m_rows = new QVBoxLayout(container);
    m_rows->setContentsMargins(0, 0, 4, 0);
    m_rows->setSpacing(6);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    m_empty = new QLabel(tr("暂无频道"));
    m_empty->setObjectName("settingsRowName");
    layout->addWidget(m_empty);
    auto *hint = new QLabel(tr("频道修改即时生效"));
    hint->setObjectName("settingsRowName");
    layout->addWidget(hint);
}

void CommunityChannelsPage::setChannels(const QList<Channel> &channels) {
    m_editingRow = nullptr;
    while (auto *item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QMap<QString, QList<Channel>> groups;
    for (const auto &channel : channels) {
        if (channel.communityId != m_communityId) continue;
        groups[channel.category.isEmpty() ? tr("一般") : channel.category].append(channel);
    }
    m_empty->setVisible(groups.isEmpty());
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        auto *heading = new QLabel(it.key());
        heading->setObjectName("settingsSidebarTitle");
        heading->setContentsMargins(2, 6, 0, 2);
        m_rows->addWidget(heading);
        for (const auto &channel : it.value()) m_rows->addWidget(makeRow(channel));
    }
    m_rows->addStretch();
}

QWidget *CommunityChannelsPage::makeRow(const Channel &channel) {
    auto *row = new QStackedWidget;
    row->setObjectName("settingsRow");
    row->setAttribute(Qt::WA_StyledBackground);
    row->setProperty("channelId", channel.id);
    row->setMinimumHeight(54);
    row->setMaximumHeight(54);
    auto *display = new QWidget;
    auto *bar = new QHBoxLayout(display);
    bar->setContentsMargins(12, 9, 10, 9);
    bar->setSpacing(8);
    auto *name = new QLabel(QStringLiteral("#  %1").arg(channel.name));
    name->setTextFormat(Qt::PlainText);
    name->setObjectName("settingsRowValue");
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    name->setToolTip(channel.name);
    bar->addWidget(name, 1);
    auto *edit = action(tr("编辑"));
    edit->setProperty("action", "editChannel");
    auto *remove = action(tr("删除"), "linkButton");
    remove->setProperty("action", "deleteChannel");
    bar->addWidget(edit);
    bar->addWidget(remove);
    edit->setVisible(m_owner);
    remove->setVisible(m_owner);
    row->addWidget(display);
    auto *editor = new QWidget;
    auto *fields = new QHBoxLayout(editor);
    fields->setContentsMargins(10, 9, 10, 9);
    fields->setSpacing(6);
    auto *nameInput = new QLineEdit(channel.name);
    nameInput->setObjectName("settingsInput");
    nameInput->setMaxLength(qMax<qsizetype>(60, channel.name.size()));
    nameInput->setMinimumHeight(30);
    auto *categoryInput = new QLineEdit(channel.category);
    categoryInput->setObjectName("settingsInput");
    categoryInput->setPlaceholderText(tr("分区"));
    categoryInput->setMaxLength(qMax<qsizetype>(60, channel.category.size()));
    categoryInput->setMinimumHeight(30);
    fields->addWidget(nameInput, 3);
    fields->addWidget(categoryInput, 2);
    auto *cancel = action(tr("取消"));
    auto *save = action(tr("保存"), "primaryButton");
    fields->addWidget(cancel);
    fields->addWidget(save);
    row->addWidget(editor);
    connect(edit, &QPushButton::clicked, this, [this, row, nameInput, categoryInput, channel] {
        if (auto *previous = qobject_cast<QStackedWidget *>(m_editingRow)) previous->setCurrentIndex(0);
        m_editingRow = row;
        nameInput->setText(channel.name);
        categoryInput->setText(channel.category);
        row->setCurrentIndex(1);
        nameInput->setFocus();
        nameInput->selectAll();
    });
    connect(cancel, &QPushButton::clicked, row, [this, row] {
        row->setCurrentIndex(0);
        m_editingRow = nullptr;
    });
    connect(nameInput, &QLineEdit::textChanged, save, [save](const QString &text) {
        save->setEnabled(!text.trimmed().isEmpty());
    });
    connect(save, &QPushButton::clicked, this, [this, row, channel, nameInput, categoryInput] {
        const QString name = nameInput->text().trimmed();
        const QString category = categoryInput->text().trimmed().isEmpty() ? tr("一般") : categoryInput->text().trimmed();
        if (!m_owner || name.isEmpty()) return;
        if (name == channel.name && category == channel.category) { row->setCurrentIndex(0); return; }
        m_editingRow = row;
        emit modifyRequested(channel.id, name, category);
    });
    connect(nameInput, &QLineEdit::returnPressed, save, &QPushButton::click);
    connect(remove, &QPushButton::clicked, this, [this, channel] {
        InputDialog confirm(tr("删除频道「%1」？此操作不可撤销。").arg(channel.name), {}, this);
        if (auto *window = qobject_cast<QDialog *>(this->window()))
            connect(window, &QDialog::finished, &confirm, &QDialog::reject);
        if (confirm.exec() == QDialog::Accepted && isEnabled()) emit deleteRequested(channel.id);
    });
    return row;
}

void CommunityChannelsPage::created() { m_newChannel->clear(); }
void CommunityChannelsPage::modified() {
    if (auto *row = qobject_cast<QStackedWidget *>(m_editingRow)) row->setCurrentIndex(0);
    m_editingRow = nullptr;
}
