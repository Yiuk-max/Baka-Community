#include "AutoLoginDialog.h"

#include "Spinner.h"

#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

AutoLoginDialog::AutoLoginDialog(QWidget *parent) : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角），和 LoginDialog 同一套做法
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("Baka Community"));   // 没有标题栏，只在任务栏显示

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // 窗口大小跟着卡片走

    auto *card = new QWidget(this);
    card->setObjectName("loadingCard");
    card->setFixedSize(440, 280);   // 横向长方形
    root->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(0);
    layout->addStretch();   // 上下各一个弹簧，内容在卡片里垂直居中

    // logo
    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/badge-256.png"))
                        .scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo, 0, Qt::AlignHCenter);

    layout->addSpacing(24);
    layout->addWidget(new Spinner, 0, Qt::AlignHCenter);

    layout->addSpacing(18);
    m_text = new QLabel(tr("登录..."));
    m_text->setObjectName("loadingText");
    // 标签宽度按最长的 "登录..." 定死、文字靠左，点数变化时 "登录" 两个字不会左右跳
    m_text->ensurePolished();   // 先让 QSS 的字号生效，量出来的宽度才对
    m_text->setFixedWidth(m_text->sizeHint().width());
    m_text->setText(tr("登录."));
    layout->addWidget(m_text, 0, Qt::AlignHCenter);
    layout->addStretch();

    m_dotsTimer = new QTimer(this);
    m_dotsTimer->setInterval(400);
    connect(m_dotsTimer, &QTimer::timeout, this, &AutoLoginDialog::advanceDots);
    m_dotsTimer->start();
}

void AutoLoginDialog::fallbackToLogin() { done(NeedLogin); }

void AutoLoginDialog::advanceDots() {
    m_dots = m_dots % 3 + 1;   // 1 → 2 → 3 → 1
    m_text->setText(tr("登录") + QString(m_dots, QLatin1Char('.')));
}
