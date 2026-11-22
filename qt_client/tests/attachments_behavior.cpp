// 完全离线：本地 WebSocket 假服务 + offscreen Widgets，不接触生产账号。
#include "MainWindow.h"
#include "net/ChatClient.h"
#include "net/PacketCodec.h"
#include "ui/AttachmentWidget.h"
#include "ui/ChatPage.h"
#include "ui/TransferProgressPanel.h"
#include "util/DownloadPaths.h"
#include "store/MediaStore.h"
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QDateTime>
#include <QToolButton>
#include <QWebSocket>
#include <QWebSocketServer>
#include <functional>
#include <iostream>

// 截获系统打开请求，验证本地文件路径；测试期间不真正启动照片应用。
class LocalFileOpener : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void open(const QUrl &url) { opened = url; }
};

namespace {
int failures = 0;
void check(bool ok, const char *name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
    if (!ok) ++failures;
}
bool waitFor(const std::function<bool()> &condition, int ms = 4000) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < ms) {
        QApplication::processEvents(); QThread::msleep(5);
    }
    return condition();
}
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("BakaOfflineAttachmentTests");
    app.setApplicationName("OfflineAttachments");
    QTemporaryDir settings, downloads;
    // 每次使用从未出现过的本地账号目录，避免上一次缓存掩盖下载请求。
    const int testUid = 1000000000 + int(QDateTime::currentMSecsSinceEpoch() % 900000000);
    const QString testRoot = downloadpaths::userRoot(testUid);
    if (QFile::exists(testRoot)) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    qputenv("BAKA_DOWNLOAD_DIR", downloads.path().toLocal8Bit());
    QFile qss(QStringLiteral(BAKA_TEST_STYLE));
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    QImage image(80, 40, QImage::Format_RGB32); image.fill(Qt::green);
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    QHash<int, QByteArray> data{{101, png}, {102, QByteArray(128, 'v')}, {103, QByteArray("document bytes")},
                              {106, QByteArray(21 * 1024 * 1024, 'V')}};
    QHash<int, int> requests;
    QHash<int, int> transfers;
    int nextTransfer = 1000;
    QWebSocketServer server("offline", QWebSocketServer::NonSecureMode);
    check(server.listen(QHostAddress::LocalHost, 0), "local server listens");
    QObject::connect(&server, &QWebSocketServer::newConnection, &server, [&] {
        auto *socket = server.nextPendingConnection();
        QObject::connect(socket, &QWebSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket, [&, socket](const QByteArray &frame) {
            chat_proto::Envelope env, reply;
            if (!packetcodec::decode(frame, env)) return;
            const auto send = [&](const chat_proto::Envelope &e, const QByteArray &bytes = {}) {
                socket->sendBinaryMessage(packetcodec::encode(e, bytes));
            };
            if (env.type() == "login") {
                reply.set_type("login_success"); reply.mutable_user()->set_id(testUid);
                reply.mutable_user()->set_username("Offline"); reply.set_token("offline-only"); send(reply);
            } else if (env.type() == "history_request") {
                reply.set_type("history_response"); reply.set_peer_id(env.peer_id());
                for (int id : {101, 102, 103, 104}) {
                    auto *msg = reply.add_messages(); msg->set_message_id(id); msg->set_sender_uid(77);
                    msg->set_sender_name("Local fixture"); msg->set_is_file(true); msg->set_file_id(id);
                    msg->set_file_name(id == 101 ? "picture.png" : id == 102 ? "small.mp4"
                                                              : id == 103 ? "report.txt" : "large.mp4");
                    msg->set_file_size(id == 104 ? 30 * 1024 * 1024 : data[id].size());
                }
                send(reply);
            } else if (env.type() == "file_download_init") {
                const int id = std::stoi(env.file_id()); ++requests[id];
                if (id == 105) { reply.set_type("system"); reply.set_content("File not found."); send(reply); return; }
                const int transfer = ++nextTransfer; transfers[transfer] = id;
                reply.set_type("file_download_ready"); reply.mutable_file_meta()->set_file_id(env.file_id());
                const int size = id == 104 ? 30 * 1024 * 1024 : data[id].size();
                reply.mutable_file_meta()->set_size(size);
                auto *info = reply.mutable_transfer_info(); info->set_transfer_id(std::to_string(transfer));
                info->set_chunk_size(4194304); info->set_chunk_count((size + 4194303) / 4194304);
                info->set_total_size(size); send(reply);
            } else if (env.type() == "file_download_chunk") {
                const int id = transfers.value(env.transfer_id());
                if (id == 104) return; // 故意悬挂：验证取消正在下载的任务
                reply.set_type("file_chunk"); auto *meta = reply.mutable_meta();
                meta->set_transfer_id(std::to_string(env.transfer_id())); meta->set_file_id(std::to_string(id));
                const qint64 offset = qint64(env.chunk_index()) * 4194304;
                const QByteArray chunk = data[id].mid(offset, 4194304);
                meta->set_chunk_index(env.chunk_index()); meta->set_offset(offset); meta->set_chunk_size(chunk.size());
                send(reply, chunk);
            }
        });
    });

    ChatClient client(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort()));
    MainWindow window(&client); window.show(); client.start();
    check(waitFor([&] { return client.isConnected(); }), "client connects locally");
    client.login(QString::number(testUid), "local-fixture");
    check(waitFor([&] { return client.uid() > 0; }), "local login");
    auto pages = window.findChildren<ChatPage *>();
    ChatPage *chat = pages.first();
    for (auto *page : pages) if (qobject_cast<QStackedWidget *>(page->parentWidget())) chat = page;
    Contact contact; contact.uid = 77; contact.name = "Offline peer";
    chat->openConversation(contact, client.uid());
    qobject_cast<QStackedWidget *>(chat->parentWidget())->setCurrentWidget(chat);
    check(waitFor([&] { return requests[101] && requests[102]; }), "visible image and small video auto-download");
    check(waitFor([&] { return !downloadpaths::cachedMediaPath(client.uid(), 101).isEmpty()
                                  && !downloadpaths::cachedMediaPath(client.uid(), 102).isEmpty(); }),
          "image and video saved in user folder");
    check(requests[103] == 0 && requests[104] == 0, "ordinary file and large video do not auto-download");
    auto *panel = window.findChild<TransferProgressPanel *>();
    check(panel && panel->cardCount() == 0, "silent cache creates no progress cards");
    const auto attachment = [&](const QString &name) -> AttachmentWidget * {
        for (auto *widget : chat->findChildren<AttachmentWidget *>())
            for (auto *label : widget->findChildren<QLabel *>()) if (label->text() == name) return widget;
        return nullptr;
    };
    LocalFileOpener opener;
    QDesktopServices::setUrlHandler(QStringLiteral("file"), &opener, "open");
    auto *picture = attachment("picture.png");
    if (picture) {
        check(picture->findChild<QToolButton *>("attachmentOpen") == nullptr,
              "no auxiliary preview button");
        check(picture->findChild<QLabel *>("attachmentStatus") == nullptr,
              "no auxiliary attachment status text");
        auto *preview = picture->findChild<QLabel *>("attachmentPreview");
        QMouseEvent click(QEvent::MouseButtonRelease, QPointF(10, 10), QPointF(10, 10),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(preview, &click);
    }
    check(waitFor([&] { return opener.opened.isLocalFile() && QFile::exists(opener.opened.toLocalFile()); }),
          "cached image delegates local file to system viewer");
    QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
    auto *file = attachment("report.txt");
    check(file != nullptr, "ordinary attachment visible");
    if (file) file->findChild<QToolButton *>("messageFileDownload")->click();
    check(waitFor([&] { return QFile::exists(downloads.filePath("report.txt")); }), "manual file saved to environment download directory");
    check(panel->cardCount() == 1, "manual download creates terminal progress card");
    check(waitFor([&] { return panel->cardCount() == 0; }, 5000), "terminal card auto-dismisses after 3 seconds");
    auto *large = attachment("large.mp4");
    if (large) large->findChild<QToolButton *>("messageFileDownload")->click();
    check(waitFor([&] { return requests[104] == 1 && panel->cardCount() == 1; }), "large video manual download displays progress");
    auto *close = panel->findChild<QPushButton *>("transferClose");
    if (close) close->click();
    check(waitFor([&] {
        for (auto *label : panel->findChildren<QLabel *>()) if (label->text() == QString::fromUtf8("已取消")) return true;
        return false;
    }), "active close cancels task");
    check(waitFor([&] { return panel->cardCount() == 0; }, 5000), "canceled card auto-dismisses");
    // 独立附件控件也使用同一个 MediaStore，验证大视频的多分片落盘和失败卡片。
    auto *store = window.findChild<MediaStore *>();
    Message largeMessage; largeMessage.isFile = true; largeMessage.fileId = 106;
    largeMessage.fileName = "multi-chunk.mp4"; largeMessage.fileSize = data[106].size();
    store->retry(largeMessage);
    check(waitFor([&] { return !store->localPath(largeMessage).isEmpty(); }, 10000), "multi-chunk large video completes");
    check(store->localPath(largeMessage).startsWith(downloadpaths::videosDir(client.uid()))
          && !QFile::exists(downloads.filePath("multi-chunk.mp4")), "large video stays in user videos folder");
    Message missing; missing.isFile = true; missing.fileId = 105; missing.fileName = "missing.txt";
    store->retry(missing);
    check(waitFor([&] { return !store->error(105).isEmpty(); }), "server error reaches attachment retry state");
    check(waitFor([&] { return panel->cardCount() == 0; }, 5000), "failure and completion cards auto-dismiss");
    chat->openConversation(contact, client.uid());
    check(waitFor([&] { return chat->findChildren<AttachmentWidget *>().size() >= 4; }), "history reopened");
    QElapsedTimer settle; settle.start();
    while (settle.elapsed() < 300) QApplication::processEvents();
    check(requests[101] == 1 && requests[102] == 1, "cached media avoids repeat requests");
    window.grab().save(QStringLiteral(BAKA_TEST_SCREENSHOT));
    client.logout();
    check(panel->cardCount() == 0, "logout clears cards");
    // 只清理本次测试自己创建的唯一账号目录。
    QDir(testRoot).removeRecursively();
    std::cout << "Failures: " << failures << std::endl;
    return failures ? 1 : 0;
}

#include "attachments_behavior.moc"
