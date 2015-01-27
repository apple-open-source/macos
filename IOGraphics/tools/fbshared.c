/*
cc -g -o /tmp/fbshared fbshared.c -framework ApplicationServices -framework IOKit -Wall -arch i386
*/
#define IOCONNECT_MAPMEMORY_10_6    1
#define IOFB_ARBITRARY_SIZE_CURSOR
#define IOFB_ARBITRARY_FRAMES_CURSOR    1
#include <IOKit/graphics/IOFramebufferShared.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <mach/mach_time.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char * argv[])
{
    kern_return_t       kr;
    io_iterator_t       iter;
    io_service_t        framebuffer;
    io_string_t         path;
    uint32_t            index, maxIndex;
    io_connect_t        connect;
    mach_timebase_info_data_t timebase;
    StdFBShmem_t *      shmem[16];
    vm_size_t           shmemSize;
    CFNumberRef         clk, count;
    vm_address_t        mapAddr;

    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(
                IOFRAMEBUFFER_CONFORMSTO), &iter);
    assert( KERN_SUCCESS == kr );

    for ( index = 0; 
            index++, (framebuffer = IOIteratorNext(iter));
            IOObjectRelease(framebuffer))
    {
        kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
        assert( KERN_SUCCESS == kr );
        printf("\n/* [%d] Using device: %s */\n", index, path);

	clk = IORegistryEntryCreateCFProperty(framebuffer, CFSTR(kIOFBCurrentPixelClockKey),
								kCFAllocatorDefault, kNilOptions);
	count = IORegistryEntryCreateCFProperty(framebuffer, CFSTR(kIOFBCurrentPixelCountKey),
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
    
	kr = mach_timebase_info(&timebase);
	assert(kIOReturnSuccess == kr);
    
	kr = IOServiceOpen(framebuffer, mach_task_self(), kIOFBSharedConnectType, &connect);
	if (kIOReturnSuccess != kr)
	{
    	    printf("IOServiceOpen(%x)\n", kr);
    	    continue;
    	}
	kr = IOConnectMapMemory(connect, kIOFBCursorMemory, mach_task_self(),
			&mapAddr, &shmemSize,
			kIOMapAnywhere);
	if (kIOReturnSuccess != kr)
	{
    	    printf("IOConnectMapMemory(%x)\n", kr);
    	    continue;
    	}
	shmem[index] = (StdFBShmem_t *) mapAddr;
    
 //     bzero( shmem, shmemSize); // make sure its read only!
    
	printf("screenBounds (%d, %d), (%d, %d)\n",
		shmem[index]->screenBounds.minx, shmem[index]->screenBounds.miny, 
		shmem[index]->screenBounds.maxx, shmem[index]->screenBounds.maxy);

	printf("size[0] (%d, %d)\n",
		shmem[index]->cursorSize[0].width, shmem[index]->cursorSize[0].height);

        if (shmem[index]->hardwareCursorActive && !shmem[index]->cursorShow)
        do 
        {
            struct {
               uint8_t  identLength;
               uint8_t  colorMapType;
               uint8_t  dataType;
               uint8_t  colorMap[5];
               uint16_t origin[2];
               uint16_t width;
               uint16_t height;
               uint8_t  bitsPerPixel;
               uint8_t  imageDesc;
            } hdr;
            FILE *    f;
            char      path[256];
            uint8_t * bits;
            uint32_t  w, h, y;

            w = shmem[index]->cursorSize[0].width;
            h = shmem[index]->cursorSize[0].height;

            bzero(&hdr, sizeof(hdr));
            hdr.dataType     = 2;
            hdr.width        = OSSwapHostToLittleInt16(w);
            hdr.height       = OSSwapHostToLittleInt16(h);
            hdr.bitsPerPixel = 32;
            hdr.imageDesc    = (1<<5) | 8;
    
            snprintf(path, sizeof(path), "/tmp/curs%d.tga", index);
            f = fopen(path, "w" /*"r+"*/);
            if (!f) continue;
            fwrite(&hdr, sizeof(hdr), 1, f);
            bits = (uint8_t *)(uintptr_t) &shmem[index]->cursor[0];
            for (y = 0; y < h; y++)
            {
                fwrite(bits, sizeof(uint32_t), w, f);
                bits += w * sizeof(uint32_t);
            }
            fclose(f);
        }
        while (false);
    }

    maxIndex = index;
    while (true)
    {
	printf("\n");
	for (index = 0; index < maxIndex; index++)
	{
	    if (!shmem[index])
		continue;
    
	    uint64_t time      = (((uint64_t) shmem[index]->vblTime.hi) << 32 | shmem[index]->vblTime.lo);
	    uint64_t delta     = (((uint64_t) shmem[index]->vblDelta.hi) << 32 | shmem[index]->vblDelta.lo);
	    uint64_t deltaReal = (((uint64_t) shmem[index]->vblDeltaReal.hi) << 32 | shmem[index]->vblDeltaReal.lo);
	    double usecs     = delta * timebase.numer / timebase.denom / 1e6;
	    double usecsReal = deltaReal * timebase.numer / timebase.denom / 1e6;

		if (!delta) continue;
    
	    printf("[%d] time of last VBL 0x%qx, delta %qd (%f us), unthrottled delta %qd (%f us), count %qd, measured delta %qd(%f%%), drift %qd(%qd%%)\n", 
		    index, time, delta, usecs, deltaReal, usecsReal, shmem[index]->vblCount,
		    shmem[index]->vblDeltaMeasured, ((shmem[index]->vblDeltaMeasured * 100.0) / delta),
		    shmem[index]->vblDrift, ((shmem[index]->vblDrift * 100) / delta));
	}
	for (index = 0; index < maxIndex; index++)
	{
	    if (!shmem[index]) continue;
		if ((shmem[index]->screenBounds.maxx - shmem[index]->screenBounds.minx) < 128) continue;

	    printf("[%d] cursorShow %d, hw %d, frame %d, loc (%d, %d), hs (%d, %d), cursorRect (%d, %d), (%d, %d), saveRect (%d, %d), (%d, %d)\n",
		    index, 
		    shmem[index]->cursorShow, shmem[index]->hardwareCursorActive, 
		    shmem[index]->frame, shmem[index]->cursorLoc.x, shmem[index]->cursorLoc.y,
			shmem[index]->hotSpot[0].x, shmem[index]->hotSpot[0].y,
		    shmem[index]->cursorRect.minx, shmem[index]->cursorRect.miny, 
		    shmem[index]->cursorRect.maxx, shmem[index]->cursorRect.maxy,
		    shmem[index]->saveRect.minx, shmem[index]->saveRect.miny, 
		    shmem[index]->saveRect.maxx, shmem[index]->saveRect.maxy);
	}
	sleep(1);
    }
    
    exit(0);
    return(0);
}

