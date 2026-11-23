#pragma once

// 用户头像的本地存储。每个登录账号一个文件夹，放这个账号要用到的文件：
//   <程序所在目录>/users/<自己的UID>/avatars/<头像主人UID>_<file.id>.<实际图片格式>
// 只识别带图片后缀的新缓存，不读取或迁移旧的无后缀文件。
// 规则：本地有、且 file.id 和服务端列表里给的一样 → 直接用本地的；
//       本地没有，或 file.id 对不上（对方换了头像）→ 重新下载，下完替换掉旧文件。
#include <QHash>
#include <QMultiHash>
#include <QObject>
#include <QString>

class FileDownloader;

class AvatarStore : public QObject {
    Q_OBJECT
public:
    explicit AvatarStore(FileDownloader *downloader, QObject *parent = nullptr);

    void setUser(int selfUid);          // 登录后切到这个账号的文件夹；0 = 退出登录
    // 服务端说“uid 这个人的头像是 fileId”（<= 0 表示没设置头像）。需要的话会去下载
    void ensure(int uid, int fileId);
    QString path(int uid) const;        // uid 的本地头像文件；没有返回空字符串
    FileDownloader *downloader() const { return m_downloader; } // 图片预览复用同一下载队列

signals:
    void avatarChanged(int uid);        // uid 的本地头像文件变了（下完 / 被清除），path(uid) 是新值

private:
    struct CachedAvatar {
        int fileId = 0;
        QString path;
    };
    void save(int fileId, const QByteArray &data);

    FileDownloader *m_downloader = nullptr;
    QString m_dir;                      // 当前账号的 avatars 目录；空 = 没登录
    QHash<int, CachedAvatar> m_local;   // 头像主人 UID → 本地文件编号和完整路径
    QMultiHash<int, int> m_waiting;     // 正在下的 file.id → 等它的 UID
};
