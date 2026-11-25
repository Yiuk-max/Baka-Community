#include "AttachmentWidget.h"
#include "Assets.h"
#include "FileCover.h"
#include "store/MediaStore.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMouseEvent>
#include <QImageReader>
#include <QLabel>
#include <QThreadPool>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace {
bool isImage(const QString &name) {
    return filecover::isImage(name) || QFileInfo(name).suffix().compare("bmp", Qt::CaseInsensitive) == 0;
}
QThreadPool *imagePool() {
    static QThreadPool pool;
    pool.setMaxThreadCount(1); // 图片解码也有限流，防止翻历史时启动大量解码
    return &pool;
}
}

AttachmentWidget::AttachmentWidget(const Message &message, MediaStore *store, QWidget *parent)
    : QWidget(parent), m_message(message), m_store(store) {
    setObjectName("attachmentWidget");
    setAttribute(Qt::WA_StyledBackground);
    setMaximumWidth(380);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    const bool media = isImage(message.fileName) || filecover::isVideo(message.fileName);
    if (media) {
        m_picture = new QLabel;
        m_picture->setObjectName("attachmentPreview");
        m_picture->setCursor(Qt::PointingHandCursor);
        m_picture->installEventFilter(this);
        m_picture->setMinimumSize(280, 160);
        m_picture->setAlignment(Qt::AlignCenter);
        m_picture->setPixmap(isImage(message.fileName) ? filecover::imagePlaceholder({280, 160})
                                                      : filecover::videoCover({280, 160}));
        layout->addWidget(m_picture);
    }
    auto *info = new QHBoxLayout;
    if (!media) {
        auto *icon = new QLabel;
        icon->setPixmap(filecover::cover(message.fileName, 36));
        info->addWidget(icon);
    }
    auto *name = new QLabel(message.fileName);
    name->setTextFormat(Qt::PlainText);
    name->setToolTip(message.fileName);
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    info->addWidget(name, 1);
    auto *size = new QLabel(message.fileSize ? filecover::formatSize(message.fileSize) : tr("大小未知"));
    size->setObjectName("messageFileSize");
    info->addWidget(size, 0, Qt::AlignRight);
    m_download = new QToolButton;
    m_download->setObjectName("messageFileDownload");
    m_download->setIcon(QIcon(assets::downloadIcon()));
    m_download->setIconSize({20, 20});
    m_download->setToolTip(tr("下载 / 继续下载"));
    m_download->setCursor(Qt::PointingHandCursor);
    info->addWidget(m_download);
    layout->addLayout(info);
    connect(m_download, &QToolButton::clicked, this, [this] {
        const QString path = m_store->localPath(m_message);
        if (!path.isEmpty()) {
            if (m_picture) activate();
            else QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            return;
        }
        m_store->retry(m_message);
        refresh();
    });
    connect(store, &MediaStore::mediaChanged, this, [this](int fileId) {
        if (fileId == m_message.fileId) refresh();
    });
    refresh();
}

void AttachmentWidget::setNearViewport(bool near) {
    if (m_near == near) return;
    m_near = near;
    if (near) {
        m_store->ensure(m_message);
        refresh();
    } else if (m_picture && isImage(m_message.fileName)) {
        ++m_decodeEpoch;
        m_loadedPath.clear();
        m_picture->setPixmap(filecover::imagePlaceholder({280, 160}));
    }
}

void AttachmentWidget::refresh() {
    const QString path = m_store->localPath(m_message);
    const QString error = m_store->error(m_message.fileId);
    m_download->setToolTip(path.isEmpty() ? tr("下载 / 继续下载")
                                         : m_picture ? tr("打开本地媒体") : tr("打开下载文件夹"));
    if (!error.isEmpty()) m_download->setToolTip(error);
    if (m_picture) m_picture->setToolTip(error);
    if (m_near && m_picture && isImage(m_message.fileName) && !path.isEmpty()
            && path != m_loadedPath && !m_decoding) decode(path);
}

void AttachmentWidget::decode(const QString &path) {
    m_decoding = true;
    const int epoch = ++m_decodeEpoch;
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, epoch, path] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        m_decoding = false;
        if (epoch != m_decodeEpoch || !m_near) { if (m_near) refresh(); return; }
        m_loadedPath = path;
        if (image.isNull()) { m_picture->setToolTip(tr("图片无法解码")); return; }
        m_picture->setPixmap(QPixmap::fromImage(image));
    });
    watcher->setFuture(QtConcurrent::run(imagePool(), [path] {
        QImageReader reader(path);
        const QSize size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) return QImage();
        reader.setAutoTransform(true);
        reader.setScaledSize(size.scaled(560, 320, Qt::KeepAspectRatio));
        return reader.read().scaled(280, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }));
}

bool AttachmentWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_picture && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && m_picture->rect().contains(mouse->position().toPoint())) {
            activate();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AttachmentWidget::activate() {
    const QString path = m_store->localPath(m_message);
    if (path.isEmpty()) { m_store->retry(m_message); return; }
    if (isImage(m_message.fileName) || filecover::isVideo(m_message.fileName)) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            m_picture->setToolTip(tr("无法启动系统默认查看器"));
    }
}
