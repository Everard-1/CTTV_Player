#include <QContextMenuEvent>
#include <QFileDialog>

#include "medialist.h"

#pragma execution_character_set("utf-8") // 确保使用 UTF-8 编码

// 构造函数
MediaList::MediaList(QWidget *parent)
    : QListWidget(parent), // 初始化父类 QListWidget
      m_stMenu(this), // 创建右键菜单对象
      m_stActAdd(this), // 创建“添加”操作对象
      m_stActRemove(this), // 创建“移除”操作对象
      m_stActClearList(this) // 创建“清空列表”操作对象
{
}

// 析构函数
MediaList::~MediaList()
{
}

// 初始化函数
bool MediaList::Init()
{
    // 设置“添加”操作的文本并将其添加到菜单中
    m_stActAdd.setText("添加");
    m_stMenu.addAction(&m_stActAdd);

    // 设置“移除所选项”操作的文本，并将其添加到子菜单中
    m_stActRemove.setText("移除所选项");
    QMenu* stRemoveMenu = m_stMenu.addMenu("移除");
    stRemoveMenu->addAction(&m_stActRemove);

    // 设置“清空列表”操作的文本并将其添加到菜单中
    m_stActClearList.setText("清空列表");
    m_stMenu.addAction(&m_stActClearList);

    // 连接信号和槽
    connect(&m_stActAdd, &QAction::triggered, this, &MediaList::AddFile); // 当“添加”操作触发时，调用 AddFile 函数
    connect(&m_stActRemove, &QAction::triggered, this, &MediaList::RemoveFile); // 当“移除所选项”操作触发时，调用 RemoveFile 函数
    connect(&m_stActClearList, &QAction::triggered, this, &QListWidget::clear); // 当“清空列表”操作触发时，调用 QListWidget 的 clear 函数

    return true; // 初始化成功
}

// 右键菜单事件处理函数
void MediaList::contextMenuEvent(QContextMenuEvent* event)
{
    // 显示右键菜单，菜单位置为全局位置
    m_stMenu.exec(event->globalPos());
}

// 添加文件到列表的函数
void MediaList::AddFile()
{
    // 打开文件对话框，允许选择多个视频文件
    QStringList listFileName = QFileDialog::getOpenFileNames(this, "打开文件", QDir::homePath(),
        "视频文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp)");

    // 遍历选中的文件名列表
    for (QString strFileName : listFileName)
    {
        // 发送信号，传递每个文件的路径
        emit SigAddFile(strFileName);
    }
}

// 移除文件的函数
void MediaList::RemoveFile()
{
    // 从列表中移除当前选中的项
    takeItem(currentRow());
}
