#include "Composer.h"
#include "EmojiPanel.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

// 拦截 Enter 键，不走 Q_OBJECT（用回调代替信号）
class ComposerEdit : public QTextEdit {
public:
    std::function<void()> onSend;
    explicit ComposerEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}

protected:
    void keyPressEvent(QKeyEvent *e) override {
        // IME 组字时 Enter 由输入法内部消耗，不会到达这里；Shift+Enter 插入换行
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
                && !(e->modifiers() & Qt::ShiftModifier)) {
            if (onSend) onSend();
            e->accept();
            return;
        }
        QTextEdit::keyPressEvent(e);
    }
};

Composer::Composer(QWidget *parent) : QWidget(parent) {
    setObjectName("composerOuter");
    setAttribute(Qt::WA_StyledBackground);

    // 外层只留边距：左右 16、下 24（web 端 .composer 的 margin: 0 16px 24px）
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 0, 16, 24);
    outerLayout->setSpacing(0);

    // 圆角盒子：最小高 64，左右内边距 16，文字和按钮在里面垂直居中
    auto *inner = new QWidget(this);
    inner->setObjectName("composerInner");
    inner->setMinimumHeight(64);
    auto *innerLayout = new QHBoxLayout(inner);
    innerLayout->setContentsMargins(16, 10, 16, 10);
    innerLayout->setSpacing(16);

    auto *edit = new ComposerEdit(inner);
    edit->setObjectName("composerInput");
    edit->setFrameShape(QFrame::NoFrame);
    edit->document()->setDocumentMargin(0);
    edit->setAcceptRichText(false);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit = edit;

    edit->onSend = [this] { onSendClicked(); };
    // 文档高度变了（打字、换行、窗口变窄导致折行、字体生效）就重算输入框高度
    connect(edit->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &Composer::adjustHeight);

    // 左侧“＋”附件按钮：先只放按钮，上传功能还没接
    m_attachBtn = new QToolButton(inner);
    m_attachBtn->setObjectName("composerToolBtn");
    m_attachBtn->setText(QStringLiteral("＋"));
    m_attachBtn->setFixedSize(24, 24);
    m_attachBtn->setCursor(Qt::PointingHandCursor);
    m_attachBtn->setFocusPolicy(Qt::NoFocus);
    m_attachBtn->setToolTip(tr("更多附件选项"));

    // 发送按钮：输入框里有字才出现（和 web 端一样），样式复用 #primaryButton
    m_sendBtn = new QPushButton(tr("发送"), inner);
    m_sendBtn->setObjectName("primaryButton");
    m_sendBtn->setFixedSize(56, 32);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setFocusPolicy(Qt::NoFocus);
    m_sendBtn->hide();
    connect(m_sendBtn, &QPushButton::clicked, this, &Composer::onSendClicked);
    connect(edit->document(), &QTextDocument::contentsChanged, this, [this] {
        m_sendBtn->setVisible(!m_edit->toPlainText().trimmed().isEmpty());
    });

    // 表情按钮：Unicode 笑脸字符（U+263A）
    m_emojiBtn = new QToolButton(inner);
    m_emojiBtn->setObjectName("composerToolBtn");
    m_emojiBtn->setText(QStringLiteral("☺"));
    m_emojiBtn->setFixedSize(24, 24);
    m_emojiBtn->setCursor(Qt::PointingHandCursor);
    m_emojiBtn->setFocusPolicy(Qt::NoFocus); // 点它不抢输入框的焦点
    m_emojiBtn->setToolTip(tr("表情"));
    connect(m_emojiBtn, &QToolButton::clicked, this, &Composer::toggleEmojiPanel);

    innerLayout->addWidget(m_attachBtn, 0, Qt::AlignVCenter);
    innerLayout->addWidget(m_edit, 1, Qt::AlignVCenter);
    innerLayout->addWidget(m_sendBtn, 0, Qt::AlignVCenter);
    innerLayout->addWidget(m_emojiBtn, 0, Qt::AlignVCenter);

    // 回复提示条：右键消息点“回复”后出现在输入框上方
    m_replyBar = new QWidget(this);
    m_replyBar->setObjectName("composerReply");
    auto *replyLayout = new QHBoxLayout(m_replyBar);
    replyLayout->setContentsMargins(16, 6, 12, 6);
    replyLayout->setSpacing(8);
    m_replyText = new QLabel(m_replyBar);
    m_replyText->setObjectName("composerReplyText");
    m_replyText->setTextFormat(Qt::PlainText);
    m_replyText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 太长就截断，不把窗口撑宽
    auto *replyCancel = new QToolButton(m_replyBar);
    replyCancel->setObjectName("composerReplyCancel");
    replyCancel->setText(QStringLiteral("×"));
    replyCancel->setFixedSize(18, 18);
    replyCancel->setCursor(Qt::PointingHandCursor);
    replyCancel->setFocusPolicy(Qt::NoFocus);
    replyCancel->setToolTip(tr("取消回复"));
    replyLayout->addWidget(m_replyText, 1);
    replyLayout->addWidget(replyCancel);
    m_replyBar->hide();
    connect(replyCancel, &QToolButton::clicked, this, [this] {
        clearReplyTarget();
        emit replyCancelled();
    });

    outerLayout->addWidget(m_replyBar);
    outerLayout->addWidget(inner);
    adjustHeight();
}

void Composer::setReplyTarget(const QString &name, const QString &preview) {
    m_replyText->setText(tr("正在回复 @%1：%2").arg(name, preview.simplified()));
    m_replyBar->show();
    m_edit->setFocus();
}

void Composer::clearReplyTarget() {
    m_replyBar->hide();
    m_replyText->clear();
}

void Composer::toggleEmojiPanel() {
    if (!m_emojiPanel) {
        m_emojiPanel = new EmojiPanel(this);
        connect(m_emojiPanel, &EmojiPanel::emojiSelected, this, [this](const QString &emoji) {
            m_edit->insertPlainText(emoji);
        });
    }
    m_emojiPanel->popup(QRect(m_emojiBtn->mapToGlobal(QPoint(0, 0)), m_emojiBtn->size()));
}

void Composer::setPlaceholder(const QString &text) {
    m_edit->setPlaceholderText(text);
}

void Composer::setEnabled(bool enabled) {
    m_edit->setEnabled(enabled);
    m_emojiBtn->setEnabled(enabled);
    m_attachBtn->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled);
}

void Composer::onSendClicked() {
    const QString text = m_edit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    emit messageSent(text);
    m_edit->clear();
}

void Composer::adjustHeight() {
    // 一行时就是一行字的高度（由盒子负责垂直居中），多行跟着内容长，最高 160
    const int lineH  = QFontMetrics(m_edit->font()).lineSpacing();
    const int docH   = qCeil(m_edit->document()->size().height());
    const int target = qMin(qMax(docH, lineH), 160);
    if (m_edit->height() != target) m_edit->setFixedHeight(target);
}
