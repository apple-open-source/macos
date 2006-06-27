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

// public
#import <IOKit/firewire/IOConfigDirectory.h>
#import <IOKit/firewire/IOFireWireDevice.h>

// private
#import "FWDebugging.h"
#import "IOConfigDirectoryIterator.h"

// system
#import <libkern/c++/OSIterator.h>

OSDefineMetaClassAndStructors(IOConfigDirectoryIterator, OSIterator)

// init
//
//

IOReturn IOConfigDirectoryIterator::init(IOConfigDirectory *owner,
                                  		 UInt32 testVal, UInt32 testMask)
{
	IOReturn status = kIOReturnSuccess;
	
    if( !OSIterator::init() )
        status = kIOReturnError;
	
	if( status == kIOReturnSuccess )
	{
		fDirectorySet = OSSet::withCapacity(2);
		if( fDirectorySet == NULL )
			status = kIOReturnNoMemory;
	}
	
	int position = 0;
	while( status == kIOReturnSuccess && position < owner->getNumEntries() ) 
	{
		UInt32 value;
		IOConfigDirectory * next;
		
		status = owner->getIndexEntry( position, value );
		if( status == kIOReturnSuccess && (value & testMask) == testVal ) 
		{
			status = owner->getIndexValue( position, next );
			if( status == kIOReturnSuccess )
			{
				fDirectorySet->setObject( next );
				next->release();
			}
		}
		
		position++;
	}
    
	if( status == kIOReturnSuccess )
	{
		fDirectoryIterator = OSCollectionIterator::withCollection( fDirectorySet );
		if( fDirectoryIterator == NULL )
			status = kIOReturnNoMemory;
	}
	
    return status;
}

// free
//
//

void IOConfigDirectoryIterator::free()
{
	if( fDirectoryIterator != NULL )
	{
		fDirectoryIterator->release();
		fDirectoryIterator = NULL;
	}
	
	if( fDirectorySet != NULL )
	{
		fDirectorySet->release();
		fDirectorySet = NULL;
	}
		
    OSIterator::free();
}

// reset
//
//

void IOConfigDirectoryIterator::reset()
{
    fDirectoryIterator->reset();
}

// isValid
//
//

bool IOConfigDirectoryIterator::isValid()
{
    return true;
}

// getNextObject
//
//

OSObject *IOConfigDirectoryIterator::getNextObject()
{
	return fDirectoryIterator->getNextObject();
}
