#pragma once

// 主窗口最左侧的竖向工具栏
// 目前只有顶部的程序 logo 按钮；社区图标、添加、设置等按钮以后往下加
#include <QWidget>

class QPushButton;
class QVBoxLayout;

class ToolRail : public QWidget {
    Q_OBJECT
public:
    explicit ToolRail(QWidget *parent = nullptr);

signals:
    void homeClicked();          // 单击 logo：回到私聊 / 群聊页
    void logoDoubleClicked();    // 双击 logo：播放 Baka 音效
    void logoutClicked();        // 底部的退出登录按钮

protected:
    // QPushButton 没有双击信号，用事件过滤器接
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QVBoxLayout *m_layout = nullptr;
    QPushButton *m_logoButton = nullptr;
    QPushButton *m_logoutButton = nullptr;
};
