#include "AvatarStore.h"

#include "net/FileDownloader.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QRegularExpression>
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
    // 读出本地已有的头像：文件名是 "<UID>_<file.id>.<实际图片格式>"。
    const QRegularExpression pattern(QStringLiteral("^([1-9][0-9]*)_([1-9][0-9]*)\\.([a-z0-9]+)$"));
    const auto formats = QImageReader::supportedImageFormats();
    for (const QString &name : dir.entryList(QDir::Files)) {
        const auto match = pattern.match(name);
        if (!match.hasMatch() || !formats.contains(match.captured(3).toLatin1())) continue;
        const int uid = match.captured(1).toInt();
        const int fileId = match.captured(2).toInt();
        if (uid > 0 && fileId > 0 && fileId >= m_local.value(uid).fileId)
            m_local.insert(uid, {fileId, dir.filePath(name)});
    }
}

QString AvatarStore::path(int uid) const {
    return m_local.value(uid).path;
}

void AvatarStore::ensure(int uid, int fileId) {
    if (m_dir.isEmpty() || uid <= 0) return;
    const auto local = m_local.value(uid);

    if (fileId <= 0) {   // 对方没有头像（或清除了）：本地存的也删掉
        if (local.fileId > 0) {
            QFile::remove(local.path);
            m_local.remove(uid);
            emit avatarChanged(uid);
        }
        return;
    }
    if (local.fileId == fileId && QFile::exists(local.path)) return;   // 本地的就是最新的

    // 本地没有，或者 file.id 对不上：重新拉。旧文件先留着用，下完再替换
    if (!m_waiting.contains(fileId, uid)) m_waiting.insert(fileId, uid);
    m_downloader->download(fileId);
}

void AvatarStore::save(int fileId, const QByteArray &data) {
    const QList<int> uids = m_waiting.values(fileId);
    m_waiting.remove(fileId);
    if (m_dir.isEmpty() || uids.isEmpty()) return;

    // 依据文件头识别格式，不猜扩展名，也不把 JPEG 等原始字节伪装成 PNG。
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    const QByteArray format = QImageReader::imageFormat(&buffer).toLower();
    if (format.isEmpty() || !QImageReader::supportedImageFormats().contains(format)) return;

    for (int uid : uids) {
        const QString filename = m_dir + QStringLiteral("/%1_%2.%3").arg(uid).arg(fileId).arg(QString::fromLatin1(format));
        QSaveFile file(filename);   // 写完整了才落到正式文件名，中途退出不会留下半张图
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) continue;
        const auto old = m_local.value(uid);
        if (!old.path.isEmpty() && old.path != filename) QFile::remove(old.path);
        m_local.insert(uid, {fileId, filename});
        emit avatarChanged(uid);
    }
}
