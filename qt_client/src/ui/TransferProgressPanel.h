#pragma once
#include "model/DownloadTask.h"
#include <QHash>
#include <QWidget>
class QLabel;
class QProgressBar;
class QVBoxLayout;
class QScrollArea;
class MediaStore;
class FileDownloader;

// 覆盖在主窗口内容区右下角，只占卡片区域，不抢焦点、不阻塞聊天。
class TransferProgressPanel : public QWidget {
    Q_OBJECT
public:
    explicit TransferProgressPanel(MediaStore *store, QWidget *parent = nullptr);
    void clear();
    int cardCount() const { return m_cards.size(); }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    struct Card {
        QWidget *widget;
        QLabel *icon;
        QLabel *name;
        QLabel *detail;
        QProgressBar *bar;
        DownloadTask task;
    };
    void updateTask(const DownloadTask &task);
    void removeCard(const QString &id);
    void resizePanel();
    QHash<QString, Card> m_cards;
    FileDownloader *m_downloader;
    QVBoxLayout *m_list;
    QScrollArea *m_scroll;
};
