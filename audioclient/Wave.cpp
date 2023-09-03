//
// Created by luliuliu on 2021/7/8.
//

#include <cstring>
#include "Wave.h"

#define  SAMPLE_FREQUENCY 8000

MyWave::MyWave(int Size)
{
    riff[0] = 'R'; //"RIFF";
    riff[1] = 'I';
    riff[2] = 'F';
    riff[3] = 'F';

    wave[0] = 'W'; //"WAVE";
    wave[1] = 'A';
    wave[2] = 'V';
    wave[3] = 'E';

    fmt[0] = 'f';//fmt";
    fmt[1] = 'm';
    fmt[2] = 't';
    fmt[3] = ' ';
    //unuse[4]
    unuse[0] = 16;
    unuse[1] = 0;
    unuse[2] = 0;
    unuse[3] = 0;

    format = 1;//int16_t PCM

    chnls = 1; //int16_t

    sampleRate = SAMPLE_FREQUENCY;  //int32_t
    dataRate = SAMPLE_FREQUENCY*2;  //int32_t
    dataBlock = 2;	//  int16_t

    bits = 16;      //int16_t
    data[0] = 'd';  //"data";
    data[1] = 'a';
    data[2] = 't';
    data[3] = 'a';

    voiceLen = Size;   //  int32_t
    fileLen = Size + sizeof(MyWave) - 8;   //文件长度int32_t
}

MyWave::~MyWave()
{
}

//-------------------------------------------------------------------------------
bool MyWave::IsMyPcm()
{
    return (format == 1 && sampleRate == SAMPLE_FREQUENCY && dataBlock == 2 && bits == 16) ? true : false;
}

bool MyWave::IsWave()
{
    bool bIsWave = true;

    if( wave[0]!='W' || wave[1] !='A' || wave[2]!='V'|| wave[3]!='E' ||
        riff[1] !='I' || riff[2] !='F' || riff[3] !='F')
        bIsWave = false;

    return bIsWave;

}
int MyWave::readWavHead(unsigned char *buf) {
    if (!buf) return -1;
    uint64_t pos = 0, len = 1024 * 1024;
    while (pos < len - 3) {
        if (buf[pos] == 'R' && buf[pos + 1] == 'I' && buf[pos + 2] == 'F' && buf[pos + 3] == 'F') break;
        pos++;
    }
    if (pos == len - 3) return -1;
    fileLen = *(int32_t *)(buf + pos + 4);
    format = *(int16_t *)(buf + pos + 20);
    chnls = *(int16_t *)(buf + pos + 22);
    sampleRate = *(int32_t *)(buf + pos + 24);
    dataRate = *(int32_t *)(buf + pos + 28);
    dataBlock = *(int16_t *)(buf + pos + 32);
    bits = *(int16_t *)(buf + pos + 34);
    pos = pos + 36;
    while (pos < len - 3) {
        if ((buf[pos] == 'd' && buf[pos + 1] == 'a' && buf[pos + 2] == 't' && buf[pos + 3] == 'a')) break;
        pos++;
    }
    if (pos == len - 3) return -1;
    voiceLen = *(int32_t *)(buf + pos + 4);
    return pos + 8;
}