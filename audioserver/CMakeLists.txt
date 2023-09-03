cmake_minimum_required(VERSION 3.15)
project(audioServer)

set(CMAKE_CXX_STANDARD 11)

add_executable(audioServer
        main.cpp
        AudioDataUnit.h
        global.h
        global.cpp
        CCfgFileParser.cpp
        CCfgFileParser.h
        Analyse.h
        Analyse.cpp
        Classify.h
        Classify.cpp
        Lang.h
        Lang.cpp
        AudioCluster.cpp
        AudioCluster.h
        EpollServer.cpp
        EpollServer.h
        AudioServer.cpp
        AudioServer.h
        MyFile.cpp
        MyFile.h

		FileMergeModule.cpp 
		FileMergeModule.h
        Wave.h
        Wave.cpp)