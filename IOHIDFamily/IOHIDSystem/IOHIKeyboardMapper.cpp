/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/* 	Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved. 
 *
 * KeyMap.m - Generic keymap string parser and keycode translator.
 *
 * HISTORY
 * 19 June 1992    Mike Paquette at NeXT
 *      Created. 
 * 5  Aug 1993	  Erik Kay at NeXT
 *	minor API cleanup
 * 11 Nov 1993	  Erik Kay at NeXT
 *	fix to allow prevent long sequences from overflowing the event queue
 * 12 Nov 1998    Dan Markarian at Apple
 *      major cleanup of public API's; converted to C++
 */

#include <sys/systm.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIKeyboardMapper.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDSystem.h>
#include <libkern/OSByteOrder.h>

#include "IOHIDKeyboardDevice.h"

#define super OSObject
OSDefineMetaClassAndStructors(IOHIKeyboardMapper, OSObject);

// sticky keys private state flags
enum
{
    kState_ClearHeldKeysFirst	= 0x0100,	// when set, we should clear all held keys
											// this is a hack we are using since we
											// cannot post key up events when our
											// entry point is not a key event
};



// delay filter private state flags
enum
{
    kState_Aborted_Flag		= 0x0200,
    kState_In_Progess_Flag	= 0x0400,
    kState_Is_Repeat_Flag	= 0x0800,
};

// ADB Key code for F12
#define kADB_KEYBOARD_F12 0x6f

// Shortcut for post slow key translation
#define postSlowKeyTranslateKeyCode(owner,key,keyDown,keyBits) 	\
    if (!owner->f12EjectFilterKey(key, keyDown, keyBits)) 	\
        if (!owner->stickyKeysFilterKey(key, keyDown, keyBits))	\
            owner->rawTranslateKeyCode(key, keyDown, keyBits);


// Shortcut for determining if we are interested in this modifier
#define modifierOfInterest(keyBits) \
            ((keyBits & NX_MODMASK) && \
            ((keyBits & NX_WHICHMODMASK) >= NX_MODIFIERKEY_SHIFT) && \
            ((keyBits & NX_WHICHMODMASK) <= NX_MODIFIERKEY_COMMAND))

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOHIKeyboardMapper * IOHIKeyboardMapper::keyboardMapper(
                                        IOHIKeyboard * delegate,
                                        const UInt8 *  mapping,
                                        UInt32         mappingLength,
                                        bool           mappingShouldBeFreed )
{
  IOHIKeyboardMapper * me = new IOHIKeyboardMapper;

  if (me && !me->init(delegate, mapping, mappingLength, mappingShouldBeFreed))
  {
    me->free();
    return 0;
  }

  return me;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Common KeyMap initialization
 */
bool IOHIKeyboardMapper::init( IOHIKeyboard * delegate,
                               const UInt8 *  mapping,
                               UInt32         mappingLength,
                               bool           mappingShouldBeFreed )
{
	if (!super::init())  return false;
	
	_delegate                 = delegate;
	
	if (!parseKeyMapping(mapping, mappingLength, &_parsedMapping))  return false;
	
	_mappingShouldBeFreed		= mappingShouldBeFreed;
	_parsedMapping.mapping		= mapping;
	_parsedMapping.mappingLen	= mappingLength;
	
	_hidSystem					= NULL;
	_stateDirty					= false;
        
        _reserved = IONew(ExpansionData, 1);
        
        _ejectTimerEventSource		= 0;
        
        _f12Eject_State			= 0;
        
        _eject_Delay_MS			= 250;  // Default HI setting.
        
        _slowKeys_State			= 0;
        
        _slowKeys_Delay_MS		= 0;
        
        _slowKeysTimerEventSource	= 0;

        _stickyKeysMouseClickEventSource = 0;
        
        _hidKeyboardNub			= 0;
        
        // If any keys were defined, create a device nub to make this keyboard
        // viewable to the HIDManager
        if (_parsedMapping.numDefs &&
            _delegate && !OSDynamicCast(IOHIDDevice, _delegate->getProvider()))
        {
            _hidKeyboardNub = IOHIDKeyboardDevice::newKeyboardDevice();
                                
            if (_hidKeyboardNub &&
                (!_hidKeyboardNub->attach(_delegate) || 
                !_hidKeyboardNub->start(_delegate)))
            {
                _hidKeyboardNub->release();
                _hidKeyboardNub = 0;
            }
        }


	return stickyKeysinit();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOHIKeyboardMapper::free()
{
  stickyKeysfree();
  
  if (_ejectTimerEventSource) {
    _ejectTimerEventSource->release();
    _ejectTimerEventSource = 0; 
  }
  
  if (_slowKeysTimerEventSource) {
    _slowKeysTimerEventSource->release();
    _slowKeysTimerEventSource = 0; 
  }

  if (_hidKeyboardNub) {
        _hidKeyboardNub->release();
        _hidKeyboardNub = 0;
  }

  if (_stickyKeysMouseClickEventSource) {
    _stickyKeysMouseClickEventSource->release();
    _stickyKeysMouseClickEventSource = 0; 
  }

  if (_reserved) {
    IODelete(_reserved, ExpansionData, 1);
  }
  
  if (_mappingShouldBeFreed && _parsedMapping.mapping)
    IOFree((void *)_parsedMapping.mapping, _parsedMapping.mappingLen);

  super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt8 * IOHIKeyboardMapper::mapping()
{
  return (const UInt8 *)_parsedMapping.mapping;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 IOHIKeyboardMapper::mappingLength()
{
  return _parsedMapping.mappingLen;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOHIKeyboardMapper::serialize(OSSerialize *s) const
{
    OSData * data;
    bool ok;

    if (s->previouslySerialized(this)) return true;

    data = OSData::withBytesNoCopy( (void *) _parsedMapping.mapping, _parsedMapping.mappingLen );
    if (data) {
	ok = data->serialize(s);
	data->release();
    } else
	ok = false;

    return( ok );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//
// Perform the mapping of 'key' moving in the specified direction
// into events.
//

void IOHIKeyboardMapper::translateKeyCode(UInt8        key,
                                          bool         keyDown,
                                          kbdBitVector keyBits)
{

    // Post the event to the HID Manager
    if (_hidKeyboardNub)
        _hidKeyboardNub->postKeyboardEvent(key, keyDown);

    // SlowKeys filter, if slowKeysFilterKey returns true, 
    // this key is already processed
    if (!slowKeysFilterKey(key, keyDown, keyBits))
        // Filter out F12 to check for an eject
        if (!f12EjectFilterKey(key, keyDown, keyBits))
            // Stickykeys filter, if stickyKeysFilterKey returns true, 
            // this key is already processed
            if (!stickyKeysFilterKey(key, keyDown, keyBits))
                    // otherwise, call the original raw translate key code
                    rawTranslateKeyCode(key, keyDown, keyBits);
}

// rawTranslateKeyCode is the original translateKeyCode function,
// 	prior to the Stickykeys feature
//
// Perform the mapping of 'key' moving in the specified direction
// into events.
//
void IOHIKeyboardMapper::rawTranslateKeyCode(UInt8        key,
                                          bool         keyDown,
                                          kbdBitVector keyBits)
{
  unsigned char thisBits = _parsedMapping.keyBits[key];

  /* do mod bit update and char generation in useful order */
  if (keyDown)
  {
    EVK_KEYDOWN(key, keyBits);

    if (thisBits & NX_MODMASK)     doModCalc(key, keyBits);
    if (thisBits & NX_CHARGENMASK) doCharGen(key, keyDown);
  }
  else
  {
    EVK_KEYUP(key, keyBits);
    if (thisBits & NX_CHARGENMASK) doCharGen(key, keyDown);
    if (thisBits & NX_MODMASK)     doModCalc(key, keyBits);
  }

  //Fix JIS localization.  We are here because the JIS keys Yen, Ro, Eisu,
  //  Kana, and "," are not matched in _parsedMapping.keyBits[] above even
  //  though the keyboard drivers are sending the correct scan codes.
  //  The check for interfaceID() below makes sure both ADB and USB works.
  //  This fix has been tested with AppKit and Carbon for Kodiak 1H 
  if( 0 == (thisBits & (NX_MODMASK | NX_CHARGENMASK))) 
    if (_delegate->interfaceID() == NX_EVS_DEVICE_INTERFACE_ADB)  
  {
    	unsigned charCode=0;

    	switch (key) {
		case 0x5F:	// numpad ',' using raw ADB scan code
			charCode = ',';
			break;
		case 0x5E:  //ro
			charCode = '_';
			break;
		case 0x5d:  //Yen
			charCode = '\\';
			break;
		case 0x0a:
			charCode = 0xa7;
			break;
		case 0x66:	// eisu
		case 0x68:	// kana	
		default:
			// do nothing. AppKit has fix in 1H
			break;
    	}
    	/* Post the keyboard event */
    	_delegate->keyboardEvent(keyDown ? NX_KEYDOWN : NX_KEYUP,
        	/* flags */            _delegate->eventFlags(),
        	/* keyCode */          key,
        	/* charCode */         charCode,
        	/* charSet */          0,  //0 is adequate for JIS
        	/* originalCharCode */ 0,
        	/* originalCharSet */  0);
  }

#ifdef OMITPENDINGKEYCAPS
  unsigned char *  	bp;

  //Make KeyCaps.app see the caps lock 
  if (key == _parsedMapping.specialKeys[NX_KEYTYPE_CAPS_LOCK])  //ADB caps lock 0x39
  {
    if (_delegate->alphaLock() == keyDown) 
    //This logic is needed for non-locking USB caps lock
    {
	_delegate->keyboardEvent(keyDown ? NX_KEYDOWN : NX_KEYUP,
	    _delegate->eventFlags(), key, 0, 0, 0, 0);
    }
  }

    //Find scan code corresponding to PowerBook fn key (0x3f in ADB)
    bp = _parsedMapping.modDefs[NX_MODIFIERKEY_SECONDARYFN];  //7th array entry
    if (bp)
    {
	bp++;  //now points to actual ADB scan code
	if (key == *bp )  //ADB fn key should be 0x3f here 
	{
		_delegate->keyboardEvent(keyDown ? NX_KEYDOWN : NX_KEYUP,
		_delegate->eventFlags(), key, 0, 0, 0, 0);
	}
    }
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//
// Support goop for parseKeyMapping.  These routines are
// used to walk through the keymapping string.  The string
// may be composed of bytes or shorts.  If using shorts, it
// MUST always be aligned to use short boundries.
//
typedef struct {
    unsigned const char *bp;
    unsigned const char *endPtr;
    int shorts;
} NewMappingData;

static inline unsigned int NextNum(NewMappingData *nmd)
{
    if (nmd->bp >= nmd->endPtr)
		return(0);
    if (nmd->shorts)
		return OSSwapBigToHostInt16(*((unsigned short *)nmd->bp)++);
    else
		return (*((unsigned char *)nmd->bp)++);
}

//
// Perform the actual parsing operation on a keymap.  Returns false on failure.
//

bool IOHIKeyboardMapper::parseKeyMapping(const UInt8 *        mapping,
                                         UInt32               mappingLength,
	                                 NXParsedKeyMapping * parsedMapping) const
{
	NewMappingData nmd;
	int i, j, k, l, n;
	unsigned int m;
	int keyMask, numMods;
	int maxSeqNum = -1;
        unsigned char *         bp; 


	/* Initialize the new map. */
	bzero( parsedMapping, sizeof (NXParsedKeyMapping) );
	parsedMapping->maxMod = -1;
	parsedMapping->numDefs = -1;
	parsedMapping->numSeqs = -1;

	nmd.endPtr = mapping + mappingLength;
	nmd.bp = mapping;
	nmd.shorts = 1;		// First value, the size, is always a short

	/* Start filling it in with the new data */
	parsedMapping->mapping = (unsigned char *)mapping;
	parsedMapping->mappingLen = mappingLength;
	parsedMapping->shorts = nmd.shorts = NextNum(&nmd);

	/* Walk through the modifier definitions */
	numMods = NextNum(&nmd);
	for(i=0; i<numMods; i++)
	{
	    /* Get bit number */
	    if ((j = NextNum(&nmd)) >= NX_NUMMODIFIERS)
		return false;

	    /* Check maxMod */
	    if (j > parsedMapping->maxMod)
		parsedMapping->maxMod = j;

	    /* record position of this def */
	    parsedMapping->modDefs[j] = (unsigned char *)nmd.bp;

	    /* Loop through each key assigned to this bit */
	    for(k=0,n = NextNum(&nmd);k<n;k++)
	    {
		/* Check that key code is valid */
		if ((l = NextNum(&nmd)) >= NX_NUMKEYCODES)
		    return false;
		/* Make sure the key's not already assigned */
		if (parsedMapping->keyBits[l] & NX_MODMASK)
			return false;
		/* Set bit for modifier and which one */
 
		//The "if" here is to patch the keymapping file.  That file has nothing
		// for num lock, so no change is required here for num lock.
		// Also, laptop Macs have num lock handled by Buttons driver
		if ((j != NX_MODIFIERKEY_ALPHALOCK) || (_delegate->doesKeyLock(NX_KEYTYPE_CAPS_LOCK)) )
	  	{
			parsedMapping->keyBits[l] |=NX_MODMASK | (j & NX_WHICHMODMASK);
		}

	    }
	}
	
	//This is here because keymapping file has an entry for caps lock, but in
	//  order to trigger special code (line 646-), the entry needs to be zero
	if (!_delegate->doesKeyLock(NX_KEYTYPE_CAPS_LOCK))
		parsedMapping->modDefs[NX_MODIFIERKEY_ALPHALOCK] = 0;  

	//This section is here to force keymapping to include the PowerBook's secondary
	// fn key as a new modifier key.  This code can be removed once the keymapping
	// file has the fn key (ADB=0x3f) in the modifiers section.  
	// NX_MODIFIERKEY_SECONDARYFN = 8 in ev_keymap.h
	if (_delegate->interfaceID() == NX_EVS_DEVICE_INTERFACE_ADB)
	{
		parsedMapping->keyBits[0x3f] |=NX_MODMASK | (NX_MODIFIERKEY_SECONDARYFN & NX_WHICHMODMASK);
	}

	/* Walk through each key definition */
	parsedMapping->numDefs = NextNum(&nmd);
	n = parsedMapping->numDefs;
	for( i=0; i < NX_NUMKEYCODES; i++)
	{
	    if (i < n)
	    {
		parsedMapping->keyDefs[i] = (unsigned char *)nmd.bp;
		if ((keyMask = NextNum(&nmd)) != (nmd.shorts ? 0xFFFF: 0x00FF))
		{
		    /* Set char gen bit for this guy: not a no-op */
		    parsedMapping->keyBits[i] |= NX_CHARGENMASK;
		    /* Check key defs to find max sequence number */
		    for(j=0, k=1; j<=parsedMapping->maxMod; j++, keyMask>>=1)
		    {
			    if (keyMask & 0x01)
				k*= 2;
		    }
		    for(j=0; j<k; j++)
		    {
			m = NextNum(&nmd);
			l = NextNum(&nmd);
			if (m == (unsigned)(nmd.shorts ? 0xFFFF: 0x00FF))
			    if (((int)l) > maxSeqNum)
				maxSeqNum = l;	/* Update expected # of seqs */
		    }
		}
		else /* unused code within active range */
		    parsedMapping->keyDefs[i] = NULL;
	    }
	    else /* Unused code past active range */
	    {
		parsedMapping->keyDefs[i] = NULL;
	    }
	}
	/* Walk through sequence defs */
	parsedMapping->numSeqs = NextNum(&nmd);
       	/* If the map calls more sequences than are declared, bail out */
	if (parsedMapping->numSeqs <= maxSeqNum)
	    return false;

	/* Walk past all sequences */
	for(i = 0; i < parsedMapping->numSeqs; i++)
	{
	    parsedMapping->seqDefs[i] = (unsigned char *)nmd.bp;
	    /* Walk thru entries in a seq. */
	    for(j=0, l=NextNum(&nmd); j<l; j++)
	    {
		NextNum(&nmd);
		NextNum(&nmd);
	    }
	}
	/* Install Special device keys.  These override default values. */
	numMods = NextNum(&nmd);	/* Zero on old style keymaps */
	parsedMapping->numSpecialKeys = numMods;
	if ( numMods > NX_NUMSPECIALKEYS )
	    return false;
	if ( numMods )
	{
	    for ( i = 0; i < NX_NUMSPECIALKEYS; ++i )
		parsedMapping->specialKeys[i] = NX_NOSPECIALKEY;

            //This "if" will cover both ADB and USB keyboards.  This code does not
            //  have to be here if the keymaps include these two entries.  Keyboard
	    //  drivers already have these entries, but keymapping file does not
	    if (_delegate->interfaceID() == NX_EVS_DEVICE_INTERFACE_ADB)
	    {
		//ADB capslock:
	    	parsedMapping->specialKeys[NX_KEYTYPE_CAPS_LOCK] = 0x39;

		//ADB numlock for external keyboards, not PowerBook keyboards:
	    	parsedMapping->specialKeys[NX_KEYTYPE_NUM_LOCK] = 0x47; 
		
		//HELP key needs to be visible
		parsedMapping->keyDefs[0x72] = parsedMapping->keyDefs[0x47];
	    }

	    //Keymapping file can override caps and num lock above now:
	    for ( i = 0; i < numMods; ++i )
	    {
		j = NextNum(&nmd);	/* Which modifier key? */
		l = NextNum(&nmd);	/* Scancode for modifier key */
		if ( j >= NX_NUMSPECIALKEYS )
		    return false;
		parsedMapping->specialKeys[j] = l;
	    }
	}
	else  /* No special keys defs implies an old style keymap */
	{
		return false;	/* Old style keymaps are guaranteed to do */
				/* the wrong thing on ADB keyboards */
	}
	/* Install bits for Special device keys */
	for(i=0; i<NX_NUM_SCANNED_SPECIALKEYS; i++)
	{
	    if ( parsedMapping->specialKeys[i] != NX_NOSPECIALKEY )
	    {
		parsedMapping->keyBits[parsedMapping->specialKeys[i]] |=
		    (NX_CHARGENMASK | NX_SPECIALKEYMASK);
	    }
	}
    
        //caps lock keys should not generate characters.
        if (_delegate->doesKeyLock(NX_KEYTYPE_CAPS_LOCK))
        {
                parsedMapping->keyBits[ parsedMapping->specialKeys[NX_KEYTYPE_CAPS_LOCK] ]
                        &= ~NX_CHARGENMASK;
        }

        //Find scan code corresponding to PowerBook fn key (0x3f in ADB)
        //   and then make sure it does not generate a character
        bp = _parsedMapping.modDefs[NX_MODIFIERKEY_SECONDARYFN];  //7th array entry
        if (bp)
        {
                bp++;  //now points to actual ADB scan code
                parsedMapping->keyBits[ *bp ] &= ~NX_CHARGENMASK;
        }

	return true;
}


//Retrieve a key from mapping above.  Useful for IOHIKeyboard 
UInt8 IOHIKeyboardMapper::getParsedSpecialKey(UInt8 logical)
{
    UInt8	retval;
    
    if ( logical < NX_NUMSPECIALKEYS)
	retval = _parsedMapping.specialKeys[logical];
    else
	retval = 0xff;  //careful, 0 is mapped already
    return retval;
}


static inline int NEXTNUM(unsigned char ** mapping, short shorts)
{
  int returnValue;

  if (shorts)
  {
    returnValue = OSSwapBigToHostInt16(*((unsigned short *)*mapping));
    *mapping += sizeof(unsigned short);
  }
  else
  {
    returnValue = **((unsigned char  **)mapping);
    *mapping += sizeof(unsigned char);
  }

  return returnValue;
}

//
// Look up in the keymapping each key associated with the modifier bit.
// Look in the device state to see if that key is down.
// Return 1 if a key for modifier 'bit' is down.  Return 0 if none is down
//
static inline int IsModifierDown(NXParsedKeyMapping *parsedMapping,
			 	 kbdBitVector keyBits,
				 int bit )
{
    int i, n;
    unsigned char *mapping;
    unsigned key;
    short shorts = parsedMapping->shorts;

    if ( (mapping = parsedMapping->modDefs[bit]) != 0 ) {
	for(i=0, n=NEXTNUM(&mapping, shorts); i<n; i++)
	{
	    key = NEXTNUM(&mapping, shorts);
	    if ( EVK_IS_KEYDOWN(key, keyBits) )
		return 1;
	}
    }
    return 0;
}

void IOHIKeyboardMapper::calcModBit(int bit, kbdBitVector keyBits)
{
	int		bitMask;
	unsigned	myFlags;

	bitMask = 1<<(bit+16);

	/* Initially clear bit, as if key-up */
	myFlags = _delegate->deviceFlags() & (~bitMask);
	/* Set bit if any associated keys are down */
	if ( IsModifierDown( &_parsedMapping, keyBits, bit ) )
		myFlags |= bitMask;

	if ( bit == NX_MODIFIERKEY_ALPHALOCK ) /* Caps Lock key */
	    _delegate->setAlphaLock((myFlags & NX_ALPHASHIFTMASK) ? true : false);
	else if ( bit == NX_MODIFIERKEY_NUMLOCK ) /* Num Lock key */
	    _delegate->setNumLock((myFlags & NX_NUMERICPADMASK) ? true : false);

	_delegate->setDeviceFlags(myFlags);

}


//
// Perform flag state update and generate flags changed events for this key.
//
void IOHIKeyboardMapper::doModCalc(int key, kbdBitVector keyBits)
{
    int thisBits;
    thisBits = _parsedMapping.keyBits[key];
    if (thisBits & NX_MODMASK)
    {
	calcModBit((thisBits & NX_WHICHMODMASK), keyBits);
	/* The driver generates flags-changed events only when there is
	   no key-down or key-up event generated */
	if (!(thisBits & NX_CHARGENMASK))
	{
		/* Post the flags-changed event */
		_delegate->keyboardEvent(NX_FLAGSCHANGED,
		 /* flags */            _delegate->eventFlags(),
		 /* keyCode */          key,
		 /* charCode */         0,
		 /* charSet */          0,
		 /* originalCharCode */ 0,
		 /* originalCharSet */  0);
	}
	else	/* Update, but don't generate an event */
		_delegate->updateEventFlags(_delegate->eventFlags());
    }
}

//
// Perform character event generation for this key
//
void IOHIKeyboardMapper::doCharGen(int keyCode, bool down)
{
    int	i, n, eventType, adjust, thisMask, modifiers, saveModifiers;
    short shorts;
    unsigned charSet, origCharSet;
    unsigned charCode, origCharCode;
    unsigned char *mapping;
    unsigned eventFlags, origflags;

    _delegate->setCharKeyActive(true);	// a character generating key is active

    eventType = (down == true) ? NX_KEYDOWN : NX_KEYUP;
    eventFlags = _delegate->eventFlags();
    saveModifiers = eventFlags >> 16;	// machine independent mod bits
    /* Set NX_ALPHASHIFTMASK based on alphaLock OR shift active */
    if( saveModifiers & (NX_SHIFTMASK >> 16))
	saveModifiers |= (NX_ALPHASHIFTMASK >> 16);


    /* Get this key's key mapping */
    shorts = _parsedMapping.shorts;
    mapping = _parsedMapping.keyDefs[keyCode];
    modifiers = saveModifiers;
    if ( mapping )
    {


	/* Build offset for this key */
	thisMask = NEXTNUM(&mapping, shorts);
	if (thisMask && modifiers)
	{
	    adjust = (shorts ? sizeof(short) : sizeof(char))*2;
	    for( i = 0; i <= _parsedMapping.maxMod; ++i)
	    {
		if (thisMask & 0x01)
		{
		    if (modifiers & 0x01)
			mapping += adjust;
		    adjust *= 2;
		}
		thisMask >>= 1;
		modifiers >>= 1;
	    }
	}
	charSet = NEXTNUM(&mapping, shorts);
	charCode = NEXTNUM(&mapping, shorts);

	/* construct "unmodified" character */
	mapping = _parsedMapping.keyDefs[keyCode];
        modifiers = saveModifiers & ((NX_ALPHASHIFTMASK | NX_SHIFTMASK) >> 16);

	thisMask = NEXTNUM(&mapping, shorts);
	if (thisMask && modifiers)
	{
	    adjust = (shorts ? sizeof(short) : sizeof(char)) * 2;
	    for ( i = 0; i <= _parsedMapping.maxMod; ++i)
	    {
		if (thisMask & 0x01)
		{
		    if (modifiers & 0x01)
			mapping += adjust;
		    adjust *= 2;
		}
		thisMask >>= 1;
		modifiers >>= 1;
	    }
	}
	origCharSet = NEXTNUM(&mapping, shorts);
	origCharCode = NEXTNUM(&mapping, shorts);
	
	if (charSet == (unsigned)(shorts ? 0xFFFF : 0x00FF))
	{
	    // Process as a character sequence
	    // charCode holds the sequence number
	    mapping = _parsedMapping.seqDefs[charCode];
	    
	    origflags = eventFlags;
	    for(i=0,n=NEXTNUM(&mapping, shorts);i<n;i++)
	    {
		if ( (charSet = NEXTNUM(&mapping, shorts)) == 0xFF ) /* metakey */
		{
		    if ( down == true )	/* down or repeat */
		    {
			eventFlags |= (1 << (NEXTNUM(&mapping, shorts) + 16));
			_delegate->keyboardEvent(NX_FLAGSCHANGED,
			 /* flags */            _delegate->deviceFlags(),
			 /* keyCode */          keyCode,
			 /* charCode */         0,
			 /* charSet */          0,
			 /* originalCharCode */ 0,
			 /* originalCharSet */  0);
		    }
		    else
			NEXTNUM(&mapping, shorts);	/* Skip over value */
		}
		else
		{
		    charCode = NEXTNUM(&mapping, shorts);
		    _delegate->keyboardEvent(eventType,
		     /* flags */            eventFlags,
		     /* keyCode */          keyCode,
		     /* charCode */         charCode,
		     /* charSet */          charSet,
		     /* originalCharCode */ charCode,
		     /* originalCharSet */  charSet);
		}
	    }
	    /* Done with macro.  Restore the flags if needed. */
	    if ( eventFlags != origflags )
	    {
		_delegate->keyboardEvent(NX_FLAGSCHANGED,
		 /* flags */            _delegate->deviceFlags(),
		 /* keyCode */          keyCode,
		 /* charCode */         0,
		 /* charSet */          0,
		 /* originalCharCode */ 0,
		 /* originalCharSet */  0);
		eventFlags = origflags;
	    }
	}
	else	/* A simple character generating key */
	{
	    _delegate->keyboardEvent(eventType,
	     /* flags */            eventFlags,
	     /* keyCode */          keyCode,
	     /* charCode */         charCode,
	     /* charSet */          charSet,
	     /* originalCharCode */ origCharCode,
	     /* originalCharSet */  origCharSet);
	}
    } /* if (mapping) */
    
    /*
     * Check for a device control key: note that they always have CHARGEN
     * bit set
     */
    if (_parsedMapping.keyBits[keyCode] & NX_SPECIALKEYMASK)
    {
	for(i=0; i<NX_NUM_SCANNED_SPECIALKEYS; i++)
	{
	    if ( keyCode == _parsedMapping.specialKeys[i] )
	    {
		_delegate->keyboardSpecialEvent(eventType,
		 	        /* flags */     eventFlags,
			        /* keyCode */   keyCode,
			        /* specialty */ i);
		/*
		 * Special keys hack for letting an arbitrary (non-locking)
		 * key act as a CAPS-LOCK key.  If a special CAPS LOCK key
		 * is designated, and there is no key designated for the 
		 * AlphaLock function, then we'll let the special key toggle
		 * the AlphaLock state.
		 */
		if (i == NX_KEYTYPE_CAPS_LOCK
		    && down == true
		    && !_parsedMapping.modDefs[NX_MODIFIERKEY_ALPHALOCK] )
		{
		    unsigned myFlags = _delegate->deviceFlags();
		    bool alphaLock = (_delegate->alphaLock() == false);

		    // Set delegate's alphaLock state
		    _delegate->setAlphaLock(alphaLock);
		    // Update the delegate's flags
		    if ( alphaLock )
		    	myFlags |= NX_ALPHASHIFTMASK;
		    else
		        myFlags &= ~NX_ALPHASHIFTMASK;

		    _delegate->setDeviceFlags(myFlags);

		    _delegate->keyboardEvent(NX_FLAGSCHANGED,
		     /* flags */            myFlags,
		     /* keyCode */          keyCode,
		     /* charCode */         0,
		     /* charSet */          0,
		     /* originalCharCode */ 0,
		     /* originalCharSet */  0);
		} 
		else 	if (i == NX_KEYTYPE_NUM_LOCK
		    && down == true
		    && !_parsedMapping.modDefs[NX_MODIFIERKEY_NUMLOCK] )
		{
		    unsigned myFlags = _delegate->deviceFlags();
		    bool numLock = (_delegate->numLock() == false);

		    // Set delegate's alphaLock state
		    _delegate->setNumLock(numLock);
		    if ( numLock )
		    	myFlags |= NX_NUMERICPADMASK;
		    else
		        myFlags &= ~NX_NUMERICPADMASK;

		    _delegate->setDeviceFlags(myFlags);
		    _delegate->keyboardEvent(NX_FLAGSCHANGED,
		     /* flags */            myFlags,
		     /* keyCode */          keyCode,
		     /* charCode */         0,
		     /* charSet */          0,
		     /* originalCharCode */ 0,
		     /* originalCharSet */  0);
		} 

		break;
	    }
	}
    }
}


void IOHIKeyboardMapper::setKeyboardTarget (IOService * keyboardTarget)
{
	_hidSystem = OSDynamicCast( IOHIDSystem, keyboardTarget );
}

void IOHIKeyboardMapper::makeParamProperty( OSDictionary * dict, const char * key,
                                    const void * bytes, unsigned int length )
{
    OSData * data;

    data = OSData::withBytes( bytes, length );
    if( data) {
        dict->setObject( key, data);
        data->release();
    }
}

bool IOHIKeyboardMapper::updateProperties( void )
{
    bool	ok = true;

    return( ok );
}

IOReturn IOHIKeyboardMapper::setParamProperties( OSDictionary * dict )
{
    OSData *		data;
    IOReturn		err = kIOReturnSuccess;
    bool			updated = false;
    bool			turnedOff = false;
    UInt32			propertydata;

    // Check for eject delay property
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDF12EjectDelayKey)))) {
        // we know we set this as a 32 bit number
        propertydata = *((UInt32 *)(data->getBytesNoCopy()));
		
        _eject_Delay_MS = propertydata;
		
        // we changed something
        updated = true;
    } 
    
    // Check for slowKeys delay property
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDSlowKeysDelayKey)))) {
        // we know we set this as a 32 bit number
        propertydata = *((UInt32 *)(data->getBytesNoCopy()));
        
        // If we are in progess and we are turned off
        // cancel the timeout
        if ((_slowKeys_Delay_MS > 0) && !propertydata && 
            ((_slowKeys_State & kState_In_Progess_Flag) != 0))
            _slowKeysTimerEventSource->cancelTimeout();

		
        _slowKeys_Delay_MS = propertydata;
		
        // we changed something
        updated = true;
    } 

	// check for disabled property in the dictionary
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDStickyKeysDisabledKey))))
	{
		// we know we set this as a 32 bit number
        propertydata = *((UInt32 *)(data->getBytesNoCopy()));
		
		// if set, then set the bit in our state
		if (propertydata)
			_stickyKeys_State |= kState_Disabled_Flag;
		// otherwise clear the bit in our state
		else
			_stickyKeys_State &= ~kState_Disabled_Flag;
		
		// if set, then we turned off
		if (propertydata)
			turnedOff = true;
		
		// we changed something
        updated = true;
    }

	// check for on/off property in the dictionary
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDStickyKeysOnKey))))
	{
		// we know we set this as a 32 bit number
        propertydata = *((UInt32 *)(data->getBytesNoCopy()));
		
		// if set, then set the bit in our state
		if (propertydata)
			_stickyKeys_State |= kState_On;
		// otherwise clear the bit in our state
		else
			_stickyKeys_State &= ~kState_On;
		
		// if clear, then we turned off
		if (propertydata == 0)
			turnedOff = true;
		
		// we changed something
        updated = true;
    }

	// check for shift toggles property in the dictionary
    if( (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDStickyKeysShiftTogglesKey))))
	{
		// we know we set this as a 32 bit number
        propertydata = *((UInt32 *)(data->getBytesNoCopy()));
		
		// if set, then set the bit in our state
		if (propertydata)
			_stickyKeys_State |= kState_ShiftActivates_Flag;
		// otherwise clear the bit in our state
		else
			_stickyKeys_State &= ~kState_ShiftActivates_Flag;
		
		// we changed something
        updated = true;
    }
	
	// if turned off, flush some things
    if (turnedOff)
	{
            // if we were on, clear a few things going off

            // post the keyups for all the modifier keys being held
            for (int index = 0; index < _stickyKeys_NumModifiersDown; index++)
                    rawTranslateKeyCode(_stickyKeys_StuckModifiers[index].key, false, _stickyKeys_Modifier_KeyBits);

            // clear state, modifiers no longer down
            _stickyKeys_State &= ~kState_On_ModifiersDown;
            _stickyKeys_NumModifiersDown = 0;
                            
	}

	// right now updateProperties does nothing interesting
    if (updated)
        updateProperties();

    return( err );
}

// ************* Sticky Keys Functionality ****************

// stickyKeysinit
// initialize sticky keys variables
bool IOHIKeyboardMapper::stickyKeysinit( void )
{
	// default state to off, unless the UI part is installed and turns us on.
	// instead of setting shifttoggles and disabled, which would be the other way to do it
	// we will just keep all flags clear, making us off but not explicitly 'disabled'
	// the behavior is the same, but the meaning is different, we are in default state off, 
	// not user explictly setting us to off
	// NOTE: the real default ends up being set in IOHIDSystem::createParameters
	_stickyKeys_State	    		= 0;
	
	_stickyKeys_NumModifiersDown 	= 0;
		
	// allocate shift toggle struct
	_stickyKeys_ShiftToggle = stickyKeysAllocToggleInfo (kNUM_SHIFTS_TO_ACTIVATE);
	if (_stickyKeys_ShiftToggle == NULL)
		return false;
	
	// initialize shift toggle struct
	_stickyKeys_ShiftToggle->toggleModifier = NX_MODIFIERKEY_SHIFT;
	_stickyKeys_ShiftToggle->repetitionsToToggle = kNUM_SHIFTS_TO_ACTIVATE;
	clock_interval_to_absolutetime_interval( kDEFAULT_SHIFTEXPIREINTERVAL,
											kMillisecondScale, 
											&_stickyKeys_ShiftToggle->expireInterval);
	_stickyKeys_ShiftToggle->currentCount = 0;
	
	// allocate option toggle struct
	_stickyKeys_OptionToggle = stickyKeysAllocToggleInfo (kNUM_SHIFTS_TO_ACTIVATE);
	if (_stickyKeys_OptionToggle == NULL)
		return false;
	
	// initialize option toggle struct
	_stickyKeys_OptionToggle->toggleModifier = NX_MODIFIERKEY_ALTERNATE;
	_stickyKeys_OptionToggle->repetitionsToToggle = kNUM_SHIFTS_TO_ACTIVATE;
	clock_interval_to_absolutetime_interval( kDEFAULT_SHIFTEXPIREINTERVAL,
											kMillisecondScale, 
											&_stickyKeys_OptionToggle->expireInterval);
	_stickyKeys_OptionToggle->currentCount = 0;
	
	return createParamDicts();
}

// stickyKeysfree
// free sticky keys variables
void IOHIKeyboardMapper::stickyKeysfree (void)
{
	// release shift toggle struct
	if (_stickyKeys_ShiftToggle)
		stickyKeysFreeToggleInfo(_stickyKeys_ShiftToggle);

	// release option toggle struct
	if (_stickyKeys_OptionToggle)
		stickyKeysFreeToggleInfo(_stickyKeys_OptionToggle);

	// release on param dict
	if (_onParamDict)
		_onParamDict->release();

	// release off param dict
	if (_offParamDict)
		_offParamDict->release();
}

// allocate a StickyKeys_ToggleInfo struct
StickyKeys_ToggleInfo * IOHIKeyboardMapper::stickyKeysAllocToggleInfo (unsigned maxCount)
{
	StickyKeys_ToggleInfo *		toggleInfo;
	IOByteCount					size;
	
	// size should be size of the structure plus
	// the size of each entry of the array (AbsoluteTime)
	// note the struct already has room for the first entry
	size = sizeof(StickyKeys_ToggleInfo) + 
				(sizeof(AbsoluteTime) * (maxCount - 1));
	
	// allocate shift toggle struct
	toggleInfo = (StickyKeys_ToggleInfo *)
		IOMalloc (size);
	
	// set the size
	if (toggleInfo)
		toggleInfo->size = size;
	
	return toggleInfo;
}

// free a StickyKeys_ToggleInfo struct
void IOHIKeyboardMapper::stickyKeysFreeToggleInfo (StickyKeys_ToggleInfo * toggleInfo)
{
	// free shift toggle struct
	IOFree (toggleInfo, toggleInfo->size);
}

// createParamDicts
// create on/off dicts as part of init
bool IOHIKeyboardMapper::createParamDicts ( void )
{
    bool	ok = true;
	UInt32	propertydata;
	
	// create a dictionary that sets state to on
	_onParamDict = OSDictionary::withCapacity(4);
	if (_onParamDict)
	{
		propertydata = 1; // on
		makeParamProperty( _onParamDict, kIOHIDStickyKeysOnKey,
					&propertydata, sizeof(propertydata) );
	}
	else
		ok = false;
	
	// create a dictionary that sets state to off
	if (ok)
		_offParamDict = OSDictionary::withCapacity(4);
	if (_offParamDict)
	{
		propertydata = 0; // off
		makeParamProperty( _offParamDict, kIOHIDStickyKeysOnKey,
					&propertydata, sizeof(propertydata) );
	}
	else
		ok = false;

    return( ok );
}

// postKeyboardSpecialEvent
// called to post special keyboard events
// thru the event system to outside of kernel clients
void	IOHIKeyboardMapper::postKeyboardSpecialEvent (unsigned subtype)
{
	_delegate->keyboardSpecialEvent (
				/* eventType */	NX_SYSDEFINED,
				/* flags */     _delegate->eventFlags(),
				/* keyCode */   NX_NOSPECIALKEY,
				/* specialty */ subtype);
}

bool IOHIKeyboardMapper::stickyKeysModifierToggleCheck(
							StickyKeys_ToggleInfo * toggleInfo,
							UInt8        key,
							bool         keyDown,
							kbdBitVector keyBits)
{
    unsigned char 	thisBits = _parsedMapping.keyBits[key];
    int				index, innerindex;
    AbsoluteTime	now, deadline;
    bool			shouldToggle = false;

	// is this the shift key?
	if ((unsigned)(thisBits & NX_WHICHMODMASK) == toggleInfo->toggleModifier)
	{
		// get the time
		clock_get_uptime(&now);

		// prune the list of all occurences whose deadline has expired
		// start at the end, which is the most recent shift
		for (index = toggleInfo->currentCount - 1; index >= 0; index--)
			// did this item's deadline expire?
			if (AbsoluteTime_to_scalar(&now) >
				AbsoluteTime_to_scalar(&toggleInfo->deadlines[index]))
			{
				// remove this shift and all shifts that occured previously
				
				// move all newer shifts to the start
				int entries_to_delete = index + 1;

				for (innerindex = 0; innerindex < (int)(toggleInfo->currentCount - entries_to_delete); innerindex++)
					toggleInfo->deadlines[innerindex] = 
						toggleInfo->deadlines[innerindex + entries_to_delete];
				
				// update the count
				toggleInfo->currentCount -= entries_to_delete;
				
				// defensive code
				index = -1;
				
				// stop looping
				break;
			}

		// is this a keydown, if so, add it to the list
		if (keyDown)
		{
			// we will add it if we have room
			if (toggleInfo->currentCount < toggleInfo->repetitionsToToggle)
			{
				// compute a new deadline
				clock_absolutetime_interval_to_deadline(toggleInfo->expireInterval, &deadline);
				
				// add it
				toggleInfo->deadlines[toggleInfo->currentCount++] = deadline;
			}
		}
		// otherwise its a shift key up, if this the 5th one, then turn us on
		else
		{
			// is this the 5th shift
			if (toggleInfo->currentCount == toggleInfo->repetitionsToToggle)
			{
				// clear the list of shifts we are tracking
				toggleInfo->currentCount = 0;
				
				// turn us on
				shouldToggle = true;
			}
		}

	}
	// a non-shift key was used, start over timing the shift keys
	else
		toggleInfo->currentCount = 0;
	
	return shouldToggle;
}

// stickyKeysNonModifierKey
// called when a non-modifier key event occurs (up or down)
void IOHIKeyboardMapper::stickyKeysNonModifierKey(
								UInt8        key,
								bool         keyDown,
								kbdBitVector keyBits,
                                                                bool 	     mouseClick)
{
    int				index;

	// first post this non-modifier key down
        if (!mouseClick)
            rawTranslateKeyCode(key, keyDown, keyBits);
	
	// determine whether or not to post the keyups for the modifiers being held
	for (index = 0; index < _stickyKeys_NumModifiersDown; index++) {
            _stickyKeys_StuckModifiers[index].state |= kModifier_DidPerformModifiy;
            
            // Has this key been keyed up.  If not, leave the key alone
            // because the user is still holding it down.
            if (((_stickyKeys_StuckModifiers[index].state & kModifier_DidKeyUp) != 0) && 
                ((_stickyKeys_StuckModifiers[index].state & kModifier_Locked) == 0)) {
                // We keyed up ealier, so we should call stickyKeysModifierKey to
                // individually release the key
                stickyKeysModifierKey(_stickyKeys_StuckModifiers[index].key, false, keyBits);
                
                // We basically took a modifier off the list, so we have to decrement
                // our local index
                index --;
            }
        }
	
	// clear state, if modifiers no longer down
        if (_stickyKeys_NumModifiersDown == 0)
            _stickyKeys_State &= ~kState_On_ModifiersDown;
	
}

// stickyKeysModifierKey
// called when shift/command/control/option key goes down
void IOHIKeyboardMapper::stickyKeysModifierKey(
							UInt8        key,
							bool         keyDown,
							kbdBitVector keyBits)
{
    unsigned char 	thisBits = _parsedMapping.keyBits[key];
    int				index, innerindex;
	bool			isBeingHeld;
	int				heldIndex;

	// is this key being held? (if so, dont post the down)
	isBeingHeld = false;
	for (index = 0; index < _stickyKeys_NumModifiersDown; index++)
		if (_stickyKeys_StuckModifiers[index].key == key)
		{
			isBeingHeld = true;
			heldIndex = index;

			// break out of loop, modifers will not be in twice
			break;
		}

	// The following condition has been added, so that chording of key combinations can be supported
	if (! keyDown) {
                // For chording, we only care if the modifier is being held
                if (isBeingHeld)
                {
                            if (((_stickyKeys_StuckModifiers[heldIndex].state & kModifier_DidPerformModifiy) != 0) && 
                                ((_stickyKeys_StuckModifiers[heldIndex].state & kModifier_Locked) == 0)) {
                                    // This modifier keyed up, but it also modified a key.
                                    // Therefore it needs to be released.
                                    goto RELEASE_STICKY_MODIFIER_KEY;
                            } 
                            else 
                            {
                                    // set the state flag, so that stickyKeysNonModifierKey can call
                                    // back into this function to release the key.
                                    _stickyKeys_StuckModifiers[heldIndex].state |= kModifier_DidKeyUp;
                            }
                }
        }
        // we have a key down
        else 
        {                
                // is this key being held? (if so, stop holding it - toggle off)
		if (isBeingHeld)
		{

                    // If this key has been locked, then it needs to be release
                    // The third and final state of the modifier key
                    if ((_stickyKeys_StuckModifiers[heldIndex].state & kModifier_Locked) != 0)
                    { 
RELEASE_STICKY_MODIFIER_KEY:
			// stop holding this key down
			
			// post the key up 
			rawTranslateKeyCode(key, false, keyBits);

			// clear this one (handles the case this is the last key held)
			_stickyKeys_StuckModifiers[heldIndex].key = 0;
                        _stickyKeys_StuckModifiers[heldIndex].state = 0;
			
			// reduce our global count
			// (do this before the loop, so loop does not overrun)
			_stickyKeys_NumModifiersDown--;
			
			// if no more keys being held, clear our state
			if (_stickyKeys_NumModifiersDown == 0)
				_stickyKeys_State &= ~kState_On_ModifiersDown;

			// move each item after this one forward
			// note, _stickyKeys_NumModifiersDown already decremented
			for (innerindex = heldIndex; innerindex < _stickyKeys_NumModifiersDown; innerindex++) {
				_stickyKeys_StuckModifiers[innerindex].key = _stickyKeys_StuckModifiers[innerindex + 1].key;
                                _stickyKeys_StuckModifiers[innerindex].state = _stickyKeys_StuckModifiers[innerindex + 1].state;
                        }
                        
                        // notify the world we changed
                        // note, we send a new event every time the user releses the modifier
                        // while we are still holding modifiers. Clients must know to expect this
                        if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_SHIFT)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_SHIFT_UP);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_CONTROL)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_CONTROL_UP);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_ALTERNATE)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_ALTERNATE_UP);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_COMMAND)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_COMMAND_UP);

                    }
                    
                    // This is the second press, therefore this key needs to be locked.
                    else 
                    {
                        _stickyKeys_StuckModifiers[heldIndex].state |= kModifier_Locked;
                        
                        // notify the world we changed
                        // note, we send a new event every time the user releses the modifier
                        // while we are still holding modifiers. Clients must know to expect this
                        if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_SHIFT)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_SHIFT_LOCK);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_CONTROL)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_CONTROL_LOCK);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_ALTERNATE)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_ALTERNATE_LOCK);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_COMMAND)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_COMMAND_LOCK);
                    }
                }
                
                // if this key is not being held already, then post the modifier down
		else
		{
                        // cache the keyBits.  this will be used for clearing
                        _stickyKeys_Modifier_KeyBits = keyBits;
                        rawTranslateKeyCode(key, keyDown, keyBits);
			
			// and remember it is down
			if (_stickyKeys_NumModifiersDown < kMAX_MODIFIERS) {
                                int modifierIndex = _stickyKeys_NumModifiersDown++;
				_stickyKeys_StuckModifiers[modifierIndex].key = key;
                                _stickyKeys_StuckModifiers[modifierIndex].state = 0;
                        }
			else
				;	// add a system log error here?
	
			// change our state
			_stickyKeys_State |= kState_On_ModifiersDown;
                        
                                        
                        // notify the world we changed
                        // note, we send a new event every time the user releses the modifier
                        // while we are still holding modifiers. Clients must know to expect this
                        if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_SHIFT)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_SHIFT_DOWN);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_CONTROL)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_CONTROL_DOWN);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_ALTERNATE)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_ALTERNATE_DOWN);
                        else if ((thisBits & NX_WHICHMODMASK) == NX_MODIFIERKEY_COMMAND)
                                postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_COMMAND_DOWN);
		}
	}
}


// stickyKeysFilterKey 
// returns true if this key event should be ignored
// It may call rawTranslateKeyCode multiple times to generate keyups
// at the end of a sticky keys sequence
// this function is the essence of the sticky keys feature

bool IOHIKeyboardMapper::stickyKeysFilterKey(
							UInt8        key,
							bool         keyDown,
							kbdBitVector keyBits)
{
    unsigned char 	thisBits = _parsedMapping.keyBits[key];
    bool			shouldFilter = false;
    int				index;
    bool			shouldToggleState = false;
	
    // if we are disabled, then do nothing
    if ((_stickyKeys_State & kState_Disabled_Flag) != 0)
        return false;
    
	// check to see if option key toggle activated
    // only do so if the shift toggles bit is set
	// we could have a seperate bit to set whether option
	// should send the event, but we dont. Since the UI
	// uses these to be one in the same, we will as well.
    if ((_stickyKeys_State & kState_ShiftActivates_Flag) != 0) 
    {
                // check to see if shift key pressed, possible to toggle state
                shouldToggleState = stickyKeysModifierToggleCheck 
                                            (_stickyKeys_ShiftToggle, key, keyDown, keyBits);
                
		// if the option was toggled
		if (stickyKeysModifierToggleCheck (_stickyKeys_OptionToggle,
											key, keyDown, keyBits))
			// if so, send the event to toggle mouse driving
			postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_TOGGLEMOUSEDRIVING);
    }
            	
    // if we are on and holding modifier keys and we have a key down, finish if this is not modifier
    if (((_stickyKeys_State & kState_On_ModifiersDown) != 0)  && keyDown)
    {
        // Make sure that this is not a key and not a modifier that we track
        // This will allow us to pass through other modifiers like the arrow keys
        if (! modifierOfInterest(thisBits))
        {
            // we will handle all the events here, so dont process this one normally
            shouldFilter = true;
            
			// handle non-modifer key
			stickyKeysNonModifierKey (key, keyDown, keyBits);
        }
    }

    // if we are on and looking for modifier keys, see if this is one
    if ((_stickyKeys_State & kState_On) != 0)
    {
        // is this a sticky keys modifier key?
        // is this a modifier?
        if (modifierOfInterest(thisBits))
        {
            // we will handle all the events here, so dont process this one normally
            shouldFilter = true;
            
            // is this a key down
			stickyKeysModifierKey (key, keyDown, keyBits);
        }
    }
    
    // if we are supposed to toggle our state, do it now
    if (shouldToggleState)
    {
        // if we were on, clear a few things going off
        if ((_stickyKeys_State & kState_On) != 0)
        {
            // post the keyups for all the modifier keys being held
            for (index = 0; index < _stickyKeys_NumModifiersDown; index++)
                rawTranslateKeyCode(_stickyKeys_StuckModifiers[index].key, false, keyBits);

            // clear state, modifiers no longer down
            _stickyKeys_State &= ~kState_On_ModifiersDown;
            _stickyKeys_NumModifiersDown = 0;
        }
        
        // toggle our state
        _stickyKeys_State ^= kState_On;
		
		// remember we changed
		_stateDirty = true;
		
		// notify the world we changed
		// are we now on?
        if ((_stickyKeys_State & kState_On) != 0)
			postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_ON);
		// else, we are now off
		else
			postKeyboardSpecialEvent (NX_SUBTYPE_STICKYKEYS_OFF);
    }
    
    return shouldFilter;
}

void IOHIKeyboardMapper::keyEventPostProcess (void)
{
	bool			nowOn;
	OSDictionary * 	dict;
	
	// if the state changed
	if (_stateDirty)
	{
		// if we have a valid IOHIDSystem
		if (_hidSystem)
		{
			// are we now on?
			nowOn = ((_stickyKeys_State & kState_On) != 0);
			
			// choose the proper dictionary
			dict = nowOn ? _onParamDict : _offParamDict;
			
			// set it
			_hidSystem->setParamProperties (dict);
		}
		
		// no longer dirty
		// (the case of a NULL _hidSystem should not happen, so
		//  no point in maintaining a dirty state till one shows up)
		_stateDirty = false;
	}

}



// F12 Eject member functions

// f12EjectFilterKey
// This function will determine whether or not f12 was pressed.
// If held down for a predetermined amount of time and eject
// notification will be issued.  If key release early, this method
// will call rawTranslateKeyCode for keyDown and return false.

bool IOHIKeyboardMapper::f12EjectFilterKey (UInt8 key, bool keyDown, kbdBitVector keyBits) {

    // Check the delay time.  If 0, then the feature is off.
    if (_eject_Delay_MS == 0)
        return false;
    
    // Let's check to see if this is an ApplePro Keyboard.
    // These are the gest codes defined in AppleUSBKeyboard.h :
    // Portable2001 ANSI = 0xca = 202 d
    // Portable2001 ISO  = 0xcb = 203 d
    // Andy ANSI	 = 0xcc = 204 d  |  0x204 = 516 d = USB Product ID
    // Andy ISO  	 = 0xcd = 205 d  |  0x205 = 517 d = USB Product ID
    // Andy JIS  	 = 0xce = 206 d  |  0x206 = 518 d = USB Product ID
    // Portable2001 JIS  = 0xcf = 207 d
    // Q6 ANSI	 	 = 0x1f = 31 d  |  0x208 = 520 d = USB Product ID
    // Q6 ISO  	 	 = 0x20 = 32 d  |  0x209 = 521 d = USB Product ID
    // Q6 JIS  	 	 = 0x21 = 33 d  |  0x20a = 522 d = USB Product ID
    // Q30 ANSI	 	 = 0x22 = 34 d  |  0x20b = 523 d = USB Product ID
    // Q30 ISO   	 = 0x23 = 35 d  |  0x20c = 524 d = USB Product ID
    // Q30 JIS	 	 = 0x24 = 36 d  |  0x20d = 525 d = USB Product ID


    if (((_delegate->deviceType() >= 0xca) && (_delegate->deviceType() <= 0xcf)) || 
        ((_delegate->deviceType() >= 0x1f) && (_delegate->deviceType() <= 0x24)))
        return false;
        
    // We will be using ADB key codes, so we need to make sure we an ADB Keyboard.
    // No need to worry, USB Keyboard drivers trick IOHIDSystem into thinking
    // that they are ADB Keyboards.  This might be a problem when we introduce
    // PS2 in the mix.  Worry about that when it comes.
    if (_delegate->interfaceID() != NX_EVS_DEVICE_INTERFACE_ADB)
        return false;
    
    
    if (key == kADB_KEYBOARD_F12) {
        
        // Let's check to see if an IOTimerEventSource exists.
        // If not, create one.
        if (!_ejectTimerEventSource) {
            // Make sure we have an instance of _hidSystem.
            if (_hidSystem == NULL)
                return false;

            _ejectTimerEventSource = IOTimerEventSource::timerEventSource
                                        (this, (IOTimerEventSource::Action) &IOHIKeyboardMapper::performF12Eject);
                                     
            if (_hidSystem->getWorkLoop()->addEventSource(_ejectTimerEventSource) != kIOReturnSuccess)
                return false;
    
        }    
         
        if (keyDown == true) {
            // Set the time out and the flag
            _f12Eject_State |= kState_In_Progess_Flag;
            
            _ejectTimerEventSource->setTimeoutMS(_eject_Delay_MS);
            
            return true;  // prevent processing of f12
            
        } else {
            
            // If the user pulled out early, send a key down event.
            // Since we return false, the system will take care of the
            // key up.
            if ((_f12Eject_State & kState_In_Progess_Flag) != 0) {
                
                _ejectTimerEventSource->cancelTimeout();

                _f12Eject_State &= ~kState_In_Progess_Flag;
                                
                rawTranslateKeyCode (key, true, keyBits);
            }
            
            // If we get to this point, the eject happened.
            // Therefore we should ignore the key up.
            else
                return true;

        }
    } 
    
    // I think we should process all other key events during this check.
    // That is why I'm returning false;
        
    return false;
    
}

// performF12Eject
// This is a static method called by the ejectTimerEventSource.
// It will send an System eject event

void IOHIKeyboardMapper::performF12Eject(IOHIKeyboardMapper *owner, IOTimerEventSource *sender) { 
   
    // Post the eject keydown event.
    owner->postKeyboardSpecialEvent(NX_SUBTYPE_EJECT_KEY);
    
    // Clear the state
    owner->_f12Eject_State &= ~kState_In_Progess_Flag;
    
}

// Slowkeys member functions

// slowKeysFilterKey
// This function will determine whether or not a a key need to be
// processed for the slow keys feature.  If so, if a key is held
// down for a predetermined amount of time, a key down will be
// pushed on up to the HID System.  Other scenarios to the state
// machine are documented in further detail below.

bool IOHIKeyboardMapper::slowKeysFilterKey (UInt8 key, bool keyDown, kbdBitVector keyBits) {
    bool returnValue = true;

    if (_slowKeys_Delay_MS == 0)
        return false;  
            
    // Let's check to see if an IOTimerEventSource exists.
    // If not, create one.
    if (!_slowKeysTimerEventSource) {
                    
        // Make sure we have an instance of _hidSystem.
        if (_hidSystem == NULL)
            return false;
    
        _slowKeysTimerEventSource = IOTimerEventSource::timerEventSource
                                    (this, (IOTimerEventSource::Action) &IOHIKeyboardMapper::slowKeysPostProcess );
                                    
        
        if(_hidSystem->getWorkLoop()->addEventSource(_slowKeysTimerEventSource) != kIOReturnSuccess)
            return false;

    }

    
    if (keyDown) {
        // Ladies and Gentlemen start your engines
        if ((_slowKeys_State & kState_In_Progess_Flag) == 0) {
                
            // Check to see if we are currently handling a repeated key.
            // If so, and the key pressed differs from the key that is 
            // being repeated post the key up for that repeated key and
            // clear the flag.
            if ((key != _slowKeys_Current_Key) && ((_slowKeys_State & kState_Is_Repeat_Flag) != 0)) {
                
                postSlowKeyTranslateKeyCode(this, _slowKeys_Current_Key, false, _slowKeys_Current_KeyBits);
                
                _slowKeys_State &= ~kState_Is_Repeat_Flag;
            }
            
            // Start the normal processing.
                    
            _slowKeys_State |= kState_In_Progess_Flag;
            
            _slowKeys_Current_Key = key;
            _slowKeys_Current_KeyBits = keyBits;
            
            _slowKeysTimerEventSource->setTimeoutMS(_slowKeys_Delay_MS);
            
            // Set a state flag telling us that the key is being repeated.
            if (_delegate->isRepeat())
                _slowKeys_State |= kState_Is_Repeat_Flag;

            // Notify System that this is the start
            postKeyboardSpecialEvent(NX_SUBTYPE_SLOWKEYS_START);
        }
        
        // if another key goes down while in progress, start abort process 
        else if (((_slowKeys_State & kState_In_Progess_Flag) != 0) && (key != _slowKeys_Current_Key)) {
        
            _slowKeysTimerEventSource->cancelTimeout();
            
            _slowKeys_State |= kState_Aborted_Flag;
            _slowKeys_State &= ~kState_In_Progess_Flag;
            
            _slowKeys_Aborted_Key = key;
        
            // If the slow key is being repeated, send a key up 
            // for that key and clear the flag
            if ((_slowKeys_State & kState_Is_Repeat_Flag) != 0) {
            
                postSlowKeyTranslateKeyCode(this, _slowKeys_Current_Key, false, _slowKeys_Current_KeyBits);
                
                _slowKeys_State &= ~kState_Is_Repeat_Flag;
            }
            
            // Notify System that this is an abort
            postKeyboardSpecialEvent(NX_SUBTYPE_SLOWKEYS_ABORT);
        }
    } 
    
    // handing for a key up
    else {
    
        if (key == _slowKeys_Current_Key) {
            
            // If the current key come up while in progress, kill it
            if ((_slowKeys_State & kState_In_Progess_Flag) != 0) {
            
                _slowKeysTimerEventSource->cancelTimeout();
                _slowKeys_State &= ~kState_In_Progess_Flag;
                
                // If the key is being repeated, pass the key up through
                // and clear the flag
                if ((_slowKeys_State & kState_Is_Repeat_Flag) != 0) {
                    
                    _slowKeys_State &= ~kState_Is_Repeat_Flag;

                    returnValue = false;
                }
            }
            
            // Otherwise, if the key was not aborted, pass the key up through
            else if ((_slowKeys_State & kState_Aborted_Flag) == 0) {
            
                // Clear the flag if this was a repeated key
                if ((_slowKeys_State & kState_Is_Repeat_Flag) != 0)
                    _slowKeys_State &= ~kState_Is_Repeat_Flag;
                    
                returnValue = false;
            }
        }
        
        // If the key that caused an abort comes up, it will kill any current slowkeys action
        else if ((key == _slowKeys_Aborted_Key) && ((_slowKeys_State & kState_Aborted_Flag) != 0)){

            _slowKeysTimerEventSource->cancelTimeout();
            
            _slowKeys_State &= ~kState_Aborted_Flag;
            _slowKeys_State &= ~kState_In_Progess_Flag;
                        
            // If the slow key is being repeated, send a key up for the slow key
            // and clear the flag
            if ((_slowKeys_State & kState_Is_Repeat_Flag) != 0) {
            
                postSlowKeyTranslateKeyCode(this, _slowKeys_Current_Key, false, _slowKeys_Current_KeyBits);
                
                _slowKeys_State &= ~kState_Is_Repeat_Flag;
            }
            
            // Notify System that this is an abort
            postKeyboardSpecialEvent(NX_SUBTYPE_SLOWKEYS_ABORT);
        }
        
        // This key has already been processed/  Pass the key up through
        else {
        
            returnValue = false;
        }
    } 
    
    return returnValue;
}

// slowKeysPostProcess

// This is a IOTimerEventSource::Action.
// It is responsible for sending a key down
// to the HID System. 

void IOHIKeyboardMapper::slowKeysPostProcess (IOHIKeyboardMapper *owner, IOTimerEventSource *sender) {
    
    owner->_slowKeys_State &= ~kState_In_Progess_Flag;

    // Post the key down
    postSlowKeyTranslateKeyCode(owner, owner->_slowKeys_Current_Key, true, owner->_slowKeys_Current_KeyBits);
                
    // Notify System that this is the end
    owner->postKeyboardSpecialEvent(NX_SUBTYPE_SLOWKEYS_END);
} 

void IOHIKeyboardMapper::stickyKeysMouseDown(IOHIKeyboardMapper *owner, IOEventSource *sender)
{
    owner->stickyKeysNonModifierKey (0, 0, owner->_reserved->stickyKeys_Modifier_KeyBits, true);
}

OSMetaClassDefineReservedUsed(IOHIKeyboardMapper,  0);
IOReturn IOHIKeyboardMapper::message( UInt32 type, IOService * provider, void * argument)
{
    
    switch (type)
    {
        case kIOHIDSystem508MouseClickMessage:
            if ((_stickyKeys_State & kState_On) != 0)
            {
                if (!_stickyKeysMouseClickEventSource)
                {
                    _stickyKeysMouseClickEventSource = IOInterruptEventSource::interruptEventSource
                        (this, (IOInterruptEventSource::Action) stickyKeysMouseDown);
                                    
                    if(_stickyKeysMouseClickEventSource &&
                        _hidSystem && 
        (_hidSystem->getWorkLoop()->addEventSource(_stickyKeysMouseClickEventSource) != kIOReturnSuccess)) {
                        _stickyKeysMouseClickEventSource->release();
                        _stickyKeysMouseClickEventSource = 0;
                    }
                }
                
                // Since we most likely running via IOHIDSystem::cmdGate runAction,
                // we should really trigger an interrupt to run this later on the
                // workloop.  This will also avoid any synchronization anomolies.
                if (_stickyKeysMouseClickEventSource)
                    _stickyKeysMouseClickEventSource->interruptOccurred(0, 0, 0);
                        
            }
            break;
            
        default:
            break;
    }
    return kIOReturnSuccess;
}
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  1);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  2);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  3);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  4);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  5);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  6);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  7);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  8);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper,  9);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 10);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 11);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 12);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 13);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 14);
OSMetaClassDefineReservedUnused(IOHIKeyboardMapper, 15);
