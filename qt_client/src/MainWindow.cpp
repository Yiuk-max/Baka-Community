#include "MainWindow.h"

#include <QStatusBar>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Baka Community"));
    resize(1280, 800);
    setCentralWidget(new QWidget(this));
    statusBar()->showMessage(tr("Qt %1 / 骨架就绪").arg(QT_VERSION_STR));
}
