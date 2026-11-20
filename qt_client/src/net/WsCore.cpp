#include "WsCore.h"

#include <QWebSocket>

WsCore::WsCore(const QUrl &url, QObject *parent) : QObject(parent), m_url(url) {}

void WsCore::start() {
    // 必须在当前线程（工作线程）创建，之后所有回调都在这个线程里执行
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::connected, this, [this] {
        emit logMessage(tr("已连接 %1").arg(m_url.toString()));
        emit connected();
    });

    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        emit disconnected(tr("连接已断开"));
    });

    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &WsCore::frameReceived);

    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit disconnected(m_socket->errorString());
    });

    emit logMessage(tr("正在连接 %1").arg(m_url.toString()));
    m_socket->open(m_url);
}

void WsCore::stop() {
    if (m_socket) m_socket->close();
}

void WsCore::sendFrame(const QByteArray &payload) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit logMessage(tr("未连接，发送被丢弃"));
        return;
    }
    m_socket->sendBinaryMessage(payload);
}
