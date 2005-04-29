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
 cc -o wake wake.c -framework IOKit -Wall
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>


int
main(int argc, char **argv)
{
    mach_port_t	master_device_port;
    io_connect_t	fb;
    kern_return_t	kr;
    IOReturn	err;

    kr = IOMasterPort(bootstrap_port,&master_device_port);
    if ( kr == kIOReturnSuccess ) {
        fb = IOPMFindPowerManagement(master_device_port);
        if ( fb != NULL ) {
            err = IOPMWakeSystem ( fb );
            if ( err == kIOReturnSuccess ) {
                 return 1;
            }
        }
    }
    return 0;
}

