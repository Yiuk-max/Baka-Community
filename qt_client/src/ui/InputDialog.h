#pragma once

// 小输入弹窗：一句标题 + 若干个输入框 + 取消 / 确定。没有输入框时就是一个确认框。
// 外观和 NoticeDialog 同一套（无标题栏的圆角卡片），样式全部复用已有的 QSS。
#include <QDialog>
#include <QList>
#include <QStringList>

class QLineEdit;

class InputDialog : public QDialog {
    Q_OBJECT
public:
    // fieldLabels：每个输入框上方的说明文字。第一个输入框必填，空着点确定不会关
    InputDialog(const QString &title, const QStringList &fieldLabels, QWidget *parent = nullptr);

    QString value(int index) const;     // 第 index 个输入框的内容（已去掉首尾空白）

private:
    QList<QLineEdit *> m_inputs;
};
