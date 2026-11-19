#pragma once

// 登录 / 注册页：复刻 web 端 .demo-login-window（同一张卡片里切换两种表单）。
// 只做界面与输入校验；提交时发信号，网络层还没接。
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget *parent = nullptr);

signals:
    // 账号：UID 或邮箱，原样交给上层判断
    void loginRequested(const QString &identity, const QString &password);
    void registerRequested(const QString &nickname, const QString &email, const QString &password);

private:
    void submitLogin();
    void submitRegister();
    void switchMode();

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_error = nullptr;
    QStackedWidget *m_forms = nullptr;   // 0 = 登录表单，1 = 注册表单
    //登录
    QLineEdit *m_identity = nullptr;     // 登录身份码，UID或者email
    QLineEdit *m_password = nullptr;
    //注册
    QLineEdit *m_regNickname = nullptr;
    QLineEdit *m_regEmail = nullptr;
    QLineEdit *m_regPassword = nullptr;
    //提交、切换
    QPushButton *m_submit = nullptr;
    QPushButton *m_switch = nullptr;
};
