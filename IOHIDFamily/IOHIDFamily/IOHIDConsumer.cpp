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

#include <IOKit/IOKitKeys.h>

#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDUsageTables.h>

#include "IOHIDElement.h"
#include "IOHIDConsumer.h"
#include "AppleHIDUsageTables.h"


//====================================================================================================
// Defines
//====================================================================================================

#define super 			IOHIKeyboard

#define DEBUGGING_LEVEL 	0

OSDefineMetaClassAndStructors( IOHIDConsumer, IOHIKeyboard )

//====================================================================================================
// Consumer - constructor
//====================================================================================================
IOHIDConsumer * 
IOHIDConsumer::Consumer(bool isDispatcher) 
{
    IOHIDConsumer *consumer = new IOHIDConsumer;
    
    if ((consumer == 0) || !consumer->init())
    {
        if (consumer) consumer->release();
        return 0;
    }
    
    consumer->_isDispatcher = isDispatcher;
    
    return consumer;
}

//====================================================================================================
// init
//====================================================================================================
bool
IOHIDConsumer::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
  
    _otherEventFlags 	= 0;
    _otherCapsLockOn 	= FALSE;
	
	_repeat				= true;
	setRepeatMode(_repeat);
            
    return true;
}

//====================================================================================================
// start
//====================================================================================================
bool IOHIDConsumer::start(IOService * provider)
{
    setProperty(kIOHIDVirtualHIDevice, kOSBooleanTrue);
                
    return super::start(provider);
}

//====================================================================================================
// stop
//====================================================================================================
void IOHIDConsumer::stop(IOService * provider)
{
    OSSafeReleaseNULL(_keyboardNub);
    super::stop(provider);
}


void IOHIDConsumer::dispatchConsumerEvent(
                                IOHIDKeyboard *             sendingkeyboardNub,
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32						value,
                                IOOptionBits                options)
{
    SInt32  keyCode = -1;
    bool    repeat  = ((options & kHIDDispatchOptionKeyboardNoRepeat) == 0);
    
    if (usagePage == kHIDPage_Consumer)
    {
        switch(usage)
        {
            case kHIDUsage_Csmr_Power:
            case kHIDUsage_Csmr_Reset:
            case kHIDUsage_Csmr_Sleep:
                keyCode = NX_POWER_KEY;
                break;
            case kHIDUsage_Csmr_Play:
            case kHIDUsage_Csmr_PlayOrPause:
            case kHIDUsage_Csmr_PlayOrSkip:
                keyCode = NX_KEYTYPE_PLAY;
                break;
            case kHIDUsage_Csmr_ScanNextTrack:
                keyCode = NX_KEYTYPE_NEXT;
                break;
            case kHIDUsage_Csmr_ScanPreviousTrack:
                keyCode = NX_KEYTYPE_PREVIOUS;
                break;
            case kHIDUsage_Csmr_FastForward:
                keyCode = NX_KEYTYPE_FAST;
                break;
            case kHIDUsage_Csmr_Rewind:
                keyCode = NX_KEYTYPE_REWIND;
                break;
            case kHIDUsage_Csmr_StopOrEject:
            case kHIDUsage_Csmr_Eject:
                keyCode = NX_KEYTYPE_EJECT;
                break;
            case kHIDUsage_Csmr_VolumeIncrement:
                keyCode = NX_KEYTYPE_SOUND_UP;
                break;
            case kHIDUsage_Csmr_VolumeDecrement:
                keyCode = NX_KEYTYPE_SOUND_DOWN;
                break;
            case kHIDUsage_Csmr_Mute:
                keyCode = NX_KEYTYPE_MUTE;
                break;
            case kHIDUsage_Csmr_DisplayBrightnessIncrement:
                keyCode = NX_KEYTYPE_BRIGHTNESS_UP;
                break;
            case kHIDUsage_Csmr_DisplayBrightnessDecrement:
                keyCode = NX_KEYTYPE_BRIGHTNESS_DOWN;
                break;
            default:
                break;
        }
    }
    else if (usagePage == kHIDPage_GenericDesktop)
    {
        switch (usage)
        {
            case kHIDUsage_GD_SystemPowerDown:
            case kHIDUsage_GD_SystemSleep:
            case kHIDUsage_GD_SystemWakeUp:
                keyCode = NX_POWER_KEY;
                break;
        }
    }	
    else if (usagePage == kHIDPage_AppleVendorTopCase)
    {
        switch (usage)
        {
            case kHIDUsage_AV_TopCase_BrightnessUp:
                keyCode = NX_KEYTYPE_BRIGHTNESS_UP;
                break;    
            case kHIDUsage_AV_TopCase_BrightnessDown:
                keyCode = NX_KEYTYPE_BRIGHTNESS_DOWN;
                break;    
            case kHIDUsage_AV_TopCase_VideoMirror:
                keyCode = NX_KEYTYPE_VIDMIRROR;
                break;    
            case kHIDUsage_AV_TopCase_IlluminationDown:
                keyCode = NX_KEYTYPE_ILLUMINATION_DOWN;
                break;    
            case kHIDUsage_AV_TopCase_IlluminationUp:
                keyCode = NX_KEYTYPE_ILLUMINATION_UP;
                break;    
            case kHIDUsage_AV_TopCase_IlluminationToggle:
                keyCode = NX_KEYTYPE_ILLUMINATION_TOGGLE;
                break; 
        }
    }
    else if ((usagePage == kHIDPage_KeyboardOrKeypad) &&
                (usage == kHIDUsage_KeyboardLockingNumLock))
    {
        keyCode = NX_KEYTYPE_NUM_LOCK;
    }


    if (keyCode == -1)
        return;
        
	if (repeat != _repeat)
	{
		_repeat = repeat;
		setRepeatMode(_repeat);
	}
    
    //Copy the device flags (modifier flags) from the ADB keyboard driver
    OSSafeReleaseNULL(_keyboardNub);
    if ( NULL != (_keyboardNub = sendingkeyboardNub) )
    {
        UInt32  currentFlags;
        
        currentFlags        = deviceFlags() & ~_cachedEventFlags;
        _cachedEventFlags   = _keyboardNub->deviceFlags();
        currentFlags       |= _cachedEventFlags;
        _deviceType         = _keyboardNub->deviceType();
                    
        setDeviceFlags(currentFlags);
        _keyboardNub->retain();
    }
    else 
    {
        findKeyboardsAndGetModifiers();
    }
    
    dispatchKeyboardEvent( keyCode, value, timeStamp );
}

//====================================================================================================
// eventFlags - IOHIKeyboard override. This is necessary because we will need to return the state
// of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like all modifiers are off.
//====================================================================================================
unsigned IOHIDConsumer::eventFlags()
{
    unsigned flags = 0;
        
    flags = (_keyboardNub) ? _keyboardNub->eventFlags() : _otherEventFlags;
    
    return( flags );
}

//====================================================================================================
// deviceFlags - IOHIKeyboard override. This is necessary because we will need to return the state
// of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like all modifiers are off.
//====================================================================================================
unsigned IOHIDConsumer::deviceFlags()
{
    unsigned flags = 0;
        
    flags = (_keyboardNub) ? _keyboardNub->deviceFlags() : _otherEventFlags;
    
    return( flags );
}

//====================================================================================================
// setDeviceFlags - IOHIKeyboard override. This is necessary because we will need to return the state
// of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like all modifiers are off.
//====================================================================================================
void IOHIDConsumer::setDeviceFlags(unsigned flags)
{
    if ( _keyboardNub )
        _keyboardNub->setDeviceFlags(flags);
        
    super::setDeviceFlags(flags);
}


//====================================================================================================
// alphaLock  - IOHIKeyboard override. This is necessary because we will need to return the state
// of the caps lock keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like caps lock keys are off.
//====================================================================================================
bool IOHIDConsumer::alphaLock()
{
    bool state = false;
        
    state = (_keyboardNub) ? _keyboardNub->alphaLock() : _otherCapsLockOn;
    
    return( state );
}

//====================================================================================================
// setNumLock  - IOHIKeyboard override. This is necessary because we will need to toggle the num lock
// led on the keyboard interface
//====================================================================================================
void IOHIDConsumer::setNumLock(bool val)
{    
    if (_keyboardNub) _keyboardNub->setNumLock(val);    
}

//====================================================================================================
// numLock  - IOHIKeyboard override. This is necessary because we will need to check the num lock
// status on the keyboard interface
//====================================================================================================
bool IOHIDConsumer::numLock()
{
    bool state = false;
        
    state = (_keyboardNub) ? _keyboardNub->numLock() : super::numLock();
    
    return( state );
}

//====================================================================================================
// doesKeyLock  - IOHIKeyboard override. This is necessary because the system will only toggle the led
// and set appropriate event flags if the num lock key physically locks.
//====================================================================================================
bool IOHIDConsumer:: doesKeyLock ( unsigned key)
{
    if ( key == NX_KEYTYPE_NUM_LOCK )
        return true;
    
    return false;
}

//====================================================================================================
// defaultKeymapOfLength - IOHIKeyboard override
// This allows us to associate the scancodes we choose with the special
// keys we are interested in posting later. This gives us auto-repeats for free. Kewl.
//====================================================================================================
const unsigned char * IOHIDConsumer::defaultKeymapOfLength( UInt32 * length )
{
    static const unsigned char ConsumerKeyMap[] =
    {
		// The first 16 bits are always read first, to determine if the rest of
        // the keymap is in shorts (16 bits) or bytes (8 bits). If the first 16 bits
        // equals 0, data is in bytes; if first 16 bits equal 1, data is in shorts.
        
        0x00,0x00,		// data is in bytes

        // The next value is the number of modifier keys. We have none in our driver.

        0x00,
        
        // The next value is number of key definitions. We have none in our driver.
        
        0x00,
        
        // The next value is number of of sequence definitions there are. We have none.
        
        0x00,
        
        // The next value is the number of special keys. We use these.
        
        NX_NUMSPECIALKEYS,
        
        // Special Key	  	SCANCODE
        //-----------------------------------------------------------        
                
        NX_KEYTYPE_SOUND_UP,		NX_KEYTYPE_SOUND_UP,
        NX_KEYTYPE_SOUND_DOWN,		NX_KEYTYPE_SOUND_DOWN,
        NX_KEYTYPE_BRIGHTNESS_UP,	NX_KEYTYPE_BRIGHTNESS_UP,
        NX_KEYTYPE_BRIGHTNESS_DOWN,	NX_KEYTYPE_BRIGHTNESS_DOWN,
        NX_KEYTYPE_CAPS_LOCK,		NX_KEYTYPE_CAPS_LOCK,
        NX_KEYTYPE_HELP,		NX_KEYTYPE_HELP,
        NX_POWER_KEY,			NX_POWER_KEY,
        NX_KEYTYPE_MUTE,		NX_KEYTYPE_MUTE,
        NX_UP_ARROW_KEY,		NX_UP_ARROW_KEY,
        NX_DOWN_ARROW_KEY,		NX_DOWN_ARROW_KEY,
        NX_KEYTYPE_NUM_LOCK,		NX_KEYTYPE_NUM_LOCK,
        NX_KEYTYPE_CONTRAST_UP,		NX_KEYTYPE_CONTRAST_UP,
        NX_KEYTYPE_CONTRAST_DOWN,	NX_KEYTYPE_CONTRAST_DOWN,
        NX_KEYTYPE_LAUNCH_PANEL,	NX_KEYTYPE_LAUNCH_PANEL,
        NX_KEYTYPE_EJECT,		NX_KEYTYPE_EJECT,
        NX_KEYTYPE_VIDMIRROR,		NX_KEYTYPE_VIDMIRROR,
        NX_KEYTYPE_PLAY,		NX_KEYTYPE_PLAY,
        NX_KEYTYPE_NEXT,		NX_KEYTYPE_NEXT,
        NX_KEYTYPE_PREVIOUS,		NX_KEYTYPE_PREVIOUS,
        NX_KEYTYPE_FAST,		NX_KEYTYPE_FAST,
        NX_KEYTYPE_REWIND,		NX_KEYTYPE_REWIND,
        NX_KEYTYPE_ILLUMINATION_UP,	NX_KEYTYPE_ILLUMINATION_UP,
        NX_KEYTYPE_ILLUMINATION_DOWN,	NX_KEYTYPE_ILLUMINATION_DOWN,
        NX_KEYTYPE_ILLUMINATION_TOGGLE,	NX_KEYTYPE_ILLUMINATION_TOGGLE

    };
    
 
    if( length ) *length = sizeof( ConsumerKeyMap );
    
    return( ConsumerKeyMap );
}

//====================================================================================================
// findKeyboardsAndGetModifiers
//====================================================================================================
UInt32 IOHIDConsumer::findKeyboardsAndGetModifiers()
{
	OSIterator	*iterator = NULL;
	OSDictionary	*matchingDictionary = NULL;
	IOHIKeyboard	*device = NULL;	
	
	_otherEventFlags = 0;
    _cachedEventFlags = 0;
	_otherCapsLockOn = FALSE;
	
	// Get matching dictionary.
	
	matchingDictionary = IOService::serviceMatching( "IOHIKeyboard" );
	if( !matchingDictionary )
	{
		goto exit;
	}
	
	// Get an iterator for the IOHIKeyboard devices.
	
	iterator = IOService::getMatchingServices( matchingDictionary );
	if( !iterator )
	{
		goto exit;
	}
	
	// User iterator to find devices and eject.
	//
	while( (device = (IOHIKeyboard*) iterator->getNextObject()) )
	{		
		
		//Ignore the eventFlags keyboards that don't have modifiers defined
		if (!device->getProperty(kIOHIDKeyboardSupportedModifiersKey))
			continue;
		
		// Save the caps lock state. If more than one keyboard has it down, that's fine -- we
		// just want to know if ANY keyboards have the key down.
		//
		if( device->alphaLock() )
		{
			_otherCapsLockOn = TRUE;
		}

		// OR in the flags, so we get a combined IOHIKeyboard device flags state. That
		// way, if the user is pressing command on one keyboard, shift on another, and
		// then hits an eject key, we'll get both modifiers.
		//
		_otherEventFlags |= device->eventFlags();
	}
	
exit:
	
	if( matchingDictionary ) matchingDictionary->release();
	if( iterator ) iterator->release();
	
	return( _otherEventFlags );
}

