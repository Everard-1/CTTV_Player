#include "CustomSlider.h"
#include "globalhelper.h"

// 构造函数，初始化滑块
CustomSlider::CustomSlider(QWidget *parent)
    : QSlider(parent) // 调用父类QSlider的构造函数
{
    this->setMaximum(MAX_SLIDER_VALUE); // 设置滑块的最大值
}

// 析构函数
CustomSlider::~CustomSlider()
{
}

// 鼠标按下事件处理函数
void CustomSlider::mousePressEvent(QMouseEvent *ev)
{
    // 调用父类的鼠标按下事件处理函数，以确保滑块的拖动操作正常进行
    QSlider::mousePressEvent(ev);

    // 获取鼠标点击的位置相对于滑块宽度的比例
    double pos = ev->pos().x() / (double)width();
    // 根据比例计算滑块的新值，并设置滑块的值
    setValue(pos * (maximum() - minimum()) + minimum());

    // 发射自定义信号，通知滑块的值已经改变
    emit SigCustomSliderValueChanged();
}

// 鼠标释放事件处理函数
void CustomSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    // 调用父类的鼠标释放事件处理函数
    QSlider::mouseReleaseEvent(ev);

    // 鼠标释放时不需要发射自定义信号
    // emit SigCustomSliderValueChanged();
}

// 鼠标移动事件处理函数
void CustomSlider::mouseMoveEvent(QMouseEvent *ev)
{
    // 调用父类的鼠标移动事件处理函数
    QSlider::mouseMoveEvent(ev);

    // 获取鼠标当前位置相对于滑块宽度的比例
    double pos = ev->pos().x() / (double)width();
    // 根据比例计算滑块的新值，并设置滑块的值
    setValue(pos * (maximum() - minimum()) + minimum());

    // 发射自定义信号，通知滑块的值已经改变
    emit SigCustomSliderValueChanged();
}
