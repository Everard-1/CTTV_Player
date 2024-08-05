#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

#include "globalhelper.h"

// 定义常量
const QString PLAYER_CONFIG_BASEDIR = QDir::tempPath(); // 配置文件的基本目录，使用系统的临时目录
const QString PLAYER_CONFIG = "player_config.ini"; // 配置文件名
const QString APP_VERSION = "0.1.0"; // 应用版本号

// 构造函数
GlobalHelper::GlobalHelper()
{
}

// 从指定路径读取QSS样式表文件的内容
QString GlobalHelper::GetQssStr(QString strQssPath)
{
    QString strQss; // 存储样式表内容的字符串
    QFile FileQss(strQssPath); // 创建QFile对象以打开样式表文件
    if (FileQss.open(QIODevice::ReadOnly)) // 尝试以只读模式打开文件
    {
        strQss = FileQss.readAll(); // 读取文件所有内容
        FileQss.close(); // 关闭文件
    }
    else
    {
        qDebug() << "读取样式表失败" << strQssPath; // 如果无法打开文件，打印调试信息
    }
    return strQss; // 返回样式表内容
}

// 设置按钮的图标
void GlobalHelper::SetIcon(QPushButton* btn, int iconSize, QChar icon)
{
    QFont font; // 创建字体对象
    font.setFamily("FontAwesome"); // 设置字体为FontAwesome
    font.setPointSize(iconSize); // 设置字体大小

    btn->setFont(font); // 应用字体到按钮
    btn->setText(icon); // 设置按钮文本为图标字符
}

// 保存播放列表到配置文件
void GlobalHelper::SavePlaylist(QStringList& playList)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG; // 配置文件路径
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat); // 使用INI格式的QSettings对象
    settings.beginWriteArray("playlist"); // 开始写入数组
    for (int i = 0; i < playList.size(); ++i)
    {
        settings.setArrayIndex(i); // 设置数组索引
        settings.setValue("movie", playList.at(i)); // 保存播放列表项
    }
    settings.endArray(); // 结束数组写入
}

// 从配置文件读取播放列表
void GlobalHelper::GetPlaylist(QStringList& playList)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG; // 配置文件路径
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat); // 使用INI格式的QSettings对象

    int size = settings.beginReadArray("playlist"); // 开始读取数组并获取数组大小
    for (int i = 0; i < size; ++i)
    {
        settings.setArrayIndex(i); // 设置数组索引
        playList.append(settings.value("movie").toString()); // 读取播放列表项并添加到列表中
    }
    settings.endArray(); // 结束数组读取
}

// 保存播放音量到配置文件
void GlobalHelper::SavePlayVolume(double& nVolume)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG; // 配置文件路径
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat); // 使用INI格式的QSettings对象
    settings.setValue("volume/size", nVolume); // 保存音量值
}

// 从配置文件读取播放音量
void GlobalHelper::GetPlayVolume(double& nVolume)
{
    QString strPlayerConfigFileName = PLAYER_CONFIG_BASEDIR + QDir::separator() + PLAYER_CONFIG; // 配置文件路径
    QSettings settings(strPlayerConfigFileName, QSettings::IniFormat); // 使用INI格式的QSettings对象
    QString str = settings.value("volume/size").toString(); // 读取音量值的字符串
    nVolume = settings.value("volume/size", nVolume).toDouble(); // 将读取的值转换为双精度浮点数并赋值
}

// 获取应用版本号
QString GlobalHelper::GetAppVersion()
{
    return APP_VERSION; // 返回应用版本号
}
