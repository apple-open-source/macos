/*
cc -g -o /tmp/fbshared fbshared.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef kIOFBCurrentPixelClockKey
#define kIOFBCurrentPixelClockKey 	"IOFBCurrentPixelClock"
#endif

#ifndef kIOFBCurrentPixelCountKey
#define kIOFBCurrentPixelCountKey 	"IOFBCurrentPixelCount"
#endif


int main(int argc, char * argv[])
{
    kern_return_t	status;
    io_service_t 	service;
    io_connect_t	connect;
    StdFBShmem_t *	shmem2;
    StdFBShmem_t *	shmem;
    vm_size_t		shmemSize;
    CFNumberRef		clk, count;

    service = CGDisplayIOServicePort(CGMainDisplayID());

    clk = IORegistryEntryCreateCFProperty(service, CFSTR(kIOFBCurrentPixelClockKey),
							    kCFAllocatorDefault, kNilOptions);
    count = IORegistryEntryCreateCFProperty(service, CFSTR(kIOFBCurrentPixelCountKey),
							    kCFAllocatorDefault, kNilOptions);
    if (clk && count)
    {
	float num, div;
	CFNumberGetValue(clk, kCFNumberFloatType, &num);
	CFNumberGetValue(count, kCFNumberFloatType, &div);
	printf("Refresh rate: %f Hz\n", num / div);
    }
    if (clk)
	CFRelease(clk);
    if (count)
	CFRelease(count);

    status = IOServiceOpen(service, mach_task_self(), kIOFBSharedConnectType, &connect);
    assert(kIOReturnSuccess == status);
    status = IOConnectMapMemory(connect, kIOFBCursorMemory, mach_task_self(),
		    (vm_address_t *)&shmem, &shmemSize,
		    kIOMapAnywhere);
    assert(kIOReturnSuccess == status);

//    bzero( shmem, shmemSize); // make sure its read only!

    while (1)
    {
	printf("time of last VBL %qx, delta %qx, count %qd\n", shmem->vblTime, shmem->vblDelta, shmem->vblCount);
	sleep(1);
    }
    
    exit(0);
    return(0);
}

