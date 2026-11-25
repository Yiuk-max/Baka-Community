#pragma once

// 头像上传：把裁剪好的正方形 PNG 按分片协议传上去，成功后给出 file.id。
//   file_upload_init{file_meta{original_name, mime_type, size, type="avatar"}}
//   → file_upload_ready{transfer_info}          （拿 transfer_id / chunk_size / chunk_count）
//   → file_chunk{meta + 帧尾原始数据} × N
//   → file_upload_done{transfer_id} → file_upload_complete{file_meta.file_id}
// 上传完还要再发一次 set_avatar 才算换头像，那一步在 ChatClient::setAvatar()。
// 一次只传一个：头像 PNG 一般只有几百 KB（服务端分片 4 MiB），基本一片就完事。
#include <QByteArray>
#include <QObject>

class ChatClient;
class QTimer;

namespace chat_proto {
class Envelope;
}

class AvatarUploader : public QObject {
    Q_OBJECT
public:
    explicit AvatarUploader(ChatClient *client, QObject *parent = nullptr);

    void upload(const QByteArray &png);   // 正在传的时候再调会被忽略
    void uploadImage(const QByteArray &png, bool banner); // 社区背景按 image 类型上传
    bool isBusy() const { return m_state != State::Idle; }

signals:
    void finished(int fileId);
    void failed(const QString &message);

private:
    enum class State { Idle, Init, Chunks, Done };

    void handleEnvelope(const chat_proto::Envelope &envelope);
    void sendChunks();
    void fail(const QString &message);
    void reset();

    ChatClient *m_client = nullptr;
    QTimer *m_timeout = nullptr;     // 每一步等回复的超时，防止界面一直卡在“上传中”
    State m_state = State::Idle;
    QByteArray m_data;               // 待上传的 PNG
    bool m_banner = false;
    int m_transferId = 0;
    quint32 m_chunkSize = 0;
    quint32 m_chunkCount = 0;
};
