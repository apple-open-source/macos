/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#include "KeyLargoWatchDogTimer.h"

#define super IOWatchDogTimer

OSDefineMetaClassAndStructors(KeyLargoWatchDogTimer, IOWatchDogTimer);


KeyLargoWatchDogTimer *KeyLargoWatchDogTimer::withKeyLargo(KeyLargo *keyLargo)
{
	KeyLargoWatchDogTimer *watchDogTimer = new KeyLargoWatchDogTimer;
  
	if (watchDogTimer == 0) return 0;
  
	while (1) {
		if (!watchDogTimer->init()) break;
    
		watchDogTimer->attach(keyLargo);
    
		if (watchDogTimer->start(keyLargo)) return watchDogTimer;
    
		watchDogTimer->detach(keyLargo);
		break;
	}
  
	return 0;
}

bool KeyLargoWatchDogTimer::start(IOService *provider)
{
	keyLargo = OSDynamicCast(KeyLargo, provider);
	if (keyLargo == 0) return false;
  
	return super::start(provider);
}

void KeyLargoWatchDogTimer::setWatchDogTimer(UInt32 timeOut)
{
	UInt32 timeLow, timeHigh, watchDogLow, watchDogHigh;
	UInt64 offset, time;
  
    if(fSleeping)
        return;	// Don't touch the watchdog timer while the system is sleeping or dozing
        
	if (timeOut != 0) {
		offset = (UInt64)timeOut * kKeyLargoGTimerFreq;
    
		// Read low 32-bits (latches high 32-bits)
		timeLow = keyLargo->readRegUInt32(kKeyLargoCounterLoOffset);
		timeHigh = keyLargo->readRegUInt32(kKeyLargoCounterHiOffset);

		time = (((UInt64)timeHigh) << 32) + timeLow;
    
		time += offset;
    
		watchDogLow = time & 0x0FFFFFFFFULL;
		watchDogHigh = time >> 32;
    
		keyLargo->writeRegUInt32(kKeyLargoWatchDogLowOffset, watchDogLow);
		keyLargo->writeRegUInt32(kKeyLargoWatchDogHighOffset, watchDogHigh);
	}
  
	keyLargo->writeRegUInt32(kKeyLargoWatchDogEnableOffset, (timeOut != 0) ? 1 : 0);
	
	return;
}

void KeyLargoWatchDogTimer::setSleeping(bool val)
{
    // If we're sleeping disable the watchdog
    if(val)
        keyLargo->writeRegUInt32(kKeyLargoWatchDogEnableOffset, 0);
        
    fSleeping = val;
}

