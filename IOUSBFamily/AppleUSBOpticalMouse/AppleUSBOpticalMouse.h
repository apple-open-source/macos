/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_APPLEUSBOPTICALMOUSE_H
#define _IOKIT_APPLEUSBOPTICALMOUSE_H

#include "IOUSBHIDDriver.h"

#define kMouseRetryCount	3

class AppleUSBOpticalMouse : public IOUSBHIDDriver
{
    OSDeclareDefaultStructors(AppleUSBOpticalMouse)

private:
    IONotifier * 		_notifier;

    // IOKit methods
    bool		willTerminate(IOService * provider, IOOptionBits options);
    
    // IOUSBHIDDriver methods
    virtual IOReturn	StartFinalProcessing(void);
    
    // leaf class methods
    static IOReturn 	PowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service,
                                      void *messageArgument, vm_size_t argSize);
};

#endif _IOKIT_APPLEUSBOPTICALMOUSE_H
