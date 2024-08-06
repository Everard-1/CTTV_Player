# CTTV_Player
一个基于ffmpeg和SDL2的视频播放器，实现一个基本视频播放器应有的功能
<a name="MdV2E"></a>
# 项目环境
Qt：5.14.2     kits：desktop Qt MinGW 32bit

- 使用 FFmpeg-4.2.1 (x86) 解码，SDL2-2.0.7 (x86) 渲染。  
- 在 Windows 下使用 Qt5.14.2 (MinGW x86) 开发。  
- 项目目录下的 .pro 文件，支持在多平台（Windows、Linux、Mac）下 QtCreator 打开编译调试。
<a name="nO7lE"></a>
# Windows平台编译调试

1. 下载 FFmpeg、SDL2 动态库，放在 bin 目录下。(直接从官网下载即可，亦可下载本项目最新release，安装后，从安装目录下拷贝动态库。)  
2. 使用 QtCreator 打开 CTTV_Player.pro。  
3. 编译运行。

[ffmpeg-4.2.1-win32-dev.zip](https://www.yuque.com/attachments/yuque/0/2024/zip/27393008/1722848117188-2ffa70e6-e44f-4be7-8d22-8f62e5d063e2.zip?_lake_card=%7B%22src%22%3A%22https%3A%2F%2Fwww.yuque.com%2Fattachments%2Fyuque%2F0%2F2024%2Fzip%2F27393008%2F1722848117188-2ffa70e6-e44f-4be7-8d22-8f62e5d063e2.zip%22%2C%22name%22%3A%22ffmpeg-4.2.1-win32-dev.zip%22%2C%22size%22%3A607409%2C%22ext%22%3A%22zip%22%2C%22source%22%3A%22%22%2C%22status%22%3A%22done%22%2C%22download%22%3Atrue%2C%22taskId%22%3A%22u29097328-e0bb-4d59-9bf7-d67584b4b1c%22%2C%22taskType%22%3A%22upload%22%2C%22type%22%3A%22application%2Fx-zip-compressed%22%2C%22__spacing%22%3A%22both%22%2C%22mode%22%3A%22title%22%2C%22id%22%3A%22u972caee2%22%2C%22margin%22%3A%7B%22top%22%3Atrue%2C%22bottom%22%3Atrue%7D%2C%22card%22%3A%22file%22%7D)[SDL2.zip](https://www.yuque.com/attachments/yuque/0/2024/zip/27393008/1722848122734-d6f90f6f-fd2c-4c55-a255-0d9f9e49f7ad.zip?_lake_card=%7B%22src%22%3A%22https%3A%2F%2Fwww.yuque.com%2Fattachments%2Fyuque%2F0%2F2024%2Fzip%2F27393008%2F1722848122734-d6f90f6f-fd2c-4c55-a255-0d9f9e49f7ad.zip%22%2C%22name%22%3A%22SDL2.zip%22%2C%22size%22%3A1633677%2C%22ext%22%3A%22zip%22%2C%22source%22%3A%22%22%2C%22status%22%3A%22done%22%2C%22download%22%3Atrue%2C%22taskId%22%3A%22u564b9f70-ab26-46bb-9bb2-ac07d9220ee%22%2C%22taskType%22%3A%22upload%22%2C%22type%22%3A%22application%2Fx-zip-compressed%22%2C%22__spacing%22%3A%22both%22%2C%22mode%22%3A%22title%22%2C%22id%22%3A%22u4b8c7659%22%2C%22margin%22%3A%7B%22top%22%3Atrue%2C%22bottom%22%3Atrue%7D%2C%22card%22%3A%22file%22%7D)
<a name="QJSu1"></a>
# 项目结果：
![image.png](https://cdn.nlark.com/yuque/0/2024/png/27393008/1722847316556-81a2bb9c-3195-4eea-9dfd-263588fad015.png#averageHue=%232e2633&clientId=u44410218-b732-4&from=paste&id=uc755b0e1&originHeight=502&originWidth=799&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=295198&status=done&style=none&taskId=u74f0f3de-981d-4eae-b515-cef6ef8d666&title=)<br />实现了基本播放器功能：

- 暂停/播放
- 停止
- 上一个/下一个
- 倍速播放
- 进度条时间显示
- 音量控制
- 媒体文件的导入
- 播放列表
- 全屏播放
- 显示媒体文件名
<a name="KXvHZ"></a>
# 视频演示
data-canonical-src="https://private-user-images.githubusercontent.com/103874084/355138310-65b50bdc-f1f4-443e-b305-49ced1839b66.mp4"
<a name="HrPT3"></a>
# 项目分析
![](https://cdn.nlark.com/yuque/0/2024/jpeg/27393008/1721728596809-c3d598c0-ca2c-4342-8fd9-b948bbf89ba2.jpeg)
<a name="VdUIR"></a>
# 播放器框架：
![](https://cdn.nlark.com/yuque/0/2024/jpeg/27393008/1722845128703-aa0f0fed-34a9-47aa-bd3c-7bcd943d122a.jpeg)
<a name="oEUmr"></a>
# UI实现
主要是先设计出基础的ui界面，创建其中涉及到的界面、窗口、按钮、滑块等，先设置好对应的布局，在对要用到的ui空间进行变量名的修改，栅格布局，提升等<br />![image.png](https://cdn.nlark.com/yuque/0/2024/png/27393008/1722845753135-6901c9fa-0ffa-4ff8-8de7-1770346bb800.png#averageHue=%23f1f1f1&clientId=u80d5e214-9ea2-4&from=paste&height=557&id=u0cfef126&originHeight=835&originWidth=1279&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=35194&status=done&style=none&taskId=u0d56f958-4a55-4f16-904b-9cb46858ee2&title=&width=852.6666666666666)<br />后面我们再用css资源设计更加美观的界面和字体<br />![image.png](https://cdn.nlark.com/yuque/0/2024/png/27393008/1722845836607-2720a5ca-3348-4511-a4b9-54857556afaf.png#averageHue=%23c9f4ef&clientId=u80d5e214-9ea2-4&from=paste&height=469&id=u8797577e&originHeight=704&originWidth=1194&originalType=binary&ratio=1.5&rotation=0&showTitle=false&size=234155&status=done&style=none&taskId=u216da051-4826-4b0d-a2cc-44338dac92a&title=&width=796)
<a name="tzMDr"></a>
# sonic音频处理
<a name="QU4wD"></a>
### 1. **基本概念**
`sonic` 是一个开源音频处理库，主要用于调整音频的速度、音调和采样率。其核心功能包括：

- **速度调整**: 改变音频播放速度。
- **音调调整**: 改变音频的音调。
- **采样率调整**: 改变音频的采样率。
- **音量调整**: 改变音频的音量。
<a name="Z73vN"></a>
### 2. **核心处理流程**
<a name="gT2ps"></a>
#### **输入与输出处理**

- `**sonicWriteFloatToStream**`**, **`**sonicWriteShortToStream**`**, **`**sonicWriteUnsignedCharToStream**`: 这些函数将不同类型的数据（`float`, `short`, `unsigned char`）写入流并进行处理。它们调用 `addFloatSamplesToInputBuffer`, `addShortSamplesToInputBuffer`, `addUnsignedCharSamplesToInputBuffer` 分别将数据添加到输入缓冲区，并调用 `processStreamInput` 进行处理。
- `**sonicChangeFloatSpeed**`**, **`**sonicChangeShortSpeed**`: 这些函数提供了不使用流的接口来更改音频样本的速度。它们创建一个 `sonicStream` 实例，设置相关参数，然后处理音频数据并返回处理后的样本数量。
<a name="mTrcz"></a>
#### **速度和音调调整**

- `**changeSpeed**`: 这个函数在音频流中重新采样基音周期，以根据给定的速度调整音频的播放速度。它处理音频样本的不同情况（速度大于1.0或小于1.0）来决定是跳过基音周期还是插入基音周期。
- `**insertPitchPeriod**`** 和 **`**skipPitchPeriod**`: 用于调整速度时插入或跳过基音周期，从而改变音频播放的速度。速度大于1.0时，跳过基音周期，速度小于1.0时，插入新的基音周期。
<a name="CdVdT"></a>
#### **采样率调整**

- `**adjustRate**`: 这个函数使用 sinc FIR 滤波器来调整音频的采样率。它插值新的输出样本，以改变音频流的采样率。`interpolate` 函数负责计算新的音频样本值。
- `**interpolate**`: 使用 sinc 滤波器进行插值。这是一种常见的重采样技术，通过计算加权平均来生成新的样本。
<a name="pHYYJ"></a>
#### **音量调整**

- `**processStreamInput**`: 在处理完输入样本后，根据需要调整音量。如果音量不为1.0，调用 `scaleSamples` 函数来调整输出音量。
<a name="V62rZ"></a>
### 3. **技术细节**

- **Sinc 滤波器**: 在 `adjustRate` 函数中使用 sinc 滤波器进行插值，处理采样率调整。`interpolate` 函数实现了 sinc 滤波器，并通过加权平均的方式生成新的样本值。
- **音调和速度调整的结合**: `sonic` 可以同时调整音调和速度，通过调整基音周期来处理音调变化，并使用 `changeSpeed` 函数处理速度变化。
- **错误处理**: 多个函数检查是否成功执行，例如纹理的创建、样本的添加等。如果发生错误，它们返回错误码（如 `-1`），并进行适当的处理。
<a name="hTAya"></a>
# 视频控制类
<a name="8688e00f"></a>
### 1. **初始化和资源管理**

- **构造函数 (**`**VideoCtl::VideoCtl**`**)**：
   - 初始化类成员变量。
   - 调用 `av_register_all()` 和 `avformat_network_init()` 初始化 FFmpeg 的所有编解码器和网络功能。
- **初始化 (**`**VideoCtl::Init**`**)**：
   - 检查是否已经初始化。
   - 连接信号和槽。
   - 初始化 SDL 库，设置视频、音频和定时器功能。
   - 忽略某些系统和用户事件。
   - 注册自定义的锁管理器，以确保线程安全。
- **析构函数 (**`**VideoCtl::~VideoCtl**`**)**：
   - 注销锁管理器。
   - 反初始化 FFmpeg 网络功能。
   - 退出 SDL 库，释放资源。
<a name="3b27e45c"></a>
### 2. **播放控制**

- **启动播放 (**`**VideoCtl::StartPlay**`**)**：
   - 关闭当前播放循环，如果线程正在运行则等待其结束。
   - 发送播放开始信号。
   - 使用 FFmpeg 打开指定的视频文件流。
   - 如果打开成功，则创建播放线程。
- **播放线程 (**`**VideoCtl::LoopThread**`**)**：
   - 进入事件循环，处理用户输入和窗口事件。
   - 支持的操作包括：
      - 切换音频、视频和字幕流。
      - 调整音量。
      - 播放控制（播放、暂停、停止、跳转）。
<a name="c5b1d721"></a>
### 3. **视频显示和处理**

- **视频显示 (**`**VideoCtl::video_display**`**)**：
   - 检查并创建渲染窗口和渲染器。
   - 使用 SDL 渲染器绘制视频图像。
- **打开视频 (**`**VideoCtl::video_open**`**)**：
   - 创建或调整 SDL 窗口和渲染器。
   - 设置窗口的大小。
   - 配置渲染器（使用硬件加速或软件渲染）。
<a name="889ce6dd"></a>
### 4. **流管理和操作**

- **流关闭 (**`**VideoCtl::do_exit**`**)**：
   - 关闭并释放当前流的资源。
   - 释放 SDL 渲染器和窗口。
- **音量控制 (**`**VideoCtl::OnPlayVolume**`** 和 **`**VideoCtl::UpdateVolume**`**)**：
   - 根据百分比调整音量。
   - 使用对数函数计算新的音量级别。
- **快进和快退 (**`**VideoCtl::OnSeekForward**`** 和 **`**VideoCtl::OnSeekBack**`**)**：
   - 根据当前时间和增量调整播放位置。
- **跳转到特定章节 (**`**VideoCtl::seek_chapter**`**)**：
   - 寻找当前章节并跳转到指定章节。
<a name="161c96c4"></a>
### 5. **事件处理**

- **事件处理 (**`**VideoCtl::LoopThread**`**)**：
   - 处理 SDL 事件，包括键盘按键、窗口调整和退出事件。
   - 根据用户的操作（如按键）调用相应的控制函数。
<a name="745c069c"></a>
### 6. **锁管理**

- **锁管理 (**`**lockmgr**`**)**：
   - 实现了一个简单的锁管理器，处理 FFmpeg 的线程安全问题。

