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
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include "IOHIDKeyboard.h"

// extra includes.
#include <libkern/OSByteOrder.h>

//====================================================================================================
//	IOHIDConsumer
//	Generic driver for usb devices that contain special keys.
//====================================================================================================

class IOHIDConsumer : public IOHIKeyboard
{
    OSDeclareDefaultStructors(IOHIDConsumer)

    bool				_isDispatcher;
    bool				_states[NX_NUMSPECIALKEYS];
    
    OSNumber *				_vendorID;
    OSNumber *				_productID;
    OSNumber *				_locationID;
    OSString *				_transport;
    
    // Generic Deskop Element Value Ptrs
    UInt32 **				_systemPowerValuePtrs;
    UInt32				_systemPowerValuePtrsCount;
    UInt32 **				_systemSleepValuePtrs;
    UInt32				_systemSleepValuePtrsCount;
    UInt32 **				_systemWakeUpValuePtrs;
    UInt32				_systemWakeUpValuePtrsCount;
    
    // Consumer Element Value Ptrs    
    UInt32 **				_powerValuePtrs;
    UInt32				_powerValuePtrsCount;
    UInt32 **				_resetValuePtrs;
    UInt32				_resetValuePtrsCount;
    UInt32 **				_sleepValuePtrs;
    UInt32				_sleepValuePtrsCount;
    
    UInt32 **				_playValuePtrs;
    UInt32				_playValuePtrsCount;
    UInt32 **				_playOrPauseValuePtrs;
    UInt32				_playOrPauseValuePtrsCount;
    UInt32 **				_playOrSkipPtrs;
    UInt32				_playOrSkipPtrsCount;
    UInt32 **				_nextTrackValuePtrs;
    UInt32				_nextTrackValuePtrsCount;
    UInt32 **				_prevTrackValuePtrs;
    UInt32				_prevTrackValuePtrsCount;
    UInt32 **				_fastFowardValuePtrs;
    UInt32				_fastFowardValuePtrsCount;
    UInt32 **				_rewindValuePtrs;
    UInt32				_rewindValuePtrsCount;
    UInt32 **				_stopOrEjectPtrs;
    UInt32				_stopOrEjectPtrsCount;
    UInt32 **				_ejectValuePtrs;
    UInt32				_ejectValuePtrsCount;

    UInt32 **				_volumeIncValuePtrs;
    UInt32				_volumeIncValuePtrsCount;
    UInt32 **				_volumeDecValuePtrs;
    UInt32				_volumeDecValuePtrsCount;
    UInt32 **				_volumeMuteValuePtrs;
    UInt32				_volumeMuteValuePtrsCount;
    
    IOHIDKeyboard *			_keyboard;
    IONotifier *			_publishNotify;
    IOLock *				_keyboardLock;
    
    UInt32				_otherEventFlags;
    bool				_otherCapsLockOn;
    
    // Our implementation specific stuff.
    bool				findDesiredElements(OSArray *elements);
    UInt32				findKeyboardsAndGetModifiers();
    static bool 			_publishNotificationHandler(void * target, void * ref, IOService * newService );
    
public:
    // Allocator
    static IOHIDConsumer * 		Consumer(OSArray *elements);
    static IOHIDConsumer * 		Dispatcher(IOService * owner);
    
    // IOService methods
    virtual bool			init(OSDictionary *properties=0);
    virtual void			free();
    virtual bool			start(IOService * provider);
    virtual void 			stop(IOService *  provider);
    virtual bool 			matchPropertyTable(OSDictionary * table, SInt32 * score);    
    virtual void			handleReport();
    virtual void			dispatchSpecialKeyEvent(int key, bool down, AbsoluteTime ts);

   
    // IOHIKeyboard methods
    virtual const unsigned char*	defaultKeymapOfLength( UInt32 * length );
    virtual bool 			doesKeyLock(unsigned key);
    virtual unsigned 			eventFlags();
    virtual void 			setNumLock(bool val);
    virtual bool 			numLock();
    virtual bool 			alphaLock();
    virtual UInt32    			deviceType();
    
    inline bool				isDispatcher() const { return _isDispatcher;}
};
#endif /* !_IOKIT_HID_IOHIDCONSUMER_H */
