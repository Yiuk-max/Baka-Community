#include "SoundPlayer.h"

#include "Assets.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>

SoundPlayer::SoundPlayer(QObject *parent) : QObject(parent) {
    m_output = new QAudioOutput(this);
    m_output->setVolume(0.65f);   // 与 web 端默认音量一致，65%

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_output);//解码出来的音频，通过这个 QAudioOutput 输出
    m_player->setSource(QUrl(assets::bakaSoundUrl()));

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [](QMediaPlayer::Error, const QString &message) {
        qWarning("[sound] %s", qPrintable(message));
    });
}

void SoundPlayer::playBaka() {
    m_player->setPosition(0);
    m_player->play();
}
