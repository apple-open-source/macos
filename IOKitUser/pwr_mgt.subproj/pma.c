/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 cc -framework iokit -o pma pma.c
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>


int
main(int argc, char **argv)
{
    mach_port_t	master_device_port;
    io_connect_t	fb;
    kern_return_t	kr;
    int 		aggressiveness;
    unsigned long	current_value;
    IOReturn	err;

    if ( (argc != 2) || ((aggressiveness = atoi(argv[1])) < 0) ||  (aggressiveness > 1000) ) {
       printf("Type \"pma \" followed by an aggressiveness factor from 0 to 1000\n");
        return 0;
    }

    kr = IOMasterPort(bootstrap_port,&master_device_port);
    if ( kr == kIOReturnSuccess ) {
        fb = IOPMFindPowerManagement(master_device_port);
        if ( fb != NULL ) {
            err = IOPMGetAggressiveness ( fb, kPMSetGeneralAggressiveness, &current_value );
            if ( err == kIOReturnSuccess ) {
                printf("General aggressiveness currently %ld\n",current_value);
            }
            err = IOPMGetAggressiveness ( fb, kPMSetMinutesToDim, &current_value );
            if ( err == kIOReturnSuccess ) {
                printf("Display aggressiveness currently %ld minutes\n",current_value);
            }
            err = IOPMGetAggressiveness ( fb, kPMSetMinutesToSpinDown, &current_value );
            if ( err == kIOReturnSuccess ) {
                printf("Disk aggressiveness currently %ld minutes\n",current_value);
            }
            err = IOPMGetAggressiveness ( fb, kPMSetMinutesToSleep, &current_value );
            if ( err == kIOReturnSuccess ) {
                printf("Sleep aggressiveness currently %ld minutes\n",current_value);
            }
            err = IOPMSetAggressiveness ( fb, kPMSetGeneralAggressiveness, aggressiveness );
            if ( err == kIOReturnSuccess ) {
                printf("Power Management aggressiveness set to %d\n",aggressiveness);
                return 1;
            }
#if 0
            err = IOPMSetAggressiveness ( fb, kPMSetMinutesToDim, 10 );
            if ( err == kIOReturnSuccess ) {
                printf("kPMSetMinutesToDim good\n",aggressiveness);
            }
            err = IOPMSetAggressiveness ( fb, kPMSetMinutesToSpinDown, 11 );
            if ( err == kIOReturnSuccess ) {
                printf("kPMSetMinutesToSpinDown good\n",aggressiveness);
            }
            err = IOPMSetAggressiveness ( fb, kPMSetMinutesToSleep, 12 );
            if ( err == kIOReturnSuccess ) {
                printf("kPMSetMinutesToSleep good\n",aggressiveness);
                return 1;
            }
#endif
        }
    }
    printf("That didn't work for some reason\n");
    return 0;
}
