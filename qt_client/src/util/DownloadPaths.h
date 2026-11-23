#pragma once

// 下载相关的本地路径与文件名规则（不含任何界面代码）：
//   * 媒体缓存放当前账号自己的目录 <exe>/users/<UID>/...
//   * 普通文件放系统“下载”目录，路径每次新建任务时重新解析
//   * 文件名要做跨平台清洗，避免路径穿越 / Windows 保留名 / 重名覆盖
#include <QString>

namespace downloadpaths {

// ---- 账号目录（媒体缓存） ----
QString userRoot(int uid);              // <exe>/users/<uid>
QString mediaDir(int uid);              // <exe>/users/<uid>/media
QString imagesDir(int uid);             // .../media/images
QString videosDir(int uid);             // .../media/videos
QString thumbsDir(int uid);             // .../media/thumbs
QString transfersDir(int uid);          // .../transfers（断点记录）
QString ensureDir(const QString &dir);  // 创建目录；失败返回空字符串

// 缓存里已经存在的媒体文件：media/{images,videos,thumbs}/<fileId>.<ext>。
// 找不到返回空字符串。调用方自己决定是否信任它（大小不符要重新下载）。
QString cachedMediaPath(int uid, int fileId);

// ---- 系统下载目录 ----
// 解析顺序：BAKA_DOWNLOAD_DIR（可含 %VAR% / $VAR / 开头的 ~）→ 系统默认下载目录。
// 解析、创建或写权限失败时返回空，并在 error 里写原因；不会回落到工作目录。
QString downloadDirPath(QString *error = nullptr);

// ---- 文件名 ----
// 去掉路径分隔符、控制字符、Windows 非法字符和结尾的点/空格；保留扩展名并限长。
QString sanitizeFileName(const QString &name,
                         const QString &fallback = QStringLiteral("file"));

// dir 下不冲突的完整路径：重名时用 "名称 (1).扩展名" 递增。
QString uniqueFilePath(const QString &dir, const QString &fileName);

// 临时文件路径：和最终文件同目录，避免跨盘改名失败。
QString partPathFor(const QString &finalPath, const QString &taskTag);

} // namespace downloadpaths
