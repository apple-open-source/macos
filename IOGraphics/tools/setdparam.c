/*
cc -g -o /tmp/setdparam setdparam.c -framework ApplicationServices -framework IOKit -Wall -arch i386
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char * argv[])
{
    io_service_t        service;
    io_string_t         path;
    CGError             err;
    int                 i;
    CGDisplayCount      max;
    CGDirectDisplayID   displayIDs[8];
    CFStringRef         key;
    float               value;
    SInt32              ivalue, imin, imax;

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


    for(i = 0; i < max; i++, IOObjectRelease(service) )
    {
        service = CGDisplayIOServicePort(displayIDs[i]);
        if(MACH_PORT_NULL == service)
            continue;

        err = IORegistryEntryGetPath(service, kIOServicePlane, path);
        if( kIOReturnSuccess != err)
        {
            printf("IORegistryEntryGetPath(err 0x%x, %d)\n", err, service);
            continue;
        }
        printf("framebuffer: %s\n", path);
        service = IODisplayForFramebuffer(service, kNilOptions);
        if(MACH_PORT_NULL == service)
        {
            printf("no display there\n", err, service);
            continue;
        }
        err = IORegistryEntryGetPath(service, kIOServicePlane, path);
        if( kIOReturnSuccess != err)
        {
            printf("IORegistryEntryGetPath(err 0x%x, %d)\n", err, service);
            continue;
        }
        printf("display: %s\n", path);

        err = IODisplayGetIntegerRangeParameter(service, kNilOptions, key,
                                                &ivalue, &imin, &imax);
        if( kIOReturnSuccess != err)
            continue;

        err = IODisplayGetFloatParameter(service, kNilOptions, key, &value);
        printf("Display %x: %f == 0x%x / [0x%x - 0x%x]\n", 
                    displayIDs[i], value, (int) ivalue, (int) imin, (int) imax);
        if( kIOReturnSuccess != err)
            continue;

        if (argc < 3)
            continue;

        if (strchr(argv[argc - 1], '.'))
        {
            sscanf( argv[argc - 1], "%f", &value );
            err = IODisplaySetFloatParameter(service, kNilOptions, key, value);
            printf("IODisplaySetFloatParameter(0x%x, %f)\n", err, value);
        }
        else
        {
            ivalue = strtol(argv[argc - 1], 0, 0);
            err = IODisplaySetIntegerParameter(service, kNilOptions, key, ivalue);
            printf("IODisplaySetIntegerParameter(0x%x, 0x%x)\n", err, (int) ivalue);
        }
    }
   
    exit(0);
    return(0);
}

