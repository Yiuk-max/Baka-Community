#pragma once

// 主窗口左侧的通讯录：
//   搜索框 / “私信”标题 / 好友和群的列表 / 底部账号栏（含设置按钮）。
// 只负责显示和发信号，数据由 MainWindow 喂进来。
#include "model/Contact.h"

#include <QIcon>
#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;

class ContactPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContactPanel(QWidget *parent = nullptr);

    void setContacts(const QList<Contact> &contacts);   // 整份替换列表，尽量保留原来的选中项
    void setAccount(const QString &name, int uid);      // 底部账号栏：当前登录的账号
    void setOnline(bool online);                        // 底部账号栏：在线 / 离线
    void clear();                                       // 退出登录时清空

signals:
    void contactSelected(const Contact &contact);       // 用户点了某个好友 / 群
    void settingsClicked();                             // 点了账号栏右侧的设置按钮

private:
    void rebuildList();                                 // 按 m_contacts + 搜索关键字重建列表
    void onCurrentRowChanged(int row);

    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_accountName = nullptr;
    QLabel *m_accountStatus = nullptr;

    QList<Contact> m_contacts;
    // 还没做头像下载，好友 / 群各用一张默认头像，所以只生成一次
    QIcon m_userIcon;
    QIcon m_groupIcon;
    // 当前选中项，用 (isGroup, uid) 识别：好友 UID 和群 id 是两套编号，可能撞
    bool m_hasSelection = false;
    bool m_selectedIsGroup = false;
    int m_selectedUid = 0;
};
