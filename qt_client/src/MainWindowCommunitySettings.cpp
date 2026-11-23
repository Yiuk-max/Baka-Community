// 社区设置请求接线：页面不直接访问网络，图片下载复用头像存储的单连接队列。
#include "MainWindow.h"
#include "net/AvatarUploader.h"
#include "net/ChatClient.h"
#include "net/FileDownloader.h"
#include "store/AvatarStore.h"
#include "ui/CommunityPage.h"
#include "ui/CommunitySettingsDialog.h"

void MainWindow::openCommunitySettings(const Community &community, bool channels) {
    if (m_communityPage->community().id != community.id) openCommunity(community);
    CommunitySettingsDialog dialog(community, channels, this);
    dialog.setChannels(m_communityPage->channels());
    // 强制关闭必须绕过草稿确认，否则被顶下线时会留下模态窗口。
    connect(m_client, &ChatClient::loggedOut, &dialog, [&dialog] { dialog.done(QDialog::Rejected); });
    connect(m_client, &ChatClient::disconnected, &dialog, &CommunitySettingsDialog::connectionLost);
    connect(m_communityPage, &CommunityPage::channelsChanged, &dialog, [this, &dialog, community] {
        if (m_communityPage->community().id == community.id) dialog.setChannels(m_communityPage->channels());
    });
    connect(m_client, &ChatClient::envelopeReceived, &dialog, [&dialog](const chat_proto::Envelope &env) {
        if (env.type() == "system") dialog.handleSystemText(QString::fromStdString(env.content()).trimmed());
    });
    connect(&dialog, &CommunitySettingsDialog::saveRequested, this,
            [this, community](const QString &name, const QString &description, int avatarId, int bannerId) {
        chat_proto::Envelope env;
        env.set_type("modify_community");
        env.set_community_id(community.id);
        env.set_group_name(name.toStdString());
        env.set_description(description.toStdString());
        env.set_community_avatar_id(avatarId);
        env.set_community_banner_id(bannerId);
        m_client->send(env);
    });
    connect(&dialog, &CommunitySettingsDialog::createRequested, this,
            [this, community](const QString &name, const QString &category) {
        chat_proto::Envelope env;
        env.set_type("create_channel");
        env.set_community_id(community.id);
        env.set_group_name(name.toStdString());
        env.set_category(category.toStdString());
        m_client->send(env);
    });
    connect(&dialog, &CommunitySettingsDialog::modifyRequested, this,
            [this, community](int id, const QString &name, const QString &category) {
        chat_proto::Envelope env;
        env.set_type("modify_channel");
        env.set_community_id(community.id);
        env.set_channel_id(id);
        env.set_group_name(name.toStdString());
        env.set_category(category.toStdString());
        m_client->send(env);
    });
    connect(&dialog, &CommunitySettingsDialog::deleteRequested, this, [this, community](int id) {
        chat_proto::Envelope env;
        env.set_type("delete_channel");
        env.set_community_id(community.id);
        env.set_channel_id(id);
        m_client->send(env);
    });
    connect(&dialog, &CommunitySettingsDialog::dissolveRequested, this, [this, community] {
        chat_proto::Envelope env;
        env.set_type("delete_community");
        env.set_community_id(community.id);
        m_client->send(env);
    });
    auto *uploader = new AvatarUploader(m_client, &dialog);
    connect(&dialog, &CommunitySettingsDialog::assetUploadRequested, uploader, &AvatarUploader::uploadImage);
    connect(uploader, &AvatarUploader::finished, &dialog, &CommunitySettingsDialog::assetUploaded);
    connect(uploader, &AvatarUploader::failed, &dialog, &CommunitySettingsDialog::assetFailed);
    auto *downloader = m_avatars->downloader();
    connect(downloader, &FileDownloader::finished, &dialog, &CommunitySettingsDialog::setAsset);
    const auto loadAssets = [downloader](int avatarId, int bannerId) {
        if (avatarId > 0) downloader->download(avatarId);
        if (bannerId > 0) downloader->download(bannerId);
    };
    connect(&dialog, &CommunitySettingsDialog::assetsReset, &dialog, loadAssets);
    loadAssets(community.avatarId, community.bannerId);
    dialog.exec();
}
