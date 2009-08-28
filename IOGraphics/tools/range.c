/*
cc -g -o /tmp/range range.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char * argv[])
{
    io_service_t 	service;
    CGError		err;
    int			i;
    CGDisplayCount	max;
    CGDirectDisplayID	displayIDs[8];
    uint32_t		mask;
    CFDataRef		fbRange;

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;

    if( argc < 2)
	mask = 0xffffffff;
    else
	mask = strtol( argv[1], 0, 0 );

    for(i = 0; i < max; i++ )
    {
	if (!(mask & (1 << i)))
	    continue;

        service = CGDisplayIOServicePort(displayIDs[i]);


        printf("Display %p:\n", displayIDs[i]);

	fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
								CFSTR(kIOFBTimingRangeKey),
								kCFAllocatorDefault, kNilOptions);
	if (fbRange && CFDataGetLength(fbRange) >= sizeof(IODisplayTimingRange))
	{
	    IODisplayTimingRange * range = (IODisplayTimingRange *) CFDataGetBytePtr(fbRange);

	    printf("  minPixelClock                 %qd\n", range->minPixelClock);
	    printf("  maxPixelClock                 %qd\n", range->maxPixelClock);
	    
	    printf("  maxPixelError                 %ld\n", range->maxPixelError);
	    printf("  supportedSyncFlags            %ld\n", range->supportedSyncFlags);
	    printf("  supportedSignalLevels         %ld\n", range->supportedSignalLevels);
	    printf("  supportedSignalConfigs        %ld\n", range->supportedSignalConfigs);
	    printf("  minFrameRate                  %ld\n", range->minFrameRate);
	    printf("  maxFrameRate                  %ld\n", range->maxFrameRate);
	    printf("  minLineRate                   %ld\n", range->minLineRate);
	    printf("  maxLineRate                   %ld\n", range->maxLineRate);

	    printf("  maxHorizontalTotal            %ld\n", range->maxHorizontalTotal);
	    printf("  maxVerticalTotal              %ld\n", range->maxVerticalTotal);
	    printf("  charSizeHorizontalActive      %d\n", range->charSizeHorizontalActive);
	    printf("  charSizeHorizontalBlanking    %d\n", range->charSizeHorizontalBlanking);
	    printf("  charSizeHorizontalSyncOffset  %d\n", range->charSizeHorizontalSyncOffset);
	    printf("  charSizeHorizontalSyncPulse   %d\n", range->charSizeHorizontalSyncPulse);
	    printf("  charSizeVerticalActive        %d\n", range->charSizeVerticalActive);
	    printf("  charSizeVerticalBlanking      %d\n", range->charSizeVerticalBlanking);
	    printf("  charSizeVerticalSyncOffset    %d\n", range->charSizeVerticalSyncOffset);
	    printf("  charSizeVerticalSyncPulse     %d\n", range->charSizeVerticalSyncPulse);
	    printf("  charSizeHorizontalBorderLeft  %d\n", range->charSizeHorizontalBorderLeft);
	    printf("  charSizeHorizontalBorderRight %d\n", range->charSizeHorizontalBorderRight);
	    printf("  charSizeVerticalBorderTop     %d\n", range->charSizeVerticalBorderTop);
	    printf("  charSizeVerticalBorderBottom  %d\n", range->charSizeVerticalBorderBottom);
	    printf("  charSizeHorizontalTotal       %d\n", range->charSizeHorizontalTotal);
	    printf("  charSizeVerticalTotal         %d\n", range->charSizeVerticalTotal);

	    printf("  minHorizontalActiveClocks     %ld\n", range->minHorizontalActiveClocks);
	    printf("  maxHorizontalActiveClocks     %ld\n", range->maxHorizontalActiveClocks);
	    printf("  minHorizontalBlankingClocks   %ld\n", range->minHorizontalBlankingClocks);
	    printf("  maxHorizontalBlankingClocks   %ld\n", range->maxHorizontalBlankingClocks);
	    printf("  minHorizontalSyncOffsetClocks %ld\n", range->minHorizontalSyncOffsetClocks);
	    printf("  maxHorizontalSyncOffsetClocks %ld\n", range->maxHorizontalSyncOffsetClocks);
	    printf("  minHorizontalPulseWidthClocks %ld\n", range->minHorizontalPulseWidthClocks);
	    printf("  maxHorizontalPulseWidthClocks %ld\n", range->maxHorizontalPulseWidthClocks);

	    printf("  minVerticalActiveClocks       %ld\n", range->minVerticalActiveClocks);
	    printf("  maxVerticalActiveClocks       %ld\n", range->maxVerticalActiveClocks);
	    printf("  minVerticalBlankingClocks     %ld\n", range->minVerticalBlankingClocks);
	    printf("  maxVerticalBlankingClocks     %ld\n", range->maxVerticalBlankingClocks);

	    printf("  minVerticalSyncOffsetClocks   %ld\n", range->minVerticalSyncOffsetClocks);
	    printf("  maxVerticalSyncOffsetClocks   %ld\n", range->maxVerticalSyncOffsetClocks);
	    printf("  minVerticalPulseWidthClocks   %ld\n", range->minVerticalPulseWidthClocks);
	    printf("  maxVerticalPulseWidthClocks   %ld\n", range->maxVerticalPulseWidthClocks);

	    printf("  minHorizontalBorderLeft       %ld\n", range->minHorizontalBorderLeft);
	    printf("  maxHorizontalBorderLeft       %ld\n", range->maxHorizontalBorderLeft);
	    printf("  minHorizontalBorderRight      %ld\n", range->minHorizontalBorderRight);
	    printf("  maxHorizontalBorderRight      %ld\n", range->maxHorizontalBorderRight);

	    printf("  minVerticalBorderTop          %ld\n", range->minVerticalBorderTop);
	    printf("  maxVerticalBorderTop          %ld\n", range->maxVerticalBorderTop);
	    printf("  minVerticalBorderBottom       %ld\n", range->minVerticalBorderBottom);
	    printf("  maxVerticalBorderBottom       %ld\n", range->maxVerticalBorderBottom);

	    printf("  maxNumLinks                   %ld\n", range->maxNumLinks);
	    printf("  minLink0PixelClock            %ld\n", range->minLink0PixelClock);
	    printf("  maxLink0PixelClock            %ld\n", range->maxLink0PixelClock);
	    printf("  minLink1PixelClock            %ld\n", range->minLink1PixelClock);
	    printf("  maxLink1PixelClock            %ld\n", range->maxLink1PixelClock);

	}

	fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
								CFSTR(kIOFBScalerInfoKey),
								kCFAllocatorDefault, kNilOptions);
	if (fbRange && CFDataGetLength(fbRange) >= sizeof(IODisplayScalerInformation))
	{
	    IODisplayScalerInformation * range = (IODisplayScalerInformation *) CFDataGetBytePtr(fbRange);
	    printf("IODisplayScalerInformation:\n");
	    printf("  scalerFeatures                %lx\n", range->scalerFeatures);
	    printf("  maxHorizontalPixels           %ld\n", range->maxHorizontalPixels);
	    printf("  maxVerticalPixels             %ld\n", range->maxVerticalPixels);
	}


    }
    
    exit(0);
    return(0);
}

