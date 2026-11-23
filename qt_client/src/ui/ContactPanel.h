#pragma once

// 主窗口左侧的通讯录：
//   搜索框 / “私信”标题 / 好友和群的列表 / 底部账号栏
// 只负责显示和发信号，数据由 MainWindow 喂进来
#include "model/Contact.h"

#include <QIcon>
#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;

class ContactPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContactPanel(QWidget *parent = nullptr);

    void setContacts(const QList<Contact> &contacts);   // 整份替换列表，尽量保留原来的选中项
    void setAccount(const QString &name, int uid);      // 底部账号栏：当前登录的账号
    void setAccountAvatar(const QString &path);         // 底部账号栏：自己的头像文件；空 = 默认头像
    void setOnline(bool online);                        // 底部账号栏：在线 / 离线
    void clear();                                       // 退出登录时清空
    void select(const Contact &contact);                // 从别处（主页）打开了会话：把这一行标成选中，不发信号
    void clearSelection();                              // 回到主页时取消选中，这样再点同一行还能打开会话
    void setCommunitySidebar(QWidget *sidebar);         // 社区侧栏与私信内容原位切换，底部账号栏共用
    void showCommunitySidebar(bool community);

signals:
    void contactSelected(const Contact &contact);       // 用户点了某个好友 / 群
    void settingsClicked();                             // 点了账号栏右侧的设置按钮
    void inviteRequested(const Contact &group);         // 右键某个群 → 邀请好友

private:
    void showContextMenu(const QPoint &pos);            // 列表右键菜单（目前只有群有）
    void rebuildList();                                 // 按 m_contacts + 搜索关键字重建列表
    void onCurrentRowChanged(int row);

    QStackedWidget *m_sidebars = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_accountAvatar = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountStatus = nullptr;

    QList<Contact> m_contacts;
    // 好友有本地头像文件就用它（Contact::avatarPath），没有的好友共用默认头像；群还没有头像，统一用默认群头像
    QIcon m_userIcon;
    QIcon m_groupIcon;
    // 当前选中项，用 (isGroup, uid) 识别：好友 UID 和群 id 是两套编号，可能撞
    bool m_hasSelection = false;
    bool m_selectedIsGroup = false;
    int m_selectedUid = 0;
};
