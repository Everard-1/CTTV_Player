#include <QDebug>
#include <QMutex>

#include "show.h"
#include "ui_show.h"

#include "globalhelper.h"

// 设置字符集为 UTF-8
#pragma execution_character_set("utf-8")

// 定义一个全局互斥量，用于保护显示区域的几何变化
QMutex g_show_rect_mutex;

Show::Show(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Show),          // 创建 UI 实例
    m_stActionGroup(this),     // 创建动作组
    m_stMenu(this)             // 创建菜单
{
    ui->setupUi(this); // 设置 UI

    // 加载样式表
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/show.css"));
    setAcceptDrops(true); // 允许拖放操作

    // 防止过度刷新显示，设置窗口属性
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    ui->label->setUpdatesEnabled(false); // 禁用 label 的更新

    // 启用鼠标跟踪
    this->setMouseTracking(true);

    m_nLastFrameWidth = 0; ///< 记录视频宽度
    m_nLastFrameHeight = 0; ///< 记录视频高度

    // 添加菜单项
    m_stActionGroup.addAction("全屏");
    m_stActionGroup.addAction("暂停");
    m_stActionGroup.addAction("停止");

    // 将动作添加到菜单中
    m_stMenu.addActions(m_stActionGroup.actions());
}

Show::~Show()
{
    delete ui; // 删除 UI 实例
}

bool Show::Init()
{
    // 连接信号和槽
    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    return true;
}

// 当视频帧尺寸发生变化时调用
void Show::OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight)
{
    qDebug() << "Show::OnFrameDimensionsChanged" << nFrameWidth << nFrameHeight;
    m_nLastFrameWidth = nFrameWidth;
    m_nLastFrameHeight = nFrameHeight;

    ChangeShow(); // 重新计算并设置显示区域
}

// 改变显示区域
void Show::ChangeShow()
{
    g_show_rect_mutex.lock(); // 加锁以保护显示区域的几何变化

    if (m_nLastFrameWidth == 0 && m_nLastFrameHeight == 0)
    {
        ui->label->setGeometry(0, 0, width(), height()); // 如果没有视频尺寸，则填满整个窗口
    }
    else
    {
        float aspect_ratio;
        int width, height, x, y;
        int scr_width = this->width();
        int scr_height = this->height();

        aspect_ratio = (float)m_nLastFrameWidth / (float)m_nLastFrameHeight;

        height = scr_height;
        width = lrint(height * aspect_ratio) & ~1; // 按宽高比计算宽度并确保其为偶数
        if (width > scr_width)
        {
            width = scr_width;
            height = lrint(width / aspect_ratio) & ~1; // 按宽高比计算高度并确保其为偶数
        }
        x = (scr_width - width) / 2; // 计算水平居中的 X 坐标
        y = (scr_height - height) / 2; // 计算垂直居中的 Y 坐标

        ui->label->setGeometry(x, y, width, height); // 设置 label 的几何形状
    }

    g_show_rect_mutex.unlock(); // 解锁
}

// 拖放事件进入时调用
void Show::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction(); // 接受拖放操作
}

// 窗口大小调整事件处理
void Show::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event); // 忽略事件

    ChangeShow(); // 调整显示区域
}

// 键释放事件处理
void Show::keyReleaseEvent(QKeyEvent *event)
{
    qDebug() << "Show::keyPressEvent:" << event->key();
    switch (event->key())
    {
    case Qt::Key_Return: // 全屏
        SigFullScreen();
        break;
    case Qt::Key_Left: // 后退 5 秒
        emit SigSeekBack();
        break;
    case Qt::Key_Right: // 前进 5 秒
        qDebug() << "前进5s";
        emit SigSeekForward();
        break;
    case Qt::Key_Up: // 增加 10 音量
        emit SigAddVolume();
        break;
    case Qt::Key_Down: // 减少 10 音量
        emit SigSubVolume();
        break;
    case Qt::Key_Space: // 播放/暂停
        emit SigPlayOrPause();
        break;

    default:
        QWidget::keyPressEvent(event); // 处理其他按键事件
        break;
    }
}

// 鼠标按下事件处理
void Show::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton)
    {
        emit SigShowMenu(); // 右键点击时发射显示菜单信号
    }

    QWidget::mousePressEvent(event); // 处理其他鼠标事件
}

// 显示消息的槽函数
void Show::OnDisplayMsg(QString strMsg)
{
    qDebug() << "Show::OnDisplayMsg " << strMsg;
}

// 播放文件的槽函数
void Show::OnPlay(QString strFile)
{
    VideoCtl::GetInstance()->StartPlay(strFile, ui->label->winId());
}

// 停止播放完成后的处理
void Show::OnStopFinished()
{
    update(); // 更新窗口
}

// 定时器更新光标显示的槽函数
void Show::OnTimerShowCursorUpdate()
{
    // 这里可以实现隐藏光标的逻辑
    //setCursor(Qt::BlankCursor);
}

// 动作触发处理
void Show::OnActionsTriggered(QAction *action)
{
    QString strAction = action->text();
    if (strAction == "全屏")
    {
        emit SigFullScreen();
    }
    else if (strAction == "停止")
    {
        emit SigStop();
    }
    else if (strAction == "暂停" || strAction == "播放")
    {
        emit SigPlayOrPause();
    }
}

// 连接信号和槽函数
bool Show::ConnectSignalSlots()
{
    QList<bool> listRet;
    bool bRet;

    // 连接播放信号到 OnPlay 槽
    bRet = connect(this, &Show::SigPlay, this, &Show::OnPlay);
    listRet.append(bRet);

    // 连接定时器超时信号到 OnTimerShowCursorUpdate 槽
    timerShowCursor.setInterval(2000);
    bRet = connect(&timerShowCursor, &QTimer::timeout, this, &Show::OnTimerShowCursorUpdate);
    listRet.append(bRet);

    // 连接动作组的 triggered 信号到 OnActionsTriggered 槽
    connect(&m_stActionGroup, &QActionGroup::triggered, this, &Show::OnActionsTriggered);

    // 检查连接是否成功
    for (bool bReturn : listRet)
    {
        if (bReturn == false)
        {
            return false;
        }
    }

    return true;
}

// 拖放事件处理
void Show::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls(); // 获取拖放的 URL 列表
    if(urls.isEmpty())
    {
        return;
    }

    // 处理拖放的第一个文件
    for(QUrl url: urls)
    {
        QString strFileName = url.toLocalFile();
        qDebug() << strFileName;
        emit SigOpenFile(strFileName); // 发射打开文件信号
        break;
    }
}
