/*
cc -g -o /tmp/setdparam setdparam.c -framework ApplicationServices -framework IOKit -Wall
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
    CFStringRef		key;
    float 		value;

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;

    if( argc < 2)
	key = CFSTR(kIODisplayBrightnessKey);
    else
	key = CFStringCreateWithCString( kCFAllocatorDefault, argv[1],
					kCFStringEncodingMacRoman );


    for(i = 0; i < max; i++ ) {

        service = CGDisplayIOServicePort(displayIDs[i]);

        err = IODisplayGetFloatParameter(service, kNilOptions, key, &value);
        printf("Display %x: IODisplayGetFloatParameter(%d), %f\n", displayIDs[i], err, value);
        if( kIOReturnSuccess != err)
            continue;

	if (argc < 3)
            continue;

	sscanf( argv[argc - 1], "%f", &value );
        err = IODisplaySetFloatParameter(service, kNilOptions, key, value);
        printf("IODisplaySetFloatParameter(%d, %f)\n", err, value);
        if( kIOReturnSuccess != err)
            continue;
    }
    
    exit(0);
    return(0);
}

