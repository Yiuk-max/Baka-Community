#include "TransferProgressPanel.h"
#include "FileCover.h"
#include "net/FileDownloader.h"
#include "store/MediaStore.h"
#include <QFrame>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

TransferProgressPanel::TransferProgressPanel(MediaStore *store, QWidget *parent)
    : QWidget(parent), m_downloader(store->downloader()) {
    setFocusPolicy(Qt::NoFocus);
    parent->installEventFilter(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("transferScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget;
    body->setObjectName("transferBody");
    m_list = new QVBoxLayout(body);
    m_list->setContentsMargins(0, 0, 4, 0);
    m_list->setSpacing(8);
    m_list->addStretch();
    m_scroll->setWidget(body);
    layout->addWidget(m_scroll);
    hide();
    connect(store, &MediaStore::taskChanged, this, &TransferProgressPanel::updateTask);
}

void TransferProgressPanel::updateTask(const DownloadTask &task) {
    if (task.silent) return;
    if (!m_cards.contains(task.taskId)) {
        Card card{};
        auto *frame = new QFrame;
        frame->setObjectName("transferCard");
        card.widget = frame;
        auto *outer = new QVBoxLayout(frame);
        outer->setContentsMargins(12, 10, 12, 10);
        outer->setSpacing(8);
        auto *row = new QHBoxLayout;
        card.icon = new QLabel(QStringLiteral("↓"));
        card.icon->setObjectName("transferIcon");
        row->addWidget(card.icon);
        auto *text = new QVBoxLayout;
        card.name = new QLabel;
        card.name->setTextFormat(Qt::PlainText);
        card.name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        card.detail = new QLabel;
        card.detail->setObjectName("transferDetail");
        card.detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        card.detail->setTextFormat(Qt::PlainText);
        text->addWidget(card.name);
        text->addWidget(card.detail);
        row->addLayout(text, 1);
        auto *close = new QPushButton(QStringLiteral("×"));
        close->setObjectName("transferClose");
        close->setMaximumWidth(24);
        close->setFocusPolicy(Qt::NoFocus);
        close->setCursor(Qt::PointingHandCursor);
        row->addWidget(close);
        outer->addLayout(row);
        card.bar = new QProgressBar;
        card.bar->setObjectName("transferBar");
        card.bar->setRange(0, 100);
        card.bar->setTextVisible(false);
        card.bar->setMaximumHeight(4);
        outer->addWidget(card.bar);
        m_list->insertWidget(0, frame);
        m_cards.insert(task.taskId, card);
        connect(close, &QPushButton::clicked, this, [this, id = task.taskId] {
            if (!m_cards.contains(id)) return;
            if (m_cards.value(id).task.isTerminal()) removeCard(id);
            else m_downloader->cancel(id);
        });
    }
    Card &card = m_cards[task.taskId];
    const bool wasTerminal = card.task.isTerminal();
    card.task = task;
    card.name->setText(task.name);
    card.name->setToolTip(task.name);
    card.detail->setToolTip(task.error);
    card.detail->setText(task.isTerminal() ? task.stateText()
        : tr("%1 · %2 / %3 · %4% ").arg(task.stateText(), filecover::formatSize(task.transferred),
                                              filecover::formatSize(task.totalSize))
                                             .arg(task.progressPercent()));
    const QString status = task.state == DownloadTask::State::Completed ? QStringLiteral("done")
        : task.state == DownloadTask::State::Failed || task.state == DownloadTask::State::Canceled
            ? QStringLiteral("failed") : QStringLiteral("active");
    card.icon->setText(status == QLatin1String("done") ? QStringLiteral("✓")
                          : status == QLatin1String("failed") ? QStringLiteral("✕") : QStringLiteral("↓"));
    card.widget->setProperty("status", status);
    card.bar->setProperty("status", status);
    for (QWidget *widget : {card.widget, static_cast<QWidget *>(card.bar)}) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    card.bar->setValue(task.progressPercent());
    resizePanel();
    if (task.isTerminal() && !wasTerminal) {
        QTimer::singleShot(3000, this, [this, id = task.taskId] {
            if (m_cards.contains(id) && m_cards.value(id).task.isTerminal()) removeCard(id);
        });
    }
}

void TransferProgressPanel::resizePanel() {
    setMaximumHeight(qMax(100, parentWidget()->height() - 60));
    setMinimumWidth(qMin(340, qMax(180, parentWidget()->width() - 24)));
    setMaximumWidth(minimumWidth());
    setMinimumHeight(qMin(maximumHeight(), qMax(1, m_cards.size()) * 98));
    setVisible(!m_cards.isEmpty());
    raise();
}
void TransferProgressPanel::removeCard(const QString &id) {
    if (!m_cards.contains(id)) return;
    auto card = m_cards.take(id);
    m_list->removeWidget(card.widget);
    card.widget->deleteLater();
    resizePanel();
}
bool TransferProgressPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) resizePanel();
    return QWidget::eventFilter(watched, event);
}

void TransferProgressPanel::clear() {
    const auto keys = m_cards.keys();
    for (const auto &id : keys) removeCard(id);
}
