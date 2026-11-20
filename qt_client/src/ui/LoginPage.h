#pragma once

// 登录 / 注册页：复刻 web 端 .demo-login-window（同一张卡片里切换两种表单）。
// 只做界面与输入校验；提交时发信号，结果由上层通过 setBusy / showError / showRegistered 回填。
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget *parent = nullptr);

    void setBusy(bool busy);                    // 请求在途：禁用提交按钮，防止重复提交
    void showError(const QString &message);     // 显示失败原因，并解除 busy
    // 注册成功：切回登录表单并把 UID 填好（注册不建立登录态，还要再登录一次）
    void showRegistered(const QString &uid, const QString &email);
    void resetForm();                           // 退出登录后再次显示前：回到登录表单、清密码和提示

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
