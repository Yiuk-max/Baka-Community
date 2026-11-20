#pragma once

// 提示音播放（内置 baka.mp3）
// mp3 要走 QMediaPlayer（QSoundEffect 只认 wav），所以依赖 Qt Multimedia 模块。
#include <QObject>

class QAudioOutput;
class QMediaPlayer;

class SoundPlayer : public QObject {
    Q_OBJECT
public:
    explicit SoundPlayer(QObject *parent = nullptr);

public slots:
    void playBaka();   // 从头播放一次；正在播时再调用会重新开始

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_output = nullptr;
};
