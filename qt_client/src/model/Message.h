#pragma once
#include <QString>

// 界面用的消息纯数据，由 MainWindow 从 Envelope / ChatItem 转来，控件不认 protobuf。
struct Message {
    int     messageId    = 0;      // 服务端 id；负值为本地乐观占位
    int     senderUid    = 0;
    QString senderName;
    QString content;
    QString timestamp;             // UTC "YYYY-MM-DD HH:MM:SS"
    bool    isSelf       = false;  // 发送者是当前账号
    bool    isOptimistic = false;  // 本地占位，尚未收到服务端确认

    // 回复引用（只一层，reply_to 子消息为空时 replyToId == 0）
    int     replyToId         = 0;
    QString replyToSenderName;
    QString replyToContent;
    // 文件消息（isFile 为 true 时正文不显示，显示附件）
    bool    isFile   = false;
    int     fileId   = 0;
    QString fileName;
    quint64 fileSize = 0;
};
