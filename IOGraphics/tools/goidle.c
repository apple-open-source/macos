/*
cc -g -o /tmp/goidle goidle.c -framework CoreFoundation -framework IOKit -Wall -arch ppc -arch i386
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>



int
main(int argc, char **argv)
{
    kern_return_t kr;
    CFTypeRef obj;

    io_registry_entry_t regEntry;

    regEntry = IORegistryEntryFromPath(kIOMasterPortDefault, 
                                    kIOServicePlane ":/IOResources/IODisplayWrangler");

    obj = CFRetain(kCFBooleanTrue);
    if (argc > 1)
    {
        SInt32 num = 1000 * strtol(argv[1], 0, 0);
        if (!num)
            obj = CFRetain(kCFBooleanFalse);
        else
            obj = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &num);
    }

    kr = IORegistryEntrySetCFProperty(regEntry, CFSTR("IORequestIdle"), obj);

    printf("IORegistryEntrySetCFProperty(IORequestIdle) 0x%x\n", kr);

    CFRelease(obj);
    IOObjectRelease(regEntry);

    return (0);
}
