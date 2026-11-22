#pragma once

// 社区的“服务器指南”页：大头像 + 社区名 + 简介 + 成员 / 频道统计 + 邀请 / 设置 / 离开按钮。
// 只负责显示和发信号；数据由 CommunityPage 喂进来，请求由 MainWindow 发。
#include "model/Contact.h"

#include <QWidget>

class QLabel;
class QPushButton;

class CommunityGuidePage : public QWidget {
    Q_OBJECT
public:
    explicit CommunityGuidePage(QWidget *parent = nullptr);

    // 名称 / 简介 / 身份来自社区，成员数、频道数由页面统计后传进来
    void setCommunity(const Community &community, int memberCount, int channelCount);
    void setAssets(const QPixmap &avatar, const QPixmap &banner);

signals:
    void inviteRequested();
    void settingsRequested();
    void leaveRequested();      // 所有者是“解散社区”，成员是“离开社区”

private:
    QLabel *m_avatar = nullptr;
    QLabel *m_banner = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_stats = nullptr;
    QPushButton *m_settings = nullptr;
    QPushButton *m_leave = nullptr;
};
