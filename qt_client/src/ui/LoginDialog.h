#pragma once

// 登录 / 注册窗口：无系统标题栏的独立子窗口（对应 web 端的登录浮层）。
// 窗口本身就是那张卡片，没有标题栏，所以自己提供：× 关闭按钮 + 按住空白处拖动。
#include <QDialog>
#include <QPoint>

class LoginPage;
class QMouseEvent;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

    // 网络结果回填：原样转给 LoginPage
    void setBusy(bool busy);
    void showError(const QString &message);
    void showRegistered(const QString &uid, const QString &email);

signals:
    void loginRequested(const QString &identity, const QString &password);
    void registerRequested(const QString &nickname, const QString &email, const QString &password);

protected:
    // 无边框窗口得自己处理拖动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    LoginPage *m_page = nullptr;
    QPoint m_dragOffset;    //拖拽游标坐标
    bool m_dragging = false;
};
