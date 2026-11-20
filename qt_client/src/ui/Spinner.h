#pragma once

// 转圈加载指示器：一个底圈 + 一段沿着圈转的弧
// 颜色不写在代码里，由 style.qss 通过 qproperty-trackColor / qproperty-arcColor 设置。
#include <QColor>
#include <QElapsedTimer>
#include <QWidget>

class QTimer;

class Spinner : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor trackColor MEMBER m_trackColor)
    Q_PROPERTY(QColor arcColor MEMBER m_arcColor)
public:
    explicit Spinner(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;   // 显示时才开始转
    void hideEvent(QHideEvent *event) override;   // 隐藏就停，不空转

private:
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock; //计时器
    QColor m_trackColor;   // 底圈
    QColor m_arcColor;     // 转动的弧
};
