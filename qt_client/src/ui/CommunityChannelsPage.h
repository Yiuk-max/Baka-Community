#pragma once

#include "model/Contact.h"
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

// 频道行内编辑：无需先选中行，再去底部找操作。
class CommunityChannelsPage : public QWidget {
    Q_OBJECT
public:
    CommunityChannelsPage(int communityId, bool owner, QWidget *parent = nullptr);
    void setChannels(const QList<Channel> &channels);
    void created();
    void modified();

signals:
    void createRequested(const QString &name, const QString &category);
    void modifyRequested(int channelId, const QString &name, const QString &category);
    void deleteRequested(int channelId);

private:
    QWidget *makeRow(const Channel &channel);
    int m_communityId;
    bool m_owner;
    QLineEdit *m_newChannel = nullptr;
    QPushButton *m_create = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QLabel *m_empty = nullptr;
    QWidget *m_editingRow = nullptr;
};
