#include "MainWindow.h"
#include "net/ChatClient.h"
#include "store/AvatarStore.h"
#include "store/MediaStore.h"
#include "ui/TransferProgressPanel.h"
#include "ui/ContactPanel.h"
#include "ui/ChatPage.h"
#include "ui/HomePage.h"
#include <QStackedWidget>
#include <QGridLayout>
#include <QStatusBar>

void MainWindow::openChat(const Contact &contact) {
    m_contactPanel->showCommunitySidebar(false);
    m_toolRail->setActiveCommunity(0);
    m_chatPage->openConversation(contact, m_client->uid());
    m_center->setCurrentWidget(m_chatPage);
}

void MainWindow::showHome() {
    m_contactPanel->showCommunitySidebar(false);
    m_toolRail->setActiveCommunity(0);
    m_contactPanel->clearSelection();
    m_center->setCurrentWidget(m_homePage);
}

void MainWindow::initializeAttachments() {
    m_media = new MediaStore(m_client, m_avatars->downloader(), this);
    connect(m_media, &MediaStore::errorOccurred, this, [this](const QString &text) {
        statusBar()->showMessage(text, 7000);
    });
}

QWidget *MainWindow::wrapAttachments(QWidget *content) {
    auto *host = new QWidget;
    auto *grid = new QGridLayout(host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->addWidget(content, 0, 0);
    // 同一格子的对齐布局覆盖，不使用绝对坐标，也不创建会抢焦点的顶层弹窗。
    m_transfers = new TransferProgressPanel(m_media, host);
    auto *corner = new QVBoxLayout;
    corner->setContentsMargins(12, 12, 20, 24);
    corner->addWidget(m_transfers);
    grid->addLayout(corner, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    connect(m_client, &ChatClient::loggedOut, m_transfers, &TransferProgressPanel::clear);
    return host;
}
