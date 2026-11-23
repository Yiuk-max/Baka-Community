#pragma once

// 社区成员面板（社区页右侧）：按身份（所有者 / 管理员 / 成员）分组列出成员。
// 所有者可以在成员右键菜单里升 / 降管理员、移出社区；面板只发信号，请求由 MainWindow 发。
#include "model/Contact.h"

#include <QHash>
#include <QList>
#include <QWidget>

class AvatarStore;
class QLabel;
class QPoint;
class QVBoxLayout;

class CommunityMembersPanel : public QWidget {
    Q_OBJECT
public:
    explicit CommunityMembersPanel(AvatarStore *avatars, QWidget *parent = nullptr);

    void setMembers(const QList<CommunityMember> &members, int selfUid, bool isOwner);
    void clear();

signals:
    void roleChangeRequested(int uid, bool promote);   // promote = true：设为管理员；false：取消管理员
    void removeRequested(int uid);
    void closed();                                     // 点了面板右上角的 ×

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;   // 成员行右键菜单

private:
    QWidget *makeRow(const CommunityMember &member, bool isOwner);
    void showRowMenu(const CommunityMember &member, const QPoint &globalPos);
    void rebuild();

    AvatarStore *m_avatars = nullptr;
    QLabel *m_count = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QList<CommunityMember> m_members;
    QHash<int, QLabel *> m_avatarLabels;   // uid → 行里的头像，头像下载好后原地换图
    int m_selfUid = 0;
    bool m_isOwner = false;
};
