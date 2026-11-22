#include "FileDownloader.h"

#include "ChatClient.h"
#include "DownloadJournal.h"
#include "store/DownloadWriter.h"
#include "util/DownloadPaths.h"
#include <QFileInfo>
#include "message.pb.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimer>

namespace {

const int kStepTimeoutMs = 20000;                  // 单步（init / 一个分片）等回复的超时
const int kChunkRetries = 2;                       // 分片超时重试次数，超过就转“已中断”
// 头像 / 社区图片这类静默任务调用方要的是整份 QByteArray；32 MiB 只是防误用上限，
// 超过就明确失败，改走 enqueue() 落盘（聊天附件走的就是这条路）。
const qint64 kMemoryTaskLimit = 32 * 1024 * 1024;

// 服务端 file_meta.hash 是 64 位十六进制；格式不对就当没有，不做假校验
QByteArray normalizedHash(const QString &hash) {
    static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]{64}$"));
    const QString trimmed = hash.trimmed();
    return hex.match(trimmed).hasMatch() ? trimmed.toLower().toLatin1() : QByteArray();
}

// 断点文件的路径：只保留 taskId 里的安全字符，防止路径拼接出问题
} // namespace

FileDownloader::FileDownloader(ChatClient *client, QObject *parent)
    : QObject(parent), m_client(client) {
    m_writer = new DownloadWriter(this);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kStepTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (!m_hasActive) return;
        if (m_active.task.state == DownloadTask::State::Initializing) {
            failActive(tr("服务器响应超时，请重试"));
            return;
        }
        if (m_active.task.state != DownloadTask::State::Downloading) return;
        if (m_active.retries >= kChunkRetries) {
            stopActive(DownloadTask::State::Interrupted, tr("分片请求超时"));
            return;
        }
        ++m_active.retries;   // 断线前的一次超时不代表文件坏了，重试同一片
        requestChunk();
    });

    connect(m_client, &ChatClient::fileFrameReceived, this, &FileDownloader::handleFrame);
    connect(m_client, &ChatClient::envelopeReceived, this, &FileDownloader::handleEnvelope);
    connect(m_client, &ChatClient::disconnected, this, &FileDownloader::reset);
    connect(m_client, &ChatClient::loggedOut, this, &FileDownloader::reset);
    // 断线期间入队的任务：等 ChatClient 自己连回来再继续，这里不主动重连
    connect(m_client, &ChatClient::loginSucceeded, this, [this] { startNext(); });
}

FileDownloader::~FileDownloader() = default;

void FileDownloader::download(int fileId) {
    if (fileId <= 0) return;
    DownloadTask task;
    task.taskId = newTaskId();
    task.fileId = fileId;
    task.silent = true;   // 头像 / 社区图片：不出进度卡，整份进内存
    enqueue(task);
}

void FileDownloader::startNext() {
    if (m_hasActive || m_queue.isEmpty() || !m_client->isConnected() || m_client->uid() <= 0) return;
    while (!m_queue.isEmpty() && m_queue.first().ownerUid != m_client->uid())
        holdTask(m_queue.takeFirst());
    if (m_queue.isEmpty()) return;

    m_active = Active{};
    m_active.task = m_queue.takeFirst();
    m_active.memory = m_active.task.targetPath.isEmpty();
    m_hasActive = true;

    DownloadTask &task = m_active.task;
    task.state = DownloadTask::State::Initializing;
    task.error.clear();
    emitState(task, {});
    m_timeout->start();

    chat_proto::Envelope envelope;
    envelope.set_type("file_download_init");
    envelope.set_file_id(std::to_string(task.fileId));
    m_client->send(envelope);
}

void FileDownloader::requestChunk() {
    if (!m_hasActive) return;
    DownloadTask &task = m_active.task;
    if (task.nextChunk >= task.chunkCount) return;

    chat_proto::Envelope envelope;
    envelope.set_type("file_download_chunk");
    envelope.set_transfer_id(task.transferId);
    envelope.set_chunk_index(static_cast<int>(task.nextChunk));
    m_timeout->start();
    m_client->send(envelope);
}

void FileDownloader::handleFrame(const chat_proto::Envelope &envelope, const QByteArray &fileData) {
    if (!m_hasActive) return;
    DownloadTask &task = m_active.task;

    if (envelope.type() == "file_download_ready") {
        if (task.state != DownloadTask::State::Initializing) return;
        // 回复里带 file_id，用它确认是不是当前这个文件（不只靠“先发先回”的顺序）
        if (envelope.file_meta().file_id() != std::to_string(task.fileId)) return;

        const quint32 previousChunkSize = task.chunkSize;
        const qint64 previousSize = task.totalSize;
        const QString previousHash = task.hash;
        task.transferId = QString::fromStdString(envelope.transfer_info().transfer_id()).toInt();
        task.chunkSize = envelope.transfer_info().chunk_size();
        task.chunkCount = envelope.transfer_info().chunk_count();
        task.totalSize = static_cast<qint64>(envelope.transfer_info().total_size());
        if (task.totalSize <= 0) task.totalSize = static_cast<qint64>(envelope.file_meta().size());
        if (task.name.isEmpty())
            task.name = QString::fromStdString(envelope.file_meta().original_name());
        task.mime = QString::fromStdString(envelope.file_meta().mime_type());
        task.hash = QString::fromStdString(envelope.file_meta().hash());
        if (task.transferred > 0 && ((previousChunkSize && previousChunkSize != task.chunkSize)
                || (previousSize > 0 && previousSize != task.totalSize)
                || (!previousHash.isEmpty() && previousHash != task.hash))) {
            failActive(tr("文件元数据已变化，请重新下载"));
            return;
        }
        if (task.transferId <= 0 || task.chunkSize == 0 || task.chunkSize > 4 * 1024 * 1024
                || task.totalSize < 0 || task.chunkCount != (quint64(task.totalSize) + task.chunkSize - 1) / task.chunkSize) {
            failActive(tr("服务器返回的下载参数不完整"));
            return;
        }

        if (m_active.memory) {
            if (task.totalSize > kMemoryTaskLimit) {
                failActive(tr("文件超过内存下载上限，请改用磁盘下载"));
                return;
            }
            m_active.data.clear();
            m_active.data.reserve(static_cast<int>(task.totalSize));
        } else {
            quint32 resume = 0;
            QString error;
            if (!m_writer->begin(task.taskId, task.partPath, task.chunkSize,
                                 normalizedHash(task.hash), &resume, &error)) {
                failActive(error);
                return;
            }
            task.nextChunk = qMin(resume, task.chunkCount);
            task.transferred = qMin<qint64>(
                task.totalSize, static_cast<qint64>(task.nextChunk) * task.chunkSize);
            emit taskProgress(task.taskId, task.transferred, task.totalSize);
        }

        task.state = DownloadTask::State::Downloading;
        emitState(task, {});
        if (task.nextChunk >= task.chunkCount) {   // 空文件或断点已完整：直接校验提交
            finishActive();
            return;
        }
        requestChunk();
        return;
    }

    if (envelope.type() != "file_chunk") return;
    const chat_proto::FileChunkMeta &meta = envelope.meta();
    if (task.state != DownloadTask::State::Downloading) return;   // 取消 / 暂停后的迟到片直接丢
    if (task.transferId == 0 || meta.transfer_id() != std::to_string(task.transferId)) return;
    if (meta.chunk_index() != task.nextChunk) return;             // 拉取式下载，只认下一块
    const qint64 offset = qint64(task.nextChunk) * task.chunkSize;
    const qint64 expected = qMin<qint64>(task.chunkSize, task.totalSize - offset);
    if (expected <= 0 || meta.offset() != quint64(offset) || meta.chunk_size() != fileData.size()
            || fileData.size() != expected || meta.file_id() != std::to_string(task.fileId)) {
        failActive(tr("收到无效文件分片"));
        return;
    }

    if (m_active.memory) {
        m_active.data.append(fileData);
    } else {
        QString error;
        if (!m_writer->writeChunk(task.taskId, task.nextChunk, fileData, &error)) {
            failActive(error);
            return;
        }
    }
    ++task.nextChunk;
    task.transferred += fileData.size();
    m_active.retries = 0;
    emit taskProgress(task.taskId, task.transferred, task.totalSize);
    emit taskUpdated(task);
    if (!m_active.memory) downloadjournal::save(task);   // 每片落盘后更新断点

    if (task.nextChunk >= task.chunkCount) {
        finishActive();
        return;
    }
    requestChunk();
}

void FileDownloader::finishActive() {
    if (!m_hasActive) return;

    DownloadTask task = m_active.task;
    m_timeout->stop();
    task.state = DownloadTask::State::Verifying;
    m_active.task = task;
    emitState(task, {});

    if (m_active.memory) {
        const QByteArray data = m_active.data;
        m_active = Active{};
        m_hasActive = false;

        task.state = DownloadTask::State::Completed;
        task.transferred = data.size();
        emit taskProgress(task.taskId, task.transferred, task.totalSize);
        emitState(task, {});
        startNext();
        emit finished(task.fileId, data);
        return;
    }

    QString error;
    const QFileInfo targetInfo(task.targetPath);
    const QString target = downloadpaths::uniqueFilePath(targetInfo.absolutePath(), targetInfo.fileName());
    if (!m_writer->commit(task.taskId, target, task.totalSize, task.hash, &error)) {
        failActive(error);
        return;
    }
    downloadjournal::remove(task.ownerUid, task.taskId);

    task.state = DownloadTask::State::Completed;
    task.transferred = task.totalSize;
    task.localPath = target;
    const QString taskId = task.taskId;
    m_active = Active{};
    m_hasActive = false;

    emit taskProgress(taskId, task.totalSize, task.totalSize);
    emitState(task, {});
    startNext();
    emit taskFinished(taskId, target);
}

void FileDownloader::failActive(const QString &message) {
    if (!m_hasActive) return;

    DownloadTask task = m_active.task;
    const bool memory = m_active.memory;
    m_timeout->stop();
    if (!memory) {
        m_writer->discard(task.taskId);   // 失败不留半成品
        downloadjournal::remove(task.ownerUid, task.taskId);
    }
    m_active = Active{};
    m_hasActive = false;

    task.state = DownloadTask::State::Failed;
    task.error = message;
    emitState(task, message);
    startNext();
    emit taskFailed(task.taskId, message);
    if (memory) emit failed(task.fileId);
}

// 暂停 / 中断 / 取消共用：先把当前任务从 active 摘下来，再决定临时文件留还是删
void FileDownloader::stopActive(DownloadTask::State state, const QString &message) {
    if (!m_hasActive) return;

    DownloadTask task = m_active.task;
    const bool memory = m_active.memory;
    m_timeout->stop();

    task.state = state;
    task.error = message;
    if (!memory) {
        if (state == DownloadTask::State::Paused || state == DownloadTask::State::Interrupted) {
            m_writer->closeKeep(task.taskId);
            downloadjournal::save(task);
            holdTask(task);
        } else {
            m_writer->discard(task.taskId);
            downloadjournal::remove(task.ownerUid, task.taskId);
        }
    }
    task.state = state;
    task.error = message;

    // 已经拿到服务端传输会话的，告诉服务端别再按“进行中”算了
    if (task.transferId > 0) {
        chat_proto::Envelope envelope;
        envelope.set_transfer_id(task.transferId);
        if (state == DownloadTask::State::Paused) {
            envelope.set_type("file_transfer_pause");
            m_client->send(envelope);
        } else if (state == DownloadTask::State::Canceled) {
            envelope.set_type("file_transfer_cancel");
            m_client->send(envelope);
        }
    }

    m_active = Active{};
    m_hasActive = false;
    emitState(task, message);
    startNext();
}

void FileDownloader::emitState(const DownloadTask &task, const QString &message) {
    emit taskUpdated(task);
    emit taskStateChanged(task.taskId, task.state, message);
}

// 下载失败时服务端只回一句 system 文本（原文见后端 src/file/file_service.cpp 的 download_init / download_chunk）
void FileDownloader::handleEnvelope(const chat_proto::Envelope &envelope) {
    if (!m_hasActive || envelope.type() != "system") return;
    static const QRegularExpression failRe(QStringLiteral(
        "Invalid file_id|File not found|Failed to create download transfer record|Transfer not found"
        "|Not a download transfer|Transfer is not active|Invalid chunk_index|Failed to open file on disk"
        "|Failed to seek file|Failed to read file chunk|You must be logged in to download files"));
    const QString text = QString::fromStdString(envelope.content()).trimmed();
    if (failRe.match(text).hasMatch()) failActive(text);
}

QString FileDownloader::newTaskId() {
    static int sequence = 0;
    return QStringLiteral("dl-%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(++sequence);
}
