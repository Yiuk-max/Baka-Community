#include "InputDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

InputDialog::InputDialog(const QString &title, const QStringList &fieldLabels, QWidget *parent)
    : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角），和 NoticeDialog 同一套做法
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);   // 窗口大小跟着卡片走

    auto *card = new QWidget(this);
    card->setObjectName("noticeCard");
    card->setFixedWidth(360);
    root->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(0);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("noticeText");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    for (const QString &label : fieldLabels) {
        layout->addSpacing(16);
        auto *fieldLabel = new QLabel(label);
        fieldLabel->setObjectName("fieldLabel");
        layout->addWidget(fieldLabel);
        layout->addSpacing(6);
        auto *input = new QLineEdit;
        input->setObjectName("settingsInput");
        input->setFixedHeight(36);
        layout->addWidget(input);
        m_inputs.append(input);
    }

    layout->addSpacing(24);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch();
    auto *cancelButton = new QPushButton(tr("取消"));
    cancelButton->setObjectName("settingsRowButton");
    cancelButton->setFixedSize(80, 36);
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setAutoDefault(false);
    buttons->addWidget(cancelButton);
    auto *okButton = new QPushButton(tr("确定"));
    okButton->setObjectName("primaryButton");
    okButton->setFixedSize(80, 36);
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setDefault(true);   // 回车 = 确定
    buttons->addWidget(okButton);
    layout->addLayout(buttons);

    connect(cancelButton, &QPushButton::clicked, this, &InputDialog::reject);
    connect(okButton, &QPushButton::clicked, this, [this] {
        if (!m_inputs.isEmpty() && value(0).isEmpty()) {
            m_inputs.first()->setFocus();
            return;
        }
        accept();
    });
    if (!m_inputs.isEmpty()) m_inputs.first()->setFocus();
}

QString InputDialog::value(int index) const {
    return m_inputs.value(index) ? m_inputs.at(index)->text().trimmed() : QString();
}
