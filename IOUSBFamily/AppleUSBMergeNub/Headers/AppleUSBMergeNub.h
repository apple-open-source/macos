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
#ifndef _IOKIT_APPLEUSBMERGENUB_H
#define _IOKIT_APPLEUSBMERGENUB_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>

class AppleUSBMergeNub : public IOService
{
    OSDeclareDefaultStructors(AppleUSBMergeNub)
    
public:
    IOService *	probe(IOService *provider, SInt32 *score);

};
    
#endif _IOKIT_APPLEUSBMERGENUB_H
