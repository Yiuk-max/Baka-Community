// ChatClient 的入站消息处理：帧解码、登录成功、认证回执与资料回执。
// 单独一个文件是为了让连接生命周期（ChatClient.cpp / ChatClientReconnect.cpp）保持清晰。
#include "ChatClient.h"

#include "PacketCodec.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

namespace {
const char *kTokenKey = "auth/token";   // 与 ChatClient.cpp 里同一把 QSettings 键
}

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

void ChatClient::handleFrame(int generation, const QByteArray &payload) {
    if (generation != m_transportGeneration) return;   // 旧代次的迟到帧

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
        m_heartbeatWatchdog->stop();
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
            m_autoRecover = false;
            m_tokenAutoLogin = false;
            m_reconnectTimer->stop();
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
    if (m_pending == Pending::None) return;   // 不是我们期待的认证回执（已取消 / 过期）
    const bool reconnected = (m_uid > 0);
    const int newUid = envelope.user().id();

    if (reconnected && newUid != m_uid) {
        // 同一个 token 恢复了另一个账号：按凭证异常处理，回到登录页
        m_authTimer->stop();
        m_pending = Pending::None;
        m_autoRecover = false;
        m_tokenAutoLogin = false;
        clearSession();
        emit loggedOut(false);
        return;
    }

    m_authTimer->stop();
    m_pending = Pending::None;
    m_uid = newUid;
    m_username = QString::fromStdString(envelope.user().username());
    m_email = QString::fromStdString(envelope.email());
    m_avatarId = envelope.avatar_id();
    QSettings().setValue(QLatin1String(kTokenKey), QString::fromStdString(envelope.token()));

    m_authenticated = true;
    emit readyChanged(true);
    beginHeartbeat();
    m_stableTimer->start();

    if (reconnected) {
        m_reconnectAttempts = 0;   // 真正认证成功，本次恢复已完成
        emit sessionRestored();
    } else {
        emit loginSucceeded();
    }
}
