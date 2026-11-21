#pragma once

// 主页（好友页）：登录后中间区域默认显示它，单击工具栏 logo 也回到这里。
//   顶部：“好友”标题 + 全部 / 待定 两个标签
//   全部：搜索框 / 好友列表 / 群聊列表 / 社区列表
// 只负责显示和发信号，数据由 MainWindow 喂进来（和 ContactPanel 用的是同一份）
#include "model/Contact.h"

#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class PendingPage;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);

    void setContacts(const QList<Contact> &contacts);          // 好友 + 群，整份替换
    void setCommunities(const QList<Community> &communities);  // 已加入的社区，整份替换
    PendingPage *pendingPage() const { return m_pendingPage; }  // “待定”页，申请数据直接喂给它
    void clear();                                              // 退出登录时清空（含待定页）

signals:
    void contactActivated(const Contact &contact);        // 点了某个好友 / 群：打开聊天
    void communityActivated(const Community &community);  // 点了某个社区
    void createGroupClicked();                            // 点了“＋ 新建群聊”

private:
    QWidget *buildHeader();
    QWidget *buildAllPage();
    // 往 column 里加一个小节（标题 + 说明），返回用来放这一节各行的布局
    QVBoxLayout *addSection(QVBoxLayout *column, const QString &title, const QString &hint);
    QPushButton *makeRow(const QPixmap &avatarPixmap, const QString &name, bool showArrow);
    void rebuildContacts();      // 按 m_contacts + 搜索关键字重建好友行和群聊行
    void rebuildCommunities();

    QStackedWidget *m_pages = nullptr;            // 0 = 全部，1 = 待定
    QLineEdit   *m_search = nullptr;
    QLabel      *m_friendsHeading = nullptr;      // “全部 — N”
    QLabel      *m_friendsEmpty = nullptr;
    QLabel      *m_communitiesEmpty = nullptr;
    QVBoxLayout *m_friendRows = nullptr;
    QVBoxLayout *m_groupRows = nullptr;
    QVBoxLayout *m_communityRows = nullptr;
    QPushButton *m_pendingTab = nullptr;          // “待定”标签，有申请时文字里带上数量
    PendingPage *m_pendingPage = nullptr;

    QList<Contact>   m_contacts;
    QList<Community> m_communities;
};
