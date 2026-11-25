// ChatClient 的断线重连与连接生命周期；单独一个文件是为了让认证/编解码（ChatClient.cpp）保持清晰。
#include "ChatClient.h"

#include "WsCore.h"

#include <QMetaObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>

namespace {

// 测试可用环境变量缩短退避，正常运行时用默认 1s 起步、30s 封顶
int reconnectBaseMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("BAKA_RECONNECT_BASE_MS", &ok);
    return ok && value > 0 ? value : 1000;
}
int reconnectCapMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("BAKA_RECONNECT_MAX_MS", &ok);
    return ok && value > 0 ? value : 30000;
}

} // namespace

void ChatClient::onConnected(int generation) {
    m_transportGeneration = generation;
    m_transportConnected = true;
    emit connected();

    // 有认证请求在等这次连接（断线后点了登录 / 注册）：现在补发
    if (m_hasQueuedAuth) {
        m_hasQueuedAuth = false;
        rawSend(m_queuedAuth);
        return;
    }

    // 之前已经登录过：断线重连后必须重新验证 token，不能把“连上了”当“已登录”
    if (m_uid > 0) {
        const QString token = savedToken();
        if (token.isEmpty()) {
            handleTransportLost(tr("本地登录凭证已失效"));
            return;
        }
        chat_proto::Envelope envelope;
        envelope.set_type("verify_token");
        envelope.set_token(token.toStdString());
        sendAuth(Pending::Token, envelope);
        return;
    }

    // 首次启动且本地有 token：自动登录
    if (m_tokenAutoLogin) {
        const QString token = savedToken();
        if (!token.isEmpty()) {
            chat_proto::Envelope envelope;
            envelope.set_type("verify_token");
            envelope.set_token(token.toStdString());
            sendAuth(Pending::Token, envelope);
        }
    }
}

void ChatClient::onDisconnected(int generation, const QString &reason) {
    Q_UNUSED(generation);   // WsCore 已经过滤掉旧代次，能到这里的都是当前尝试
    if (m_stopping) return;
    handleTransportLost(reason);
}

void ChatClient::onSendResult(int generation, bool accepted) {
    if (!accepted && generation == m_transportGeneration)
        emit statusMessage(tr("发送失败：连接已断开"));
}

// 心跳超时主动判定的失联，与网络层报上来的断线共用同一条收口逻辑
void ChatClient::handleTransportLost(const QString &reason) {
    m_transportConnected = false;
    m_transportGeneration = 0;   // 旧代次的迟到帧 / 待发送帧全部作废
    m_authenticated = false;
    stopHeartbeat();
    emit readyChanged(false);

    // 用户主动发起的认证被断线打断：回给登录界面；自动 token 认证保持安静并退避重试
    if (m_pending == Pending::Login || m_pending == Pending::Register
            || m_pending == Pending::Verify || m_pending == Pending::Resend) {
        failAuth(tr("连接已断开：%1").arg(reason));
    } else if (m_pending == Pending::Token) {
        m_authTimer->stop();
        m_pending = Pending::None;
    }

    emit disconnected(reason);

    if (m_autoRecover && !m_stopping) scheduleReconnect();
}

void ChatClient::requestConnect() {
    if (m_stopping || !m_autoRecover) return;
    m_reconnectTimer->stop();
    emit reconnecting(false, m_reconnectAttempts, 0);
    QMetaObject::invokeMethod(m_core, "start", Qt::QueuedConnection);
}

void ChatClient::scheduleReconnect() {
    if (m_stopping || !m_autoRecover) return;
    if (m_reconnectTimer->isActive()) return;   // 已有一个退避定时器在跑

    const int base = reconnectBaseMs();
    const int cap = qMax(base, reconnectCapMs());
    qint64 delay = qint64(base) << qMin(m_reconnectAttempts, 20);
    if (delay > cap) delay = cap;
    // ±20% 抖动，避免多端同一时刻重连
    const int jitterPercent = int(QRandomGenerator::global()->bounded(80, 121));
    delay = qMax<qint64>(base, delay * jitterPercent / 100);
    if (delay > cap) delay = cap;

    ++m_reconnectAttempts;
    emit reconnecting(true, m_reconnectAttempts, int(delay / 1000));
    m_reconnectTimer->start(int(delay));
}

bool ChatClient::tokenInvalidText(const QString &text) const {
    return text.contains(QLatin1String("Invalid or expired token"))
        || text.contains(QLatin1String("Token expired"))
        || text.contains(QLatin1String("Token is required"));
}
