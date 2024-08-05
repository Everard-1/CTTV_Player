#include "about.h"
#include "ui_about.h"
#include "globalhelper.h"

// 关于窗口的构造函数
About::About(QWidget *parent)
    : QWidget(parent)
{
    ui = new Ui::About();
    ui->setupUi(this);
}

// 关于窗口的析构函数
About::~About()
{
    delete ui;
}

// 初始化关于窗口
bool About::Init()
{
    // 设置关于窗口为模态窗口，弹出关于窗口时主窗口不可点击
    this->setWindowModality(Qt::ApplicationModal);

    // 设置窗口图标和Logo标签的图片
    this->setWindowIcon(QIcon("://res/player.png"));
    ui->LogoLabel->setPixmap(QPixmap("://res/player.png").scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 设置关于窗口的版本信息标签，使用QString::arg的多参数版本
    QString strVersion = QString("版本：%1\n时间：%2").arg(GlobalHelper::GetAppVersion(), QString(__DATE__) + " " + QString(__TIME__));
    ui->VersionLabel->setText(strVersion);

    return true;
}

// 处理鼠标按下事件
void About::mousePressEvent(QMouseEvent *event)
{
    // 如果按下的是左键，记录鼠标位置并开始拖动
    if (event->buttons() & Qt::LeftButton)
    {
        m_bMoveDrag = true;
        m_DragPosition = event->globalPos() - this->pos();
    }

    // 调用基类的鼠标按下事件处理函数
    QWidget::mousePressEvent(event);
}

// 处理鼠标释放事件
void About::mouseReleaseEvent(QMouseEvent *event)
{
    // 停止拖动
    m_bMoveDrag = false;

    // 调用基类的鼠标释放事件处理函数
    QWidget::mouseReleaseEvent(event);
}

// 处理鼠标移动事件
void About::mouseMoveEvent(QMouseEvent *event)
{
    // 如果正在拖动，移动窗口到新的位置
    if (m_bMoveDrag)
    {
        move(event->globalPos() - m_DragPosition);
    }

    // 调用基类的鼠标移动事件处理函数
    QWidget::mouseMoveEvent(event);
}

// 处理关闭按钮点击事件
void About::on_ClosePushButton_clicked()
{
    // 隐藏关于窗口
    hide();
}
