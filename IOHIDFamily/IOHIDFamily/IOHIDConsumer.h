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

#ifndef _IOKIT_HID_IOHIDCONSUMER_H
#define _IOKIT_HID_IOHIDCONSUMER_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

// HID system includes.
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>

// extra includes.
#include <libkern/OSByteOrder.h>

//====================================================================================================
//	IOHIDConsumer
//	Generic driver for usb devices that contain special keys.
//====================================================================================================

class IOHIDConsumer : public IOHIKeyboard
{
    OSDeclareDefaultStructors(IOHIDConsumer)

    bool				_muteIsPressed;
    bool				_ejectIsPressed;
    bool				_powerIsPressed;
    bool				_playIsPressed;        
    bool				_soundUpIsPressed;
    bool				_soundDownIsPressed;
    bool				_fastForwardIsPressed;
    bool				_rewindIsPressed;
    bool				_nextTrackIsPressed;
    bool				_prevTrackIsPressed;
    
    unsigned				_eventFlags;
    bool				_capsLockOn;
    
    // Generic Deskop Element Value Ptrs
    UInt32 *				_systemPowerValuePtr;
    UInt32 *				_systemSleepValuePtr;
    UInt32 *				_systemWakeUpValuePtr;
    
    // Consumer Element Value Ptrs    
    UInt32 *				_powerValuePtr;
    UInt32 *				_resetValuePtr;
    UInt32 *				_sleepValuePtr;
    
    UInt32 *				_playValuePtr;
    UInt32 *				_playOrPauseValuePtr;
    UInt32 *				_playOrSkipPtr;
    UInt32 *				_nextTrackValuePtr;
    UInt32 *				_prevTrackValuePtr;
    UInt32 *				_fastFowardValuePtr;
    UInt32 *				_rewindValuePtr;
    UInt32 *				_stopOrEjectPtr;
    UInt32 *				_ejectValuePtr;

    UInt32 *				_volumeIncValuePtr;
    UInt32 *				_volumeDecValuePtr;
    UInt32 *				_volumeMuteValuePtr;
    
    // Our implementation specific stuff.
    bool				findDesiredElements(OSArray *elements);
    UInt32				FindKeyboardsAndGetModifiers();
    
public:
    // Allocator
    static IOHIDConsumer * 		Consumer(OSArray *elements);
    
    // IOService methods
    virtual bool			init(OSDictionary *properties=0);
    
    virtual void			handleReport();

   
    // IOHIKeyboard methods
    virtual const unsigned char*	defaultKeymapOfLength( UInt32 * length );
    virtual unsigned 			eventFlags();
    virtual bool 			alphaLock();
};
#endif /* !_IOKIT_HID_IOHIDCONSUMER_H */
