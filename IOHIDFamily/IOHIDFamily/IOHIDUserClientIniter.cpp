/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <libkern/OSAtomic.h>
#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOLib.h>

#include "IOHIDUserClientIniter.h"

#define super IOService

OSDefineMetaClassAndStructors(IOHIDUserClientIniter, IOService);

/*
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 0);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 1);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 2);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 3);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 4);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 5);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 6);
OSMetaClassDefineReservedUnused(IOHIDUserClientIniter, 7);
*/

bool IOHIDUserClientIniter::start(IOService* provider)
{
    OSDictionary *	providerMergeProperties = NULL;
    bool            result = false;

    providerMergeProperties = OSDynamicCast(OSDictionary, getProperty("IOProviderMergeProperties"));
    if ( !providerMergeProperties )
        return false;

    result = mergeDictionaryIntoProvider(provider, providerMergeProperties) ;
    
    return result;
}

bool IOHIDUserClientIniter::mergeDictionaryIntoProvider(IOService * provider, OSDictionary * dictionaryToMerge)
{
    const OSSymbol * 		dictionaryEntry = NULL;
    OSCollectionIterator * 	iter = NULL;
    bool                    result = false;


    if (!provider || !dictionaryToMerge)
        return false;

    // Get the dictionary whose entries we need to merge into our provider and get
    // an iterator to it.
    //
    iter = OSCollectionIterator::withCollection((OSDictionary *)dictionaryToMerge);
    if ( iter != NULL ) {
        // Iterate through the dictionary until we run out of entries
        //
        while ( NULL != (dictionaryEntry = (const OSSymbol *)iter->getNextObject()) ) {
            OSDictionary *	sourceDictionary = NULL;
            OSDictionary *	providerDictionary = NULL;
            OSObject *		providerProperty = NULL;

            // Check to see if our destination already has the same entry.
            //
            providerProperty = provider->getProperty(dictionaryEntry);
            if ( providerProperty )
                providerDictionary = OSDynamicCast(OSDictionary, providerProperty);
            
            // See if our source entry is also a dictionary
            //
            sourceDictionary = OSDynamicCast(OSDictionary, dictionaryToMerge->getObject(dictionaryEntry));
            
            if ( providerDictionary &&  sourceDictionary )  {
                // Need to merge our entry into the provider's dictionary.  However, we don't have a copy of our dictionary, just
                // a reference to it.  So, we need to make a copy of our provider's dictionary so that we don't modify our provider's
                // dictionary using non-synchronize calls.
                //
                OSDictionary *		localCopyOfProvidersDictionary;
                UInt32			providerSize;
                UInt32			providerSizeAfterMerge;

                // A capacity of 0 indicates that the dictionary should have the same size as the source
                //
                localCopyOfProvidersDictionary = OSDictionary::withDictionary( providerDictionary, 0);
                if ( localCopyOfProvidersDictionary == NULL )
                    break;
                
                // Get the size of our provider's dictionary so that we can check later whether it changed
                //
                providerSize = providerDictionary->getCapacity();
                
                // Recursively merge the two dictionaries
                //
                result = mergeDictionaryIntoDictionary(  sourceDictionary, localCopyOfProvidersDictionary);
                if ( result ) {
                    // Get the size of our provider's dictionary so to see if it's changed  (Yes, the size could remain the same but the contents
                    // could have changed, but this gives us a first approximation.  We're not doing anything with this result, although we could
                    // remerge if the size changed)
                    //
                    providerSizeAfterMerge = providerDictionary->getCapacity();

                    // OK, now we can just set the property in our provider
                    //
                    result = provider->setProperty( dictionaryEntry, localCopyOfProvidersDictionary );
                    if ( !result )
                        break;
                }
                else
                    // If we got an error merging dictionaries, then just bail out without doing anything
                    //
                    break;
            } else {
                // Not a dictionary, so just set the property
                //
                result = provider->setProperty(dictionaryEntry, dictionaryToMerge->getObject(dictionaryEntry));
                if ( !result )
                    break;
            }
        }
        iter->release();
    }

    return result;
}


bool IOHIDUserClientIniter::mergeDictionaryIntoDictionary(OSDictionary * parentSourceDictionary,  OSDictionary * parentTargetDictionary)
{
    OSCollectionIterator*	srcIterator     = NULL;
    OSSymbol*               keyObject       = NULL;
    bool                    result          = false;

    if (!parentSourceDictionary || !parentTargetDictionary)
        return false ;

    // Get our source dictionary
    //
    srcIterator = OSCollectionIterator::withCollection(parentSourceDictionary);

    while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject()))) {
        OSDictionary *	childSourceDictionary   = NULL;
        OSDictionary *	childTargetDictionary   = NULL;
        OSObject *      childTargetObject       = NULL;

        // Check to see if our destination already has the same entry.
        //
        childTargetObject = parentTargetDictionary->getObject(keyObject);
        if ( childTargetObject )
            childTargetDictionary = OSDynamicCast(OSDictionary, childTargetObject);

        // See if our source entry is also a dictionary
        //
        childSourceDictionary = OSDynamicCast(OSDictionary, parentSourceDictionary->getObject(keyObject));

        if ( childTargetDictionary && childSourceDictionary) {
            // Our destination dictionary already has the entry for this same object AND our
            // source is also a dcitionary, so we need to recursively add it.
            //
            result = mergeDictionaryIntoDictionary(childSourceDictionary, childTargetDictionary) ;
            if ( !result )
                break;
        } else {
            // We have a property that we need to merge into our parent dictionary.
            //
            result = parentTargetDictionary->setObject(keyObject, parentSourceDictionary->getObject(keyObject)) ;
            if ( !result )
                break;
        }
    }

    srcIterator->release();

    return result;
}
