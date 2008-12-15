/*
cc -g -o /tmp/fbshared fbshared.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
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
    StdFBShmem_t *	shmem;
    vm_size_t		shmemSize;
    CFNumberRef		clk, count;

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
	printf("Refresh rate: %f Hz\n", num / div);
    }
    if (clk)
	CFRelease(clk);
    if (count)
	CFRelease(count);

    status = IOServiceOpen(service, mach_task_self(), kIOFBSharedConnectType, &connect);
    assert(kIOReturnSuccess == status);

#if __LP64__
    mach_vm_address_t mapAddr;
    mach_vm_size_t    mapSize;
#else
    vm_address_t mapAddr;
    vm_size_t    mapSize;
#endif

    status = IOConnectMapMemory(connect, kIOFBCursorMemory, mach_task_self(),
		    &mapAddr, &mapSize,
		    kIOMapAnywhere);
    assert(kIOReturnSuccess == status);

    shmem = (StdFBShmem_t *) mapAddr;
    shmemSize = mapSize;

//    bzero( shmem, shmemSize); // make sure its read only!

    while (1)
    {
#if __LP64__
	printf("time of last VBL 0x%x%08x, delta 0x%x%08x, count %qd\n", 
		shmem->vblTime.hi, shmem->vblTime.lo, 
		shmem->vblDelta.hi, shmem->vblDelta.lo, 
		shmem->vblCount);
#else
	printf("time of last VBL 0x%lx%08lx, delta 0x%lx%08lx, count %qd\n", 
		shmem->vblTime.hi, shmem->vblTime.lo, 
		shmem->vblDelta.hi, shmem->vblDelta.lo, 
		shmem->vblCount);
#endif
	sleep(1);
    }
    
    exit(0);
    return(0);
}

