#include "LoginDialog.h"

#include "LoginPage.h"

#include <QGridLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角，圆角外面透出桌面）
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);//允许窗口背景透明
    setWindowTitle(tr("Baka Community"));//没有系统标题栏，实际不显示

    // 用 QGridLayout：卡片占满，× 叠在右上角（窗口自己是透明的，8px 外圈看不见）
    auto *layout = new QGridLayout(this);//网格布局
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // 窗口大小跟着卡片走：登录/注册表单切换时高度变化，这里不用手工 setFixedSize
    layout->setSizeConstraint(QLayout::SetFixedSize);

    m_page = new LoginPage(this);
    layout->addWidget(m_page, 0, 0);//网格0行0列

    auto *closeButton = new QPushButton(tr("×"));
    closeButton->setObjectName("closeButton");
    closeButton->setFixedSize(24, 24);
    closeButton->setCursor(Qt::PointingHandCursor);//鼠标悬停时，变手形光标

    // 再包一层容器：容器贴右上角，容器内的边距决定 × 离卡片角多远
    auto *closeWrap = new QWidget(this);
    auto *closeWrapLayout = new QVBoxLayout(closeWrap);
    closeWrapLayout->setContentsMargins(0, 14, 14, 0);//左上右下
    closeWrapLayout->addWidget(closeButton);
    layout->addWidget(closeWrap, 0, 0, Qt::AlignTop | Qt::AlignRight);
    connect(closeButton, &QPushButton::clicked, this, &LoginDialog::reject);//触发关闭按钮，结束Dialog，没有正常处理

    // LoginPage 不关心自己是不是在对话框里，这里只做信号转发
    connect(m_page, &LoginPage::loginRequested, this, &LoginDialog::loginRequested);
    connect(m_page, &LoginPage::registerRequested, this, &LoginDialog::registerRequested);
    connect(m_page, &LoginPage::verifyCodeSubmitted, this, &LoginDialog::verifyCodeSubmitted);
    connect(m_page, &LoginPage::resendVerifyRequested, this, &LoginDialog::resendVerifyRequested);

}

void LoginDialog::setBusy(bool busy) { m_page->setBusy(busy); }

void LoginDialog::showError(const QString &message) { m_page->showError(message); }

void LoginDialog::showRegistered(const QString &uid, const QString &email) {
    m_page->showRegistered(uid, email);
}

void LoginDialog::showVerificationRequired(const QString &uid, const QString &email) {
    m_page->showVerificationRequired(uid, email);
}

void LoginDialog::showVerified() { m_page->showVerified(); }

void LoginDialog::showVerifyCodeSent(int seconds) { m_page->showVerifyCodeSent(seconds); }

void LoginDialog::showVerifyCooldown(int seconds) { m_page->showVerifyCooldown(seconds); }

void LoginDialog::resetForm() { m_page->resetForm(); }

void LoginDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {//鼠标左键
        m_dragging = true;
        //计算鼠标与窗口左上角的偏移量
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);//其它情况，交给父类
}

void LoginDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);//其它情况，交给父类
}

void LoginDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);//其它情况，交给父类
}
