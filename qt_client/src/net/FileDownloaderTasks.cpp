// FileDownloader 的一部分：入队 / 取消 / 暂停 / 恢复 / 断点读回。
// 单独放一个文件是为了让协议收发（FileDownloader.cpp）保持在 400 行以内；类还是同一个。
#include "FileDownloader.h"

#include "ChatClient.h"
#include "DownloadJournal.h"
#include "store/DownloadWriter.h"
#include "util/DownloadPaths.h"
#include "message.pb.h"

#include <QFile>
#include <QTimer>

namespace {

bool failWith(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

} // namespace

bool FileDownloader::enqueue(const DownloadTask &incoming, QString *error) {
    if (error) error->clear();
    DownloadTask task = incoming;
    if (task.fileId <= 0) return failWith(error, tr("文件编号无效"));
    if (task.taskId.isEmpty()) task.taskId = newTaskId();
    if (task.ownerUid <= 0) task.ownerUid = m_client->uid();
    if (task.ownerUid <= 0 || task.ownerUid != m_client->uid())
        return failWith(error, tr("请先登录当前账号"));

    if (task.targetPath.isEmpty() && !task.silent) {
        // 普通文件下载：目录每次重新解析（系统设置 / 环境变量可能变了）
        QString reason;
        const QString dir = downloadpaths::downloadDirPath(&reason);
        if (dir.isEmpty()) {
            return failWith(error, reason.isEmpty() ? tr("无法确定下载目录") : reason);
        }
        task.targetPath = downloadpaths::uniqueFilePath(
            dir, task.name.isEmpty() ? QStringLiteral("file") : task.name);
        if (task.targetPath.isEmpty()) return failWith(error, tr("无法生成目标文件名"));
    }
    if (!task.targetPath.isEmpty() && task.partPath.isEmpty())
        task.partPath = downloadpaths::partPathFor(task.targetPath, task.taskId);

    if (hasSameFile(task)) return failWith(error, tr("这个文件已经在下载队列里了"));

    task.state = DownloadTask::State::Queued;
    task.error.clear();
    insertQueued(task);
    emitState(task, {});
    startNext();
    return true;
}

void FileDownloader::insertQueued(const DownloadTask &task) {
    if (!task.urgent) {
        m_queue.append(task);
        return;
    }
    int index = 0;   // 主动下载排在自动缓存前面，但仍按先后顺序
    while (index < m_queue.size() && m_queue.at(index).urgent) ++index;
    m_queue.insert(index, task);
}

bool FileDownloader::hasSameFile(const DownloadTask &task) const {
    const bool memory = task.targetPath.isEmpty();
    const auto same = [&task, memory](const DownloadTask &other) {
        if (other.ownerUid != task.ownerUid) return false;
        if (other.taskId == task.taskId) return true;
        return other.fileId == task.fileId && other.targetPath.isEmpty() == memory;
    };
    if (m_hasActive && same(m_active.task)) return true;
    for (const DownloadTask &queued : m_queue)
        if (same(queued)) return true;
    for (const DownloadTask &held : m_held)
        if (same(held)) return true;
    return false;
}

void FileDownloader::cancel(const QString &taskId) {
    if (m_hasActive && m_active.task.taskId == taskId) {
        stopActive(DownloadTask::State::Canceled, tr("已取消"));
        return;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).taskId != taskId) continue;
        DownloadTask task = m_queue.takeAt(i);
        task.state = DownloadTask::State::Canceled;
        task.error = tr("已取消");
        emitState(task, task.error);
        return;
    }
    for (int i = 0; i < m_held.size(); ++i) {
        if (m_held.at(i).taskId != taskId) continue;
        DownloadTask task = m_held.takeAt(i);
        if (!task.partPath.isEmpty()) QFile::remove(task.partPath);
        downloadjournal::remove(task.ownerUid, task.taskId);
        task.state = DownloadTask::State::Canceled;
        task.error = tr("已取消");
        emitState(task, task.error);
        return;
    }
}

bool FileDownloader::pause(const QString &taskId) {
    if (m_hasActive && m_active.task.taskId == taskId) {
        if (m_active.memory) return false;   // 内存任务没有断点可用
        stopActive(DownloadTask::State::Paused, tr("已暂停"));
        return true;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).taskId != taskId) continue;
        DownloadTask task = m_queue.takeAt(i);
        task.state = DownloadTask::State::Paused;
        task.error = tr("已暂停");
        downloadjournal::save(task);
        holdTask(task);
        emitState(task, task.error);
        return true;
    }
    return false;
}

bool FileDownloader::resume(const QString &taskId) {
    for (int i = 0; i < m_held.size(); ++i) {
        if (m_held.at(i).taskId != taskId) continue;
        if (m_held.at(i).ownerUid != m_client->uid() || !m_client->isAuthenticated()) return false;
        DownloadTask task = m_held.takeAt(i);
        if (hasSameFile(task)) { holdTask(task); return false; }
        task.state = DownloadTask::State::Queued;
        task.error.clear();
        insertQueued(task);
        emitState(task, {});
        startNext();
        return true;
    }
    return false;
}

void FileDownloader::reset() {
    m_timeout->stop();

    // 正在下的：磁盘任务留临时文件 + 断点，内存任务直接丢并通知等待方重新请求
    if (m_hasActive) {
        DownloadTask task = m_active.task;
        const bool memory = m_active.memory;
        m_active = Active{};
        m_hasActive = false;
        if (!memory) {
            m_writer->closeKeep(task.taskId);
            task.state = DownloadTask::State::Interrupted;
            task.error = tr("连接已断开");
            downloadjournal::save(task);
            holdTask(task);
            emitState(task, task.error);
        } else {
            emit failed(task.fileId);   // 头像 / 社区图片的等待项要清掉，登录后重新拉
        }
    }

    const QList<DownloadTask> queued = m_queue;
    m_queue.clear();
    for (const DownloadTask &item : queued) {
        if (item.targetPath.isEmpty()) {
            emit failed(item.fileId);   // 静默内存任务（头像等）丢掉后通知等待方
            continue;
        }
        DownloadTask task = item;
        task.state = DownloadTask::State::Interrupted;
        task.error = tr("连接已断开");
        downloadjournal::save(task);
        holdTask(task);
        emitState(task, task.error);
    }
}

QList<DownloadTask> FileDownloader::tasks() const {
    QList<DownloadTask> all;
    if (m_hasActive) all.append(m_active.task);
    all.append(m_queue);
    all.append(m_held);
    return all;
}

bool FileDownloader::findTask(const QString &taskId, DownloadTask *out) const {
    if (m_hasActive && m_active.task.taskId == taskId) {
        if (out) *out = m_active.task;
        return true;
    }
    for (const DownloadTask &queued : m_queue) {
        if (queued.taskId != taskId) continue;
        if (out) *out = queued;
        return true;
    }
    for (const DownloadTask &held : m_held) {
        if (held.taskId != taskId) continue;
        if (out) *out = held;
        return true;
    }
    return false;
}

bool FileDownloader::promote(const QString &taskId) {
    if (m_hasActive && m_active.task.taskId == taskId) {
        m_active.task.silent = false;
        m_active.task.urgent = true;
        emitState(m_active.task, {});
        return true;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).taskId != taskId) continue;
        DownloadTask task = m_queue.takeAt(i);
        task.silent = false;
        task.urgent = true;
        insertQueued(task);
        emitState(task, {});
        return true;
    }
    for (DownloadTask &task : m_held) {
        if (task.taskId != taskId) continue;
        task.silent = false;
        task.urgent = true;
        emitState(task, task.error);
        return true;
    }
    return false;
}

// 暂停 / 中断的任务统一进 m_held：同一个 taskId 只留一份，避免断线又退出登录时重复
void FileDownloader::holdTask(const DownloadTask &task) {
    for (const DownloadTask &held : m_held)
        if (held.taskId == task.taskId) return;
    m_held.append(task);
}

int FileDownloader::restorePending(int uid) {
    if (uid <= 0) return 0;
    int count = 0;
    for (const DownloadTask &item : downloadjournal::load(uid)) {
        if (hasSameFile(item)) continue;
        DownloadTask task = item;
        task.state = DownloadTask::State::Interrupted;
        task.error = tr("上次下载未完成，可按断点继续");
        holdTask(task);
        emitState(task, task.error);
        ++count;
    }
    return count;
}

// 断线重连后：本次会话里被网络中断的磁盘任务自动续传；用户暂停 / 取消 / 失败的不动
void FileDownloader::resumeNetworkInterrupted() {
    if (!m_client->isAuthenticated() || m_client->uid() <= 0) return;
    QList<DownloadTask> targets;
    for (const DownloadTask &task : m_held) {
        if (task.ownerUid == m_client->uid() && task.state == DownloadTask::State::Interrupted
                && !task.targetPath.isEmpty())
            targets.append(task);
    }
    for (const DownloadTask &task : targets) resume(task.taskId);
}

