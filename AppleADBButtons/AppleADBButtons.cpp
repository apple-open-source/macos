/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#include "AppleADBButtons.h"
#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AppleADBButtons,IOHIKeyboard)

bool displayWranglerFound( OSObject *, void *, IOService * );
void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data );
void asyncFunc ( void * );
static void check_eject_held(thread_call_param_t arg);


// **********************************************************************************
// start
//
// **********************************************************************************
bool AppleADBButtons::start ( IOService * theNub )
{
    int i;
    
    for ( i = 0; i < kMax_registrations; i++ ) {
        keycodes[i] = kNullKey;
        downHandlers[i] = NULL;
    }

    adbDevice = (IOADBDevice *)theNub;

    register_for_button = OSSymbol::withCString("register_for_button");

    if( !super::start(theNub))
        return false;

    if( !adbDevice->seizeForClient(this, button_data) ) {
        IOLog("%s: Seize failed\n", getName());
        return false;
    }

    addNotification( gIOPublishNotification,serviceMatching("IODisplayWrangler"),	// look for the display wrangler
                     (IOServiceNotificationHandler)displayWranglerFound, this, 0 );
    _initial_handler_id = adbDevice->handlerID();
    _eject_released = true;  //initially assume eject button is released
    _peject_timer = thread_call_allocate((thread_call_func_t)check_eject_held, (thread_call_param_t)this);
    
    return true;
}

IOReturn AppleADBButtons::callPlatformFunction(const OSSymbol *functionName,
                                                            bool waitForFunction,
                                                            void *param1, void *param2,
                                                            void *param3, void *param4)
{  
    if (functionName == register_for_button)
    {
        registerForButton((unsigned int)param1, (IOService *)param2,
	    (button_handler)param3, (bool)param4);
        return kIOReturnSuccess;
    }
    return kIOReturnBadArgument;

}


UInt64 AppleADBButtons::getGUID()
{
  return(kAppleOnboardGUID);
}

// **********************************************************************************
// displayWranglerFound
//
// The Display Wrangler has appeared.  We will be calling its
// ActivityTickle method when there is user activity.
// **********************************************************************************
bool displayWranglerFound( OSObject * us, void * ref, IOService * yourDevice )
{
 if ( yourDevice != NULL ) {
     ((AppleADBButtons *)us)->displayManager = yourDevice;
 }
 return true;
}

UInt32 AppleADBButtons::interfaceID()
{
    return NX_EVS_DEVICE_INTERFACE_ADB;
}

UInt32 AppleADBButtons::deviceType()
{
    OSNumber 	*xml_handlerID;

    //Info.plist settings always have highest priority
    xml_handlerID = OSDynamicCast( OSNumber, getProperty("alt_handler_id"));
    if (xml_handlerID)
    {
	return xml_handlerID->unsigned32BitValue();
    }

    //If initial id is 31 then this is a post-WallStreet PowerBook, so 
    // pretend it is on an ANSI keyboard.  If the real keyboard is JIS or
    // ISO then any keypresses from there will load the correct system
    // resources automatically.  Returning a nonexistent keyboard id
    // prevents language resources from loading properly.
    if (_initial_handler_id == 31)
    {
	return 195; //Gestalt.h domestic (ANSI) Powerbook keyboard
    }
    else
    return adbDevice->handlerID();
}

// **********************************************************************************
// registerForButton
//
// Clients call here, specifying a button and a routine to call when that
// button is pressed or released.
// **********************************************************************************
IOReturn AppleADBButtons::registerForButton ( unsigned int keycode, IOService * registrant, button_handler handler, bool down )
{
    int i;

    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == kNullKey ) {
            if ( down ) {
                registrants[i] = registrant;
                downHandlers[i] = handler;
                keycodes[i] = keycode;
                break;
            }
        }
    }
    return kIOReturnSuccess;
}

// **********************************************************************************
// button_data
//
// **********************************************************************************
void button_data ( IOService * us, UInt8 adbCommand, IOByteCount length, UInt8 * data )
{
((AppleADBButtons *)us)->packet(data,length,adbCommand);
}


// **********************************************************************************
// packet
//
// **********************************************************************************
IOReturn AppleADBButtons::packet (UInt8 * data, IOByteCount, UInt8 adbCommand )
{
    unsigned int	keycode;
    bool		down;

    keycode = *data;
    down = ((keycode & 0x80) == 0);
    keycode &= 0x7f;
    dispatchButtonEvent(keycode,down);
    
    keycode = *(data + 1);
    if( keycode != 0xff ) {
        down = ((keycode & 0x80) == 0);
        keycode &= 0x7f;
        dispatchButtonEvent(keycode,down);
    }
    
    if ( displayManager != NULL ) {			// if there is a display manager, tell
        displayManager->activityTickle(kIOPMSuperclassPolicy1);	// it there is user activity
    }
    
    return kIOReturnSuccess;
}


// **********************************************************************************
// dispatchButtonEvent
//
// Look for any registered handlers for this button and notify them.
// **********************************************************************************
void AppleADBButtons::dispatchButtonEvent (unsigned int keycode, bool down )
{
    int i;
    AbsoluteTime now;

    if (_initial_handler_id == 0xc0)  //For Apple ADB AV and ColorSync monitors
    {
	switch (keycode)
	{
	    case kVolume_up_AV:
		keycode = kVolume_up;
		break;
	    case kVolume_down_AV:
		keycode = kVolume_down;
		break;
	    case kMute_AV:
		keycode = kMute;
		break;
	    default:
		//No other volume codes are available for OS X
		break;
	}
    }

    clock_get_uptime(&now);
    
    for ( i = 0; i < kMax_registrations; i++ ) {
        if ( keycodes[i] == keycode ) {
            if ( down ) {
                if (downHandlers[i] != NULL ) {
                    thread_call_func((thread_call_func_t)downHandlers[i],
                                     (thread_call_param_t)registrants[i],
                                     true);
                }
            }
        }
    }
    
    //Copy the device flags (modifier flags) from the ADB keyboard driver
    if (_initial_handler_id == 31)
    {
	mach_timespec_t     	t;
	unsigned 		adb_keyboard_flags;
	
	t.tv_sec = 1; //Wait for keyboard driver for up to 1 second
	t.tv_nsec = 0;
	_pADBKeyboard = waitForService(serviceMatching("AppleADBKeyboard"), &t);
	if (_pADBKeyboard)
	{
	    const OSSymbol 	*get_device_flags;
	    
	    get_device_flags = OSSymbol::withCString("get_device_flags");
	    _pADBKeyboard->callPlatformFunction(get_device_flags, false, 
		(void *)&adb_keyboard_flags, 0, 0, 0);
	    super::setDeviceFlags(adb_keyboard_flags);
	}
    }

    //Only dispatch keycodes that this driver understands.  
    //     See appleADBButtonsKeyMap[] for the list.
    switch (keycode)
    {
	case kVolume_up:
	case kVolume_down:
	case kMute:
	case kBrightness_up:
	case kBrightness_down:
	case kNum_lock_on_laptops:
	case kVideoMirror:
	case kIllumination_toggle:
	case kIllumination_down:
	case kIllumination_up:
	    dispatchKeyboardEvent(keycode, down, now);
	    break;
	case kEject:
	    if (down)
	    {
		AbsoluteTime 	deadline;
		OSNumber 	*plist_time;

		_eject_released = false;
		_eject_delay = 250;	//.25 second default
		plist_time = OSDynamicCast( OSNumber, getProperty("Eject Delay Milliseconds"));
		if (plist_time)
		{
		    _eject_delay = plist_time->unsigned32BitValue();
		}
		clock_interval_to_deadline(_eject_delay, kMillisecondScale, &deadline);
		thread_call_enter_delayed(_peject_timer, deadline);
	    }
	    else
	    {
		_eject_released = true;
		thread_call_cancel(_peject_timer);
		dispatchKeyboardEvent(kEject, FALSE, now);
	    }
	default:  //Don't dispatch anything else
	    break;
    }
}

static void check_eject_held(thread_call_param_t us)
{
   ((AppleADBButtons *)us)->_check_eject_held();
}

void AppleADBButtons::_check_eject_held( void ) 
{
    AbsoluteTime now;
    
    if (!_eject_released)
    {
	clock_get_uptime(&now);
	dispatchKeyboardEvent(kEject, TRUE, now);
    }
}


const unsigned char *AppleADBButtons::defaultKeymapOfLength(UInt32 *length)
{
    static const unsigned char appleADBButtonsKeyMap[] = {
        0x00, 0x00,	// chars
        0x00,		// no modifier keys
        0x00,		// no defs
        0x00,		// no seqs
        0x0B,		// 11 special keys
        NX_KEYTYPE_SOUND_UP, kVolume_up,
        NX_KEYTYPE_SOUND_DOWN, kVolume_down,
        NX_KEYTYPE_MUTE, kMute,
        NX_KEYTYPE_BRIGHTNESS_UP, kBrightness_up,
        NX_KEYTYPE_BRIGHTNESS_DOWN, kBrightness_down,
        NX_KEYTYPE_NUM_LOCK, kNum_lock_on_laptops,
        NX_KEYTYPE_EJECT, kEject,
        NX_KEYTYPE_VIDMIRROR, kVideoMirror,
        NX_KEYTYPE_ILLUMINATION_TOGGLE, kIllumination_toggle,
        NX_KEYTYPE_ILLUMINATION_DOWN, kIllumination_down,
        NX_KEYTYPE_ILLUMINATION_UP, kIllumination_up,
    };
    
    *length = sizeof(appleADBButtonsKeyMap);
    
    return appleADBButtonsKeyMap;
}

IOReturn AppleADBButtons::setParamProperties(OSDictionary *dict)
{
    dict->removeObject(kIOHIDKeyMappingKey);

    return super::setParamProperties(dict);
}
