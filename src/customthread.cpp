#include "customthread.h"

// 构造函数
CustomThread::CustomThread()
    : m_bRunning(false) // 初始化成员变量
{
}

// 析构函数
CustomThread::~CustomThread()
{
    // 确保线程在析构时停止
    if (this->isRunning())
    {
        StopThread();
        this->wait(); // 等待线程安全退出
    }
}

// 启动线程
bool CustomThread::StartThread()
{
    m_bRunning = true; // 设置运行标志为true
    if (!this->isRunning()) // 如果线程未运行
    {
        this->start(); // 启动线程
    }
    return true; // 返回true表示线程已启动
}

// 停止线程
bool CustomThread::StopThread()
{
    m_bRunning = false; // 设置运行标志为false
    return true; // 返回true表示线程已停止
}
