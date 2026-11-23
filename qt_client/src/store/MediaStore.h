#pragma once

#include "model/Message.h"
#include "model/DownloadTask.h"
#include <QHash>
#include <QObject>

class ChatClient;
class FileDownloader;

// 主窗口拥有的共享附件服务：媒体缓存、下载去重、账号隔离；不持有消息控件。
class MediaStore : public QObject {
    Q_OBJECT
public:
    MediaStore(ChatClient *client, FileDownloader *downloader, QObject *parent = nullptr);
    int uid() const { return m_uid; }
    QString localPath(const Message &message) const;
    QString status(int fileId) const;
    void ensure(const Message &message, bool manual = false);
    void retry(const Message &message);
    FileDownloader *downloader() const { return m_downloader; }

signals:
    void mediaChanged(int fileId);
    void taskChanged(const DownloadTask &task);
    void errorOccurred(const QString &text);

private:
    void setUser(int uid);
    void update(const DownloadTask &task);
    ChatClient *m_client;
    FileDownloader *m_downloader;
    int m_uid = 0;
    qint64 m_smallVideoLimit = 20 * 1024 * 1024;
    QHash<int, QString> m_paths;
    QHash<QString, DownloadTask> m_tasks;
    QHash<int, QString> m_errors;
public:
    QString error(int fileId) const { return m_errors.value(fileId); }
};
