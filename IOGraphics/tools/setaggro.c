/*
cc -g -o /tmp/setaggro setaggro.c -framework ApplicationServices -framework IOKit -Wall
*/

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef sub_iokit_graphics
#define sub_iokit_graphics           err_sub(5)
#endif

#ifndef kIOFBLowPowerAggressiveness
#define kIOFBLowPowerAggressiveness	iokit_family_err(sub_iokit_graphics, 1)
#endif

int main(int argc, char * argv[])
{
    kern_return_t err;
    io_connect_t  connect;
    unsigned long value;

    if (argc < 2)
    {
	fprintf(stderr, "%s value\n", argv[0]);
	return (1);
    }

    connect = IOPMFindPowerManagement(kIOMasterPortDefault);
    if (!connect) 
    {
	fprintf(stderr, "IOPMFindPowerManagement(%x)\n", err);
	return (1);
    }

    value = strtol(argv[1], 0, 0);
    err = IOPMSetAggressiveness( connect, kIOFBLowPowerAggressiveness, value );
    fprintf(stderr, "IOPMSetAggressiveness(kIOFBLowPowerAggressiveness, %lx) result %x\n", value, err);
    
    IOServiceClose(connect);

    exit (0);
    return (0);
}

