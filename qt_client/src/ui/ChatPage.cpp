#include "ChatPage.h"

#include "net/ChatClient.h"
#include "store/AvatarStore.h"
#include "ui/Assets.h"
#include "ui/Avatar.h"
#include "ui/Composer.h"
#include "ui/MenuStyle.h"
#include "ui/MessageRow.h"
#include "message.pb.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

// 文件消息的几个字段：历史记录（ChatItem）和实时推送（Envelope）里名字一样、位置不同，统一从这里填
static void fillFile(Message &msg, bool isFile, int fileId, const std::string &name, quint64 size) {
    if (!isFile) return;
    msg.isFile   = true;
    msg.fileId   = fileId;
    msg.fileName = QString::fromStdString(name);
    msg.fileSize = size;
}

ChatPage::ChatPage(ChatClient *client, AvatarStore *avatars, MediaStore *media, QWidget *parent)
    : QWidget(parent), m_client(client), m_avatars(avatars), m_media(media)
{
    // 某个人的头像下好了 / 换了：把他已经显示出来的消息行的头像换掉
    connect(m_avatars, &AvatarStore::avatarChanged, this, [this](int uid) {
        for (MessageRow *row : std::as_const(m_rows)) {
            if (row->message().senderUid == uid) row->setAvatar(m_avatars->path(uid));
        }
    });

    setObjectName("chatPage");
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部：会话标题栏
    m_header = new QLabel(tr("选择一个会话"), this);
    m_header->setObjectName("chatHeader");
    m_header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mainLayout->addWidget(m_header);

    // 消息滚动区
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("chatScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_msgContainer = new QWidget(m_scroll);
    m_msgContainer->setObjectName("msgContainer");
    m_msgLayout = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(0, 8, 0, 8);
    m_msgLayout->setSpacing(0);
    m_msgLayout->addStretch();   // 消息从底部往上堆，新消息在下方
    m_scroll->setWidget(m_msgContainer);
    mainLayout->addWidget(m_scroll, 1);

    // 底部：输入框
    m_composer = new Composer(this);
    m_composer->setEnabled(false);
    mainLayout->addWidget(m_composer);

    connect(m_composer, &Composer::messageSent, this, [this](const QString &text) {
        if (m_conv.uid == 0) return;
        // 乐观消息：立刻显示，收到 "Message sent. Message ID: N" 后确认
        Message opt;
        opt.messageId    = m_nextTempId--;
        opt.senderUid    = m_selfUid;
        opt.senderName   = m_client->username();
        opt.content      = text;
        opt.timestamp    = QDateTime::currentDateTimeUtc()
                               .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        opt.isSelf       = true;
        opt.isOptimistic = true;

        chat_proto::Envelope env;
        env.set_message(text.toStdString());
        env.set_target_uid(m_conv.uid);
        env.set_type(m_conv.isChannel ? "channel_chat"
                                      : m_conv.isGroup ? "group_chat" : "private_chat");
        // 正在回复某条消息：带上它的 id，本地这条也先把引用显示出来
        if (m_replyTo.messageId > 0) {
            env.set_reply_to_message_id(m_replyTo.messageId);
            opt.replyToId         = m_replyTo.messageId;
            opt.replyToSenderName = m_replyTo.senderName;
            opt.replyToContent    = m_replyTo.content;
            m_replyTo = {};
            m_composer->clearReplyTarget();
        }
        appendMessages({opt});
        scrollToBottom();
        m_client->send(env);
    });
    connect(m_composer, &Composer::replyCancelled, this, [this] { m_replyTo = {}; });

    m_mediaTimer = new QTimer(this);
    m_mediaTimer->setSingleShot(true);
    m_mediaTimer->setInterval(80);
    connect(m_mediaTimer, &QTimer::timeout, this, &ChatPage::updateVisibleMedia);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatPage::scheduleMedia);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::rangeChanged, this, &ChatPage::scheduleMedia);

    // 滚动到顶部时加载更早的历史
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (value < 60 && m_hasMore && !m_loading && m_loaded)
            requestHistory(m_oldestId);
    });
}

void ChatPage::openConversation(const Contact &contact, int selfUid) {
    if (contact.uid == 0) { clear(); return; }
    m_conv      = contact;
    m_selfUid   = selfUid;
    m_loaded    = false;
    m_loading   = false;
    m_hasMore   = true;
    m_oldestId  = 0;
    m_messageIds.clear();
    m_rows.clear();
    m_pendingIds.clear();
    m_replyTo = {};
    m_composer->clearReplyTarget();

    // 清空旧消息（保留末尾的 stretch）
    while (m_msgLayout->count() > 1) {
        QLayoutItem *item = m_msgLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    m_header->setText(contact.isChannel ? QStringLiteral("# %1").arg(contact.name) : contact.name);
    m_composer->setEnabled(true);
    m_composer->setPlaceholder(contact.isChannel
                                   ? tr("发送消息到 #%1").arg(contact.name)
                                   : tr("向 %1 发送消息…").arg(contact.name));
    requestHistory(0);
}

void ChatPage::setHeaderVisible(bool visible) { m_header->setVisible(visible); }

void ChatPage::clear() {
    m_conv      = {};
    m_loaded    = false;
    m_loading   = false;
    m_hasMore   = true;
    m_oldestId  = 0;
    m_messageIds.clear();
    m_rows.clear();
    m_pendingIds.clear();
    m_replyTo = {};
    m_composer->clearReplyTarget();
    while (m_msgLayout->count() > 1) {
        QLayoutItem *item = m_msgLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_header->setText(tr("选择一个会话"));
    m_composer->setEnabled(false);
    m_composer->setPlaceholder({});
}

void ChatPage::requestHistory(int beforeId) {
    if (m_loading) return;
    m_loading = true;
    chat_proto::Envelope env;
    env.set_type("history_request");
    env.set_peer_id(m_conv.uid);
    if (beforeId > 0) env.set_before_id(beforeId);
    // 频道历史要带 is_channel=true，好友 / 群不带
    if (m_conv.isChannel) env.set_is_channel(true);
    m_client->send(env);
}

// 一条实时推送：私聊 / 群聊 / 频道的字段布局一样，统一在这里落后
void ChatPage::appendIncoming(const chat_proto::Envelope &envelope) {
    const int mid = envelope.message_id();
    if (m_messageIds.contains(mid)) return;   // 已经显示过（历史页里可能先到）
    m_messageIds.insert(mid);

    Message msg;
    msg.messageId  = mid;
    msg.senderUid  = envelope.sender_uid();
    msg.senderName = QString::fromStdString(envelope.sender_name());
    msg.content    = QString::fromStdString(envelope.content());
    msg.timestamp  = QString::fromStdString(envelope.timestamp());
    fillFile(msg, envelope.is_file(), QString::fromStdString(envelope.file_id()).toInt(),
             envelope.file_name(), envelope.file_size());
    msg.isSelf     = (envelope.sender_uid() == m_selfUid);
    msg.replyToId = envelope.reply_to().message_id();
    msg.replyToSenderName = QString::fromStdString(envelope.reply_to().sender_name());
    msg.replyToContent = QString::fromStdString(envelope.reply_to().content());

    // 群聊 / 频道的推送也发给发送者自己（自己给自己私聊也一样）：用它确认那条乐观消息，不再重复添加
    if (msg.isSelf && !m_pendingIds.isEmpty()) {
        confirmPending(mid);
        return;
    }
    const bool follow = isAtBottom();   // 用户在往上翻旧消息时不打断他
    appendMessages({msg});
    if (follow) scrollToBottom();
}

void ChatPage::handleEnvelope(const chat_proto::Envelope &envelope) {
    const QString type = QString::fromStdString(envelope.type());

    if (type == QStringLiteral("history_response")) {
        if (envelope.peer_id() != m_conv.uid) return;  // 不属于当前会话
        m_loading = false;
        m_loaded  = true;
        const bool prepend = (m_oldestId > 0);  // 不是第一页
        QList<Message> msgs;
        for (const chat_proto::ChatItem &item : envelope.messages()) {
            if (m_messageIds.contains(item.message_id())) continue;
            Message msg;
            msg.messageId  = item.message_id();
            msg.senderUid  = item.sender_uid();
            msg.senderName = QString::fromStdString(item.sender_name());
            msg.content    = QString::fromStdString(item.content());
            msg.timestamp  = QString::fromStdString(item.timestamp());
            fillFile(msg, item.is_file(), item.file_id(), item.file_name(), item.file_size());
            msg.isSelf     = (item.sender_uid() == m_selfUid);
            if (item.reply_to().message_id() > 0) {
                msg.replyToId         = item.reply_to().message_id();
                msg.replyToSenderName = QString::fromStdString(item.reply_to().sender_name());
                msg.replyToContent    = QString::fromStdString(item.reply_to().content());
            }
            msgs.append(msg);
            m_messageIds.insert(msg.messageId);
            if (m_oldestId == 0 || msg.messageId < m_oldestId)
                m_oldestId = msg.messageId;
        }
        m_hasMore = envelope.has_more();
        if (msgs.isEmpty()) return;
        appendMessages(msgs, prepend);
        if (!prepend) scrollToBottom();
        return;
    }

    // 实时推送：私聊 / 群聊 / 频道（同一个会话只接对应的那一种）
    if ((type == QStringLiteral("private_chat") && !m_conv.isGroup && !m_conv.isChannel
                && envelope.sender_uid() == m_conv.uid)
            || (type == QStringLiteral("Group_Chat") && m_conv.isGroup
                && envelope.group_uid() == m_conv.uid)
            || (type == QStringLiteral("Channel_Chat") && m_conv.isChannel
                && envelope.group_uid() == m_conv.uid)) {
        appendIncoming(envelope);
        return;
    }

    // 对方（或群里别人）撤回了一条消息
    if (type == QStringLiteral("delete_message")) {
        removeMessage(envelope.message_id());
        return;
    }
}

bool ChatPage::handleSystemText(const QString &text) {
    // "Message sent. Message ID: N"：私聊发送确认
    static const QRegularExpression sentRe(QStringLiteral("Message sent\\. Message ID: (\\d+)"));
    // "Message [N] deleted"：自己撤回成功
    static const QRegularExpression deletedRe(QStringLiteral("Message \\[(\\d+)\\] deleted"));
    // 发送失败（和 web 端 backendClient.js 用的是同一组关键字）
    static const QRegularExpression sendFailRe(QStringLiteral(
        "Message cannot be empty|(?:UID|Group|Channel) \\[\\d+\\] does not exist|not your friend"
        "|Failed to store message|must be logged in to send|not a member of this community"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m = sentRe.match(text);
    if (m.hasMatch()) {
        if (m_pendingIds.isEmpty()) return false;
        confirmPending(m.captured(1).toInt());
        return true;
    }
    m = deletedRe.match(text);
    if (m.hasMatch()) {
        removeMessage(m.captured(1).toInt());
        return true;
    }
    if (text.contains(QLatin1String("Failed to load chat history"))) {
        m_loading = false;  // 解除卡死，允许用户再次触发加载
        return true;
    }
    // 发送失败：把那条灰色的乐观消息拿掉；返回 false，让主窗口把原因显示出来
    if (sendFailRe.match(text).hasMatch() && !m_pendingIds.isEmpty())
        removeMessage(m_pendingIds.takeFirst());
    return false;
}

void ChatPage::confirmPending(int realId) {
    m_messageIds.insert(realId);
    if (m_pendingIds.isEmpty()) return;
    MessageRow *row = m_rows.take(m_pendingIds.takeFirst());
    if (!row) return;
    row->confirm(realId);
    m_rows.insert(realId, row);
}

void ChatPage::removeMessage(int id) {
    MessageRow *row = m_rows.take(id);
    if (!row) return;
    if (m_replyTo.messageId == id) {   // 正要回复的那条被撤回了
        m_replyTo = {};
        m_composer->clearReplyTarget();
    }
    m_msgLayout->removeWidget(row);
    row->hide();
    row->deleteLater();
}

// 消息右键菜单：只放 web 端真正实现了的功能（回复 / 复制文字 / 撤回自己的消息）
void ChatPage::showMessageMenu(MessageRow *row, const QPoint &globalPos) {
    const Message msg = row->message();   // 先拷一份：菜单开着的时候这一行可能被撤回删掉
    const bool sent = msg.messageId > 0;  // 还没确认的乐观消息没有真实 id，不能回复 / 撤回

    QMenu menu(this);
    menus::prepare(&menu);
    QAction *replyAction = menu.addAction(tr("回复"));
    replyAction->setEnabled(sent);
    QAction *copyAction = menu.addAction(tr("复制文字"));
    QAction *recallAction = nullptr;
    if (msg.isSelf) {
        menu.addSeparator();
        recallAction = menu.addAction(tr("撤回"));
        recallAction->setEnabled(sent);
    }

    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;
    if (chosen == copyAction) {
        QGuiApplication::clipboard()->setText(msg.isFile ? msg.fileName : msg.content);
    } else if (chosen == replyAction) {
        m_replyTo = msg;
        m_composer->setReplyTarget(msg.senderName, msg.content);
    } else if (chosen == recallAction) {
        chat_proto::Envelope env;
        env.set_type("delete_message");
        env.set_message_id(msg.messageId);
        m_client->send(env);
    }
}

void ChatPage::appendMessages(const QList<Message> &msgs, bool prepend) {
    if (msgs.isEmpty()) return;
    if (prepend) {
        // 记住当前滚动条最大值，插入后做补偿，避免内容区跳动
        QScrollBar *vbar = m_scroll->verticalScrollBar();
        const int prevMax = vbar->maximum();
        for (int i = 0; i < msgs.size(); ++i)
            m_msgLayout->insertWidget(i, buildMessageWidget(msgs[i]));
        // 单次事件循环后布局已重算，把滚动条往下移相同距离
        QTimer::singleShot(0, this, [this, prevMax] {
            QScrollBar *v = m_scroll->verticalScrollBar();
            v->setValue(v->value() + v->maximum() - prevMax);
        });
    } else {
        for (const Message &msg : msgs)
            m_msgLayout->insertWidget(m_msgLayout->count() - 1, buildMessageWidget(msg));
    }
}

// 一条消息一个 MessageRow（长什么样在 MessageRow.cpp）；这里只登记它并接上右键菜单
MessageRow *ChatPage::buildMessageWidget(const Message &msg) {
    auto *row = new MessageRow(msg, m_media, m_msgContainer);
    scheduleMedia();
    row->setAvatar(m_avatars->path(msg.senderUid));   // 本地没有这个人的头像时是默认图
    m_rows.insert(msg.messageId, row);
    if (msg.isOptimistic) m_pendingIds.append(msg.messageId);
    // 排队连接 + QPointer：菜单开着的时候这一行可能被撤回删掉，不能在它自己的事件处理里弹菜单
    const QPointer<MessageRow> guard(row);
    connect(row, &MessageRow::menuRequested, this, [this, guard](const QPoint &globalPos) {
        if (guard) showMessageMenu(guard, globalPos);
    }, Qt::QueuedConnection);
    return row;
}

// 滚动条是否停在最底下（留 4px 误差）。收到新消息时只有本来就在底部才跟着滚
bool ChatPage::isAtBottom() const {
    const QScrollBar *vbar = m_scroll->verticalScrollBar();
    return vbar->value() >= vbar->maximum() - 4;
}

void ChatPage::scrollToBottom() {
    QTimer::singleShot(0, this, [this] {
        m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum());
    });
}
