#pragma once

// 设置 - 账户页：用户名 / UID / 邮箱。用户名和邮箱可以点“编辑”修改，UID 只读。
// 只发信号和显示结果，不碰网络：请求由上层交给 ChatClient，结果再通过 setProfile / showError 回填。
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class AccountSettingsPage : public QWidget {
    Q_OBJECT
public:
    AccountSettingsPage(const QString &username, int uid, const QString &email,
                        QWidget *parent = nullptr);

    void setProfile(const QString &username, const QString &email);   // 修改成功：显示新值，收起编辑框
    void showError(const QString &message);                           // 修改失败：显示原因，保留编辑框

signals:
    void nameChangeRequested(const QString &name);
    void emailChangeRequested(const QString &email);

private:
    // 一行里的控件：平时显示 value + “编辑”，编辑时换成 edit + “保存” + “取消”
    struct Row {
        QLabel *value = nullptr;
        QLineEdit *edit = nullptr;
        QPushButton *editButton = nullptr;
        QPushButton *saveButton = nullptr;
        QPushButton *cancelButton = nullptr;
    };

    QWidget *makeRow(const QString &name, Row &row, bool editable);
    void setEditing(Row &row, bool editing);
    void showMessage(const QString &text, bool isError);   // 行下面的提示：成功绿色，失败红色
    void submitName();
    void submitEmail();

    Row m_name;
    Row m_uid;
    Row m_email;
    QString m_emailValue;      // 真实邮箱；没绑定时为空（界面上显示“未绑定”）
    QLabel *m_message = nullptr;
};
