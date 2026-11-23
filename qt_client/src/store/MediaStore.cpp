#include "MediaStore.h"
#include "AppConfig.h"
#include "net/ChatClient.h"
#include "net/FileDownloader.h"
#include "util/DownloadPaths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace {
bool image(const QString &name) {
    const QString ext = QFileInfo(name).suffix().toLower();
    return QStringList{"png", "jpg", "jpeg", "gif", "webp", "bmp"}.contains(ext);
}
bool video(const QString &name) {
    const QString ext = QFileInfo(name).suffix().toLower();
    return QStringList{"mp4", "mkv", "mov", "avi", "webm", "flv", "wmv", "m4v", "ogv"}.contains(ext);
}
}

MediaStore::MediaStore(ChatClient *client, FileDownloader *downloader, QObject *parent)
    : QObject(parent), m_client(client), m_downloader(downloader) {
    QFile config(appconfig::filePath());
    if (config.open(QIODevice::ReadOnly)) {
        const auto cache = QJsonDocument::fromJson(config.readAll()).object()
                               .value(QStringLiteral("media_cache")).toObject();
        const qint64 limit = cache.value(QStringLiteral("small_video_max_bytes")).toInteger(m_smallVideoLimit);
        if (limit >= 0) m_smallVideoLimit = limit;
    }
    connect(client, &ChatClient::loginSucceeded, this, [this] { setUser(m_client->uid()); });
    connect(client, &ChatClient::loggedOut, this, [this] { setUser(0); });
    connect(downloader, &FileDownloader::taskUpdated, this, &MediaStore::update);
}

void MediaStore::setUser(int uid) {
    m_uid = uid;
    m_paths.clear();
    m_tasks.clear();
    m_errors.clear();
    if (uid > 0) {
        m_downloader->restorePending(uid);
        for (const auto &task : m_downloader->tasks()) update(task);
    }
}

QString MediaStore::localPath(const Message &message) const {
    if (m_uid <= 0) return {};
    const auto valid = [&message](const QString &path) {
        const QFileInfo info(path);
        return info.isFile() && !info.isSymLink()
            && (!message.fileSize || quint64(info.size()) == message.fileSize);
    };
    const QString remembered = m_paths.value(message.fileId);
    if (!remembered.isEmpty() && valid(remembered)) return remembered;
    if (image(message.fileName) || video(message.fileName)) {
        const QDir dir(image(message.fileName) ? downloadpaths::imagesDir(m_uid)
                                              : downloadpaths::videosDir(m_uid));
        const QString id = QString::number(message.fileId);
        // 包括重名递增的缓存；只接受本 fileId 的名字，不误用缩略图。
        for (const QString &name : dir.entryList(QDir::Files, QDir::Time)) {
            if (!(name.startsWith(id + QLatin1Char('.'))
                    || name.startsWith(id + QStringLiteral(" (")))) continue;
            const QString path = dir.filePath(name);
            if (valid(path)) return path;
        }
    }
    return {};
}

QString MediaStore::status(int fileId) const {
    for (const auto &task : m_tasks) {
        if (task.fileId == fileId)
            return tr("%1 · %2%").arg(task.stateText()).arg(task.progressPercent());
    }
    return {};
}

void MediaStore::update(const DownloadTask &task) {
    if (task.ownerUid != m_uid || task.targetPath.isEmpty()) return;
    if (task.isTerminal()) m_tasks.remove(task.taskId);
    else m_tasks.insert(task.taskId, task);
    if (task.state == DownloadTask::State::Completed) {
        m_paths.insert(task.fileId, task.localPath);
        m_errors.remove(task.fileId);
    } else if (task.state == DownloadTask::State::Failed || task.state == DownloadTask::State::Canceled
               || task.state == DownloadTask::State::Interrupted) {
        m_errors.insert(task.fileId, task.error.isEmpty() ? task.stateText() : task.error);
    }
    emit taskChanged(task);
    emit mediaChanged(task.fileId);
}

void MediaStore::retry(const Message &message) {
    m_errors.remove(message.fileId);
    ensure(message, true);
}

void MediaStore::ensure(const Message &message, bool manual) {
    if (m_uid <= 0 || message.fileId <= 0) return;
    if (!localPath(message).isEmpty()) { emit mediaChanged(message.fileId); return; }
    if (!manual && m_errors.contains(message.fileId)) return;
    const bool isImage = image(message.fileName);
    const bool isVideo = video(message.fileName);
    if (!manual && !isImage && !(isVideo && message.fileSize > 0
                                && message.fileSize <= quint64(m_smallVideoLimit))) return;
    if (!m_client->isAuthenticated() || m_client->uid() != m_uid) {
        if (manual) emit errorOccurred(tr("连接已断开，请稍后重试"));
        return;
    }
    for (const auto &task : m_downloader->tasks()) {
        if (task.ownerUid != m_uid || task.fileId != message.fileId || task.targetPath.isEmpty()) continue;
        if (manual) {
            m_errors.remove(message.fileId);
            m_downloader->promote(task.taskId);
            if (task.state == DownloadTask::State::Paused || task.state == DownloadTask::State::Interrupted)
                m_downloader->resume(task.taskId);
        }
        return;
    }
    DownloadTask task;
    task.taskId = QUuid::createUuid().toString(QUuid::Id128);
    task.ownerUid = m_uid;
    task.fileId = message.fileId;
    task.name = message.fileName;
    task.totalSize = qint64(message.fileSize);
    task.silent = !manual;
    task.urgent = manual;
    if (isImage || isVideo) {
        const QString dir = isImage ? downloadpaths::imagesDir(m_uid) : downloadpaths::videosDir(m_uid);
        task.targetPath = QDir(dir).filePath(QStringLiteral("%1.%2").arg(message.fileId)
                                              .arg(QFileInfo(message.fileName).suffix().toLower()));
    }
    QString reason;
    if (!m_downloader->enqueue(task, &reason)) {
        m_errors.insert(message.fileId, reason);
        emit mediaChanged(message.fileId);
        if (manual) emit errorOccurred(reason);
    }
}
