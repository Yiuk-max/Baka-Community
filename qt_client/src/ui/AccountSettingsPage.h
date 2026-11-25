#pragma once

// 设置 - 账户页：头像 / 用户名 / UID / 邮箱。用户名和邮箱可以点“编辑”修改，UID 只读。
// 头像点“上传新头像”→ 选图 → 裁剪成 1:1 正方形 → 发出 PNG，上传和 set_avatar 由上层做。
// 只发信号和显示结果，不碰网络：请求由上层交给 ChatClient，结果再通过 setProfile / showError 回填。
#include <QByteArray>
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
    void setAvatar(const QString &path);                              // 本地头像文件；空 = 默认头像
    void setAvatarUploading(bool busy);                               // 上传中：头像按钮变灰
    void showAvatarDone();                                            // 上传 + set_avatar 都成功
    void showAvatarError(const QString &message);                     // 选图 / 裁剪 / 上传失败

signals:
    void nameChangeRequested(const QString &name);
    void emailChangeRequested(const QString &email);
    void avatarUploadRequested(const QByteArray &png);   // 裁剪好的正方形 PNG

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
    QWidget *makeAvatarRow();
    void chooseAvatar();     // 选图 + 裁剪，确定后发出 avatarUploadRequested
    void setEditing(Row &row, bool editing);
    void showMessage(const QString &text, bool isError);   // 行下面的提示：成功绿色，失败红色
    void submitName();
    void submitEmail();

    Row m_name;
    Row m_uid;
    Row m_email;
    QLabel *m_avatar = nullptr;         // 当前头像预览（圆形）
    QPushButton *m_avatarButton = nullptr;   // “上传新头像”
    QString m_emailValue;      // 真实邮箱；没绑定时为空（界面上显示“未绑定”）
    QLabel *m_message = nullptr;
};
