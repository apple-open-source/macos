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

#include <IOKit/IOLib.h>
#include "AppleEmbeddedKeyboard.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDUsageTables.h"

#define kFnFunctionUsageMapKey      "FnFunctionUsageMap"
#define	kFnKeyboardUsageMapKey      "FnKeyboardUsageMap"
#define	kNumLockKeyboardUsageMapKey "NumLockKeyboardUsageMap"

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleEmbeddedKeyboard, super )

//====================================================================================================
// AppleEmbeddedKeyboard::handleStart
//====================================================================================================
bool AppleEmbeddedKeyboard::init(OSDictionary * properties)
{
    if ( !super::init(properties) )
        return false;
        
    bzero(_secondaryKeys, sizeof(SecondaryKey)*255);
    
    return true;
}

//====================================================================================================
// AppleEmbeddedKeyboard::handleStart
//====================================================================================================
bool AppleEmbeddedKeyboard::handleStart( IOService * provider )
{
    if (!super::handleStart(provider))
        return false;

    // RY: The fn key is reported in the 8th byte of the keyboard report.
    // This byte is part of the normal keyboard boot protocol report.
    // Unfortunately, because of this, a keyboard roll over will trick 
    // the driver into thinking that the fn key is down.
    findKeyboardRollOverElement(getReportElements());
    
    parseSecondaryUsages();
    
    setProperty(kIOHIDFKeyModeKey, _fKeyMode, sizeof(_fKeyMode));
        
    return true;
}


//====================================================================================================
// AppleEmbeddedKeyboard::setElementValue
//====================================================================================================
void AppleEmbeddedKeyboard::setElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value )
{
    if ((usagePage == kHIDPage_LEDs) && (usage == kHIDUsage_LED_NumLock))
        _numLockDown = (value != 0);

    super::setElementValue(usagePage, usage, value);
}

//====================================================================================================
// AppleEmbeddedKeyboard::dispatchKeyboardEvent
//====================================================================================================
void AppleEmbeddedKeyboard::dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value,
                                IOOptionBits                options)
{
    if ( (( usagePage == kHIDPage_AppleVendorTopCase ) && ( usage == kHIDUsage_AppleVendor_KeyboardFn )) || 
         (( usagePage == kHIDPage_AppleVendorKeyboard ) && ( usage == kHIDUsage_AppleVendorKeyboard_Function )) )
    {        
        if (_keyboardRollOverElement 
            && (CMP_ABSOLUTETIME(&(_keyboardRollOverElement->getTimeStamp()), &timeStamp) == 0)
            && ((_keyboardRollOverElement->getValue() && value && !_fnKeyDown) || (_fnKeyDown == value)))
            return;
            
        _fnKeyDown = (value != 0);
    }
    else if ( usagePage == kHIDPage_KeyboardOrKeypad )
    {
        if (!filterSecondaryFnFunctionUsage(&usagePage, &usage, (value!=0)))            
            if (!filterSecondaryFnKeyboardUsage(&usagePage, &usage, (value!=0)))   
                if (filterSecondaryNumLockKeyboardUsage(&usagePage, &usage, (value!=0)))
                    return;
    }

    super::dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
}


#define SHOULD_SWAP_FN_FUNCTION_KEY(key, down)                   \
    ((_secondaryKeys[key].bits & kSecondaryKeyFnFunction) &&	\
    (!( _fnKeyDown ^ _fKeyMode ) ||	(!down &&		\
    (_secondaryKeys[key].swapping & kSecondaryKeyFnFunction))))

#define SHOULD_SWAP_FN_KEYBOARD_KEY(key, down)                   \
    ((_secondaryKeys[key].bits & kSecondaryKeyFnKeyboard) &&	\
    (( _fnKeyDown ^ 					\
    (_fKeyMode && _stickyKeysOn) ) || (!down && \
    (_secondaryKeys[key].swapping & kSecondaryKeyFnKeyboard))))
    
#define SHOULD_SWAP_NUM_LOCK_KEY(key, down)			\
    ((_numLockDown || ( !down &&					\
    (_secondaryKeys[key].swapping & kSecondaryKeyNumLockKeyboard))))

//====================================================================================================
// AppleEmbeddedKeyboard::filterSecondaryFnFunctionUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterSecondaryFnFunctionUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down)
{
    if (SHOULD_SWAP_FN_FUNCTION_KEY(*usage, down))
    {
        if (down)
            _secondaryKeys[*usage].swapping |= kSecondaryKeyFnFunction;
        else
            _secondaryKeys[*usage].swapping = 0; 

        *usagePage  = _secondaryKeys[*usage].fnFunctionUsagePage;
        *usage      = _secondaryKeys[*usage].fnFunctionUsage;

        return true;
    }
    
    return false;
}

//====================================================================================================
// AppleEmbeddedKeyboard::filterSecondaryFnKeyboardUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterSecondaryFnKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down)
{   

    if (SHOULD_SWAP_FN_KEYBOARD_KEY(*usage, down))
    {
        if (down)
            _secondaryKeys[*usage].swapping |= kSecondaryKeyFnKeyboard;
        else
            _secondaryKeys[*usage].swapping = 0; 

        *usagePage  = _secondaryKeys[*usage].fnKeyboardUsagePage;
        *usage      = _secondaryKeys[*usage].fnKeyboardUsage;
    }
    
    return false;
}

//====================================================================================================
// AppleEmbeddedKeyboard::filterSecondaryNumLockKeyboardUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterSecondaryNumLockKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down)
{

    if (SHOULD_SWAP_NUM_LOCK_KEY(*usage, down))
    {
        // If the key is not a swapped numpad key, consume it
        if (_secondaryKeys[*usage].bits & kSecondaryKeyNumLockKeyboard)
        {
            if (down)
                _secondaryKeys[*usage].swapping |= kSecondaryKeyNumLockKeyboard;
            else
                _secondaryKeys[*usage].swapping = 0; 

            *usagePage  = _secondaryKeys[*usage].numLockKeyboardUsagePage;
            *usage      = _secondaryKeys[*usage].numLockKeyboardUsage;
        }
        else if ( down )
            return true;
    }

    return false;
}

//====================================================================================================
// AppleEmbeddedKeyboard::findKeyboardRollOverElement
//====================================================================================================
void AppleEmbeddedKeyboard::findKeyboardRollOverElement(OSArray * reportElements)
{
    IOHIDElement *  element;
    UInt32          count;
    
    if (!reportElements)
        return;
                
    count = reportElements->getCount();
    
    for (UInt32 i=0; i<count; i++)
    {
        element = (IOHIDElement *) reportElements->getObject(i);
        
        if (element && (element->getUsagePage() == kHIDPage_KeyboardOrKeypad ) && 
            (element->getUsage() == kHIDUsage_KeyboardErrorRollOver))
        {
            _keyboardRollOverElement = element;
            return;
        }
    }
}

//====================================================================================================
// AppleEmbeddedKeyboard::parseSecondaryUsages
//====================================================================================================
void AppleEmbeddedKeyboard::parseSecondaryUsages()
{
    OSString *      mappingString;
    char *          str;
    UInt32          index, value;

#define DECODE_MAP(type,key,bit)                                                    \
    do {                                                                            \
        mappingString = OSDynamicCast(OSString,getProperty(key));                   \
        if (!mappingString) break;                                                  \
        str = (char *)mappingString->getCStringNoCopy();                            \
        while ( str && (*str != '\0')) {                                            \
            index = strtoul(str, &str, 16) & 0xff;                                  \
            while ((*str!='\0')&&((*str < '0')||(*str > '9'))) { str ++; }          \
            value = strtoul(str, &str, 16);                                         \
            while ((*str!='\0')&&((*str < '0')||(*str > '9'))) { str ++; }          \
            _secondaryKeys[index].type##UsagePage   = (value >> 16) & 0xffff;       \
            _secondaryKeys[index].type##Usage       = value & 0xffff;               \
            _secondaryKeys[index].bits             |= bit;                          \
        }                                                                           \
    } while (0)

    DECODE_MAP(numLockKeyboard, kNumLockKeyboardUsageMapKey, kSecondaryKeyNumLockKeyboard);
    DECODE_MAP(fnKeyboard, kFnKeyboardUsageMapKey, kSecondaryKeyFnKeyboard);
    DECODE_MAP(fnFunction, kFnFunctionUsageMapKey, kSecondaryKeyFnFunction);
    
    if ( getProperty(kNumLockKeyboardUsageMapKey) ) {
        _virtualMouseKeysSupport = TRUE;
        for (index=0; index<255; index++) {
            if ( ( _secondaryKeys[index].bits & kSecondaryKeyFnFunction ) && 
                ( _secondaryKeys[index].fnFunctionUsagePage == kHIDPage_KeyboardOrKeypad ) &&
                ( _secondaryKeys[index].fnFunctionUsage == kHIDUsage_KeyboardLockingNumLock ) ) {
                
                _virtualMouseKeysSupport = FALSE;
                break;
            }
        }
    } else {
        _virtualMouseKeysSupport = FALSE;
    }
}


//====================================================================================================
// AppleEmbeddedKeyboard::setSystemProperties
//====================================================================================================
IOReturn AppleEmbeddedKeyboard::setSystemProperties( OSDictionary * properties )
{
    OSNumber * number;
    
    if ((number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDFKeyModeKey))))
    {	
        _fKeyMode = number->unsigned32BitValue();
        setProperty(kIOHIDFKeyModeKey, number);
    }

    if ((number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDStickyKeysOnKey))))
    {	
        _stickyKeysOn = number->unsigned32BitValue();
    }
    
    if (_virtualMouseKeysSupport && (number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDMouseKeysOnKey))))
    {
        _numLockDown = number->unsigned32BitValue();
    }

    return super::setSystemProperties(properties);
}
