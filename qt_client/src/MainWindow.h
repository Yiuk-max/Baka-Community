#pragma once
#include "ui/ToolRail.h"   // 要用到它的 AddAction / CommunityAction，以及 Contact / Community

#include <QMainWindow>

namespace chat_proto {
class Envelope;
}
class ChatClient;
class ChatPage;
class ContactPanel;
class HomePage;
class QLabel;
class QStackedWidget;
class SoundPlayer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 网络由 ChatClient 统一负责（main 里创建，登录窗口和主窗口共用），这里只订阅它的信号
    explicit MainWindow(ChatClient *client, QWidget *parent = nullptr);

private:
    void openSettings();                                        // 打开设置窗口
    void requestLists();                                       // 登录成功后拉联系人和社区
    void handleEnvelope(const chat_proto::Envelope &envelope);  // 业务消息按 type 分给各控件
    void openChat(const Contact &contact);                      // 中间区域切到聊天页并打开这个会话
    void showHome();                                            // 中间区域切回主页

    void request(const char *type);                             // 发一个只有 type 的请求（show_contacts 之类）
    // 弹输入框；点了确定返回 true，values 里是各个输入框的内容。labels 为空时就是确认框
    bool ask(const QString &title, const QStringList &labels, QStringList *values = nullptr);
    void createGroup();
    void handleAddAction(ToolRail::AddAction action);           // 工具栏“＋”菜单
    void handleCommunityAction(const Community &community, ToolRail::CommunityAction action);  // 社区右键菜单
    bool handleFriendRequestList(const QString &text);          // show_friend_requests 的文本回复；是的话返回 true
    void refreshAfter(const QString &text);                     // 某些 system 通知说明列表变了，重新拉一次

    // 下面几个的实现在 MainWindowRequests.cpp
    void inviteFriends(const QString &title, bool toCommunity, int targetId);   // 弹好友列表，点谁邀请谁
    void requestGroupRequests();                                // 拉每个群的入群申请
    void requestCommunityRequests();                            // 拉自己是所有者的社区的入社区申请
    bool handleJoinRequestList(const QString &text);            // 上面两个请求的文本回复；是的话返回 true
    void handleJoinDecision(const JoinRequest &request, bool accept);   // “待定”页点了 接受 / 拒绝

    QList<Contact>   m_contacts;      // 最近一次拉到的好友 + 群（邀请弹窗、申请列表里的群名要用）
    QList<Community> m_communities;

    ChatClient   *m_client       = nullptr;
    SoundPlayer  *m_sound        = nullptr;
    ToolRail     *m_toolRail     = nullptr;
    ContactPanel *m_contactPanel = nullptr;
    QStackedWidget *m_center     = nullptr;   // 中间区域：主页 / 聊天页 二选一
    HomePage     *m_homePage     = nullptr;   // 主页（好友页），登录后默认显示
    ChatPage     *m_chatPage     = nullptr;   // 中间聊天页
    QLabel       *m_heartbeatLabel = nullptr;
};

