#pragma once

// 通用网络收发层（不含任何业务）：跑在独立线程里。
//   start() 每调用一次都是一次新的连接尝试（generation+1），旧 socket 的回调全部作废；
//   连接的建立/超时、失联合并都在这一层做，ChatClient 只关心“连上了/断了”和代次。
// 帧格式由 PacketCodec 负责（4 字节长度 + protobuf），这一层只搬 QByteArray。
#include <QByteArray>
#include <QObject>
#include <QUrl>

class QTimer;
class QWebSocket;

class WsCore : public QObject {
    Q_OBJECT
public:
    explicit WsCore(const QUrl &url, QObject *parent = nullptr);
    ~WsCore() override;

public slots:
    void start();                                  // 开始一次新的连接尝试（内部 generation+1）
    void abort();                                  // 取消当前尝试（不发 disconnected）
    void stop();                                   // 永久停止
    // 只发属于这个代次的帧；代次不符 / 未连接会回 sendResult(false)
    void sendFrame(int generation, const QByteArray &payload);

signals:
    void connected(int generation);
    void disconnected(int generation, const QString &reason);   // 每次尝试最多触发一次
    void frameReceived(int generation, const QByteArray &payload);
    void sendResult(int generation, bool accepted);
    void logMessage(const QString &text);

private:
    void emitLoss(int generation, const QString &reason);
    void teardownSocket();

    QUrl m_url;
    QWebSocket *m_socket = nullptr;
    QTimer *m_connectTimer = nullptr;   // 单次握手/DNS/TCP/TLS 总超时
    int m_generation = 0;               // 当前尝试的代次
    bool m_stopped = true;
    bool m_lostEmitted = false;         // 本代次是否已经报过失联
    QString m_lastError;
};
