#include "LoginPage.h"

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
    root->setContentsMargins(24, 24, 24, 24);
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

    m_forms->addWidget(registerForm);

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

    //默认登录页面
    m_forms->setCurrentIndex(0);
}

void LoginPage::switchMode() {
    const bool toRegister = m_forms->currentIndex() == 0;
    m_forms->setCurrentIndex(toRegister ? 1 : 0);
    m_title->setText(toRegister ? tr("创建账号") : tr("欢迎回来"));
    m_subtitle->setText(toRegister ? tr("注册后即可加入社区、添加好友")
                                   : tr("登录以继续使用 Baka Community"));
    m_submit->setText(toRegister ? tr("注册") : tr("登录"));
    m_switch->setText(toRegister ? tr("已有账号？登录") : tr("没有账号？注册"));
    m_error->hide();
}

void LoginPage::submitLogin() {
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
