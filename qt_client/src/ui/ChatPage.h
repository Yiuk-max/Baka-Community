#pragma once

#include "model/Contact.h"
#include "model/Message.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

namespace chat_proto { class Envelope; }
class AvatarStore;
class ChatClient;
class Composer;
class MediaStore;
class QTimer;
class MessageRow;
class QLabel;
class QScrollArea;
class QVBoxLayout;

// 中间聊天页面：顶部会话标题 + 消息滚动区 + 底部输入框。
// 由 MainWindow 创建，调用 openConversation 切换会话；
// MainWindow 把相关 Envelope 路由进来（见 handleEnvelope）。
class ChatPage : public QWidget {
    Q_OBJECT
public:
    // avatars：消息行的头像从它那里取本地文件，头像下好后跟着刷新
    explicit ChatPage(ChatClient *client, AvatarStore *avatars, MediaStore *media, QWidget *parent = nullptr);

    // 切换到某个会话并立刻拉取首页历史
    void openConversation(const Contact &contact, int selfUid);
    // 退出登录时清空
    void clear();
    void setHeaderVisible(bool visible);   // 社区复用统一顶部栏，不重复显示会话标题

    // MainWindow 把 envelopeReceived 里的相关消息路由到这里
    void handleEnvelope(const chat_proto::Envelope &envelope);
    // system 文本里和聊天有关的（发送确认 / 撤回成功 / 发送失败）。返回 true 表示已处理完，不用再提示
    bool handleSystemText(const QString &text);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void scheduleMedia();
    void updateVisibleMedia();
    void requestHistory(int beforeId = 0);
    void appendMessages(const QList<Message> &msgs, bool prepend = false);
    void appendIncoming(const chat_proto::Envelope &envelope);   // 一条实时推送（私聊 / 群聊 / 频道）
    MessageRow *buildMessageWidget(const Message &msg);
    void showMessageMenu(MessageRow *row, const QPoint &globalPos);   // 消息右键菜单
    void confirmPending(int realId);    // 最早的一条乐观消息得到确认
    void removeMessage(int id);         // 撤回 / 被对方撤回：把这一行拿掉
    void scrollToBottom();
    bool isAtBottom() const;
    void onConnectionLost();      // 断线：复位历史 loading、作废未确认回执队列
    void onSessionRestored();     // 重连认证后：重新拉当前会话最新一页

    ChatClient  *m_client       = nullptr;
    AvatarStore *m_avatars      = nullptr;
    MediaStore *m_media = nullptr;
    QTimer *m_mediaTimer = nullptr;
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
    QMap<int, MessageRow*> m_rows;          // 消息 id（乐观消息是负的临时 id）→ 那一行
    QList<int>         m_pendingIds;        // 还没确认的乐观消息临时 id，按发送顺序
    Message            m_replyTo;           // 正在回复的消息；messageId == 0 表示没在回复
};
