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

#include "FWDebugging.h"

#include "IOFireWireROMCache.h"

#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>

OSDefineMetaClassAndStructors(IOFireWireROMCache, OSObject)
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 0);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 1);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 2);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 3);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 4);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 5);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 6);
OSMetaClassDefineReservedUnused(IOFireWireROMCache, 7);

// withBytes
//
//

IOFireWireROMCache * IOFireWireROMCache::withOwnerAndBytes( IOFireWireDevice *owner, const void *bytes, unsigned int inLength, UInt32 generation )
{
    IOFireWireROMCache *me = new IOFireWireROMCache;

    if( me && !me->initWithOwnerAndBytes( owner, bytes, inLength, generation ) ) 
	{
        me->free();
        return 0;
    }
	
    return me;
}

// initWithBytes
//
//

bool IOFireWireROMCache::initWithOwnerAndBytes( IOFireWireDevice *owner, const void *bytes, unsigned int inLength, UInt32 generation )
{
	bool result = true;
	
	fOwner = owner;
	
	if( result )
	{
		fLock = IORecursiveLockAlloc();
		if( fLock == NULL )
			result = false;
	}
	
	if( result )
	{	
		fROM = OSData::withBytes( bytes, inLength );	
		if( fROM == NULL )
			result = false;
	}
	
	setROMState( kROMStateResumed, generation );
	
	FWKLOG(( "IOFireWireROMCache@0x%08lx::initWithOwnerAndBytes created ROM cache\n", (UInt32)this ));
	
    return result;
}

// free
//
//

void IOFireWireROMCache::free()
{
	FWKLOG(( "IOFireWireROMCache@0x%08lx::free()\n", (UInt32)this ));

	if( fROM != NULL )
	{
		fROM->release();
		fROM = NULL;
	}
	
	if( fLock != NULL )
	{
		IORecursiveLockFree( fLock );
		fLock = NULL;
	}
	
	OSObject::free();
}

// getLength
//
//

unsigned int IOFireWireROMCache::getLength()
{
	unsigned int result;
	
	lock();
	result = fROM->getLength();
	unlock();
	
	return result;
}

// ensureCapacity
//
//

unsigned int IOFireWireROMCache::ensureCapacity( unsigned int newCapacity )
{
	unsigned int result;
	
	lock();
	result = fROM->ensureCapacity( newCapacity );
	unlock();
	
	return result;
}

// appendBytes
//
//

bool IOFireWireROMCache::appendBytes( const void *bytes, unsigned int inLength )
{
	bool result;
	
	lock();
	result = fROM->appendBytes( bytes, inLength );
	unlock();
	
	return result;
}

// appendBytes
//
//

bool IOFireWireROMCache::appendBytes( const OSData *other )
{
	bool result;
	
	lock();
	result = fROM->appendBytes( other );
	unlock();

	return result;
}

// getBytesNoCopy
//
//

const void * IOFireWireROMCache::getBytesNoCopy()
{
	const void * result;
	
	lock();
	result = fROM->getBytesNoCopy();
	unlock();

//	IOLog( "IOFireWireROMCache::getBytesNoCopy = 0x%08lx\n", (UInt32)result );
	
	return result;
}

// getBytesNoCopy
//
//

const void * IOFireWireROMCache::getBytesNoCopy( unsigned int start, unsigned int inLength )
{
	const void * result;
	
	lock();
	result = fROM->getBytesNoCopy( start, inLength );
	unlock();

//	IOLog( "IOFireWireROMCache::getBytesNoCopy(start,length) = 0x%08lx\n", (UInt32)result );
	
	return result;
}

// lock
//
//

void IOFireWireROMCache::lock( void )
{
//	IOLog( "IOFireWireROMCache::lock entered\n" );

	IORecursiveLockLock(fLock);
	
//	IOLog( "IOFireWireROMCache::lock exited\n" );
}

// unlock
//
//

void IOFireWireROMCache::unlock( void )
{

//	IOLog( "IOFireWireROMCache::unlock entered\n" );

    IORecursiveLockUnlock(fLock);

//	IOLog( "IOFireWireROMCache::unlock exited\n" );
}

// hasROMChanged
//
//

bool IOFireWireROMCache::hasROMChanged( const UInt32 * newBIB, UInt32 newBIBSize )
{
	bool rom_changed = true;
	
	lock();
	
	// the ROM generation is in the Bus Info Block, so our bcmp here
	// will fail if the generation has changed.
	
	// ROM generation = 0 means you can't assume the ROM is the same,
	// however since the only real world devices whose ROM will change
	// are going to be 1394a or later we assume all generation zero ROMs
	// are unchanging
	
	// we compare using the incoming BIB size instead of just assuming 20
	// bytes as minimal config ROM devices do not have a full BIB

	if( newBIBSize == getLength() || 
		(newBIBSize == 20 && newBIBSize < getLength()) )
	{
		if(	!bcmp( newBIB, getBytesNoCopy(), newBIBSize) ) 
		{
			rom_changed = false;
		}
	}

	// don't come back from an invalid state
	if( fState == kROMStateInvalid )
	{
		rom_changed = true;
	}
	
#if 0
	if( rom_changed )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::hasROMChanged - ROM changed\n", (UInt32)this ));
	}
	else
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::hasROMChanged - ROM unchanged\n", (UInt32)this ));
	}
#endif

	unlock();
	
	return rom_changed;
}

// checkROMState
//
//

IOReturn IOFireWireROMCache::checkROMState( UInt32 &generation )
{
	IOReturn status = kIOReturnSuccess;
	
	FWKLOGASSERT( fOwner->getController()->inGate() == false );
	
	lock();
	
	while( fState == kROMStateSuspended )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState fROMState == kROMStateSuspended sleep thread 0x%08lx\n", 
																	(UInt32)this, (UInt32)IOThreadSelf() ));
		IORecursiveLockSleep( fLock, &fState, THREAD_UNINT );
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState fROMState != kROMStateSuspended - wake thread 0x%08lx\n", 
																	(UInt32)this, (UInt32)IOThreadSelf() ));
	}
	
	FWKLOGASSERT( fState == kROMStateInvalid || fState == kROMStateResumed );
	
	if( fState == kROMStateInvalid )
	{
		status = kIOFireWireConfigROMInvalid;
	}
	else if( fState == kROMStateResumed )
	{
		status = kIOReturnSuccess;
	}
	
	generation = fGeneration;
		
#if 0
	if( status == kROMStateInvalid )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState(generation) return kIOFireWireConfigROMInvalid\n", (UInt32)this ));
	}
	else
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState(generation) return kIOReturnSuccess\n", (UInt32)this ));
	}
#endif

	unlock();

	return status;
}

// checkROMState
//
//

IOReturn IOFireWireROMCache::checkROMState( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWKLOGASSERT( fOwner->getController()->inGate() == false );
	
	lock();
	
	while( fState == kROMStateSuspended )
	{
		//zzz THREAD_UNINT ?
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState fROMState == kROMStateSuspended sleep thread 0x%08lx\n", 
																	(UInt32)this, (UInt32)IOThreadSelf() ));
		IORecursiveLockSleep( fLock, &fState, THREAD_UNINT );
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState fROMState != kROMStateSuspended - wake thread 0x%08lx\n", 
																	(UInt32)this, (UInt32)IOThreadSelf() ));

	}
	
	FWKLOGASSERT( fState == kROMStateInvalid || fState == kROMStateResumed );
	
	if( fState == kROMStateInvalid )
	{
		status = kIOFireWireConfigROMInvalid;
	}
	else if( fState == kROMStateResumed )
	{
		status = kIOReturnSuccess;
	}

#if 0
	if( status == kROMStateInvalid )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState return kIOFireWireConfigROMInvalid\n", (UInt32)this ));
	}
	else
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::checkROMState return kIOReturnSuccess\n", (UInt32)this ));
	}
#endif
		
	unlock();
	
	return status;
}

// setROMState
//
//

void IOFireWireROMCache::setROMState( ROMState state, UInt32 generation )
{
	lock();
	
#if 0
	if( state == kROMStateResumed )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::setROMState - kROMStateResumed, gen = %ld\n", (UInt32)this, generation ));
	}
	else if( state == kROMStateSuspended )
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::setROMState - kROMStateSuspended\n", (UInt32)this ));
	}
	else
	{
		FWKLOG(( "IOFireWireROMCache@0x%08lx::setROMState - kROMStateInvalid\n", (UInt32)this ));
	}
#endif

	fState = state;
	
	if( fState == kROMStateResumed )
	{
		fGeneration = generation;
	}
	
	if( fState == kROMStateResumed || fState == kROMStateInvalid )
	{
		unlock();

//		FWKLOG(( "IOFireWireROMCache@0x%08lx::setROMState not kROMStateSuspended signal wake from thread 0x%08lx\n", (UInt32)this, (UInt32)IOThreadSelf() ));

		IORecursiveLockWakeup( fLock, &fState, false );
	}
	else
	{
		unlock();
	}
	
}
	
// updateROMCache
//
//

IOReturn IOFireWireROMCache::updateROMCache( UInt32 offset, UInt32 length )
{
    IOReturn status = kIOReturnSuccess;

	FWKLOG(( "IOFireWireROMCache@0x%08lx::updateROMCache entered offset = %ld, length = %ld\n", (UInt32)this, offset, length ));

	FWKLOGASSERT( fOwner->getController()->inGate() == false );
		
	//
	// get the generation and make sure we're resumed
	//
	
	UInt32 generation = 0;
	status = checkROMState( generation );    
	
	if( status == kIOReturnSuccess )
	{
		unsigned int romLength = getLength();
		UInt32 romEnd = (offset + length) * sizeof(UInt32);
			
		while( romEnd > romLength && kIOReturnSuccess == status ) 
		{
			UInt32 *				buff;
			int 					bufLen;
			IOFWReadQuadCommand *	cmd;
		
			FWKLOG(( "IOFireWireROMCache %p:Need to extend ROM cache from 0x%lx to 0x%lx quads\n", 
					this, romLength/sizeof(UInt32), romEnd ));
			
			//
			// read the config ROM with the latched generation
			//
			
			bufLen = romEnd - romLength;
			buff = (UInt32 *)IOMalloc(bufLen);
			cmd = fOwner->createReadQuadCommand( FWAddress(kCSRRegisterSpaceBaseAddressHi, kFWBIBHeaderAddress+romLength),
												buff, bufLen/sizeof(UInt32), NULL, NULL, true );
			cmd->setGeneration( generation );
			status = cmd->submit();
			cmd->release();
			
			// 
			// if the command fails because of a bus reset, wait until the
			// bus is resumed or the ROM becomes invalid
			//
			
			if( status == kIOFireWireBusReset )
			{
				// on good return status the generation will be updated, but we won't have incremented
				// the romLength so we will retry the read the next time through the loop
			
				// on invalid return status the generation will be updated, but status will be invalid
				// and we will bail out of this loop
					
				status = checkROMState( generation );
			}
			else if( status == kIOReturnSuccess ) 
			{
				unsigned int newLength;
				
				lock();
				
				newLength = getLength();
				
				if( romLength == newLength ) 
				{
					appendBytes( buff, bufLen );
					newLength += bufLen;
				}
				
				romLength = newLength;
				
				unlock();
			}
			else
			{
				FWKLOG(( "%p: err 0x%x reading ROM\n", this, status ));
			}
			
			IOFree( buff, bufLen );
		}
	}
	
	FWKLOG(( "IOFireWireROMCache@%08lx::updateROMCache exited status = 0x%08lx\n", (UInt32)this, (UInt32)status ));
    
	return status;
}

// serialize
//
//

bool IOFireWireROMCache::serialize( OSSerialize * s ) const
{
	OSDictionary *	dictionary;
	bool			ok;
	
	dictionary = OSDictionary::withCapacity( 4 );
	if( !dictionary )
		return false;
			
	dictionary->setObject( "Offset 0", fROM );
	
	ok = dictionary->serialize(s);
	dictionary->release();
	
	return ok;
}