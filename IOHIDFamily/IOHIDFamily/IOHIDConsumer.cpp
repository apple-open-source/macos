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

#define InitButtons(ptrs) 			\
{						\
    ptrs = 0;					\
    ptrs ## Count = 0;				\	
}


#define AppendButtons(ptrs, ptr)		\
{						\
    UInt32 size = sizeof(UInt32 *) * ( ptrs ## Count); \
    UInt32 ** tempPtrs = IOMalloc(size + sizeof(UInt32 *)); \
                                                \
    if (ptrs) {					\
        bcopy( ptrs, tempPtrs, size );		\
        IOFree(ptrs, size);			\
    }						\
    ptrs = tempPtrs;				\
    ptrs[ ptrs ## Count ++] = ptr;		\
}


#define ReleaseButtons(ptrs)			\
{						\
    UInt32 size = sizeof(UInt32 *) * ( ptrs ## Count); \
    IOFree(ptrs, size);				\
    ptrs = 0;					\
    ptrs ## Count = 0;				\
}			

#define GetButtonValues(ptrs, value)		\
{						\
    for (int i=0; i< ptrs ## Count ; i++)	\
    {						\
        value |= (ptrs[i][0] & 0x1);		\
    }						\
}						

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
    
    InitButtons(_systemPowerValuePtrs);
    InitButtons(_systemSleepValuePtrs);
    InitButtons(_systemWakeUpValuePtrs);
    
    InitButtons(_volumeIncValuePtrs);
    InitButtons(_volumeDecValuePtrs);
    InitButtons(_volumeMuteValuePtrs);

    InitButtons(_powerValuePtrs);
    InitButtons(_resetValuePtrs);
    InitButtons(_sleepValuePtrs);
    
    InitButtons(_playValuePtrs);
    InitButtons(_playOrPauseValuePtrs);
    InitButtons(_playOrSkipPtrs);
    InitButtons(_nextTrackValuePtrs);
    InitButtons(_prevTrackValuePtrs);
    InitButtons(_fastFowardValuePtrs);
    InitButtons(_rewindValuePtrs);
    InitButtons(_stopOrEjectPtrs);
    InitButtons(_ejectValuePtrs);

    return true;
}

void IOHIDConsumer::free()
{
    ReleaseButtons(_systemPowerValuePtrs);
    ReleaseButtons(_systemSleepValuePtrs);
    ReleaseButtons(_systemWakeUpValuePtrs);
    
    ReleaseButtons(_volumeIncValuePtrs);
    ReleaseButtons(_volumeDecValuePtrs);
    ReleaseButtons(_volumeMuteValuePtrs);

    ReleaseButtons(_powerValuePtrs);
    ReleaseButtons(_resetValuePtrs);
    ReleaseButtons(_sleepValuePtrs);
    
    ReleaseButtons(_playValuePtrs);
    ReleaseButtons(_playOrPauseValuePtrs);
    ReleaseButtons(_playOrSkipPtrs);
    ReleaseButtons(_nextTrackValuePtrs);
    ReleaseButtons(_prevTrackValuePtrs);
    ReleaseButtons(_fastFowardValuePtrs);
    ReleaseButtons(_rewindValuePtrs);
    ReleaseButtons(_stopOrEjectPtrs);
    ReleaseButtons(_ejectValuePtrs);


    super::free();
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
                    AppendButtons(_powerValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Reset:
                    AppendButtons(_resetValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Sleep:
                    AppendButtons(_sleepValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Play:
                    AppendButtons(_playValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrPause:
                    AppendButtons(_playOrPauseValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_PlayOrSkip:
                    AppendButtons(_playOrSkipPtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_ScanNextTrack:
                    AppendButtons(_nextTrackValuePtrs, element->getElementValue()->value);
                     found = true;
                    break;
                case kHIDUsage_Csmr_ScanPreviousTrack:
                    AppendButtons(_prevTrackValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_FastForward:
                    AppendButtons(_fastFowardValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Rewind:
                    AppendButtons(_rewindValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_StopOrEject:
                    AppendButtons(_stopOrEjectPtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Eject:
                    AppendButtons(_ejectValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeIncrement:
                    AppendButtons(_volumeIncValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    AppendButtons(_volumeDecValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_Csmr_Mute:
                    AppendButtons(_volumeMuteValuePtrs, element->getElementValue()->value);
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
                    AppendButtons(_systemPowerValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_GD_SystemSleep:
                    AppendButtons(_systemSleepValuePtrs, element->getElementValue()->value);
                    found = true;
                    break;
                case kHIDUsage_GD_SystemWakeUp:
                    AppendButtons(_systemWakeUpValuePtrs, element->getElementValue()->value);
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
    powerIsPressed = 0;
    GetButtonValues(_powerValuePtrs, powerIsPressed);
    GetButtonValues(_resetValuePtrs, powerIsPressed);
    GetButtonValues(_sleepValuePtrs, powerIsPressed);
    GetButtonValues(_systemPowerValuePtrs, powerIsPressed);
    GetButtonValues(_systemSleepValuePtrs, powerIsPressed);
    GetButtonValues(_systemWakeUpValuePtrs, powerIsPressed);

    playIsPressed = 0;
    GetButtonValues(_playValuePtrs, playIsPressed);
    GetButtonValues(_playOrPauseValuePtrs, playIsPressed);
    GetButtonValues(_playOrSkipPtrs, playIsPressed);
    
    ejectIsPressed = 0;
    GetButtonValues(_ejectValuePtrs, ejectIsPressed);

    muteIsPressed = 0;
    GetButtonValues(_volumeMuteValuePtrs, muteIsPressed);

    soundUpIsPressed = 0;
    GetButtonValues(_volumeIncValuePtrs, soundUpIsPressed);

    soundDownIsPressed = 0;
    GetButtonValues(_volumeDecValuePtrs, soundDownIsPressed);

    fastForwardIsPressed = 0;
    GetButtonValues(_fastFowardValuePtrs, fastForwardIsPressed);

    rewindIsPressed = 0;
    GetButtonValues(_rewindValuePtrs, rewindIsPressed);

    nextTrackIsPressed = 0;
    GetButtonValues(_nextTrackValuePtrs, nextTrackIsPressed);

    prevTrackIsPressed = 0;
    GetButtonValues(_prevTrackValuePtrs, prevTrackIsPressed);


    if ( !( muteIsPressed && soundUpIsPressed && soundDownIsPressed ) )
    {
        if ( muteIsPressed != _muteIsPressed )
        {
                // Mute state has changed.
                dispatchKeyboardEvent( kVolumeMute, muteIsPressed, now );
        }
        
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
    }
    
    if ( ejectIsPressed != _ejectIsPressed )
    {
            // Eject state has changed.
            dispatchKeyboardEvent( kEject, ejectIsPressed, now );
    }
        
    if ( powerIsPressed != _powerIsPressed )
    {
            // Power state has changed.
            dispatchKeyboardEvent( kPower, powerIsPressed, now );
    }
     
    if ( playIsPressed != _playIsPressed )
    {
            // Play state has changed.
            dispatchKeyboardEvent( kPlay, playIsPressed, now );
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
		
		//Ignore the eventFlags keyboards that don't have modifiers defined
		if (!device->getProperty(kIOHIDKeyboardSupportedModifiersKey))
			continue;
		
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
