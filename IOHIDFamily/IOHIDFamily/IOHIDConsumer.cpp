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

#include <IOKit/hidsystem/ev_keymap.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDUsageTables.h>

#include "IOHIDElement.h"
#include "IOHIDConsumer.h"


//====================================================================================================
// Defines
//====================================================================================================

#define super 			IOHIKeyboard

#define DEBUGGING_LEVEL 	0

// Defining scan codes for our use because usage values > NX_NUMKEYCODES
#define kPower			0x01

#define kVolumeUp		0x02
#define kVolumeDown		0x03
#define kVolumeMute		0x04

#define kPlay			0x05
#define kFastForward		0x06
#define kRewind			0x07
#define kNextTrack		0x08
#define kPreviousTrack		0x09
#define kEject			0x0a

OSDefineMetaClassAndStructors( IOHIDConsumer, IOHIKeyboard )

//====================================================================================================
// Consumer - constructor
//====================================================================================================
IOHIDConsumer * 
IOHIDConsumer::Consumer(OSArray *elements) 
{
    IOHIDConsumer *consumer = new IOHIDConsumer;
    
    if ((consumer == 0) || !consumer->init() || 
            !consumer->findDesiredElements(elements))
    {
        if (consumer) consumer->release();
        return 0;
    }

    return consumer;
}

//====================================================================================================
// init
//====================================================================================================
bool
IOHIDConsumer::init(OSDictionary *properties)
{
  if (!super::init(properties))  return false;
  
    _soundUpIsPressed = false;
    _soundDownIsPressed = false;
    
    _systemPowerValuePtr = 0;
    _systemSleepValuePtr = 0;
    _systemWakeUpValuePtr = 0;
    
    _volumeIncValuePtr = 0;
    _volumeDecValuePtr = 0;
    _volumeMuteValuePtr = 0;

    _powerValuePtr = 0;
    _resetValuePtr = 0;
    _sleepValuePtr = 0;
    
    _playValuePtr = 0;
    _playOrPauseValuePtr = 0;
    _playOrSkipPtr = 0;
    _nextTrackValuePtr = 0;
    _prevTrackValuePtr = 0;
    _fastFowardValuePtr = 0;
    _rewindValuePtr = 0;
    _stopOrEjectPtr = 0;
    _ejectValuePtr = 0;

    return true;
}

//====================================================================================================
// findDesiredElements
//====================================================================================================
bool
IOHIDConsumer::findDesiredElements(OSArray *elements)
{
    IOHIDElement 	*element;
    bool		found = false;
    
    if (!elements)
        return false;
    
    for (int i=0; i<elements->getCount(); i++)
    {
        element = elements->getObject(i);

        if (element->getUsagePage() == kHIDPage_Consumer)
        {
            switch(element->getUsage())
            {
                case kHIDUsage_Csmr_Power:
                    if (!_powerValuePtr)
                        _powerValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Reset:
                    if (!_resetValuePtr)
                        _resetValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Sleep:
                    if (!_sleepValuePtr)
                        _sleepValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Play:
                    if (!_playValuePtr)
                        _playValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrPause:
                    if (!_playOrPauseValuePtr)
                        _playOrPauseValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrSkip:
                    if (!_playOrSkipPtr)
                        _playOrSkipPtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_ScanNextTrack:
                    if (!_nextTrackValuePtr)
                        _nextTrackValuePtr = element->getElementValue()->value;
                     found = true;
                    break;
                case kHIDUsage_Csmr_ScanPreviousTrack:
                    if (!_prevTrackValuePtr)
                        _prevTrackValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_FastForward:
                    if (!_fastFowardValuePtr)
                        _fastFowardValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Rewind:
                    if (!_rewindValuePtr)
                        _rewindValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_StopOrEject:
                    if (!_stopOrEjectPtr)
                        _stopOrEjectPtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Eject:
                    if (!_ejectValuePtr)
                        _ejectValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeIncrement:
                    if (!_volumeIncValuePtr)
                        _volumeIncValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    if (!_volumeDecValuePtr)
                        _volumeDecValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_Csmr_Mute:
                    if (!_volumeMuteValuePtr)
                        _volumeMuteValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                default:
                    break;
            }
        }
        else if (element->getUsagePage() == kHIDPage_GenericDesktop)
        {
            switch (element->getUsage())
            {
                case kHIDUsage_GD_SystemPowerDown:
                    if (!_systemPowerValuePtr)
                        _systemPowerValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_GD_SystemSleep:
                    if (!_systemSleepValuePtr)
                        _systemSleepValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
                case kHIDUsage_GD_SystemWakeUp:
                    if (!_systemWakeUpValuePtr)
                        _systemWakeUpValuePtr = element->getElementValue()->value;
                    found = true;
                    break;
            }
        }
    }
    return found;
}

//====================================================================================================
// handleReport
//====================================================================================================
void IOHIDConsumer::handleReport()
{
    AbsoluteTime		now;
    bool			muteIsPressed		= false;
    bool			ejectIsPressed		= false;
    bool			powerIsPressed		= false;
    bool			playIsPressed		= false;
    bool			soundUpIsPressed 	= false;
    bool			soundDownIsPressed	= false;
    bool			fastForwardIsPressed	= false;
    bool			rewindIsPressed		= false;
    bool			nextTrackIsPressed	= false;
    bool			prevTrackIsPressed	= false;
    
    // Record current time for the keypress posting.
    clock_get_uptime( &now );

    // Get modifier states for all attached keyboards. This way, when we are queried as to
    // what the state of event flags are, we can tell them and be correct about it.
    FindKeyboardsAndGetModifiers();
    

    // Check and see if the states have changed since last report, if so, we'll notify whoever
    // cares by posting the appropriate key and keystates.
    powerIsPressed 		= ((_powerValuePtr && _powerValuePtr[0] == 1) ||
                                    (_resetValuePtr && _resetValuePtr[0] == 1) ||
                                    (_sleepValuePtr && _sleepValuePtr[0] == 1) ||
                                    (_systemPowerValuePtr && _systemPowerValuePtr[0] == 1) ||
                                    (_systemSleepValuePtr && _systemSleepValuePtr[0] == 1) ||
                                    (_systemWakeUpValuePtr && _systemWakeUpValuePtr[0] == 1));
    playIsPressed 		= ((_playValuePtr && _playValuePtr[0] == 1) ||
                                    (_playOrPauseValuePtr && _playOrPauseValuePtr[0] == 1) ||
                                    (_playOrSkipPtr && _playOrSkipPtr[0] == 1));
    ejectIsPressed 		= (_ejectValuePtr && _ejectValuePtr[0] == 1);
    muteIsPressed 		= (_volumeMuteValuePtr && _volumeMuteValuePtr[0] == 1);
    soundUpIsPressed 		= (_volumeIncValuePtr && _volumeIncValuePtr[0] == 1);
    soundDownIsPressed 		= (_volumeDecValuePtr && _volumeDecValuePtr[0] == 1);
    fastForwardIsPressed 	= (_fastFowardValuePtr && _fastFowardValuePtr[0] == 1);
    rewindIsPressed 		= (_rewindValuePtr && _rewindValuePtr[0] == 1);
    nextTrackIsPressed 		= (_nextTrackValuePtr && _nextTrackValuePtr[0] == 1);
    prevTrackIsPressed 		= (_prevTrackValuePtr && _prevTrackValuePtr[0] == 1);

    // *** BEGIN ONE SHOT BUTTON
    // Post key down and key up events. We don't want auto-repeat happening here.
    if ( muteIsPressed && !_muteIsPressed )
    {
            dispatchKeyboardEvent( kVolumeMute, true, now );
            dispatchKeyboardEvent( kVolumeMute, false, now );
    }
    
    if ( ejectIsPressed && !_ejectIsPressed )
    {
            dispatchKeyboardEvent( kEject, true, now );
            dispatchKeyboardEvent( kEject, false, now );
    }
        
    if ( powerIsPressed && !_powerIsPressed )
    {
            dispatchKeyboardEvent( kPower, true, now );
            dispatchKeyboardEvent( kPower, false, now );
    }
     
    if ( playIsPressed && !_playIsPressed )
    {
            dispatchKeyboardEvent( kPlay, true, now );
            dispatchKeyboardEvent( kPlay, false, now );
    }
    // *** END ONE-SHOT BUTTONS
       
    // *** REPEATING BUTTONS
    // For these keys, we will use IOHIKeyboard's
    // repeat keys code
    if( soundUpIsPressed != _soundUpIsPressed )
    {
            // Sound up state has changed.
            dispatchKeyboardEvent( kVolumeUp, soundUpIsPressed, now );
    }
    
    if( soundDownIsPressed != _soundDownIsPressed )
    {
            // Sound down state has changed.
            dispatchKeyboardEvent( kVolumeDown, soundDownIsPressed, now );
    }
    if( fastForwardIsPressed != _fastForwardIsPressed )
    {
            // Sound up state has changed.
            dispatchKeyboardEvent( kFastForward, soundUpIsPressed, now );
    }
    if( rewindIsPressed != _rewindIsPressed )
    {
            // Sound down state has changed.
            dispatchKeyboardEvent( kRewind, soundDownIsPressed, now );
    }
    if( nextTrackIsPressed != _nextTrackIsPressed )
    {
            // Sound up state has changed.
            dispatchKeyboardEvent( kNextTrack, soundUpIsPressed, now );
    }
    
    if( prevTrackIsPressed != _prevTrackIsPressed )
    {
            // Sound down state has changed.
            dispatchKeyboardEvent( kPreviousTrack, soundDownIsPressed, now );
    }
    // *** END REPEATING BUTTONS
    
    // Save states for our next report.
    _soundUpIsPressed		= soundUpIsPressed;
    _soundDownIsPressed		= soundDownIsPressed;
    _muteIsPressed 		= muteIsPressed;
    _ejectIsPressed		= ejectIsPressed;
    _powerIsPressed		= powerIsPressed;
    _playIsPressed		= playIsPressed;
    _fastForwardIsPressed 	= fastForwardIsPressed;
    _rewindIsPressed		= rewindIsPressed;
    _nextTrackIsPressed		= nextTrackIsPressed;
    _prevTrackIsPressed		= prevTrackIsPressed;
}

//====================================================================================================
// eventFlags - IOHIKeyboard override. This is necessary because we will need to return the state
// of the modifier keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like all modifiers are off.
//====================================================================================================
unsigned IOHIDConsumer::eventFlags()
{
    return( _eventFlags );
}

//====================================================================================================
// alphaLock  - IOHIKeyboard override. This is necessary because we will need to return the state
// of the caps lock keys on attached keyboards. If we don't, then we the HIDSystem gets
// the event, it will look like caps lock keys are off.
//====================================================================================================

bool IOHIDConsumer::alphaLock()
{
    return( _capsLockOn );
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
        
        0x0a,
        
        // Special Key	  	SCANCODE
        //-----------------------------------------------------------
        NX_POWER_KEY,		kPower,
        NX_KEYTYPE_SOUND_UP, 	kVolumeUp,
        NX_KEYTYPE_SOUND_DOWN, 	kVolumeDown,
        NX_KEYTYPE_MUTE, 	kVolumeMute,
        NX_KEYTYPE_PLAY,	kPlay,
	NX_KEYTYPE_NEXT,	kNextTrack,
	NX_KEYTYPE_PREVIOUS,	kPreviousTrack,
	NX_KEYTYPE_FAST,	kFastForward,
	NX_KEYTYPE_REWIND, 	kRewind,
        NX_KEYTYPE_EJECT, 	kEject
    };
    
 
    if( length ) *length = sizeof( ConsumerKeyMap );
    
    return( ConsumerKeyMap );
}

//====================================================================================================
// FindKeyboardsAndGetModifiers
//====================================================================================================
UInt32 IOHIDConsumer::FindKeyboardsAndGetModifiers()
{
	OSIterator	*iterator = NULL;
	OSDictionary	*matchingDictionary = NULL;
	IOHIKeyboard	*device = NULL;
	Boolean 	value = false;
	OSObject 	*adbProperty;
	const char 	*adbKey;
	
	
	_eventFlags = 0;
	_capsLockOn = FALSE;
	
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
		
		//Ignore the eventFlags of non-keyboard ADB devices such as audio buttons
                //
		adbProperty = device->getProperty("ADB Match");
		if (adbProperty)
		{
		    adbKey = ((OSString *)adbProperty)->getCStringNoCopy();
		    if( *adbKey != '2' )	//If not a keyboard
			continue;
		}

		value = false;
		
		// Save the caps lock state. If more than one keyboard has it down, that's fine -- we
		// just want to know if ANY keyboards have the key down.
		//
		if( device->alphaLock() )
		{
			_capsLockOn = TRUE;
		}

		// OR in the flags, so we get a combined IOHIKeyboard device flags state. That
		// way, if the user is pressing command on one keyboard, shift on another, and
		// then hits an eject key, we'll get both modifiers.
		//
		_eventFlags |= device->eventFlags();
	}
	
exit:
	
	if( matchingDictionary ) matchingDictionary->release();
	if( iterator ) iterator->release();
	
	return( _eventFlags );
}
