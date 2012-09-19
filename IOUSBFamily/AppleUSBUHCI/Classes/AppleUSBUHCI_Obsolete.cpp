/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBUHCI.h"


// method in 1.8 and 1.8.1
IOReturn
AppleUSBUHCI::UIMCreateInterruptTransfer(
                                         short				functionNumber,
                                         short				endpointNumber,
                                         IOUSBCompletion                completion,
                                         IOMemoryDescriptor *		CBP,
                                         bool				bufferRounding,
                                         UInt32				bufferSize,
                                         short				direction)
{
#pragma unused (functionNumber, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // deprecated
	USBLog(1, "AppleUSBUHCI::UIMCreateInterruptTransfer - deprecated");
    return kIOReturnBadArgument;
}

// method in 1.8 and 1.8.1
IOReturn
AppleUSBUHCI::UIMCreateBulkTransfer(
                                    short				functionNumber,
                                    short				endpointNumber,
                                    IOUSBCompletion			completion,
                                    IOMemoryDescriptor *		CBP,
                                    bool				bufferRounding,
                                    UInt32				bufferSize,
                                    short				direction)
{
#pragma unused (functionNumber, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // deprecated
	USBLog(1, "AppleUSBUHCI::UIMCreateBulkTransfer - deprecated");
    return kIOReturnBadArgument;
}

IOReturn
AppleUSBUHCI::UIMCreateControlTransfer(
                                       short				functionNumber,
                                       short				endpointNumber,
                                       IOUSBCommand*			command,
                                       void *				CBP,
                                       bool				bufferRounding,
                                       UInt32				bufferSize,
                                       short				direction)
{
#pragma unused (functionNumber, endpointNumber, command, CBP, bufferRounding, bufferSize, direction)
    // deprecated
	USBLog(1, "AppleUSBUHCI::UIMCreateControlTransfer - deprecated");
    return kIOReturnBadArgument;
}

// method in 1.8 and 1.8.1
IOReturn
AppleUSBUHCI::UIMCreateControlTransfer(
                                       short				functionNumber,
                                       short				endpointNumber,
                                       IOUSBCompletion			completion,
                                       IOMemoryDescriptor *		CBP,
                                       bool				bufferRounding,
                                       UInt32				bufferSize,
                                       short				direction)
{
#pragma unused (functionNumber, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // deprecated
	USBLog(1, "AppleUSBUHCI::UIMCreateControlTransfer - deprecated");
    return kIOReturnBadArgument;
}

// method in 1.8 and 1.8.1
IOReturn
AppleUSBUHCI::UIMCreateControlTransfer(
                                       short				functionNumber,
                                       short				endpointNumber,
                                       IOUSBCompletion			completion,
                                       void *				CBP,
                                       bool				bufferRounding,
                                       UInt32				bufferSize,
                                       short				direction)
{
#pragma unused (functionNumber, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // deprecated
    return kIOReturnIPCError;
}



