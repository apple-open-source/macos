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


OSDefineMetaClassAndStructors(IOFireWireUserClientIniter, IOService);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 0);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 1);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 2);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 3);

UInt32						IOFireWireUserClientIniter::fHasUCIniter = false ;
IOFireWireUserClientIniter*	IOFireWireUserClientIniter::fUCIniter = NULL ;
OSDictionary*				IOFireWireUserClientIniter::fProviderMergeProperties = NULL ;
OSDictionary*				IOFireWireUserClientIniter::fPropTable = NULL ;
IOService*					IOFireWireUserClientIniter::fProvider = NULL ;


// init
//
//

bool
IOFireWireUserClientIniter::init(OSDictionary * propTable)
{
	fPropTable = propTable ;
	fProvider = NULL ;
	
	return IOService::init(propTable) ;
}

// start
//
//

bool
IOFireWireUserClientIniter::start(
	IOService*	provider)
{
	fProvider = provider ;

	OSObject*	dictObj = getProperty("IOProviderMergeProperties");
	
	fProviderMergeProperties = OSDynamicCast(OSDictionary, dictObj);
	
	if ( !fProviderMergeProperties )
	{
		fHasUCIniter = false;
		IOLog("%s %u: couldn't get fProviderMergeProperties\n", __FILE__, __LINE__ ) ;
		return false;
	}

	//
	// make sure the user client class object is an OSSymbol
	//
	
	OSObject * userClientClassObject = fProviderMergeProperties->getObject( gIOUserClientClassKey );
	if( OSDynamicCast(OSString, userClientClassObject) != NULL )
	{
		// if the the user client class object is an OSString, turn it into an OSSymbol
		
		const OSSymbol * userClientClassSymbol = OSSymbol::withString((const OSString *) userClientClassObject);
		if( userClientClassSymbol != NULL )
		{
			fProviderMergeProperties->setObject(gIOUserClientClassKey, (OSObject *) userClientClassSymbol);
			userClientClassSymbol->release();
		}
		
	}
	else if( OSDynamicCast(OSSymbol, userClientClassObject) == NULL )
	{
		// if its not an OSString or an OSymbol remove it from the merge properties
		
		fProviderMergeProperties->removeObject(gIOUserClientClassKey);
	}
	
    OSDictionary*	providerProps = fProvider->getPropertyTable() ;
	if (providerProps)
	{
		mergeProperties(providerProps, fProviderMergeProperties) ;
//		fProviderMergeProperties->flushCollection() ;
	}

    return true ;
}

void
IOFireWireUserClientIniter::stop(IOService* provider)
{
	IOService::stop(provider) ;
}

// mergeProperties
//
// recurively merge the properties of two dictionaries

void
IOFireWireUserClientIniter::mergeProperties(OSObject* inDest, OSObject* inSrc)
{
	OSDictionary*	dest = OSDynamicCast(OSDictionary, inDest);
	OSDictionary*	src = OSDynamicCast(OSDictionary, inSrc);

	if (!src || !dest)
		return;

	OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection(src);
	
	OSSymbol*	keyObject	= NULL;
	OSObject*	destObject	= NULL;
	OSObject*	srcObject	= NULL;
	
	while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
	{
		srcObject 	= src->getObject(keyObject);
		destObject	= dest->getObject(keyObject);
		
		if (OSDynamicCast(OSDictionary, srcObject))
		{
			srcObject = copyDictionaryProperty((OSDictionary*)srcObject);
			
			// if there's a already a dictionary with the same key as the 
			// srcObject, we need to merge the new properties with that 
			// dictionary, otherwise we can just add the srcObject to the
			// dest dictionary
			
			if( destObject )
				mergeProperties(destObject, srcObject);
			else
				dest->setObject(keyObject, srcObject);
			
			// copyDictionaryProperty creates a new dictionary, so we should release 
			// it after we add it to our dictionary
	
			srcObject->release();
		}
		else
		{
			// if the property is not a dictionary, then we can simply add it to the
			// destination dictionary
			
			dest->setObject(keyObject, srcObject);
		}
	}
	
	// have to release this, or we'll leak.
	srcIterator->release();
}

// copyDictionaryProperty
//
// recursively copy am OSDictionary

OSDictionary*
IOFireWireUserClientIniter::copyDictionaryProperty(
	OSDictionary*	srcDictionary)
{
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
			while ( keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject()) )
			{
				srcObject	= srcDictionary->getObject(keyObject);
				if (OSDynamicCast(OSDictionary, srcObject))
				{
					srcObject = copyDictionaryProperty((OSDictionary*)srcObject);
					
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
	
	return result;
}