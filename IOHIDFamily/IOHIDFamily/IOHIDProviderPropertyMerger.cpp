/*
 * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
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

#include <AssertMacros.h>
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOLib.h>

#include "IOHIDProviderPropertyMerger.h"

#define super IOService

OSDefineMetaClassAndStructors(IOHIDProviderPropertyMerger, IOService);

bool IOHIDProviderPropertyMerger::start(IOService* provider)
{
    OSObject * properties = NULL;

    properties = copyProperty("IOProviderMergeProperties");
    mergeProperties(provider, OSDynamicCast(OSDictionary, properties));
    OSSafeReleaseNULL(properties);
    
    return false;
}

bool IOHIDProviderPropertyMerger::mergeProperties(IOService * provider, OSDictionary * properties)
{
    const OSSymbol *        dictionaryEntry = NULL;
    OSCollectionIterator *  iterator        = NULL;
    bool                    result          = false;

    require(provider && properties, exit);

    // Iterate through the properties until we run out of entries
    iterator = OSCollectionIterator::withCollection(properties);
    require(iterator, exit);

    while ( (dictionaryEntry = (const OSSymbol *)iterator->getNextObject()) ) {
        OSDictionary *	sourceDictionary    = NULL;
        OSObject *      providerObject      = NULL;
        OSDictionary *	providerDictionary  = NULL;
        
        providerObject = provider->copyProperty(dictionaryEntry);
        
        // See if our source entry is also a dictionary
        sourceDictionary    = OSDynamicCast(OSDictionary, properties->getObject(dictionaryEntry));
        providerDictionary  = OSDynamicCast(OSDictionary, providerObject);
        
        if ( providerDictionary && sourceDictionary )  {

            // Because access to the registry table may not be synchronized, we should take a copy
            OSDictionary *  providerDictionaryCopy = NULL;

            providerDictionaryCopy = OSDictionary::withDictionary( providerDictionary, 0);
            require_action(providerDictionaryCopy, dictionaryExit, result=false);
            
            // Recursively merge the two dictionaries
            result = mergeDictionaries(sourceDictionary, providerDictionaryCopy);
            require(result, dictionaryExit);
            
            // OK, now we can just set the property in our provider
            result = provider->setProperty(dictionaryEntry, providerDictionaryCopy);
            require(result, dictionaryExit);

dictionaryExit:
            if ( providerDictionaryCopy )
                providerDictionaryCopy->release();
        } else {
            // Not a dictionary, so just set the property
            result = provider->setProperty(dictionaryEntry, properties->getObject(dictionaryEntry));
        }
        
        if ( providerObject )
            providerObject->release();
        
        if ( !result )
            break;
    }

exit:
    if ( iterator )
        iterator->release();

    return result;
}


bool IOHIDProviderPropertyMerger::mergeDictionaries(OSDictionary * source,  OSDictionary * target)
{
    OSCollectionIterator *  srcIterator = NULL;
    OSSymbol*               keyObject   = NULL;
    bool                    result      = false;

    require(source && target, exit);

    // Get our source dictionary
    srcIterator = OSCollectionIterator::withCollection(source);
    require(srcIterator, exit);

    while ((keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject()))) {
        OSDictionary *	childSourceDictionary   = NULL;
        OSDictionary *	childTargetDictionary   = NULL;
        OSObject *      childTargetObject       = NULL;

        // Check to see if our destination already has the same entry.
        childTargetObject = target->getObject(keyObject);
        if ( childTargetObject )
            childTargetDictionary = OSDynamicCast(OSDictionary, childTargetObject);

        // See if our source entry is also a dictionary
        childSourceDictionary = OSDynamicCast(OSDictionary, source->getObject(keyObject));

        if ( childTargetDictionary && childSourceDictionary) {
            // Our destination dictionary already has the entry for this same object AND our
            // source is also a dcitionary, so we need to recursively add it.
            //
            result = mergeDictionaries(childSourceDictionary, childTargetDictionary) ;
            if ( !result )
                break;
        } else {
            // We have a property that we need to merge into our parent dictionary.
            //
            result = target->setObject(keyObject, source->getObject(keyObject)) ;
            if ( !result )
                break;
        }
    }

exit:
    if ( srcIterator )
        srcIterator->release();

    return result;
}
