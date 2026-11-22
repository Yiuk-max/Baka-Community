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
    // “＋”按钮菜单里的五项（和 web 端 .community-menu 一致）
    enum class AddAction { CreateCommunity, CreateGroup, JoinCommunity, JoinGroup, AddFriend };
    // 社区图标右键菜单里的项
    enum class CommunityAction { Invite, CreateChannel, Settings, Leave };

    explicit ToolRail(QWidget *parent = nullptr);

    void setCommunityAvatar(int id, const QPixmap &avatar);
    void setActiveCommunity(int id);   // 0 = 首页，社区 id = Web 竖栏当前项
    void setCommunities(const QList<Community> &communities);   // 整份替换社区图标；传空列表即清空

signals:
    void communityClicked(const Community &community);          // 点了某个社区图标
    void addActionTriggered(ToolRail::AddAction action);        // 在“＋”菜单里选了一项
    void communityActionTriggered(const Community &community, ToolRail::CommunityAction action);
    void homeClicked();          // 单击 logo：回到私聊 / 群聊页
    void logoDoubleClicked();    // 双击 logo：播放 Baka 音效

protected:
    // QPushButton 没有双击信号，用事件过滤器接
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showAddMenu();
    void showCommunityMenu(const Community &community, const QPoint &globalPos);

    int m_activeCommunityId = 0;
    QVBoxLayout *m_layout = nullptr;
    QPushButton *m_logoButton = nullptr;
    QPushButton *m_addButton = nullptr;           // 社区图标下面的“＋”
    QVBoxLayout *m_communityLayout = nullptr;     // 滚动区里放社区图标的布局
    QList<QPushButton *> m_communityButtons;
};
