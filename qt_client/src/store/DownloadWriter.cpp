#include "DownloadWriter.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QThread>

namespace {

const qint64 kHashBufferSize = 1024 * 1024;   // 续传重算 hash 时的读取块

struct Session {
    QFile file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    bool hashEnabled = false;
    qint64 hashedBytes = 0;   // 已经喂进 hash 的字节数（前缀 + 本次下载）
    quint32 chunkSize = 0;
};

// 检查 + 赋值错误信息的两个小工具，省得每个分支都写一遍
bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

} // namespace

// 工作线程里的实体：只有 QObject 身份（供 moveToThread / invokeMethod），不声明信号槽，
// 所以不需要 moc；对外接口全是普通成员函数，由 GUI 线程以阻塞调用执行。
class DownloadWriter::Impl : public QObject {
public:
    ~Impl() override { clear(); }

    bool begin(const QString &key, const QString &partPath, quint32 chunkSize,
               const QByteArray &expectedHash, quint32 *resumeChunk, QString *error) {
        if (chunkSize == 0) return fail(error, QStringLiteral("分片大小为 0"));
        if (partPath.isEmpty()) return fail(error, QStringLiteral("临时文件路径为空"));

        const QFileInfo info(partPath);
        if (!QDir().mkpath(info.absolutePath()))
            return fail(error, QStringLiteral("无法创建下载目录：%1")
                                   .arg(QDir::toNativeSeparators(info.absolutePath())));

        Session *session = m_sessions.value(key, nullptr);
        if (session && session->file.fileName() != partPath) {
            session->file.close();
            delete session;
            m_sessions.remove(key);
            session = nullptr;
        }
        if (!session) {
            session = new Session;
            session->file.setFileName(partPath);
            m_sessions.insert(key, session);
        }
        session->chunkSize = chunkSize;

        if (!session->file.isOpen()
                && !session->file.open(QIODevice::ReadWrite | QIODevice::Unbuffered)) {
            return fail(error, QStringLiteral("无法打开临时文件：%1（%2）")
                                   .arg(QDir::toNativeSeparators(partPath),
                                        session->file.errorString()));
        }

        // 已有进度只算完整分片：最后半片是上次没写完的，截掉重下
        const qint64 size = session->file.size();
        const quint32 resume = static_cast<quint32>(size / chunkSize);
        const qint64 aligned = static_cast<qint64>(resume) * chunkSize;
        if (size != aligned && !session->file.resize(aligned))
            return fail(error, QStringLiteral("无法截断未写完整的临时文件：%1")
                                   .arg(session->file.errorString()));

        session->hash.reset();
        session->hashEnabled = !expectedHash.isEmpty();
        session->hashedBytes = 0;
        if (session->hashEnabled && aligned > 0) {
            // 续传时对已有前缀重算，保证整份 hash 覆盖到磁盘上的真实内容
            if (!session->file.seek(0)) return fail(error, QStringLiteral("无法定位临时文件进行校验"));
            QByteArray buffer(static_cast<int>(kHashBufferSize), Qt::Uninitialized);
            qint64 remaining = aligned;
            while (remaining > 0) {
                const qint64 n = session->file.read(buffer.data(), qMin<qint64>(buffer.size(), remaining));
                if (n <= 0) return fail(error, QStringLiteral("读取临时文件失败，无法校验断点"));
                session->hash.addData(QByteArrayView(buffer.constData(), n));
                session->hashedBytes += n;
                remaining -= n;
            }
        }

        if (!session->file.seek(aligned))
            return fail(error, QStringLiteral("无法定位到断点位置：%1").arg(session->file.errorString()));
        if (resumeChunk) *resumeChunk = resume;
        return true;
    }

    bool writeChunk(const QString &key, quint32 index, const QByteArray &data, QString *error) {
        Session *session = m_sessions.value(key, nullptr);
        if (!session || !session->file.isOpen())
            return fail(error, QStringLiteral("下载任务不在写入状态"));

        const qint64 offset = static_cast<qint64>(index) * session->chunkSize;
        if (!session->file.seek(offset))
            return fail(error, QStringLiteral("写入定位失败：%1").arg(session->file.errorString()));
        if (session->file.write(data) != data.size())
            return fail(error, QStringLiteral("写入磁盘失败：%1").arg(session->file.errorString()));
        if (session->hashEnabled) {
            session->hash.addData(data);
            session->hashedBytes += data.size();
        }
        return true;
    }

    bool commit(const QString &key, const QString &finalPath, qint64 totalSize,
                const QString &expectedHash, QString *error) {
        Session *session = m_sessions.value(key, nullptr);
        if (!session) return fail(error, QStringLiteral("下载任务不在写入状态"));

        // 失败分支保留 session（文件已关闭）：调用方的 discard() 才能把临时文件删干净
        session->file.flush();
        session->file.close();
        const QString part = session->file.fileName();
        const qint64 actualSize = QFileInfo(part).size();
        const bool hashEnabled = session->hashEnabled;
        const qint64 hashedBytes = session->hashedBytes;
        QString actualHash = hashEnabled ? QString::fromLatin1(session->hash.result().toHex()) : QString();

        if (actualSize != totalSize)
            return fail(error, QStringLiteral("文件大小不符（%1 / %2 字节）").arg(actualSize).arg(totalSize));

        if (!expectedHash.isEmpty()) {
            if (!hashEnabled || hashedBytes != actualSize) {
                // 续传起点没有 hash 或增量没覆盖全文件：从磁盘重算一次
                QCryptographicHash hash(QCryptographicHash::Sha256);
                QFile file(part);
                if (!file.open(QIODevice::ReadOnly))
                    return fail(error, QStringLiteral("无法读取临时文件校验内容"));
                if (!hash.addData(&file))
                    return fail(error, QStringLiteral("校验读取失败"));
                actualHash = QString::fromLatin1(hash.result().toHex());
            }
            if (actualHash.compare(expectedHash, Qt::CaseInsensitive) != 0)
                return fail(error, QStringLiteral("文件校验失败（SHA-256 不一致）"));
        }

        const QFileInfo finalInfo(finalPath);
        if (!QDir().mkpath(finalInfo.absolutePath()))
            return fail(error, QStringLiteral("无法创建目标目录：%1")
                                   .arg(QDir::toNativeSeparators(finalInfo.absolutePath())));
        // 不覆盖用户文件；目标在提交前出现冲突时明确失败。
        if (finalInfo.exists() || finalInfo.isSymLink())
            return fail(error, QStringLiteral("目标文件已存在：%1")
                                   .arg(QDir::toNativeSeparators(finalPath)));
        if (!QFile::rename(part, finalPath))
            return fail(error, QStringLiteral("无法把临时文件改名成最终文件"));

        m_sessions.remove(key);
        delete session;
        return true;
    }

    void discard(const QString &key) {
        Session *session = m_sessions.value(key, nullptr);
        if (!session) return;
        m_sessions.remove(key);
        const QString part = session->file.fileName();
        if (session->file.isOpen()) session->file.close();
        delete session;
        if (!part.isEmpty()) QFile::remove(part);
    }

    bool closeKeep(const QString &key) {
        Session *session = m_sessions.value(key, nullptr);
        if (!session) return true;
        m_sessions.remove(key);
        if (session->file.isOpen()) {
            session->file.flush();
            session->file.close();
        }
        delete session;
        return true;
    }

    bool contains(const QString &key) const { return m_sessions.contains(key); }

private:
    void clear() {
        for (Session *session : m_sessions) {
            if (session->file.isOpen()) session->file.close();
            delete session;
        }
        m_sessions.clear();
    }

    QHash<QString, Session *> m_sessions;
};

DownloadWriter::DownloadWriter(QObject *parent) : QObject(parent) {
    m_impl = new Impl;
    m_thread = new QThread(this);
    m_impl->moveToThread(m_thread);
    m_thread->start();
}

DownloadWriter::~DownloadWriter() {
    // 在工作线程里销毁，QFile 才不会被跨线程析构
    QMetaObject::invokeMethod(m_impl, [impl = m_impl] { delete impl; }, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait(3000);
}

// 下面几个都是“投递到工作线程 → 阻塞等结果”。值是局部变量，阻塞期间一直有效。
bool DownloadWriter::begin(const QString &key, const QString &partPath, quint32 chunkSize,
                           const QByteArray &expectedHash, quint32 *resumeChunk, QString *error) {
    bool ok = false;
    QMetaObject::invokeMethod(m_impl, [&] {
        ok = m_impl->begin(key, partPath, chunkSize, expectedHash, resumeChunk, error);
    }, Qt::BlockingQueuedConnection);
    return ok;
}

bool DownloadWriter::writeChunk(const QString &key, quint32 index, const QByteArray &data,
                                QString *error) {
    bool ok = false;
    QMetaObject::invokeMethod(m_impl, [&] {
        ok = m_impl->writeChunk(key, index, data, error);
    }, Qt::BlockingQueuedConnection);
    return ok;
}

bool DownloadWriter::commit(const QString &key, const QString &finalPath, qint64 totalSize,
                            const QString &expectedHash, QString *error) {
    bool ok = false;
    QMetaObject::invokeMethod(m_impl, [&] {
        ok = m_impl->commit(key, finalPath, totalSize, expectedHash, error);
    }, Qt::BlockingQueuedConnection);
    return ok;
}

void DownloadWriter::discard(const QString &key) {
    QMetaObject::invokeMethod(m_impl, [&] { m_impl->discard(key); }, Qt::BlockingQueuedConnection);
}

bool DownloadWriter::closeKeep(const QString &key) {
    bool ok = false;
    QMetaObject::invokeMethod(m_impl, [&] { ok = m_impl->closeKeep(key); },
                              Qt::BlockingQueuedConnection);
    return ok;
}

bool DownloadWriter::contains(const QString &key) const {
    bool ok = false;
    QMetaObject::invokeMethod(m_impl, [&] { ok = m_impl->contains(key); },
                              Qt::BlockingQueuedConnection);
    return ok;
}
