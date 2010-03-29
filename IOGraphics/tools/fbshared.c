/*
cc -g -o /tmp/fbshared fbshared.c -framework ApplicationServices -framework IOKit -Wall -arch i386
*/
#define IOCONNECT_MAPMEMORY_10_6    1
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <mach/mach_time.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char * argv[])
{
    kern_return_t       status;
    io_service_t        service;
    io_connect_t        connect;
    mach_timebase_info_data_t timebase;
    StdFBShmem_t *      shmem;
    vm_size_t           shmemSize;
    CFNumberRef         clk, count;
    vm_address_t        mapAddr;

    service = CGDisplayIOServicePort(CGMainDisplayID());
   
    if (!service) service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                    IOServiceMatching(IOFRAMEBUFFER_CONFORMSTO));

    clk = IORegistryEntryCreateCFProperty(service, CFSTR(kIOFBCurrentPixelClockKey),
                                                            kCFAllocatorDefault, kNilOptions);
    count = IORegistryEntryCreateCFProperty(service, CFSTR(kIOFBCurrentPixelCountKey),
                                                            kCFAllocatorDefault, kNilOptions);
    if (clk && count)
    {
        float num, div;
        CFNumberGetValue(clk, kCFNumberFloatType, &num);
        CFNumberGetValue(count, kCFNumberFloatType, &div);
        printf("clock %.0f, count %.0f, rate %f Hz, period %f us\n",
                num, div, num / div, div * 1000 / num);
    }
    if (clk)
        CFRelease(clk);
    if (count)
        CFRelease(count);

    status = mach_timebase_info(&timebase);
    assert(kIOReturnSuccess == status);

    status = IOServiceOpen(service, mach_task_self(), kIOFBSharedConnectType, &connect);
    assert(kIOReturnSuccess == status);

    status = IOConnectMapMemory(connect, kIOFBCursorMemory, mach_task_self(),
                    &mapAddr, &shmemSize,
                    kIOMapAnywhere);
    assert(kIOReturnSuccess == status);

    shmem = (StdFBShmem_t *) mapAddr;

//    bzero( shmem, shmemSize); // make sure its read only!

    while (1)
    {
        uint64_t time  = (((uint64_t) shmem->vblTime.hi) << 32 | shmem->vblTime.lo);
        uint64_t delta = (((uint64_t) shmem->vblDelta.hi) << 32 | shmem->vblDelta.lo);
        double usecs = delta * timebase.numer / timebase.denom / 1e6;

        printf("time of last VBL 0x%qx, delta 0x%qx (%f us), count %qd\n", 
                time, delta, usecs, shmem->vblCount);

        sleep(1);
    }
    
    exit(0);
    return(0);
}

