/*
cc -o /tmp/hibstat hibstat.c -Wall
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

    uint32_t graphicsReadyTime    = 1;
    uint32_t wakeNotificationTime = 2;
    uint32_t lockScreenReadyTime  = 3;
    uint32_t hidReadyTime         = 4;

    if ((sysctlbyname(kIOSysctlHibernateGraphicsReady, NULL, NULL, &graphicsReadyTime,    sizeof(graphicsReadyTime)) < 0)
     || (sysctlbyname(kIOSysctlHibernateWakeNotify,    NULL, NULL, &wakeNotificationTime, sizeof(wakeNotificationTime)) < 0)
     || (sysctlbyname(kIOSysctlHibernateScreenReady,   NULL, NULL, &lockScreenReadyTime,  sizeof(lockScreenReadyTime)) < 0)
     || (sysctlbyname(kIOSysctlHibernateHIDReady,      NULL, NULL, &hidReadyTime,         sizeof(hidReadyTime)) < 0)
     || (sysctlbyname(kIOSysctlHibernateStatistics, &stats, &len, NULL, 0) < 0))
    {
        printf("ERROR\n");
        exit (1);
    }

    printf("image1Size                   0x%qx\n", stats.image1Size);
    printf("imageSize                    0x%qx\n", stats.imageSize);
    printf("image1Pages                  %d\n",    stats.image1Pages);
    printf("imagePages                   %d\n",    stats.imagePages);
    printf("booterStart                  %d ms\n", stats.booterStart);
    printf("smcStart                     %d ms\n", stats.smcStart);
    printf("booterDuration0              %d ms\n", stats.booterDuration0);
    printf("booterDuration1              %d ms\n", stats.booterDuration1);
    printf("booterDuration2              %d ms\n", stats.booterDuration2);
    printf("booterDuration               %d ms\n", stats.booterDuration);
    printf("booterConnectDisplayDuration %d ms\n", stats.booterConnectDisplayDuration);
    printf("booterSplashDuration         %d ms\n", stats.booterSplashDuration);
    printf("trampolineDuration           %d ms\n", stats.trampolineDuration);
    printf("kernelImageReadDuration      %d ms\n", stats.kernelImageReadDuration);

    printf("graphicsReadyTime            %d ms\n", stats.graphicsReadyTime);
    printf("wakeNotificationTime         %d ms\n", stats.wakeNotificationTime);
    printf("lockScreenReadyTime          %d ms\n", stats.lockScreenReadyTime);
    printf("hidReadyTime                 %d ms\n", stats.hidReadyTime);

    exit(0);
}


