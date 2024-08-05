
#include <QPainter>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>
#include <QFileDialog>

#include "title.h"
#include "ui_title.h"

#include "globalhelper.h"

// 设置字符集为 UTF-8
#pragma execution_character_set("utf-8")

// Title 类构造函数
Title::Title(QWidget *parent) :
    QWidget(parent),                // 父窗口
    ui(new Ui::Title),              // 初始化 UI 实例
    m_stActionGroup(this),          // 初始化动作组
    m_stMenu(this)                  // 初始化菜单
{
    ui->setupUi(this); // 设置 UI 界面

    // 连接各个按钮的点击信号到相应的槽函数
    connect(ui->CloseBtn, &QPushButton::clicked, this, &Title::SigCloseBtnClicked);
    connect(ui->MinBtn, &QPushButton::clicked, this, &Title::SigMinBtnClicked);
    connect(ui->MaxBtn, &QPushButton::clicked, this, &Title::SigMaxBtnClicked);
    connect(ui->FullScreenBtn, &QPushButton::clicked, this, &Title::SigFullScreenBtnClicked);
    connect(ui->MenuBtn, &QPushButton::clicked, this, &Title::SigShowMenu);

    // 添加菜单项并连接到槽函数
    m_stMenu.addAction("最大化", this, &Title::SigMaxBtnClicked);
    m_stMenu.addAction("最小化", this, &Title::SigMinBtnClicked);
    m_stMenu.addAction("退出", this, &Title::SigCloseBtnClicked);

    QMenu* stMenu = m_stMenu.addMenu("打开");
    stMenu->addAction("打开文件", this, &Title::OpenFile);

    // 设置按钮的工具提示
    ui->MenuBtn->setToolTip("显示主菜单");
    ui->MinBtn->setToolTip("最小化");
    ui->MaxBtn->setToolTip("最大化");
    ui->CloseBtn->setToolTip("关闭");
    ui->FullScreenBtn->setToolTip("全屏");
}

// Title 类析构函数
Title::~Title()
{
    delete ui; // 删除 UI 实例
}

// 初始化函数
bool Title::Init()
{
    if (InitUi() == false)
    {
        return false; // 如果 UI 初始化失败，返回 false
    }

    return true; // 初始化成功，返回 true
}

// 初始化 UI
bool Title::InitUi()
{
    ui->MovieNameLab->clear(); // 清除电影名称标签

    // 确保窗口透明
    setAttribute(Qt::WA_TranslucentBackground);

    // 设置样式表
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/title.css"));

    // 设置按钮图标
    GlobalHelper::SetIcon(ui->MaxBtn, 9, QChar(0xf2d0));
    GlobalHelper::SetIcon(ui->MinBtn, 9, QChar(0xf2d1));
    GlobalHelper::SetIcon(ui->CloseBtn, 9, QChar(0xf00d));
    GlobalHelper::SetIcon(ui->FullScreenBtn, 9, QChar(0xf065));

    // 初始化完成检查
    if (about.Init() == false)
    {
        return false; // 如果 `about` 初始化失败，返回 false
    }

    return true; // 成功初始化 UI，返回 true
}

// 打开文件对话框
void Title::OpenFile()
{
    QString strFileName = QFileDialog::getOpenFileName(this, "打开文件", QDir::homePath(),
        "视频文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp)");

    emit SigOpenFile(strFileName); // 发射打开文件信号
}

// 绘制事件处理（空实现）
void Title::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event); // 忽略事件
}

// 鼠标双击事件处理
void Title::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        emit SigDoubleClicked(); // 发射双击信号
    }
}

// 窗口大小调整事件处理（暂未实现）
void Title::resizeEvent(QResizeEvent *event)
{
    ChangeMovieNameShow(); // 可能用于调整电影名称的显示
}

// 修改电影名称显示
void Title::ChangeMovieNameShow()
{
    QFontMetrics fontMetrics(ui->MovieNameLab->font());
    int fontSize = fontMetrics.width(m_strMovieName); // 获取电影名称的宽度
    int showwidth = ui->MovieNameLab->width(); // 获取标签的宽度
    if (fontSize > showwidth)
    {
        QString str = fontMetrics.elidedText(m_strMovieName, Qt::ElideRight, ui->MovieNameLab->width()); // 返回带省略号的字符串
        ui->MovieNameLab->setText(str);
    }
    else
    {
        ui->MovieNameLab->setText(m_strMovieName); // 设置电影名称
    }
}

// 修改最大化按钮的样式
void Title::OnChangeMaxBtnStyle(bool bIfMax)
{
    if (bIfMax)
    {
        GlobalHelper::SetIcon(ui->MaxBtn, 9, QChar(0xf2d2)); // 设置还原图标
        ui->MaxBtn->setToolTip("还原"); // 设置工具提示
    }
    else
    {
        GlobalHelper::SetIcon(ui->MaxBtn, 9, QChar(0xf2d0)); // 设置最大化图标
        ui->MaxBtn->setToolTip("最大化"); // 设置工具提示
    }
}

// 播放函数，设置电影名称
void Title::OnPlay(QString strMovieName)
{
    qDebug() << "Title::OnPlay";
    QFileInfo fileInfo(strMovieName); // 获取文件信息
    m_strMovieName = fileInfo.fileName(); // 获取文件名
    ui->MovieNameLab->setText(m_strMovieName); // 设置电影名称标签
    //ChangeMovieNameShow(); // 可能用于调整电影名称的显示
}

// 播放停止完成后的处理
void Title::OnStopFinished()
{
    qDebug() << "Title::OnStopFinished";
    ui->MovieNameLab->clear(); // 清空电影名称标签
}
