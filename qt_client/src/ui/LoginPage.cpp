#include "LoginPage.h"

#include "VerifyEmailForm.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

// 创建输入框
QLineEdit *makeInput(const QString &placeholder, bool password = false) {
    auto *edit = new QLineEdit;
    edit->setObjectName("loginInput");
    edit->setPlaceholderText(placeholder);
    edit->setFixedHeight(40);
    if (password) edit->setEchoMode(QLineEdit::Password);
    return edit;
}

// 创建输入框标签
QLabel *makeFieldLabel(const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("fieldLabel");
    return label;
}

} // namespace

LoginPage::LoginPage(QWidget *parent) : QWidget(parent) {
    setObjectName("loginPage");

    // 主页面纵向居中 + 横向居中
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);//完全融入Dialog，所以填0
    root->addStretch();//顶部弹簧

    auto *row = new QHBoxLayout;
    row->addStretch();//左侧弹簧

    // 创建真正的登录卡片，用QSS设置样式，固定宽度
    auto *card = new QWidget;
    card->setObjectName("loginCard");
    card->setFixedWidth(400);
    row->addWidget(card);

    row->addStretch();//右侧弹簧
    root->addLayout(row);
    root->addStretch();//底部弹簧

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 28, 28, 24);
    layout->setSpacing(0);// 控制组件间距为0，后面手动控制

    // logo
    auto *logo = new QLabel;
    logo->setObjectName("loginLogo");
    // 设置图标，缩放到 80x80，保持纵横比，平滑缩放
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/badge-256.png"))
                        .scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignHCenter);
    layout->addWidget(logo);

    // title
    m_title = new QLabel(tr("欢迎回来"));
    m_title->setObjectName("loginTitle");
    m_title->setAlignment(Qt::AlignHCenter);
    layout->addSpacing(14);
    layout->addWidget(m_title);

    // 子标题
    m_subtitle = new QLabel(tr("登录以继续使用 Baka Community"));
    m_subtitle->setObjectName("loginSubtitle");
    m_subtitle->setAlignment(Qt::AlignHCenter);
    layout->addSpacing(6);
    layout->addWidget(m_subtitle);

    // 控制组件间距
    layout->addSpacing(20);

    m_forms = new QStackedWidget;

    // --- 登录表单 ---
    auto *loginForm = new QWidget;
    auto *loginLayout = new QVBoxLayout(loginForm);
    loginLayout->setContentsMargins(0, 0, 0, 0);
    loginLayout->setSpacing(0);
    // 账号输入框
    loginLayout->addWidget(makeFieldLabel(tr("账号")));
    loginLayout->addSpacing(3);
    m_identity = makeInput(tr("UID 或邮箱"));
    loginLayout->addWidget(m_identity);
    loginLayout->addSpacing(10);
    // 密码输入框
    loginLayout->addWidget(makeFieldLabel(tr("密码")));
    loginLayout->addSpacing(3);
    m_password = makeInput(tr("请输入密码"), true);
    loginLayout->addWidget(m_password);

    m_forms->addWidget(loginForm);

    // --- 注册表单 ---
    // 注册拆成两页：先填资料，注册成功后留在同一块里输入邮箱验证码
    auto *registerPage = new QWidget;
    auto *registerPageLayout = new QVBoxLayout(registerPage);
    registerPageLayout->setContentsMargins(0, 0, 0, 0);
    registerPageLayout->setSpacing(0);
    m_regForms = new QStackedWidget;
    registerPageLayout->addWidget(m_regForms);

    auto *registerForm = new QWidget;
    auto *registerLayout = new QVBoxLayout(registerForm);
    registerLayout->setContentsMargins(0, 0, 0, 0);
    registerLayout->setSpacing(0);
    registerLayout->addWidget(makeFieldLabel(tr("昵称")));
    registerLayout->addSpacing(6);
    m_regNickname = makeInput(tr("你的昵称"));
    registerLayout->addWidget(m_regNickname);
    registerLayout->addSpacing(14);
    registerLayout->addWidget(makeFieldLabel(tr("邮箱")));
    registerLayout->addSpacing(6);
    m_regEmail = makeInput(tr("name@example.com"));
    registerLayout->addWidget(m_regEmail);
    registerLayout->addSpacing(14);
    registerLayout->addWidget(makeFieldLabel(tr("密码")));
    registerLayout->addSpacing(6);
    m_regPassword = makeInput(tr("至少 6 位"), true);
    registerLayout->addWidget(m_regPassword);

    m_regForms->addWidget(registerForm);

    // --- 邮箱验证码表单 ---
    m_verifyForm = new VerifyEmailForm;
    m_regForms->addWidget(m_verifyForm);

    m_forms->addWidget(registerPage);

    layout->addWidget(m_forms);

    // 错误提示标签，默认隐藏
    m_error = new QLabel;
    m_error->setObjectName("loginError");
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addSpacing(10);
    layout->addWidget(m_error);

    layout->addSpacing(16);
    // 登录按钮
    m_submit = new QPushButton(tr("登录"));
    m_submit->setObjectName("primaryButton");
    m_submit->setFixedHeight(40);
    m_submit->setCursor(Qt::PointingHandCursor);
    m_submit->setDefault(true);
    layout->addWidget(m_submit);

    layout->addSpacing(12);
    // 注册按钮
    m_switch = new QPushButton(tr("没有账号？注册"));
    m_switch->setObjectName("linkButton");
    m_switch->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_switch, 0, Qt::AlignHCenter);

    connect(m_submit, &QPushButton::clicked, this, [this] {
        m_forms->currentIndex() == 0 ? submitLogin() : submitRegister();
    });
    connect(m_switch, &QPushButton::clicked, this, &LoginPage::switchMode);
    //enter快捷键
    connect(m_identity, &QLineEdit::returnPressed, this, &LoginPage::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &LoginPage::submitLogin);
    connect(m_regPassword, &QLineEdit::returnPressed, this, &LoginPage::submitRegister);

    // 验证码表单只负责输入和倒计时，校验和发信号在这里做（和其他表单保持一致）
    connect(m_verifyForm, &VerifyEmailForm::codeSubmitted, this, [this](const QString &code) {
        if (code.isEmpty()) {
            m_error->setText(tr("请输入邮件里的验证码"));
            m_error->show();
            return;
        }
        m_error->hide();
        emit verifyCodeSubmitted(code, m_verifyUid);
    });
    connect(m_verifyForm, &VerifyEmailForm::resendRequested, this, [this] {
        if (m_verifyEmail.isEmpty()) {
            m_error->setText(tr("无法确定邮箱，请用邮箱登录后再验证"));
            m_error->show();
            return;
        }
        m_error->hide();
        emit resendVerifyRequested(m_verifyEmail);
    });
    connect(m_verifyForm, &VerifyEmailForm::backRequested, this, [this] {
        m_verifyForm->clearCode();
        m_verifyForm->startCooldown(0);
        m_verifyUid.clear();
        // 从登录被拦住进来的就回登录表单，否则回填资料表单
        if (m_verifyFromLogin) {
            setModeLogin();
        } else {
            setModeRegister();
        }
    });

    //默认登录页面
    m_forms->setCurrentIndex(0);
}

void LoginPage::switchMode() {
    if (m_forms->currentIndex() == 0) {
        setModeRegister();
    } else {
        setModeLogin();
    }
}

// 登录表单：底部的主按钮/切换按钮都是共用的，所以切页时要把文案恢复
void LoginPage::setModeLogin() {
    m_forms->setCurrentIndex(0);
    m_verifyFromLogin = false;
    m_title->setText(tr("欢迎回来"));
    m_subtitle->setText(tr("登录以继续使用 Baka Community"));
    m_submit->setText(tr("登录"));
    m_submit->show();
    m_switch->setText(tr("没有账号？注册"));
    m_switch->show();
    m_error->hide();
}

// 填资料表单
void LoginPage::setModeRegister() {
    m_forms->setCurrentIndex(1);
    m_regForms->setCurrentIndex(0);
    m_verifyFromLogin = false;
    m_title->setText(tr("创建账号"));
    m_subtitle->setText(tr("注册后即可加入社区、添加好友"));
    m_submit->setText(tr("注册"));
    m_submit->show();
    m_switch->setText(tr("已有账号？登录"));
    m_switch->show();
    m_error->hide();
}

// 邮箱验证码表单：验证按钮就在表单里，所以底部共用按钮要藏起来
void LoginPage::setModeVerify() {
    m_forms->setCurrentIndex(1);
    m_regForms->setCurrentIndex(1);
    m_title->setText(tr("验证邮箱"));
    m_subtitle->setText(tr("输入邮件里的 6 位验证码"));
    m_submit->hide();
    m_switch->hide();
    m_error->hide();
}

void LoginPage::setBusy(bool busy) {
    const bool isLogin = m_forms->currentIndex() == 0;
    m_submit->setEnabled(!busy);
    m_switch->setEnabled(!busy);
    m_submit->setText(busy ? tr("请稍候…") : (isLogin ? tr("登录") : tr("注册")));
    // 验证码表单整块跟着禁用，避免重复提交验证码
    m_verifyForm->setBusy(busy);
}

void LoginPage::showError(const QString &message) {
    setBusy(false);
    m_error->setText(message);
    m_error->show();
}

void LoginPage::showRegistered(const QString &uid, const QString &email) {
    setBusy(false);
    m_verifyUid = uid;
    m_verifyEmail = email;
    m_verifyFromLogin = false;
    setModeVerify();
    m_verifyForm->setHint(tr("注册成功。验证码已发送至 %1，请查收后输入。").arg(email));
    m_verifyForm->clearCode();
    m_verifyForm->focusCode();
    m_verifyForm->startCooldown(60);   // 注册时服务端已经发过一封，这里跟着算冷却
}

void LoginPage::showVerificationRequired(const QString &uid, const QString &email) {
    setBusy(false);
    m_verifyUid = uid;
    if (!email.isEmpty()) m_verifyEmail = email;
    m_verifyFromLogin = true;
    setModeVerify();
    m_verifyForm->setHint(m_verifyEmail.isEmpty()
                              ? tr("该账号需先验证邮箱，请输入注册时收到的 6 位验证码。")
                              : tr("该账号需先验证邮箱。验证码已发送至 %1，请查收后输入。")
                                    .arg(m_verifyEmail));
    m_verifyForm->clearCode();
    m_verifyForm->focusCode();
    m_verifyForm->startCooldown(60);
}

void LoginPage::showVerified() {
    setBusy(false);
    setModeLogin();
    m_subtitle->setText(tr("邮箱验证成功，请登录"));
    m_identity->setText(m_verifyUid);   // 填好 UID，省得自己背
    m_verifyUid.clear();
    m_verifyEmail.clear();
    m_verifyForm->clearCode();
    m_verifyForm->startCooldown(0);
    m_verifyFromLogin = false;
    m_password->clear();
    m_password->setFocus();
}

void LoginPage::showVerifyCodeSent(int seconds) {
    setBusy(false);
    m_error->hide();
    if (!m_verifyEmail.isEmpty()) {
        m_verifyForm->setHint(tr("新的验证码已发送至 %1，请查收。").arg(m_verifyEmail));
    }
    m_verifyForm->startCooldown(seconds);
    m_verifyForm->focusCode();
}

void LoginPage::showVerifyCooldown(int seconds) {
    setBusy(false);
    m_verifyForm->startCooldown(seconds);
    m_error->setText(tr("请 %1 秒后再重新发送").arg(seconds));
    m_error->show();
}

void LoginPage::resetForm() {
    setBusy(false);   // 上次登录成功时按钮还停在“请稍候…”
    setModeLogin();
    m_verifyUid.clear();
    m_verifyEmail.clear();
    m_verifyForm->clearCode();
    m_verifyForm->startCooldown(0);
    m_password->clear();
    m_error->hide();
}

void LoginPage::submitLogin() {
    if (!m_submit->isEnabled()) return; // 请求在途时回车不重复提交
    const QString identity = m_identity->text().trimmed();
    const QString password = m_password->text();
    if (identity.isEmpty() || password.isEmpty()) {
        m_error->setText(tr("请输入账号和密码"));
        m_error->show();
        return;
    }
    m_error->hide();
    emit loginRequested(identity, password);
}

void LoginPage::submitRegister() {
    if (!m_submit->isEnabled()) return;
    const QString nickname = m_regNickname->text().trimmed();
    const QString email = m_regEmail->text().trimmed();
    const QString password = m_regPassword->text();
    if (nickname.isEmpty() || email.isEmpty() || password.isEmpty()) {
        m_error->setText(tr("请填写昵称、邮箱和密码"));
        m_error->show();
        return;
    }
    m_error->hide();
    emit registerRequested(nickname, email, password);
}
