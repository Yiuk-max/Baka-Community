#pragma once
#include <QMainWindow>

class QLabel;
class QThread;
class QTimer;
class WsClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &serverUrl, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void startNetwork(const QString &serverUrl);
    void sendHeartbeat();                    // 业务层心跳（服务端没有 WS 自带 ping/pong）
    void handleFrame(const QByteArray &payload);

    QThread *m_netThread = nullptr;
    WsClient *m_client = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    QLabel *m_heartbeatLabel = nullptr;       // 状态栏右侧：显示收到的心跳
    qint64 m_heartbeatSentAtMs = 0;           // 心跳发送时间
};

