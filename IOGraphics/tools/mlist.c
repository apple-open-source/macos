// cc -o /tmp/mlist -g mlist.c -framework ApplicationServices -framework IOKit -Wall


#include <mach/mach.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <ApplicationServices/ApplicationServices.h>

int main(int argc, char * argv[])
{
    kern_return_t 				kr;
    io_iterator_t 				iter;
    io_service_t  				framebuffer;
    io_string_t   				path;
    uint32_t                    index;
    CFIndex						idx;
    IODisplayModeInformation *  modeInfo;
    IODetailedTimingInformationV2 *timingInfo;

    CFDictionaryRef             dict;
    CFArrayRef                  modes;
    CFIndex                     count;
    CFDictionaryRef             mode;
    CFDataRef                   data;
    CFNumberRef                 num;

    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(
                IOFRAMEBUFFER_CONFORMSTO), &iter);
    assert( KERN_SUCCESS == kr );

    for ( index = 0; 
            index++, (framebuffer = IOIteratorNext(iter));
            IOObjectRelease(framebuffer))
    {
        kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
        assert( KERN_SUCCESS == kr );
        fprintf(stderr, "\n%s\n", path);

        dict = IORegistryEntryCreateCFProperty(framebuffer, CFSTR(kIOFBConfigKey),
                                                kCFAllocatorDefault, kNilOptions);
		if (!dict) continue;

        modes = CFDictionaryGetValue(dict, CFSTR(kIOFBModesKey));
        assert(modes);

        count = CFArrayGetCount(modes);
        for (idx = 0; idx < count; idx++)
        {
            mode = CFArrayGetValueAtIndex(modes, idx);

            data = CFDictionaryGetValue(mode, CFSTR(kIOFBModeDMKey));
            if (!data)
                continue;
            modeInfo = (IODisplayModeInformation *) CFDataGetBytePtr(data);

            data = CFDictionaryGetValue(mode, CFSTR(kIOFBModeTMKey));
            if (!data)
                continue;
            timingInfo = (IODetailedTimingInformationV2 *) CFDataGetBytePtr(data);

			IODisplayModeID modeID = 0;
            num = CFDictionaryGetValue(mode, CFSTR(kIOFBModeIDKey));
            if (num) CFNumberGetValue(num, kCFNumberSInt32Type, &modeID );

            printf("0x%x: %d x %d %d Hz - %d x %d - flags 0x%x\n",
            		modeID,
            		modeInfo->nominalWidth, modeInfo->nominalHeight, 
            		((modeInfo->refreshRate + 0x8000) >> 16),
            		modeInfo->imageWidth, modeInfo->imageHeight, 
            		modeInfo->flags);

			if (argc > 1)
			{
				printf("  horizontalScaledInset    %d\n", timingInfo->horizontalScaledInset);
				printf("  verticalScaledInset      %d\n", timingInfo->verticalScaledInset);
				printf("  scalerFlags              0x%x\n", timingInfo->scalerFlags);
				printf("  horizontalScaled         %d\n", timingInfo->horizontalScaled);
				printf("  verticalScaled           %d\n", timingInfo->verticalScaled);
				printf("  pixelClock               %lld\n", timingInfo->pixelClock);
				printf("  minPixelClock            %lld\n", timingInfo->minPixelClock);
				printf("  maxPixelClock            %lld\n", timingInfo->maxPixelClock);
				printf("  horizontalActive         %d\n", timingInfo->horizontalActive);
				printf("  horizontalBlanking       %d\n", timingInfo->horizontalBlanking);
				printf("  horizontalSyncOffset     %d\n", timingInfo->horizontalSyncOffset);
				printf("  horizontalSyncPulseWidth %d\n", timingInfo->horizontalSyncPulseWidth);
				printf("  verticalActive           %d\n", timingInfo->verticalActive);
				printf("  verticalBlanking         %d\n", timingInfo->verticalBlanking);
				printf("  verticalSyncOffset       %d\n", timingInfo->verticalSyncOffset);
				printf("  verticalSyncPulseWidth   %d\n", timingInfo->verticalSyncPulseWidth);
				printf("  horizontalBorderLeft     %d\n", timingInfo->horizontalBorderLeft);
				printf("  horizontalBorderRight    %d\n", timingInfo->horizontalBorderRight);
				printf("  verticalBorderTop        %d\n", timingInfo->verticalBorderTop);
				printf("  verticalBorderBottom     %d\n", timingInfo->verticalBorderBottom);
				printf("  horizontalSyncConfig     %d\n", timingInfo->horizontalSyncConfig);
				printf("  horizontalSyncLevel      %d\n", timingInfo->horizontalSyncLevel);
				printf("  verticalSyncConfig       0x%x\n", timingInfo->verticalSyncConfig);
				printf("  verticalSyncLevel        %d\n", timingInfo->verticalSyncLevel);
				printf("  signalConfig             0x%x\n", timingInfo->signalConfig);
				printf("  signalLevels             %d\n", timingInfo->signalLevels);
				printf("  numLinks                 %d\n", timingInfo->numLinks);
			}
		}
		CFRelease(dict);
    }
    IOObjectRelease(iter);
    exit(0);
    return(0);
}
