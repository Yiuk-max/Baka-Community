#pragma once

#include "model/Contact.h"
#include <QDialog>

class CommunityOverviewPage;
class CommunityChannelsPage;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QMouseEvent;

// 与用户设置共用视觉样式；本类仅负责导航、请求状态和回执。
class CommunitySettingsDialog : public QDialog {
    Q_OBJECT
public:
    CommunitySettingsDialog(const Community &community, bool channels, QWidget *parent = nullptr);
    void setChannels(const QList<Channel> &channels);
    void handleSystemText(const QString &text);
    void connectionLost();
    void setAsset(int fileId, const QByteArray &data);
    void assetUploaded(int fileId);
    void assetFailed(const QString &message);
    void reject() override;

signals:
    void saveRequested(const QString &name, const QString &description, int avatarId, int bannerId);
    void createRequested(const QString &name, const QString &category);
    void modifyRequested(int channelId, const QString &name, const QString &category);
    void deleteRequested(int channelId);
    void dissolveRequested();
    void assetUploadRequested(const QByteArray &png, bool banner);
    void assetsReset(int avatarId, int bannerId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool begin(const QString &operation);
    void finish();
    void showMessage(const QString &text, bool error = false);
    bool m_owner;
    bool m_online = true;
    QString m_pending;
    QByteArray m_uploadPng;
    bool m_uploadBanner = false;
    CommunityOverviewPage *m_overview = nullptr;
    CommunityChannelsPage *m_channels = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_message = nullptr;
    QPushButton *m_dissolve = nullptr;
    QTimer *m_timeout = nullptr;
    QTimer *m_messageTimer = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
};
