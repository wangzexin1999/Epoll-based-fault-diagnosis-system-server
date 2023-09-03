#include <zconf.h>
#include "MyFile.h"
bool g_monthdir = false;

/*
 * get such format: sample.cpp:line
 * @param file _FILE_
 * @param line _LINE_
 * @return sample.cpp:line
 * */
char* getCodeLine(const char* file, int line) {
    static char str[100] = { 0 };
    const char* pNameBegin = file + strlen(file);
    do
    {
        if (*pNameBegin == '\\' || *pNameBegin == '/') { pNameBegin++; break; }
        if (pNameBegin == file) { break; }
        pNameBegin--;
    } while (true);
    memcpy(str, pNameBegin, strlen(pNameBegin));
    sprintf(str + strlen(pNameBegin), ":%d", line);
    return str;
}
/*
 * get such format: year-month-day hh:mm:ss
 * */
char* getCurTime() {
    static char str[100] = { 0 }; // data
    timeval tmv;
    gettimeofday(&tmv, NULL);
    tm tt = { 0 };
    localtime_r(&tmv.tv_sec, &tt);
    tt.tm_hour = 0;
    tt.tm_min = 0;
    tt.tm_sec = 0;
    time_t sec = tmv.tv_sec - mktime(&tt);
    sprintf(str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d",
            tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday,
            (int)sec / 3600, (int)(sec % 3600) / 60, (int)sec % 60, (int)tmv.tv_usec / 1000);
    return str;
}

bool accessMyDir(const char* dir_name) {
    DIR* dir = opendir(dir_name);
    if (!dir) {
        int err_code = mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (err_code < 0) {
            printf("%s Create dir %s error %s %s\n", getCurTime(), dir_name, strerror(errno), getCodeInfo());
            return false;
        }
    }
    else {
        closedir(dir);
    }
    return true;
}

int getFileNumInDir(const char *dirname) {
    const char *dir_name;
    std::string dirName;
    if (g_monthdir) {
        char temp[10] = { 0 };
        memcpy(temp, getCurTime(), 7);
        dirName += dirname;
        if (*(dirname + strlen(dirname) - 1) != '/') dirName += '/';
        dirName += temp;
        dir_name = (const char *)dirName.c_str();
    } else {
        dir_name = dirname;
    }
    int num = 0;
    if (!accessMyDir(dir_name)) return false;
    DIR *dir = opendir(dir_name);
    chdir(dir_name);
    struct dirent *dp;
    struct stat statbuf;
    while((dp = readdir(dir)) != nullptr) {
        lstat(dp->d_name, &statbuf);
        if(!S_ISDIR(statbuf.st_mode)) {
            num += 1;
        }
    };
    if (g_monthdir) chdir("../..");
    else chdir("..");
    closedir(dir);
    return num;
}
bool getFileName(MyFileName* file_name, char* char_file_name) {
    std::string fileName = "";
    if (file_name->file_path) {
        fileName += file_name->file_path;
        int len = strlen(file_name->file_path);
        const char* fpp = file_name->file_path + (len - 1);
        if (*fpp != '/') fileName += '/';
        if (!accessMyDir((const char*)fileName.c_str())) return false;
    }
    int name_len = 0;
    if (g_monthdir) {
        char temp[10] = { 0 };
        memcpy(temp, getCurTime(), 7);
        fileName += temp;
        if (!accessMyDir((const char*)fileName.c_str())) return false;
        fileName += "/";
    }
    if (file_name->prefix) {
        fileName += file_name->prefix;
        name_len += strlen(file_name->prefix);
    }
    if (file_name->use_cur_time) {
        char temp[20] = { 0 };
        memcpy(temp, getCurTime(), 10);
        if (fileName[fileName.length() - 1] != '/') {
            fileName += '_';
            name_len += 1;
        }
        fileName += temp;
        name_len += 10;
    }
    if (!name_len || !file_name->suffix) {
        printf("%s Invalid file name %s\n", getCurTime(), getCodeInfo());
        return false;
    }
    fileName += '.';
    fileName += file_name->suffix;
    if (!char_file_name) return false;
    sprintf(char_file_name, "%s", fileName.c_str());
    return true;
}

bool openMyFile(FILE* &my_file, MyFileName* file_name) {
    char char_file_name[50] = { 0 };
    if (!getFileName(file_name, char_file_name)) return false;
    my_file = fopen(char_file_name, file_name->mode);
    if (my_file == NULL) {
        printf("%s cannot open the file %s %s\n", getCurTime(), char_file_name, getCodeInfo());
        return false;
    }
    return true;
}

int getFileSize(const char *char_file_name) {
    struct stat statbuf;
    if (stat(char_file_name, &statbuf) == 0) {
        return (statbuf.st_size / 1024);
    }
    return -1;
}

int getFileSize(MyFileName* file_name)
{
    char char_file_name[50] = { 0 };
    if (!getFileName(file_name, char_file_name)) return false;
    return getFileSize(char_file_name);
}