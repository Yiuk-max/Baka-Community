#include "ChatPage.h"

#include "net/ChatClient.h"
#include "ui/Assets.h"
#include "ui/Avatar.h"
#include "ui/Composer.h"
#include "util/TimeFormat.h"
#include "message.pb.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

ChatPage::ChatPage(ChatClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
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
        appendMessages({opt});
        scrollToBottom();

        chat_proto::Envelope env;
        env.set_message(text.toStdString());
        env.set_target_uid(m_conv.uid);
        env.set_type(m_conv.isGroup ? "group_chat" : "private_chat");
        m_client->send(env);
    });

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
    m_optimisticLabels.clear();

    // 清空旧消息（保留末尾的 stretch）
    while (m_msgLayout->count() > 1) {
        QLayoutItem *item = m_msgLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    m_header->setText(contact.name);
    m_composer->setEnabled(true);
    m_composer->setPlaceholder(tr("向 %1 发送消息…").arg(contact.name));
    requestHistory(0);
}

void ChatPage::clear() {
    m_conv      = {};
    m_loaded    = false;
    m_loading   = false;
    m_hasMore   = true;
    m_oldestId  = 0;
    m_messageIds.clear();
    m_optimisticLabels.clear();
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
    // 频道需要 is_channel=true，好友/群不带；频道入口在社区面板
    m_client->send(env);
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

    // 实时推送：私聊（对方发来）
    if (type == QStringLiteral("private_chat") && !m_conv.isGroup
            && envelope.sender_uid() == m_conv.uid) {
        const int mid = envelope.message_id();
        if (m_messageIds.contains(mid)) return;
        m_messageIds.insert(mid);
        Message msg;
        msg.messageId  = mid;
        msg.senderUid  = envelope.sender_uid();
        msg.senderName = QString::fromStdString(envelope.sender_name());
        msg.content    = QString::fromStdString(envelope.content());
        msg.timestamp  = QString::fromStdString(envelope.timestamp());
        msg.isSelf     = false;
        const bool follow = isAtBottom();   // 用户在往上翻旧消息时不打断他
        appendMessages({msg});
        if (follow) scrollToBottom();
        return;
    }

    // 实时推送：群聊（服务端广播给全员，含自己发的）
    if (type == QStringLiteral("Group_Chat") && m_conv.isGroup
            && envelope.group_uid() == m_conv.uid) {
        const int mid = envelope.message_id();
        if (m_messageIds.contains(mid)) return;
        m_messageIds.insert(mid);
        Message msg;
        msg.messageId  = mid;
        msg.senderUid  = envelope.sender_uid();
        msg.senderName = QString::fromStdString(envelope.sender_name());
        msg.content    = QString::fromStdString(envelope.content());
        msg.timestamp  = QString::fromStdString(envelope.timestamp());
        msg.isSelf     = (envelope.sender_uid() == m_selfUid);
        // 群聊推送是确认：把对应乐观消息升级为已发送
        if (msg.isSelf && !m_optimisticLabels.isEmpty()) {
            const int tempId = m_optimisticLabels.firstKey();
            QLabel *lbl = m_optimisticLabels.take(tempId);
            if (lbl) lbl->setObjectName("msgText");
            return;  // 已有乐观占位，不重复添加
        }
        const bool follow = isAtBottom();
        appendMessages({msg});
        if (follow) scrollToBottom();
        return;
    }

    // "Message sent. Message ID: N" 私聊确认
    if (type == QStringLiteral("system")) {
        static const QRegularExpression sentRe(
            QStringLiteral("Message sent\\. Message ID: (\\d+)"));
        static const QRegularExpression failRe(
            QStringLiteral("Failed to load chat history"));
        const QString text = QString::fromStdString(envelope.content());
        QRegularExpressionMatch m = sentRe.match(text);
        if (m.hasMatch()) {
            const int realId = m.captured(1).toInt();
            m_messageIds.insert(realId);
            if (!m_optimisticLabels.isEmpty()) {
                const int tempId = m_optimisticLabels.firstKey();
                QLabel *lbl = m_optimisticLabels.take(tempId);
                if (lbl) lbl->setObjectName("msgText");
            }
            return;
        }
        if (failRe.match(text).hasMatch()) {
            m_loading = false;  // 解除卡死，允许用户再次触发加载
            return;
        }
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

QWidget *ChatPage::buildMessageWidget(const Message &msg) {
    auto *row = new QWidget(m_msgContainer);
    row->setObjectName("messageRow");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 3, 16, 3);
    layout->setSpacing(12);

    // 头像（好友用圆形，暂无真实头像，统一用默认图）
    auto *avatarLabel = new QLabel(row);
    avatarLabel->setFixedSize(36, 36);
    avatarLabel->setPixmap(avatar::rounded(assets::defaultAvatar(), 36, 18));
    layout->addWidget(avatarLabel, 0, Qt::AlignTop);

    // 右侧内容区
    auto *content  = new QWidget(row);
    auto *cLayout  = new QVBoxLayout(content);
    cLayout->setContentsMargins(0, 0, 0, 0);
    cLayout->setSpacing(2);

    // 发送者名 + 时间戳
    auto *hRow    = new QWidget(content);
    auto *hLayout = new QHBoxLayout(hRow);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(8);
    auto *nameLabel = new QLabel(msg.senderName, hRow);
    nameLabel->setObjectName(msg.isSelf ? "msgSelfName" : "msgName");
    auto *timeLabel = new QLabel(timeformat::toLocal(msg.timestamp), hRow);
    timeLabel->setObjectName("msgTime");
    hLayout->addWidget(nameLabel);
    hLayout->addWidget(timeLabel);
    hLayout->addStretch();
    cLayout->addWidget(hRow);

    // 回复引用（如有）
    if (msg.replyToId > 0 && !msg.replyToContent.isEmpty()) {
        auto *quote = new QLabel(
            QStringLiteral("↩ ") + msg.replyToSenderName + QStringLiteral(": ") + msg.replyToContent,
            content);
        quote->setObjectName("msgQuote");
        quote->setWordWrap(true);
        cLayout->addWidget(quote);
    }

    // 消息正文
    auto *textLabel = new QLabel(msg.content, content);
    textLabel->setObjectName(msg.isOptimistic ? "msgTextPending" : "msgText");
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cLayout->addWidget(textLabel);

    content->setLayout(cLayout);
    layout->addWidget(content, 1);

    if (msg.isOptimistic)
        m_optimisticLabels[msg.messageId] = textLabel;

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
