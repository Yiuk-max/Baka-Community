#pragma once

// 界面用的纯数据结构：MainWindow 把服务端的 protobuf 列表转成这些，再交给各个控件，
// 控件因此不用认识 protobuf，也不碰网络。
#include <QString>

// 通讯录里的一行：好友或群（对应 proto 的 ContactInfo）
struct Contact {
    int uid = 0;            // 好友 UID 或群 / 频道 id
    QString name;           // 好友昵称 / 备注名，群名或频道名
    bool isGroup = false;
    bool isChannel = false; // true = 社区频道（回复用 channel_chat、历史带 is_channel）
    int avatarId = 0;       // 头像 file.id；<= 0 表示没设置（群 / 频道恒为 0）
    QString avatarPath;     // 本地头像文件（MainWindow 从 AvatarStore 填）；空 = 用默认头像
};

// 工具栏上的一个社区图标（对应 proto 的 CommunityInfo）
struct Community {
    int id = 0;
    QString name;
    QString description;
    int avatarId = 0;       // 社区头像 file.id；<= 0 表示没设置
    int bannerId = 0;       // 社区背景 file.id；<= 0 表示没设置
    QString role;           // 我在这个社区的身份：owner / admin / member
};

// 社区下的一个普通频道（对应 proto 的 ChannelInfo）
struct Channel {
    int id = 0;
    int communityId = 0;
    QString name;
    QString category;       // 分区，空 = “一般”
};

// 社区成员（对应 proto 的 MemberInfo）
struct CommunityMember {
    int uid = 0;
    QString name;
    QString role;           // owner / admin / member
    int avatarId = 0;
    QString avatarPath;     // 本地头像文件（AvatarStore）；空 = 默认头像
};

// 一条入群 / 入社区申请（show_group_requests / show_community_requests 返回的文本解析出来的）
struct JoinRequest {
    bool isCommunity = false;   // false = 群聊，true = 社区
    int targetId = 0;           // 群 id 或社区 id
    QString targetName;
    int uid = 0;                // 申请人 UID
    QString note;               // 申请留言，可能为空
};

// 一条收到的好友申请（服务端 show_friend_requests 返回的是文本，由 MainWindow 解析成这个）
struct FriendRequest {
    int uid = 0;            // 申请人 UID
    QString name;
    QString note;           // 申请留言，可能为空
};
