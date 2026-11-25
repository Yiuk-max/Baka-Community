#include "BakaSettingsPage.h"

#include "Assets.h"
#include "SoundPlayer.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {

// 一行的外框：左边是标题 + 一行小字说明，右边的控件由调用方自己加进返回的布局
QHBoxLayout *makeRow(QVBoxLayout *pageLayout, const QString &title, QLabel *detail) {
    auto *row = new QWidget;
    row->setObjectName("settingsRow");
    row->setAttribute(Qt::WA_StyledBackground);
    row->setFixedHeight(64);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(16);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addStretch();
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName("settingsRowValue");
    text->addWidget(titleLabel);
    detail->setObjectName("settingsRowName");
    text->addWidget(detail);
    text->addStretch();
    layout->addLayout(text, 1);

    pageLayout->addWidget(row);
    return layout;
}

} // namespace

BakaSettingsPage::BakaSettingsPage(SoundPlayer *sound, QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *heading = new QLabel(tr("Baka 音效"));
    heading->setObjectName("settingsHeading");
    layout->addWidget(heading);
    layout->addSpacing(4);

    // 音量：拖动时实时生效，松手时播一声试听
    auto *volumeText = new QLabel(tr("%1%").arg(sound->volume()));
    auto *volumeRow = makeRow(layout, tr("音量"), volumeText);
    auto *slider = new QSlider(Qt::Horizontal);//水平滑块 QSlider
    slider->setObjectName("volumeSlider");
    slider->setRange(0, 100);
    slider->setValue(sound->volume());
    slider->setFixedWidth(200);
    slider->setCursor(Qt::PointingHandCursor);
    volumeRow->addWidget(slider);

    // 双击 logo 是否播放：滑动开关（开 / 关两张图，和 web 端 .toggle 同样大小）
    auto *logoRow = makeRow(layout, tr("双击 LOGO 播放"), new QLabel(tr("双击左侧竖栏顶部的 LOGO 播放一次。")));
    QIcon toggleIcon;
    toggleIcon.addPixmap(QPixmap(assets::toggleOff()), QIcon::Normal, QIcon::Off);
    toggleIcon.addPixmap(QPixmap(assets::toggleOn()), QIcon::Normal, QIcon::On);
    auto *toggle = new QPushButton;
    toggle->setObjectName("toggleSwitch");
    toggle->setCheckable(true);
    toggle->setChecked(sound->playOnLogo());
    toggle->setIcon(toggleIcon);            // 选中时 Qt 自动换成 On 那张
    toggle->setIconSize(QSize(60, 30));
    toggle->setFixedSize(60, 30);
    toggle->setCursor(Qt::PointingHandCursor);
    logoRow->addWidget(toggle);

    layout->addStretch();

    connect(slider, &QSlider::valueChanged, this, [sound, volumeText](int value) {
        sound->setVolume(value);
        volumeText->setText(tr("%1%").arg(value));
    });
    connect(slider, &QSlider::sliderReleased, sound, &SoundPlayer::playBaka);
    connect(toggle, &QPushButton::toggled, sound, &SoundPlayer::setPlayOnLogo);
}
