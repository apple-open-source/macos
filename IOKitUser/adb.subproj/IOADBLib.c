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

#include <Kernel/IOKit/adb/IOADBLib.h>


//*************************************************************************************************************
// IOPMFindADBController
//
//*************************************************************************************************************
io_connect_t IOPMFindADBController( mach_port_t master_device_port )
{
io_connect_t	fb;
kern_return_t	kr;
io_iterator_t	enumer;
io_connect_t	obj = NULL;

kr = IORegistryCreateIterator( master_device_port, kIOServicePlane, TRUE, &enumer);

if ( kIOReturnSuccess == kr ) {
    while( (obj = IOIteratorNext( enumer ))) {
        if( IOObjectConformsTo( obj, "IOADBController" )) {
            break;
        }
        IOObjectRelease( obj );
    }
}
kr = IORegistryDisposeEnumerator(enumer);

if( obj ) {
    kr = IOServiceOpen( obj,mach_task_self(), 0, &fb);
    if ( kr == kIOReturnSuccess ) {
        return fb;
    }
}
return 0;
}


//*************************************************************************************************************
// IOPMClaimADBDevice
//
//*************************************************************************************************************
IOReturn IOPMClaimADBDevice ( io_connect_t fb, unsigned long ADBaddress )
{
    IOByteCount	len = 0;

    if ( (ADBaddress > 15) || (ADBaddress == 0) ) {
        return kIOReturnBadArgument;
    }
    return (io_connect_method_scalarI_scalarO( fb, kADBClaimDevice, &ADBaddress, 1, NULL, &len));
}


//*************************************************************************************************************
// IOPMReleaseADBDevice
//
//*************************************************************************************************************
IOReturn IOPMReleaseADBDevice ( io_connect_t fb, unsigned long ADBaddress )
{
    IOByteCount	len = 0;

    if ( (ADBaddress > 15) || (ADBaddress == 0) ) {
        return kIOReturnBadArgument;
    }

    return (io_connect_method_scalarI_scalarO( fb, kADBReleaseDevice, &ADBaddress, 1, NULL, &len));
}


//*************************************************************************************************************
// IOPMReadADBDevice
//
//*************************************************************************************************************
IOReturn IOPMReadADBDevice ( io_connect_t fb, unsigned long ADBaddress, unsigned long ADBregister,
                             			unsigned char * buffer, unsigned long * length )
{
    void * input_params[2];

    if ( (ADBaddress > 15) || (ADBaddress == 0) ||  (ADBregister > 3) ) {
        return kIOReturnBadArgument;
    }

    input_params[0] = (void *)ADBaddress;
    input_params[1] = (void *)ADBregister;

    *length = 8;
    return (io_connect_method_scalarI_structureO( fb, kADBReadDevice, input_params, 2, buffer, length));
}


//*************************************************************************************************************
// IOPMWriteADBDevice
//
//*************************************************************************************************************
IOReturn IOPMWriteADBDevice ( io_connect_t fb, unsigned long ADBaddress, unsigned long ADBregister,
                              			unsigned char * buffer, unsigned long length)
{
    IOByteCount	len = 0;
    void *		input_params[4];

    if ( (ADBaddress > 15) || (ADBaddress == 0) ||  (ADBregister > 3) ) {
        return kIOReturnBadArgument;
    }

    input_params[0] = (void *)ADBaddress;
    input_params[1] = (void *)ADBregister;
    input_params[2] = (void *)buffer;
    input_params[3] = (void *)&length;

    return (io_connect_method_scalarI_scalarO( fb, kADBWriteDevice, input_params, 4, NULL, &len));
}
