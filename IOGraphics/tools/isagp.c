/*
cc -g -o /tmp/isagp isagp.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char * argv[])
{
    io_service_t        device;
    io_service_t        framebuffer;
    io_service_t        accelerator;
    UInt32              framebufferIndex;
    CGError             err;
    int                 i;
    CGDisplayCount      max;
    CGDirectDisplayID   displayIDs[8];

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;

    for(i = 0; i < max; i++ ) {

        framebuffer = CGDisplayIOServicePort(displayIDs[i]);

        err = IOAccelFindAccelerator(framebuffer, &accelerator, &framebufferIndex);
        if(kIOReturnSuccess != err)
            continue;

        err = IORegistryEntryGetParentEntry(accelerator, kIOServicePlane, &device);
        IOObjectRelease(accelerator);
        if(kIOReturnSuccess != err)
            continue;

        printf("Display ID %p ", displayIDs[i]);
        if(IOObjectConformsTo(device, "IOAGPDevice"))
            printf("is");
        else
            printf("isn't");
        printf(" agp\n");

        IOObjectRelease(device);
    }
    
    exit(0);
    return(0);
}

