#pragma once

// 邮箱验证码表单：注册卡片里的第二页（QStackedWidget 的 index 1）。
// 只做输入、页面内文案和重发倒计时；验证结果由 LoginPage 通过 setBusy / setHint 回填。
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class VerifyEmailForm : public QWidget {
    Q_OBJECT
public:
    explicit VerifyEmailForm(QWidget *parent = nullptr);

    void setHint(const QString &text);      // “验证码已发送至 xxx，请查收”
    void setBusy(bool busy);                // 请求在途：禁用验证 / 重发 / 返回
    void startCooldown(int seconds);        // 重发倒计时，0 表示立刻可以重发
    void clearCode();
    void focusCode();

signals:
    void codeSubmitted(const QString &code);
    void resendRequested();
    void backRequested();

private:
    void submit();
    void onCooldownTick();

    QLabel *m_hint = nullptr;
    QLineEdit *m_code = nullptr;
    QPushButton *m_submit = nullptr;
    QPushButton *m_resend = nullptr;
    QPushButton *m_back = nullptr;
    QTimer *m_cooldownTimer = nullptr;
    int m_cooldown = 0;                     // 剩余秒数
    bool m_busy = false;
};
