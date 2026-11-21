#pragma once

#include "model/Contact.h"
#include "model/Message.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

namespace chat_proto { class Envelope; }
class ChatClient;
class Composer;
class QLabel;
class QScrollArea;
class QVBoxLayout;

// 中间聊天页面：顶部会话标题 + 消息滚动区 + 底部输入框。
// 由 MainWindow 创建，调用 openConversation 切换会话；
// MainWindow 把相关 Envelope 路由进来（见 handleEnvelope）。
class ChatPage : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(ChatClient *client, QWidget *parent = nullptr);

    // 切换到某个会话并立刻拉取首页历史
    void openConversation(const Contact &contact, int selfUid);
    // 退出登录时清空
    void clear();

    // MainWindow 把 envelopeReceived 里的相关消息路由到这里
    void handleEnvelope(const chat_proto::Envelope &envelope);

private:
    void requestHistory(int beforeId = 0);
    void appendMessages(const QList<Message> &msgs, bool prepend = false);
    QWidget *buildMessageWidget(const Message &msg);
    void scrollToBottom();
    bool isAtBottom() const;

    ChatClient  *m_client       = nullptr;
    int          m_selfUid      = 0;
    Contact      m_conv;
    bool         m_loaded       = false;
    bool         m_loading      = false;
    bool         m_hasMore      = true;
    int          m_oldestId     = 0;
    int          m_nextTempId   = -1;   // 乐观消息临时 id（负值递减）

    QLabel      *m_header       = nullptr;
    QScrollArea *m_scroll       = nullptr;
    QWidget     *m_msgContainer = nullptr;
    QVBoxLayout *m_msgLayout    = nullptr;
    Composer    *m_composer     = nullptr;

    QSet<int>          m_messageIds;        // 已展示的真实消息 id（防重复推送）
    QMap<int, QLabel*> m_optimisticLabels;  // tempId → 内容 label（确认后改 objectName）
};
