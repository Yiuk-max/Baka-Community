#pragma once

// 一次下载任务（只有下载方向；上传由 AvatarUploader / 后续附件上传器负责）。
// 纯数据 + 状态枚举，界面只读它，下载器负责改它。
//
// 两种形态：
//   * targetPath 非空：逐片写磁盘，完成后 localPath 是正式文件（附件、大视频都走这条）；
//   * targetPath 为空：只允许 silent 任务，整份读进内存（头像 / 社区图片这类小图）。
#include <QString>
#include <QtGlobal>

struct DownloadTask {
    enum class State {
        Queued,        // 排队中
        Initializing,  // 已发 file_download_init，等 ready
        Downloading,   // 正在拉分片
        Verifying,     // 分片齐了，校验 + 提交
        Paused,        // 用户暂停（保留临时文件）
        Interrupted,   // 断线 / 退出登录（保留临时文件，认证后可恢复）
        Completed,
        Failed,
        Canceled,
    };

    QString taskId;            // 本地下载任务号（日志 / 界面定位用，同一个任务只入队一次）
    int     ownerUid = 0;      // 哪个账号的任务（切账号后旧任务不能继续写）
    int     session = 0;       // 连接代次，用来丢掉上一个登录会话的迟到回执
    int     fileId = 0;        // 服务端 file.id
    QString name;              // 原始文件名（展示 + 生成目标文件名）
    QString mime;

    qint64  totalSize = 0;     // 总字节数（ready 之前可能未知）
    qint64  transferred = 0;   // 已落盘字节数
    int     transferId = 0;    // 服务端传输会话 id
    quint32 chunkSize = 0;
    quint32 chunkCount = 0;
    quint32 nextChunk = 0;     // 下一个要请求的块号（断点）
    QString hash;              // 服务端给的 SHA-256（可能为空）

    QString targetPath;        // 最终落盘位置（空 = 内存任务）
    QString partPath;          // 下载中的临时文件（和 targetPath 同目录）
    QString localPath;         // 完成后实际路径（重名改过名时和 targetPath 不同）

    bool silent = false;       // 静默任务：不出现在右下角进度卡（图片 / 小视频自动缓存、头像）
    bool urgent = false;       // 用户主动点开 / 下载的任务，插队到前面
    State state = State::Queued;
    QString error;             // 失败 / 中断原因（给界面显示）

    bool isTerminal() const {
        return state == State::Completed || state == State::Failed || state == State::Canceled;
    }
    bool isActive() const {
        return state == State::Initializing || state == State::Downloading
            || state == State::Verifying;
    }
    // 0~100；总大小未知时返回 0
    int progressPercent() const {
        if (state == State::Completed) return 100;
        if (totalSize <= 0) return 0;
        const qint64 done = qBound<qint64>(0, transferred, totalSize);
        return static_cast<int>(done * 100 / totalSize);
    }
    // 和 Web 端 transfer-progress-card 上显示的状态词一致
    QString stateText() const {
        switch (state) {
        case State::Queued:       return QStringLiteral("排队中");
        case State::Initializing: return QStringLiteral("初始化");
        case State::Downloading:  return QStringLiteral("下载中");
        case State::Verifying:    return QStringLiteral("校验中");
        case State::Paused:       return QStringLiteral("已暂停");
        case State::Interrupted:  return QStringLiteral("已中断");
        case State::Completed:    return QStringLiteral("已完成");
        case State::Failed:       return QStringLiteral("失败");
        case State::Canceled:     return QStringLiteral("已取消");
        }
        return QString();
    }
};
