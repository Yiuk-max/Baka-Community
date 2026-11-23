#pragma once

// 社区频道列表（社区页左侧）：按分区分组显示频道，点一下切换频道
// 所有者可以右键频道重命名 / 删除；列表只发信号，请求由 MainWindow 发
#include "model/Contact.h"

#include <QList>
#include <QSet>
#include <QWidget>

class QButtonGroup;
class QVBoxLayout;

class CommunityChannelList : public QWidget {
    Q_OBJECT
public:
    explicit CommunityChannelList(QWidget *parent = nullptr);

    void setChannels(const QList<Channel> &channels, bool owner);
    void setActive(int channelId);   // 高亮某个频道；-1 = 一个都不选（显示服务器指南）
    void clear();

signals:
    void createRequested();
    void channelActivated(int channelId);
    void renameRequested(int channelId, const QString &name);
    void deleteRequested(int channelId, const QString &name);

private:
    void rebuild();

    QVBoxLayout *m_rows = nullptr;
    QButtonGroup *m_group = nullptr;   // 互斥高亮
    QList<Channel> m_channels;
    bool m_owner = false;
    QSet<QString> m_collapsed;
    int m_activeId = -1;
};
