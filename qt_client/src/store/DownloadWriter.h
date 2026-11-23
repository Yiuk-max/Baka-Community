#pragma once

// 分片落盘：写文件、增量算 SHA-256、按分片边界续写、完成后改名。
// 磁盘操作全部在内部工作线程执行；调用方阻塞拿结果，所以“这一片写完”之后才会请求
// 下一片 —— 下载的背压就靠这一点，内存里不会攒着整个文件。
#include <QByteArray>
#include <QObject>
#include <QString>

class QThread;

class DownloadWriter : public QObject {
    Q_OBJECT
public:
    explicit DownloadWriter(QObject *parent = nullptr);
    ~DownloadWriter() override;

    // 打开（或复用）临时文件。超出最后一个完整分片的半片会被截掉，
    // resumeChunk 返回续写起点；expectedHash 非空时对已有前缀重算 SHA-256。
    bool begin(const QString &key, const QString &partPath, quint32 chunkSize,
               const QByteArray &expectedHash, quint32 *resumeChunk, QString *error);
    // 写到 index * chunkSize 处；顺序下载，不要求调用方自己算偏移
    bool writeChunk(const QString &key, quint32 index, const QByteArray &data, QString *error);
    // 校验总大小 / hash（expectedHash 非空时），再把临时文件改名成 finalPath。
    // finalPath 已存在会被替换：调用方负责保证最终路径要么是缓存位、要么是新名字。
    bool commit(const QString &key, const QString &finalPath, qint64 totalSize,
                const QString &expectedHash, QString *error);
    void discard(const QString &key);    // 关闭并删除临时文件（取消 / 失败）
    bool closeKeep(const QString &key);  // 只关闭，保留临时文件（暂停 / 断线中断）
    bool contains(const QString &key) const;

private:
    class Impl;                       // 隐藏 QFile / QCryptographicHash 细节
    Impl *m_impl = nullptr;
    QThread *m_thread = nullptr;
};
