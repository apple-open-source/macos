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

#ifndef _IOKIT_HID_APPLEEMBEDDEDKEYBOARD_H
#define _IOKIT_HID_APPLEEMBEDDEDKEYBOARD_H

#include <IOKit/hidevent/IOHIDEventDriver.h>

// Moved up to allow subclasses to use the same keys
#define kFnFunctionUsageMapKey      "FnFunctionUsageMap"
#define kFnKeyboardUsageMapKey      "FnKeyboardUsageMap"
#define kNumLockKeyboardUsageMapKey "NumLockKeyboardUsageMap"
#define kKeyboardUsageMapKey        "KeyboardUsageMap"

enum {
    kSecondaryKeyFnFunction         = 0x01,
    kSecondaryKeyFnKeyboard         = 0x02,
    kSecondaryKeyNumLockKeyboard    = 0x04
};

typedef struct _SecondaryKey {
    UInt8	bits;
    UInt8	swapping;
    UInt16	fnFunctionUsagePage;
    UInt16	fnFunctionUsage;
    UInt16	fnKeyboardUsagePage;
    UInt16	fnKeyboardUsage;
    UInt16	numLockKeyboardUsagePage;
    UInt16	numLockKeyboardUsage;
} SecondaryKey;

class AppleEmbeddedKeyboard: public IOHIDEventDriver
{
    OSDeclareDefaultStructors( AppleEmbeddedKeyboard )
    
    bool                    _fnKeyDownPhysical;
    bool                    _fnKeyDownVirtual;
    bool                    _numLockDown;
    bool                    _virtualMouseKeysSupport;
    UInt32                  _fKeyMode;
    SecondaryKey    		_secondaryKeys[255];
    IOHIDElement *          _keyboardRollOverElement;
    OSDictionary *          _keyboardMap;

    void                    findKeyboardRollOverElement(OSArray * reportElements);
        
    void                    parseSecondaryUsages();
    
    bool                    filterSecondaryFnFunctionUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down);
                                
    bool                    filterSecondaryFnKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down);
                                
    bool                    filterSecondaryNumLockKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down);
    
    bool                    filterKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down);

protected:
        
    virtual bool            handleStart( IOService * provider );
    
    virtual void            setElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value );

    virtual void            dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value,
                                IOOptionBits                options = 0 );

public:
    virtual bool            init(OSDictionary * properties = 0);
    virtual void            free();

    virtual IOReturn        setSystemProperties( OSDictionary * properties );

};

#endif /* !_IOKIT_HID_APPLEEMBEDDEDKEYBOARD_H */
