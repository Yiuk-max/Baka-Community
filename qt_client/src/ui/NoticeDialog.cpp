#include "NoticeDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

NoticeDialog::NoticeDialog(const QString &message, QWidget *parent) : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角），和 LoginDialog 同一套做法
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // 窗口大小跟着卡片走

    auto *card = new QWidget(this);
    card->setObjectName("noticeCard");
    card->setFixedWidth(320);
    root->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(0);

    auto *text = new QLabel(message);
    text->setObjectName("noticeText");
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignHCenter);
    layout->addWidget(text);

    layout->addSpacing(24);
    auto *okButton = new QPushButton(tr("我知道了"));
    okButton->setObjectName("primaryButton");   // 和登录按钮同一个样式
    okButton->setFixedHeight(40);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setDefault(true);                 // 回车也能关
    layout->addWidget(okButton);

    connect(okButton, &QPushButton::clicked, this, &NoticeDialog::accept);
}
