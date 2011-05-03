#include <time.h>

// Sleep milli seconds

// http://cc.byexamples.com/2007/05/25/nanosleep-is-better-than-sleep-and-usleep/

int msleep(unsigned long milisec)
{
    struct timespec req={0};
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
         continue;
    return 1;
}
