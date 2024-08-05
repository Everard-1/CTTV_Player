/* Sonic library
   Copyright 2010
   Bill Cox
   This file is part of the Sonic Library.

   This file is licensed under the Apache 2.0 license, and also placed into the public domain.
   Use it either way, at your option.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include "sonic.h"
//#include "webrtc/base/logging.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
    The following code was used to generate the following sinc lookup table.

    #include <math.h>
    #include <limits.h>
    #include <stdio.h>

    double findHannWeight(int N, double x) {
        return 0.5*(1.0 - cos(2*M_PI*x/N));
    }

    double findSincCoefficient(int N, double x) {
        double hannWindowWeight = findHannWeight(N, x);
        double sincWeight;

        x -= N/2.0;
        if (x > 1e-9 || x < -1e-9) {
            sincWeight = sin(M_PI*x)/(M_PI*x);
        } else {
            sincWeight = 1.0;
        }
        return hannWindowWeight*sincWeight;
    }

    int main() {
        double x;
        int i;
        int N = 12;

        for (i = 0, x = 0.0; x <= N; x += 0.02, i++) {
            printf("%u %d\n", i, (int)(SHRT_MAX*findSincCoefficient(N, x)));
        }
        return 0;
    }
*/

/* 使用 sinc FIR 滤波器进行重采样的点数。 */
#define SINC_FILTER_POINTS 12 /* 增加 N 的值没有听到明显的改善。 */
#define SINC_TABLE_SIZE 601   /* sinc 表的大小 */

/* 窗口化 sinc 函数的查找表，包含 SINC_FILTER_POINTS 个点。 */
static short sincTable[SINC_TABLE_SIZE] = {
    0, 0, 0, 0, 0, 0, 0, -1, -1, -2, -2, -3, -4, -6, -7, -9, -10, -12, -14,
    -17, -19, -21, -24, -26, -29, -32, -34, -37, -40, -42, -44, -47, -48, -50,
    -51, -52, -53, -53, -53, -52, -50, -48, -46, -43, -39, -34, -29, -22, -16,
    -8, 0, 9, 19, 29, 41, 53, 65, 79, 92, 107, 121, 137, 152, 168, 184, 200,
    215, 231, 247, 262, 276, 291, 304, 317, 328, 339, 348, 357, 363, 369, 372,
    374, 375, 373, 369, 363, 355, 345, 332, 318, 300, 281, 259, 234, 208, 178,
    147, 113, 77, 39, 0, -41, -85, -130, -177, -225, -274, -324, -375, -426,
    -478, -530, -581, -632, -682, -731, -779, -825, -870, -912, -951, -989,
    -1023, -1053, -1080, -1104, -1123, -1138, -1149, -1154, -1155, -1151,
    -1141, -1125, -1105, -1078, -1046, -1007, -963, -913, -857, -796, -728,
    -655, -576, -492, -403, -309, -210, -107, 0, 111, 225, 342, 462, 584, 708,
    833, 958, 1084, 1209, 1333, 1455, 1575, 1693, 1807, 1916, 2022, 2122, 2216,
    2304, 2384, 2457, 2522, 2579, 2625, 2663, 2689, 2706, 2711, 2705, 2687,
    2657, 2614, 2559, 2491, 2411, 2317, 2211, 2092, 1960, 1815, 1658, 1489,
    1308, 1115, 912, 698, 474, 241, 0, -249, -506, -769, -1037, -1310, -1586,
    -1864, -2144, -2424, -2703, -2980, -3254, -3523, -3787, -4043, -4291,
    -4529, -4757, -4972, -5174, -5360, -5531, -5685, -5819, -5935, -6029,
    -6101, -6150, -6175, -6175, -6149, -6096, -6015, -5905, -5767, -5599,
    -5401, -5172, -4912, -4621, -4298, -3944, -3558, -3141, -2693, -2214,
    -1705, -1166, -597, 0, 625, 1277, 1955, 2658, 3386, 4135, 4906, 5697, 6506,
    7332, 8173, 9027, 9893, 10769, 11654, 12544, 13439, 14335, 15232, 16128,
    17019, 17904, 18782, 19649, 20504, 21345, 22170, 22977, 23763, 24527,
    25268, 25982, 26669, 27327, 27953, 28547, 29107, 29632, 30119, 30569,
    30979, 31349, 31678, 31964, 32208, 32408, 32565, 32677, 32744, 32767,
    32744, 32677, 32565, 32408, 32208, 31964, 31678, 31349, 30979, 30569,
    30119, 29632, 29107, 28547, 27953, 27327, 26669, 25982, 25268, 24527,
    23763, 22977, 22170, 21345, 20504, 19649, 18782, 17904, 17019, 16128,
    15232, 14335, 13439, 12544, 11654, 10769, 9893, 9027, 8173, 7332, 6506,
    5697, 4906, 4135, 3386, 2658, 1955, 1277, 625, 0, -597, -1166, -1705,
    -2214, -2693, -3141, -3558, -3944, -4298, -4621, -4912, -5172, -5401,
    -5599, -5767, -5905, -6015, -6096, -6149, -6175, -6175, -6150, -6101,
    -6029, -5935, -5819, -5685, -5531, -5360, -5174, -4972, -4757, -4529,
    -4291, -4043, -3787, -3523, -3254, -2980, -2703, -2424, -2144, -1864,
    -1586, -1310, -1037, -769, -506, -249, 0, 241, 474, 698, 912, 1115, 1308,
    1489, 1658, 1815, 1960, 2092, 2211, 2317, 2411, 2491, 2559, 2614, 2657,
    2687, 2705, 2711, 2706, 2689, 2663, 2625, 2579, 2522, 2457, 2384, 2304,
    2216, 2122, 2022, 1916, 1807, 1693, 1575, 1455, 1333, 1209, 1084, 958, 833,
    708, 584, 462, 342, 225, 111, 0, -107, -210, -309, -403, -492, -576, -655,
    -728, -796, -857, -913, -963, -1007, -1046, -1078, -1105, -1125, -1141,
    -1151, -1155, -1154, -1149, -1138, -1123, -1104, -1080, -1053, -1023, -989,
    -951, -912, -870, -825, -779, -731, -682, -632, -581, -530, -478, -426,
    -375, -324, -274, -225, -177, -130, -85, -41, 0, 39, 77, 113, 147, 178,
    208, 234, 259, 281, 300, 318, 332, 345, 355, 363, 369, 373, 375, 374, 372,
    369, 363, 357, 348, 339, 328, 317, 304, 291, 276, 262, 247, 231, 215, 200,
    184, 168, 152, 137, 121, 107, 92, 79, 65, 53, 41, 29, 19, 9, 0, -8, -16,
    -22, -29, -34, -39, -43, -46, -48, -50, -52, -53, -53, -53, -52, -51, -50,
    -48, -47, -44, -42, -40, -37, -34, -32, -29, -26, -24, -21, -19, -17, -14,
    -12, -10, -9, -7, -6, -4, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0
};

/* 结构体定义 sonicStreamStruct，用于处理音频流的各种属性和缓冲区 */
struct sonicStreamStruct {
    short *inputBuffer;         /* 输入缓冲区指针 */
    short *outputBuffer;        /* 输出缓冲区指针 */
    short *pitchBuffer;         /* 音高缓冲区指针 */
    short *downSampleBuffer;    /* 降采样缓冲区指针 */
    float speed;                /* 速度因子 */
    float volume;               /* 音量因子 */
    float pitch;                /* 音高因子 */
    float rate;                 /* 采样率因子 */
    int oldRatePosition;        /* 旧的采样率位置 */
    int newRatePosition;        /* 新的采样率位置 */
    int useChordPitch;          /* 是否使用和声音高 */
    int quality;                /* 质量等级 */
    int numChannels;            /* 声道数 */
    int inputBufferSize;        /* 输入缓冲区大小 */
    int pitchBufferSize;        /* 音高缓冲区大小 */
    int outputBufferSize;       /* 输出缓冲区大小 */
    int numInputSamples;        /* 输入样本数 */
    int numOutputSamples;       /* 输出样本数 */
    int numPitchSamples;        /* 音高样本数 */
    int minPeriod;              /* 最小周期 */
    int maxPeriod;              /* 最大周期 */
    int maxRequired;            /* 最大需求 */
    int remainingInputToCopy;   /* 剩余待复制的输入量 */
    int sampleRate;             /* 采样率 */
    int prevPeriod;             /* 上一个周期 */
    int prevMinDiff;            /* 上一个最小差值 */
    float avePower;             /* 平均功率 */
};

// 改变音量
static void scaleSamples(
    short *samples,    /* 输入的样本数组指针 */
    int numSamples,    /* 样本数量 */
    float volume)      /* 音量因子 */
{
    // 将音量因子转换为固定点格式的值
    int fixedPointVolume = volume * 4096.0f;
    int value;

    // 遍历所有样本
    while(numSamples--) {
        // 使用固定点音量因子缩放样本值，并移位调整到合适的范围
        value = (*samples * fixedPointVolume) >> 12;
        // 限制缩放后的样本值在有效范围内
        if(value > 32767) {
            value = 32767;
        } else if(value < -32767) {
            value = -32767;
        }
        // 将调整后的值写回样本数组
        *samples++ = value;
    }
}

// 得到流的速度
float sonicGetSpeed(
    sonicStream stream)
{
    return stream->speed;
}

// 设置流的速度
void sonicSetSpeed(
    sonicStream stream,
    float speed)
{
    stream->speed = speed;
}

// 得到流的音调
float sonicGetPitch(
    sonicStream stream)
{
    return stream->pitch;
}

// 设置流的音调
void sonicSetPitch(
    sonicStream stream,
    float pitch)
{
    stream->pitch = pitch;
}

// 得到流的速率
float sonicGetRate(
    sonicStream stream)
{
    return stream->rate;
}

// 设置回放流的速率，同时也重设音高和速度
void sonicSetRate(
    sonicStream stream,  /* 指向 sonicStream 结构体的指针 */
    float rate)          /* 要设置的采样率因子 */
{
    // 设置流的采样率因子
    stream->rate = rate;

    // 重置旧的采样率位置
    stream->oldRatePosition = 0;
    // 重置新的采样率位置
    stream->newRatePosition = 0;
}

/* 获取和声音高设置。 */
int sonicGetChordPitch(
    sonicStream stream)  /* 指向 sonicStream 结构体的指针 */
{
    // 返回是否使用和声音高的标志
    return stream->useChordPitch;
}


/* 设置音高计算的和声音高模式。默认为关闭。 */
void sonicSetChordPitch(
    sonicStream stream,    /* 指向 sonicStream 结构体的指针 */
    int useChordPitch)    /* 是否使用和声音高的标志 */
{
    // 设置和声音高标志
    stream->useChordPitch = useChordPitch;
}

/* 获取质量设置。 */
int sonicGetQuality(
    sonicStream stream)    /* 指向 sonicStream 结构体的指针 */
{
    // 返回质量设置
    return stream->quality;
}

/* 设置“质量”。默认值 0 几乎与 1 一样好，但速度更快。 */
void sonicSetQuality(
    sonicStream stream,    /* 指向 sonicStream 结构体的指针 */
    int quality)          /* 要设置的质量等级 */
{
    // 设置质量等级
    stream->quality = quality;
}

/* 获取流的缩放因子。 */
float sonicGetVolume(
    sonicStream stream)    /* 指向 sonicStream 结构体的指针 */
{
    // 返回流的音量因子
    return stream->volume;
}

/* 设置流的缩放因子。 */
// 设置流的音量
void sonicSetVolume(
    sonicStream stream,    /* 指向 sonicStream 结构体的指针 */
    float volume)         /* 要设置的音量因子 */
{
    // 设置音量因子
    stream->volume = volume;
}

/* 释放流内的缓冲区。 */
// 释放流内的缓冲区
static void freeStreamBuffers(
    sonicStream stream)    /* 指向 sonicStream 结构体的指针 */
{
    // 释放输入缓冲区
    if(stream->inputBuffer != NULL) {
        free(stream->inputBuffer);
    }
    // 释放输出缓冲区
    if(stream->outputBuffer != NULL) {
        free(stream->outputBuffer);
    }
    // 释放音高缓冲区
    if(stream->pitchBuffer != NULL) {
        free(stream->pitchBuffer);
    }
    // 释放降采样缓冲区
    if(stream->downSampleBuffer != NULL) {
        free(stream->downSampleBuffer);
    }
}

/* 销毁流。 */
// 销毁流
void sonicDestroyStream(
    sonicStream stream)    /* 指向 sonicStream 结构体的指针 */
{
    // 释放流内的所有缓冲区
    freeStreamBuffers(stream);
    // 释放流结构体本身
    free(stream);
}


/* Allocate stream buffers. */
/**
 * 开辟流的数据缓存空间
 * stream 流
 * sampleRate 采样率
 * numChnnels 声道数
 */
static int allocateStreamBuffers(
    sonicStream stream,
    int sampleRate,
    int numChannels)
{   // 最小的pitch周期 44100/400 = 110
    int minPeriod = sampleRate/SONIC_MAX_PITCH;
    // 最大的pitch周期 44100/65 = 678 个采样点
    int maxPeriod = sampleRate/SONIC_MIN_PITCH;
    // 最大 1356
    int maxRequired = 2*maxPeriod;
    // 输入缓冲区的大小 = maxRequired
    stream->inputBufferSize = maxRequired;
    // 为inputBuffer开辟空间并初始化为0
    stream->inputBuffer = (short *)calloc(maxRequired, sizeof(short)*numChannels);
    // 如果开辟失败返回0
    if(stream->inputBuffer == NULL) {
        sonicDestroyStream(stream);
        return 0;
    }
    // 输出缓冲区的大小= maxRequired
    stream->outputBufferSize = maxRequired;
    // 为oututBUffer开辟空间
    stream->outputBuffer = (short *)calloc(maxRequired, sizeof(short)*numChannels);
    if(stream->outputBuffer == NULL) {
        sonicDestroyStream(stream);
        return 0;
    }
    // 为pitchBuffer开辟空间
    stream->pitchBufferSize = maxRequired;
    stream->pitchBuffer = (short *)calloc(maxRequired, sizeof(short)*numChannels);
    if(stream->pitchBuffer == NULL) {
        sonicDestroyStream(stream);
        return 0;
    }
    // 为downSampleBuffer（降采样）开辟空间
    stream->downSampleBuffer = (short *)calloc(maxRequired, sizeof(short));
    if(stream->downSampleBuffer == NULL) {
        sonicDestroyStream(stream);
        return 0;
    }
    // 初始化各项参数
    stream->sampleRate = sampleRate;
    stream->numChannels = numChannels;
    stream->oldRatePosition = 0;
    stream->newRatePosition = 0;
    stream->minPeriod = minPeriod;
    stream->maxPeriod = maxPeriod;
    stream->maxRequired = maxRequired;
    stream->prevPeriod = 0;
    return 1;
}

/* Create a sonic stream.  Return NULL only if we are out of memory and cannot
   allocate the stream. */
// 创建一个音频流
sonicStream sonicCreateStream(
    int sampleRate,
    int numChannels)
{
    // 开辟一个sonicStreamStruct大小的空间
    sonicStream stream = (sonicStream)calloc(1, sizeof(struct sonicStreamStruct));
    // 如果流为空，证明开辟失败
    if(stream == NULL) {
        return NULL;
    }
    if(!allocateStreamBuffers(stream, sampleRate, numChannels)) {
        return NULL;
    }
    // 初始化各项参数
    stream->speed = 1.0f;
    stream->pitch = 1.0f;
    stream->volume = 1.0f;
    stream->rate = 1.0f;
    stream->oldRatePosition = 0;
    stream->newRatePosition = 0;
    stream->useChordPitch = 0;
    stream->quality = 0;
    stream->avePower = 50.0f;
    return stream;
}

/* Get the sample rate of the stream. */
// 取得流的采样率
int sonicGetSampleRate(
    sonicStream stream)
{
    return stream->sampleRate;
}

/* Set the sample rate of the stream.  This will cause samples buffered in the stream to
   be lost. */
// 设置流的采样率，可能使流中的已经缓冲的数据丢失
void sonicSetSampleRate(
    sonicStream stream,
    int sampleRate)
{
    freeStreamBuffers(stream);
    allocateStreamBuffers(stream, sampleRate, stream->numChannels);
}

/* Get the number of channels. */
// 取得流的声道的数量
int sonicGetNumChannels(
    sonicStream stream)
{
    return stream->numChannels;
}

/* Set the num channels of the stream.  This will cause samples buffered in the stream to
   be lost. */
// 设置流的声道数量，可能造成流中已缓存的额数据的丢失
void sonicSetNumChannels(
    sonicStream stream,
    int numChannels)
{
    freeStreamBuffers(stream);
    allocateStreamBuffers(stream, stream->sampleRate, numChannels);
}

/* Enlarge the output buffer if needed. */
// 根据需要扩大输出缓冲区
static int enlargeOutputBufferIfNeeded(
    sonicStream stream,
    int numSamples)
{
    if(stream->numOutputSamples + numSamples > stream->outputBufferSize) {
        stream->outputBufferSize += (stream->outputBufferSize >> 1) + numSamples;
        stream->outputBuffer = (short *)realloc(stream->outputBuffer,
            stream->outputBufferSize*sizeof(short)*stream->numChannels);
        if(stream->outputBuffer == NULL) {
            return 0;
        }
    }
    return 1;
}

/* Enlarge the input buffer if needed. */
// 如果需要的话增大输入缓冲区
static int enlargeInputBufferIfNeeded(
    sonicStream stream,
    int numSamples)
{
    // 流中已经有的采样数据的大小 + 新的采样点个数
    if(stream->numInputSamples + numSamples > stream->inputBufferSize) {
        stream->inputBufferSize += (stream->inputBufferSize >> 1) + numSamples;
        // 重新设置内存空间的大小
        stream->inputBuffer = (short *)realloc(stream->inputBuffer,
            stream->inputBufferSize*sizeof(short)*stream->numChannels);
        if(stream->inputBuffer == NULL) {
            return 0;
        }
    }
    return 1;
}

/* Add the input samples to the input buffer. */
// 向流的输入缓冲区中写入float格式的采样数据
static int addFloatSamplesToInputBuffer(
    sonicStream stream,
    float *samples,
    int numSamples)
{
    short *buffer;
    int count = numSamples*stream->numChannels;

    if(numSamples == 0) {
        return 1;
    }
    if(!enlargeInputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    buffer = stream->inputBuffer + stream->numInputSamples*stream->numChannels;
    while(count--) {
        *buffer++ = (*samples++)*32767.0f;
    }
    stream->numInputSamples += numSamples;
    return 1;
}

/* Add the input samples to the input buffer. */
// 向流的输入缓冲区中写入short类型的数据
static int addShortSamplesToInputBuffer(
    sonicStream stream,
    short *samples,
    int numSamples)
{
    if(numSamples == 0) {
        return 1;
    }
    if(!enlargeInputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    // 向输入缓冲区拷贝数据，重设numInputSamples大小
    memcpy(stream->inputBuffer + stream->numInputSamples*stream->numChannels, samples,
        numSamples*sizeof(short)*stream->numChannels);
    stream->numInputSamples += numSamples;
    return 1;
}

/* Add the input samples to the input buffer. */
// 向流的输如缓冲区中写入unsigned格式的采样数据
static int addUnsignedCharSamplesToInputBuffer(
    sonicStream stream,
    unsigned char *samples,
    int numSamples)
{
    short *buffer;
    int count = numSamples*stream->numChannels;

    if(numSamples == 0) {
        return 1;
    }
    if(!enlargeInputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    buffer = stream->inputBuffer + stream->numInputSamples*stream->numChannels;
    while(count--) {
        *buffer++ = (*samples++ - 128) << 8;
    }
    stream->numInputSamples += numSamples;
    return 1;
}

/* Remove input samples that we have already processed. */
// 移除已经处理过的输入缓冲区中的数据
static void removeInputSamples(
    sonicStream stream,
    int position)
{
    int remainingSamples = stream->numInputSamples - position;

    if(remainingSamples > 0) {
        memmove(stream->inputBuffer, stream->inputBuffer + position*stream->numChannels,
            remainingSamples*sizeof(short)*stream->numChannels);
    }
    stream->numInputSamples = remainingSamples;
}

/* Just copy from the array to the output buffer */
// 拷贝数组到输出缓冲区
static int copyToOutput(
    sonicStream stream,
    short *samples,
    int numSamples)
{
    if(!enlargeOutputBufferIfNeeded(stream, numSamples)) {
        return 0;
    }
    memcpy(stream->outputBuffer + stream->numOutputSamples*stream->numChannels,
        samples, numSamples*sizeof(short)*stream->numChannels);
    stream->numOutputSamples += numSamples;
    return 1;
}

/* Just copy from the input buffer to the output buffer.  Return 0 if we fail to
   resize the output buffer.  Otherwise, return numSamples */
// 仅仅把输入缓冲区中的数据拷贝到输出缓冲区中，返回转移了的采样点的个数
// position表示偏移量
static int copyInputToOutput(
    sonicStream stream,
    int position)
{
    int numSamples = stream->remainingInputToCopy;
    //
    if(numSamples > stream->maxRequired) {
        numSamples = stream->maxRequired;
    }
    if(!copyToOutput(stream, stream->inputBuffer + position*stream->numChannels,
            numSamples)) {
        return 0;
    }
    // 剩余需要拷贝的输入缓冲区的采样点数
    stream->remainingInputToCopy -= numSamples;
    return numSamples;
}

/* 从流中读取数据。有时可能没有数据可用，此时返回零，这不是错误情况。 */
int sonicReadFloatFromStream(
    sonicStream stream,    /* 指向 sonicStream 结构体的指针 */
    float *samples,        /* 存储读取样本的缓冲区指针 */
    int maxSamples)       /* 最大可读取的样本数 */
{
    int numSamples = stream->numOutputSamples;  /* 当前可用的输出样本数 */
    int remainingSamples = 0;   /* 剩余的样本数 */
    short *buffer;               /* 输出缓冲区指针 */
    int count;                  /* 计数器 */

    // 如果当前没有样本可用，返回 0
    if(numSamples == 0) {
        return 0;
    }
    // 如果可用样本数超过最大样本数，调整样本数
    if(numSamples > maxSamples) {
        remainingSamples = numSamples - maxSamples;  /* 计算剩余样本数 */
        numSamples = maxSamples;                      /* 读取样本数设为最大样本数 */
    }
    buffer = stream->outputBuffer;   /* 获取输出缓冲区的指针 */
    count = numSamples * stream->numChannels;  /* 计算总的样本数（考虑声道数） */
    // 将样本数据从 short 转换为 float，并存储到 samples 缓冲区
    while(count--) {
        *samples++ = (*buffer++) / 32767.0f;  /* 转换并存储样本 */
    }
    // 如果还有剩余的样本，将它们移动到缓冲区的前面
    if(remainingSamples > 0) {
        memmove(stream->outputBuffer, stream->outputBuffer + numSamples * stream->numChannels,
            remainingSamples * sizeof(short) * stream->numChannels);
    }
    // 更新流中的输出样本数
    stream->numOutputSamples = remainingSamples;
    // 返回实际读取的样本数
    return numSamples;
}

/* Read short data out of the stream.  Sometimes no data will be available, and zero
   is returned, which is not an error condition. */
// 从流中读取short类型的数据，如果没有数据返回0
int sonicReadShortFromStream(
    sonicStream stream,
    short *samples,
    int maxSamples)
{
    int numSamples = stream->numOutputSamples;
    int remainingSamples = 0;

    if(numSamples == 0) {
        return 0;
    }
    if(numSamples > maxSamples) {
        remainingSamples = numSamples - maxSamples;
        numSamples = maxSamples;
    }
    memcpy(samples, stream->outputBuffer, numSamples*sizeof(short)*stream->numChannels);
    if(remainingSamples > 0) {
        memmove(stream->outputBuffer, stream->outputBuffer + numSamples*stream->numChannels,
            remainingSamples*sizeof(short)*stream->numChannels);
    }
    stream->numOutputSamples = remainingSamples;
    return numSamples;
}

/* 从流中读取无符号字符数据。有时可能没有数据可用，此时返回零，这不是错误情况。 */
int sonicReadUnsignedCharFromStream(
    sonicStream stream,        /* 指向 sonicStream 结构体的指针 */
    unsigned char *samples,    /* 存储读取样本的缓冲区指针 */
    int maxSamples)           /* 最大可读取的样本数 */
{
    int numSamples = stream->numOutputSamples;  /* 当前可用的输出样本数 */
    int remainingSamples = 0;   /* 剩余的样本数 */
    short *buffer;               /* 输出缓冲区指针 */
    int count;                  /* 计数器 */

    // 如果当前没有样本可用，返回 0
    if(numSamples == 0) {
        return 0;
    }
    // 如果可用样本数超过最大样本数，调整样本数
    if(numSamples > maxSamples) {
        remainingSamples = numSamples - maxSamples;  /* 计算剩余样本数 */
        numSamples = maxSamples;                      /* 读取样本数设为最大样本数 */
    }
    buffer = stream->outputBuffer;   /* 获取输出缓冲区的指针 */
    count = numSamples * stream->numChannels;  /* 计算总的样本数（考虑声道数） */
    // 将样本数据从 short 转换为无符号字符，并存储到 samples 缓冲区
    while(count--) {
        *samples++ = (char)((*buffer++) >> 8) + 128;  /* 转换并存储样本 */
    }
    // 如果还有剩余的样本，将它们移动到缓冲区的前面
    if(remainingSamples > 0) {
        memmove(stream->outputBuffer, stream->outputBuffer + numSamples * stream->numChannels,
            remainingSamples * sizeof(short) * stream->numChannels);
    }
    // 更新流中的输出样本数
    stream->numOutputSamples = remainingSamples;
    // 返回实际读取的样本数
    return numSamples;
}


/* 强制 sonic 流使用当前拥有的数据生成输出。不会在输出中添加额外的延迟，但在词语中间进行刷新可能会引入失真。 */
int sonicFlushStream(
    sonicStream stream)  /* 指向 sonicStream 结构体的指针 */
{
    int maxRequired = stream->maxRequired;  /* 需要的最大缓冲区大小 */
    int remainingSamples = stream->numInputSamples;  /* 剩余的输入样本数 */
    float speed = stream->speed / stream->pitch;    /* 实际播放速度（考虑音调） */
    float rate = stream->rate * stream->pitch;      /* 实际播放速率（考虑音调） */
    int expectedOutputSamples = stream->numOutputSamples +
        (int)((remainingSamples / speed + stream->numPitchSamples) / rate + 0.5f);  /* 计算期望的输出样本数 */

    /* 添加足够的静音以清空输入和音高缓冲区 */
    if(!enlargeInputBufferIfNeeded(stream, remainingSamples + 2 * maxRequired)) {
        return 0;  /* 如果需要扩大缓冲区但失败，则返回 0 */
    }
    memset(stream->inputBuffer + remainingSamples * stream->numChannels, 0,
        2 * maxRequired * sizeof(short) * stream->numChannels);  /* 将缓冲区的末尾填充为 0 */
    stream->numInputSamples += 2 * maxRequired;  /* 更新输入样本数 */
    if(!sonicWriteShortToStream(stream, NULL, 0)) {
        return 0;  /* 如果写入短整型数据到流失败，则返回 0 */
    }
    /* 丢弃由于添加静音而生成的额外样本 */
    if(stream->numOutputSamples > expectedOutputSamples) {
        stream->numOutputSamples = expectedOutputSamples;  /* 调整输出样本数 */
    }
    /* 清空输入和音高缓冲区 */
    stream->numInputSamples = 0;  /* 输入样本数设置为 0 */
    stream->remainingInputToCopy = 0;  /* 剩余要复制的输入样本数设置为 0 */
    stream->numPitchSamples = 0;  /* 音高样本数设置为 0 */
    return 1;  /* 成功刷新流，返回 1 */
}

/* 返回输出缓冲区中的样本数 */
int sonicSamplesAvailable(
    sonicStream stream)  /* 指向 sonicStream 结构体的指针 */
{
    return stream->numOutputSamples;  /* 返回当前可用的输出样本数 */
}


/* 如果 skip 大于 1，则将跳过的样本进行平均，并将它们写入降采样缓冲区。
   如果 numChannels 大于 1，则在降采样时混合所有声道。 */
static void downSampleInput(
    sonicStream stream,  /* 指向 sonicStream 结构体的指针 */
    short *samples,      /* 输入样本缓冲区指针 */
    int skip)            /* 跳过的样本数（即每个降采样值使用的原始样本数） */
{
    int numSamples = stream->maxRequired / skip;  /* 计算需要处理的样本数量 */
    int samplesPerValue = stream->numChannels * skip;  /* 每个降采样值使用的样本总数（考虑声道数） */
    int i, j;
    int value;
    short *downSamples = stream->downSampleBuffer;  /* 降采样缓冲区指针 */

    // 对于每个降采样样本
    for(i = 0; i < numSamples; i++) {
        value = 0;
        // 计算当前降采样样本的值（对多个输入样本取平均）
        for(j = 0; j < samplesPerValue; j++) {
            value += *samples++;  /* 累加样本值 */
        }
        value /= samplesPerValue;  /* 计算平均值 */
        *downSamples++ = value;    /* 将平均值写入降采样缓冲区 */
    }
}

/* 在给定的范围内，找到最佳的频率匹配，以及给定的样本跳过倍数。
   目前仅考虑第一个声道的音高。 */
static int findPitchPeriodInRange(
    short *samples,      /* 输入样本数据的指针 */
    int minPeriod,      /* 最小周期 */
    int maxPeriod,      /* 最大周期 */
    int *retMinDiff,    /* 输出参数，最小差异 */
    int *retMaxDiff)    /* 输出参数，最大差异 */
{
    int period, bestPeriod = 0, worstPeriod = 255;  /* 初始化最佳周期和最差周期 */
    short *s, *p, sVal, pVal;  /* 指向样本数据和参考数据的指针 */
    unsigned long diff, minDiff = 1, maxDiff = 0;  /* 差异值，初始化最小差异为1，最大差异为0 */
    int i;

    // 遍历周期范围
    for(period = minPeriod; period <= maxPeriod; period++) {
        diff = 0;  /* 当前周期的差异初始化为0 */
        s = samples;  /* 指向样本数据的指针 */
        p = samples + period;  /* 指向参考数据的指针（周期后的位置） */
        // 计算当前周期内的差异
        for(i = 0; i < period; i++) {
            sVal = *s++;  /* 获取样本值 */
            pVal = *p++;  /* 获取参考值 */
            diff += sVal >= pVal ? (unsigned short)(sVal - pVal) : (unsigned short)(pVal - sVal);  /* 计算差异并累加 */
        }
        /* 注意：我们添加到 diff 中的样本数量不会超过 256，因为我们跳过了样本。
           因此，diff 是一个 24 位的数字，我们可以安全地将其乘以 numSamples 而不会溢出。 */
        /* 根据差异和周期判断最佳周期 */
        if (bestPeriod == 0 || diff * bestPeriod < minDiff * period) {
            minDiff = diff;  /* 更新最小差异 */
            bestPeriod = period;  /* 更新最佳周期 */
        }
        /* 根据差异和周期判断最差周期 */
        if(diff * worstPeriod > maxDiff * period) {
            maxDiff = diff;  /* 更新最大差异 */
            worstPeriod = period;  /* 更新最差周期 */
        }
    }
    *retMinDiff = minDiff / bestPeriod;  /* 设置返回的最小差异 */
    *retMaxDiff = maxDiff / worstPeriod;  /* 设置返回的最大差异 */
    return bestPeriod;  /* 返回最佳周期 */
}

/* 在有声单词的突然结束时，我们可能会发现前一个音高周期的估计更为准确。
   尝试检测这种情况。 */
static int prevPeriodBetter(
    sonicStream stream,    /* 声音流的指针 */
    int period,            /* 当前周期 */
    int minDiff,          /* 当前周期的最小差异 */
    int maxDiff,          /* 当前周期的最大差异 */
    int preferNewPeriod)  /* 是否偏好新的周期 */
{
    if(minDiff == 0 || stream->prevPeriod == 0) {
        /* 如果最小差异为0，或者之前的周期为0，说明没有足够的数据进行比较 */
        return 0;
    }
    if(preferNewPeriod) {
        /* 如果偏好新的周期 */
        if(maxDiff > minDiff*3) {
            /* 当前周期的最大差异远大于最小差异的三倍，认为当前周期匹配较好 */
            return 0;
        }
        if(minDiff*2 <= stream->prevMinDiff*3) {
            /* 当前周期的最小差异的两倍不大于之前周期最小差异的三倍，
               认为当前周期和之前周期的差异不大 */
            return 0;
        }
    } else {
        /* 如果不偏好新的周期 */
        if(minDiff <= stream->prevMinDiff) {
            /* 当前周期的最小差异不大于之前周期的最小差异，认为之前的周期更好 */
            return 0;
        }
    }
    /* 否则，认为当前周期更好 */
    return 1;
}

/* 寻找音高周期。这是一个关键步骤，我们可能需要尝试多种方法以获得良好的结果。
   这个版本使用了平均幅度差函数（AMDF）。为了提高速度，我们通过整数因子下采样到11KHz范围内，
   然后在没有下采样的情况下使用更窄的频率范围再次进行处理 */
static int findPitchPeriod(
    sonicStream stream,   /* 声音流的指针 */
    short *samples,       /* 输入样本数据 */
    int preferNewPeriod) /* 是否偏好新的周期 */
{
    int minPeriod = stream->minPeriod;       /* 最小周期 */
    int maxPeriod = stream->maxPeriod;       /* 最大周期 */
    int sampleRate = stream->sampleRate;     /* 采样率 */
    int minDiff, maxDiff, retPeriod;         /* 最小差异、最大差异、返回的周期 */
    int skip = 1;                            /* 下采样因子 */
    int period;

    if(sampleRate > SONIC_AMDF_FREQ && stream->quality == 0) {
        /* 如果采样率高于AMDF频率并且质量设置为0，设置下采样因子 */
        skip = sampleRate/SONIC_AMDF_FREQ;
    }
    if(stream->numChannels == 1 && skip == 1) {
        /* 如果是单声道且下采样因子为1，直接在样本范围内寻找音高周期 */
        period = findPitchPeriodInRange(samples, minPeriod, maxPeriod, &minDiff, &maxDiff);
    } else {
        /* 否则，首先进行下采样以减少计算量 */
        downSampleInput(stream, samples, skip);
        period = findPitchPeriodInRange(stream->downSampleBuffer, minPeriod/skip,
            maxPeriod/skip, &minDiff, &maxDiff);
        if(skip != 1) {
            /* 如果进行了下采样，则恢复到原始采样因子，并在更窄的周期范围内进行精确计算 */
            period *= skip;
            minPeriod = period - (skip << 2);
            maxPeriod = period + (skip << 2);
            if(minPeriod < stream->minPeriod) {
                minPeriod = stream->minPeriod;
            }
            if(maxPeriod > stream->maxPeriod) {
                maxPeriod = stream->maxPeriod;
            }
            if(stream->numChannels == 1) {
                period = findPitchPeriodInRange(samples, minPeriod, maxPeriod,
                    &minDiff, &maxDiff);
            } else {
                downSampleInput(stream, samples, 1);
                period = findPitchPeriodInRange(stream->downSampleBuffer, minPeriod,
                    maxPeriod, &minDiff, &maxDiff);
            }
        }
    }
    if(prevPeriodBetter(stream, period, minDiff, maxDiff, preferNewPeriod)) {
        /* 如果之前的周期更好，则返回之前的周期 */
        retPeriod = stream->prevPeriod;
    } else {
        /* 否则，返回当前计算的周期 */
        retPeriod = period;
    }
    stream->prevMinDiff = minDiff;   /* 更新之前的最小差异 */
    stream->prevPeriod = period;     /* 更新之前的周期 */
    return retPeriod;                /* 返回最终的音高周期 */
}

/* 重叠两个声音片段，将其中一个的音量逐渐降低，同时将另一个的音量从零逐渐增大，并将结果存储到输出缓冲区 */
static void overlapAdd(
    int numSamples,      /* 样本数量 */
    int numChannels,     /* 声道数 */
    short *out,          /* 输出缓冲区 */
    short *rampDown,     /* 音量逐渐降低的片段 */
    short *rampUp)       /* 音量逐渐增大的片段 */
{
    short *o, *u, *d;    /* 指向输出、逐渐增大的片段和逐渐降低的片段的指针 */
    int i, t;

    for(i = 0; i < numChannels; i++) {
        /* 对每个声道处理 */
        o = out + i;
        u = rampUp + i;
        d = rampDown + i;
        for(t = 0; t < numSamples; t++) {
#ifdef SONIC_USE_SIN
            /* 如果定义了SONIC_USE_SIN，使用正弦函数进行音量渐变 */
            float ratio = sin(t*M_PI/(2*numSamples));
            *o = *d*(1.0f - ratio) + *u*ratio;
#else
            /* 否则，使用线性插值进行音量渐变 */
            *o = (*d*(numSamples - t) + *u*t)/numSamples;
#endif
            o += numChannels;  /* 移动到下一个样本 */
            d += numChannels;  /* 移动到下一个样本 */
            u += numChannels;  /* 移动到下一个样本 */
        }
    }
}

/* 重叠两个声音片段，将其中一个的音量逐渐降低，同时将另一个的音量从零逐渐增大，并将结果存储到输出缓冲区。支持分离时间的设置。 */
static void overlapAddWithSeparation(
    int numSamples,      /* 样本数量 */
    int numChannels,     /* 声道数 */
    int separation,      /* 音量逐渐增大和逐渐降低的片段之间的分离时间 */
    short *out,          /* 输出缓冲区 */
    short *rampDown,     /* 音量逐渐降低的片段 */
    short *rampUp)       /* 音量逐渐增大的片段 */
{
    short *o, *u, *d;    /* 指向输出、逐渐增大的片段和逐渐降低的片段的指针 */
    int i, t;

    for(i = 0; i < numChannels; i++) {
        /* 对每个声道处理 */
        o = out + i;
        u = rampUp + i;
        d = rampDown + i;
        for(t = 0; t < numSamples + separation; t++) {
            if(t < separation) {
                /* 在分离时间内，仅对逐渐降低的片段进行处理 */
                *o = *d*(numSamples - t)/numSamples;
                d += numChannels;
            } else if(t < numSamples) {
                /* 在分离时间后，重叠两个片段，进行音量渐变处理 */
                *o = (*d*(numSamples - t) + *u*(t - separation))/numSamples;
                d += numChannels;
                u += numChannels;
            } else {
                /* 在音频片段结束后，仅对逐渐增大的片段进行处理 */
                *o = *u*(t - separation)/numSamples;
                u += numChannels;
            }
            o += numChannels;  /* 移动到下一个样本 */
        }
    }
}

/* 将新的样本从输出缓冲区移动到音高缓冲区 */
static int moveNewSamplesToPitchBuffer(
    sonicStream stream,
    int originalNumOutputSamples)
{
    int numSamples = stream->numOutputSamples - originalNumOutputSamples;
    int numChannels = stream->numChannels;

    /* 如果音高缓冲区不够大，则扩大其大小 */
    if(stream->numPitchSamples + numSamples > stream->pitchBufferSize) {
        stream->pitchBufferSize += (stream->pitchBufferSize >> 1) + numSamples;
        stream->pitchBuffer = (short *)realloc(stream->pitchBuffer,
            stream->pitchBufferSize*sizeof(short)*numChannels);
        if(stream->pitchBuffer == NULL) {
            return 0;  /* 扩大缓冲区失败 */
        }
    }
    /* 复制样本到音高缓冲区 */
    memcpy(stream->pitchBuffer + stream->numPitchSamples*numChannels,
        stream->outputBuffer + originalNumOutputSamples*numChannels,
        numSamples*sizeof(short)*numChannels);
    stream->numOutputSamples = originalNumOutputSamples;
    stream->numPitchSamples += numSamples;
    return 1;  /* 成功 */
}

/* 从音高缓冲区中移除处理过的样本 */
static void removePitchSamples(
    sonicStream stream,
    int numSamples)
{
    int numChannels = stream->numChannels;
    short *source = stream->pitchBuffer + numSamples*numChannels;

    if(numSamples == 0) {
        return;  /* 无需移除样本 */
    }
    if(numSamples != stream->numPitchSamples) {
        /* 移动剩余样本 */
        memmove(stream->pitchBuffer, source, (stream->numPitchSamples -
            numSamples)*sizeof(short)*numChannels);
    }
    stream->numPitchSamples -= numSamples;
}

/* 调整音高。此操作可能引入延迟，减少延迟的方法是查看过去的样本而不是未来的样本。 */
static int adjustPitch(
    sonicStream stream,
    int originalNumOutputSamples)
{
    float pitch = stream->pitch;
    int numChannels = stream->numChannels;
    int period, newPeriod, separation;
    int position = 0;
    short *out, *rampDown, *rampUp;

    if(stream->numOutputSamples == originalNumOutputSamples) {
        return 1;  /* 无需调整音高 */
    }
    if(!moveNewSamplesToPitchBuffer(stream, originalNumOutputSamples)) {
        return 0;  /* 移动样本失败 */
    }
    /* 根据音高调整输出样本 */
    while(stream->numPitchSamples - position >= stream->maxRequired) {
        period = findPitchPeriod(stream, stream->pitchBuffer + position*numChannels, 0);
        newPeriod = period/pitch;
        if(!enlargeOutputBufferIfNeeded(stream, newPeriod)) {
            return 0;  /* 扩大输出缓冲区失败 */
        }
        out = stream->outputBuffer + stream->numOutputSamples*numChannels;
        if(pitch >= 1.0f) {
            rampDown = stream->pitchBuffer + position*numChannels;
            rampUp = stream->pitchBuffer + (position + period - newPeriod)*numChannels;
            overlapAdd(newPeriod, numChannels, out, rampDown, rampUp);
        } else {
            rampDown = stream->pitchBuffer + position*numChannels;
            rampUp = stream->pitchBuffer + position*numChannels;
            separation = newPeriod - period;
            overlapAddWithSeparation(period, numChannels, separation, out, rampDown, rampUp);
        }
        stream->numOutputSamples += newPeriod;
        position += period;
    }
    removePitchSamples(stream, position);
    return 1;  /* 成功 */
}

/* 从 sinc 表中近似计算 sinc 函数乘以 Hann 窗口的系数 */
static int findSincCoefficient(int i, int ratio, int width) {
    int lobePoints = (SINC_TABLE_SIZE-1)/SINC_FILTER_POINTS;
    int left = i*lobePoints + (ratio*lobePoints)/width;
    int right = left + 1;
    int position = i*lobePoints*width + ratio*lobePoints - left*width;
    int leftVal = sincTable[left];
    int rightVal = sincTable[right];

    return ((leftVal*(width - position) + rightVal*position) << 1)/width;
}

/* 返回值的符号，如果值大于等于 0 返回 1，否则返回 -1 */
static int getSign(int value) {
    return value >= 0? 1 : -1;
}

/* 插值新的输出样本 */
static short interpolate(
    sonicStream stream,
    short *in,
    int oldSampleRate,
    int newSampleRate)
{
    /* 计算 N 点 sinc FIR 滤波器。为了防止溢出，使用剪裁而不是溢出 */
    int i;
    int total = 0;
    int position = stream->newRatePosition * oldSampleRate;
    int leftPosition = stream->oldRatePosition * newSampleRate;
    int rightPosition = (stream->oldRatePosition + 1) * newSampleRate;
    int ratio = rightPosition - position - 1;
    int width = rightPosition - leftPosition;
    int weight, value;
    int oldSign;
    int overflowCount = 0;

    for (i = 0; i < SINC_FILTER_POINTS; i++) {
        weight = findSincCoefficient(i, ratio, width);
        /* printf("%u %f\n", i, weight); */
        value = in[i * stream->numChannels] * weight;
        oldSign = getSign(total);
        total += value;
        if (oldSign != getSign(total) && getSign(value) == oldSign) {
            /* 发生了溢出。这可能发生在 sinc 滤波器中。 */
            overflowCount += oldSign;
        }
    }
    /* 如果发生了溢出，剪裁值而不是包装值更好 */
    if (overflowCount > 0) {
        return SHRT_MAX;
    } else if (overflowCount < 0) {
        return SHRT_MIN;
    }
    return total >> 16;
}

/* 调整采样率。使用 sinc FIR 滤波器和 Hann 窗口进行插值。 */
static int adjustRate(
    sonicStream stream,
    float rate,
    int originalNumOutputSamples)
{
    int newSampleRate = stream->sampleRate / rate;
    int oldSampleRate = stream->sampleRate;
    int numChannels = stream->numChannels;
    int position = 0;
    short *in, *out;
    int i;
    int N = SINC_FILTER_POINTS;

    /* 设置这些值以帮助进行整数运算 */
    while (newSampleRate > (1 << 14) || oldSampleRate > (1 << 14)) {
        newSampleRate >>= 1;
        oldSampleRate >>= 1;
    }
    if (stream->numOutputSamples == originalNumOutputSamples) {
        return 1;  /* 不需要调整速率 */
    }
    if (!moveNewSamplesToPitchBuffer(stream, originalNumOutputSamples)) {
        return 0;  /* 移动样本失败 */
    }
    /* 保留至少 N 个音高样本在缓冲区中 */
    for (position = 0; position < stream->numPitchSamples - N; position++) {
        while ((stream->oldRatePosition + 1) * newSampleRate >
                stream->newRatePosition * oldSampleRate) {
            if (!enlargeOutputBufferIfNeeded(stream, 1)) {
                return 0;  /* 扩大输出缓冲区失败 */
            }
            out = stream->outputBuffer + stream->numOutputSamples * numChannels;
            in = stream->pitchBuffer + position * numChannels;
            for (i = 0; i < numChannels; i++) {
                *out++ = interpolate(stream, in, oldSampleRate, newSampleRate);
                in++;
            }
            stream->newRatePosition++;
            stream->numOutputSamples++;
        }
        stream->oldRatePosition++;
        if (stream->oldRatePosition == oldSampleRate) {
            stream->oldRatePosition = 0;
            if (stream->newRatePosition != newSampleRate) {
                fprintf(stderr,
                    "Assertion failed: stream->newRatePosition != newSampleRate\n");
                exit(1);
            }
            stream->newRatePosition = 0;
        }
    }
    removePitchSamples(stream, position);
    return 1;  /* 成功 */
}

/* 跳过一个音高周期，并将周期/速度样本复制到输出 */
static int skipPitchPeriod(
    sonicStream stream,
    short *samples,
    float speed,
    int period)
{
    long newSamples;
    int numChannels = stream->numChannels;

    if (speed >= 2.0f) {
        newSamples = period / (speed - 1.0f);
    } else {
        newSamples = period;
        stream->remainingInputToCopy = period * (2.0f - speed) / (speed - 1.0f);
    }
    if (!enlargeOutputBufferIfNeeded(stream, newSamples)) {
        return 0;  /* 扩大输出缓冲区失败 */
    }
    overlapAdd(newSamples, numChannels, stream->outputBuffer +
        stream->numOutputSamples * numChannels, samples, samples + period * numChannels);
    stream->numOutputSamples += newSamples;
    return newSamples;
}

/* 插入一个音高周期，并确定要直接复制的输入量 */
static int insertPitchPeriod(
    sonicStream stream,
    short *samples,
    float speed,
    int period)
{
    long newSamples;
    short *out;
    int numChannels = stream->numChannels;

    if (speed < 0.5f) {
        newSamples = period * speed / (1.0f - speed);
    } else {
        newSamples = period;
        stream->remainingInputToCopy = period * (2.0f * speed - 1.0f) / (1.0f - speed);
    }
    if (!enlargeOutputBufferIfNeeded(stream, period + newSamples)) {
        return 0;  /* 扩大输出缓冲区失败 */
    }
    out = stream->outputBuffer + stream->numOutputSamples * numChannels;
    memcpy(out, samples, period * sizeof(short) * numChannels);
    out = stream->outputBuffer + (stream->numOutputSamples + period) * numChannels;
    overlapAdd(newSamples, numChannels, out, samples + period * numChannels, samples);
    stream->numOutputSamples += period + newSamples;
    return newSamples;
}

/* 尽可能多地将输入缓冲区中的基音周期进行重采样。失败时返回0，成功时返回1 */
static int changeSpeed(
    sonicStream stream,
    float speed)
{
    short *samples;
    int numSamples = stream->numInputSamples;
    int position = 0, period, newSamples;
    int maxRequired = stream->maxRequired;

    /* printf("Changing speed to %f\n", speed); */
    if (stream->numInputSamples < maxRequired) {
        return 1;  /* 不需要重采样 */
    }
    do {
        /* 如果流中有剩余的输入样本需要复制 */
        if (stream->remainingInputToCopy > 0) {
            newSamples = copyInputToOutput(stream, position);
            position += newSamples;
        } else {
            samples = stream->inputBuffer + position * stream->numChannels;
            period = findPitchPeriod(stream, samples, 1);
            if (speed > 1.0) {
                newSamples = skipPitchPeriod(stream, samples, speed, period);
                position += period + newSamples;
            } else {
                newSamples = insertPitchPeriod(stream, samples, speed, period);
                position += newSamples;
            }
        }
        if (newSamples == 0) {
            return 0; /* 扩大输入或输出缓冲区失败 */
        }
    } while (position + maxRequired <= numSamples);
    removeInputSamples(stream, position);
    return 1;  /* 成功 */
}

/* 尽可能多地将输入缓冲区中的基音周期进行重采样，并调整输出音量。如果失败返回0，成功返回1 */
static int processStreamInput(
    sonicStream stream)
{
    int originalNumOutputSamples = stream->numOutputSamples;
    float speed = stream->speed / stream->pitch;
    float rate = stream->rate;

    if (!stream->useChordPitch) {
        rate *= stream->pitch;
    }

    /* 调整速度 */
    if (speed > 1.00001 || speed < 0.99999) {
        if (!changeSpeed(stream, speed)) {
            return 0;  /* 调整速度失败 */
        }
    } else {
        if (!copyToOutput(stream, stream->inputBuffer, stream->numInputSamples)) {
            return 0;  /* 复制到输出缓冲区失败 */
        }
        stream->numInputSamples = 0;
    }

    /* 根据是否使用和声音高进行音高调整或速率调整 */
    if (stream->useChordPitch) {
        if (stream->pitch != 1.0f) {
            if (!adjustPitch(stream, originalNumOutputSamples)) {
                return 0;  /* 调整音高失败 */
            }
        }
    } else if (rate != 1.0f) {
        if (!adjustRate(stream, rate, originalNumOutputSamples)) {
            return 0;  /* 调整速率失败 */
        }
    }

    /* 调整输出音量 */
    if (stream->volume != 1.0f) {
        scaleSamples(stream->outputBuffer + originalNumOutputSamples * stream->numChannels,
            (stream->numOutputSamples - originalNumOutputSamples) * stream->numChannels,
            stream->volume);
    }
    return 1;  /* 成功 */
}

/* 将浮点数据写入输入缓冲区并处理它 */
int sonicWriteFloatToStream(
    sonicStream stream,
    float *samples,
    int numSamples)
{
    if (!addFloatSamplesToInputBuffer(stream, samples, numSamples)) {
        return 0;  /* 添加浮点样本到输入缓冲区失败 */
    }
    return processStreamInput(stream);  /* 处理输入流 */
}

/* 包装函数，将short类型的数据写入流并进行处理 */
int sonicWriteShortToStream(
    sonicStream stream,
    short *samples,
    int numSamples)
{
    if (!addShortSamplesToInputBuffer(stream, samples, numSamples)) {
        return 0;  /* 添加short样本到输入缓冲区失败 */
    }
    return processStreamInput(stream);  /* 处理输入流 */
}

/* 包装函数，将unsigned char类型的数据写入流并进行处理 */
int sonicWriteUnsignedCharToStream(
    sonicStream stream,
    unsigned char *samples,
    int numSamples)
{
    if (!addUnsignedCharSamplesToInputBuffer(stream, samples, numSamples)) {
        return 0;  /* 添加unsigned char样本到输入缓冲区失败 */
    }
    return processStreamInput(stream);  /* 处理输入流 */
}

/* 非流式接口，用于仅改变声音样本的速度 */
int sonicChangeFloatSpeed(
    float *samples,
    int numSamples,
    float speed,
    float pitch,
    float rate,
    float volume,
    int useChordPitch,
    int sampleRate,
    int numChannels)
{
    sonicStream stream = sonicCreateStream(sampleRate, numChannels);

    sonicSetSpeed(stream, speed);             /* 设置速度 */
    sonicSetPitch(stream, pitch);             /* 设置音调 */
    sonicSetRate(stream, rate);               /* 设置速率 */
    sonicSetVolume(stream, volume);           /* 设置音量 */
    sonicSetChordPitch(stream, useChordPitch); /* 设置和声音高 */
    sonicWriteFloatToStream(stream, samples, numSamples); /* 写入浮点数据并处理 */
    sonicFlushStream(stream);                 /* 刷新流 */
    numSamples = sonicSamplesAvailable(stream); /* 获取可用样本数 */
    sonicReadFloatFromStream(stream, samples, numSamples); /* 从流中读取样本 */
    sonicDestroyStream(stream);               /* 销毁流 */
    return numSamples;                        /* 返回处理后的样本数量 */
}

/* 非流式接口，用于仅改变声音样本的速度 */
int sonicChangeShortSpeed(
    short *samples,
    int numSamples,
    float speed,
    float pitch,
    float rate,
    float volume,
    int useChordPitch,
    int sampleRate,
    int numChannels)
{
    sonicStream stream = sonicCreateStream(sampleRate, numChannels);

    sonicSetSpeed(stream, speed);             /* 设置速度 */
    sonicSetPitch(stream, pitch);             /* 设置音调 */
    sonicSetRate(stream, rate);               /* 设置速率 */
    sonicSetVolume(stream, volume);           /* 设置音量 */
    sonicSetChordPitch(stream, useChordPitch); /* 设置和声音高 */
    sonicWriteShortToStream(stream, samples, numSamples); /* 写入short数据并处理 */
    sonicFlushStream(stream);                 /* 刷新流 */
    numSamples = sonicSamplesAvailable(stream); /* 获取可用样本数 */
    sonicReadShortFromStream(stream, samples, numSamples); /* 从流中读取样本 */
    sonicDestroyStream(stream);               /* 销毁流 */
    return numSamples;                        /* 返回处理后的样本数量 */
}
