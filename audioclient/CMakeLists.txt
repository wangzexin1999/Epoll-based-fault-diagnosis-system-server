cmake_minimum_required(VERSION 3.5.1)
project(audioClient)

set(CMAKE_CXX_STANDARD 11)

add_library(audioClient SHARED
        # include/log4z/log4z.h
        # include/log4z/log4z.cpp
        # include/AudioInterface/audio_cluster.h
        # include/AudioInterface/audio_cluster.cpp
        include/CCfgFileParser.h
        include./CCfgFileParser.cpp
        include/NetWorkUtility/AudioDataUnit.h
        include/NetWorkUtility/ClientManagement.cpp
        include/NetWorkUtility/ClientManagement.h
        include/NetWorkUtility/TcpClient.cpp
        include/NetWorkUtility/TcpClient.h
        include/NetWorkUtility/MyFile.h
        include/NetWorkUtility/MyFile.cpp
        AudioClientLibrary.h
        AudioClientLibrary.cpp)

# cmake_minimum_required(VERSION 3.5.1)  # CMake 版本要求
# project(testClient)  # 项目名称

# # 设置 C++ 标准
# set(CMAKE_CXX_STANDARD 11)

# include_directories(/home/liuxiu/smallsample/wangzexin/FengDian/epollproject/libAudioClient.so)
# link_directories("/home/liuxiu/smallsample/wangzexin/FengDian/epollproject")
# include_directories(/home/liuxiu/smallsample/wangzexin/FengDian/epollproject/libmmdecoder.so)
# link_directories("/home/liuxiu/smallsample/wangzexin/FengDian/epollproject")

# # 添加源代码
# add_executable(testClient main.cpp 
#                         TcpClient.h
#                         TcpClient.cpp
#                         mylog.h
#                         # errno.h
#                         # string.h
#                         )
# target_link_libraries(testClient libmmdecoder.so)
# target_link_libraries(testClient libAudioClient.so)
