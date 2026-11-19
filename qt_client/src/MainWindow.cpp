#include "MainWindow.h"

#include <QStatusBar>
#include <QWidget>

MainWindow::MainWindow(const QString &serverUrl, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Baka Community"));
    resize(1100, 700);

    // 主界面还没做，先放一个空控件占位
    setCentralWidget(new QWidget(this));

    // 骨架阶段把读到的配置显示出来，方便确认 config.json 生效
    statusBar()->showMessage(tr("Qt %1 / 服务器 %2").arg(QT_VERSION_STR, serverUrl));
}
