#pragma once
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
class ToolRail;
struct Contact;

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

    ChatClient   *m_client       = nullptr;
    SoundPlayer  *m_sound        = nullptr;
    ToolRail     *m_toolRail     = nullptr;
    ContactPanel *m_contactPanel = nullptr;
    QStackedWidget *m_center     = nullptr;   // 中间区域：主页 / 聊天页 二选一
    HomePage     *m_homePage     = nullptr;   // 主页（好友页），登录后默认显示
    ChatPage     *m_chatPage     = nullptr;   // 中间聊天页
    QLabel       *m_heartbeatLabel = nullptr;
};

