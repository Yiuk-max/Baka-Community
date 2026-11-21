#pragma once

// 文件下载：按 file.id 把服务端的文件整个拉到内存里。
//   file_download_init{file_id} → file_download_ready{file_meta, transfer_info}
//   → 从 0 开始逐块 file_download_chunk{transfer_id, chunk_index} → file_chunk + 帧尾文件数据
// 一次只下一个，其余排队：服务端单连接待发送队列上限 8 MiB，分片 4 MiB，并发会被断开（web 端同样限制为 1）。
#include <QByteArray>
#include <QList>
#include <QObject>

namespace chat_proto {
class Envelope;
}
class ChatClient;
class QTimer;

class FileDownloader : public QObject {
    Q_OBJECT
public:
    explicit FileDownloader(ChatClient *client, QObject *parent = nullptr);

    void download(int fileId);   // 排队下载；已经在排队或正在下的不会重复加
    void reset();                // 退出登录 / 断线：丢掉正在下的和排队的

signals:
    void finished(int fileId, const QByteArray &data);
    void failed(int fileId);

private:
    void startNext();
    void handleFrame(const chat_proto::Envelope &envelope, const QByteArray &fileData);
    void handleEnvelope(const chat_proto::Envelope &envelope);   // 只看下载失败的 system 文本
    void requestChunk();
    void failCurrent();

    ChatClient *m_client = nullptr;
    QTimer *m_timeout = nullptr;     // 每一步等回复的超时，防止一个文件卡住后面全部
    QList<int> m_queue;              // 排队中的 file.id
    int m_fileId = 0;                // 正在下的 file.id；0 = 空闲
    int m_transferId = 0;            // 服务端给的传输会话 id；0 = 还没收到 ready
    quint32 m_chunkCount = 0;
    quint32 m_nextChunk = 0;
    QByteArray m_data;               // 已收到的分片按顺序拼在这里
};
