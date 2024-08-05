#include <QDebug>
#include <QTime>
#include <QSettings>

#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include "globalhelper.h"
#include "mainwid.h"

// CtrlBar类的构造函数
CtrlBar::CtrlBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CtrlBar)
{
    ui->setupUi(this);

    m_dLastVolumePercent = 1.0; // 初始化最后的音量百分比
}

// CtrlBar类的析构函数
CtrlBar::~CtrlBar()
{
    delete ui;
}

// 初始化CtrlBar
bool CtrlBar::Init()
{
    // 设置控件的样式表
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/ctrlbar.css"));

    // 设置按钮图标
    GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    GlobalHelper::SetIcon(ui->StopBtn, 12, QChar(0xf04d));
    GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
    GlobalHelper::SetIcon(ui->PlaylistCtrlBtn, 12, QChar(0xf036));
    GlobalHelper::SetIcon(ui->ForwardBtn, 12, QChar(0xf051));
    GlobalHelper::SetIcon(ui->BackwardBtn, 12, QChar(0xf048));
    GlobalHelper::SetIcon(ui->SettingBtn, 12, QChar(0xf013));

    // 设置按钮提示信息
    ui->PlaylistCtrlBtn->setToolTip("播放列表");
    ui->SettingBtn->setToolTip("设置");
    ui->VolumeBtn->setToolTip("静音");
    ui->ForwardBtn->setToolTip("下一个");
    ui->BackwardBtn->setToolTip("上一个");
    ui->StopBtn->setToolTip("停止");
    ui->PlayOrPauseBtn->setToolTip("播放");
    ui->speedBtn->setToolTip("倍速");

    // 连接信号和槽
    ConnectSignalSlots();

    // 初始化音量
    double dPercent = -1.0;
    GlobalHelper::GetPlayVolume(dPercent);
    if (dPercent != -1.0)
    {
        emit SigPlayVolume(dPercent);
        OnVideopVolume(dPercent);
    }

    return true;
}

// 连接信号和槽
bool CtrlBar::ConnectSignalSlots()
{
    // 将控件的信号连接到相应的槽函数
    connect(ui->PlaylistCtrlBtn, &QPushButton::clicked, this, &CtrlBar::SigShowOrHidePlaylist);
    connect(ui->PlaySlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnPlaySliderValueChanged);
    connect(ui->VolumeSlider, &CustomSlider::SigCustomSliderValueChanged, this, &CtrlBar::OnVolumeSliderValueChanged);
    connect(ui->BackwardBtn, &QPushButton::clicked, this, &CtrlBar::SigBackwardPlay);
    connect(ui->ForwardBtn, &QPushButton::clicked, this, &CtrlBar::SigForwardPlay);
    return true;
}

// 设置视频的总播放时间
void CtrlBar::OnVideoTotalSeconds(int nSeconds)
{
    m_nTotalPlaySeconds = nSeconds;

    // 计算小时、分钟和秒
    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    // 设置总时间显示
    ui->VideoTotalTimeTimeEdit->setTime(TotalTime);
}

// 设置视频的当前播放时间
void CtrlBar::OnVideoPlaySeconds(int nSeconds)
{
    int thh, tmm, tss;
    thh = nSeconds / 3600;
    tmm = (nSeconds % 3600) / 60;
    tss = (nSeconds % 60);
    QTime TotalTime(thh, tmm, tss);

    // 设置当前播放时间显示
    ui->VideoPlayTimeTimeEdit->setTime(TotalTime);

    // 更新播放滑块的值
    ui->PlaySlider->setValue(nSeconds * 1.0 / m_nTotalPlaySeconds * MAX_SLIDER_VALUE);
}

// 设置视频音量
void CtrlBar::OnVideopVolume(double dPercent)
{
    ui->VolumeSlider->setValue(dPercent * MAX_SLIDER_VALUE);
    m_dLastVolumePercent = dPercent;

    // 根据音量百分比设置音量按钮的图标
    if (m_dLastVolumePercent == 0)
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
    }
    else
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
    }

    // 保存音量设置
    GlobalHelper::SavePlayVolume(dPercent);
}

// 设置播放/暂停按钮的状态
void CtrlBar::OnPauseStat(bool bPaused)
{
    qDebug() << "CtrlBar::OnPauseStat" << bPaused;
    if (bPaused)
    {
        GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
        ui->PlayOrPauseBtn->setToolTip("播放");
    }
    else
    {
        GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04c));
        ui->PlayOrPauseBtn->setToolTip("暂停");
    }
}

// 停止播放时的处理
void CtrlBar::OnStopFinished()
{
    ui->PlaySlider->setValue(0);
    QTime StopTime(0, 0, 0);
    ui->VideoTotalTimeTimeEdit->setTime(StopTime);
    ui->VideoPlayTimeTimeEdit->setTime(StopTime);
    GlobalHelper::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    ui->PlayOrPauseBtn->setToolTip("播放");
}

// 设置播放速度
void CtrlBar::OnSpeed(float speed)
{
    // 更新速度按钮的文本
    ui->speedBtn->setText(QString("倍速:%1").arg(speed));
}

// 播放滑块值改变时的处理
void CtrlBar::OnPlaySliderValueChanged()
{
    double dPercent = ui->PlaySlider->value() * 1.0 / ui->PlaySlider->maximum();
    emit SigPlaySeek(dPercent);
}

// 音量滑块值改变时的处理
void CtrlBar::OnVolumeSliderValueChanged()
{
    double dPercent = ui->VolumeSlider->value() * 1.0 / ui->VolumeSlider->maximum();
    emit SigPlayVolume(dPercent);

    OnVideopVolume(dPercent);
}

// 播放/暂停按钮点击事件的处理
void CtrlBar::on_PlayOrPauseBtn_clicked()
{
    emit SigPlayOrPause();
}

// 音量按钮点击事件的处理
void CtrlBar::on_VolumeBtn_clicked()
{
    if (ui->VolumeBtn->text() == QChar(0xf028))
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
        ui->VolumeSlider->setValue(0);
        emit SigPlayVolume(0);
    }
    else
    {
        GlobalHelper::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
        ui->VolumeSlider->setValue(m_dLastVolumePercent * MAX_SLIDER_VALUE);
        emit SigPlayVolume(m_dLastVolumePercent);
    }
}

// 停止按钮点击事件的处理
void CtrlBar::on_StopBtn_clicked()
{
    emit SigStop();
}

// 设置按钮点击事件的处理
void CtrlBar::on_SettingBtn_clicked()
{
    //emit SigShowSetting(); // 注释掉，因为没有实现
}

// 倍速按钮点击事件的处理
void CtrlBar::on_speedBtn_clicked()
{
    // 设置变速
    emit SigSpeed();
}

