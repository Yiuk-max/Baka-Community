#include "MainWindow.h"

#include "net/PacketCodec.h"
#include "net/WsClient.h"

#include "message.pb.h"

#include <QDateTime>
#include <QLabel>
#include <QMetaObject>
#include <QStatusBar>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QWidget>

MainWindow::MainWindow(const QString &serverUrl, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Baka Community"));
    resize(1100, 700);

    // 主界面还没做，先放一个空控件占位
    setCentralWidget(new QWidget(this));

    // 骨架阶段把读到的配置显示出来，方便确认 config.json 生效
    statusBar()->showMessage(tr("Qt %1 / 服务器 %2").arg(QT_VERSION_STR, serverUrl));

    // 状态栏右下角：心跳回执
    m_heartbeatLabel = new QLabel(tr("心跳：未连接"), this);
    statusBar()->addPermanentWidget(m_heartbeatLabel);

    // 业务层心跳定时器（30s，与服务端 heartbeat_interval 默认值一致）
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendHeartbeat);

    startNetwork(serverUrl);
}

MainWindow::~MainWindow() {
    // 先让工作线程里的 socket 收尾，再退出事件循环
    if (m_netThread && m_netThread->isRunning()) {
        QMetaObject::invokeMethod(m_client, "stop", Qt::BlockingQueuedConnection);
        m_netThread->quit();
        m_netThread->wait(3000);
    }
}

void MainWindow::startNetwork(const QString &serverUrl) {
    m_netThread = new QThread(this);
    m_client = new WsClient(QUrl(serverUrl));
    m_client->moveToThread(m_netThread);

    connect(m_netThread, &QThread::started, m_client, &WsClient::start);
    connect(m_netThread, &QThread::finished, m_client, &QObject::deleteLater);

    // 连接状态/异常：临时信息走左侧 statusBar 普通消息
    connect(m_client, &WsClient::logMessage, this, [this](const QString &text) {
        statusBar()->showMessage(text, 5000);
    });

    // 连上就先发一次心跳，之后交给定时器
    connect(m_client, &WsClient::connected, this, [this] {
        sendHeartbeat();
        m_heartbeatTimer->start();
    });

    connect(m_client, &WsClient::disconnected, this, [this](const QString &reason) {
        m_heartbeatTimer->stop();
        m_heartbeatLabel->setText(tr("心跳：断开（%1）").arg(reason));
    });

    // 收到的字节流统一在这里解 protobuf，再按 type 分发（目前只认心跳回执）
    connect(m_client, &WsClient::frameReceived, this, &MainWindow::handleFrame);

    m_netThread->start();
}

void MainWindow::sendHeartbeat() {
    chat_proto::Envelope envelope;
    envelope.set_type("heartbeat");

    m_heartbeatSentAtMs = QDateTime::currentMSecsSinceEpoch();
    const QByteArray frame = packetcodec::encode(envelope);
    // 跨线程：投递到网络线程执行，QByteArray 是已注册的元类型
    QMetaObject::invokeMethod(m_client, "sendFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
}

void MainWindow::handleFrame(const QByteArray &payload) {
    chat_proto::Envelope envelope;
    if (!packetcodec::decode(payload, envelope)) {
        statusBar()->showMessage(tr("收到无法解析的帧（%1 字节）").arg(payload.size()), 5000);
        return;
    }

    const QString type = QString::fromStdString(envelope.type());
    if (type == QStringLiteral("heartbeat_ack")) {
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_heartbeatSentAtMs;
        m_heartbeatLabel->setText(tr("❤ 收到心跳包 %1 · %2ms · %3")
                                      .arg(QString::fromStdString(envelope.content()))
                                      .arg(elapsed)
                                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }

    statusBar()->showMessage(tr("收到 %1 类型消息（暂未处理）").arg(type), 5000);
}
