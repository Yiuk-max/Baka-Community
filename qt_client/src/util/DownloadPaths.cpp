#include "DownloadPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QHash>
#include <QStringList>
#include <QTemporaryFile>

namespace downloadpaths {

namespace {

const char *kDownloadDirEnv = "BAKA_DOWNLOAD_DIR";
const int kMaxFileNameBytes = 180;   // 留出 part 后缀和目录的余量，别撞 Windows 260 上限

// 账号根目录：和 config.json 一起放在 exe 旁边（AvatarStore 用的是同一个父目录）
QString accountDir(int uid) {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("users/%1").arg(uid));
}

// 把 text 里所有匹配 pattern 的“变量名”替换成环境变量值；变量不存在时保留原文。
// Qt 没有 QString::replace(正则, lambda) 这个重载，这里自己拼一遍。
QString replaceVariables(const QString &text, const QString &pattern, int nameGroup) {
    static QHash<QString, QRegularExpression> cache;
    if (!cache.contains(pattern)) cache.insert(pattern, QRegularExpression(pattern));
    const QRegularExpression &expression = cache.value(pattern);

    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QByteArray value = qgetenv(match.captured(nameGroup).toLatin1().constData());
        if (value.isEmpty()) continue;   // 原样保留，交给后面的路径校验报错
        out += text.mid(last, match.capturedStart() - last);
        out += QString::fromLocal8Bit(value);
        last = match.capturedEnd();
    }
    out += text.mid(last);
    return out;
}

// 展开 %VAR%（Windows 写法）、$VAR 与 ${VAR}
QString expandEnvironment(const QString &raw) {
    static const QString percent = QStringLiteral("%([A-Za-z_][A-Za-z0-9_]*)%");
    static const QString dollar = QStringLiteral(
        "\\$(?:\\{([A-Za-z_][A-Za-z0-9_]*)\\}|([A-Za-z_][A-Za-z0-9_]*))");

    QString text = raw;
    for (int guard = 0; guard < 20; ++guard) {   // 防止变量套变量时无限展开
        QString next = replaceVariables(text, percent, 1);
        // $VAR / ${VAR}：第一个非空捕获组就是变量名，分组 1 空时取分组 2
        static const QRegularExpression probe(dollar);
        QString expanded;
        int last = 0;
        QRegularExpressionMatchIterator it = probe.globalMatch(next);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString name = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
            const QByteArray value = qgetenv(name.toLatin1().constData());
            if (value.isEmpty()) continue;
            expanded += next.mid(last, match.capturedStart() - last);
            expanded += QString::fromLocal8Bit(value);
            last = match.capturedEnd();
        }
        expanded += next.mid(last);
        next = expanded;
        if (next == text) break;
        text = next;
    }
    if (text.startsWith(QLatin1Char('~')))
        text = QDir::homePath() + text.mid(1);
    return text;
}

QString extensionOf(const QString &fileName) {
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    return (dot <= 0 || dot == fileName.size() - 1) ? QString() : fileName.mid(dot);
}

// 目录本身可用：存在（或能创建）并且真的能写
bool usableDir(const QString &dir, QString *error) {
    if (dir.isEmpty()) {
        if (error) *error = QStringLiteral("下载目录为空");
        return false;
    }
    if (!QDir().mkpath(dir)) {
        if (error) *error = QStringLiteral("无法创建下载目录：%1").arg(QDir::toNativeSeparators(dir));
        return false;
    }
    // 随机临时探针，不能截断用户已有的同名文件。
    QTemporaryFile file(QDir(dir).filePath(QStringLiteral(".baka-write-XXXXXX")));
    if (!file.open()) {
        if (error) *error = QStringLiteral("下载目录不可写：%1").arg(QDir::toNativeSeparators(dir));
        return false;
    }
    file.close();
    return true;
}

} // namespace

QString userRoot(int uid) { return QDir::toNativeSeparators(QDir(accountDir(uid)).absolutePath()); }

QString mediaDir(int uid) { return QDir(userRoot(uid)).filePath(QStringLiteral("media")); }
QString imagesDir(int uid) { return QDir(mediaDir(uid)).filePath(QStringLiteral("images")); }
QString videosDir(int uid) { return QDir(mediaDir(uid)).filePath(QStringLiteral("videos")); }
QString thumbsDir(int uid) { return QDir(mediaDir(uid)).filePath(QStringLiteral("thumbs")); }
QString transfersDir(int uid) { return QDir(userRoot(uid)).filePath(QStringLiteral("transfers")); }

QString ensureDir(const QString &dir) {
    if (dir.isEmpty()) return QString();
    return QDir().mkpath(dir) ? dir : QString();
}

QString cachedMediaPath(int uid, int fileId) {
    if (uid <= 0 || fileId <= 0) return QString();
    const QString prefix = QString::number(fileId) + QLatin1Char('.');
    const QStringList dirs{imagesDir(uid), videosDir(uid), thumbsDir(uid)};
    for (const QString &dir : dirs) {
        // 前缀 + 任意扩展名；只认文件，不认同名目录
        const QStringList names = QDir(dir).entryList({prefix + QStringLiteral("*")}, QDir::Files);
        for (const QString &name : names) {
            if (name.size() <= prefix.size()) continue;
            return QDir(dir).filePath(name);
        }
    }
    return QString();
}

QString downloadDirPath(QString *error) {
    if (error) error->clear();

    QString dir;
    const QByteArray custom = qgetenv(kDownloadDirEnv);
    if (!custom.isEmpty()) {
        const QString raw = QString::fromLocal8Bit(custom).trimmed();
        if (!raw.isEmpty()) {
            dir = QDir::cleanPath(expandEnvironment(raw));
            if (!QDir::isAbsolutePath(dir)) {
                if (error) {
                    *error = QStringLiteral("%1 不是绝对路径：%2")
                                 .arg(QString::fromLatin1(kDownloadDirEnv),
                                      QDir::toNativeSeparators(dir));
                }
                return QString();
            }
            if (!usableDir(dir, error)) return QString();
            return dir;
        }
    }

    // 系统“下载”目录：Windows 走 Known Folder 重定向，Linux 走 XDG，macOS 走系统目录
    dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) {
        if (error) *error = QStringLiteral("系统没有提供默认下载目录");
        return QString();
    }
    dir = QDir::cleanPath(dir);
    if (!usableDir(dir, error)) return QString();
    return dir;
}

QString sanitizeFileName(const QString &name, const QString &fallback) {
    QString text = name;
    text.replace(QLatin1Char('\\'), QLatin1Char('_'));
    text.replace(QLatin1Char('/'), QLatin1Char('_'));
    text.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")));
    // Windows 不允许的字符：: * ? " < > |（macOS 的冒号也一起处理）
    text.remove(QRegularExpression(QStringLiteral("[<>:\"|?*]")));
    text = text.simplified();
    // 结尾的点和空格在 Windows 上会被静默吞掉，导致名字和预期不一致
    while (text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    if (text.isEmpty()) text = fallback;

    static const QStringList reserved{
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")};
    const QString stem = text.section(QLatin1Char('.'), 0, 0);
    if (reserved.contains(stem.toUpper())) text.prepend(QLatin1Char('_'));

    const QString ext = extensionOf(text);
    QString base = ext.isEmpty() ? text : text.chopped(ext.size());
    const int maxBase = kMaxFileNameBytes - ext.toUtf8().size();
    if (base.toUtf8().size() > maxBase && maxBase > 0) {
        // 按字符逐个截，避免把多字节 UTF-8 切坏
        QString cut;
        int bytes = 0;
        for (const QChar &ch : base) {
            const int size = QString(ch).toUtf8().size();
            if (bytes + size > maxBase) break;
            cut.append(ch);
            bytes += size;
        }
        base = cut.isEmpty() ? QStringLiteral("file") : cut;
    }
    return base + ext;
}

QString uniqueFilePath(const QString &dir, const QString &fileName) {
    if (dir.isEmpty()) return QString();
    const QString safe = sanitizeFileName(fileName);
    const QString ext = extensionOf(safe);
    const QString base = ext.isEmpty() ? safe : safe.chopped(ext.size());
    QString path = QDir(dir).filePath(safe);
    for (int index = 1; QFileInfo::exists(path) || QFileInfo(path).isSymLink(); ++index) {
        if (index > 9999) break;   // 极端重名：交给调用方报错，别在这里无限循环
        path = QDir(dir).filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(index).arg(ext));
    }
    return path;
}

QString partPathFor(const QString &finalPath, const QString &taskTag) {
    if (finalPath.isEmpty()) return QString();
    const QFileInfo info(finalPath);
    QString tag = taskTag;
    tag.remove(QRegularExpression(QStringLiteral("[^0-9A-Za-z_-]")));
    if (tag.isEmpty()) tag = QStringLiteral("part");
    return info.dir().filePath(
        QStringLiteral(".%1.%2.part").arg(info.fileName(), tag));
}

} // namespace downloadpaths
