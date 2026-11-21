#include "MessageRow.h"

#include "Assets.h"
#include "Avatar.h"
#include "FileCover.h"
#include "util/TimeFormat.h"

#include <QContextMenuEvent>
#include <QFrame>
#include <QGridLayout>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

MessageRow::MessageRow(const Message &msg, QWidget *parent) : QWidget(parent), m_msg(msg) {
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

// 附件长什么样全在 FileCover 里；这里只负责摆放。下载 / 预览还没接，所以图片显示的是默认背景图
QWidget *MessageRow::buildAttachment() {
    const QString sizeText = filecover::formatSize(m_msg.fileSize);
    const bool image = filecover::isImage(m_msg.fileName);

    if (image || filecover::isVideo(m_msg.fileName)) {
        auto *box = new QWidget(this);
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 4, 0, 0);
        boxLayout->setSpacing(4);
        const QSize size(300, 170);
        auto *picture = new QLabel(box);
        picture->setPixmap(image ? filecover::imagePlaceholder(size) : filecover::videoCover(size));
        // 图和下载按钮放进同一个格子：按钮靠右下角，叠在图上面
        auto *stack = new QGridLayout;
        stack->setContentsMargins(0, 0, 0, 0);
        stack->addWidget(picture, 0, 0);
        auto *mediaDownload = makeDownloadButton(box, "messageMediaDownload");
        auto *corner = new QVBoxLayout;
        corner->setContentsMargins(0, 0, 8, 8);
        corner->addWidget(mediaDownload);
        stack->addLayout(corner, 0, 0, Qt::AlignRight | Qt::AlignBottom);
        boxLayout->addLayout(stack);
        auto *caption = new QLabel(m_msg.fileName + QStringLiteral(" · ") + sizeText, box);
        caption->setObjectName("messageFileSize");
        caption->setTextFormat(Qt::PlainText);
        boxLayout->addWidget(caption);
        return box;
    }

    // 普通文件：封面 + 文件名 + 大小 的卡片
    auto *card = new QFrame(this);
    card->setObjectName("messageFile");
    card->setFixedWidth(360);
    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);
    cardLayout->setSpacing(12);
    auto *coverLabel = new QLabel(card);
    coverLabel->setPixmap(filecover::cover(m_msg.fileName, 40));
    cardLayout->addWidget(coverLabel);
    auto *body = new QVBoxLayout;
    body->setSpacing(2);
    auto *nameLabel = new QLabel(m_msg.fileName, card);
    nameLabel->setObjectName("messageFileName");
    nameLabel->setTextFormat(Qt::PlainText);
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 太长就截断
    nameLabel->setToolTip(m_msg.fileName);
    auto *sizeLabel = new QLabel(sizeText, card);
    sizeLabel->setObjectName("messageFileSize");
    body->addWidget(nameLabel);
    body->addWidget(sizeLabel);
    cardLayout->addLayout(body, 1);
    cardLayout->addWidget(makeDownloadButton(card, "messageFileDownload"));
    return card;
}

// 下载按钮：文件下载协议还没接，目前只是占位，点了没有动作
QToolButton *MessageRow::makeDownloadButton(QWidget *parent, const char *objectName) {
    auto *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setIcon(QIcon(assets::downloadIcon()));
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(28, 28);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setToolTip(tr("下载（还没做）"));
    return button;
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
