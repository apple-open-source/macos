/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include "AppleK2Driver.h"
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>


#define super IOService

OSDefineMetaClassAndStructors (AppleK2Driver, IOService);

IOService *AppleK2Driver::probe (IOService *provider, SInt32 *score)
{
	OSIterator *iterator;
	bool ok = 0;

    // Set our probe score -ve to be below drivers with no IOProbeScore
    *score = -1000;
	// Only need to attach to providers with children...

	iterator = provider->getChildIterator(gIODTPlane);
	if (iterator)
	{
		if (iterator->getNextObject())
			ok = 1;
		iterator->release();
	}

	if (ok) // let others match with a higher probe score than our personality specifies.
		return this;

	return false;
}

// Start method will publish children...
bool AppleK2Driver::start( IOService * provider)
{
	if (super::start (provider) != true)
		return false;

	// Let the MacIO driver publish our children..
	if (kIOReturnSuccess != callPlatformFunction("mac-io-publishChildren", 0,
			(void *)this, (void *)0, (void *)0, (void *)0))
	{
		return false;
	}

	return true;
}
