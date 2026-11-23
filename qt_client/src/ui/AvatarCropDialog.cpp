#include "AvatarCropDialog.h"

#include <QBuffer>
#include <QEvent>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

AvatarCropDialog::AvatarCropDialog(const QString &imagePath, QWidget *parent) : QDialog(parent) {
    // 无标题栏 + 背景透明，卡片自己画圆角（和 SettingsDialog 一致）
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("裁剪头像"));

    // 按拍照时的方向读入（手机照片带 EXIF 旋转信息）
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    m_source = reader.read();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // 窗口大小跟着卡片走

    auto *card = new QWidget(this);
    card->setObjectName("cropCard");
    root->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(0);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(tr("裁剪头像"));
    title->setObjectName("settingsTitle");
    titleRow->addWidget(title, 1);
    auto *closeButton = new QPushButton(tr("×"));
    closeButton->setObjectName("closeButton");
    closeButton->setFixedSize(24, 24);
    closeButton->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    auto *hint = new QLabel(isValid() ? tr("拖动调整位置，滚轮或滑杆缩放。头像按 1:1 正方形裁剪。")
                                     : tr("这张图片打不开，请换一张（png / jpg 等常见格式）。"));
    hint->setObjectName("cropHint");
    hint->setWordWrap(true);
    layout->addSpacing(6);
    layout->addWidget(hint);

    // ---- 1:1 预览框：所见即所得，框里显示的就是要上传的区域 ----
    m_view = new QLabel;
    m_view->setObjectName("cropView");
    m_view->setFixedSize(kViewSize, kViewSize);
    m_view->setCursor(Qt::OpenHandCursor);
    m_view->installEventFilter(this);   // 拖动 / 滚轮交给对话框处理
    layout->addSpacing(14);
    layout->addWidget(m_view, 0, Qt::AlignHCenter);

    // ---- 缩放滑杆 ----
    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(8);
    auto *zoomLabel = new QLabel(tr("缩放"));
    zoomLabel->setObjectName("settingsRowName");
    zoomRow->addWidget(zoomLabel);
    m_zoom = new QSlider(Qt::Horizontal);
    m_zoom->setObjectName("zoomSlider");
    m_zoom->setRange(100, kZoomMax);
    m_zoom->setValue(100);
    m_zoom->setEnabled(isValid());
    zoomRow->addWidget(m_zoom, 1);
    layout->addSpacing(12);
    layout->addLayout(zoomRow);

    // ---- 底部按钮 ----
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(tr("取消"));
    cancel->setObjectName("settingsRowButton");
    cancel->setFixedSize(72, 32);
    cancel->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(cancel);
    buttons->addSpacing(8);
    auto *confirm = new QPushButton(tr("确定"));
    confirm->setObjectName("primaryButton");
    confirm->setFixedSize(72, 32);
    confirm->setCursor(Qt::PointingHandCursor);
    confirm->setEnabled(isValid());
    buttons->addWidget(confirm);
    layout->addSpacing(16);
    layout->addLayout(buttons);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, &AvatarCropDialog::acceptCrop);
    connect(m_zoom, &QSlider::valueChanged, this, &AvatarCropDialog::onZoomSlider);

    resetView();
}

// 初始状态：缩放到刚好盖满预览框，并居中
void AvatarCropDialog::resetView() {
    if (!isValid()) return;
    m_baseScale = qMax(static_cast<qreal>(kViewSize) / m_source.width(),
                       static_cast<qreal>(kViewSize) / m_source.height());
    m_scale = m_baseScale;
    m_offset = QPointF((kViewSize - m_source.width() * m_scale) / 2.0,
                       (kViewSize - m_source.height() * m_scale) / 2.0);
    clampOffset();
    renderPreview();
}

void AvatarCropDialog::applyScale(qreal scale, const QPointF &anchor) {
    if (!isValid()) return;
    const qreal next = qBound(m_baseScale, scale, m_baseScale * kZoomMax / 100.0);
    if (qFuzzyCompare(next, m_scale)) return;

    // 缩放前后，anchor 下面盖着的那一个图片像素不动（以光标 / 框中心为锚点）
    const QPointF imagePoint((anchor.x() - m_offset.x()) / m_scale,
                             (anchor.y() - m_offset.y()) / m_scale);
    m_scale = next;
    m_offset = anchor - imagePoint * m_scale;
    clampOffset();
    renderPreview();

    // 滚轮缩放后把滑杆同步过去（不要再触发一次缩放）
    const int percent = qRound(m_scale / m_baseScale * 100.0);
    m_zoom->blockSignals(true);
    m_zoom->setValue(qBound(100, percent, kZoomMax));
    m_zoom->blockSignals(false);
}

void AvatarCropDialog::onZoomSlider(int value) {
    applyScale(m_baseScale * value / 100.0, QPointF(kViewSize / 2.0, kViewSize / 2.0));
}

void AvatarCropDialog::clampOffset() {
    const qreal width = m_source.width() * m_scale;
    const qreal height = m_source.height() * m_scale;
    // 图片必须盖满预览框：左边不能露到 0 右边，上边也不能露
    m_offset.setX(qBound(kViewSize - width, m_offset.x(), 0.0));
    m_offset.setY(qBound(kViewSize - height, m_offset.y(), 0.0));
}

void AvatarCropDialog::renderPreview() {
    const qreal ratio = devicePixelRatioF();
    QPixmap canvas(QSize(kViewSize, kViewSize) * ratio);
    canvas.fill(QColor("#17171a"));

    if (isValid()) {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRectF(m_offset.x() * ratio, m_offset.y() * ratio,
                                 m_source.width() * m_scale * ratio,
                                 m_source.height() * m_scale * ratio),
                          m_source);
        painter.end();
    }

    canvas.setDevicePixelRatio(ratio);
    m_view->setPixmap(canvas);
}

bool AvatarCropDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_view || !isValid()) return QDialog::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) break;
        m_draggingImage = true;
        m_dragPos = mouse->position();
        m_view->setCursor(Qt::ClosedHandCursor);
        return true;
    }
    case QEvent::MouseMove: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!m_draggingImage) break;
        const QPointF delta = mouse->position() - m_dragPos;
        m_dragPos = mouse->position();
        m_offset += delta;
        clampOffset();
        renderPreview();
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) break;
        m_draggingImage = false;
        m_view->setCursor(Qt::OpenHandCursor);
        return true;
    }
    case QEvent::Wheel: {
        auto *wheel = static_cast<QWheelEvent *>(event);
        const qreal steps = wheel->angleDelta().y() / 120.0;
        if (qFuzzyIsNull(steps)) break;
        applyScale(m_scale * std::pow(1.15, steps), wheel->position());
        return true;
    }
    default:
        break;
    }
    return QDialog::eventFilter(watched, event);
}

void AvatarCropDialog::acceptCrop() {
    if (!isValid()) return;

    // 预览框对应的图片区域，裁出来再缩到输出尺寸（1:1，不变形）
    const int side = qRound(static_cast<qreal>(kViewSize) / m_scale);
    const QRect source(qRound(-m_offset.x() / m_scale), qRound(-m_offset.y() / m_scale), side, side);
    const QRect area = source.intersected(m_source.rect());
    if (area.isEmpty()) {
        reject();
        return;
    }

    const QImage square = m_source.copy(area).scaled(kOutputSize, kOutputSize,
                                                     Qt::IgnoreAspectRatio,
                                                     Qt::SmoothTransformation);
    m_png.clear();
    QBuffer buffer(&m_png);
    if (!buffer.open(QIODevice::WriteOnly) || !square.save(&buffer, "PNG")) {
        m_png.clear();
        reject();
        return;
    }
    accept();
}

// ---- 拖动窗口：预览框外（标题、空白处）按住就能拖动 ----
void AvatarCropDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_draggingWindow = true;
        m_windowOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void AvatarCropDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_draggingWindow && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_windowOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void AvatarCropDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_draggingWindow = false;
    QDialog::mouseReleaseEvent(event);
}
