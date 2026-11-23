#pragma once

// 聊天里各种附件的“样子”都集中在这里（和 FileCover.cpp 两个文件）：
//   - 常见格式文件的封面（PDF / DOC / ZIP / AUDIO…，分类、标签、配色对齐 web 端 file-cover.js）
//   - 图片还没加载出来时的默认背景图
//   - 视频封面
//   - 文件大小的显示文字
// 全是纯函数，不碰网络；谁要显示附件就来这里拿图。
#include <QPixmap>
#include <QSize>
#include <QString>

namespace filecover {

bool isImage(const QString &fileName);      // 聊天里按图片显示的格式（png / jpg / gif / webp）
bool isVideo(const QString &fileName);      // 按视频封面显示的格式

// 封面上的标签：PDF / DOC / … ；不认识的扩展名取它的前 4 个字母，没有扩展名是 FILE
QString label(const QString &fileName);

// 文件封面：一张折角的纸 + 中间的小图形 + 底部的标签。size 是边长（逻辑像素）
QPixmap cover(const QString &fileName, int size);

// 图片的默认背景图：用内置的默认背景裁成圆角、压暗，中间一个占位符号
QPixmap imagePlaceholder(const QSize &size);

// 视频封面：深色圆角底 + 播放符号 + VIDEO
QPixmap videoCover(const QSize &size);

// 文件大小：不到 1 MB 显示 “N KB”，否则 “N.N MB”（和 web 端 formatFileSize 一致）
QString formatSize(quint64 bytes);

}
