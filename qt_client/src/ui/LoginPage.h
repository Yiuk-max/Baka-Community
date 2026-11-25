#pragma once

// 登录 / 注册页：复刻 web 端 .demo-login-window（同一张卡片里切换两种表单）。
// 注册表单一侧是个内层 QStackedWidget：填资料 → 输入邮箱验证码。
// 只做界面与输入校验；提交时发信号，结果由上层通过 setBusy / showError / show* 回填。
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class VerifyEmailForm;

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget *parent = nullptr);

    void setBusy(bool busy);                    // 请求在途：禁用提交按钮，防止重复提交
    void showError(const QString &message);     // 显示失败原因，并解除 busy
    // 注册成功：切到邮箱验证页（服务端已经发过验证码，注册不建立登录态）
    void showRegistered(const QString &uid, const QString &email);
    // 登录被服务端拦住（密码对但邮箱未验证）：同样切到邮箱验证页
    void showVerificationRequired(const QString &uid, const QString &email);
    void showVerified();                        // 验证通过：回登录表单并填好 UID
    void showVerifyCodeSent(int seconds);       // 重发成功：提示 + 重新倒计时
    void showVerifyCooldown(int seconds);       // 服务端还在冷却：告知还要等几秒
    void resetForm();                           // 退出登录后再次显示前：回到登录表单、清密码和提示

signals:
    // 账号：UID 或邮箱，原样交给上层判断
    void loginRequested(const QString &identity, const QString &password);
    void registerRequested(const QString &nickname, const QString &email, const QString &password);
    // 邮箱验证码：uid 是待验证账号，email 用于重发
    void verifyCodeSubmitted(const QString &code, const QString &uid);
    void resendVerifyRequested(const QString &email);

private:
    void submitLogin();
    void submitRegister();
    void switchMode();
    void setModeLogin();
    void setModeRegister();
    void setModeVerify();

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_error = nullptr;
    QStackedWidget *m_forms = nullptr;   // 0 = 登录表单，1 = 注册（填资料/邮箱验证）
    //登录
    QLineEdit *m_identity = nullptr;     // 登录身份码，UID或者email
    QLineEdit *m_password = nullptr;
    //注册
    QLineEdit *m_regNickname = nullptr;
    QLineEdit *m_regEmail = nullptr;
    QLineEdit *m_regPassword = nullptr;
    QStackedWidget *m_regForms = nullptr; // 0 = 填资料表单，1 = 邮箱验证码表单
    //邮箱验证码
    VerifyEmailForm *m_verifyForm = nullptr; // 验证码输入 + 重发倒计时（独立 QWidget）
    QString m_verifyUid;                 // 待验证账号的 UID
    QString m_verifyEmail;               // 重发验证码用的邮箱
    bool m_verifyFromLogin = false;      // 验证页是“登录被拦住”进来的：返回时回登录表单
    //提交、切换
    QPushButton *m_submit = nullptr;
    QPushButton *m_switch = nullptr;
};
