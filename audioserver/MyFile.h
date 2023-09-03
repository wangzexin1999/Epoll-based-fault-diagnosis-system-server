#include <dirent.h>
#include <sys/time.h>
#include <cstring>
#include <iostream>
#include <sys/stat.h>


#define getCodeInfo() getCodeLine(__FILE__, __LINE__)
#define logPrint(level, fmt, ...) LOGPRINT(level, fmt, ##__VA_ARGS__)
#define LOGPRINT(level, fmt, ...) { \
    if (level >= 0 && level < 7) { \
        char tempBuf[100] = { 0 }; \
        sprintf(tempBuf, fmt, ##__VA_ARGS__); \
        printf("%s%s\e[0m", LOG_COLOR[level], tempBuf); \
    }\
}
extern bool g_monthdir;

enum MY_ENUM_LOG_LEVEL
{
    MY_LOG_LEVEL_TRACE = 0,
    MY_LOG_LEVEL_DEBUG,
    MY_LOG_LEVEL_INFO,
    MY_LOG_LEVEL_WARN,
    MY_LOG_LEVEL_ERROR,
    MY_LOG_LEVEL_ALARM,
    MY_LOG_LEVEL_FATAL,
};
const static char LOG_COLOR[MY_LOG_LEVEL_FATAL + 1][50] = {
        "\e[0m",
        "\e[0m",
        "\e[34m\e[1m",//hight blue
        "\e[33m", //yellow
        "\e[31m", //red
        "\e[32m", //green
        "\e[35m" };

typedef struct _myFileName {
    const char* file_path;
    const char* prefix;
    bool use_cur_time;
    const char* suffix;
    const char* mode;
    _myFileName() : file_path(NULL), prefix(NULL), use_cur_time(true), suffix(NULL), mode("a+") {};
    _myFileName(const char* _file_path, const char* _prefix, bool _use_cur_time, const char* _suffix, const char* _mode) {
        file_path = _file_path;
        prefix = _prefix;
        use_cur_time = _use_cur_time;
        suffix = _suffix;
        mode = _mode;
    }
}MyFileName;
// get such format: sample.cpp:line
char* getCodeLine(const char* file, int line);
// get such format: year-month-day hh:mm:ss
char* getCurTime();
// test access for my dir
bool accessMyDir(const char* dir_name);
bool getFileName(MyFileName* file_name, char* char_file_name);
// open my file by myfilename
bool openMyFile(FILE*& my_file, MyFileName* file_name);
// get file size by file name(kbytes)
int getFileSize(MyFileName* file_name);
int getFileSize(const char* char_file_name);
// get current dir's numbers of file
int getFileNumInDir(const char* dir_name);
