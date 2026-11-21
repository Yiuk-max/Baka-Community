#include "AvatarStore.h"

#include "net/FileDownloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>

AvatarStore::AvatarStore(FileDownloader *downloader, QObject *parent)
    : QObject(parent), m_downloader(downloader) {
    connect(m_downloader, &FileDownloader::finished, this, &AvatarStore::save);
    connect(m_downloader, &FileDownloader::failed, this, [this](int fileId) { m_waiting.remove(fileId); });
}

void AvatarStore::setUser(int selfUid) {
    m_local.clear();
    m_waiting.clear();
    m_dir.clear();
    if (selfUid <= 0) return;

    // 放在程序本体旁边（和 config.json 同一个目录）
    m_dir = QCoreApplication::applicationDirPath()
            + QStringLiteral("/users/%1/avatars").arg(selfUid);
    QDir dir(m_dir);
    dir.mkpath(QStringLiteral("."));
    // 读出本地已有的头像：文件名是 "<UID>_<file.id>"
    for (const QString &name : dir.entryList(QDir::Files)) {
        const QStringList parts = name.split(QLatin1Char('_'));
        if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0) {
            m_local.insert(parts[0].toInt(), parts[1].toInt());
        }
    }
}

QString AvatarStore::filePath(int uid, int fileId) const {
    return m_dir + QStringLiteral("/%1_%2").arg(uid).arg(fileId);
}

QString AvatarStore::path(int uid) const {
    const int fileId = m_local.value(uid);
    return fileId > 0 ? filePath(uid, fileId) : QString();
}

void AvatarStore::ensure(int uid, int fileId) {
    if (m_dir.isEmpty() || uid <= 0) return;
    const int localId = m_local.value(uid);

    if (fileId <= 0) {   // 对方没有头像（或清除了）：本地存的也删掉
        if (localId > 0) {
            QFile::remove(filePath(uid, localId));
            m_local.remove(uid);
            emit avatarChanged(uid);
        }
        return;
    }
    if (localId == fileId && QFile::exists(filePath(uid, fileId))) return;   // 本地的就是最新的

    // 本地没有，或者 file.id 对不上：重新拉。旧文件先留着用，下完再替换
    if (!m_waiting.contains(fileId, uid)) m_waiting.insert(fileId, uid);
    m_downloader->download(fileId);
}

void AvatarStore::save(int fileId, const QByteArray &data) {
    const QList<int> uids = m_waiting.values(fileId);
    m_waiting.remove(fileId);
    if (m_dir.isEmpty()) return;

    for (int uid : uids) {
        QSaveFile file(filePath(uid, fileId));   // 写完整了才落到正式文件名，中途退出不会留下半张图
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) continue;
        const int oldId = m_local.value(uid);
        if (oldId > 0 && oldId != fileId) QFile::remove(filePath(uid, oldId));
        m_local.insert(uid, fileId);
        emit avatarChanged(uid);
    }
}
