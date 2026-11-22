#include "MainWindow.h"
#include "net/FileDownloader.h"
#include "store/AvatarStore.h"
#include "ui/Assets.h"
#include "ui/CommunityPage.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>

namespace {
QPixmap rounded(const QPixmap &source, int size, int radius) {
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
    painter.setClipPath(clip);
    const QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    return result;
}
}

void MainWindow::wireCommunityAssets() {
    connect(m_avatars->downloader(), &FileDownloader::finished, this, [this](int fileId, const QByteArray &data) {
        bool relevant = false;
        for (const auto &community : m_communities) {
            if (community.avatarId == fileId || community.bannerId == fileId) relevant = true;
        }
        if (!relevant) return;
        QBuffer buffer;
        buffer.setData(data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) return;
        reader.setScaledSize(size.scaled(1600, 900, Qt::KeepAspectRatio));
        const QImage image = reader.read();
        if (image.isNull()) return;
        auto *pixmap = new QPixmap(QPixmap::fromImage(image));
        const int cost = qMax(1, pixmap->width() * pixmap->height() * 4 / 1024);
        m_communityImages.insert(fileId, pixmap, cost);
        refreshCommunityAssets();
    });
}

void MainWindow::refreshCommunityAssets() {
    const QPixmap defaultAvatar(assets::defaultCommunityAvatar());
    const QPixmap defaultBanner(assets::defaultBackground());
    const int openId = m_communityPage->community().id;
    for (const auto &community : m_communities) {
        auto *avatar = m_communityImages.object(community.avatarId);
        m_toolRail->setCommunityAvatar(community.id, rounded(avatar ? *avatar : defaultAvatar, 48, 16));
        if (community.id == openId) {
            auto *banner = m_communityImages.object(community.bannerId);
            m_communityPage->setAssets(avatar ? *avatar : defaultAvatar, banner ? *banner : defaultBanner);
        }
        for (int id : {community.avatarId, community.bannerId}) {
            if (id > 0 && !m_communityImageRequests.contains(id)) {
                m_communityImageRequests.insert(id);
                m_avatars->downloader()->download(id);
            }
        }
    }
}
