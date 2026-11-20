#pragma once

// 通用网络收发层（不含任何业务）：跑在独立线程里
//   连接 / 断开 / 发一帧 / 收一帧。
// 帧格式由 PacketCodec 负责（4 字节长度 + protobuf），这一层只搬 QByteArray。
// 心跳之类的业务消息由上层自己拼 Envelope 后调 sendFrame()。
#include <QByteArray>
#include <QObject>
#include <QUrl>

class QWebSocket;

class WsCore : public QObject {
    Q_OBJECT
public:
    explicit WsCore(const QUrl &url, QObject *parent = nullptr);

public slots:
    void start();                              // 由线程的 started() 触发，在本线程内建 socket 并连接
    void stop();                               // 关 socket（心跳定时器在上层 ChatClient，由它自己停）
    void reconnect();                          // 连接断了之后重新连；正在连 / 已连上时什么都不做
    void sendFrame(const QByteArray &payload); // 发一帧（已带 4 字节长度前缀）

signals:
    void connected();
    void disconnected(const QString &reason);
    void frameReceived(const QByteArray &payload);
    void logMessage(const QString &text);

private:
    QUrl m_url;
    QWebSocket *m_socket = nullptr;
};
