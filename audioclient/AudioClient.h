#ifndef AUDIOCLIENT_AUDIOCLIENTLIBRARY_H
#define AUDIOCLIENT_AUDIOCLIENTLIBRARY_H
#include "ClientManage.h"

/*
	 * 初始化为audioAnalyseClient模块，设置模块向音频解码系统传递结果的回调函数
	 * @param funcReportResult 主程序提供的回调函数，动态链接库调用funcReportResult将检测结果发送给主程序
	 * @return 0-初始化成功, <0-表示出现错误
	 * */
extern "C" int32_t init_audio_client(hitsplab_audioclient::RcvAudioResult_t  funcReportResult);
/*
	 * 接收需要处理的音频数据,该函数应该在接收数据并拷贝保存后立即返回，不可以修改参数结构体的数值，如果是查询热点节目，先在本地进行查询
	 * @param data_type 查询类型
	 * @param data_ptr 数据指针
	 * @param ibytes buf指针指向内存的有效数据长度
	 * @return 0——接收成功；-1——接收失败;1热点节目本地查询成功
	 * */
extern "C" int64_t send_pcm(uint8_t data_type, void *data_ptr, uint32_t ibytes, void* my_ptr, uint32_t mybytes);

/*
 * 结束处理，释放资源,系统退出前调用该函数，以便释放分析模块占用的资源
 * @return 处理结果
 * */
extern "C" int32_t close_audio_client();


#endif //AUDIOCLIENT_AUDIOCLIENTLIBRARY_H