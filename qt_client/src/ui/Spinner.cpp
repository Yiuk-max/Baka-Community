#include "Spinner.h"

#include <QPainter>
#include <QPen>
#include <QTimer>

namespace {
const int kSize = 20;          // 控件边长（四周留 1px 抗锯齿余量，圆的外径是 18）
const qreal kLineWidth = 2.0;  // 圈的粗细
const int kPeriodMs = 800;     // 转一圈的时间
const int kArcDegrees = 90;    // 弧占四分之一圈
}

Spinner::Spinner(QWidget *parent) : QWidget(parent) {
    setFixedSize(kSize, kSize);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);   // 约 60 帧
    connect(m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

void Spinner::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);//开启抗锯齿

    // 线有宽度，圆要往里缩半个线宽，否则边缘被裁掉
    const qreal inset = kLineWidth / 2 + 1;
    const QRectF circle = QRectF(rect()).adjusted(inset, inset, -inset, -inset);//整数矩形转换成浮点矩形
    //左边向右缩进 inset
    //上边向下缩进 inset
    //右边向左缩进 inset
    //下边向上缩进 inset

    painter.setPen(QPen(m_trackColor, kLineWidth));
    painter.drawEllipse(circle);//绘制椭圆

    // 角度按经过的时间算，而不是每帧加固定值，卡顿时速度也不变
    const qreal angle = 360.0 * (m_clock.elapsed() % kPeriodMs) / kPeriodMs;
    QPen arcPen(m_arcColor, kLineWidth);
    // 圆弧
    arcPen.setCapStyle(Qt::RoundCap);//线条圆角
    painter.setPen(arcPen);
    // drawArc 的单位是 1/16 度，正值逆时针；起点 90°（正上方），取负让它顺时针转
    painter.drawArc(circle, static_cast<int>((90 - angle) * 16), -kArcDegrees * 16);
}

void Spinner::showEvent(QShowEvent *event) {
    m_clock.start();
    m_timer->start();
    QWidget::showEvent(event);
}

void Spinner::hideEvent(QHideEvent *event) {
    m_timer->stop();
    QWidget::hideEvent(event);
}
