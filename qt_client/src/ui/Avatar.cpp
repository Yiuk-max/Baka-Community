#include "Avatar.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>

namespace avatar {

QPixmap rounded(const QString &path, int size, int radius) {
    const qreal ratio = qGuiApp->devicePixelRatio();
    const int pixels = qRound(size * ratio);

    // 先按“铺满”缩放，长的一边会超出，下面居中画就等于裁掉两侧
    // 加载图片，目标宽、高，保持比例铺满、平滑缩放
    const QPixmap source = QPixmap(path).scaled(pixels, pixels, Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
    
    // 创建透明画布
    QPixmap result(pixels, pixels);
    result.fill(Qt::transparent);
    

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);//抗锯齿
    // 创建一个空路径
    QPainterPath clip;
    // 路径中添加圆形矩阵
    clip.addRoundedRect(QRectF(0, 0, pixels, pixels), radius * ratio, radius * ratio);
    // 设置裁剪区域
    painter.setClipPath(clip);
    //将原图画到画布上，   起点x                                起点y                   要绘制的资源
    painter.drawPixmap((pixels - source.width()) / 2, (pixels - source.height()) / 2, source);
    painter.end();//结束绘制

    result.setDevicePixelRatio(ratio);//存在DRP倍数关系
    return result;
}

QIcon roundedIcon(const QString &path, int size, int radius) {
    const QPixmap pixmap = rounded(path, size, radius);
    QIcon icon(pixmap);
    // 列表项选中时 Qt 默认会给图标盖一层高亮色，这里指定选中态用原图
    icon.addPixmap(pixmap, QIcon::Selected);
    return icon;
}

} // namespace avatar
