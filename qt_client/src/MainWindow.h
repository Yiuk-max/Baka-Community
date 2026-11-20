#pragma once
#include <QMainWindow>

class ChatClient;
class QLabel;
class SoundPlayer;
class ToolRail;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 网络由 ChatClient 统一负责（main 里创建，登录窗口和主窗口共用），这里只订阅它的信号
    explicit MainWindow(ChatClient *client, QWidget *parent = nullptr);

private:
    ChatClient *m_client = nullptr;
    SoundPlayer *m_sound = nullptr;
    ToolRail *m_toolRail = nullptr;           // 左侧工具栏
    QWidget *m_contactPanel = nullptr;        // 左侧通讯录
    QWidget *m_centerPage = nullptr;          // 中间页面
    QLabel *m_heartbeatLabel = nullptr;       // 状态栏右侧：显示收到的心跳
};

