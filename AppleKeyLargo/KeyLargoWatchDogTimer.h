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

#ifndef _IOKIT_KEYLARGOWATCHDOGTIMER_H
#define _IOKIT_KEYLARGOWATCHDOGTIMER_H

#include <IOKit/system_management/IOWatchDogTimer.h>

#ifndef _IOKIT_KEYLARGO_H
#include "KeyLargo.h"
#endif

class KeyLargo; 			// forward declaration

class KeyLargoWatchDogTimer : public IOWatchDogTimer
{
	OSDeclareDefaultStructors(KeyLargoWatchDogTimer);
  
private:
	KeyLargo *keyLargo;
  
public:
	static KeyLargoWatchDogTimer *withKeyLargo(KeyLargo *keyLargo);
	virtual bool start(IOService *provider);
	virtual void setWatchDogTimer(UInt32 timeOut);
};

#endif // _IOKIT_KEYLARGOWATCHDOGTIMER_H
