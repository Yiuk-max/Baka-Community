#include "Avatar.h"

#include "Assets.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace avatar {

QPixmap rounded(const QString &path, int size, int radius) {
    const qreal ratio = qGuiApp->devicePixelRatio();
    const int pixels = qRound(size * ratio);

    // 头像文件名里带 file.id，换头像就是换文件，所以按 路径 + 尺寸 缓存不会拿到旧图
    const QString cacheKey = QStringLiteral("avatar:%1:%2:%3").arg(path).arg(pixels).arg(radius);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) return cached;

    // QPixmap 按文件内容认格式，本地头像文件没有扩展名也能读
    const QPixmap loaded(path);
    if (loaded.isNull()) return QPixmap();

    // 先按“铺满”缩放，长的一边会超出，下面居中画就等于裁掉两侧
    // 加载图片，目标宽、高，保持比例铺满、平滑缩放
    const QPixmap source = loaded.scaled(pixels, pixels, Qt::KeepAspectRatioByExpanding,
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
    QPixmapCache::insert(cacheKey, result);
    return result;
}

QIcon roundedIcon(const QString &path, int size, int radius) {
    const QPixmap pixmap = rounded(path, size, radius);
    QIcon icon(pixmap);
    // 列表项选中时 Qt 默认会给图标盖一层高亮色，这里指定选中态用原图
    icon.addPixmap(pixmap, QIcon::Selected);
    return icon;
}

QPixmap user(const QString &path, int size) {
    const QPixmap pixmap = path.isEmpty() ? QPixmap() : rounded(path, size, size / 2);
    return pixmap.isNull() ? rounded(assets::defaultAvatar(), size, size / 2) : pixmap;
}

QIcon userIcon(const QString &path, int size) {
    const QPixmap pixmap = user(path, size);
    QIcon icon(pixmap);
    icon.addPixmap(pixmap, QIcon::Selected);   // 同 roundedIcon：选中态用原图
    return icon;
}

} // namespace avatar
