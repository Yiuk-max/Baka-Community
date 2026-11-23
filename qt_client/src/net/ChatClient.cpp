#include "ChatClient.h"

#include "PacketCodec.h"
#include "WsCore.h"

#include <QDateTime>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace {

const char *kTokenKey = "auth/token";

// 后端没有结构化错误码，只能按 system 文本判断（原文见后端 src/logic/handlers/auth_handler.cpp）
QString translateAuthError(const QString &text) {
    if (text.contains(QLatin1String("does not exist"))) return ChatClient::tr("账号不存在");
    if (text.contains(QLatin1String("Incorrect password"))) return ChatClient::tr("密码错误");
    if (text.contains(QLatin1String("No account is bound"))) return ChatClient::tr("该邮箱没有绑定账号");
    if (text.contains(QLatin1String("already registered"))) return ChatClient::tr("该邮箱已被注册");
    if (text.contains(QLatin1String("Registration failed"))) return ChatClient::tr("注册失败，服务器暂时不可用");
    if (text.contains(QLatin1String("Invalid or expired verification code"))) return ChatClient::tr("验证码无效或已过期");
    if (text.contains(QLatin1String("Verification code cannot be empty"))) return ChatClient::tr("请输入验证码");
    if (text.contains(QLatin1String("Failed to send verification email"))) return ChatClient::tr("验证码邮件发送失败，请稍后重试");
    if (text.contains(QLatin1String("No email is bound"))) return ChatClient::tr("该账号还没有绑定邮箱");
    if (text.contains(QLatin1String("provide your email"))) return ChatClient::tr("请填写邮箱后再获取验证码");
    return text;
}

int heartbeatIntervalMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("BAKA_HEARTBEAT_INTERVAL_MS", &ok);
    return ok && value > 0 ? value : 30000;
}
int heartbeatWatchdogMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("BAKA_HEARTBEAT_TIMEOUT_MS", &ok);
    return ok && value > 0 ? value : 15000;
}

} // namespace

ChatClient::ChatClient(const QString &serverUrl, QObject *parent)
    : QObject(parent), m_serverUrl(serverUrl) {
    // 业务层心跳定时器（服务端 heartbeat_interval 默认 60s，客户端按 30s 发）
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(heartbeatIntervalMs());
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ChatClient::sendHeartbeat);

    // 心跳没有应答 = 连接可能已经“半死”（Wi-Fi 断开、对端静默），主动判定失联
    m_heartbeatWatchdog = new QTimer(this);
    m_heartbeatWatchdog->setSingleShot(true);
    m_heartbeatWatchdog->setInterval(heartbeatWatchdogMs());
    connect(m_heartbeatWatchdog, &QTimer::timeout, this, [this] {
        // 半死连接：先作废网络层旧 socket，再走统一失联处理
        QMetaObject::invokeMethod(m_core, "abort", Qt::QueuedConnection);
        handleTransportLost(tr("心跳超时"));
    });

    m_authTimer = new QTimer(this);
    m_authTimer->setSingleShot(true);
    m_authTimer->setInterval(10000);
    connect(m_authTimer, &QTimer::timeout, this, [this] {
        // 自动 token 认证超时只是本次没成功，退避重试；用户主动登录超时则回给界面
        if (m_pending == Pending::Token) {
            m_pending = Pending::None;
            emit statusMessage(tr("自动登录超时"));
            scheduleReconnect();
            return;
        }
        failAuth(tr("服务器响应超时，请重试"));
    });

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatClient::requestConnect);

    // 认证稳定 30s 后把退避计数清零；刚连上就断不能把重试间隔打回 1s
    m_stableTimer = new QTimer(this);
    m_stableTimer->setSingleShot(true);
    m_stableTimer->setInterval(30000);
    connect(m_stableTimer, &QTimer::timeout, this, [this] { m_reconnectAttempts = 0; });
}

ChatClient::~ChatClient() {
    m_stopping = true;
    m_autoRecover = false;
    m_reconnectTimer->stop();
    m_stableTimer->stop();
    m_heartbeatTimer->stop();
    m_heartbeatWatchdog->stop();
    m_authTimer->stop();

    // 先让工作线程里的 socket 收尾，再退出事件循环
    if (m_netThread && m_netThread->isRunning()) {
        QMetaObject::invokeMethod(m_core, "stop", Qt::BlockingQueuedConnection);
        m_netThread->quit();
        m_netThread->wait(3000);
    }
}

void ChatClient::start() {
    if (m_netThread) return;
    m_tokenAutoLogin = hasSavedToken();

    m_netThread = new QThread(this);
    m_core = new WsCore(QUrl(m_serverUrl));
    m_core->moveToThread(m_netThread);

    connect(m_netThread, &QThread::started, m_core, &WsCore::start);
    connect(m_netThread, &QThread::finished, m_core, &QObject::deleteLater);

    // WsCore 在工作线程，下面这些信号都是排队投递到主线程执行
    connect(m_core, &WsCore::logMessage, this, &ChatClient::statusMessage);
    connect(m_core, &WsCore::connected, this, &ChatClient::onConnected);
    connect(m_core, &WsCore::disconnected, this, &ChatClient::onDisconnected);
    connect(m_core, &WsCore::sendResult, this, &ChatClient::onSendResult);
    // 收到的字节流统一在这里解 protobuf，再按 type 分发
    connect(m_core, &WsCore::frameReceived, this, &ChatClient::handleFrame);

    m_netThread->start();
}

bool ChatClient::send(const chat_proto::Envelope &envelope) {
    return send(envelope, QByteArray());
}

bool ChatClient::send(const chat_proto::Envelope &envelope, const QByteArray &fileData) {
    const QString type = QString::fromStdString(envelope.type());
    const bool internal = type == QLatin1String("heartbeat") || type == QLatin1String("logout");
    // 业务消息必须已认证且传输在；认证消息走 sendAuth / rawSend，不经过这里
    if (!internal && !m_authenticated) return false;
    if (!m_transportConnected || m_transportGeneration <= 0) return false;

    const QByteArray frame = packetcodec::encode(envelope, fileData);
    // 跨线程：投递到网络线程执行，QByteArray 是已注册的元类型
    QMetaObject::invokeMethod(m_core, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(int, m_transportGeneration), Q_ARG(QByteArray, frame));
    return true;
}

// 只发字节，不做业务认证检查（login / register / verify_token / heartbeat 等认证前消息用）
bool ChatClient::rawSend(const chat_proto::Envelope &envelope) {
    if (!m_transportConnected || m_transportGeneration <= 0) return false;
    const QByteArray frame = packetcodec::encode(envelope);
    QMetaObject::invokeMethod(m_core, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(int, m_transportGeneration), Q_ARG(QByteArray, frame));
    return true;
}

bool ChatClient::hasSavedToken() const {
    return !savedToken().isEmpty();
}

QString ChatClient::savedToken() const {
    return QSettings().value(QLatin1String(kTokenKey)).toString();
}

void ChatClient::login(const QString &identity, const QString &password) {
    chat_proto::Envelope envelope;
    envelope.set_type("login");
    envelope.set_password(password.toStdString());
    // 服务端规则：email 非空按邮箱登录，否则按 UID 登录
    bool isUid = false;
    const int uid = identity.toInt(&isUid);
    if (isUid) {
        envelope.set_uid(uid);
    } else {
        envelope.set_email(identity.toStdString());
    }
    // 记下邮箱：登录被“邮箱未验证”拦住时，重发验证码还得用它
    m_verifyEmail = isUid ? QString() : identity;
    sendAuth(Pending::Login, envelope);
}

void ChatClient::registerAccount(const QString &nickname, const QString &email,
                                 const QString &password) {
    chat_proto::Envelope envelope;
    envelope.set_type("register");
    envelope.set_username(nickname.toStdString());
    envelope.set_email(email.toStdString());
    envelope.set_password(password.toStdString());
    sendAuth(Pending::Register, envelope);
}

// 提交邮箱验证码；未登录时服务端靠 uid 找账号
void ChatClient::verifyEmail(const QString &code, const QString &uid) {
    chat_proto::Envelope envelope;
    envelope.set_type("verify_email");
    envelope.set_code(code.toStdString());
    envelope.set_uid(uid.toInt());
    sendAuth(Pending::Verify, envelope);
}

// 重新发送验证码；未登录时服务端靠 email 找账号
void ChatClient::resendVerifyCode(const QString &email) {
    chat_proto::Envelope envelope;
    envelope.set_type("resend_verify");
    envelope.set_email(email.toStdString());
    sendAuth(Pending::Resend, envelope);
}

void ChatClient::logout() {
    if (m_uid == 0) return;

    // 退出登录后不再自动重连 / 自动登录
    m_autoRecover = false;
    m_tokenAutoLogin = false;
    m_reconnectTimer->stop();

    // 服务端只是把这条连接变回未登录，连接本身保留，所以回到登录页可以直接再登录
    if (m_transportConnected) {
        chat_proto::Envelope envelope;
        envelope.set_type("logout");
        rawSend(envelope);
    }
    clearSession();
    emit loggedOut(false);
}

void ChatClient::cancelAutomaticLogin() {
    m_tokenAutoLogin = false;
    if (m_pending == Pending::Token) {
        m_authTimer->stop();
        m_pending = Pending::None;
    }
}

void ChatClient::changeName(const QString &name) {
    chat_proto::Envelope envelope;
    envelope.set_type("change_name");
    envelope.set_new_name(name.toStdString());
    send(envelope);
}

void ChatClient::changeEmail(const QString &email) {
    chat_proto::Envelope envelope;
    envelope.set_type("set_email");
    envelope.set_email(email.toStdString());
    send(envelope);
}

// 头像：把 avatar_id 指向一个已经上传好的文件；回执成功后才改本地缓存
void ChatClient::setAvatar(int fileId) {
    if (fileId <= 0) return;
    m_pendingAvatarId = fileId;

    chat_proto::Envelope envelope;
    envelope.set_type("set_avatar");
    envelope.set_file_id(std::to_string(fileId));   // 协议里 file_id 是字符串
    send(envelope);
}

// 回执原文见后端 src/logic/handlers/auth_handler.cpp 的 change_my_name / set_email

void ChatClient::handleAuthReply(const QString &text) {
    const Pending pending = m_pending;
    m_authTimer->stop();
    m_pending = Pending::None;

    if (pending == Pending::Token) {
        if (tokenInvalidText(text)) {
            const bool reconnected = (m_uid > 0);
            m_tokenAutoLogin = false;
            m_reconnectTimer->stop();
            clearSession();
            if (reconnected) {
                m_autoRecover = false;
                emit loggedOut(false);   // 复用“回到登录页”的现有流程
            } else {
                emit authFailed(tr("登录已过期，请重新登录"));
            }
            return;
        }
        // 不是明确的失效：保留 token，退避后重试（可能是服务端瞬时异常）
        emit statusMessage(tr("自动登录失败：%1").arg(text));
        scheduleReconnect();
        return;
    }
    if (pending == Pending::Login) {
        // "Email not verified for UID 000001. Please enter the verification code ..."
        // 密码已经对了，只是邮箱没验证：让 UI 切到验证码页（不当作登录失败）
        static const QRegularExpression notVerified(
            QStringLiteral("Email not verified for UID (\\d+)\\."));
        const QRegularExpressionMatch match = notVerified.match(text);
        if (match.hasMatch()) {
            emit emailVerificationRequired(match.captured(1), m_verifyEmail);
            return;
        }
    }
    if (pending == Pending::Register) {
        // "Registration successful. You can now log in with UID 000001 or a@b.com."
        static const QRegularExpression pattern(//正则表达式用来匹配服务器返回的注册成功消息
            QStringLiteral("Registration successful.*UID (\\d+) or (\\S+)\\.$"));
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch()) {
            emit registerSucceeded(match.captured(1), match.captured(2));
            return;
        }
    }
    if (pending == Pending::Verify) {
        if (text.contains(QLatin1String("Email verified successfully"))) {
            emit verifySucceeded();
            return;
        }
    }
    if (pending == Pending::Resend) {
        // "Verification email sent to [a@b.com]."/"Please wait 42 seconds before requesting another code."
        static const QRegularExpression sent(
            QStringLiteral("Verification email sent to \\[(.*)\\]\\."));
        static const QRegularExpression wait(
            QStringLiteral("Please wait (\\d+) seconds before requesting another code"));
        const QRegularExpressionMatch sentMatch = sent.match(text);
        if (sentMatch.hasMatch()) {
            m_verifyEmail = sentMatch.captured(1);
            emit verifyCodeSent(60);
            return;
        }
        const QRegularExpressionMatch waitMatch = wait.match(text);
        if (waitMatch.hasMatch()) {
            emit verifyCooldown(waitMatch.captured(1).toInt());
            return;
        }
    }
    emit authFailed(translateAuthError(text));
}

void ChatClient::sendHeartbeat() {
    if (!m_authenticated || !m_transportConnected) return;
    chat_proto::Envelope envelope;
    envelope.set_type("heartbeat");

    m_heartbeatSentAtMs = QDateTime::currentMSecsSinceEpoch();
    m_heartbeatWatchdog->start();
    rawSend(envelope);
}

void ChatClient::beginHeartbeat() {
    m_heartbeatTimer->start();
    sendHeartbeat();
}

void ChatClient::stopHeartbeat() {
    m_heartbeatTimer->stop();
    m_heartbeatWatchdog->stop();
}

void ChatClient::sendAuth(Pending pending, const chat_proto::Envelope &envelope) {
    if (m_pending != Pending::None) return; // 上一个请求还没回来

    m_pending = pending;
    m_authTimer->start();
    emit authStarted();

    if (m_transportConnected) {
        rawSend(envelope);
        return;
    }
    // 没连上：先存着，连上后在 onConnected 里补发。连不上由退避重连兜底
    m_queuedAuth = envelope;
    m_hasQueuedAuth = true;
    requestConnect();
}

void ChatClient::failAuth(const QString &message) {
    m_authTimer->stop();
    m_pending = Pending::None;
    m_hasQueuedAuth = false;
    emit authFailed(message);
}

void ChatClient::clearSession() {
    m_uid = 0;
    m_username.clear();
    m_email.clear();
    m_verifyEmail.clear();
    m_avatarId = 0;
    m_pendingAvatarId = 0;
    m_authenticated = false;
    m_hasQueuedAuth = false;
    QSettings().remove(QLatin1String(kTokenKey));
    emit readyChanged(false);
}
