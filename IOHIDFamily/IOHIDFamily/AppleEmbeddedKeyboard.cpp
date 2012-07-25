/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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
#include "IOHIDKeyboard.h"
#include "IOLLEvent.h"

#define kFnFunctionUsageMapKey      "FnFunctionUsageMap"
#define kFnKeyboardUsageMapKey      "FnKeyboardUsageMap"
#define kNumLockKeyboardUsageMapKey "NumLockKeyboardUsageMap"
#define kKeyboardUsageMapKey        "KeyboardUsageMap"

#define kDeviceFnFunctionUsageMapKey      "DeviceFnFunctionUsageMap"
#define kDeviceFnKeyboardUsageMapKey      "DeviceFnKeyboardUsageMap"
#define kDeviceNumLockKeyboardUsageMapKey "DeviceNumLockKeyboardUsageMap"
#define kDeviceKeyboardUsageMapKey        "DeviceKeyboardUsageMap"

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleEmbeddedKeyboard, IOHIDEventDriver )

//====================================================================================================
// AppleEmbeddedKeyboard::init
//====================================================================================================
bool AppleEmbeddedKeyboard::init(OSDictionary * properties)
{
    if ( !super::init(properties) )
        return false;
        
    bzero(_secondaryKeys, sizeof(SecondaryKey)*255);
    
    return true;
}

//====================================================================================================
// AppleEmbeddedKeyboard::free
//====================================================================================================
void AppleEmbeddedKeyboard::free()
{
    if ( _keyboardMap )
        _keyboardMap->release();
    
    super::free();
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

    _keyboardMap = OSDynamicCast(OSDictionary, copyProperty(kKeyboardUsageMapKey));
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
    filterKeyboardUsage(&usagePage, &usage, value);

    if ( (( usagePage == kHIDPage_AppleVendorTopCase ) && ( usage == kHIDUsage_AV_TopCase_KeyboardFn )) || 
         (( usagePage == kHIDPage_AppleVendorKeyboard ) && ( usage == kHIDUsage_AppleVendorKeyboard_Function )) )
    {        
        if (_keyboardRollOverElement)
        {
            AbsoluteTime rolloverTS = _keyboardRollOverElement->getTimeStamp();
            
            if ((CMP_ABSOLUTETIME(&rolloverTS, &timeStamp) == 0) && 
                ((_keyboardRollOverElement->getValue() && value && !_fnKeyDownPhysical) || (_fnKeyDownPhysical == value)))
                return;
        }
            
        _fnKeyDownPhysical = (value != 0);
    }
    else if ( usagePage == kHIDPage_KeyboardOrKeypad )
    {
        if (!filterSecondaryFnFunctionUsage(&usagePage, &usage, (value!=0))           
            && !filterSecondaryFnKeyboardUsage(&usagePage, &usage, (value!=0))   
            && filterSecondaryNumLockKeyboardUsage(&usagePage, &usage, (value!=0)))
            return;
    }

    super::dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);

#if !TARGET_OS_EMBEDDED
    _fnKeyDownVirtual = _keyboardNub ? (_keyboardNub->eventFlags() & NX_SECONDARYFNMASK) : _fnKeyDownPhysical;
#else
	_fnKeyDownVirtual = _fnKeyDownPhysical;
#endif
}


/* vtn3: rdar://7045478 removed for readability
#define SHOULD_SWAP_FN_FUNCTION_KEY(key, down)                   \
    ((_secondaryKeys[key].bits & kSecondaryKeyFnFunction) &&	\
    (!( _fnKeyDownPhysical ^ _fKeyMode ) ||	(!down &&		\
    (_secondaryKeys[key].swapping & kSecondaryKeyFnFunction))))

#define SHOULD_SWAP_FN_KEYBOARD_KEY(key, down)                   \
    ((_secondaryKeys[key].bits & kSecondaryKeyFnKeyboard) &&	\
    (_fnKeyDownVirtual || (!down && \
    (_secondaryKeys[key].swapping & kSecondaryKeyFnKeyboard))))
    
#define SHOULD_SWAP_NUM_LOCK_KEY(key, down)			\
    ((_numLockDown || ( !down &&					\
    (_secondaryKeys[key].swapping & kSecondaryKeyNumLockKeyboard))))
 */

//====================================================================================================
// AppleEmbeddedKeyboard::filterSecondaryFnFunctionUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterSecondaryFnFunctionUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down)
{
    // vtn3: rdar://7045478 {
    // This takes the place of SHOULD_SWAP_FN_FUNCTION_KEY
    // Only remap keys that have a remapping
    if (!(_secondaryKeys[*usage].bits & kSecondaryKeyFnFunction))
        return false;
    
    // On a down, we want to remap iff the Fn key is down.
    if (down && ( _fnKeyDownPhysical ^ _fKeyMode ))
        return false;
    
    // On an up we want to remap iff the key was remapped for the down. We do not
    // care if the Fn key is down.
    if (!down && !(_secondaryKeys[*usage].swapping & kSecondaryKeyFnFunction)) 
        return false;
    // end rdar://7045478 }
    
#if !TARGET_OS_EMBEDDED
    if ((*usagePage == kHIDPage_KeyboardOrKeypad) && (*usage == kHIDUsage_KeyboardF5)) {
        UInt32 flags = _keyboardNub ? _keyboardNub->eventFlags() : 0;
        if (flags & (NX_COMMANDMASK | NX_DEVICELCMDKEYMASK | NX_DEVICERCMDKEYMASK)) {
            // <rdar://problem/6305898> Enhancement: Make Cmd+Fn+Key == Cmd+Key
            return false;
        }
    }
#endif
    
    if (down)
        _secondaryKeys[*usage].swapping |= kSecondaryKeyFnFunction;
    else
        _secondaryKeys[*usage].swapping = 0; 
    
    *usagePage  = _secondaryKeys[*usage].fnFunctionUsagePage;
    *usage      = _secondaryKeys[*usage].fnFunctionUsage;

    return true;
}

//====================================================================================================
// AppleEmbeddedKeyboard::filterSecondaryFnKeyboardUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterSecondaryFnKeyboardUsage(
                                UInt32 *                    usagePage,
                                UInt32 *                    usage,
                                bool                        down)
{   
    // vtn3: rdar://7045478 {
    // This takes the place of SHOULD_SWAP_FN_KEYBOARD_KEY
    // Only remap keys that have a remapping
    if (!(_secondaryKeys[*usage].bits & kSecondaryKeyFnKeyboard))
        return false;
    
    // On a down, we want to remap iff the Fn key is down.
    if (down && !_fnKeyDownVirtual)
        return false;
    
    // On an up we want to remap iff the key was remapped for the down. We do not
    // care if the Fn key is down.
    if (!down && !(_secondaryKeys[*usage].swapping & kSecondaryKeyFnKeyboard)) 
        return false;

    // end rdar://7045478 }
    
    if (down)
        _secondaryKeys[*usage].swapping |= kSecondaryKeyFnKeyboard;
    else
        _secondaryKeys[*usage].swapping = 0; 

    *usagePage  = _secondaryKeys[*usage].fnKeyboardUsagePage;
    *usage      = _secondaryKeys[*usage].fnKeyboardUsage;

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
    // vtn3: rdar://7045478 {
    // This takes the place of SHOULD_SWAP_NUM_LOCK_KEY

    // On a down, we want to remap iff the Num Lock key is down.
    if (down && !_numLockDown)
        return false;
    
    // On an up we want to remap iff the key was remapped for the down. We do not
    // care if the Num Lock key is down.
    if (!down && !(_secondaryKeys[*usage].swapping & kSecondaryKeyNumLockKeyboard)) 
        return false;

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

    return false;
}

//====================================================================================================
// AppleEmbeddedKeyboard::filterKeyboardUsage
//====================================================================================================
bool AppleEmbeddedKeyboard::filterKeyboardUsage(UInt32 *                    usagePage,
                                                UInt32 *                    usage,
                                                bool                        down __unused)
{
    char key[32];
    
    bzero(key, sizeof(key));
    snprintf(key, sizeof(key), "0x%04x%04x", (uint16_t)*usagePage, (uint16_t)*usage);
    
    if ( _keyboardMap ) {
        OSNumber * map = OSDynamicCast(OSNumber, _keyboardMap->getObject(key));
        
        if ( map ) {            
            *usagePage  = (map->unsigned32BitValue()>>16) & 0xffff;
            *usage      = map->unsigned32BitValue()&0xffff;
            
        }
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
    
#define DECODE_MAP(type,key,bit)                                                        \
    do {                                                                                \
        OSObject *obj = copyProperty(key);                                              \
        mappingString = OSDynamicCast(OSString,obj);                                    \
        if (mappingString) {                                                            \
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
        }                                                                               \
        if (obj) obj->release();                                                        \
    } while (0)

    if (getProperty(kDeviceNumLockKeyboardUsageMapKey)) {
        DECODE_MAP(numLockKeyboard, kDeviceNumLockKeyboardUsageMapKey, kSecondaryKeyNumLockKeyboard);
    }
    else {
        DECODE_MAP(numLockKeyboard, kNumLockKeyboardUsageMapKey, kSecondaryKeyNumLockKeyboard);
    }
    
    if (getProperty(kDeviceFnKeyboardUsageMapKey)) {
        DECODE_MAP(fnKeyboard, kDeviceFnKeyboardUsageMapKey, kSecondaryKeyFnKeyboard);
    }
    else {
        DECODE_MAP(fnKeyboard, kFnKeyboardUsageMapKey, kSecondaryKeyFnKeyboard);
    }
    
    if (getProperty(kDeviceFnFunctionUsageMapKey)) {
        DECODE_MAP(fnFunction, kDeviceFnFunctionUsageMapKey, kSecondaryKeyFnFunction);
    }
    else {
        DECODE_MAP(fnFunction, kFnFunctionUsageMapKey, kSecondaryKeyFnFunction);
    }

    if ( getProperty(kNumLockKeyboardUsageMapKey) || getProperty(kDeviceNumLockKeyboardUsageMapKey) ) {
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
	
	setProperty(kIOHIDMouseKeysEnablesVirtualNumPadKey, _virtualMouseKeysSupport);
}


//====================================================================================================
// AppleEmbeddedKeyboard::setSystemProperties
//====================================================================================================
IOReturn AppleEmbeddedKeyboard::setSystemProperties( OSDictionary * properties )
{
    OSNumber * number;
    OSString * string;
    bool parseSecondaries = false;
    
    if ((number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDFKeyModeKey))))
    {	
        _fKeyMode = number->unsigned32BitValue();
        setProperty(kIOHIDFKeyModeKey, number);
    }

    if (_virtualMouseKeysSupport && ((number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDMouseKeysOnKey)))))
    {
        _numLockDown = number->unsigned32BitValue();
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kFnFunctionUsageMapKey)))) {
        setProperty(kFnFunctionUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kDeviceFnFunctionUsageMapKey)))) {
        setProperty(kDeviceFnFunctionUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kFnKeyboardUsageMapKey)))) {
        setProperty(kFnKeyboardUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kDeviceFnKeyboardUsageMapKey)))) {
        setProperty(kDeviceFnKeyboardUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kNumLockKeyboardUsageMapKey)))) {
        setProperty(kNumLockKeyboardUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if ((string = OSDynamicCast(OSString, properties->getObject(kDeviceNumLockKeyboardUsageMapKey)))) {
        setProperty(kDeviceNumLockKeyboardUsageMapKey, string);
        parseSecondaries = true;
    }
    
    if (parseSecondaries) {
        parseSecondaryUsages();
    }

    return super::setSystemProperties(properties);
}

//====================================================================================================
