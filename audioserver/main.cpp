#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "global.h"

using namespace std;
extern struct tm g_tCurLocalTime;//local time

int32_t main(int32_t argc, char *argv[])
{
    int c = 1;
    if (argc > 1) {
        if (!strcmp(argv[1], "-d"))
        {
            c++;
            daemon(1,0);
        }
        //g_iServerMode = atoi(argv[c]);
    }

    if (already_running())
    {
        exit(1);
    }
    g_StartTime = g_CurrentTime = time(NULL);
    g_tCurLocalTime = *localtime(&g_CurrentTime);
    strftime(g_strCurTime, sizeof(g_strCurTime), "%Y.%m.%d %H:%M:%S", &g_tCurLocalTime);//generate string with format of  "YYYY-MM-DD hh:mm:ss"

    int rc;
    sigset_t new_mask, old_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGPIPE);

    rc = sigprocmask(SIG_BLOCK, &new_mask, &old_mask);//add signals in new_mask into the mask set
    //rc = sigprocmask(SIG_SETMASK, &new_mask, &old_mask);//modify the mask set to new_mask
    //rc = pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
    if (rc != 0) {
        fprintf(stderr, "***block sigpipe error@%s\n", g_strCurTime);
        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)//
        {
            fprintf(stderr, "***signal(SIGPIPE, SIG_IGN) failed: %d, %s @%s\n", errno, strerror(errno), g_strCurTime);
        }
    }

    systemInitialize();

    return 0;
}
