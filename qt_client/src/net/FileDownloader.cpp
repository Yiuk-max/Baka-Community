#include "FileDownloader.h"

#include "ChatClient.h"

#include <QRegularExpression>
#include <QTimer>

FileDownloader::FileDownloader(ChatClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(20000);
    connect(m_timeout, &QTimer::timeout, this, &FileDownloader::failCurrent);

    connect(m_client, &ChatClient::fileFrameReceived, this, &FileDownloader::handleFrame);
    connect(m_client, &ChatClient::envelopeReceived, this, &FileDownloader::handleEnvelope);
    connect(m_client, &ChatClient::disconnected, this, &FileDownloader::reset);
    connect(m_client, &ChatClient::loggedOut, this, &FileDownloader::reset);
}

void FileDownloader::download(int fileId) {
    if (fileId <= 0 || fileId == m_fileId || m_queue.contains(fileId)) return;
    m_queue.append(fileId);
    if (m_fileId == 0) startNext();
}

void FileDownloader::reset() {
    m_timeout->stop();
    m_queue.clear();
    m_fileId = 0;
    m_transferId = 0;
    m_data.clear();
}

void FileDownloader::startNext() {
    m_fileId = 0;
    m_transferId = 0;
    m_chunkCount = 0;
    m_nextChunk = 0;
    m_data.clear();
    if (m_queue.isEmpty() || !m_client->isConnected()) return;

    m_fileId = m_queue.takeFirst();
    chat_proto::Envelope env;
    env.set_type("file_download_init");
    env.set_file_id(std::to_string(m_fileId));   // 协议里 file_id 是字符串
    m_client->send(env);
    m_timeout->start();
}

void FileDownloader::requestChunk() {
    chat_proto::Envelope env;
    env.set_type("file_download_chunk");
    env.set_transfer_id(m_transferId);
    env.set_chunk_index(static_cast<int>(m_nextChunk));
    m_client->send(env);
    m_timeout->start();
}

void FileDownloader::handleFrame(const chat_proto::Envelope &envelope, const QByteArray &fileData) {
    if (m_fileId == 0) return;

    if (envelope.type() == "file_download_ready") {
        // 回复里带 file_id，用它确认是不是当前这个文件的（不靠“先发先回”的顺序）
        if (m_transferId != 0 || envelope.file_meta().file_id() != std::to_string(m_fileId)) return;
        m_transferId = QString::fromStdString(envelope.transfer_info().transfer_id()).toInt();
        m_chunkCount = envelope.transfer_info().chunk_count();
        if (m_transferId <= 0 || m_chunkCount == 0) {   // 空文件或回复不完整
            failCurrent();
            return;
        }
        m_data.reserve(static_cast<int>(envelope.file_meta().size()));
        requestChunk();
        return;
    }

    // file_chunk：只认当前传输会话里、正好是下一块的那一片
    const chat_proto::FileChunkMeta &meta = envelope.meta();
    if (m_transferId == 0 || meta.transfer_id() != std::to_string(m_transferId)
            || meta.chunk_index() != m_nextChunk) return;
    m_data.append(fileData);
    ++m_nextChunk;
    if (m_nextChunk < m_chunkCount) {
        requestChunk();
        return;
    }

    m_timeout->stop();
    const int fileId = m_fileId;
    const QByteArray data = m_data;
    startNext();
    emit finished(fileId, data);
}

// 下载失败时服务端只回一句 system 文本（原文见后端 src/file/file_service.cpp 的 download_init / download_chunk）
void FileDownloader::handleEnvelope(const chat_proto::Envelope &envelope) {
    if (m_fileId == 0 || envelope.type() != "system") return;
    static const QRegularExpression failRe(QStringLiteral(
        "Invalid file_id|File not found|Failed to create download transfer record|Transfer not found"
        "|Not a download transfer|Transfer is not active|Invalid chunk_index|Failed to open file on disk"
        "|Failed to seek file|Failed to read file chunk|You must be logged in to download files"));
    if (failRe.match(QString::fromStdString(envelope.content())).hasMatch()) failCurrent();
}

void FileDownloader::failCurrent() {
    m_timeout->stop();
    const int fileId = m_fileId;
    startNext();
    if (fileId != 0) emit failed(fileId);
}
