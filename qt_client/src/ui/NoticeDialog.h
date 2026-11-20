#pragma once

// 通知弹窗：只告知一件事，没有系统标题栏、没有关闭按钮，只有一个“我知道了”。
#include <QDialog>

class NoticeDialog : public QDialog {
    Q_OBJECT
public:
    explicit NoticeDialog(const QString &message, QWidget *parent = nullptr);
};
