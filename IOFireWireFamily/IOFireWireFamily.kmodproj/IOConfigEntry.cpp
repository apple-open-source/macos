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
#import <IOKit/firewire/IOLocalConfigDirectory.h>

// private
#import "FWDebugging.h"

// system
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSData.h>
#import <libkern/c++/OSArray.h>

#import "IOConfigEntry.h"

OSDefineMetaClassAndStructors(IOConfigEntry, OSObject);

// free
//
//

void IOConfigEntry::free()
{
    if (fData) {
        fData->release();
        fData = NULL;
    }

    OSObject::free();
}

// create
//
//

IOConfigEntry* IOConfigEntry::create(UInt32 key, IOConfigKeyType type, OSObject *obj)
{
    IOConfigEntry* entry;
    entry = OSTypeAlloc( IOConfigEntry );
    if(!entry)
        return NULL;
    if(!entry->init()) {
        entry->release();
        return NULL;
    }
    assert(type == kConfigLeafKeyType ||
           type == kConfigDirectoryKeyType);
    entry->fKey = key;
    entry->fType = type;

	obj->retain() ;
    entry->fData = obj;
	
    return entry;
}

// create
//
//

IOConfigEntry* IOConfigEntry::create(UInt32 key, UInt32 value)
{
    IOConfigEntry* entry;

    if(value > kConfigEntryValue)
        return NULL;	// Too big to fit!!
    entry = OSTypeAlloc( IOConfigEntry );
    if(!entry)
        return NULL;
    if(!entry->init()) {
        entry->release();
        return NULL;
    }
    entry->fKey = key;
    entry->fType = kConfigImmediateKeyType;
    entry->fValue = value;
    return entry;
}

// create
//
//

IOConfigEntry* IOConfigEntry::create(UInt32 key, FWAddress address)
{
    IOConfigEntry* entry;
    entry = OSTypeAlloc( IOConfigEntry );
    if(!entry)
        return NULL;
    if(!entry->init()) {
        entry->release();
        return NULL;
    }
    entry->fKey = key;
    entry->fType = kConfigOffsetKeyType;
    entry->fAddr = address;
    return entry;
}

// totalSize
//
//

unsigned int IOConfigEntry::totalSize()
{
    unsigned int size = 0;
    switch(fType) {
	default:
            break;
        case kConfigLeafKeyType:
        {
            OSData *data = OSDynamicCast(OSData, fData);
            if(!data)
                return 0;
            // Round up size to multiple of 4, plus header
            size = (data->getLength()-1) / sizeof(UInt32) + 2;
            break;
            
        }
        case kConfigDirectoryKeyType:
        {
            IOLocalConfigDirectory *dir = OSDynamicCast(IOLocalConfigDirectory,
                                                            fData);
            if(!dir)
                return 0;	// Oops!
            const OSArray *entries = dir->getEntries();
            unsigned int i;
            size = 1 + entries->getCount();
            for(i=0; i<entries->getCount(); i++) {
                IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, entries->getObject(i));
                if(!entry)
                    return 0;	// Oops!
                size += entry->totalSize();
            }
            break;
        }
    }
    return size;
}
