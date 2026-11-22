// 新版社区设置离线回归：不连接服务器，不模拟键盘鼠标。
#include "ui/CommunitySettingsDialog.h"
#include "ui/CommunityOverviewPage.h"
#include "ui/CommunityChannelsPage.h"
#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QDebug>
#include <cstdlib>

static void check(bool condition, const char *message) {
    if (!condition) { qCritical("FAIL: %s", message); std::exit(1); }
}
static QPushButton *action(QWidget &widget, const char *name) {
    for (auto *button : widget.findChildren<QPushButton *>())
        if (button->property("action").toString() == QLatin1String(name)) return button;
    qFatal("Action not found: %s", name);
    return nullptr;
}
static QPushButton *button(QWidget &widget, const QString &text) {
    for (auto *item : widget.findChildren<QPushButton *>()) if (item->text() == text) return item;
    qFatal("Button not found");
    return nullptr;
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"));
    QFile style(QStringLiteral(BAKA_TEST_STYLE));
    if (style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    Community community;
    community.id = 42;
    community.name = QStringLiteral("Baka Community");
    community.description = QStringLiteral("一起聊天，分享有趣的事。");
    community.role = QStringLiteral("owner");
    community.avatarId = 101;
    community.bannerId = 102;
    CommunitySettingsDialog dialog(community, false);
    auto *overview = dialog.findChild<CommunityOverviewPage *>();
    auto *channels = dialog.findChild<CommunityChannelsPage *>();
    auto *name = overview->findChild<QLineEdit *>();
    auto *description = overview->findChild<QPlainTextEdit *>();
    auto *save = action(dialog, "saveProfile");
    auto *reset = action(dialog, "resetProfile");
    check(dialog.windowFlags() & Qt::FramelessWindowHint, "same frameless style as user settings");
    check(!save->isEnabled() && save->isHidden() == false && !save->isVisibleTo(&dialog), "save bar hidden when unchanged");
    int saves = 0;
    QObject::connect(&dialog, &CommunitySettingsDialog::saveRequested, &dialog,
        [&](const QString &n, const QString &d, int avatar, int banner) {
            ++saves;
            check(n == QStringLiteral("新社区") && d == community.description, "profile payload");
            check(avatar == 101 && banner == 102, "preserve existing images");
        });
    name->setText(QStringLiteral("  "));
    check(!save->isEnabled(), "blank name rejected");
    name->setText(QStringLiteral(" 新社区 "));
    description->setPlainText(QString(301, QLatin1Char('x')));
    check(!save->isEnabled(), "description length validation");
    description->setPlainText(community.description);
    check(save->isEnabled() && save->isVisibleTo(&dialog), "save bar shown for changes");
    save->click();
    check(saves == 1 && !overview->isEnabled(), "pending blocks duplicate submission");
    dialog.handleSystemText(QStringLiteral("Channel created successfully."));
    check(!overview->isEnabled(), "unrelated response ignored");
    dialog.handleSystemText(QStringLiteral("Failed to modify community (only owner can modify)."));
    check(overview->isEnabled() && save->isEnabled(), "failure retains draft");
    save->click();
    dialog.handleSystemText(QStringLiteral("Community updated successfully."));
    check(!save->isVisibleTo(&dialog), "confirmed save hides toolbar");
    name->setText(QStringLiteral("temporary"));
    reset->click();
    check(name->text() == QStringLiteral("新社区"), "cancel restores confirmed data");
    dialog.setChannels({{1, 42, QStringLiteral("聊天"), QStringLiteral("一般")},
                        {2, 99, QStringLiteral("其他"), QStringLiteral("一般")}});
    int rowCount = 0;
    QWidget *row = nullptr;
    for (auto *widget : channels->findChildren<QWidget *>()) {
        if (widget->property("channelId").isValid()) { ++rowCount; row = widget; }
    }
    check(rowCount == 1 && row, "filter unrelated community channels");
    action(*row, "editChannel")->click();
    auto *rowStack = qobject_cast<QStackedWidget *>(row);
    check(rowStack && rowStack->currentIndex() == 1, "edit expands inline without modal dialog");
    auto fields = row->findChildren<QLineEdit *>();
    fields[0]->setText(QStringLiteral("新名字"));
    fields[1]->setText(QStringLiteral("交流"));
    int modifications = 0;
    QObject::connect(&dialog, &CommunitySettingsDialog::modifyRequested, &dialog,
        [&](int id, const QString &n, const QString &category) {
            ++modifications;
            check(id == 1 && n == QStringLiteral("新名字") && category == QStringLiteral("交流"), "inline edit payload");
        });
    button(*row, QStringLiteral("保存"))->click();
    check(modifications == 1, "inline save sends one request");
    dialog.handleSystemText(QStringLiteral("Failed to modify channel (only owner can modify)."));
    check(rowStack->currentIndex() == 1 && fields[0]->text() == QStringLiteral("新名字"), "failure keeps inline edit");
    button(*row, QStringLiteral("保存"))->click();
    dialog.handleSystemText(QStringLiteral("Channel updated successfully."));
    check(rowStack->currentIndex() == 0, "success closes inline editor");
    auto *newName = channels->findChild<QLineEdit *>();
    newName->setText(QStringLiteral(" 新频道 "));
    int creates = 0;
    QObject::connect(&dialog, &CommunitySettingsDialog::createRequested, &dialog,
        [&](const QString &n, const QString &category) {
            ++creates;
            check(n == QStringLiteral("新频道") && category == QStringLiteral("一般"), "quick create defaults");
        });
    button(*channels, QStringLiteral("新建"))->click();
    dialog.handleSystemText(QStringLiteral("Channel created successfully. Channel ID: 3."));
    check(creates == 1 && newName->text().isEmpty(), "create resets input");
    const auto answer = [&dialog](bool accepted) {
        QTimer::singleShot(0, &dialog, [&dialog, accepted] {
            for (auto *child : dialog.findChildren<QDialog *>()) {
                if (accepted) child->accept(); else child->reject();
            }
        });
    };
    int deletions = 0;
    QObject::connect(&dialog, &CommunitySettingsDialog::deleteRequested, &dialog,
        [&](int id) { ++deletions; check(id == 1, "delete row ID"); });
    answer(false);
    action(*row, "deleteChannel")->click();
    check(deletions == 0, "cancel deletion sends nothing");
    answer(true);
    action(*row, "deleteChannel")->click();
    check(deletions == 1, "confirm deletion sends request");
    dialog.handleSystemText(QStringLiteral("Channel deleted successfully."));
    name->setText(QStringLiteral("草稿"));
    answer(false);
    dialog.reject();
    check(overview->isDirty(), "cancel closing retains draft");
    reset->click();
    community.role = QStringLiteral("member");
    CommunitySettingsDialog readonly(community, true);
    check(readonly.findChild<CommunityOverviewPage *>()->findChild<QLineEdit *>()->isReadOnly(), "member readonly");
    check(!button(readonly, QStringLiteral("解散社区"))->isEnabled(), "member cannot dissolve");
    check(readonly.findChild<CommunityChannelsPage *>()->isVisibleTo(&readonly), "channel entry selects channel page");
    dialog.connectionLost();
    check(!overview->isEnabled(), "disconnect locks actions");

    if (argc > 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--preview")) {
        community.role = QStringLiteral("owner");
        CommunitySettingsDialog preview(community, false);
        preview.setChannels({{1, 42, QStringLiteral("聊天大厅"), QStringLiteral("一般")},
                             {2, 42, QStringLiteral("分享日常"), QStringLiteral("一般")},
                             {3, 42, QStringLiteral("游戏开黑"), QStringLiteral("一起玩")}});
        preview.show();
        app.processEvents();
        const QString prefix = QString::fromLocal8Bit(argv[2]);
        check(preview.grab().save(prefix + QStringLiteral("-overview.png")), "overview screenshot");
        button(preview, QStringLiteral("频道"))->click();
        app.processEvents();
        check(preview.grab().save(prefix + QStringLiteral("-channels.png")), "channels screenshot");
        action(preview, "editChannel")->click();
        app.processEvents();
        check(preview.grab().save(prefix + QStringLiteral("-edit.png")), "inline editor screenshot");
    }
    qInfo("PASS: rebuilt community settings behavior tests");
    return 0;
}
