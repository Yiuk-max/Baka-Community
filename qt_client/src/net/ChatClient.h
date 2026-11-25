#pragma once

// 业务层网络出口（跑在主线程）：页面只和它打交道，不直接碰 WsCore。
//   - 持有网络线程 + WsCore（WsCore 在工作线程里收发字节）
//   - 负责 Envelope 编解码、业务层心跳、登录 / 注册 / token 自动登录 / 断线重连
//   - 其余消息原样通过 envelopeReceived 发给页面
//
// 连接和“业务已登录”分开：isConnected() 只表示底层传输在，isAuthenticated() 表示服务端已认证。
// 普通断网自动重连并用 token 重新认证；主动登出 / 被顶下线 / 程序关闭不重连。
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
    // 返回 false 表示当前状态不允许发送（未认证 / 传输断开），调用方据此决定是否做乐观 UI。
    bool send(const chat_proto::Envelope &envelope);
    // 发 Envelope + 帧尾原始二进制（file_chunk 上传分片用）
    bool send(const chat_proto::Envelope &envelope, const QByteArray &fileData);

    QString serverUrl() const { return m_serverUrl; }
    bool isConnected() const { return m_transportConnected; }
    bool isAuthenticated() const { return m_authenticated; }
    int uid() const { return m_uid; }               // 0 = 未登录
    QString username() const { return m_username; }
    QString email() const { return m_email; }       // 没绑定邮箱时为空
    int avatarId() const { return m_avatarId; }     // 自己头像的 file.id；<= 0 表示没设置
    bool hasSavedToken() const;                    // 本地存着 token：连上后会自动登录

public slots:
    // 账号：纯数字按 UID 登录，否则按邮箱登录
    void login(const QString &identity, const QString &password);
    void registerAccount(const QString &nickname, const QString &email, const QString &password);
    // 邮箱验证码：未登录也能发（服务端按请求里的 UID / 邮箱找账号）
    void verifyEmail(const QString &code, const QString &uid);
    void resendVerifyCode(const QString &email);
    void logout();                                  // 退出登录：通知服务端、清本地 token（连接保留）
    // 改昵称 / 改邮箱：只发请求，结果由 profileChanged / profileChangeFailed 通知
    void changeName(const QString &name);
    void changeEmail(const QString &email);
    // 头像：把 avatar_id 指向一个已经上传好的 file.id（文件要先用 AvatarUploader 传）
    void setAvatar(int fileId);
    // 用户退回到手动登录：停止后台 token 自动登录和在途的自动认证，避免打扰正在输入的用户
    void cancelAutomaticLogin();

signals:
    void connected();                               // 底层传输已建立（可能还没认证）
    void disconnected(const QString &reason);       // 底层传输已断开
    void readyChanged(bool ready);                  // 传输 + 认证都就绪时变为 true
    void reconnecting(bool active, int attempt, int delaySeconds);   // 自动重连的调度状态
    void statusMessage(const QString &text);        // 连接状态之类的临时提示
    void heartbeatAcked(const QString &content, qint64 elapsedMs);

    void authStarted();                             // 登录 / 注册 / 自动登录请求已发出
    void loginSucceeded();                          // 首次登录成功（新账号 / 手动登录）
    void sessionRestored();                         // 断线后同账号 token 认证成功（不是首次登录）
    void registerSucceeded(const QString &uid, const QString &email); // 注册不建立登录态，还要再登录
    // 登录被服务端拦住：密码对但邮箱还没验证，需要先输入验证码
    void emailVerificationRequired(const QString &uid, const QString &email);
    void verifySucceeded();                         // 邮箱验证通过（服务端回 Email verified successfully）
    void verifyCodeSent(int cooldownSeconds);       // 验证码已重新寄出，客户端开始冷却倒计时
    void verifyCooldown(int seconds);               // 服务端还在冷却，回一个剩余秒数
    void authFailed(const QString &message);
    // 已回到未登录状态。kicked = true 表示是被顶下线（账号在别处登录），否则是自己退出
    void loggedOut(bool kicked);
    void profileChanged();                          // 昵称或邮箱已改成功，username() / email() 是新值
    void profileChangeFailed(const QString &message);
    // 头像：setAvatar() 请求成功 / 失败（成功后 avatarId() 是新值）
    void avatarUpdated(int fileId);
    void avatarUpdateFailed(const QString &message);

    void envelopeReceived(const chat_proto::Envelope &envelope);      // 上面没消化的消息
    // 文件下载的两种回复（file_download_ready / file_chunk）；fileData 是分片带的文件数据
    void fileFrameReceived(const chat_proto::Envelope &envelope, const QByteArray &fileData);

private:
    enum class Pending { None, Login, Register, Token, Verify, Resend };

    bool rawSend(const chat_proto::Envelope &envelope);   // 只发字节，不检查认证（认证消息用）
    QString savedToken() const;
    void onConnected(int generation);
    void onDisconnected(int generation, const QString &reason);
    void onSendResult(int generation, bool accepted);
    void handleTransportLost(const QString &reason);      // 心跳超时 / 心跳失败主动判定失联
    void handleFrame(int generation, const QByteArray &payload);
    void handleLoginSuccess(const chat_proto::Envelope &envelope);
    void handleAuthReply(const QString &text);      // 认证在途时收到的 system 文本
    bool handleProfileReply(const QString &text);   // 改昵称 / 邮箱的回执；是的话返回 true
    void sendHeartbeat();                           // 业务层心跳（服务端没有 WS 自带 ping/pong）
    void beginHeartbeat();
    void stopHeartbeat();
    // 发出认证请求；没连上就先重连，连上后再发
    void sendAuth(Pending pending, const chat_proto::Envelope &envelope);
    void failAuth(const QString &message);
    void clearSession();                            // 清掉当前账号和本地 token
    void scheduleReconnect();                       // 指数退避后重连
    void requestConnect();                          // 立刻发起一次连接尝试（取消退避定时器）
    bool tokenInvalidText(const QString &text) const;

    QString m_serverUrl;
    QThread *m_netThread = nullptr;
    WsCore *m_core = nullptr;
    QTimer *m_heartbeatTimer = nullptr;             // 30s 周期心跳
    QTimer *m_heartbeatWatchdog = nullptr;          // 心跳应答超时（静默断线检测）
    QTimer *m_authTimer = nullptr;                  // 认证请求超时，防止界面一直卡在“请稍候”
    QTimer *m_reconnectTimer = nullptr;             // 退避重连定时器
    QTimer *m_stableTimer = nullptr;                // 认证稳定一段时间后重置退避计数

    int m_transportGeneration = 0;                  // 当前已连接 socket 的代次；0 = 无有效连接
    bool m_transportConnected = false;
    bool m_authenticated = false;
    bool m_autoRecover = true;                      // 登出 / 被顶下线 / 关闭后置 false
    bool m_tokenAutoLogin = false;                  // 是否允许后台用 token 自动登录
    bool m_stopping = false;                        // 析构 / 停止后不再调度
    int m_reconnectAttempts = 0;
    qint64 m_heartbeatSentAtMs = 0;                 // 心跳发送时间

    Pending m_pending = Pending::None;
    bool m_hasQueuedAuth = false;                   // 认证请求在等连接建立
    chat_proto::Envelope m_queuedAuth;

    int m_uid = 0;
    QString m_username;
    QString m_email;
    QString m_verifyEmail;                          // 登录被拦住时记下的邮箱（重发验证码要用）
    int m_avatarId = 0;
    int m_pendingAvatarId = 0;                      // set_avatar 在途时等激活的 file.id
};
