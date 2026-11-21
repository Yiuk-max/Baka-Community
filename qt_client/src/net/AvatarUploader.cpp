#include "AvatarUploader.h"

#include "ChatClient.h"
#include "message.pb.h"

#include <QStringList>
#include <QTimer>

namespace {

const char *kAvatarName = "avatar.png";
const char *kAvatarMime = "image/png";

// 服务端上传失败的提示（原文见后端 src/file/file_service.cpp 的 upload_init / upload_chunk）
QString translateUploadError(const QString &text) {
    if (text.contains(QLatin1String("avatar size exceeds max_avatar_size")))
        return AvatarUploader::tr("头像文件太大，请换一张小一点的图片");
    if (text.contains(QLatin1String("file size exceeds max_file_size")))
        return AvatarUploader::tr("文件超出服务器大小限制");
    if (text.contains(QLatin1String("You must be logged in")))
        return AvatarUploader::tr("登录状态已失效，请重新登录");
    return AvatarUploader::tr("头像上传失败：%1").arg(text);
}

} // namespace

AvatarUploader::AvatarUploader(ChatClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(20000);
    connect(m_timeout, &QTimer::timeout, this, [this] { fail(tr("上传超时，请重试")); });

    connect(m_client, &ChatClient::envelopeReceived, this, &AvatarUploader::handleEnvelope);
    // 断线 / 退出登录：正在传的那次直接算失败，不然界面会一直卡在“上传中”
    connect(m_client, &ChatClient::disconnected, this, [this] {
        if (isBusy()) fail(tr("连接已断开，头像上传失败"));
    });
    connect(m_client, &ChatClient::loggedOut, this, [this] {
        if (isBusy()) fail(tr("登录状态已失效，请重新登录"));
    });
}

void AvatarUploader::upload(const QByteArray &png) {
    if (isBusy() || png.isEmpty()) return;

    m_data = png;
    m_state = State::Init;
    m_transferId = 0;
    m_chunkSize = 0;
    m_chunkCount = 0;
    m_timeout->start();

    chat_proto::Envelope env;
    env.set_type("file_upload_init");
    auto *meta = env.mutable_file_meta();
    meta->set_original_name(kAvatarName);
    meta->set_mime_type(kAvatarMime);
    meta->set_size(static_cast<quint64>(m_data.size()));
    meta->set_type("avatar");   // 服务端按这个值走头像大小限制、也决定存储目录
    m_client->send(env);
}

void AvatarUploader::handleEnvelope(const chat_proto::Envelope &envelope) {
    if (m_state == State::Idle) return;
    const QString type = QString::fromStdString(envelope.type());

    if (m_state == State::Init && type == QLatin1String("file_upload_ready")) {
        // transfer_info.transfer_id 在协议里是字符串，file_upload_done 用的是 int32 字段
        m_transferId = QString::fromStdString(envelope.transfer_info().transfer_id()).toInt();
        m_chunkSize = envelope.transfer_info().chunk_size();
        m_chunkCount = envelope.transfer_info().chunk_count();
        if (m_transferId <= 0 || m_chunkSize == 0 || m_chunkCount == 0) {
            fail(tr("服务器返回的上传参数不完整"));
            return;
        }
        m_state = State::Chunks;
        sendChunks();
        return;
    }

    if (m_state == State::Done && type == QLatin1String("file_upload_complete")) {
        const int fileId = QString::fromStdString(envelope.file_meta().file_id()).toInt();
        m_timeout->stop();
        reset();
        if (fileId <= 0) {
            emit failed(tr("服务器没有返回头像文件编号"));
            return;
        }
        emit finished(fileId);
        return;
    }

    // 服务端上传出错时只回一句 system 文本
    if (type == QLatin1String("system")) {
        const QString text = QString::fromStdString(envelope.content()).trimmed();
        static const QStringList kFailures = {
            QStringLiteral("Invalid upload request"), QStringLiteral("Upload rejected"),
            QStringLiteral("Failed to create upload temporary file"),
            QStringLiteral("Failed to create upload transfer record"),
            QStringLiteral("Invalid transfer_id"), QStringLiteral("Unknown upload transfer"),
            QStringLiteral("Upload writer is not available"), QStringLiteral("chunk_index out of range"),
            QStringLiteral("Failed to write file chunk"), QStringLiteral("Upload incomplete"),
            QStringLiteral("Failed to commit uploaded file"), QStringLiteral("sha256 mismatch"),
            QStringLiteral("Failed to save file metadata"), QStringLiteral("You must be logged in"),
        };
        for (const QString &prefix : kFailures) {
            if (text.startsWith(prefix)) {
                fail(translateUploadError(text));
                return;
            }
        }
    }
}

void AvatarUploader::sendChunks() {
    // 服务端不回每片的 ack，所以一次把剩下的分片全丢过去（头像只有一片，通常是 1 次）
    for (quint32 index = 0; index < m_chunkCount; ++index) {
        const int offset = static_cast<int>(index * m_chunkSize);
        const QByteArray bytes = m_data.mid(offset, static_cast<int>(m_chunkSize));

        chat_proto::Envelope env;
        env.set_type("file_chunk");
        auto *meta = env.mutable_meta();
        meta->set_type("file_chunk");
        meta->set_transfer_id(std::to_string(m_transferId));
        meta->set_filename(kAvatarName);
        meta->set_total_size(static_cast<quint64>(m_data.size()));
        meta->set_chunk_index(index);
        meta->set_chunk_count(m_chunkCount);
        meta->set_chunk_size(static_cast<quint32>(bytes.size()));
        meta->set_offset(static_cast<quint64>(offset));
        m_client->send(env, bytes);
    }

    chat_proto::Envelope done;
    done.set_type("file_upload_done");
    done.set_transfer_id(m_transferId);
    m_client->send(done);

    m_state = State::Done;
    m_timeout->start();   // 等 file_upload_complete
}

void AvatarUploader::fail(const QString &message) {
    reset();
    emit failed(message);
}

void AvatarUploader::reset() {
    m_timeout->stop();
    m_state = State::Idle;
    m_data.clear();
    m_transferId = 0;
    m_chunkSize = 0;
    m_chunkCount = 0;
}
