#pragma once

// 头像图片处理：把一张图裁成圆角方形（radius 取边长的一半就是圆形）
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace avatar {

// 缩放到 size x size（逻辑像素，内部按屏幕缩放比放大，高分屏不糊），居中裁剪并切圆角
// 图片读不出来时返回空 QPixmap。结果会缓存，同一张图反复要不会重复读文件
QPixmap rounded(const QString &path, int size, int radius);

// 同上，包成 QIcon 给列表项 / 按钮用
QIcon roundedIcon(const QString &path, int size, int radius);

// 用户头像（圆形）：path 是本地头像文件（AvatarStore::path 给的），为空或读不出来就用默认头像
QPixmap user(const QString &path, int size);
QIcon userIcon(const QString &path, int size);

}
