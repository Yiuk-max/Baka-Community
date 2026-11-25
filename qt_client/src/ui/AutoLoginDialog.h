#pragma once

// 自动登录等待窗口：本地有 token 时代替登录窗口先显示，token 验证失败再换成登录窗口。
// 无系统标题栏、没有关闭按钮：上面 logo，中间转圈，下面 "登录." "登录.." "登录..." 轮换
#include <QDialog>

class QLabel;
class QTimer;

class AutoLoginDialog : public QDialog {
    Q_OBJECT
public:
    // exec() 的第三种结果：自动登录没成，接下来要显示登录窗口。
    // Accepted = 登录成功；Rejected = 用户自己关掉了窗口（Alt+F4 / 任务栏），直接退出程序
    enum Result { NeedLogin = QDialog::Accepted + 1 };

    explicit AutoLoginDialog(QWidget *parent = nullptr);

public slots:
    void fallbackToLogin();   // 结束等待，exec() 返回 NeedLogin

private:
    void advanceDots();

    QLabel *m_text = nullptr;
    QTimer *m_dotsTimer = nullptr;
    int m_dots = 1;           // 当前显示几个点（1~3）
};
