#pragma once

// 文件下载的唯一调度入口（头像 / 社区图片 / 聊天附件都走这里，避免几个下载器抢同一条连接）：
//   file_download_init{file_id} → file_download_ready{file_meta, transfer_info}
//   → 从断点起逐块 file_download_chunk{transfer_id, chunk_index} → file_chunk + 帧尾文件数据
// 一次只发一个分片请求：服务端单连接待发送队列上限 8 MiB、分片 4 MiB，并发会被断开
//（web 端同样把预览并发限制为 1）。附件分片交给 DownloadWriter 写磁盘，写完才请求下一块，
// 内存里不再攒整个文件。
#include "model/DownloadTask.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

namespace chat_proto {
class Envelope;
}
class ChatClient;
class DownloadWriter;
class QTimer;

class FileDownloader : public QObject {
    Q_OBJECT
public:
    explicit FileDownloader(ChatClient *client, QObject *parent = nullptr);
    ~FileDownloader() override;

    // 静默小文件（头像 / 社区图片）：整份读进内存，完成后发 finished(fileId, data)。
    // 超过内存上限的文件会直接失败，调用方应改用 enqueue() 落盘。
    void download(int fileId);
    // 附件下载：targetPath 为空时落到系统下载目录（重名自动加序号），逐片写磁盘。
    // 同一个 fileId 的同类任务、或同一个 taskId 已在队列里时不重复入队。
    bool enqueue(const DownloadTask &task, QString *error = nullptr);
    bool promote(const QString &taskId); // 自动缓存被用户点击：转为可见的高优先级任务
    void cancel(const QString &taskId);      // 停止并删掉本任务的 .part
    bool pause(const QString &taskId);       // 保留已完成分片，恢复时从断点续传
    bool resume(const QString &taskId);      // 重新入队（连接已恢复时）
    QList<DownloadTask> tasks() const;       // 排队 + 正在下 + 暂停/中断的任务快照
    bool findTask(const QString &taskId, DownloadTask *out) const;
    // 登录后调用：把上次没下完的附件任务读回来（状态“已中断”，不自动联网）
    int restorePending(int uid);
    void reset();                            // 断线 / 退出登录：保留临时文件，内存任务丢弃
    void resumeNetworkInterrupted();         // 同账号重连认证成功后：自动续传本次断网中断的磁盘任务

signals:
    void finished(int fileId, const QByteArray &data);   // 静默小文件下好了（头像 / 社区图片）
    void failed(int fileId);

    void taskUpdated(const DownloadTask &task); // 含终态的完整快照，供附件和进度卡订阅
    void taskStateChanged(const QString &taskId, DownloadTask::State state, const QString &message);
    void taskProgress(const QString &taskId, qint64 transferred, qint64 totalSize);
    void taskFinished(const QString &taskId, const QString &localPath);  // 重名时可能和请求的不同
    void taskFailed(const QString &taskId, const QString &message);

private:
    struct Active {
        DownloadTask task;
        QByteArray data;      // targetPath 为空时：整份文件放这里
        bool memory = false;
        int retries = 0;      // 分片超时重试次数（有限重试后转“已中断”）
    };

    void startNext();
    void insertQueued(const DownloadTask &task);
    void handleFrame(const chat_proto::Envelope &envelope, const QByteArray &fileData);
    void handleEnvelope(const chat_proto::Envelope &envelope);   // 只看下载失败的 system 文本
    void requestChunk();
    void finishActive();
    void failActive(const QString &message);
    void stopActive(DownloadTask::State state, const QString &message);
    void emitState(const DownloadTask &task, const QString &message);
    void holdTask(const DownloadTask &task);   // 暂停 / 中断的任务收进 m_held（按 taskId 去重）
    bool hasSameFile(const DownloadTask &task) const;
    static QString newTaskId();

    ChatClient *m_client = nullptr;
    DownloadWriter *m_writer = nullptr;
    QTimer *m_timeout = nullptr;      // 每一步等回复的超时，防止一个文件卡住后面全部
    QList<DownloadTask> m_queue;      // 排队中（urgent 的排前面）
    QList<DownloadTask> m_held;       // 暂停 / 已中断，等 resume()
    Active m_active;
    bool m_hasActive = false;
};
