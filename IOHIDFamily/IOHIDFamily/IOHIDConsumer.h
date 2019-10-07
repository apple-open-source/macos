/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_HID_IOHIDCONSUMER_H
#define _IOKIT_HID_IOHIDCONSUMER_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

// HID system includes.
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include "IOHIKeyboard.h"
#include "IOHIDKeyboard.h"
#include "IOHIDEventService.h"

// extra includes.
#include <libkern/OSByteOrder.h>

//====================================================================================================
//	IOHIDConsumer
//	Generic driver for usb devices that contain special keys.
//====================================================================================================

#if defined(KERNEL) && !defined(KERNEL_PRIVATE)
class __deprecated_msg("Use DriverKit") IOHIDConsumer : public IOHIKeyboard
#else
class IOHIDConsumer : public IOHIKeyboard
#endif
{
    OSDeclareDefaultStructors(IOHIDConsumer)
    
    IOHIDKeyboard *         _keyboardNub;
    
    UInt32                  _otherEventFlags;
    UInt32                  _cachedEventFlags;
    bool                    _otherCapsLockOn;
	
	bool					_repeat;
    
    bool                    _isDispatcher;
    
    // Our implementation specific stuff.
    UInt32                  findKeyboardsAndGetModifiers();
    
public:
    // Allocator
    static IOHIDConsumer * 		Consumer(bool isDispatcher = false);
    
    // IOService methods
    virtual bool			init(OSDictionary *properties=0) APPLE_KEXT_OVERRIDE;
    virtual bool			start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void			stop(IOService * provider) APPLE_KEXT_OVERRIDE;
    
    virtual void            dispatchConsumerEvent(
                                IOHIDKeyboard *             sendingkeyboardNub,
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32						value,
                                IOOptionBits                options = 0);

    inline bool             isDispatcher() { return _isDispatcher;};
   
    // IOHIKeyboard methods
    virtual const unsigned char*	defaultKeymapOfLength( UInt32 * length ) APPLE_KEXT_OVERRIDE;
    virtual bool                    doesKeyLock(unsigned key) APPLE_KEXT_OVERRIDE;
    virtual unsigned                eventFlags(void) APPLE_KEXT_OVERRIDE;
    virtual unsigned                deviceFlags(void) APPLE_KEXT_OVERRIDE;
    virtual void                    setDeviceFlags(unsigned flags) APPLE_KEXT_OVERRIDE;
    virtual void                    setNumLock(bool val) APPLE_KEXT_OVERRIDE;
    virtual bool                    numLock(void) APPLE_KEXT_OVERRIDE;
    virtual bool                    alphaLock(void) APPLE_KEXT_OVERRIDE;
};
#endif /* !_IOKIT_HID_IOHIDCONSUMER_H */
