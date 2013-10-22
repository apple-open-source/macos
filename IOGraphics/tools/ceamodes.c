/*
cc ceamodes.c -o /tmp/ceamodes -Wall -framework CoreFoundation
*/
#include <sys/cdefs.h>

#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libc.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>

#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsLibPrivate.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOGraphicsEngine.h>


// These values are taken directly from the CEA861 spec.
// See Tables 3 and 4 of the specification for details.
typedef struct
{
    bool        supported;
    uint16_t    hActive;
    uint16_t    vActive;
    uint16_t    interlaced;
    uint16_t    hTotal;
    uint16_t    hBlank;
    uint16_t    vTotal;
    uint16_t    vBlank;
    float       pClk;
    uint16_t    hFront;
    uint16_t    hSync;
    uint16_t    hBack;
    uint16_t    hPolarity;
    uint16_t    vFront;
    uint16_t    vSync;
    uint16_t    vBack;
    uint16_t    vPolarity;
} CEAVideoFormatData;

#define MAX_CEA861_VIDEO_FORMATS 64


static const CEAVideoFormatData gHDMIVICFormats[] = 
{
// 0 - Unused
{ false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
// 1 - 4K x 2K 30Hz
{ true, 3840, 2160, 0, 3840+560,   560, 2160+90, 90, 297,  176, 88, 296, 1, 8, 10, 72, 1 },
// 2 - 4K x 2K 25Hz
{ true, 3840, 2160, 0, 3840+1440, 1440, 2160+90, 90, 297, 1056, 88, 296, 1, 8, 10, 72, 1 },
// 3 - 4K x 2K 24Hz
{ true, 3840, 2160, 0, 3840+1660, 1660, 2160+90, 90, 297, 1276, 88, 296, 1, 8, 10, 72, 1 },
// 4 - 4K x 2K 24Hz SMPTE
{ true, 4096, 2160, 0, 4096+1404, 1404, 2160+90, 90, 297, 1020, 88, 296, 1, 8, 10, 72, 1 },
};

static const CEAVideoFormatData gCEAVideoFormats[] = 
{
// 0 - Unused
{ false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
// 1 - 640x480p
{ true, 640, 480, 0, 800, 160, 525, 45, 25.175, 16, 96, 48, 0, 10, 2, 33, 0 },
// 2 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 27, 16, 62, 60, 0, 9, 6, 30, 0 },
// 3 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 27, 16, 62, 60, 0, 9, 6, 30, 0 },
// 4 - 1280x720p
{ true, 1280, 720, 0, 1650, 370, 750, 30, 74.25, 110, 40, 220, 1, 5, 5, 20, 1 },
// 5 - 1920x1080i
{ true, 1920, 1080, 1, 2200, 280, 1125, 22.5, 74.25, 88, 44, 148, 1, 2, 5, 15, 1 },
// 6 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 27, 38, 124, 114, 0, 4, 3, 15, 0 },
// 7 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 27, 38, 124, 114, 0, 4, 3, 15, 0 },
// 8 - 720(1440)x240p
{ false, 1440, 240, 0, 1716, 276, 262, 22, 27, 38, 124, 114, 0, 4, 3, 15, 0 },
// 9 - 720(1440)x240p
{ false, 1440, 240, 0, 1716, 276, 262, 22, 27, 38, 124, 114, 0, 4, 3, 15, 0 },
// 10 - 2880x480i
{ false, 2880, 480, 1, 3432, 552, 525, 22.5, 54, 76, 248, 228, 0, 4, 3, 15, 0 },
// 11 - 2880x480i
{ false, 2880, 480, 1, 3432, 552, 525, 22.5, 54, 76, 248, 228, 0, 4, 3, 15, 0 },
// 12 - 2880x240p
{ false, 2880, 240, 0, 3432, 552, 262, 22, 54, 76, 248, 228, 0, 4, 3, 15, 0 },
// 13 - 2880x240p
{ false, 2880, 240, 0, 3432, 552, 262, 22, 54, 76, 248, 228, 0, 4, 3, 15, 0 },
// 14 - 1440x480p
{ false, 1440, 480, 0, 1716, 276, 525, 45, 54, 32, 124, 120, 0, 9, 6, 30, 0 },
// 15 - 1440x480p
{ false, 1440, 480, 0, 1716, 276, 525, 45, 54, 32, 124, 120, 0, 9, 6, 30, 0 },
// 16 - 1920x1080p
{ true, 1920, 1080, 0, 2200, 280, 1125, 45, 148.5, 88, 44, 148, 1, 4, 5, 36, 1 },
// 17 - 720x576p
{ true, 720, 576, 0, 864, 144, 625, 49, 27, 12, 64, 68, 0, 5, 5, 39, 0 },
// 18 - 720x576p
{ true, 720, 576, 0, 864, 144, 625, 49, 27, 12, 64, 68, 0, 5, 5, 39, 0 },
// 19 - 1280x720p
{ true, 1280, 720, 0, 1980, 700, 750, 30, 74.25, 440, 40, 220, 1, 5, 5, 20, 1 },
// 20 - 1920x1080i
{ true, 1920, 1080, 1, 2640, 720, 1125, 22.5, 74.25, 528, 44, 148, 1, 2, 5, 15, 1 },
// 21 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 27, 24, 126, 138, 0, 2, 3, 19, 0 },
// 22 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 27, 24, 126, 138, 0, 2, 3, 19, 0 },
// 23 - 720(1440)x288p
{ false, 1440, 288, 0, 1728, 288, 312, 24, 27, 24, 126, 138, 0, 2, 3, 19, 0 },
// 24 - 720(1440)x288p
{ false, 1440, 288, 0, 1728, 288, 312, 24, 27, 24, 126, 138, 0, 2, 3, 19, 0 },
// 25 - 2880x576i
{ false, 2880, 576, 1, 3456, 576, 625, 24.5, 54, 48, 252, 276, 0, 2, 3, 19, 0 },
// 26 - 2880x576i
{ false, 2880, 576, 1, 3456, 576, 625, 24.5, 54, 48, 252, 276, 0, 2, 3, 19, 0 },
// 27 - 2880x288p
{ false, 2880, 288, 0, 3456, 576, 312, 24, 54, 48, 252, 276, 0, 2, 3, 19, 0 },
// 28 - 2880x288p
{ false, 2880, 288, 0, 3456, 576, 312, 24, 54, 48, 252, 276, 0, 2, 3, 19, 0 },
// 29 - 1440x576p
{ false, 1440, 576, 0, 1728, 288, 625, 49, 54, 24, 128, 136, 0, 5, 5, 39, 0 },
// 30 - 1440x576p
{ false, 1440, 576, 0, 1728, 288, 625, 49, 54, 24, 128, 136, 0, 5, 5, 39, 0 },
// 31 - 1920x1080p
{ true, 1920, 1080, 0, 2640, 720, 1125, 45, 148.5, 528, 44, 148, 1, 4, 5, 36, 1 },
// 32 - 1920x1080p
{ true, 1920, 1080, 0, 2750, 830, 1125, 45, 74.25, 638, 44, 148, 1, 4, 5, 36, 1 },
// 33 - 1920x1080p
{ true, 1920, 1080, 0, 2640, 720, 1125, 45, 74.25, 528, 44, 148, 1, 4, 5, 36, 1 },
// 34 - 1920x1080p
{ true, 1920, 1080, 0, 2200, 280, 1125, 45, 74.25, 88, 44, 148, 1, 4, 5, 36, 1 },
// 35 - 2880x480p
{ false, 2880, 480, 0, 3432, 552, 525, 45, 108, 64, 248, 240, 0, 9, 6, 30, 0 },
// 36 - 2880x480p
{ false, 2880, 480, 0, 3432, 552, 525, 45, 108, 64, 248, 240, 0, 9, 6, 30, 0 },
// 37 - 2880x576p
{ false, 2880, 576, 0, 3456, 576, 625, 49, 108, 48, 256, 272, 0, 5, 5, 39, 0 },
// 38 - 2880x576p
{ false, 2880, 576, 0, 3456, 576, 625, 49, 108, 48, 256, 272, 0, 5, 5, 39, 0 },
// 39 - 1920x1080i
{ true, 1920, 1080, 1, 2304, 384, 1250, 85, 72, 32, 168, 184, 1, 23, 5, 57, 0 },
// 40 - 1920x1080i
{ true, 1920, 1080, 1, 2640, 720, 1125, 22.5, 148.5, 528, 44, 148, 1, 2, 5, 15, 1 },
// 41 - 1280x720p
{ true, 1280, 720, 0, 1980, 700, 750, 30, 148.5, 440, 40, 220, 1, 5, 5, 20, 1 },
// 42 - 720x576
{ true, 720, 576, 0, 864, 144, 625, 49, 54, 12, 64, 68, 0, 5, 5, 39, 0 },
// 43 - 720x576p
{ true, 720, 576, 0, 864, 144, 625, 49, 54, 12, 64, 68, 0, 5, 5, 39, 0 },
// 44 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 54.0, 24, 12, 138, 0, 2, 3, 19, 0 },
// 45 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 54.0, 24, 6, 138, 0, 2, 3, 19, 0 },
// 46 - 1920x1080i
{ true, 1920, 1080, 1, 2200, 280, 1125, 22.5, 148.5, 88, 44, 148, 1, 2, 5, 15, 1 },
// 47 - 1280x720p
{ true, 1280, 720, 0, 1650, 370, 750, 30, 148.5, 110, 40, 220, 1, 5, 5, 20, 1 },
// 48 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 54.0, 16, 62, 60, 0, 9, 6, 30, 0 },
// 49 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 54.0, 16, 62, 60, 0, 9, 6, 30, 0 },
// 50 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 54.0, 38, 12, 114, 0, 4, 3, 15, 0 },
// 51 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 54.0, 38, 4, 114, 0, 4, 3, 15, 0 },
// 52 - 720x576p
{ true, 720, 576, 0, 864, 144, 625, 49, 108.0, 12, 64, 68, 0, 5, 5, 39, 0 },
// 53 - 720x576p
{ true, 720, 576, 0, 864, 144, 625, 49, 108.0, 12, 64, 68, 0, 5, 5, 39, 0 },
// 54 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 108.0, 24, 12, 138, 0, 2, 3, 19, 0 },
// 55 - 720(1440)x576i
{ false, 1440, 576, 1, 1728, 288, 625, 24.5, 108.0, 24, 6, 138, 0, 2, 3, 19, 0 },
// 56 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 108.108, 16, 62, 60, 0, 9, 6, 30, 0 },
// 57 - 720x480p
{ true, 720, 480, 0, 858, 138, 525, 45, 108.108, 16, 62, 60, 0, 9, 6, 30, 0 },
// 58 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 108.108, 38, 12, 114, 0, 4, 3, 15, 0 },
// 59 - 720(1440)x480i
{ false, 1440, 480, 1, 1716, 276, 525, 22.5, 108.108, 38, 4, 114, 0, 4, 3, 15, 0 },
// 60 - 1280x720p
{ true, 1280, 720, 0, 3300, 2020, 750, 30, 59.4, 1760, 40, 220, 1, 5, 5, 20, 1 },
// 61 - 1280x720p
{ true, 1280, 720, 0, 3960, 2680, 750, 30, 74.25, 2420, 40, 220, 1, 5, 5, 20, 1 },
// 62 - 1280x720p
{ true, 1280, 720, 0, 3300, 2020, 750, 30, 74.25, 1760, 40, 220, 1, 5, 5, 20, 1 },
// 63 - 1920x1080p
{ true, 1920, 1080, 0, 2200, 280, 1125, 45, 297.0, 88, 44, 148, 1, 4, 5, 36, 1 },
// 64 - 1920x1080p
{ true, 1920, 1080, 0, 2640, 720, 1125, 45, 297.0, 528, 44, 148, 1, 4, 5, 36, 1 }
};


static IOReturn
CEAVideoFormatDataToDetailedTiming(const CEAVideoFormatData * mode, IODetailedTimingInformation * timing)
{
    bzero( timing, sizeof( IODetailedTimingInformation) );

    if( !mode->supported )
        return( kIOReturnUnsupported );
                
    timing->pixelClock          = (UInt64) (mode->pClk * 1000000.0);
    timing->maxPixelClock       = timing->pixelClock;
    timing->minPixelClock       = timing->pixelClock;

    timing->horizontalActive    = mode->hActive;
    timing->verticalActive      = mode->vActive;

    timing->horizontalBlanking  = mode->hBlank;
    timing->verticalBlanking    = mode->vTotal - mode->vActive;

    timing->horizontalSyncOffset        = mode->hFront;
    timing->horizontalSyncPulseWidth    = mode->hSync;
    timing->horizontalSyncConfig        = mode->hPolarity ? kIOSyncPositivePolarity : 0;

    if( mode->interlaced )
    {
        timing->verticalSyncOffset      = mode->vFront << 1;
        timing->verticalSyncPulseWidth  = mode->vSync << 1;
        timing->signalConfig            |= kIOInterlacedCEATiming;
    }
    else
    {
        timing->verticalSyncOffset      = mode->vFront;
        timing->verticalSyncPulseWidth  = mode->vSync;
    }
    timing->verticalSyncConfig          = mode->vPolarity ? kIOSyncPositivePolarity : 0;
        
    return( kIOReturnSuccess );
}

#define arrayCount(x)	(sizeof(x) / sizeof(x[0]))

int main(int argc, char * argv[])
{
	const CEAVideoFormatData * ceaData;
    IODetailedTimingInformationV2 timing;
	uint32_t type, count, idx;
	IOReturn err;
	CFTypeRef obj;
	CFMutableArrayRef array;
	CFMutableDictionaryRef dict;

	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
									 &kCFTypeDictionaryKeyCallBacks,
									 &kCFTypeDictionaryValueCallBacks);
	for (type = 0; type < 2; type++)
	{
		array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		if (type)
		{
		   ceaData = &gHDMIVICFormats[0];
		   count   = arrayCount(gHDMIVICFormats);
		   CFDictionarySetValue(dict, CFSTR("hdmi-vic-modes"), array);
		}
		else
		{
			ceaData = &gCEAVideoFormats[0];
			count   = arrayCount(gCEAVideoFormats);
			CFDictionarySetValue(dict, CFSTR("cea-modes"), array);
		}
		for (idx = 0; idx < count; idx++)
		{
			bzero(&timing, sizeof(IODetailedTimingInformation));
			err = CEAVideoFormatDataToDetailedTiming(ceaData, &timing);
			if (kIOReturnSuccess == err)
			{
				obj = CFDataCreate(kCFAllocatorDefault,
								   (const UInt8 *) &timing, sizeof(timing));
			}
			else obj = kCFBooleanFalse;
			CFArrayAppendValue(array, obj);
			ceaData++;
		}
	}

	CFDataRef
	data = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);
	printf("\n\n%s\n\n", CFDataGetBytePtr(data));
}

