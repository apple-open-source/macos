/*
cc -o /tmp/timefb /tmp/timefb.c -framework IOKit -framework ApplicationServices -Wall -arch x86_64
*/

#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServicesPriv.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOAccelSurfaceControl.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include </System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/CoreGraphics.framework/Versions/A/PrivateHeaders/blt.h>
#include <libc.h>

double currentTime(void)
{
        struct timeval          v;

        gettimeofday(&v, NULL);
        return (v.tv_sec  +  (1e-6) * v.tv_usec);
}

int main(int argc, char * argv[])
{
    IOReturn            kr;
    CGDirectDisplayID   dspy = CGMainDisplayID();
    io_service_t        framebuffer;
    io_connect_t	connect;
    CGRect              bounds;
    mach_vm_size_t      size;
    CGSRowBytes         rb;
    mach_vm_address_t   p, mapAddr;
    double		t;
    int i;

    dspy = CGMainDisplayID();
    bounds = CGDisplayBounds(dspy);
//    rb = CGDisplayBytesPerRow(dspy);
    CGSGetDisplayRowBytes(dspy, &rb);

    framebuffer = CGDisplayIOServicePort(dspy);
    assert (framebuffer != MACH_PORT_NULL);
    kr = IOServiceOpen(framebuffer, mach_task_self(), kIOFBSharedConnectType, &connect);
    if (kIOReturnSuccess != kr)
    {
	printf("IOServiceOpen(%x)\n", kr);
	exit(1);
    }

    static IOOptionBits mapTypes[] = { 
    	kIOMapInhibitCache, kIOMapWriteThruCache, kIOMapCopybackCache, kIOMapWriteCombineCache };
    static const char * mapNames[] = { 
    	"kIOMapInhibitCache", "kIOMapWriteThruCache", "kIOMapCopybackCache", "kIOMapWriteCombineCache" };

    int mapType, x, y;
    for (mapType = 0; mapType < sizeof(mapTypes) / sizeof(mapTypes[0]); mapType++)
    {
	kr = IOConnectMapMemory64(connect, kIOFBVRAMMemory, mach_task_self(),
			&mapAddr, &size,
			kIOMapAnywhere | mapTypes[mapType]);
	if (kIOReturnSuccess != kr)
	{
	    printf("IOConnectMapMemory(%x)\n", kr);
	    exit(1);
	}
	mapAddr += 128*1024;
	t = currentTime();
	for (i = 0; i < 20; i++) {
	    CGBlt_fillBytes(bounds.size.width*4, bounds.size.height, 1, (void*)(uintptr_t) mapAddr, rb, 0);
	}
	printf("CGBlt_fillBytes(%s) %d times at (%dx%dx32): %g MB/sec\n", mapNames[mapType], i,
		(int)bounds.size.width, (int)bounds.size.height, 
		i*bounds.size.width*bounds.size.height*4/(1e6*(currentTime() - t)));

	for (i = 0; i < 20; i++) {
	    p = mapAddr;
	    for (y = 0; y < bounds.size.height; y++)
	    {
		for (x = 0; x < bounds.size.width; x++)
		{
		    ((uint32_t*)(uintptr_t)p)[x] = random();
		}
		p += rb;
	    }
	}
	printf("random(%s) %d times at (%dx%dx32): %g MB/sec\n", mapNames[mapType], i,
		(int)bounds.size.width, (int)bounds.size.height, 
		i*bounds.size.width*bounds.size.height*4/(1e6*(currentTime() - t)));

    
	kr = IOConnectUnmapMemory(connect, kIOFBVRAMMemory, mach_task_self(), mapAddr);
	if (kIOReturnSuccess != kr)
	{
	    printf("IOConnectUnmapMemory(%x)\n", kr);
	    exit(1);
	}
    }

    return (0);
}

