/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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
 * HISTORY
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IONetworkLib.h>

#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>     // mig generated

//---------------------------------------------------------------------------
// IONetworkOpen - Open a connection to an IONetworkInterface object.
//                 An IONetworkUserClient object is created to manage
//                 the connection.

IOReturn IONetworkOpen(io_object_t obj, io_connect_t * con)
{
    return IOServiceOpen(obj, mach_task_self(), kIONUCType, con);
}

//---------------------------------------------------------------------------
// IONetworkClose - Close the connection to an IONetworkInterface object.

IOReturn IONetworkClose(io_connect_t con)
{    
    return IOServiceClose(con);
}

//---------------------------------------------------------------------------
// IONetworkWriteData - Write to the buffer of a network data object.

IOReturn IONetworkWriteData(io_connect_t  con,
                            IONDHandle    dataHandle,
                            UInt8 *       srcBuf,
                            UInt32        inSize)
{
    IOReturn  kr;

    if (!srcBuf || !inSize)
        return kIOReturnBadArgument;

    kr = io_connect_method_scalarI_structureI(
                con,
                kIONUCWriteNetworkDataIndex,         /* method index */
                (int *) &dataHandle,                 /* input[] */
                1,                                   /* inputCount */
                (char *) srcBuf,                     /* inputStruct */
                inSize);                             /* inputStructCount */

    return kr;
}

//---------------------------------------------------------------------------
// IONetworkReadData - Read the buffer of a network data object.

IOReturn IONetworkReadData(io_connect_t con,
                           IONDHandle   dataHandle,
                           UInt8 *      destBuf,
                           UInt32 *     inOutSizeP)
{
    IOReturn               kr;

    if (!destBuf || !inOutSizeP)
        return kIOReturnBadArgument;

    kr = io_connect_method_scalarI_structureO(
                con,
                kIONUCReadNetworkDataIndex,          /* method index */
                (int *) &dataHandle,                 /* input[] */
                1,                                   /* inputCount */
                (char *) destBuf,                    /* output */
                (int *) inOutSizeP);                 /* outputCount */

    return kr;
}

//---------------------------------------------------------------------------
// IONetworkResetData - Fill the buffer of a network data object with zeroes.

IOReturn IONetworkResetData(io_connect_t con, IONDHandle dataHandle)
{
    IOReturn               kr;
    mach_msg_type_number_t zero = 0;

    kr = io_connect_method_scalarI_scalarO(
                con,
                kIONUCResetNetworkDataIndex,         /* method index */
                (int *) &dataHandle,                 /* input[] */
                1,                                   /* inputCount */
                0,                                   /* output */
                &zero);                              /* outputCount */

    return kr;
}

//---------------------------------------------------------------------------
// IONetworkGetDataCapacity - Get the capacity (in bytes) of a network data 
//                            object.

IOReturn IONetworkGetDataCapacity(io_connect_t con,
                                  IONDHandle   dataHandle,
                                  UInt32 *     capacityP)
{
    mach_msg_type_number_t one = 1;

    if (!capacityP)
        return kIOReturnBadArgument;

    return io_connect_method_scalarI_scalarO(
                con,
                kIONUCGetNetworkDataCapacityIndex,   /* method index */
                (int *) &dataHandle,                 /* input[] */
                1,                                   /* inputCount */
                (int *) capacityP,                   /* output */
                &one);                               /* outputCount */
}

//---------------------------------------------------------------------------
// IONetworkGetDataHandle - Get the handle of a network data object with
//                          the given name.

IOReturn IONetworkGetDataHandle(io_connect_t con,
                                const char * dataName,
                                IONDHandle * dataHandleP)
{
    UInt32                  nameSize;
    mach_msg_type_number_t  outCount = sizeof(*dataHandleP);

    if (!dataName || !dataHandleP)
        return kIOReturnBadArgument;

    nameSize = strlen(dataName) + 1;

    return io_connect_method_structureI_structureO(
                con,
                kIONUCGetNetworkDataHandleIndex,     /* method index */
                (char *) dataName,                   /* input[] */
                nameSize,                            /* inputCount */
                (char *) dataHandleP,                /* output */
                &outCount);                          /* outputCount */
}
