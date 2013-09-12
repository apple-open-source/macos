/*
cc -o /tmp/hibstats hibstats.c -Wall
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <IOKit/IOHibernatePrivate.h>

int main(int argc, char * argv[])
{
    hibernate_statistics_t stats;
    size_t len = sizeof(stats);
    if(sysctlbyname("kern.hibernatestatistics", &stats, &len, NULL, 0) < 0)
    {
        printf("ERROR\n");
        exit (1);
    }

    printf("image1Size         0x%qx\n", stats.image1Size);
    printf("imageSize          0x%qx\n", stats.imageSize);
    printf("image1Pages        %d\n",    stats.image1Pages);
    printf("imagePages         %d\n",    stats.imagePages);
    printf("booterStart        %d ms\n", stats.booterStart);
    printf("smcStart           %d ms\n", stats.smcStart);
    printf("booterTime0        %d ms\n", stats.booterTime0);
    printf("booterTime1        %d ms\n", stats.booterTime1);
    printf("booterTime2        %d ms\n", stats.booterTime2);
    printf("booterTime         %d ms\n", stats.booterTime);
    printf("connectDisplayTime %d ms\n", stats.connectDisplayTime);
    printf("splashTime         %d ms\n", stats.splashTime);
    printf("trampolineTime     %d ms\n", stats.trampolineTime);
    printf("kernelImageTime    %d ms\n", stats.kernelImageTime);

    exit(0);
}


