#include <QDebug>
#include <QDir>

#include "playlist.h"
#include "ui_playlist.h"

#include "globalhelper.h"

// 构造函数
Playlist::Playlist(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Playlist)
{
    ui->setupUi(this); // 设置 UI
}

// 析构函数
Playlist::~Playlist()
{
    QStringList strListPlayList;
    // 遍历播放列表，将每个项目的工具提示文本添加到 strListPlayList
    for (int i = 0; i < ui->List->count(); i++)
    {
        strListPlayList.append(ui->List->item(i)->toolTip());
    }
    // 保存播放列表
    GlobalHelper::SavePlaylist(strListPlayList);

    // 删除 UI
    delete ui;
}

// 初始化函数
bool Playlist::Init()
{
    // 初始化播放列表
    if (ui->List->Init() == false)
    {
        return false;
    }

    // 初始化 UI
    if (InitUi() == false)
    {
        return false;
    }

    // 连接信号和槽
    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    // 允许拖放操作
    setAcceptDrops(true);

    return true;
}

// 初始化 UI 函数
bool Playlist::InitUi()
{
    // 设置样式表
    setStyleSheet(GlobalHelper::GetQssStr("://res/qss/playlist.css"));

    ui->List->clear(); // 清空播放列表

    QStringList strListPlaylist;
    // 获取播放列表
    GlobalHelper::GetPlaylist(strListPlaylist);

    // 遍历播放列表，添加每个文件到播放列表中
    for (QString strVideoFile : strListPlaylist)
    {
        QFileInfo fileInfo(strVideoFile);
        if (fileInfo.exists())
        {
            QListWidgetItem *pItem = new QListWidgetItem(ui->List);
            pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath())); // 设置用户数据
            pItem->setText(QString("%1").arg(fileInfo.fileName())); // 设置显示文本
            pItem->setToolTip(fileInfo.filePath());
            ui->List->addItem(pItem);
        }
    }

    // 如果播放列表不为空，则选中第一个项目
    if (strListPlaylist.length() > 0)
    {
        ui->List->setCurrentRow(0);
    }

    return true;
}

// 连接信号和槽函数
bool Playlist::ConnectSignalSlots()
{
    QList<bool> listRet;
    bool bRet;

    // 连接信号和槽
    bRet = connect(ui->List, &MediaList::SigAddFile, this, &Playlist::OnAddFile);
    listRet.append(bRet);

    // 检查每个连接的结果，如果有一个失败则返回 false
    for (bool bReturn : listRet)
    {
        if (bReturn == false)
        {
            return false;
        }
    }

    return true;
}

// 双击播放列表项目时的处理函数
void Playlist::on_List_itemDoubleClicked(QListWidgetItem *item)
{
    emit SigPlay(item->data(Qt::UserRole).toString()); // 发射播放信号
    m_nCurrentPlayListIndex = ui->List->row(item); // 获取当前播放列表索引
    ui->List->setCurrentRow(m_nCurrentPlayListIndex); // 设置当前行
}

// 获取播放列表状态函数
bool Playlist::GetPlaylistStatus()
{
    // 如果窗口隐藏则返回 false
    if (this->isHidden())
    {
        return false;
    }

    return true;
}

// 获取当前索引函数
int Playlist::GetCurrentIndex()
{
    return 0;
}

// 添加文件函数
void Playlist::OnAddFile(QString strFileName)
{
    // 检查文件是否为支持的格式
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }

    QFileInfo fileInfo(strFileName);
    QList<QListWidgetItem *> listItem = ui->List->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;

    // 如果播放列表中没有该文件则添加
    if (listItem.isEmpty())
    {
        pItem = new QListWidgetItem(ui->List);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath())); // 设置用户数据
        pItem->setText(fileInfo.fileName()); // 设置显示文本
        pItem->setToolTip(fileInfo.filePath());
        ui->List->addItem(pItem);
    }
    else
    {
        pItem = listItem.at(0); // 获取已有的项目
    }
}

// 添加文件并播放函数
void Playlist::OnAddFileAndPlay(QString strFileName)
{
    // 检查文件是否为支持的格式
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }

    QFileInfo fileInfo(strFileName);
    QList<QListWidgetItem *> listItem = ui->List->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;

    // 如果播放列表中没有该文件则添加并播放
    if (listItem.isEmpty())
    {
        pItem = new QListWidgetItem(ui->List);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath())); // 设置用户数据
        pItem->setText(fileInfo.fileName()); // 设置显示文本
        pItem->setToolTip(fileInfo.filePath());
        ui->List->addItem(pItem);
    }
    else
    {
        pItem = listItem.at(0); // 获取已有的项目
    }
    on_List_itemDoubleClicked(pItem); // 双击项目以播放
}

// 后退播放函数
void Playlist::OnBackwardPlay()
{
    // 如果当前索引为 0，则跳转到最后一个项目并播放
    if (m_nCurrentPlayListIndex == 0)
    {
        m_nCurrentPlayListIndex = ui->List->count() - 1;
        on_List_itemDoubleClicked(ui->List->item(m_nCurrentPlayListIndex));
        ui->List->setCurrentRow(m_nCurrentPlayListIndex);
    }
    else
    {
        // 否则索引减一并播放
        m_nCurrentPlayListIndex--;
        on_List_itemDoubleClicked(ui->List->item(m_nCurrentPlayListIndex));
        ui->List->setCurrentRow(m_nCurrentPlayListIndex);
    }
}

// 前进播放函数
void Playlist::OnForwardPlay()
{
    // 如果当前索引为最后一个项目，则跳转到第一个项目并播放
    if (m_nCurrentPlayListIndex == ui->List->count() - 1)
    {
        m_nCurrentPlayListIndex = 0;
        on_List_itemDoubleClicked(ui->List->item(m_nCurrentPlayListIndex));
        ui->List->setCurrentRow(m_nCurrentPlayListIndex);
    }
    else
    {
        // 否则索引加一并播放
        m_nCurrentPlayListIndex++;
        on_List_itemDoubleClicked(ui->List->item(m_nCurrentPlayListIndex));
        ui->List->setCurrentRow(m_nCurrentPlayListIndex);
    }
}

// 拖放事件处理函数
void Playlist::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls(); // 获取拖放的 URL 列表
    if (urls.isEmpty())
    {
        return;
    }

    // 遍历 URL 列表，添加每个文件到播放列表中
    for (QUrl url : urls)
    {
        QString strFileName = url.toLocalFile();
        OnAddFile(strFileName);
    }
}

// 拖放进入事件处理函数
void Playlist::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction(); // 接受拖放操作
}
