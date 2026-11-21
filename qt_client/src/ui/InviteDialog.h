#pragma once

// 邀请好友弹窗：列出好友，每行一个“邀请”按钮，点一下发一次邀请（可以连着邀请多个人）。
// 群聊邀请和社区邀请共用；弹窗只发信号，请求由 MainWindow 发。
#include "model/Contact.h"

#include <QDialog>
#include <QList>

class InviteDialog : public QDialog {
    Q_OBJECT
public:
    // friends：只传好友（不含群）
    InviteDialog(const QString &title, const QList<Contact> &friends, QWidget *parent = nullptr);

signals:
    void inviteRequested(int uid);
};
