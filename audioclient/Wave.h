//
// Created by luliuliu on 2021/7/8.
//

#ifndef FILEMERGE_WAVE_H
#define FILEMERGE_WAVE_H


#include <cstdint>

class MyWave {
public:
    MyWave(int Size);
    ~MyWave();
    int readWavHead(unsigned char* buf);
    bool IsMyPcm();
    bool IsWave();
    char   riff[4];//"RIFF"标志
    int32_t fileLen;//文件长度 （实际长度－8）
    char   wave[4];//char "WAVE"标志
    char   fmt[4];//char "fmt"标志
    char   unuse[4];//过渡字节（不定）
    int16_t format;//格式类别（10H为PCM形式的声音数据)
    int16_t chnls;//通道数，单声道为1，双声道为2
    int32_t sampleRate;//采样率（每秒样本数），表示每个通道的播放速度，
    int32_t dataRate;//波形音频数据传送速率，其值为通道数×每秒数据位数×每样本的数据位数／8
    int16_t dataBlock;//数据块的调整数（按字节算的），其值为通道数×每样本的数据位值／8
    int16_t bits; // 每样本的数据位数
    char   data[4]; //数据标记符＂data＂
    int32_t voiceLen;//语音数据的长度
};


#endif //FILEMERGE_WAVE_H
