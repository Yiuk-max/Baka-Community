#include "MessageRow.h"

#include "Assets.h"
#include "Avatar.h"
#include "AttachmentWidget.h"
#include "util/TimeFormat.h"

#include <QContextMenuEvent>
#include <QFrame>
#include <QGridLayout>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

MessageRow::MessageRow(const Message &msg, MediaStore *media, QWidget *parent)
    : QWidget(parent), m_media(media), m_msg(msg) {
    setObjectName("messageRow");
    setAttribute(Qt::WA_StyledBackground);   // QWidget 子类默认不画 QSS 背景（悬停变色要用）
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 3, 16, 3);
    layout->setSpacing(12);

    // 头像：先放默认图，ChatPage 建完这一行会用 setAvatar 换成发送者的本地头像
    m_avatar = new QLabel(this);
    m_avatar->setFixedSize(36, 36);
    m_avatar->setPixmap(avatar::user(QString(), 36));
    layout->addWidget(m_avatar, 0, Qt::AlignTop);

    // 右侧内容区
    auto *cLayout = new QVBoxLayout;
    cLayout->setContentsMargins(0, 0, 0, 0);
    cLayout->setSpacing(2);

    // 发送者名 + 时间戳
    auto *hLayout = new QHBoxLayout;
    hLayout->setSpacing(8);
    auto *nameLabel = new QLabel(msg.senderName, this);
    nameLabel->setObjectName(msg.isSelf ? "msgSelfName" : "msgName");
    auto *timeLabel = new QLabel(timeformat::toLocal(msg.timestamp), this);
    timeLabel->setObjectName("msgTime");
    hLayout->addWidget(nameLabel);
    hLayout->addWidget(timeLabel);
    hLayout->addStretch();
    cLayout->addLayout(hLayout);

    // 回复引用（如有）
    if (msg.replyToId > 0 && !msg.replyToContent.isEmpty()) {
        auto *quote = new QLabel(
            QStringLiteral("↩ ") + msg.replyToSenderName + QStringLiteral(": ") + msg.replyToContent,
            this);
        quote->setObjectName("msgQuote");
        quote->setWordWrap(true);
        cLayout->addWidget(quote);
    }

    // 文件消息显示附件，不显示正文
    if (msg.isFile) {
        cLayout->addWidget(buildAttachment(), 0, Qt::AlignLeft);
        layout->addLayout(cLayout, 1);
        return;
    }

    // 消息正文：可以用鼠标选中，但不要 Qt 自带的英文右键菜单，右键交给整行处理
    m_text = new QLabel(msg.content, this);
    m_text->setObjectName(msg.isOptimistic ? "msgTextPending" : "msgText");
    m_text->setTextFormat(Qt::PlainText);   // 消息里的 <b> 之类原样显示，不当成富文本
    m_text->setWordWrap(true);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_text->setContextMenuPolicy(Qt::NoContextMenu);
    cLayout->addWidget(m_text);

    layout->addLayout(cLayout, 1);
}

// 封面、下载 / 预览和右侧体积标签在 AttachmentWidget 中，消息行只负责摆放。
QWidget *MessageRow::buildAttachment() {
    m_attachment = new AttachmentWidget(m_msg, m_media, this);
    return m_attachment;
}

void MessageRow::setAttachmentNearViewport(bool near) {
    if (m_attachment) m_attachment->setNearViewport(near);
}

void MessageRow::confirm(int realId) {
    m_msg.messageId = realId;
    m_msg.isOptimistic = false;
    if (!m_text) return;
    // 换了 objectName 之后要重新套一遍 QSS，颜色才会变
    m_text->setObjectName("msgText");
    m_text->style()->unpolish(m_text);
    m_text->style()->polish(m_text);
}

void MessageRow::setAvatar(const QString &path) {
    m_avatar->setPixmap(avatar::user(path, 36));
}

void MessageRow::contextMenuEvent(QContextMenuEvent *event) {
    emit menuRequested(event->globalPos());
    event->accept();
}
