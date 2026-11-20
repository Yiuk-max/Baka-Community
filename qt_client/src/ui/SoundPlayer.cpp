#include "SoundPlayer.h"

#include "Assets.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QSettings>
#include <QUrl>

namespace {
const char *kVolumeKey = "baka/volume";
const char *kPlayOnLogoKey = "baka/playOnLogo";
}

SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent) {
    // 读上次保存的设置；没存过就用默认值（音量与 web 端默认一致，65%）
    QSettings settings;
    m_volume = qBound(0, settings.value(QLatin1String(kVolumeKey), 65).toInt(), 100);
    m_playOnLogo = settings.value(QLatin1String(kPlayOnLogoKey), true).toBool();

    m_output = new QAudioOutput(this);
    m_output->setVolume(m_volume / 100.0f);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_output);//解码出来的音频，通过这个 QAudioOutput 输出
    m_player->setSource(QUrl(assets::bakaSoundUrl()));

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [](QMediaPlayer::Error, const QString &message) {
        qWarning("[sound] %s", qPrintable(message));
    });
}

void SoundPlayer::setVolume(int percent) {
    m_volume = qBound(0, percent, 100);
    m_output->setVolume(m_volume / 100.0f);
    QSettings().setValue(QLatin1String(kVolumeKey), m_volume);
}

void SoundPlayer::setPlayOnLogo(bool on) {
    m_playOnLogo = on;
    QSettings().setValue(QLatin1String(kPlayOnLogoKey), on);
}

void SoundPlayer::playBaka() {
    m_player->setPosition(0);
    m_player->play();
}
