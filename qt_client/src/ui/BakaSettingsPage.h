#pragma once

// 设置 - Baka 页：提示音的音量、双击 logo 是否播放。改了立刻生效并保存（存取都在 SoundPlayer 里）
#include <QWidget>

class SoundPlayer;

class BakaSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit BakaSettingsPage(SoundPlayer *sound, QWidget *parent = nullptr);
};
