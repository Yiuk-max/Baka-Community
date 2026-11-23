#pragma once

// 程序里所有弹出菜单（消息右键、社区右键、工具栏“＋”）共用的外观设置。
// 颜色圆角写在 style.qss 的 QMenu#appMenu；这里只做 QSS 做不了的事。
#include <QMenu>

namespace menus {

inline void prepare(QMenu *menu) {
    menu->setObjectName("appMenu");
    // 圆角外面要透明：去掉系统边框和阴影，否则四个角是黑的
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);
}

}
