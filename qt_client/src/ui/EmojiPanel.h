#pragma once
#include <QFrame>
#include <QList>

class QPushButton;
class QScrollArea;
class QStackedWidget;

// 表情面板：Qt::Popup 弹出，点外面自动关闭；选了表情不关，可以连着选
// 尺寸、配色对齐 web 端
class EmojiPanel : public QFrame {
    Q_OBJECT
public:
    explicit EmojiPanel(QWidget *parent = nullptr);
    // anchorGlobal：表情按钮的全局矩形。面板右边缘对齐按钮右边缘，在按钮上方 8px 弹出；上方放不下就放下方
    void popup(const QRect &anchorGlobal);

signals:
    void emojiSelected(const QString &emoji);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void fillPage(int idx);       // 某个分类第一次显示时才建它的按钮
    void showCategory(int idx);

    QList<QPushButton *> m_tabs;
    QList<QScrollArea *> m_pages;
    QStackedWidget      *m_stack = nullptr;
    QRect                m_anchor;
};
