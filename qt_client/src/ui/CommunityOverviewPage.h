#pragma once

#include "model/Contact.h"
#include <QWidget>
#include <QPixmap>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// 社区资料草稿：一屏编辑，只有修改后才出现保存栏。
class CommunityOverviewPage : public QWidget {
    Q_OBJECT
public:
    explicit CommunityOverviewPage(const Community &community, QWidget *parent = nullptr);
    Community draft() const;
    bool isDirty() const;
    void saved();
    void reset();
    void setAsset(int fileId, const QByteArray &data);
    void applyUpload(int fileId, const QByteArray &data, bool banner);

signals:
    void saveRequested();
    void uploadRequested(const QByteArray &png, bool banner);
    void assetsReset(int avatarId, int bannerId);
    void imageError(const QString &message);

private:
    void updateActions();
    void chooseAsset(bool banner);
    void refreshPreview();
    Community m_saved;
    int m_avatarId;
    int m_bannerId;
    QPixmap m_avatarPixmap;
    QPixmap m_bannerPixmap;
    QPixmap m_savedAvatar;
    QPixmap m_savedBanner;
    QLineEdit *m_name = nullptr;
    QPlainTextEdit *m_description = nullptr;
    QLabel *m_count = nullptr;
    QLabel *m_avatar = nullptr;
    QLabel *m_banner = nullptr;
    QWidget *m_saveBar = nullptr;
    QPushButton *m_save = nullptr;
};
