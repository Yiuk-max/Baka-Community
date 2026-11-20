#pragma once

// 内置静态资源的 qrc 路径（文件在 resources/ 下，由 CMakeLists.txt 的 qt_add_resources 打进 exe）
#include <QString>

namespace assets {

inline QString logo() { return QStringLiteral(":/images/logo.png"); }
// 没设置头像（avatar_id <= 0）时的占位图
inline QString defaultAvatar() { return QStringLiteral(":/images/default_avatar.png"); }
inline QString defaultGroupAvatar() { return QStringLiteral(":/images/default_group_avatar.png"); }
inline QString defaultCommunityAvatar() { return QStringLiteral(":/images/default_comm_avatar.png"); }
inline QString defaultBackground() { return QStringLiteral(":/images/default_background.png"); }
// QMediaPlayer 要的是 URL 形式
inline QString bakaSoundUrl() { return QStringLiteral("qrc:/audio/baka.mp3"); }

}
