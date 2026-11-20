#pragma once
#include <QMainWindow>

namespace chat_proto {
class Envelope;
}
class ChatClient;
class ContactPanel;
class QLabel;
class SoundPlayer;
class ToolRail;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 网络由 ChatClient 统一负责（main 里创建，登录窗口和主窗口共用），这里只订阅它的信号
    explicit MainWindow(ChatClient *client, QWidget *parent = nullptr);

private:
    void requestLists();                                        // 登录成功后拉联系人和社区
    void handleEnvelope(const chat_proto::Envelope &envelope);  // 业务消息按 type 分给各控件

    ChatClient *m_client = nullptr;
    SoundPlayer *m_sound = nullptr;
    ToolRail *m_toolRail = nullptr;           // 左侧工具栏
    ContactPanel *m_contactPanel = nullptr;   // 左侧通讯录
    QWidget *m_centerPage = nullptr;          // 中间页面
    QLabel *m_heartbeatLabel = nullptr;       // 状态栏右侧：显示收到的心跳
};

