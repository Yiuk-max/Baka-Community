#pragma once

// 业务层网络出口（跑在主线程）：页面只和它打交道，不直接碰 WsCore。
//   - 持有网络线程 + WsCore（WsCore 在工作线程里收发字节）
//   - 负责 Envelope 编解码、业务层心跳、登录 / 注册 / token 自动登录
//   - 其余消息原样通过 envelopeReceived 发给页面
#include "message.pb.h"

#include <QObject>
#include <QString>

class QThread;
class QTimer;
class WsCore;

class ChatClient : public QObject {
    Q_OBJECT
public:
    explicit ChatClient(const QString &serverUrl, QObject *parent = nullptr);
    ~ChatClient() override;

    void start();                                   // 起网络线程并连接服务器
    void send(const chat_proto::Envelope &envelope);

    QString serverUrl() const { return m_serverUrl; }
    bool isConnected() const { return m_connected; }
    int uid() const { return m_uid; }               // 0 = 未登录
    QString username() const { return m_username; }

public slots:
    // 账号：纯数字按 UID 登录，否则按邮箱登录
    void login(const QString &identity, const QString &password);
    void registerAccount(const QString &nickname, const QString &email, const QString &password);

signals:
    void connected();
    void disconnected(const QString &reason);
    void statusMessage(const QString &text);        // 连接状态之类的临时提示
    void heartbeatAcked(const QString &content, qint64 elapsedMs);

    void authStarted();                             // 登录 / 注册 / 自动登录请求已发出
    void loginSucceeded();
    void registerSucceeded(const QString &uid, const QString &email); // 注册不建立登录态，还要再登录
    void authFailed(const QString &message);

    void envelopeReceived(const chat_proto::Envelope &envelope);      // 上面没消化的消息

private:
    enum class Pending { None, Login, Register, Token };

    void onConnected();
    void onDisconnected(const QString &reason);
    void handleFrame(const QByteArray &payload);
    void handleLoginSuccess(const chat_proto::Envelope &envelope);
    void handleAuthReply(const QString &text);      // 认证在途时收到的 system 文本
    void sendHeartbeat();                           // 业务层心跳（服务端没有 WS 自带 ping/pong）
    bool beginAuth(Pending pending);
    void failAuth(const QString &message);

    QString m_serverUrl;
    QThread *m_netThread = nullptr;
    WsCore *m_core = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    QTimer *m_authTimer = nullptr;                  // 认证请求超时，防止界面一直卡在“请稍候”
    qint64 m_heartbeatSentAtMs = 0;                 // 心跳发送时间
    bool m_connected = false;
    Pending m_pending = Pending::None;
    int m_uid = 0;
    QString m_username;
};
