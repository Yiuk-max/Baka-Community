// ChatPage 的可见区媒体预取；隐藏页不排队预取，远离视口释放图片解码结果。
#include "ChatPage.h"
#include "MessageRow.h"
#include <QHideEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>

void ChatPage::scheduleMedia() {
    if (m_mediaTimer && isVisible() && !m_mediaTimer->isActive()) m_mediaTimer->start();
}

void ChatPage::updateVisibleMedia() {
    const auto *viewport = m_scroll->viewport();
    const int height = viewport->height();
    const QRect nearRect(0, -height, viewport->width(), height * 3);
    for (MessageRow *row : std::as_const(m_rows)) {
        const QPoint pos = row->mapTo(viewport, QPoint());
        row->setAttachmentNearViewport(isVisible() && nearRect.intersects(QRect(pos, row->size())));
    }
}

void ChatPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    scheduleMedia();
}
void ChatPage::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (m_mediaTimer) m_mediaTimer->stop();
    for (auto *row : std::as_const(m_rows)) row->setAttachmentNearViewport(false);
}
void ChatPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    scheduleMedia();
}

void ChatPage::onConnectionLost() {
    m_loading = false;          // 历史请求在途时断线，解除 loading，恢复后可以重拉
    m_pendingIds.clear();       // 旧连接的“发送成功”回执不能再确认上一批乐观消息
}

void ChatPage::onSessionRestored() {
    if (m_conv.uid == 0) return;
    // 重新校准当前会话的最新一页；输入框草稿保留，未确认的乐观消息在重拉后由服务端数据覆盖
    openConversation(m_conv, m_selfUid);
}
