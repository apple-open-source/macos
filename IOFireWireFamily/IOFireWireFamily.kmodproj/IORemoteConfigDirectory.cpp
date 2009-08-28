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
#import <IOKit/firewire/IORemoteConfigDirectory.h>
#import <IOKit/firewire/IOFireWireDevice.h>

// private
#import "FWDebugging.h"

// system
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSData.h>

OSDefineMetaClassAndStructors(IORemoteConfigDirectory, IOConfigDirectory)
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 0);
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 1);
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 2);

// initWithOwnerOffset
//
//

bool
IORemoteConfigDirectory::initWithOwnerOffset( IOFireWireROMCache *rom,
                         int start, int type)
{

    // Do this first so that init can load ROM
    fROM = rom;
    fROM->retain();

    if( !IOConfigDirectory::initWithOffset(start, type) ) 
	{
		fROM->release();
		fROM = NULL;
		
        return false;       
    }

    return true;
}

// free
//
//

void
IORemoteConfigDirectory::free()
{
    if(fROM)
        fROM->release();
    IOConfigDirectory::free();

}

// withOwnerOffset
//
//

IOConfigDirectory *
IORemoteConfigDirectory::withOwnerOffset( IOFireWireROMCache *rom,
                                           int start, int type)
{
    IORemoteConfigDirectory *dir;

    dir = OSTypeAlloc( IORemoteConfigDirectory );
    if( !dir )
        return NULL;

    if( !dir->initWithOwnerOffset(rom, start, type) ) 
	{
        dir->release();
        dir = NULL;
    }
    return dir;
}

// getBase
//
//

const UInt32 *IORemoteConfigDirectory::getBase()
{
    return ((const UInt32 *)fROM->getBytesNoCopy())+fStart+1;
}

// update
//
//

IOReturn IORemoteConfigDirectory::update(UInt32 offset, const UInt32 *&romBase)
{
	// unsupported
	
	return kIOReturnError;
}

IOConfigDirectory *
IORemoteConfigDirectory::getSubDir(int start, int type)
{
    return withOwnerOffset(fROM, start, type);
}

// lockData
//
//

const UInt32 * IORemoteConfigDirectory::lockData( void )
{
	fROM->lock();
	return (UInt32 *)fROM->getBytesNoCopy();
}

// unlockData
//
//

void IORemoteConfigDirectory::unlockData( void )
{
	fROM->unlock();
}

// updateROMCache
//
//

IOReturn IORemoteConfigDirectory::updateROMCache( UInt32 offset, UInt32 length )
{
	return fROM->updateROMCache( offset, length );
}

// checkROMState
//
//

IOReturn IORemoteConfigDirectory::checkROMState( void )
{
	return fROM->checkROMState();
}
