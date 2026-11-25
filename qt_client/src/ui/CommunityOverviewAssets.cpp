#include "CommunityOverviewPage.h"
#include "AvatarCropDialog.h"

#include <QBuffer>
#include <QFileDialog>
#include <QImageReader>
#include <QLabel>
#include <QDialog>
#include <QPainter>
#include <QPainterPath>

void CommunityOverviewPage::chooseAsset(bool banner) {
    const QString path = QFileDialog::getOpenFileName(this, banner ? tr("选择背景") : tr("选择头像"),
        QString(), tr("图片 (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) return;
    QByteArray png;
    if (banner) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) {
            emit imageError(tr("图片无效或过大，请换一张"));
            return;
        }
        const auto image = reader.read();
        if (image.isNull()) { emit imageError(tr("无法读取图片")); return; }
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.scaled(1600, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG");
    } else {
        AvatarCropDialog crop(path, this);
        if (auto *dialog = qobject_cast<QDialog *>(window()))
            connect(dialog, &QDialog::finished, &crop, &QDialog::reject);
        if (!crop.isValid()) { emit imageError(tr("无法读取图片")); return; }
        if (crop.exec() != QDialog::Accepted) return;
        png = crop.croppedPng();
    }
    if (png.isEmpty()) { emit imageError(tr("图片处理失败，请重试")); return; }
    if (isEnabled()) emit uploadRequested(png, banner);
}

void CommunityOverviewPage::applyUpload(int fileId, const QByteArray &data, bool banner) {
    if (banner) m_bannerId = fileId;
    else m_avatarId = fileId;
    setAsset(fileId, data);
    updateActions();
}

void CommunityOverviewPage::setAsset(int fileId, const QByteArray &data) {
    if (fileId != m_avatarId && fileId != m_bannerId) return;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) return;
    reader.setScaledSize(size.scaled(1600, 900, Qt::KeepAspectRatio));
    const auto image = QPixmap::fromImage(reader.read());
    if (image.isNull()) return;
    if (fileId == m_avatarId) m_avatarPixmap = image;
    if (fileId == m_bannerId) m_bannerPixmap = image;
    if (fileId == m_saved.avatarId) m_savedAvatar = image;
    if (fileId == m_saved.bannerId) m_savedBanner = image;
    refreshPreview();
}

void CommunityOverviewPage::refreshPreview() {
    const auto thumbnail = [](const QPixmap &source, const QSize &size) {
        QPixmap result(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(QPointF(), QSizeF(size)), 8, 8);
        painter.setClipPath(clip);
        const auto scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
        return result;
    };
    m_avatar->setPixmap(thumbnail(m_avatarPixmap, QSize(56, 56)));
    m_banner->setPixmap(thumbnail(m_bannerPixmap, QSize(100, 56)));
}
