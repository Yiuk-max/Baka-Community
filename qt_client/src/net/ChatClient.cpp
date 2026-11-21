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

} // namespace

ChatClient::ChatClient(const QString &serverUrl, QObject *parent)
    : QObject(parent), m_serverUrl(serverUrl) {
    // 业务层心跳定时器（30s，与服务端 heartbeat_interval 默认值一致）
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ChatClient::sendHeartbeat);

    m_authTimer = new QTimer(this);
    m_authTimer->setSingleShot(true);
    m_authTimer->setInterval(10000);
    connect(m_authTimer, &QTimer::timeout, this,
            [this] { failAuth(tr("服务器响应超时，请重试")); });
}

ChatClient::~ChatClient() {
    // 先让工作线程里的 socket 收尾，再退出事件循环
    if (m_netThread && m_netThread->isRunning()) {
        QMetaObject::invokeMethod(m_core, "stop", Qt::BlockingQueuedConnection);
        m_netThread->quit();
        m_netThread->wait(3000);
    }
}

void ChatClient::start() {
    if (m_netThread) return;

    m_netThread = new QThread(this);
    m_core = new WsCore(QUrl(m_serverUrl));
    m_core->moveToThread(m_netThread);

    connect(m_netThread, &QThread::started, m_core, &WsCore::start);
    connect(m_netThread, &QThread::finished, m_core, &QObject::deleteLater);

    // WsCore 在工作线程，下面这些信号都是排队投递到主线程执行
    connect(m_core, &WsCore::logMessage, this, &ChatClient::statusMessage);
    connect(m_core, &WsCore::connected, this, &ChatClient::onConnected);
    connect(m_core, &WsCore::disconnected, this, &ChatClient::onDisconnected);
    // 收到的字节流统一在这里解 protobuf，再按 type 分发
    connect(m_core, &WsCore::frameReceived, this, &ChatClient::handleFrame);

    m_netThread->start();
}

void ChatClient::send(const chat_proto::Envelope &envelope) {
    const QByteArray frame = packetcodec::encode(envelope);
    // 跨线程：投递到网络线程执行，QByteArray 是已注册的元类型
    QMetaObject::invokeMethod(m_core, "sendFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
}

bool ChatClient::hasSavedToken() const {
    return !QSettings().value(QLatin1String(kTokenKey)).toString().isEmpty();
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

    // 服务端只是把这条连接变回未登录，连接本身保留，所以回到登录页可以直接再登录
    if (m_connected) {
        chat_proto::Envelope envelope;
        envelope.set_type("logout");
        send(envelope);
    }
    clearSession();
    emit loggedOut(false);
}

void ChatClient::send(const chat_proto::Envelope &envelope, const QByteArray &fileData) {
    const QByteArray frame = packetcodec::encode(envelope, fileData);
    QMetaObject::invokeMethod(m_core, "sendFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
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
bool ChatClient::handleProfileReply(const QString &text) {
    static const QRegularExpression renamed(
        QStringLiteral("^Name updated successfully\\. Your new name is \\[(.*)\\]\\.$"));
    static const QRegularExpression emailUpdated(
        QStringLiteral("^Email updated successfully\\. Your email is \\[(.*)\\]\\.$"));

    QRegularExpressionMatch match = renamed.match(text);
    if (match.hasMatch()) {
        m_username = match.captured(1);
        emit profileChanged();
        return true;
    }
    match = emailUpdated.match(text);
    if (match.hasMatch()) {
        m_email = match.captured(1);
        emit profileChanged();
        return true;
    }
    if (text.startsWith(QLatin1String("Failed to update name"))) {
        emit profileChangeFailed(tr("昵称修改失败，服务器暂时不可用"));
        return true;
    }
    if (text.startsWith(QLatin1String("Failed to set email"))) {
        emit profileChangeFailed(tr("邮箱修改失败，该邮箱可能已被其他账号使用"));
        return true;
    }
    // 头像：回执原文见后端 src/logic/handlers/auth_handler.cpp 的 set_avatar。
    // 只在 set_avatar 在途时才拦，否则会把文件下载的 “File not found” 也吞掉。
    if (m_pendingAvatarId > 0) {
        if (text.startsWith(QLatin1String("Avatar updated successfully"))) {
            m_avatarId = m_pendingAvatarId;
            m_pendingAvatarId = 0;
            emit avatarUpdated(m_avatarId);
            return true;
        }
        if (text.startsWith(QLatin1String("Failed to update avatar"))
                || text.startsWith(QLatin1String("Invalid file_id"))
                || text.startsWith(QLatin1String("File not found"))) {
            m_pendingAvatarId = 0;
            emit avatarUpdateFailed(tr("头像更新失败，请重试"));
            return true;
        }
    }
    return false;
}

void ChatClient::onConnected() {
    m_connected = true;
    // 连上就先发一次心跳，之后交给定时器
    sendHeartbeat();
    m_heartbeatTimer->start();
    emit connected();

    // 有认证请求在等这次连接（断线后点了登录 / 注册）：现在补发
    if (m_hasQueuedAuth) {
        m_hasQueuedAuth = false;
        send(m_queuedAuth);
        return;
    }

    // 本地存着 token 就自动登录（7 天滑动过期，失效时服务端回 system 文本）
    const QString token = QSettings().value(QLatin1String(kTokenKey)).toString();
    if (m_uid == 0 && !token.isEmpty()) {
        chat_proto::Envelope envelope;
        envelope.set_type("verify_token");
        envelope.set_token(token.toStdString());
        sendAuth(Pending::Token, envelope);
    }
}

void ChatClient::onDisconnected(const QString &reason) {
    m_connected = false;
    m_heartbeatTimer->stop();
    if (m_pending != Pending::None) failAuth(tr("连接已断开：%1").arg(reason));
    emit disconnected(reason);
}

void ChatClient::handleFrame(const QByteArray &payload) {
    chat_proto::Envelope envelope;
    QByteArray fileData;
    if (!packetcodec::decode(payload, envelope, &fileData)) {
        emit statusMessage(tr("收到无法解析的帧（%1 字节）").arg(payload.size()));
        return;
    }

    const QString type = QString::fromStdString(envelope.type());
    if (type == QStringLiteral("file_download_ready") || type == QStringLiteral("file_chunk")) {
        emit fileFrameReceived(envelope, fileData);
        return;
    }
    if (type == QStringLiteral("heartbeat_ack")) {
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_heartbeatSentAtMs;
        emit heartbeatAcked(QString::fromStdString(envelope.content()), elapsed);
        return;
    }
    if (type == QStringLiteral("login_success")) {
        handleLoginSuccess(envelope);
        return;
    }
    if (type == QStringLiteral("system")) {
        const QString text = QString::fromStdString(envelope.content()).trimmed();
        // 被顶下线：账号在别处登录，服务端发完这条就会关掉本连接
        if (m_uid != 0 && text.contains(QLatin1String("you have been kicked offline"))) {
            clearSession();
            emit loggedOut(true);
            return;
        }
        // 自己退出登录的回执，状态在 logout() 里已经处理完了
        if (text.contains(QLatin1String("Logout successful"))) return;
        // 未登录时服务端不会主动推送，所以认证在途期间收到的 system 就是这次请求的结果
        if (m_pending != Pending::None) {
            handleAuthReply(text);
            return;
        }
        if (handleProfileReply(text)) return;
    }

    emit envelopeReceived(envelope);
}

void ChatClient::handleLoginSuccess(const chat_proto::Envelope &envelope) {
    m_authTimer->stop();
    m_pending = Pending::None;
    m_uid = envelope.user().id();
    m_username = QString::fromStdString(envelope.user().username());
    m_email = QString::fromStdString(envelope.email());
    m_avatarId = envelope.avatar_id();
    QSettings().setValue(QLatin1String(kTokenKey), QString::fromStdString(envelope.token()));
    emit loginSucceeded();
}

void ChatClient::handleAuthReply(const QString &text) {
    const Pending pending = m_pending;
    m_authTimer->stop();
    m_pending = Pending::None;

    if (pending == Pending::Token) {
        // token 失效：清掉本地 token，回到手动登录
        QSettings().remove(QLatin1String(kTokenKey));
        emit authFailed(tr("登录已过期，请重新登录"));
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
    chat_proto::Envelope envelope;
    envelope.set_type("heartbeat");

    m_heartbeatSentAtMs = QDateTime::currentMSecsSinceEpoch();
    send(envelope);
}

void ChatClient::sendAuth(Pending pending, const chat_proto::Envelope &envelope) {
    if (m_pending != Pending::None) return; // 上一个请求还没回来

    m_pending = pending;
    m_authTimer->start();
    emit authStarted();

    if (m_connected) {
        send(envelope);
        return;
    }
    // 没连上（被顶下线后服务端关了连接 / 启动时没连上）：先存着，连上后在 onConnected 里补发。
    // 连不上的话 onDisconnected 会报错，一直没动静则由 m_authTimer 超时兜底
    m_queuedAuth = envelope;
    m_hasQueuedAuth = true;
    QMetaObject::invokeMethod(m_core, "reconnect", Qt::QueuedConnection);
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
    QSettings().remove(QLatin1String(kTokenKey));
}
