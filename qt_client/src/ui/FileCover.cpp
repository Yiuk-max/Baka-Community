#include "FileCover.h"

#include "Assets.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

namespace filecover {

namespace {

enum Kind { Pdf, Doc, Xls, Ppt, Zip, Audio, Video, Image, Text, Code, Exe, Font, File };

struct KindInfo {
    Kind kind;
    const char *label;
    QRgb color;         // web 端 .file-cover-xxx 的 --cover
    const char *exts;   // 空格分隔的扩展名
};

// 分类表抄自 frontend/src/components/file-cover.js，颜色抄自 reference-theme.css，改那边要同步
const KindInfo kKinds[] = {
    {Pdf,   "PDF",   0xed4245, "pdf"},
    {Doc,   "DOC",   0x4f7ef5, "doc docx odt rtf pages wps"},
    {Xls,   "XLS",   0x23a559, "xls xlsx csv ods numbers"},
    {Ppt,   "PPT",   0xe8792b, "ppt pptx odp key"},
    {Zip,   "ZIP",   0xf0b232, "zip rar 7z tar gz bz2 xz iso"},
    {Audio, "AUDIO", 0xeb459e, "mp3 wav flac ogg m4a aac wma opus"},
    {Video, "VIDEO", 0x9b59b6, "mp4 mkv mov avi webm flv wmv m4v"},
    {Image, "IMG",   0x1abc9c, "png jpg jpeg gif webp psd ai tif tiff heic raw bmp"},
    {Text,  "TXT",   0x949ba4, "txt md log ini cfg conf"},
    {Code,  "CODE",  0x5a65ea, "js ts jsx tsx vue py java c cpp h hpp cs go rs rb php swift kt html css "
                               "scss json xml yml yaml sh bat ps1 sql proto"},
    {Exe,   "EXE",   0x6d7079, "exe msi apk dmg pkg deb rpm appx"},
    {Font,  "FONT",  0xc27c0e, "ttf otf woff woff2"},
};
const QRgb kDefaultColor = 0x80848e;

QString extensionOf(const QString &fileName) {
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot < 0 ? QString() : fileName.mid(dot + 1).toLower();
}

// 找不到就返回 nullptr（= 普通文件）
const KindInfo *kindOf(const QString &fileName) {
    const QString ext = extensionOf(fileName);
    if (ext.isEmpty()) return nullptr;
    for (const KindInfo &info : kKinds) {
        if (QString::fromLatin1(info.exts).split(QLatin1Char(' ')).contains(ext)) return &info;
    }
    return nullptr;
}

// 建一张透明画布（按屏幕缩放比放大，高分屏不糊）
QPixmap canvas(const QSize &size) {
    const qreal ratio = qGuiApp->devicePixelRatio();
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

// 封面中间的小图形。坐标是 web 端 svg 的 24x24 视口，画之前 painter 已经按比例放大
void drawGlyph(QPainter &p, Kind kind) {
    switch (kind) {
    case Pdf: case Doc: case Text:      // 三行文字
        p.drawLine(QPointF(8.5, 11.5), QPointF(15.5, 11.5));
        p.drawLine(QPointF(8.5, 14.5), QPointF(15.5, 14.5));
        p.drawLine(QPointF(8.5, 17.5), QPointF(12.5, 17.5));
        break;
    case Xls:                           // 表格
        p.drawRect(QRectF(7.5, 10.5, 9, 8));
        p.drawLine(QPointF(7.5, 14.5), QPointF(16.5, 14.5));
        p.drawLine(QPointF(11.5, 10.5), QPointF(11.5, 18.5));
        break;
    case Ppt:                           // 柱状图
        p.drawLine(QPointF(8, 18.5), QPointF(8, 15.5));
        p.drawLine(QPointF(12, 18.5), QPointF(12, 11.5));
        p.drawLine(QPointF(16, 18.5), QPointF(16, 13.5));
        break;
    case Zip:                           // 拉链
        p.drawLine(QPointF(11, 10), QPointF(13, 10));
        p.drawLine(QPointF(11, 12.5), QPointF(13, 12.5));
        p.drawLine(QPointF(11, 15), QPointF(13, 15));
        p.drawLine(QPointF(10.5, 17), QPointF(13.5, 17));
        break;
    case Audio:                         // 音符
        p.drawEllipse(QPointF(8.4, 18.2), 1.6, 1.6);
        p.drawEllipse(QPointF(14.9, 16.6), 1.6, 1.6);
        p.drawPolyline(QPolygonF({{10, 18.2}, {10, 10}, {16.5, 8.4}, {16.5, 16.6}}));
        break;
    case Video:                         // 播放三角
        p.drawPolygon(QPolygonF({{9.5, 10.5}, {16, 14.25}, {9.5, 18}}));
        break;
    case Image:                         // 山 + 太阳
        p.drawPolygon(QPolygonF({{7.5, 18.5}, {10.7, 14.5}, {13, 17.1}, {14.9, 14.7}, {17.5, 18.5}}));
        p.drawEllipse(QPointF(9.4, 11.6), 0.9, 0.9);
        break;
    case Code:                          // < >
        p.drawPolyline(QPolygonF({{9.6, 11.5}, {7, 14}, {9.6, 16.5}}));
        p.drawPolyline(QPolygonF({{14.4, 11.5}, {17, 14}, {14.4, 16.5}}));
        break;
    case Exe:                           // 齿轮
        p.drawEllipse(QPointF(12, 14.1), 1.9, 1.9);
        p.drawLine(QPointF(12, 9.8), QPointF(12, 11));
        p.drawLine(QPointF(12, 17.2), QPointF(12, 18.4));
        p.drawLine(QPointF(7.8, 14.1), QPointF(9, 14.1));
        p.drawLine(QPointF(15, 14.1), QPointF(16.2, 14.1));
        break;
    case Font:                          // 字母 A
        p.drawPolyline(QPolygonF({{8.3, 18.5}, {12, 9.5}, {15.7, 18.5}}));
        p.drawLine(QPointF(9.6, 15.5), QPointF(14.4, 15.5));
        break;
    case File:
        break;
    }
}

// 在 rect 正中画一个符号，下面可选一行小字（图片占位和视频封面共用）
void drawCenterMark(QPainter &p, const QRect &rect, const QString &mark, const QString &caption) {
    QFont font = p.font();
    font.setPixelSize(30);
    p.setFont(font);
    p.setPen(QColor(0xb5, 0xba, 0xc1));
    QRect markRect = rect;
    if (!caption.isEmpty()) markRect.setBottom(rect.center().y() + 8);
    p.drawText(markRect, Qt::AlignHCenter | (caption.isEmpty() ? Qt::AlignVCenter : Qt::AlignBottom), mark);
    if (caption.isEmpty()) return;
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRect(rect.left(), rect.center().y() + 14, rect.width(), 16), Qt::AlignHCenter, caption);
}

} // namespace

bool isImage(const QString &fileName) {
    static const QStringList exts = {"png", "jpg", "jpeg", "gif", "webp"};
    return exts.contains(extensionOf(fileName));
}

bool isVideo(const QString &fileName) {
    const KindInfo *info = kindOf(fileName);
    return info && info->kind == Video;
}

QString label(const QString &fileName) {
    if (const KindInfo *info = kindOf(fileName)) return QString::fromLatin1(info->label);
    const QString ext = extensionOf(fileName);
    return ext.isEmpty() ? QStringLiteral("FILE") : ext.toUpper().left(4);
}

QPixmap cover(const QString &fileName, int size) {
    const KindInfo *info = kindOf(fileName);
    const QColor color(info ? info->color : kDefaultColor);

    QPixmap pixmap = canvas(QSize(size, size));
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    // 纸 + 折角 + 小图形：按 24x24 视口画，整体放大到 size
    p.save();
    p.scale(size / 24.0, size / 24.0);
    QColor fill = color;
    fill.setAlphaF(0.16);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(fill);
    p.drawPolygon(QPolygonF({{6, 2}, {14, 2}, {20, 8}, {20, 22}, {6, 22}}));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QPolygonF({{14, 2}, {14, 8}, {20, 8}}));
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    drawGlyph(p, info ? info->kind : File);
    p.restore();

    // 底部居中的标签：实心圆角小块 + 白字
    QFont font = p.font();
    font.setPixelSize(qMax(8, size / 5));
    font.setBold(true);
    p.setFont(font);
    const QString text = label(fileName);
    const int badgeHeight = font.pixelSize() + 4;
    const int badgeWidth = p.fontMetrics().horizontalAdvance(text) + 8;
    const QRect badge((size - badgeWidth) / 2, size - badgeHeight, badgeWidth, badgeHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(badge, 3, 3);
    p.setPen(Qt::white);
    p.drawText(badge, Qt::AlignCenter, text);
    return pixmap;
}

QPixmap imagePlaceholder(const QSize &size) {
    QPixmap pixmap = canvas(size);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect rect(QPoint(0, 0), size);
    QPainterPath clip;
    clip.addRoundedRect(rect, 8, 8);
    p.setClipPath(clip);

    // 默认背景图铺满（长的一边裁掉），再盖一层半透明的深色，让它退到后面
    const QPixmap background = QPixmap(assets::defaultBackground())
        .scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap((size.width() - background.width()) / 2, (size.height() - background.height()) / 2,
                 background);
    p.fillRect(rect, QColor(0x11, 0x12, 0x14, 170));
    drawCenterMark(p, rect, QStringLiteral("▧"), QString());
    return pixmap;
}

QPixmap videoCover(const QSize &size) {
    QPixmap pixmap = canvas(size);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect rect(QPoint(0, 0), size);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x11, 0x12, 0x14));
    p.drawRoundedRect(rect, 8, 8);
    drawCenterMark(p, rect, QStringLiteral("▶"), QStringLiteral("VIDEO"));
    return pixmap;
}

QString formatSize(quint64 bytes) {
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(qMax<quint64>(1, (bytes + 512) / 1024));
    return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
}

} // namespace filecover
