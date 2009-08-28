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
/*
 *  IOFireWireUserClientIniter.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Wed Jan 24 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#import "IOFireWireUserClientIniter.h"

#import <IOKit/assert.h>
#import <IOKit/IOLib.h>
#import <IOKit/IOService.h>
#import <libkern/OSAtomic.h>

#undef super
#define super IOService

IORecursiveLock * 	IOFireWireUserClientIniter::sIniterLock = NULL;

OSDefineMetaClassAndStructors(IOFireWireUserClientIniter, super);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 0);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 1);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 2);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 3);

// init
//
//

bool
IOFireWireUserClientIniter::init(OSDictionary * propTable)
{
	fProvider = NULL ;

	if( sIniterLock == NULL )
	{
		IORecursiveLock * lock = IORecursiveLockAlloc();
		//IOLog( "IOFireWireUserClientIniter<0x%08lx>::init - IORecursiveLockAlloc = 0x%08lx\n", this, lock );

		bool result = false;
		while( sIniterLock == NULL && result == false )
		{
			result = OSCompareAndSwapPtr( NULL, lock, (void * volatile *)&sIniterLock );
		}

		if( result == false )
		{
			//IOLog( "IOFireWireUserClientIniter<0x%08lx>::init - IORecursiveLockFree = 0x%08lx\n", this, lock );
			IORecursiveLockFree( lock );
		}
	}
	
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::init - sIniterLock = 0x%08lx\n", this, sIniterLock );

	return super::init(propTable) ;
}

// start
//
//

bool
IOFireWireUserClientIniter::start(
	IOService*	provider)
{
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::start - provider = 0x%08lx\n", this, provider );

	if( provider == NULL )
	{
		return false;
	}
	
	fProvider = provider ;
	fProvider->retain();

	OSObject*	dictObj = getProperty("IOProviderMergeProperties");	

	OSDictionary * merge_properties = OSDynamicCast(OSDictionary, dictObj);
	if( merge_properties != NULL )
	{
		merge_properties = dictionaryDeepCopy( merge_properties );
	}
	
	if ( !merge_properties )
	{
		IOLog("%s %u: couldn't get merge_properties\n", __FILE__, __LINE__ ) ;
		return false;
	}

	//
	// make sure the user client class object is an OSSymbol
	//
	
	OSObject * userClientClassObject = merge_properties->getObject( gIOUserClientClassKey );
	if( OSDynamicCast(OSString, userClientClassObject) != NULL )
	{
		// if the the user client class object is an OSString, turn it into an OSSymbol
		
		const OSSymbol * userClientClassSymbol = OSSymbol::withString((const OSString *) userClientClassObject);
		if( userClientClassSymbol != NULL )
		{
			merge_properties->setObject(gIOUserClientClassKey, (OSObject *) userClientClassSymbol);
			userClientClassSymbol->release();
		}
		
	}
	else if( OSDynamicCast(OSSymbol, userClientClassObject) == NULL )
	{
		// if its not an OSString or an OSymbol remove it from the merge properties
		
		merge_properties->removeObject(gIOUserClientClassKey);
	}
	
	// serialize all firwire user client initers
	
	IORecursiveLockLock( sIniterLock );
	
	mergeProperties( fProvider, merge_properties );

	IORecursiveLockUnlock( sIniterLock );

	merge_properties->release();

	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::start - return\n", this );
	
    return true ;
}

// stop
//
//

void
IOFireWireUserClientIniter::stop(IOService* provider)
{
	IOService::stop(provider) ;
}

// free
//
//

void IOFireWireUserClientIniter::free()
{
	if( fProvider != NULL )
	{
		fProvider->release();
		fProvider = NULL;
	}
	
	IOService::free();
}

// mergeProperties
//
// recursively merge a dictionaries into a registry entry

void
IOFireWireUserClientIniter::mergeProperties( IORegistryEntry * dest, OSDictionary * src )
{
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::mergeProperties - dest = 0x%08lx, src = 0x%08lx\n", this, dest, src );

	if( !dest || !src )
		return;
	
	OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection( src );
	
	OSSymbol*	keyObject	= NULL;
	OSObject*	destObject	= NULL;
	OSObject*	srcObject	= NULL;
	
	while( NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())) )
	{
		srcObject 	= src->getObject(keyObject);
		destObject	= dest->getProperty(keyObject);
		
		OSDictionary * destDictionary = OSDynamicCast( OSDictionary, destObject );
		OSDictionary * srcDictionary = OSDynamicCast( OSDictionary, srcObject );
		
		if( destDictionary && srcDictionary )
		{
			// if there's already already a property defined in the destination
			// and the source and destination are dictionaries, we need to do 
			// a recursive merge
		
			// shallow copy the destination directory
			destDictionary = OSDictionary::withDictionary( destDictionary );
			
			// recurse
			mergeDictionaries( destDictionary, srcDictionary );
			
			// set the property
			dest->setProperty( keyObject, destDictionary );
			destDictionary->release();			
		}
		else
		{
			// if the property is not already in destination dictionary 
			// or both source a destination are not dictionaries
			// then we can set the property without merging
			
			// any dictionaries in the source should already 
			// have been deep copied before we began merging
			
			dest->setProperty( keyObject, srcObject );
		}
	}
	
	// have to release this, or we'll leak.
	srcIterator->release();
	
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::mergeProperties - return\n", this );
	
}

// mergeDictionaries
//
// recursively merge two dictionaries

void
IOFireWireUserClientIniter::mergeDictionaries( OSDictionary * dest, OSDictionary * src )
{
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::mergeDictionaries - dest = 0x%08lx, src = 0x%08lx\n", this, dest, src );
	
	if( !src || !dest )
		return;

	OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection(src);
	
	OSSymbol*	keyObject	= NULL;
	OSObject*	destObject	= NULL;
	OSObject*	srcObject	= NULL;
	
	while( NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())) )
	{
		srcObject 	= src->getObject(keyObject);
		destObject	= dest->getObject(keyObject);
		
		OSDictionary * destDictionary = OSDynamicCast( OSDictionary, destObject );
		OSDictionary * srcDictionary = OSDynamicCast( OSDictionary, srcObject );
		
		if( destDictionary && srcDictionary )
		{
			// if there's already already a property defined in the destination
			// and the source and destination are dictionaries, we need to do 
			// a recursive merge
		
			// shallow copy the destination directory
			destDictionary = OSDictionary::withDictionary( destDictionary);
			
			// recurse
			mergeDictionaries( destDictionary, srcDictionary );
			
			// set the property
			dest->setObject( keyObject, destDictionary );
			destDictionary->release();			
		}
		else
		{
			// if the property is not already in destination dictionary 
			// or both source a destination are not dictionaries
			// then we can set the property without merging
			
			// any dictionaries in the source should already 
			// have been deep copied before we began merging
			
			dest->setObject( keyObject, srcObject );
		}
	}
	
	// have to release this, or we'll leak.
	srcIterator->release();

	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::mergeDictionaries - return\n", this );
}

// dictionaryDeepCopy
//
// recursively copy an OSDictionary

OSDictionary*
IOFireWireUserClientIniter::dictionaryDeepCopy(
	OSDictionary*	srcDictionary)
{
	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::dictionaryDeepCopy - srcDictionary = 0x%08lx\n", this, srcDictionary );

	OSDictionary*			result			= NULL;
	OSObject*				srcObject		= NULL;
	OSCollectionIterator*	srcIterator		= NULL;
	OSSymbol*				keyObject		= NULL;
	
	result = OSDictionary::withCapacity(srcDictionary->getCount());
	if (result)
	{
		srcIterator = OSCollectionIterator::withCollection(srcDictionary);
		if (srcIterator)
		{
			while ( (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())) )
			{
				srcObject	= srcDictionary->getObject(keyObject);
				if (OSDynamicCast(OSDictionary, srcObject))
				{
					srcObject = dictionaryDeepCopy((OSDictionary*)srcObject);
					
					result->setObject(keyObject, srcObject);
					
					// copyDictionaryProperty creates a new dictionary, so we should release 
					// it after we add it to our dictionary
					
					srcObject->release();
				}
				else
				{
					result->setObject(keyObject, srcObject);
				}
			}
		
			srcIterator->release();
		}
	}

	//IOLog( "IOFireWireUserClientIniter<0x%08lx>::dictionaryDeepCopy - return\n", this );
	
	return result;
}
