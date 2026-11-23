#pragma once

// 主页的“待定”页：所有等我处理的申请集中在这里，分三节
//   好友申请（忽略 / 接受）、群聊申请（拒绝 / 接受）、社区申请（拒绝 / 接受）
// 只负责显示和发信号，数据由 MainWindow 喂进来
#include "model/Contact.h"

#include <QList>
#include <QWidget>

class QLabel;
class QVBoxLayout;

class PendingPage : public QWidget {
    Q_OBJECT
public:
    explicit PendingPage(QWidget *parent = nullptr);

    void setFriendRequests(const QList<FriendRequest> &requests);   // 整份替换
    // 替换某一个群 / 社区的申请（服务端是按群、按社区分别回复的）
    void setJoinRequests(bool isCommunity, int targetId, const QList<JoinRequest> &requests);
    void clearJoinRequests(bool isCommunity);   // 群列表 / 社区列表变了，重新拉之前先清掉旧的
    void clear();                               // 退出登录时清空

signals:
    void friendRequestHandled(int uid, bool accept);
    void joinRequestHandled(const JoinRequest &request, bool accept);
    void countChanged(int count);               // 待处理总数变了（主页标签上要显示）

private:
    struct Section {
        QLabel *heading = nullptr;
        QLabel *empty = nullptr;
        QVBoxLayout *rows = nullptr;
    };
    Section addSection(QVBoxLayout *column, const QString &emptyText);
    void rebuild();

    Section m_friendSection, m_groupSection, m_communitySection;
    QList<FriendRequest> m_friends;
    QList<JoinRequest> m_joins;     // 群聊和社区的申请放在一起，用 isCommunity 区分
};
