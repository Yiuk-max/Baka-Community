#include "CommunityChannelList.h"

#include "MenuStyle.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// 没填分区的频道都算“一般”
QString categoryOf(const Channel &channel) {
    return channel.category.trimmed().isEmpty() ? QObject::tr("一般") : channel.category.trimmed();
}

void clearLayout(QVBoxLayout *layout) {
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

}

CommunityChannelList::CommunityChannelList(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setObjectName("communityChannelScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *container = new QWidget;
    container->setObjectName("communityChannelContainer");
    m_rows = new QVBoxLayout(container);
    m_rows->setContentsMargins(8, 0, 8, 12);
    m_rows->setSpacing(2);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
}

void CommunityChannelList::setChannels(const QList<Channel> &channels, bool owner) {
    m_channels = channels;
    m_owner = owner;
    rebuild();
}

void CommunityChannelList::clear() {
    m_channels.clear();
    rebuild();
}

void CommunityChannelList::setActive(int channelId) {
    m_activeId = channelId;
    m_group->setExclusive(false);
    for (QAbstractButton *button : m_group->buttons()) {
        button->setChecked(button->property("channelId").toInt() == channelId);
    }
    m_group->setExclusive(true);
}

void CommunityChannelList::rebuild() {
    for (auto *button : m_group->buttons()) m_group->removeButton(button);
    clearLayout(m_rows);

    // 按 category 分组，保持服务端给的顺序（服务端按 category, id 排序）
    QList<QString> categories;
    for (const Channel &channel : m_channels) {
        const QString category = categoryOf(channel);
        if (!categories.contains(category)) categories.append(category);
    }

    for (const QString &category : categories) {
        auto *section = new QWidget;
        auto *headingLayout = new QHBoxLayout(section);
        headingLayout->setContentsMargins(0, 0, 0, 0);
        const QString collapseKey = QString::number(m_channels.first().communityId) + QLatin1Char(':') + category;
        const bool collapsed = m_collapsed.contains(collapseKey);
        auto *heading = new QPushButton((collapsed ? QStringLiteral("›  ") : QStringLiteral("v  ")) + category);
        heading->setObjectName("communityNavRow");
        heading->setFixedHeight(30);
        headingLayout->addWidget(heading, 1);
        auto *add = new QPushButton(QStringLiteral("＋"));
        add->setObjectName("communityIconButton");
        headingLayout->addWidget(add);
        connect(add, &QPushButton::clicked, this, &CommunityChannelList::createRequested);
        connect(heading, &QPushButton::clicked, this, [this, collapseKey] {
            if (m_collapsed.contains(collapseKey)) m_collapsed.remove(collapseKey);
            else m_collapsed.insert(collapseKey);
            rebuild();
        });
        m_rows->addSpacing(8);
        m_rows->addWidget(section);
        if (collapsed) continue;

        for (const Channel &channel : m_channels) {
            if (categoryOf(channel) != category) continue;

            auto *row = new QPushButton(QStringLiteral("#  %1").arg(channel.name));
            row->setObjectName("communityChannelRow");
            row->setCheckable(true);
            row->setFixedHeight(34);
            row->setCursor(Qt::PointingHandCursor);
            row->setToolTip(channel.name);
            row->setProperty("channelId", channel.id);
            m_group->addButton(row);
            m_rows->addWidget(row);

            const int channelId = channel.id;
            connect(row, &QPushButton::clicked, this, [this, channelId] {
                emit channelActivated(channelId);
            });

            row->setChecked(channel.id == m_activeId);
            // Web 的频道管理在社区设置中，不在频道行增加另一套右键操作。
        }
    }

    auto *empty = new QLabel(tr("暂无频道"));
    empty->setObjectName("communityChannelsEmpty");
    empty->setWordWrap(true);
    empty->setVisible(m_channels.isEmpty());
    m_rows->addSpacing(8);
    m_rows->addWidget(empty);
    m_rows->addStretch();
}
