#pragma once

// 主窗口最左侧的竖向工具栏
// 从上到下：程序 logo 按钮 / 分隔线 / 已加入的社区图标（多了可滚动）
#include "model/Contact.h"

#include <QList>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

class ToolRail : public QWidget {
    Q_OBJECT
public:
    explicit ToolRail(QWidget *parent = nullptr);

    void setCommunities(const QList<Community> &communities);   // 整份替换社区图标；传空列表即清空

signals:
    void communityClicked(const Community &community);          // 点了某个社区图标
    void homeClicked();          // 单击 logo：回到私聊 / 群聊页
    void logoDoubleClicked();    // 双击 logo：播放 Baka 音效

protected:
    // QPushButton 没有双击信号，用事件过滤器接
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QVBoxLayout *m_layout = nullptr;
    QPushButton *m_logoButton = nullptr;
    QVBoxLayout *m_communityLayout = nullptr;     // 滚动区里放社区图标的布局
    QList<QPushButton *> m_communityButtons;
};
