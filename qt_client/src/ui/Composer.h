#pragma once
#include <QWidget>

class QTextEdit;
class QPushButton;
class QToolButton;
class EmojiPanel;

// 底部输入框：Enter 发送，Shift+Enter 换行，高度随内容自适应（最高 160px，和 web 端一致）。
class Composer : public QWidget {
    Q_OBJECT
public:
    explicit Composer(QWidget *parent = nullptr);

    void setPlaceholder(const QString &text);
    void setEnabled(bool enabled);

signals:
    void messageSent(const QString &text);

private:
    void onSendClicked();
    void adjustHeight();
    void toggleEmojiPanel();

    QTextEdit   *m_edit       = nullptr;
    QToolButton *m_attachBtn  = nullptr;   // 左侧“＋”，功能未接
    QPushButton *m_sendBtn    = nullptr;   // 有字才显示
    QToolButton *m_emojiBtn   = nullptr;
    EmojiPanel  *m_emojiPanel = nullptr;   // 第一次点表情按钮时才创建
};
