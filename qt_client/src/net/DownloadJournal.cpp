#include "DownloadJournal.h"

#include "util/DownloadPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace downloadjournal {

namespace {

const char *kJournalSuffix = ".json";

// 断点文件的路径：只保留 taskId 里的安全字符，防止路径拼接出问题
QString journalPath(int uid, const QString &taskId) {
    QString safeId = taskId;
    safeId.remove(QRegularExpression(QStringLiteral("[^0-9A-Za-z_-]")));
    if (safeId.isEmpty()) safeId = QStringLiteral("task");
    return QDir(downloadpaths::transfersDir(uid)).filePath(safeId + QLatin1String(kJournalSuffix));
}

QJsonObject toJson(const DownloadTask &task) {
    QJsonObject object;
    object.insert(QStringLiteral("taskId"), task.taskId);
    object.insert(QStringLiteral("ownerUid"), task.ownerUid);
    object.insert(QStringLiteral("fileId"), task.fileId);
    object.insert(QStringLiteral("name"), task.name);
    object.insert(QStringLiteral("mime"), task.mime);
    object.insert(QStringLiteral("totalSize"), task.totalSize);
    object.insert(QStringLiteral("transferred"), task.transferred);
    object.insert(QStringLiteral("chunkSize"), static_cast<qint64>(task.chunkSize));
    object.insert(QStringLiteral("chunkCount"), static_cast<qint64>(task.chunkCount));
    object.insert(QStringLiteral("nextChunk"), static_cast<qint64>(task.nextChunk));
    object.insert(QStringLiteral("hash"), task.hash);
    object.insert(QStringLiteral("targetPath"), task.targetPath);
    object.insert(QStringLiteral("partPath"), task.partPath);
    object.insert(QStringLiteral("silent"), task.silent);
    return object;
}

DownloadTask fromJson(const QJsonObject &object) {
    DownloadTask task;
    task.taskId = object.value(QStringLiteral("taskId")).toString();
    task.ownerUid = object.value(QStringLiteral("ownerUid")).toInt();
    task.fileId = object.value(QStringLiteral("fileId")).toInt();
    task.name = object.value(QStringLiteral("name")).toString();
    task.mime = object.value(QStringLiteral("mime")).toString();
    task.totalSize = object.value(QStringLiteral("totalSize")).toInteger();
    task.transferred = object.value(QStringLiteral("transferred")).toInteger();
    task.chunkSize = static_cast<quint32>(object.value(QStringLiteral("chunkSize")).toInteger());
    task.chunkCount = static_cast<quint32>(object.value(QStringLiteral("chunkCount")).toInteger());
    task.nextChunk = static_cast<quint32>(object.value(QStringLiteral("nextChunk")).toInteger());
    task.hash = object.value(QStringLiteral("hash")).toString();
    task.targetPath = object.value(QStringLiteral("targetPath")).toString();
    task.partPath = object.value(QStringLiteral("partPath")).toString();
    task.silent = object.value(QStringLiteral("silent")).toBool();
    return task;
}

} // namespace

void save(const DownloadTask &task) {
    if (task.ownerUid <= 0 || task.targetPath.isEmpty()) return;
    const QString dir = downloadpaths::ensureDir(downloadpaths::transfersDir(task.ownerUid));
    if (dir.isEmpty()) return;
    QSaveFile file(journalPath(task.ownerUid, task.taskId));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(toJson(task)).toJson(QJsonDocument::Compact));
    file.commit();
}

void remove(int uid, const QString &taskId) {
    if (uid <= 0) return;
    // 任务可能已经从队列里摘掉了，不能靠 tasks() 反查 ownerUid
    QFile::remove(journalPath(uid, taskId));
}

QList<DownloadTask> load(int uid) {
    QList<DownloadTask> restored;
    if (uid <= 0) return restored;
    const QDir dir(downloadpaths::transfersDir(uid));
    const QStringList names = dir.entryList({QStringLiteral("*") + QLatin1String(kJournalSuffix)},
                                            QDir::Files);
    for (const QString &name : names) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        DownloadTask task = fromJson(document.object());
        if (task.ownerUid != uid || task.fileId <= 0 || task.targetPath.isEmpty()) continue;
        if (task.partPath.isEmpty() || !QFile::exists(task.partPath)) {
            QFile::remove(dir.filePath(name));   // 临时文件没了，断点也就没用了
            continue;
        }
        restored.append(task);
    }
    return restored;
}

} // namespace downloadjournal
