#pragma once

// 头像图片处理：把一张图裁成圆角方形（radius 取边长的一半就是圆形）。
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace avatar {

// 缩放到 size x size（逻辑像素，内部按屏幕缩放比放大，高分屏不糊），居中裁剪并切圆角
QPixmap rounded(const QString &path, int size, int radius);

// 同上，包成 QIcon 给列表项 / 按钮用
QIcon roundedIcon(const QString &path, int size, int radius);

} // namespace avatar
