#pragma once

// 设置窗口（对应 web 端 .settings-window）：左边是分类导航，右边是当前分类的页面。
// 无系统标题栏，右上角 × 关闭；点“登出”时 exec() 返回 Logout，由调用方去退出登录。
#include <QDialog>
#include <QPoint>

class AccountSettingsPage;
class QLabel;
class QMouseEvent;
class QStackedWidget;
class QVBoxLayout;
class SoundPlayer;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Result { Logout = QDialog::Accepted + 1 };

    SettingsDialog(const QString &username, int uid, const QString &email, SoundPlayer *sound,
                   QWidget *parent = nullptr);

    // 账户页的修改结果，由调用方（MainWindow）从 ChatClient 转过来
    void setProfile(const QString &username, const QString &email);
    void showProfileError(const QString &message);

signals:
    // 账户页里点了“保存”，原样转给调用方
    void nameChangeRequested(const QString &name);
    void emailChangeRequested(const QString &email);

protected:
    // 无边框窗口得自己处理拖动（和 LoginDialog 同一套做法）：按住空白处拖
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 加一个分类：导航栏里一个按钮 + 右边一个页面
    void addPage(QVBoxLayout *navLayout, const QString &name, QWidget *page);

    QLabel *m_title = nullptr;           // 右边顶部：当前分类的名字
    QStackedWidget *m_pages = nullptr;
    AccountSettingsPage *m_accountPage = nullptr;
    QPoint m_dragOffset;       // 按下时鼠标相对窗口左上角的偏移
    bool m_dragging = false;
};
