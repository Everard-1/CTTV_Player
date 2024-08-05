#include <QDebug>
#include <QMutex>

#include <thread>
#include "videoctl.h"

#pragma execution_character_set("utf-8")

extern QMutex g_show_rect_mutex;

// 这些静态变量用于控制视频播放和缓存
static int framedrop = -1;          // 帧丢弃计数
static int infinite_buffer = -1;    // 无限缓冲标志
static int64_t audio_callback_time; // 音频回调时间

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2) // 自定义事件类型，用于退出

// 重新分配纹理的内存
int VideoCtl::realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
{
    Uint32 format;
    int access, w, h;

    // 查询当前纹理的属性
    if (SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
        void *pixels;
        int pitch;

        // 销毁当前纹理
        SDL_DestroyTexture(*texture);

        // 创建新的纹理
        if (!(*texture = SDL_CreateTexture(renderer, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
            return -1; // 创建失败，返回 -1

        // 设置纹理的混合模式
        if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
            return -1; // 设置失败，返回 -1

        // 初始化纹理
        if (init_texture) {
            if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
                return -1; // 锁定纹理失败，返回 -1

            // 用零填充纹理
            memset(pixels, 0, pitch * new_height);
            SDL_UnlockTexture(*texture);
        }
    }
    return 0; // 成功，返回 0
}

// 计算显示区域的矩形
void VideoCtl::calculate_display_rect(SDL_Rect *rect,
                                      int scr_xleft, int scr_ytop, int scr_width, int scr_height,
                                      int pic_width, int pic_height, AVRational pic_sar)
{
    float aspect_ratio;
    int width, height, x, y;

    // 计算宽高比
    if (pic_sar.num == 0)
        aspect_ratio = 0;
    else
        aspect_ratio = av_q2d(pic_sar);

    if (aspect_ratio <= 0.0)
        aspect_ratio = 1.0;

    // 计算实际的显示宽高比
    aspect_ratio *= (float)pic_width / (float)pic_height;

    // 假设屏幕像素比为 1.0
    height = scr_height;
    width = lrint(height * aspect_ratio) & ~1; // 计算宽度并对齐
    if (width > scr_width) {
        width = scr_width;
        height = lrint(width / aspect_ratio) & ~1; // 计算高度并对齐
    }

    // 计算显示区域的偏移量
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;

    // 设置显示矩形
    rect->x = scr_xleft + x;
    rect->y = scr_ytop + y;
    rect->w = FFMAX(width, 1);
    rect->h = FFMAX(height, 1);
}

// 上传纹理数据
int VideoCtl::upload_texture(SDL_Texture *tex, AVFrame *frame, struct SwsContext **img_convert_ctx) {
    int ret = 0;

    switch (frame->format) {
    case AV_PIX_FMT_YUV420P:
        if (frame->linesize[0] < 0 || frame->linesize[1] < 0 || frame->linesize[2] < 0) {
            av_log(NULL, AV_LOG_ERROR, "Negative linesize is not supported for YUV.\n");
            return -1; // 负的行大小不支持，返回 -1
        }
        // 更新 YUV 纹理
        ret = SDL_UpdateYUVTexture(tex, NULL, frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]);
        break;
    case AV_PIX_FMT_BGRA:
        if (frame->linesize[0] < 0) {
            // 更新纹理数据（垂直翻转）
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
        }
        else {
            ret = SDL_UpdateTexture(tex, NULL, frame->data[0], frame->linesize[0]);
        }
        break;
    default:
        // 如果格式不受支持，使用转换上下文将图像转换为 BGRA
        *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
                                                frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height,
                                                AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);
        if (*img_convert_ctx != NULL) {
            uint8_t *pixels[4];
            int pitch[4];

            // 锁定纹理并进行图像转换
            if (!SDL_LockTexture(tex, NULL, (void **)pixels, pitch)) {
                sws_scale(*img_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                          0, frame->height, pixels, pitch);
                SDL_UnlockTexture(tex);
            }
        }
        else {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            ret = -1; // 初始化失败，返回 -1
        }
        break;
    }
    return ret; // 返回操作结果
}

// 显示视频画面
void VideoCtl::video_image_display(VideoState *is)
{
    Frame *vp; // 当前的视频帧
    Frame *sp = NULL; // 当前的字幕帧
    SDL_Rect rect; // 用于显示的视频区域矩形

    // 获取视频帧队列中的最新帧
    vp = frame_queue_peek_last(&is->pictq);

    // 检查是否有字幕流
    if (is->subtitle_st) {
        // 如果字幕队列中有剩余的字幕帧
        if (frame_queue_nb_remaining(&is->subpq) > 0) {
            // 获取字幕帧
            sp = frame_queue_peek(&is->subpq);

            // 如果当前视频帧的时间戳大于字幕帧的显示开始时间
            if (vp->pts >= sp->pts + ((float)sp->sub.start_display_time / 1000)) {
                // 如果字幕帧尚未上传
                if (!sp->uploaded) {
                    uint8_t* pixels[4];
                    int pitch[4];
                    int i;

                    // 如果字幕帧的宽高未设置，则使用视频帧的宽高
                    if (!sp->width || !sp->height) {
                        sp->width = vp->width;
                        sp->height = vp->height;
                    }

                    // 重新分配字幕纹理
                    if (realloc_texture(&is->sub_texture, SDL_PIXELFORMAT_ARGB8888, sp->width, sp->height, SDL_BLENDMODE_BLEND, 1) < 0)
                        return;

                    // 遍历每个字幕矩形
                    for (i = 0; i < sp->sub.num_rects; i++) {
                        AVSubtitleRect *sub_rect = sp->sub.rects[i];

                        // 修正字幕矩形的边界
                        sub_rect->x = av_clip(sub_rect->x, 0, sp->width);
                        sub_rect->y = av_clip(sub_rect->y, 0, sp->height);
                        sub_rect->w = av_clip(sub_rect->w, 0, sp->width - sub_rect->x);
                        sub_rect->h = av_clip(sub_rect->h, 0, sp->height - sub_rect->y);

                        // 获取或创建字幕转换上下文
                        is->sub_convert_ctx = sws_getCachedContext(is->sub_convert_ctx,
                                                                   sub_rect->w, sub_rect->h, AV_PIX_FMT_PAL8,
                                                                   sub_rect->w, sub_rect->h, AV_PIX_FMT_BGRA,
                                                                   0, NULL, NULL, NULL);
                        if (!is->sub_convert_ctx) {
                            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
                            return;
                        }

                        // 锁定纹理并进行图像转换
                        if (!SDL_LockTexture(is->sub_texture, (SDL_Rect *)sub_rect, (void **)pixels, pitch)) {
                            sws_scale(is->sub_convert_ctx, (const uint8_t * const *)sub_rect->data, sub_rect->linesize,
                                      0, sub_rect->h, pixels, pitch);
                            SDL_UnlockTexture(is->sub_texture);
                        }
                    }
                    // 标记字幕帧已上传
                    sp->uploaded = 1;
                }
            }
            else
                sp = NULL; // 如果当前视频帧未到显示时间，设置字幕帧为 NULL
        }
    }

    // 计算视频显示区域的矩形
    calculate_display_rect(&rect, is->xleft, is->ytop, is->width, is->height, vp->width, vp->height, vp->sar);

    // 如果视频帧尚未上传
    if (!vp->uploaded) {
        int sdl_pix_fmt = vp->frame->format == AV_PIX_FMT_YUV420P ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888;

        // 重新分配视频纹理
        if (realloc_texture(&is->vid_texture, sdl_pix_fmt, vp->frame->width, vp->frame->height, SDL_BLENDMODE_NONE, 0) < 0)
            return;

        // 上传视频纹理数据
        if (upload_texture(is->vid_texture, vp->frame, &is->img_convert_ctx) < 0)
            return;

        // 标记视频帧已上传
        vp->uploaded = 1;

        // 设置是否需要垂直翻转
        vp->flip_v = vp->frame->linesize[0] < 0;

        // 通知宽高变化
        if (m_nFrameW != vp->frame->width || m_nFrameH != vp->frame->height)
        {
            m_nFrameW = vp->frame->width;
            m_nFrameH = vp->frame->height;
            emit SigFrameDimensionsChanged(m_nFrameW, m_nFrameH);
        }
    }

    // 渲染视频纹理
    SDL_RenderCopyEx(renderer, is->vid_texture, NULL, &rect, 0, NULL, (SDL_RendererFlip)(vp->flip_v ? SDL_FLIP_VERTICAL : 0));

    // 如果有字幕帧，则渲染字幕纹理
    if (sp) {
        SDL_RenderCopy(renderer, is->sub_texture, NULL, &rect);
    }
}

// 关闭流对应的解码器等资源
void VideoCtl::stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic; // 媒体格式上下文
    AVCodecParameters *codecpar;

    // 检查流索引是否合法
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;

    codecpar = ic->streams[stream_index]->codecpar; // 获取流的编解码参数

    // 根据流的类型执行相应的关闭操作
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO: // 处理音频流
        // 终止音频解码器并关闭音频
        decoder_abort(&is->auddec, &is->sampq);
        SDL_CloseAudio();
        // 销毁音频解码器
        decoder_destroy(&is->auddec);
        // 释放音频重采样上下文
        swr_free(&is->swr_ctx);
        // 释放音频缓冲区
        av_freep(&is->audio_buf1);
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;

        // 如果存在离散傅里叶变换上下文，释放相关资源
        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
        break;

    case AVMEDIA_TYPE_VIDEO: // 处理视频流
        // 终止视频解码器
        decoder_abort(&is->viddec, &is->pictq);
        // 销毁视频解码器
        decoder_destroy(&is->viddec);
        break;

    case AVMEDIA_TYPE_SUBTITLE: // 处理字幕流
        // 终止字幕解码器
        decoder_abort(&is->subdec, &is->subpq);
        // 销毁字幕解码器
        decoder_destroy(&is->subdec);
        break;

    default:
        break; // 对于其他类型的流，什么也不做
    }

    // 设置流的丢弃策略，丢弃所有数据
    ic->streams[stream_index]->discard = AVDISCARD_ALL;

    // 根据流的类型清理相关状态
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;

    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;

    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;

    default:
        break; // 对于其他类型的流，什么也不做
    }
}

// 关闭视频流和释放相关资源
void VideoCtl::stream_close(VideoState *is)
{
    // 设置请求中止标志并等待读取线程结束
    is->abort_request = 1;
    is->read_tid.join();

    // 关闭每个流
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        stream_component_close(is, is->subtitle_stream);

    // 关闭输入格式上下文
    avformat_close_input(&is->ic);

    // 销毁视频、音频和字幕的包队列
    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
    packet_queue_destroy(&is->subtitleq);

    // 释放所有帧队列
    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);
    frame_queue_destory(&is->subpq);

    // 销毁条件变量
    SDL_DestroyCond(is->continue_read_thread);
    // 释放图像转换上下文
    sws_freeContext(is->img_convert_ctx);
    sws_freeContext(is->sub_convert_ctx);
    // 释放文件名缓冲区
    av_free(is->filename);

    // 销毁视频和字幕纹理
    if (is->vid_texture)
        SDL_DestroyTexture(is->vid_texture);
    if (is->sub_texture)
        SDL_DestroyTexture(is->sub_texture);

    // 释放视频状态结构体
    av_free(is);
}

// 获取时钟的当前时间
double VideoCtl::get_clock(Clock *c)
{
    // 如果时钟序列号与队列序列号不匹配，返回 NAN
    if (*c->queue_serial != c->serial)
        return NAN;

    // 如果时钟处于暂停状态，返回当前时间戳
    if (c->paused) {
        return c->pts;
    } else {
        // 计算当前相对时间
        double time = av_gettime_relative() / 1000000.0;
        // 返回时钟时间戳加上漂移值
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

// 设置时钟的时间戳和序列号，并记录更新时间
void VideoCtl::set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

// 设置时钟的时间戳，并使用当前时间作为更新时间
void VideoCtl::set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

// 设置时钟的速度，并更新时钟
void VideoCtl::set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

// 初始化时钟
void VideoCtl::init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

// 将时钟同步到从属时钟
void VideoCtl::sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    // 如果从属时钟有效且两者差异大于阈值，则同步时钟
    if (!std::isnan(slave_clock) && (std::isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

// 获取浮点属性（目前未实现）
float VideoCtl::ffp_get_property_float(int id, float default_value)
{
    return 0;
}

// 设置浮点属性
void VideoCtl::ffp_set_property_float(int id, float value)
{
    switch (id) {
    case FFP_PROP_FLOAT_PLAYBACK_RATE:
        ffp_set_playback_rate(value); // 设置播放速率
        break;
        // 其他属性暂未实现
    default:
        return;
    }
}

// 增加播放速度
void VideoCtl::OnSpeed()
{
    pf_playback_rate += PLAYBACK_RATE_SCALE; // 每次增加刻度
    if (pf_playback_rate > PLAYBACK_RATE_MAX)
    {
        pf_playback_rate = PLAYBACK_RATE_MIN; // 超过最大值时重置
    }
    pf_playback_rate_changed = 1;
    emit SigSpeed(pf_playback_rate); // 发出速度变化信号
}

// 获取主同步类型
int VideoCtl::get_master_sync_type(VideoState *is) {
    // 根据同步类型选择主同步源
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    }
    else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    }
    else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* 获取当前主时钟的值 */
double VideoCtl::get_master_clock(VideoState *is)
{
    double val;

    // 根据主同步类型获取主时钟的值
    switch (get_master_sync_type(is)) {
    case AV_SYNC_VIDEO_MASTER:
        val = get_clock(&is->vidclk); // 视频时钟为主时钟
        break;
    case AV_SYNC_AUDIO_MASTER:
        val = get_clock(&is->audclk); // 音频时钟为主时钟
        break;
    default:
        val = get_clock(&is->extclk); // 外部时钟为主时钟
        break;
    }
    return val;
}

/* 检查外部时钟速度并调整 */
void VideoCtl::check_external_clock_speed(VideoState *is) {
    // 如果视频或音频队列中的包数小于最小帧数，减少外部时钟速度
    if (is->video_stream >= 0 && is->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES ||
            is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) {
        set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
    }
    // 如果视频或音频队列中的包数大于最大帧数，增加外部时钟速度
    else if ((is->video_stream < 0 || is->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
             (is->audio_stream < 0 || is->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
        set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
    }
    // 否则，根据当前速度调整外部时钟速度
    else {
        double speed = is->extclk.speed;
        if (speed != 1.0)
            set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
    }
}

/* 在流中进行查找 */
void VideoCtl::stream_seek(VideoState *is, int64_t pos, int64_t rel)
{
    // 如果没有寻求请求，则设置寻求位置和标志，并通知读取线程
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

/* 切换视频的暂停或恢复状态 */
void VideoCtl::stream_toggle_pause(VideoState *is)
{
    if (is->paused) {
        // 计算暂停期间经过的时间，并更新时钟
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
        if (is->read_pause_return != AVERROR(ENOSYS)) {
            is->vidclk.paused = 0;
        }
        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
    }
    // 同步外部时钟
    set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    // 切换暂停状态
    is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
}

/* 切换暂停状态，并重置步进标志 */
void VideoCtl::toggle_pause(VideoState *is)
{
    stream_toggle_pause(is);
    is->step = 0;
}

/* 步进到下一帧 */
void VideoCtl::step_to_next_frame(VideoState *is)
{
    // 如果流处于暂停状态，则先恢复播放，然后执行步进
    if (is->paused)
        stream_toggle_pause(is);
    is->step = 1;
}

/* 计算目标延迟时间，以跟随主同步源 */
double VideoCtl::compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff = 0;

    // 更新延迟时间以跟随主同步源
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        // 如果视频是从属的，我们尝试通过复制或删除帧来纠正大延迟
        diff = get_clock(&is->vidclk) - get_master_clock(is);

        // 跳过或重复帧。我们考虑到延迟来计算阈值
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!std::isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
           delay, -diff);

    return delay;
}

/* 计算视频帧的持续时间 */
double VideoCtl::vp_duration(VideoState *is, Frame *vp, Frame *nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (std::isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            return vp->duration / pf_playback_rate;
        else
            return duration / pf_playback_rate;
    }
    else {
        return 0.0;
    }
}

/* 更新当前视频的时间戳 */
void VideoCtl::update_video_pts(VideoState *is, double pts, int64_t pos, int serial) {
    // 更新视频时钟的时间戳
    set_clock(&is->vidclk, pts / pf_playback_rate, serial);
    // 同步外部时钟
    sync_clock_to_slave(&is->extclk, &is->vidclk);
}

/* 设置播放速率 */
void VideoCtl::ffp_set_playback_rate(float rate)
{
    pf_playback_rate = rate;
    pf_playback_rate_changed = 1;
}

/* 获取当前播放速率 */
float VideoCtl::ffp_get_playback_rate()
{
    return pf_playback_rate;
}

/* 获取播放速率是否发生变化 */
int VideoCtl::ffp_get_playback_rate_change()
{
    return pf_playback_rate_changed;
}

/* 设置播放速率变化标志 */
void VideoCtl::ffp_set_playback_rate_change(int change)
{
    pf_playback_rate_changed = change;
}

/* 获取目标音频频率 */
int64_t VideoCtl::get_target_frequency()
{
    if (m_CurStream)
    {
        return m_CurStream->audio_tgt.freq;
    }
    return 44100;       // 默认频率
}

/* 获取目标音频通道数 */
int VideoCtl::get_target_channels()
{
    if (m_CurStream)
    {
        return m_CurStream->audio_tgt.channels;     // 目前变速只支持1/2通道
    }
    return 2;       // 默认通道数
}

/* 判断是否为正常播放速率 */
int VideoCtl::is_normal_playback_rate()
{
    if (pf_playback_rate > 0.99 && pf_playback_rate < 1.01)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* 用于显示每一帧 */
void VideoCtl::video_refresh(void *opaque, double *remaining_time)
{
    VideoState *is = (VideoState *)opaque; // 将传入的参数转换为 VideoState 指针
    double time;

    Frame *sp, *sp2;

    double rdftspeed = 0.02; // 设置 RDFT 速度

    // 如果未暂停且主同步类型为外部时钟，检查并调整外部时钟速度
    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        check_external_clock_speed(is);

    if (is->video_st) { // 如果存在视频流
retry:
        // 如果帧队列为空，则什么都不做
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            // 空队列，什么都不做
        }
        else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            /* 从队列中取出图片 */
            lastvp = frame_queue_peek_last(&is->pictq); // 获取上一帧
            vp = frame_queue_peek(&is->pictq); // 获取当前帧

            // 如果当前帧的序列号与视频队列的序列号不匹配，跳过并重试
            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                goto retry;
            }

            // 如果当前帧的序列号与上一帧不同，更新帧计时器
            if (lastvp->serial != vp->serial)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused)
                goto display; // 如果视频暂停，跳到显示逻辑

            /* 计算名义上的 last_duration */
            last_duration = vp_duration(is, lastvp, vp); // 计算上一帧与当前帧的持续时间
            delay = compute_target_delay(last_duration, is); // 计算目标延迟

            time = av_gettime_relative() / 1000000.0; // 获取当前时间
            // 如果当前时间还没到达预期的帧时间，加上延迟
            if (time < is->frame_timer + delay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            is->frame_timer += delay; // 更新帧计时器
            // 如果延迟大于0且当前时间超出预期帧时间较长，重置帧计时器
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            // 锁定图片队列的互斥锁
            SDL_LockMutex(is->pictq.mutex);
            if (!std::isnan(vp->pts))
                update_video_pts(is, vp->pts, vp->pos, vp->serial); // 更新视频时间戳
            SDL_UnlockMutex(is->pictq.mutex);

            // 如果队列中还有多于1帧，计算下一帧的持续时间并根据条件丢弃帧
            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame *nextvp = frame_queue_peek_next(&is->pictq);
                duration = vp_duration(is, vp, nextvp);
                if (!is->step && (framedrop > 0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
                    is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    goto retry;
                }
            }

            // 处理字幕显示
            if (is->subtitle_st) {
                while (frame_queue_nb_remaining(&is->subpq) > 0) {
                    sp = frame_queue_peek(&is->subpq);

                    if (frame_queue_nb_remaining(&is->subpq) > 1)
                        sp2 = frame_queue_peek_next(&is->subpq);
                    else
                        sp2 = NULL;

                    // 如果字幕的显示时间已过期，或者下一帧的显示时间已过期，移除字幕
                    if (sp->serial != is->subtitleq.serial
                            || (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
                            || (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
                    {
                        if (sp->uploaded) {
                            int i;
                            for (i = 0; i < sp->sub.num_rects; i++) {
                                AVSubtitleRect *sub_rect = sp->sub.rects[i];
                                uint8_t *pixels;
                                int pitch, j;

                                // 清除已上传字幕的区域
                                if (!SDL_LockTexture(is->sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch)) {
                                    for (j = 0; j < sub_rect->h; j++, pixels += pitch)
                                        memset(pixels, 0, sub_rect->w << 2);
                                    SDL_UnlockTexture(is->sub_texture);
                                }
                            }
                        }
                        frame_queue_next(&is->subpq);
                    }
                    else {
                        break;
                    }
                }
            }

            frame_queue_next(&is->pictq); // 显示当前帧
            is->force_refresh = 1;

            // 如果在步进模式且未暂停，则切换到暂停状态
            if (is->step && !is->paused)
                stream_toggle_pause(is);
        }
display:
        /* 显示图片 */
        if (is->force_refresh && is->pictq.rindex_shown)
            video_display(is);
    }
    is->force_refresh = 0;

    // 发出信号，表示视频播放的秒数（考虑播放速率）
    emit SigVideoPlaySeconds(get_master_clock(is) * pf_playback_rate);
}

/* 将解码后的视频帧添加到视频帧队列 */
int VideoCtl::queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    Frame *vp;

    // 获取队列中可写的帧，如果队列满则返回 -1
    if (!(vp = frame_queue_peek_writable(&is->pictq)))
        return -1;

    // 设置帧的宽度、高度、格式以及其他信息
    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    // 将源帧的内容移动到目标帧
    av_frame_move_ref(vp->frame, src_frame);
    // 将帧推送到帧队列
    frame_queue_push(&is->pictq);
    return 0;
}

/* 从视频队列中获取并解码视频帧 */
int VideoCtl::get_video_frame(VideoState *is, AVFrame *frame)
{
    int got_picture;

    // 解码一帧视频
    if ((got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {

        double dpts = NAN;

        // 计算解码帧的时间戳
        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        // 估算帧的样本纵横比
        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        // 判断是否丢帧的条件
        if (framedrop > 0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - get_master_clock(is);
                if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                        diff - is->frame_last_filter_delay < 0 &&
                        is->viddec.pkt_serial == is->vidclk.serial &&
                        is->videoq.nb_packets) {
                    // 丢帧并释放帧
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}

/* 音频线程，用于解码和处理音频帧 */
int VideoCtl::audio_thread(void *arg)
{
    VideoState *is = (VideoState *)arg; // 将传入的参数转换为 VideoState 指针
    AVFrame *frame = av_frame_alloc(); // 分配 AVFrame 结构体
    Frame *af;

    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM); // 如果无法分配内存，返回错误代码

    do {
        // 解码音频帧
        if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0)
            goto the_end;

        if (got_frame) {
            tb = { 1, frame->sample_rate }; // 设置音频时间基

            // 获取队列中可写的音频帧，如果队列满则跳转到结束
            if (!(af = frame_queue_peek_writable(&is->sampq)))
                goto the_end;

            // 设置音频帧的信息
            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            af->pos = av_frame_get_pkt_pos(frame);
            af->serial = is->auddec.pkt_serial;
            af->duration = av_q2d({ frame->nb_samples, frame->sample_rate });

            // 将源帧的内容移动到目标帧
            av_frame_move_ref(af->frame, frame);
            // 将音频帧推送到音频帧队列
            frame_queue_push(&is->sampq);

        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF); // 循环解码直到出错或结束

the_end:
    av_frame_free(&frame); // 释放 AVFrame 结构体
    return ret;
}

// 视频解码线程
int VideoCtl::video_thread(void *arg)
{
    VideoState *is = (VideoState *)arg;
    AVFrame *frame = av_frame_alloc(); // 分配一个 AVFrame 结构体
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base; // 获取视频时间基
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL); // 估算视频帧率

    if (!frame)
    {
        return AVERROR(ENOMEM); // 如果无法分配内存，返回错误代码
    }

    // 循环从队列中获取视频帧并处理
    for (;;) {
        ret = get_video_frame(is, frame); // 获取解码后的视频帧
        if (ret < 0)
            goto the_end; // 如果出错，跳转到结束部分
        if (!ret)
            continue; // 如果没有帧，继续循环

        // 计算帧的持续时间和时间戳
        duration = (frame_rate.num && frame_rate.den ? av_q2d({ frame_rate.den, frame_rate.num }) : 0);
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

        // 将帧添加到视频帧队列
        ret = queue_picture(is, frame, pts, duration, av_frame_get_pkt_pos(frame), is->viddec.pkt_serial);
        av_frame_unref(frame); // 释放 AVFrame 结构体的引用

        if (ret < 0)
            goto the_end; // 如果出错，跳转到结束部分
    }
the_end:

    av_frame_free(&frame); // 释放 AVFrame 结构体
    return 0;
}

// 字幕解码线程
int VideoCtl::subtitle_thread(void *arg)
{
    VideoState *is = (VideoState *)arg;
    Frame *sp;
    int got_subtitle;
    double pts;

    for (;;) {
        // 获取队列中可写的字幕帧
        if (!(sp = frame_queue_peek_writable(&is->subpq)))
            return 0; // 如果队列满，返回 0

        // 解码字幕帧
        if ((got_subtitle = decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0)
            break; // 如果出错，退出循环

        pts = 0;

        if (got_subtitle && sp->sub.format == 0) {
            // 处理有效的字幕帧
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE; // 计算字幕时间戳
            sp->pts = pts;
            sp->serial = is->subdec.pkt_serial;
            sp->width = is->subdec.avctx->width;
            sp->height = is->subdec.avctx->height;
            sp->uploaded = 0;

            // 将字幕帧添加到字幕帧队列
            frame_queue_push(&is->subpq);
        }
        else if (got_subtitle) {
            // 释放无效的字幕帧
            avsubtitle_free(&sp->sub);
        }
    }
    return 0;
}

/* 更新用于编辑窗口显示的音频样本 */
void VideoCtl::update_sample_display(VideoState *is, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index; // 计算剩余空间
        if (len > size)
            len = size; // 计算实际拷贝的长度
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short)); // 拷贝样本数据
        samples += len;
        is->sample_array_index += len; // 更新样本数组索引
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0; // 循环使用样本数组
        size -= len;
    }
}

/* 根据音频与视频同步类型调整所需的样本数，以获得更好的同步 */
int VideoCtl::synchronize_audio(VideoState *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    // 如果不是音频主时钟，则尝试通过添加或删除样本来修正时钟
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&is->audclk) - get_master_clock(is);

        if (!std::isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                // 如果测量次数不足以获得准确估计
                is->audio_diff_avg_count++;
            }
            else {
                // 估算 A-V 差异
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples); // 限制样本数在最小和最大值之间
                }
                av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                       diff, avg_diff, wanted_nb_samples - nb_samples,
                       is->audio_clock, is->audio_diff_threshold);
            }
        }
        else {
            // 差异过大，可能是初始 PTS 错误，重置 A-V 滤波器
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return wanted_nb_samples;
}

/**
 * 解码一帧音频并返回其未压缩的大小。
 *
 * 解码后的音频帧被解码、转换（如果需要），并存储在 is->audio_buf 中，返回值为字节数。
 */
int VideoCtl::audio_decode_frame(VideoState *is)
{
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

    // 如果处于暂停状态，则返回 -1
    if (is->paused)
        return -1;

    do {
#if defined(_WIN32)
        // Windows 特定代码：等待帧队列中有可读的帧
        while (frame_queue_nb_remaining(&is->sampq) == 0) {
            if ((av_gettime_relative() - audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep(1000);
        }
#endif
        // 从帧队列中获取可读的帧
        if (!(af = frame_queue_peek_readable(&is->sampq)))
            return -1;
        frame_queue_next(&is->sampq);
    } while (af->serial != is->audioq.serial);

    // 计算音频帧的数据大小
    data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
                                           af->frame->nb_samples,
                                           (AVSampleFormat)af->frame->format, 1);

    // 获取解码后的通道布局
    dec_channel_layout =
        (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
            af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

    // 如果音频参数需要改变，重新初始化采样率转换上下文
    if (af->frame->format != is->audio_src.fmt ||
            dec_channel_layout != is->audio_src.channel_layout ||
            af->frame->sample_rate != is->audio_src.freq ||
            (wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
        swr_free(&is->swr_ctx);
        is->swr_ctx = swr_alloc_set_opts(NULL,
                                         is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                                         dec_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
                                         0, NULL);
        if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "无法创建采样率转换器，将 %d Hz %s %d 个通道转换为 %d Hz %s %d 个通道失败！\n",
                   af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
                   is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        // 更新音频源参数
        is->audio_src.channel_layout = dec_channel_layout;
        is->audio_src.channels = av_frame_get_channels(af->frame);
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (AVSampleFormat)af->frame->format;
    }

    // 处理采样率转换
    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() 失败\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                     wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() 失败\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() 失败\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "音频缓冲区可能太小\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    }
    else {
        // 不需要转换，直接使用解码后的数据
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = is->audio_clock;
    /* 用 pts 更新音频时钟 */
    if (!std::isnan(af->pts))
        is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;

    return resampled_data_size;
}

/* 准备一个新的音频缓冲区 */
void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *is = (VideoState *)opaque;
    int audio_size, len1;

    VideoCtl *pVideoCtl = VideoCtl::GetInstance();

    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        // 如果音频缓冲区已处理完毕，解码新的音频帧
        if (is->audio_buf_index >= is->audio_buf_size) {        // 数据已经处理完毕了，需要读取新的
            audio_size = pVideoCtl->audio_decode_frame(is);
            if (audio_size < 0) {
                /* 如果出错，输出静音 */
                is->audio_buf = NULL;
                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
            }
            else {
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;

            // 处理播放速率变化
            if(pVideoCtl->ffp_get_playback_rate_change())
            {
                pVideoCtl->ffp_set_playback_rate_change(0);
                // 初始化
                if(pVideoCtl->audio_speed_convert)
                {
                    // 释放现有转换器
                    sonicDestroyStream(pVideoCtl->audio_speed_convert);
                }
                // 创建新的转换器
                pVideoCtl->audio_speed_convert = sonicCreateStream(pVideoCtl->get_target_frequency(),
                                                                   pVideoCtl->get_target_channels());

                // 设置变速系数
                sonicSetSpeed(pVideoCtl->audio_speed_convert, pVideoCtl->ffp_get_playback_rate());
                sonicSetPitch(pVideoCtl->audio_speed_convert, 1.0);
                sonicSetRate(pVideoCtl->audio_speed_convert, 1.0);
            }
            if(!pVideoCtl->is_normal_playback_rate() && is->audio_buf)
            {
                // 处理非正常播放速率
                int actual_out_samples = is->audio_buf_size / (is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt));
                int out_ret = 0;
                int out_size = 0;
                int num_samples = 0;
                int sonic_samples = 0;
                if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT)
                {
                    out_ret = sonicWriteFloatToStream(pVideoCtl->audio_speed_convert,
                                                      (float *)is->audio_buf,
                                                      actual_out_samples);
                }
                else if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                {
                    out_ret = sonicWriteShortToStream(pVideoCtl->audio_speed_convert,
                                                      (short *)is->audio_buf,
                                                      actual_out_samples);
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "sonic 不支持的格式\n");
                }
                num_samples = sonicSamplesAvailable(pVideoCtl->audio_speed_convert);
                // 2通道处理
                out_size = (num_samples) * av_get_bytes_per_sample(is->audio_tgt.fmt) * is->audio_tgt.channels;

                av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
                if(out_ret)
                {
                    // 从流中读取处理好的数据
                    if(is->audio_tgt.fmt == AV_SAMPLE_FMT_FLT) {
                        sonic_samples = sonicReadFloatFromStream(pVideoCtl->audio_speed_convert,
                                                                 (float *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else if(is->audio_tgt.fmt == AV_SAMPLE_FMT_S16)
                    {
                        sonic_samples = sonicReadShortFromStream(pVideoCtl->audio_speed_convert,
                                                                 (short *)is->audio_buf1,
                                                                 num_samples);
                    }
                    else
                    {
                        av_log(NULL, AV_LOG_ERROR, "sonic 不支持的格式\n");
                    }
                    is->audio_buf = is->audio_buf1;
                    is->audio_buf_size = sonic_samples * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
                    is->audio_buf_index = 0;
                }
            }
        }

        if(is->audio_buf_size == 0)
            continue;

        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            if (is->audio_buf)
                SDL_MixAudio(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1, is->audio_volume);
        }
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* 假设 SDL 使用的音频驱动有两个周期。 */
    if (!std::isnan(is->audio_clock)) {
        double audio_clock = is->audio_clock / pVideoCtl->ffp_get_playback_rate();
        pVideoCtl->set_clock_at(&is->audclk, audio_clock  - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        pVideoCtl->sync_clock_to_slave(&is->extclk, &is->audclk);
    }
}

int VideoCtl::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate,
                         struct AudioParams *audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
    static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    // 从环境变量获取音频通道数（如果设置了）
    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }

    // 校验通道布局
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX; // 去除立体声下混
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);

    // 配置 SDL 音频规格
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }

    // 确定音频回调的缓冲区大小
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS; // 设置音频格式为 16-bit signed little-endian
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback; // 设置回调函数
    wanted_spec.userdata = opaque;

    // 打开音频设备
    while (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());

        // 尝试不同的通道数
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            // 尝试不同的采样率
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR, "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }

    // 更新音频通道布局
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log(NULL, AV_LOG_ERROR, "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }

    // 设定音频播放参数
    switch (spec.format)
    {
    case AUDIO_U8:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    case AUDIO_S16LSB:
    case AUDIO_S16MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
        break;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_S32;
        break;
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
        audio_hw_params->fmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        audio_hw_params->fmt = AV_SAMPLE_FMT_U8;
        break;
    }

    // 更新音频参数
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels = spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);

    // 检查计算结果
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
}

int VideoCtl::stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;          // 输入的 AVFormatContext
    AVCodecContext *avctx;                 // 解码上下文
    AVCodec *codec;                        // 解码器
    const char *forced_codec_name = NULL;  // 强制使用的编解码器名称
    AVDictionary *opts = NULL;             // 编解码器选项
    AVDictionaryEntry *t = NULL;           // 选项字典条目
    int sample_rate, nb_channels;          // 音频采样率和通道数
    int64_t channel_layout;                // 音频通道布局
    int ret = 0;                           // 返回值
    int stream_lowres = 0;                 // 低分辨率设置

    // 检查流索引有效性
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    // 初始化解码上下文
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    // 从流的编码参数中初始化解码上下文
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;
    av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);

    // 查找解码器
    codec = avcodec_find_decoder(avctx->codec_id);

    // 记录流类型
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO: is->last_audio_stream = stream_index; break;
    case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; break;
    case AVMEDIA_TYPE_VIDEO: is->last_video_stream = stream_index; break;
    }

    avctx->codec_id = codec->id;

    // 设置低分辨率
    if (stream_lowres > av_codec_get_max_lowres(codec)) {
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
               av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);

#if FF_API_EMU_EDGE
    if (stream_lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif

#if FF_API_EMU_EDGE
    if (codec->capabilities & AV_CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
#endif

    opts = nullptr; // 这里是空指针，如果有自定义的编解码器选项可以设置
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);

    // 打开解码器
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        goto fail;
    }
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    is->eof = 0;  // 标记流是否结束
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT; // 默认丢弃策略

    // 根据流类型进行初始化
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        sample_rate = avctx->sample_rate;
        nb_channels = avctx->channels;
        channel_layout = avctx->channel_layout;

        /* 准备音频输出 */
        if ((ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;

        /* 初始化平均滤波器 */
        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        // 创建音频解码线程
        decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        packet_queue_start(is->auddec.queue);
        is->auddec.decode_thread = std::thread(&VideoCtl::audio_thread, this, is);
        SDL_PauseAudio(0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        // 创建视频解码线程
        decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
        packet_queue_start(is->viddec.queue);
        is->viddec.decode_thread = std::thread(&VideoCtl::video_thread, this, is);
        is->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];

        // 创建字幕解码线程
        decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread);
        packet_queue_start(is->subdec.queue);
        is->subdec.decode_thread = std::thread(&VideoCtl::subtitle_thread, this, is);
        break;
    default:
        break;
    }

    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    av_dict_free(&opts);

    return ret;
}

int decode_interrupt_cb(void *ctx)
{
    VideoState *is = (VideoState *)ctx;
    return is->abort_request;
}

int VideoCtl::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
            queue->abort_request ||
            (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
            queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

int VideoCtl::is_realtime(AVFormatContext *s)
{
    if (!strcmp(s->iformat->name, "rtp")
            || !strcmp(s->iformat->name, "rtsp")
            || !strcmp(s->iformat->name, "sdp")
            )
        return 1;

    if (s->pb && (!strncmp(s->filename, "rtp:", 4)
                  || !strncmp(s->filename, "udp:", 4)
                  )
            )
        return 1;
    return 0;
}

/* this thread gets the stream from the disk or the network */
//读取线程
void VideoCtl::ReadThread(VideoState *is)
{
    // 初始化本地变量
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;
    AVDictionary **opts;
    int orig_nb_streams;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;

    const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };

    // 创建互斥锁
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    // 初始化流索引
    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->eof = 0;

    // 分配封装格式上下文
    ic = avformat_alloc_context();
    if (!ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;

    // 打开输入文件并获取封装信息
    err = avformat_open_input(&ic, is->filename, is->iformat, nullptr/*&format_opts*/);
    if (err < 0) {
        ret = -1;
        goto fail;
    }
    is->ic = ic;

    // 注入全局侧数据
    av_format_inject_global_side_data(ic);

    opts = nullptr; // 配置解码器选项
    orig_nb_streams = ic->nb_streams;

    // 查找流信息
    err = avformat_find_stream_info(ic, opts);
    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING, "%s: could not find codec parameters\n", is->filename);
        ret = -1;
        goto fail;
    }

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    is->realtime = is_realtime(ic);

    emit SigVideoTotalSeconds(ic->duration / 1000000LL);

    // 根据指定的流类型初始化流索引
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)
                st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if (wanted_stream_spec[(AVMediaType)i] && st_index[(AVMediaType)i] == -1) {
            av_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", wanted_stream_spec[(AVMediaType)i], av_get_media_type_string((AVMediaType)i));
            st_index[(AVMediaType)i] = INT_MAX;
        }
    }

    // 查找最佳的视频、音频和字幕流
    st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);

    st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                st_index[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                     st_index[AVMEDIA_TYPE_AUDIO] :
                                     st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
    }

    // 打开音频、视频和字幕流
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n", is->filename);
        ret = -1;
        goto fail;
    }

    if (infinite_buffer < 0 && is->realtime)
        infinite_buffer = 1;

    // 主循环：读取数据包并将其存入队列
    for (;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }

        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "%s: error while seeking\n", is->ic->filename);
            }
            else {
                if (is->audio_stream >= 0) {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }
                if (is->subtitle_stream >= 0) {
                    packet_queue_flush(&is->subtitleq);
                    packet_queue_put(&is->subtitleq, &flush_pkt);
                }
                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                    set_clock(&is->extclk, NAN, 0);
                }
                else {
                    set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            if (is->paused)
                step_to_next_frame(is);
        }
        if (is->queue_attachments_req) {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                AVPacket copy;
                if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, &copy);
                packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer < 1 &&
                (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
                 || (stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) &&
                     stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
                     stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (!is->paused &&
                (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
                (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {

            //播放结束
            emit SigStop();
            continue;
        }
        //按帧读取
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
                is->eof = 1;
            }
            if (ic->pb && ic->pb->error)
                break;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        else {
            is->eof = 0;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = AV_NOPTS_VALUE == AV_NOPTS_VALUE ||
                (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(0) / 1000000
                <= ((double)AV_NOPTS_VALUE / 1000000);
        //按数据帧的类型存放至对应队列
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        }
        else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                 && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&is->videoq, pkt);
        }
        else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
        }
        else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return ;
}

VideoState* VideoCtl::stream_open(const char *filename)
{
    VideoState *is;
    // 分配并初始化 VideoState 结构体
    is = (VideoState *)av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    // 复制文件名
    is->filename = av_strdup(filename);
    if (!is->filename)
        goto fail;

    // 初始化视频和字幕的位置
    is->ytop = 0;
    is->xleft = 0;

    /* start video display */
    // 初始化视频帧队列
    if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    // 初始化字幕帧队列
    if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
    // 初始化音频帧队列
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;
    // 初始化数据包队列
    if (packet_queue_init(&is->videoq) < 0 ||
            packet_queue_init(&is->audioq) < 0 ||
            packet_queue_init(&is->subtitleq) < 0)
        goto fail;

    // 创建继续读取线程的条件变量
    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        goto fail;
    }

    // 初始化时钟
    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    init_clock(&is->extclk, &is->extclk.serial);
    is->audio_clock_serial = -1;

    // 设置音量
    if (startup_volume < 0)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d < 0, setting to 0\n", startup_volume);
    if (startup_volume > 100)
        av_log(NULL, AV_LOG_WARNING, "-volume=%d > 100, setting to 100\n", startup_volume);
    startup_volume = av_clip(startup_volume, 0, 100);
    startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = startup_volume;

    emit SigVideoVolume(startup_volume * 1.0 / SDL_MIX_MAXVOLUME);
    emit SigPauseStat(is->paused);

    is->av_sync_type = AV_SYNC_AUDIO_MASTER;

    // 创建并启动读取线程
    is->read_tid = std::thread(&VideoCtl::ReadThread, this, is);

    return is;

fail:
    stream_close(is);
    return NULL;
}

void VideoCtl::stream_cycle_channel(VideoState *is, int codec_type)
{
    AVFormatContext *ic = is->ic;
    int start_index, stream_index;
    int old_index;
    AVStream *st;
    AVProgram *p = NULL;
    int nb_streams = is->ic->nb_streams;

    // 根据传入的 codec_type 确定流的开始索引和旧索引
    if (codec_type == AVMEDIA_TYPE_VIDEO) {
        start_index = is->last_video_stream;
        old_index = is->video_stream;
    }
    else if (codec_type == AVMEDIA_TYPE_AUDIO) {
        start_index = is->last_audio_stream;
        old_index = is->audio_stream;
    }
    else {
        start_index = is->last_subtitle_stream;
        old_index = is->subtitle_stream;
    }
    stream_index = start_index;

    // 检查是否需要更新流的起始索引
    if (codec_type != AVMEDIA_TYPE_VIDEO && is->video_stream != -1) {
        p = av_find_program_from_stream(ic, NULL, is->video_stream);
        if (p) {
            nb_streams = p->nb_stream_indexes;
            for (start_index = 0; start_index < nb_streams; start_index++)
                if (p->stream_index[start_index] == stream_index)
                    break;
            if (start_index == nb_streams)
                start_index = -1;
            stream_index = start_index;
        }
    }

    // 查找新的流索引
    for (;;) {
        if (++stream_index >= nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                is->last_subtitle_stream = -1;
                goto the_end;
            }
            if (start_index == -1)
                return;
            stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = is->ic->streams[p ? p->stream_index[stream_index] : stream_index];
        if (st->codecpar->codec_type == codec_type) {
            /* check that parameters are OK */
            switch (codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codecpar->sample_rate != 0 &&
                        st->codecpar->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
the_end:
    if (p && stream_index != -1)
        stream_index = p->stream_index[stream_index];
    av_log(NULL, AV_LOG_INFO, "Switch %s stream from #%d to #%d\n",
           av_get_media_type_string((AVMediaType)codec_type),
           old_index,
           stream_index);

    // 关闭旧流并打开新流
    stream_component_close(is, old_index);
    stream_component_open(is, stream_index);
}

void VideoCtl::refresh_loop_wait_event(VideoState *is, SDL_Event *event) {
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) && m_bPlayLoop)
    {
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (!is->paused || is->force_refresh)
            video_refresh(is, &remaining_time);
        SDL_PumpEvents();
    }
}

// 跳转到指定章节
void VideoCtl::seek_chapter(VideoState *is, int incr)
{
    // 获取当前播放时间的时间戳（以微秒为单位）
    int64_t pos = get_master_clock(is) * AV_TIME_BASE;
    int i;

    // 如果没有章节信息，直接返回
    if (!is->ic->nb_chapters)
        return;

    // 查找当前章节
    for (i = 0; i < is->ic->nb_chapters; i++) {
        AVChapter *ch = is->ic->chapters[i];
        // 比较当前播放时间与章节开始时间
        if (av_compare_ts(pos, /*AV_TIME_BASE_Q*/{ 1, AV_TIME_BASE }, ch->start, ch->time_base) < 0) {
            i--;
            break;
        }
    }

    // 根据增量调整章节索引
    i += incr;
    i = FFMAX(i, 0); // 确保索引不小于0
    // 如果章节索引超出范围，直接返回
    if (i >= is->ic->nb_chapters)
        return;

    // 打印日志：正在跳转到指定章节
    av_log(NULL, AV_LOG_VERBOSE, "Seeking to chapter %d.\n", i);
    // 跳转到指定章节的开始时间
    stream_seek(is, av_rescale_q(is->ic->chapters[i]->start, is->ic->chapters[i]->time_base,
                                 /*AV_TIME_BASE_Q*/{ 1, AV_TIME_BASE }), 0);
}

// 播放控制循环线程
void VideoCtl::LoopThread(VideoState *cur_stream)
{
    SDL_Event event;
    double incr, pos, frac;

    m_bPlayLoop = true; // 标记播放循环状态为真

    while (m_bPlayLoop)
    {
        double x;
        // 等待并处理事件
        refresh_loop_wait_event(cur_stream, &event);
        switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_s: // S键：跳转到下一帧
                step_to_next_frame(cur_stream);
                break;
            case SDLK_a: // A键：切换音频流
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v: // V键：切换视频流
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_c: // C键：切换视频流、音频流和字幕流
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_t: // T键：切换字幕流
                stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                break;

            default:
                break;
            }
            break;
        case SDL_WINDOWEVENT:
            // 处理窗口事件
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED: // 窗口大小改变事件
                screen_width = cur_stream->width = event.window.data1;
                screen_height = cur_stream->height = event.window.data2;
                break;
            case SDL_WINDOWEVENT_EXPOSED: // 窗口暴露事件
                cur_stream->force_refresh = 1;
                break;
            }
            break;
        case SDL_QUIT: // 处理退出事件
        case FF_QUIT_EVENT:
            do_exit(cur_stream);
            break;
        default:
            break;
        }
    }

    // 退出播放并释放资源
    do_exit(m_CurStream);
    //m_CurStream = nullptr;
}

// 互斥锁管理函数
int lockmgr(void **mtx, enum AVLockOp op)
{
    switch (op) {
    case AV_LOCK_CREATE:
        // 创建互斥锁
        *mtx = SDL_CreateMutex();
        if (!*mtx) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
            return 1;
        }
        return 0;
    case AV_LOCK_OBTAIN:
        // 获取互斥锁
        return !!SDL_LockMutex((SDL_mutex *)*mtx);
    case AV_LOCK_RELEASE:
        // 释放互斥锁
        return !!SDL_UnlockMutex((SDL_mutex *)*mtx);
    case AV_LOCK_DESTROY:
        // 销毁互斥锁
        SDL_DestroyMutex((SDL_mutex *)*mtx);
        return 0;
    }
    return 1;
}

// 根据百分比调整播放进度
void VideoCtl::OnPlaySeek(double dPercent)
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    // 计算目标时间戳
    int64_t ts = dPercent * m_CurStream->ic->duration;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE)
        ts += m_CurStream->ic->start_time;
    // 跳转到指定位置
    stream_seek(m_CurStream, ts, 0);
}

// 根据百分比调整播放音量
void VideoCtl::OnPlayVolume(double dPercent)
{
    startup_volume = dPercent * SDL_MIX_MAXVOLUME;
    if (m_CurStream == nullptr)
    {
        return;
    }
    m_CurStream->audio_volume = startup_volume;
}

// 向前跳转播放位置
void VideoCtl::OnSeekForward()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    double incr = 5.0; // 跳转增量（秒）
    double pos = get_master_clock(m_CurStream);
    if (std::isnan(pos))
        pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
        pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
    // 跳转到新的播放位置
    stream_seek(m_CurStream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}

// 向后跳转播放位置
void VideoCtl::OnSeekBack()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    double incr = -5.0; // 跳转增量（秒）
    double pos = get_master_clock(m_CurStream);
    if (std::isnan(pos))
        pos = (double)m_CurStream->seek_pos / AV_TIME_BASE;
    pos += incr;
    if (m_CurStream->ic->start_time != AV_NOPTS_VALUE && pos < m_CurStream->ic->start_time / (double)AV_TIME_BASE)
        pos = m_CurStream->ic->start_time / (double)AV_TIME_BASE;
    // 跳转到新的播放位置
    stream_seek(m_CurStream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE));
}

// 更新音量
void VideoCtl::UpdateVolume(int sign, double step)
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    // 计算当前音量的分贝值
    double volume_level = m_CurStream->audio_volume ? (20 * log(m_CurStream->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
    // 计算新的音量值
    int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
    m_CurStream->audio_volume = av_clip(m_CurStream->audio_volume == new_volume ? (m_CurStream->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);

    // 发出信号更新音量显示
    emit SigVideoVolume(m_CurStream->audio_volume * 1.0 / SDL_MIX_MAXVOLUME);
}

/* 显示当前图像（如果有的话） */
void VideoCtl::video_display(VideoState *is)
{
    // 如果窗口未创建，创建窗口
    if (!window)
        video_open(is);

    if (renderer)
    {
        // 如果显示控件的大小正在变化，则不刷新显示
        if (g_show_rect_mutex.tryLock())
        {
            // 设置渲染器的绘制颜色为黑色
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            // 清除当前渲染器的显示
            SDL_RenderClear(renderer);
            // 显示视频图像
            video_image_display(is);
            // 显示渲染的图像
            SDL_RenderPresent(renderer);

            // 解锁互斥锁
            g_show_rect_mutex.unlock();
        }
    }
}

/* 打开视频显示窗口 */
int VideoCtl::video_open(VideoState *is)
{
    int w, h;

    w = screen_width;
    h = screen_height;

    // 如果窗口未创建
    if (!window) {
        int flags = SDL_WINDOW_SHOWN;
        flags |= SDL_WINDOW_RESIZABLE;

        // 从现有窗口句柄创建窗口
        window = SDL_CreateWindowFrom((void *)play_wid);
        SDL_GetWindowSize(window, &w, &h); // 获取窗口的宽高
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); // 设置渲染缩放质量
        if (window) {
            SDL_RendererInfo info;
            // 创建硬件加速的渲染器
            if (!renderer)
                renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (!renderer) {
                av_log(NULL, AV_LOG_WARNING, "Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
                // 如果硬件加速渲染器创建失败，则创建软件渲染器
                renderer = SDL_CreateRenderer(window, -1, 0);
            }
            if (renderer) {
                // 获取并打印渲染器信息
                if (!SDL_GetRendererInfo(renderer, &info))
                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.\n", info.name);
            }
        }
    }
    else {
        // 如果窗口已存在，设置其大小
        SDL_SetWindowSize(window, w, h);
    }

    // 如果窗口或渲染器创建失败，记录错误日志并退出
    if (!window || !renderer) {
        av_log(NULL, AV_LOG_FATAL, "SDL: could not set video mode - exiting\n");
        do_exit(is);
    }

    // 设置视频状态的宽高
    is->width = w;
    is->height = h;

    return 0;
}

/* 处理退出操作，释放资源 */
void VideoCtl::do_exit(VideoState* &is)
{
    if (is)
    {
        // 关闭流并释放指针
        stream_close(is);
        is = nullptr;
    }
    if (renderer)
    {
        // 销毁渲染器
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window)
    {
        // 销毁窗口
        // SDL_DestroyWindow(window);
        window = nullptr;
    }

    // 发出停止完成信号
    emit SigStopFinished();
}

/* 增加音量 */
void VideoCtl::OnAddVolume()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    // 更新音量，增加指定的步长
    UpdateVolume(1, SDL_VOLUME_STEP);
}

/* 减少音量 */
void VideoCtl::OnSubVolume()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    // 更新音量，减少指定的步长
    UpdateVolume(-1, SDL_VOLUME_STEP);
}

/* 切换暂停/播放状态 */
void VideoCtl::OnPause()
{
    if (m_CurStream == nullptr)
    {
        return;
    }
    // 切换播放暂停状态
    toggle_pause(m_CurStream);
    // 发出暂停状态信号
    emit SigPauseStat(m_CurStream->paused);
}

/* 停止播放 */
void VideoCtl::OnStop()
{
    // 设置播放循环标志为假
    m_bPlayLoop = false;
}

/* 构造函数，初始化类成员变量 */
VideoCtl::VideoCtl(QObject *parent) :
    QObject(parent),
    m_bInited(false),
    m_CurStream(nullptr),
    m_bPlayLoop(false),
    screen_width(0),
    screen_height(0),
    startup_volume(30),
    renderer(nullptr),
    window(nullptr),
    m_nFrameW(0),
    m_nFrameH(0),
    pf_playback_rate(1.0),
    pf_playback_rate_changed(0),
    audio_speed_convert(NULL)
{
    // 注册所有复用器、编码器
    av_register_all();
    // 初始化网络格式
    avformat_network_init();
}

/* 初始化函数，设置SDL、注册锁管理器，并连接信号槽 */
bool VideoCtl::Init()
{
    // 如果已经初始化过，则直接返回true
    if (m_bInited == true)
    {
        return true;
    }

    // 连接信号槽
    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    // 初始化SDL视频、音频和计时器模块
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        return false;
    }

    // 忽略系统和用户事件
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    // 注册自定义锁管理器
    if (av_lockmgr_register(lockmgr))
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize lock manager!\n");
        return false;
    }

    // 设置为已初始化
    m_bInited = true;

    return true;
}

/* 连接信号与槽 */
bool VideoCtl::ConnectSignalSlots()
{
    // 连接停止播放信号到OnStop槽函数
    connect(this, &VideoCtl::SigStop, &VideoCtl::OnStop);

    return true;
}

/* 静态实例化对象 */
VideoCtl *VideoCtl::m_pInstance = new VideoCtl();

/* 获取VideoCtl的实例 */
VideoCtl *VideoCtl::GetInstance()
{
    // 如果初始化失败，则返回空指针
    if (false == m_pInstance->Init())
    {
        return nullptr;
    }
    return m_pInstance;
}

/* 析构函数，释放资源 */
VideoCtl::~VideoCtl()
{
    // 注销锁管理器
    av_lockmgr_register(NULL);

    // 反初始化网络格式
    avformat_network_deinit();

    // 退出SDL
    SDL_Quit();
}

/* 启动播放，打开视频流并创建播放线程 */
bool VideoCtl::StartPlay(QString strFileName, WId widPlayWid)
{
    // 设置播放循环标志为假
    m_bPlayLoop = false;
    // 如果播放线程可连接，等待线程结束
    if (m_tPlayLoopThread.joinable())
    {
        m_tPlayLoopThread.join();
    }
    // 发送播放开始信号，通知标题栏
    emit SigStartPlay(strFileName);

    // 保存播放窗口的ID
    play_wid = widPlayWid;

    VideoState *is;

    char file_name[1024];
    memset(file_name, 0, 1024);
    // 将文件名转换为C风格字符串
    sprintf(file_name, "%s", strFileName.toLocal8Bit().data());

    // 打开视频流
    is = stream_open(file_name);
    if (!is) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize VideoState!\n");
        // 处理退出操作
        do_exit(m_CurStream);
    }

    // 设置当前流
    m_CurStream = is;

    // 创建播放循环线程
    m_tPlayLoopThread = std::thread(&VideoCtl::LoopThread, this, is);

    return true;
}
