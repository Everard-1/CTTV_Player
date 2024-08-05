#include <QFile>
#include <QPainter>
#include <QtMath>
#include <QDebug>
#include <QAbstractItemView>
#include <QMimeData>
#include <QSizeGrip>
#include <QWindow>
#include <QDesktopWidget>
#include <QScreen>
#include <QRect>
#include <QFileDialog>

#include "mainwid.h"
#include "ui_mainwid.h"
#include "globalhelper.h"
#include "videoctl.h"

const int FULLSCREEN_MOUSE_DETECT_TIME = 500;

//构造函数主要初始化了窗口的各种属性、控件和状态
MainWid::MainWid(QMainWindow *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWid),
    m_nShadowWidth(0),
    m_stPlaylist(this),
    m_stTitle(this),
    m_bMoveDrag(false),
    m_stMenu(this),
    m_stActExit(this),
    m_stActAbout(this),
    m_stActOpen(this),
    m_stActFullscreen(this)
{
    ui->setupUi(this);
    //无边框、无系统菜单、 任务栏点击最小化
    setWindowFlags(Qt::FramelessWindowHint /*| Qt::WindowSystemMenuHint*/ | Qt::WindowMinimizeButtonHint);
    //设置任务栏图标
    this->setWindowIcon(QIcon("://res/player.png"));
    //加载样式
    QString qss = GlobalHelper::GetQssStr("://res/qss/mainwid.css");
    setStyleSheet(qss);

    // 追踪鼠标 用于播放时隐藏鼠标
    this->setMouseTracking(true);

    //ui->ShowWid->setMouseTracking(true);

    //保证窗口不被绘制上的部分透明
    //setAttribute(Qt::WA_TranslucentBackground);

    //接受放下事件
    //setAcceptDrops(true);
    //可以清晰地看到放下过程中的图标指示
    //setDropIndicatorShown(true);

    //    setAcceptDrops(true);
    //    setDragDropMode(QAbstractItemView::DragDrop);
    //    setDragEnabled(true);
    //    setDropIndicatorShown(true);

    //窗口大小调节
    //    QSizeGrip   *pSizeGrip = new QSizeGrip(this);
    //    pSizeGrip->setMinimumSize(10, 10);
    //    pSizeGrip->setMaximumSize(10, 10);
    //    ui->verticalLayout->addWidget(pSizeGrip, 0, Qt::AlignBottom | Qt::AlignRight);

    //初始化播放状态
    m_bPlaying = false;
    //全屏播放
    m_bFullScreenPlay = false;
    //设置控制面板动画计时器和全屏鼠标检测计时器的间隔时间。
    m_stCtrlBarAnimationTimer.setInterval(2000);
    m_stFullscreenMouseDetectTimer.setInterval(FULLSCREEN_MOUSE_DETECT_TIME);
}

//析构函数释放窗口
MainWid::~MainWid()
{
    delete ui;
}

//Init 函数负责初始化 MainWid 窗口的各个组件，包括设置标题栏部件、初始化自定义控件、连接信号和槽、创建动画以及设置菜单操作
bool MainWid::Init()
{
    QWidget *em = new QWidget(this);
    ui->PlaylistWid->setTitleBarWidget(em);
    ui->PlaylistWid->setWidget(&m_stPlaylist);
    //ui->PlaylistWid->setFixedWidth(100);

    QWidget *emTitle = new QWidget(this);
    ui->TitleWid->setTitleBarWidget(emTitle);
    ui->TitleWid->setWidget(&m_stTitle);
    
//         FramelessHelper *pHelper = new FramelessHelper(this); //无边框管理
//         pHelper->activateOn(this);  //激活当前窗体
//         pHelper->setTitleHeight(ui->TitleWid->height());  //设置窗体的标题栏高度
//         pHelper->setWidgetMovable(true);  //设置窗体可移动
//         pHelper->setWidgetResizable(true);  //设置窗体可缩放
//         pHelper->setRubberBandOnMove(true);  //设置橡皮筋效果-可移动
//         pHelper->setRubberBandOnResize(true);  //设置橡皮筋效果-可缩放

    //连接自定义信号与槽
    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    if (ui->CtrlBarWid->Init() == false ||
            m_stPlaylist.Init() == false ||
            ui->ShowWid->Init() == false ||
            m_stTitle.Init() == false)
    {
        return false;
    }

    //创建控制面板的显示和隐藏动画
    m_stCtrlbarAnimationShow = new QPropertyAnimation(ui->CtrlBarWid, "geometry");
    m_stCtrlbarAnimationHide = new QPropertyAnimation(ui->CtrlBarWid, "geometry");
    //初始化关于窗口
    if (m_stAboutWidget.Init() == false)
    {
        return false;
    }
    //设置全屏操作的文本，并将其设置为可选，添加到菜单中
    m_stActFullscreen.setText("全屏播放");
    m_stActFullscreen.setCheckable(true);
    m_stMenu.addAction(&m_stActFullscreen);

    m_stActOpen.setText("打开文件");
    m_stMenu.addAction(&m_stActOpen);

    m_stActAbout.setText("关于我们");
    m_stMenu.addAction(&m_stActAbout);
    
    m_stActExit.setText("退出播放");
    m_stMenu.addAction(&m_stActExit);

    return true;
}

//绘制事件
void MainWid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

//进入事件
void MainWid::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
}

//离开事件
void MainWid::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

}

//ConnectSignalSlots 函数通过 connect 函数将各种信号连接到相应的槽函数，确保主窗口、标题栏、显示窗口、控制栏、播放列表和视频控制器之间的交互正常工作
bool MainWid::ConnectSignalSlots()
{
    //连接标题栏和信号槽
    connect(&m_stTitle, &Title::SigCloseBtnClicked, this, &MainWid::OnCloseBtnClicked);
    connect(&m_stTitle, &Title::SigMaxBtnClicked, this, &MainWid::OnMaxBtnClicked);
    connect(&m_stTitle, &Title::SigMinBtnClicked, this, &MainWid::OnMinBtnClicked);
    connect(&m_stTitle, &Title::SigDoubleClicked, this, &MainWid::OnMaxBtnClicked);
    connect(&m_stTitle, &Title::SigFullScreenBtnClicked, this, &MainWid::OnFullScreenPlay);
    connect(&m_stTitle, &Title::SigOpenFile, &m_stPlaylist, &Playlist::OnAddFileAndPlay);
    connect(&m_stTitle, &Title::SigShowMenu, this, &MainWid::OnShowMenu);
    
    //连接播放列表的播放信号到显示窗口的播放槽
    connect(&m_stPlaylist, &Playlist::SigPlay, ui->ShowWid, &Show::SigPlay);

    //连接处理显示窗口的各种操作信号，调用视频控制器或其他相关槽函数
    connect(ui->ShowWid, &Show::SigOpenFile, &m_stPlaylist, &Playlist::OnAddFileAndPlay);
    connect(ui->ShowWid, &Show::SigFullScreen, this, &MainWid::OnFullScreenPlay);
    connect(ui->ShowWid, &Show::SigPlayOrPause, VideoCtl::GetInstance(), &VideoCtl::OnPause);
    connect(ui->ShowWid, &Show::SigStop, VideoCtl::GetInstance(), &VideoCtl::OnStop);
    connect(ui->ShowWid, &Show::SigShowMenu, this, &MainWid::OnShowMenu);
    connect(ui->ShowWid, &Show::SigSeekForward, VideoCtl::GetInstance(), &VideoCtl::OnSeekForward);
    connect(ui->ShowWid, &Show::SigSeekBack, VideoCtl::GetInstance(), &VideoCtl::OnSeekBack);
    connect(ui->ShowWid, &Show::SigAddVolume, VideoCtl::GetInstance(), &VideoCtl::OnAddVolume);
    connect(ui->ShowWid, &Show::SigSubVolume, VideoCtl::GetInstance(), &VideoCtl::OnSubVolume);

    //连接处理控制栏的各种操作信号，调用视频控制器、播放列表或其他相关槽函数
    connect(ui->CtrlBarWid, &CtrlBar::SigSpeed, VideoCtl::GetInstance(), &VideoCtl::OnSpeed);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowOrHidePlaylist, this, &MainWid::OnShowOrHidePlaylist);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlaySeek, VideoCtl::GetInstance(), &VideoCtl::OnPlaySeek);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlayVolume, VideoCtl::GetInstance(), &VideoCtl::OnPlayVolume);
    connect(ui->CtrlBarWid, &CtrlBar::SigPlayOrPause, VideoCtl::GetInstance(), &VideoCtl::OnPause);
    connect(ui->CtrlBarWid, &CtrlBar::SigStop, VideoCtl::GetInstance(), &VideoCtl::OnStop);
    connect(ui->CtrlBarWid, &CtrlBar::SigBackwardPlay, &m_stPlaylist, &Playlist::OnBackwardPlay);
    connect(ui->CtrlBarWid, &CtrlBar::SigForwardPlay, &m_stPlaylist, &Playlist::OnForwardPlay);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowMenu, this, &MainWid::OnShowMenu);
    connect(ui->CtrlBarWid, &CtrlBar::SigShowSetting, this, &MainWid::OnShowSettingWid);

    //连接处理主窗口的各种操作信号，调用视频控制器、播放列表或标题栏的相关槽函数
    connect(this, &MainWid::SigShowMax, &m_stTitle, &Title::OnChangeMaxBtnStyle);
    connect(this, &MainWid::SigSeekForward, VideoCtl::GetInstance(), &VideoCtl::OnSeekForward);
    connect(this, &MainWid::SigSeekBack, VideoCtl::GetInstance(), &VideoCtl::OnSeekBack);
    connect(this, &MainWid::SigAddVolume, VideoCtl::GetInstance(), &VideoCtl::OnAddVolume);
    connect(this, &MainWid::SigSubVolume, VideoCtl::GetInstance(), &VideoCtl::OnSubVolume);
    connect(this, &MainWid::SigOpenFile, &m_stPlaylist, &Playlist::OnAddFileAndPlay);
    
    //连接处理视频控制器的各种信号，调用控制栏、显示窗口或标题栏的相关槽函数
    connect(VideoCtl::GetInstance(), &VideoCtl::SigSpeed, ui->CtrlBarWid, &CtrlBar::OnSpeed);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoTotalSeconds, ui->CtrlBarWid, &CtrlBar::OnVideoTotalSeconds);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoPlaySeconds, ui->CtrlBarWid, &CtrlBar::OnVideoPlaySeconds);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigVideoVolume, ui->CtrlBarWid, &CtrlBar::OnVideopVolume);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigPauseStat, ui->CtrlBarWid, &CtrlBar::OnPauseStat, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStopFinished, ui->CtrlBarWid, &CtrlBar::OnStopFinished, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStopFinished, ui->ShowWid, &Show::OnStopFinished, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigFrameDimensionsChanged, ui->ShowWid, &Show::OnFrameDimensionsChanged, Qt::QueuedConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStopFinished, &m_stTitle, &Title::OnStopFinished, Qt::DirectConnection);
    connect(VideoCtl::GetInstance(), &VideoCtl::SigStartPlay, &m_stTitle, &Title::OnPlay, Qt::DirectConnection);

    //连接控制栏动画计时器的超时信号，调用 OnCtrlBarAnimationTimeOut 槽函数
    connect(&m_stCtrlBarAnimationTimer, &QTimer::timeout, this, &MainWid::OnCtrlBarAnimationTimeOut);

    //连接全屏鼠标检测计时器的超时信号，调用 OnFullscreenMouseDetectTimeOut 槽函数
    connect(&m_stFullscreenMouseDetectTimer, &QTimer::timeout, this, &MainWid::OnFullscreenMouseDetectTimeOut);

    //连接处理菜单操作的触发信号，调用相应的槽函数
    connect(&m_stActAbout, &QAction::triggered, this, &MainWid::OnShowAbout);
    connect(&m_stActFullscreen, &QAction::triggered, this, &MainWid::OnFullScreenPlay);
    connect(&m_stActExit, &QAction::triggered, this, &MainWid::OnCloseBtnClicked);
    connect(&m_stActOpen, &QAction::triggered, this, &MainWid::OpenFile);
    
    return true;
}

//处理键盘事件。这些事件包括全屏、前后跳转、音量调整和播放/暂停
void MainWid::keyReleaseEvent(QKeyEvent *event)
{
    // 	    // 是否按下Ctrl键      特殊按键
    //     if(event->modifiers() == Qt::ControlModifier){
    //         // 是否按下M键    普通按键  类似
    //         if(event->key() == Qt::Key_M)
    //             ···
    //     }
    qDebug() << "MainWid::keyPressEvent:" << event->key();
    switch (event->key())
    {
    case Qt::Key_Return://全屏
        OnFullScreenPlay();
        break;
//    case Qt::Key_Escape://退出全屏
//        OnFullScreenPlay();
        break;
    case Qt::Key_Left://后退5s
        emit SigSeekBack();
        break;
    case Qt::Key_Right://前进5s
        qDebug() << "前进5s";
        emit SigSeekForward();
        break;
    case Qt::Key_Up://增加10音量
        emit SigAddVolume();
        break;
    case Qt::Key_Down://减少10音量
        emit SigSubVolume();
        break;
    case Qt::Key_Space://暂停播放
        emit SigPlayOrPause();
        break; 
    default:
        break;
    }
}

//处理鼠标按下事件，特别是用于窗口的拖动操作
void MainWid::mousePressEvent(QMouseEvent *event)
{
    // 检查是否按下了左键
    if (event->buttons() & Qt::LeftButton)
    {
        // 检查鼠标点击位置是否在标题栏区域内
        if (ui->TitleWid->geometry().contains(event->pos()))
        {
            m_bMoveDrag = true; // 设置标志位，表示开始拖动
            // 记录鼠标相对于窗口左上角的偏移量
            m_DragPosition = event->globalPos() - this->pos();
        }
    }
    // 调用父类的 mousePressEvent 方法，以确保事件继续传递
    QWidget::mousePressEvent(event);
}

void MainWid::mouseReleaseEvent(QMouseEvent *event)
{
    m_bMoveDrag = false; // 释放左键时，取消拖动标志位

    QWidget::mouseReleaseEvent(event); // 调用父类的 mouseReleaseEvent 方法，以确保事件继续传递
}

void MainWid::mouseMoveEvent(QMouseEvent *event)
{
    if (m_bMoveDrag)
    {
        move(event->globalPos() - m_DragPosition); // 更新窗口位置，实现拖动效果
    }

    QWidget::mouseMoveEvent(event); // 调用父类的 mouseMoveEvent 方法，以确保事件继续传递
}

//鼠标右键点击时弹出上下文菜单
void MainWid::contextMenuEvent(QContextMenuEvent* event)
{
    m_stMenu.exec(event->globalPos());
}

//在当前窗口全屏模式和普通模式之间切换，并管理控制面板的动画效果
void MainWid::OnFullScreenPlay()
{
    if (!m_bFullScreenPlay)
    {
        m_bFullScreenPlay = true;
        m_stActFullscreen.setChecked(true);

        //脱离父窗口后才能设置
        ui->ShowWid->setWindowFlags(Qt::Window);
        //多屏情况下，在当前屏幕全屏
        QScreen *pStCurScreen = qApp->screens().at(qApp->desktop()->screenNumber(this));
        ui->ShowWid->windowHandle()->setScreen(pStCurScreen);
        
        ui->ShowWid->showFullScreen();

        //计算控制面板的动画区域，并设置动画效果
        QRect stScreenRect = pStCurScreen->geometry();
        int nCtrlBarHeight = ui->CtrlBarWid->height();
        int nX = ui->ShowWid->x();
        m_stCtrlBarAnimationShow = QRect(nX, stScreenRect.height() - nCtrlBarHeight, stScreenRect.width(), nCtrlBarHeight);
        m_stCtrlBarAnimationHide = QRect(nX, stScreenRect.height(), stScreenRect.width(), nCtrlBarHeight);

        m_stCtrlbarAnimationShow->setStartValue(m_stCtrlBarAnimationHide);
        m_stCtrlbarAnimationShow->setEndValue(m_stCtrlBarAnimationShow);
        m_stCtrlbarAnimationShow->setDuration(1000);

        m_stCtrlbarAnimationHide->setStartValue(m_stCtrlBarAnimationShow);
        m_stCtrlbarAnimationHide->setEndValue(m_stCtrlBarAnimationHide);
        m_stCtrlbarAnimationHide->setDuration(2000);
        
        //设置控制面板窗口属性
        ui->CtrlBarWid->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        ui->CtrlBarWid->windowHandle()->setScreen(pStCurScreen);
        ui->CtrlBarWid->raise();
        ui->CtrlBarWid->setWindowOpacity(0.8);
        ui->CtrlBarWid->showNormal();
        ui->CtrlBarWid->windowHandle()->setScreen(pStCurScreen);
        
        //启动动画
        m_stCtrlbarAnimationShow->start();
        m_bFullscreenCtrlBarShow = true;
        m_stFullscreenMouseDetectTimer.start();

        this->setFocus();
    }
    else
    {
        m_bFullScreenPlay = false;
        m_stActFullscreen.setChecked(false);

        //停止动画并恢复状态
        m_stCtrlbarAnimationShow->stop(); //快速切换时，动画还没结束导致控制面板消失
        m_stCtrlbarAnimationHide->stop();
        ui->CtrlBarWid->setWindowOpacity(1);
        ui->CtrlBarWid->setWindowFlags(Qt::SubWindow);

        ui->ShowWid->setWindowFlags(Qt::SubWindow);

        ui->CtrlBarWid->showNormal();
        ui->ShowWid->showNormal();

        //停止鼠标检测定时器
        m_stFullscreenMouseDetectTimer.stop();
        this->setFocus();
    }
}

// 控制面板动画超时处理函数
void MainWid::OnCtrlBarAnimationTimeOut()
{
    // 将光标设置为隐藏状态
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

//全屏模式下的鼠标检测定时器超时处理函数
void MainWid::OnFullscreenMouseDetectTimeOut()
{
    //     qDebug() << m_stCtrlBarAnimationShow;
    //     qDebug() << cursor().pos();
    //     qDebug() << ui->CtrlBarWid->geometry();
    if (m_bFullScreenPlay)
    {
        // 检查鼠标是否在控制面板动画区域内
        if (m_stCtrlBarAnimationShow.contains(cursor().pos()))
        {
            //判断鼠标是否在控制面板上面
            if (ui->CtrlBarWid->geometry().contains(cursor().pos()))
            {
                //继续显示
                m_bFullscreenCtrlBarShow = true;
            }
            else
            {
                //需要显示
                ui->CtrlBarWid->raise();
                
                m_stCtrlbarAnimationShow->start();
                m_stCtrlbarAnimationHide->stop();
                stCtrlBarHideTimer.stop();
            }
        }
        else
        {
            // 如果控制面板当前是显示状态
            if (m_bFullscreenCtrlBarShow)
            {
                //需要隐藏
                m_bFullscreenCtrlBarShow = false;
                stCtrlBarHideTimer.singleShot(1000, this, &MainWid::OnCtrlBarHideTimeOut);
            }

        }

    }
}

// 控制面板隐藏定时器超时处理函数
void MainWid::OnCtrlBarHideTimeOut()
{
    // 开始控制面板隐藏动画
    m_stCtrlbarAnimationHide->start();
}

// 显示右键菜单处理函数
void MainWid::OnShowMenu()
{
    // 在鼠标当前位置显示右键菜单
    m_stMenu.exec(cursor().pos());
}

// 显示“关于”窗口处理函数
void MainWid::OnShowAbout()
{
    // 将“关于”窗口移动到鼠标当前位置的中心
    m_stAboutWidget.move(cursor().pos().x() - m_stAboutWidget.width() / 2, cursor().pos().y() - m_stAboutWidget.height() / 2);
    // 显示“关于”窗口
    m_stAboutWidget.show();
}

// 打开文件处理函数
void MainWid::OpenFile()
{
    // 打开文件对话框，选择视频文件
    QString strFileName = QFileDialog::getOpenFileName(this, "打开文件", QDir::homePath(),
                                                       "视频文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp)");
    // 发送打开文件信号
    emit SigOpenFile(strFileName);
}

// 显示设置窗口处理函数
void MainWid::OnShowSettingWid()
{
    // 显示设置窗口
    m_stSettingWid.show();
}

// 关闭按钮点击处理函数
void MainWid::OnCloseBtnClicked()
{
    // 关闭当前窗口
    this->close();
}

// 最小化按钮点击处理函数
void MainWid::OnMinBtnClicked()
{
    // 最小化当前窗口
    this->showMinimized();
}

// 最大化按钮点击处理函数
void MainWid::OnMaxBtnClicked()
{
    // 检查当前窗口是否已最大化
    if (isMaximized())
    {
        // 如果已最大化，则恢复正常窗口大小
        showNormal();
        emit SigShowMax(false);
    }
    else
    {
        // 如果未最大化，则将窗口最大化
        showMaximized();
        emit SigShowMax(true);
    }
}

// 显示或隐藏播放列表处理函数
void MainWid::OnShowOrHidePlaylist()
{
    // 检查播放列表是否隐藏
    if (ui->PlaylistWid->isHidden())
    {
        // 如果隐藏，则显示播放列表
        ui->PlaylistWid->show();
    }
    else
    {
        // 如果显示，则隐藏播放列表
        ui->PlaylistWid->hide();
    }
    // 重绘主界面
    this->repaint();
}

