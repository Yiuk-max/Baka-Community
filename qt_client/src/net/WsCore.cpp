#include "WsCore.h"

#include <QAbstractSocket>
#include <QTimer>
#include <QWebSocket>

namespace {
// 测试可用环境变量缩短超时；正常运行时默认 10s
int connectTimeoutMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("BAKA_CONNECT_TIMEOUT_MS", &ok);
    return ok && value > 0 ? value : 10000;
}
}

WsCore::WsCore(const QUrl &url, QObject *parent) : QObject(parent), m_url(url) {
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(connectTimeoutMs());
    connect(m_connectTimer, &QTimer::timeout, this, [this] {
        const int generation = m_generation;
        m_lastError = tr("连接超时");
        // abort() 会触发本代次的 disconnected，由 emitLoss 去重后只报一次
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectingState) m_socket->abort();
        emitLoss(generation, m_lastError);
    });
}

WsCore::~WsCore() {
    m_stopped = true;
    ++m_generation;
    m_connectTimer->stop();
    teardownSocket();
}

void WsCore::start() {
    m_stopped = false;
    ++m_generation;        // 先作废旧代次的回调，再拆旧 socket，避免拆的时候漏报一次 disconnected
    teardownSocket();      // 换一次新尝试前，先把旧 socket 关掉
    m_lostEmitted = false;
    m_lastError.clear();
    const int generation = m_generation;

    // 必须在当前线程（工作线程）创建，之后所有回调都在这个线程里执行
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::connected, this, [this, generation] {
        if (generation != m_generation || m_stopped) return;
        m_connectTimer->stop();
        emit connected(generation);
    });
    connect(m_socket, &QWebSocket::disconnected, this, [this, generation] {
        if (generation != m_generation || m_stopped) return;
        emitLoss(generation, m_lastError.isEmpty() ? tr("连接已断开") : m_lastError);
    });
    connect(m_socket, &QWebSocket::binaryMessageReceived, this,
            [this, generation](const QByteArray &payload) {
        if (generation != m_generation || m_stopped) return;
        emit frameReceived(generation, payload);
    });
    // errorOccurred 之后通常还会来 disconnected；这里只记原因，由 disconnected 统一收口
    connect(m_socket, &QWebSocket::errorOccurred, this,
            [this, generation](QAbstractSocket::SocketError) {
        if (generation != m_generation || m_stopped) return;
        m_lastError = m_socket->errorString();
    });

    emit logMessage(tr("正在连接 %1").arg(m_url.toString()));
    m_connectTimer->start();
    m_socket->open(m_url);
}

void WsCore::abort() {
    if (m_stopped) return;
    ++m_generation;          // 先作废旧回调，再关 socket，避免再报 disconnected
    m_connectTimer->stop();
    teardownSocket();
}

void WsCore::stop() {
    m_stopped = true;
    ++m_generation;
    m_connectTimer->stop();
    teardownSocket();
}

void WsCore::sendFrame(int generation, const QByteArray &payload) {
    if (generation != m_generation || m_stopped || !m_socket
            || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit sendResult(generation, false);
        return;
    }
    const qint64 written = m_socket->sendBinaryMessage(payload);
    emit sendResult(generation, written >= 0);
}

void WsCore::emitLoss(int generation, const QString &reason) {
    if (generation != m_generation || m_lostEmitted) return;
    m_lostEmitted = true;
    m_connectTimer->stop();
    emit disconnected(generation, reason);
}

void WsCore::teardownSocket() {
    if (!m_socket) return;
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}
