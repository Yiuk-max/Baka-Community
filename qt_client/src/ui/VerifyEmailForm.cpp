#include "VerifyEmailForm.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// 与 LoginPage 里的输入框 / 标签保持同一套 objectName（样式都写在 style.qss 里）
QLabel *makeFieldLabel(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("fieldLabel");
    return label;
}

QPushButton *makeLinkButton(const QString &text) {
    auto *button = new QPushButton(text);
    button->setObjectName("linkButton");
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

VerifyEmailForm::VerifyEmailForm(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 说明文字：验证码发到哪个邮箱
    m_hint = new QLabel;
    m_hint->setObjectName("verifyHint");
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    layout->addSpacing(14);
    layout->addWidget(makeFieldLabel(tr("验证码")));
    layout->addSpacing(6);
    m_code = new QLineEdit;
    m_code->setObjectName("loginInput");
    m_code->setPlaceholderText(tr("邮件里的 6 位数字"));
    m_code->setMaxLength(6);
    m_code->setFixedHeight(40);
    layout->addWidget(m_code);

    layout->addSpacing(16);
    m_submit = new QPushButton(tr("验证"));
    m_submit->setObjectName("primaryButton");
    m_submit->setFixedHeight(40);
    m_submit->setCursor(Qt::PointingHandCursor);
    m_submit->setDefault(true);
    layout->addWidget(m_submit);

    layout->addSpacing(10);
    m_resend = makeLinkButton(tr("重新发送验证码"));
    layout->addWidget(m_resend, 0, Qt::AlignHCenter);
    layout->addSpacing(4);
    m_back = makeLinkButton(tr("返回"));
    layout->addWidget(m_back, 0, Qt::AlignHCenter);

    connect(m_submit, &QPushButton::clicked, this, &VerifyEmailForm::submit);
    connect(m_code, &QLineEdit::returnPressed, this, &VerifyEmailForm::submit);
    connect(m_resend, &QPushButton::clicked, this, [this] {
        if (m_cooldown > 0 || m_busy) return;
        emit resendRequested();
    });
    connect(m_back, &QPushButton::clicked, this, [this] {
        if (m_busy) return;
        emit backRequested();
    });

    // 服务端 60s 冷却，本地也数一遍，省得白等一次报错
    m_cooldownTimer = new QTimer(this);
    m_cooldownTimer->setInterval(1000);
    connect(m_cooldownTimer, &QTimer::timeout, this, &VerifyEmailForm::onCooldownTick);
}

void VerifyEmailForm::setHint(const QString &text) { m_hint->setText(text); }

void VerifyEmailForm::setBusy(bool busy) {
    m_busy = busy;
    m_submit->setEnabled(!busy);
    m_submit->setText(busy ? tr("请稍候…") : tr("验证"));
    m_back->setEnabled(!busy);
    m_resend->setEnabled(!busy && m_cooldown <= 0);
}

void VerifyEmailForm::startCooldown(int seconds) {
    m_cooldown = seconds > 0 ? seconds : 0;
    if (m_cooldown > 0) {
        m_cooldownTimer->start();
    } else {
        m_cooldownTimer->stop();
    }
    m_resend->setText(m_cooldown > 0 ? tr("重新发送 (%1s)").arg(m_cooldown)
                                     : tr("重新发送验证码"));
    m_resend->setEnabled(!m_busy && m_cooldown <= 0);
}

void VerifyEmailForm::onCooldownTick() { startCooldown(m_cooldown - 1); }

void VerifyEmailForm::clearCode() { m_code->clear(); }

void VerifyEmailForm::focusCode() { m_code->setFocus(); }

void VerifyEmailForm::submit() {
    if (!m_submit->isEnabled()) return;   // 请求在途时回车不重复提交
    emit codeSubmitted(m_code->text().trimmed());
}
