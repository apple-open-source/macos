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
#include "IOHIDPrivateKeys.h"
#include "IOHIDKeys.h"
#include "IOLLEvent.h"

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleEmbeddedKeyboard, IOHIDEventDriver )

//====================================================================================================
// AppleEmbeddedKeyboard::init
//====================================================================================================
bool AppleEmbeddedKeyboard::init(OSDictionary * properties)
{
    if ( !super::init(properties) )
        return false;
        
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
    setProperty(kIOHIDAppleVendorSupported, kOSBooleanTrue);
    
    if (!super::handleStart(provider))
        return false;
    
    _keyboardMap = OSDynamicCast(OSDictionary, copyProperty(kKeyboardUsageMapKey));
    
    return true;
}


//====================================================================================================
// AppleEmbeddedKeyboard::setElementValue
//====================================================================================================
IOReturn AppleEmbeddedKeyboard::setElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value )
{

    return super::setElementValue(usagePage, usage, value);
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


    super::dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
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
// AppleEmbeddedKeyboard::setSystemProperties
//====================================================================================================
IOReturn AppleEmbeddedKeyboard::setSystemProperties( OSDictionary * properties )
{
    return super::setSystemProperties(properties);
}

//====================================================================================================
