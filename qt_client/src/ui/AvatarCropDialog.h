#pragma once

// 头像裁剪窗口：固定 1:1 正方形预览，图片可以拖动、滚轮/滑杆缩放，确定后输出正方形 PNG。
// 无系统标题栏（和 SettingsDialog 同一套做法），按住空白处可以拖动窗口。
#include <QByteArray>
#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QPointF>

class QLabel;
class QMouseEvent;
class QSlider;

class AvatarCropDialog : public QDialog {
    Q_OBJECT
public:
    explicit AvatarCropDialog(const QString &imagePath, QWidget *parent = nullptr);

    bool isValid() const { return !m_source.isNull(); }   // 图片读不出来时为 false
    QByteArray croppedPng() const { return m_png; }       // 点“确定”后是 512x512 的 PNG

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;   // 预览区的拖动 / 滚轮缩放
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void resetView();                            // 回到“铺满预览框并居中”
    void applyScale(qreal scale, const QPointF &anchor);   // anchor 是预览框里的坐标，缩放时保持它下面的像素不动
    void clampOffset();                          // 图片必须盖满预览框，不能露底
    void renderPreview();
    void onZoomSlider(int value);
    void acceptCrop();

    static constexpr int kViewSize = 300;    // 预览框边长（= 裁剪区域，所见即所得）
    static constexpr int kOutputSize = 512;  // 上传的图片边长
    static constexpr int kZoomMax = 400;     // 滑块最大值，100 = 刚好铺满

    QImage m_source;
    qreal m_baseScale = 1.0;   // 刚好铺满预览框的缩放
    qreal m_scale = 1.0;       // 当前缩放：图片 1px 在预览框里的像素数
    QPointF m_offset;          // 图片左上角在预览框里的位置
    QLabel *m_view = nullptr;
    QSlider *m_zoom = nullptr;
    QByteArray m_png;
    QPointF m_dragPos;         // 拖动图片时上一次的鼠标位置
    QPoint m_windowOffset;     // 拖动窗口时鼠标相对窗口左上角的偏移
    bool m_draggingImage = false;
    bool m_draggingWindow = false;
};
