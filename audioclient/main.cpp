#include "TcpClient.h"
#include "AudioClient.h"
#include "AudioDataUnit.h"
#include "mylog.h"
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include <iostream>



using hitsplab_audioclient::TcpClient;
using hitsplab_audioclient::OpenLogFile_t;
using namespace hitsplab_audioclient;

int receivewang(uint32_t iModuleID, void *pResult){
    return 1;
}


int main(char args[]){
    
    int32_t result = init_audio_client(&receivewang);
    if (result != 0) {
        std::cerr << "Failed to initialize audio client" << std::endl;
        return 1;
    }

    MyHeader myHeader;
    myHeader.m_id = 10;
    myHeader.m_channelNum = 18;
    myHeader.m_sampleRate = 1024;

    // Assuming you have audio data in a buffer called audioData
    // uint8_t* audioData = ;
    // uint32_t audioSize = 0;
         const char data[]="hello world\n";

    // Sending PCM audio data
    result = send_pcm(DT_LOG, (void*)data, sizeof (data),nullptr,0);
    if (result != 0) {
        std::cerr << "Failed to send PCM audio data to data queue" << std::endl;
        return 1;
    }
   

    // Wait for audio results to be processed
    // You can perform other tasks here or sleep for a while

    // Closing the audio client
    result = close_audio_client();
    if (result != 0) {
        std::cerr << "Failed to close audio client" << std::endl;
        return 1;
    }

    return 0;

}