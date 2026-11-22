#pragma once
#include "model/Message.h"
#include <QWidget>

class MediaStore;
class QLabel;
class QToolButton;

// 一条附件的显示与交互，下载由共享 MediaStore 负责。
class AttachmentWidget : public QWidget {
    Q_OBJECT
public:
    AttachmentWidget(const Message &message, MediaStore *store, QWidget *parent = nullptr);
    void setNearViewport(bool near); // ChatPage 的可见区检查调用
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void refresh();
    void activate();
    void decode(const QString &path);
    Message m_message;
    MediaStore *m_store;
    QLabel *m_picture = nullptr;
    QToolButton *m_download = nullptr;
    bool m_near = false;
    bool m_decoding = false;
    int m_decodeEpoch = 0;
    QString m_loadedPath;
};
