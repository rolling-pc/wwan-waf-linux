#include "typedef.h"

void _sleep(int msec)
{
    struct timespec tv;
    int rval;
    tv.tv_sec = (time_t)(msec/1000);
    tv.tv_nsec = (msec%1000)*1000;
    while(1)
    {
        rval = nanosleep(&tv, &tv);
        if(rval == 0)
            return;
        else if(errno == EINTR)
            continue;
        else
            return;
    }
    return;
}
#if 0
unsigned long long GetCycleCount()
{
     unsigned long long temp;
     unsigned int low, high;
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high));
     temp = high;
     temp <<= 32;
     temp += low;
     return temp;
}
#endif

/* get sys tickcount */
unsigned long GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
