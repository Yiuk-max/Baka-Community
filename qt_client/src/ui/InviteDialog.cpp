#include "InviteDialog.h"

#include "Assets.h"
#include "Avatar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

InviteDialog::InviteDialog(const QString &title, const QList<Contact> &friends, QWidget *parent)
    : QDialog(parent) {
    // 无标题栏 + 背景透明（卡片自己画圆角），和 NoticeDialog / InputDialog 同一套做法；样式全部复用已有的
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSizeConstraint(QLayout::SetFixedSize);

    auto *card = new QWidget(this);
    card->setObjectName("noticeCard");
    card->setFixedWidth(380);
    root->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(0);

    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("noticeText");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addSpacing(16);

    // 好友列表：多了就在里面滚动
    auto *scroll = new QScrollArea;
    scroll->setObjectName("inviteScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(qBound(48, int(friends.size()) * 48, 288));   // 最多露出 6 行
    auto *list = new QWidget;
    list->setObjectName("inviteList");
    auto *rows = new QVBoxLayout(list);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);

    for (const Contact &contact : friends) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 8, 8, 8);
        row->setSpacing(12);
        auto *avatarLabel = new QLabel;
        avatarLabel->setPixmap(avatar::user(contact.avatarPath, 32));
        row->addWidget(avatarLabel);
        auto *nameLabel = new QLabel(contact.name);
        nameLabel->setObjectName("settingsRowValue");
        nameLabel->setTextFormat(Qt::PlainText);
        nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // 太长就截断
        nameLabel->setToolTip(tr("%1（UID %2）").arg(contact.name).arg(contact.uid));
        row->addWidget(nameLabel, 1);

        auto *inviteButton = new QPushButton(tr("邀请"));
        inviteButton->setObjectName("primaryButton");
        inviteButton->setFixedSize(64, 32);
        inviteButton->setCursor(Qt::PointingHandCursor);
        inviteButton->setAutoDefault(false);
        row->addWidget(inviteButton);
        const int uid = contact.uid;
        // 点过就变灰，防止重复邀请同一个人；成功还是失败由服务端的提示告知
        connect(inviteButton, &QPushButton::clicked, this, [this, inviteButton, uid] {
            inviteButton->setEnabled(false);
            inviteButton->setText(tr("已邀请"));
            emit inviteRequested(uid);
        });
        rows->addLayout(row);
    }
    if (friends.isEmpty()) {
        auto *empty = new QLabel(tr("还没有好友可以邀请"));
        empty->setObjectName("settingsRowName");
        rows->addWidget(empty);
    }
    rows->addStretch();
    scroll->setWidget(list);
    layout->addWidget(scroll);

    layout->addSpacing(20);
    auto *closeButton = new QPushButton(tr("完成"));
    closeButton->setObjectName("settingsRowButton");
    closeButton->setFixedHeight(36);
    closeButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &InviteDialog::accept);
}
