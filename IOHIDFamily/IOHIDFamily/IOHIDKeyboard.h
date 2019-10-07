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

#ifndef _IOKIT_HID_IOHIDKEYBOARD_H
#define _IOKIT_HID_IOHIDKEYBOARD_H

#include <IOKit/hidsystem/IOHIDTypes.h>
#include "IOHIKeyboard.h"
#include "IOHIDDevice.h"
#include "IOHIDConsumer.h"
#include "IOHIDElement.h"
#include "IOHIDEventService.h"
#include "IOHIDFamilyPrivate.h"

enum {
    kUSB_CAPSLOCKLED_SET = 2,
    kUSB_NUMLOCKLED_SET = 1
};

#define ADB_CONVERTER_LEN       0xff + 1   //length of array def_usb_2_adb_keymap[]
#define APPLE_ADB_CONVERTER_LEN 0xff + 1   //length of array def_usb_apple_2_adb_keymap[]

#if defined(KERNEL) && !defined(KERNEL_PRIVATE)
class __deprecated_msg("Use DriverKit") IOHIDKeyboard : public IOHIKeyboard
#else
class IOHIDKeyboard : public IOHIKeyboard
#endif
{
    OSDeclareDefaultStructors(IOHIDKeyboard)

    IOHIDEventService *     _provider;
    	
	bool					_repeat;
	bool					_resyncLED;
        
    // LED Specific Members
    UInt8                   _ledState;
    thread_call_t           _asyncLEDThread;

    // Scan Code Array Specific Members
    unsigned int            _usb_2_adb_keymap[ADB_CONVERTER_LEN + 1];
    unsigned int            _usb_apple_2_adb_keymap[APPLE_ADB_CONVERTER_LEN + 1];
    
    // FN Key Member
    bool                    _containsFKey;
    bool                    _isDispatcher;
    
    // *** PRIVATE HELPER METHODS ***
    void                    Set_LED_States(UInt8);
    UInt32                  handlerID();

    // *** END PRIVATE HELPER METHODS ***
    
    // static methods for callbacks, the command gate, new threads, etc.
    static void             _asyncLED (OSObject *target);
                                
public:    
    // Allocator
    static IOHIDKeyboard * 	Keyboard(UInt32 supportedModifiers, bool isDispatcher = false);
    
    // IOService methods
    virtual bool            init(OSDictionary * properties = 0) APPLE_KEXT_OVERRIDE;
    virtual bool            start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void            stop(IOService *  provider) APPLE_KEXT_OVERRIDE;
    virtual void            free(void) APPLE_KEXT_OVERRIDE;

    inline bool             isDispatcher() { return _isDispatcher;};

    // IOHIDevice methods
    UInt32                  interfaceID(void) APPLE_KEXT_OVERRIDE;
    UInt32                  deviceType(void) APPLE_KEXT_OVERRIDE;

    // IOHIKeyboard methods
    const unsigned char * 	defaultKeymapOfLength(UInt32 * length) APPLE_KEXT_OVERRIDE;
    void                    setAlphaLockFeedback(bool LED_state) APPLE_KEXT_OVERRIDE;
    void                    setNumLockFeedback(bool LED_state) APPLE_KEXT_OVERRIDE;
    unsigned                getLEDStatus(void) APPLE_KEXT_OVERRIDE;
    IOReturn                setParamProperties( OSDictionary * dict ) APPLE_KEXT_OVERRIDE;

	void                    dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                bool                        keyDown,
                                IOOptionBits                options = 0);

};


#endif /* !_IOKIT_HID_IOHIDKEYBOARD_H */
