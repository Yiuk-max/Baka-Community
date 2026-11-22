#pragma once

// 聊天页里的一条消息：头像 + 名字 / 时间 + 可选的回复引用 + 正文。
// 自己不弹菜单，右键时只发信号，菜单由 ChatPage 决定放什么。
#include "model/Message.h"

#include <QWidget>

class QLabel;
class AttachmentWidget;
class MediaStore;

class MessageRow : public QWidget {
    Q_OBJECT
public:
    explicit MessageRow(const Message &msg, MediaStore *media, QWidget *parent = nullptr);
    void setAttachmentNearViewport(bool near);

    const Message &message() const { return m_msg; }
    // 乐观消息得到服务端确认：换成真实 id，正文从灰色变成正常颜色
    void confirm(int realId);
    // 换成本地头像文件；path 为空就是默认头像
    void setAvatar(const QString &path);

signals:
    void menuRequested(const QPoint &globalPos);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QWidget *buildAttachment();     // 附件展示与交互交给独立控件
    MediaStore *m_media = nullptr;
    AttachmentWidget *m_attachment = nullptr;

    Message m_msg;
    QLabel *m_avatar = nullptr;
    QLabel *m_text = nullptr;       // 文件消息没有正文，这时是 nullptr
};
