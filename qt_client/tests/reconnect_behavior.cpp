// 完全离线：本地 WebSocket 假服务，验证断线后自动重连 + token 重新认证、登出/失效不重连。
#include "net/ChatClient.h"
#include "net/PacketCodec.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QWebSocket>
#include <QWebSocketServer>
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char *name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
    if (!ok) ++failures;
}
bool waitFor(const std::function<bool()> &condition, int ms = 4000) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < ms) {
        QCoreApplication::processEvents(); QThread::msleep(5);
    }
    return condition();
}

struct Server {
    QWebSocketServer server{QStringLiteral("reconnect-fixture"), QWebSocketServer::NonSecureMode};
    QList<QWebSocket *> clients;
    int connections = 0;
    int logins = 0;
    int verifies = 0;
    bool rejectToken = false;
    int uid = 0;

    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    int port() { return server.serverPort(); }

    void wire() {
        QObject::connect(&server, &QWebSocketServer::newConnection, &server, [this] {
            ++connections;
            QWebSocket *socket = server.nextPendingConnection();
            clients.append(socket);
            QObject::connect(socket, &QWebSocket::disconnected, socket, [this, socket] {
                clients.removeAll(socket);
                socket->deleteLater();
            });
            QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket, [this, socket](const QByteArray &frame) {
                chat_proto::Envelope env, reply;
                if (!packetcodec::decode(frame, env)) return;
                const auto send = [&](const chat_proto::Envelope &e) {
                    socket->sendBinaryMessage(packetcodec::encode(e));
                };
                if (env.type() == "login") {
                    ++logins;
                    reply.set_type("login_success");
                    reply.mutable_user()->set_id(uid);
                    reply.mutable_user()->set_username("ReconnectTest");
                    reply.set_token("good-token");
                    send(reply);
                } else if (env.type() == "verify_token") {
                    ++verifies;
                    if (rejectToken) {
                        reply.set_type("system");
                        reply.set_content("Invalid or expired token.\n");
                    } else {
                        reply.set_type("login_success");
                        reply.mutable_user()->set_id(uid);
                        reply.mutable_user()->set_username("ReconnectTest");
                        reply.set_token("good-token");
                    }
                    send(reply);
                } else if (env.type() == "heartbeat") {
                    reply.set_type("heartbeat_ack");
                    reply.set_content("pong");
                    send(reply);
                }
            });
        });
    }
    void dropAll() {
        const QList<QWebSocket *> copy = clients;
        for (QWebSocket *socket : copy) socket->abort();
    }
};
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QTemporaryDir settings;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    qputenv("BAKA_RECONNECT_BASE_MS", "10");
    qputenv("BAKA_RECONNECT_MAX_MS", "30");
    qputenv("BAKA_CONNECT_TIMEOUT_MS", "2000");

    // 场景 A：断线后自动重连并重新验证 token，触发 sessionRestored
    {
        Server fixture;
        fixture.uid = 200000001;
        fixture.wire();
        check(fixture.listen(), "A: server listens");
        ChatClient client(QStringLiteral("ws://127.0.0.1:%1").arg(fixture.port()));
        int restored = 0;
        QObject::connect(&client, &ChatClient::sessionRestored, &client, [&] { ++restored; });
        client.start();
        check(waitFor([&] { return client.isConnected(); }), "A: transport connects");
        client.login(QStringLiteral("200000001"), QStringLiteral("pw"));
        check(waitFor([&] { return client.uid() > 0 && client.isAuthenticated(); }), "A: first login");
        const int before = fixture.connections;
        fixture.dropAll();   // 模拟网络中断
        check(waitFor([&] { return !client.isConnected(); }), "A: transport lost");
        check(waitFor([&] { return restored == 1 && client.isAuthenticated(); }, 6000),
              "A: auto-reconnect re-authenticates and restores session");
        check(fixture.connections > before && fixture.verifies >= 1, "A: server saw new connection + verify_token");
        client.logout();
    }

    // 场景 B：主动登出后不再自动重连
    {
        Server fixture;
        fixture.uid = 200000002;
        fixture.wire();
        check(fixture.listen(), "B: server listens");
        ChatClient client(QStringLiteral("ws://127.0.0.1:%1").arg(fixture.port()));
        client.start();
        check(waitFor([&] { return client.isConnected(); }), "B: transport connects");
        client.login(QStringLiteral("200000002"), QStringLiteral("pw"));
        check(waitFor([&] { return client.uid() > 0 && client.isAuthenticated(); }), "B: login");
        client.logout();
        const int connections = fixture.connections;
        fixture.dropAll();
        QElapsedTimer settle; settle.start();
        while (settle.elapsed() < 400) { QCoreApplication::processEvents(); QThread::msleep(5); }
        check(fixture.connections == connections, "B: no reconnect after explicit logout");
    }

    // 场景 C：重连时 token 已失效 → 回到未登录且不再重连
    {
        Server fixture;
        fixture.uid = 200000003;
        fixture.wire();
        check(fixture.listen(), "C: server listens");
        ChatClient client(QStringLiteral("ws://127.0.0.1:%1").arg(fixture.port()));
        bool loggedOut = false;
        QObject::connect(&client, &ChatClient::loggedOut, &client, [&](bool kicked) {
            if (!kicked) loggedOut = true;
        });
        client.start();
        check(waitFor([&] { return client.isConnected(); }), "C: transport connects");
        client.login(QStringLiteral("200000003"), QStringLiteral("pw"));
        check(waitFor([&] { return client.uid() > 0 && client.isAuthenticated(); }), "C: login");
        fixture.rejectToken = true;   // 之后 verify_token 一律回失效
        fixture.dropAll();
        check(waitFor([&] { return loggedOut; }, 6000), "C: invalid token during reconnect returns to login");
        check(!client.hasSavedToken() && client.uid() == 0, "C: local token cleared");
        const int connections = fixture.connections;
        QElapsedTimer settle; settle.start();
        while (settle.elapsed() < 400) { QCoreApplication::processEvents(); QThread::msleep(5); }
        check(fixture.connections == connections, "C: no reconnect after token invalid");
    }

    std::cout << "Failures: " << failures << std::endl;
    return failures ? 1 : 0;
}
