/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

// public
#import <IOKit/firewire/IOConfigDirectory.h>
#import <IOKit/firewire/IORemoteConfigDirectory.h>
#import <IOKit/firewire/IOFireWireDevice.h>

// private
#import "FWDebugging.h"
#include "IOConfigDirectoryIterator.h"

// system
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSData.h>

static int findIndex(const UInt32* base, int size, int key,
                     UInt32 type = kInvalidConfigROMEntryType);

int findIndex(const UInt32* base, int size, int key, UInt32 type)
{
    int i;
    UInt32 mask, test;
    test = (UInt32)key << kConfigEntryKeyValuePhase;
    mask = kConfigEntryKeyValue;
    if(type != kInvalidConfigROMEntryType) {
        test |= type << kConfigEntryKeyTypePhase;
        mask |= kConfigEntryKeyType;
    }
    // OR test into mask, in case key was more than just the key value
    mask |= test;
    for(i=0; i<size; i++) {
        if( (base[i] & mask) == test )
            break;
    }
    if(i >= size)
        i = -1;
    return i;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOConfigDirectory, OSObject )
OSDefineAbstractStructors(IOConfigDirectory, OSObject)
OSMetaClassDefineReservedUnused(IOConfigDirectory, 0);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 1);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 2);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 3);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 4);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 5);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 6);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 7);
OSMetaClassDefineReservedUnused(IOConfigDirectory, 8);

// initWithOffset
//
//

bool IOConfigDirectory::initWithOffset(int start, int type)
{
    IOReturn status = kIOReturnSuccess;
    const UInt32 *data;
	
    if( !OSObject::init() )
	{
        status = kIOReturnError;
	}
    
	if( status == kIOReturnSuccess )
	{
		fStart = start;
		fType = type;
    
		status = updateROMCache( start, 1 );
	}
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();
		fNumEntries = (data[start] & kConfigLeafDirLength) >> kConfigLeafDirLengthPhase;
		unlockData();
	
	//	FWKLOG(( "IOConfigDirectory::initWithOffset updateROMCache( %d, %d )\n", start, fNumEntries ));
		status = updateROMCache(start + 1, fNumEntries);
	}
	
	if( status != kIOReturnSuccess )
	{
		fNumEntries = 0;
	}
	
	return true;
}

// createIterator
//
//

IOReturn IOConfigDirectory::createIterator(UInt32 testVal, UInt32 testMask, OSIterator *&iterator)
{
	IOReturn status = kIOReturnSuccess;
	
    IOConfigDirectoryIterator *iter = NULL;
	
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		iter = new IOConfigDirectoryIterator;
		if( iter == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = iter->init( this, testVal, testMask );
		if( status == kIOReturnSuccess )
		{
			iterator = iter;
		}
		else
		{
			iter->release();
			iter = NULL;
		}
	}
	
	return status;
}

// getKeyType
//
//

IOReturn IOConfigDirectory::getKeyType(int key, IOConfigKeyType &type)
{
	IOReturn status = kIOReturnSuccess;
    int index = 0;
    
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData() + fStart + 1;
		index = findIndex(data, fNumEntries, key);
		unlockData();

		if( index < 0 )
			status = kIOConfigNoEntry;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexType(index, type);
	}
	
    return status;
}

// getKeyValue
//
//

IOReturn IOConfigDirectory::getKeyValue(int key, UInt32 &value, OSString** text)
{
	IOReturn status = kIOReturnSuccess;
    int index = 0;
 
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData() + fStart + 1;
		index = findIndex(data, fNumEntries, key);
		unlockData();
	
		if( index < 0 )
        	status = kIOConfigNoEntry;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexValue(index, value);
    }
	
	if( status == kIOReturnSuccess && text ) 
	{
		// textual descriptor is optional
        *text = NULL;
        status = getIndexValue(index+1, *text);
		if( status != kIOFireWireConfigROMInvalid )
			status = kIOReturnSuccess;
    }
	
    return status;
}

// getKeyValue
//
//

IOReturn IOConfigDirectory::getKeyValue(int key, OSData *&value, OSString** text)
{
	IOReturn status = kIOReturnSuccess;
    int index = 0;

	status = checkROMState();

	if( status == kIOReturnSuccess )
	{    
		const UInt32 * data = lockData() + fStart + 1;
		index = findIndex(data, fNumEntries, key, kConfigLeafKeyType);
		unlockData();
		
		if( index < 0 )
		{
			status = kIOConfigNoEntry;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexValue(index, value);
    }
	
	if( status == kIOReturnSuccess && text ) 
	{
		// textual descriptor is optional
        *text = NULL;
        status = getIndexValue(index+1, *text);
		if( status != kIOFireWireConfigROMInvalid )
			status = kIOReturnSuccess;
    }
	
    return status;
}

// getKeyValue
//
//

IOReturn IOConfigDirectory::getKeyValue(int key, IOConfigDirectory *&value,
                                                        OSString** text)
{
	IOReturn status = kIOReturnSuccess;
    int index = 0;
    
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData() + fStart + 1;
		index = findIndex(data, fNumEntries, key, kConfigDirectoryKeyType);
		unlockData();
		
		if( index < 0 )
		{
			status = kIOConfigNoEntry;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexValue(index, value);
    }
	
	if( status == kIOReturnSuccess && text ) 
	{
		// textual descriptor is optional
        *text = NULL;
        status = getIndexValue(index+1, *text);
		if( status != kIOFireWireConfigROMInvalid )
			status = kIOReturnSuccess;
    }
	
    return status;
}

// getKeyOffset
//
//

IOReturn IOConfigDirectory::getKeyOffset(int key, FWAddress &value, OSString** text)
{
	IOReturn status = kIOReturnSuccess;
    int index = 0;
    
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData() + fStart + 1;
		index = findIndex(data, fNumEntries, key, kConfigOffsetKeyType);
		unlockData();
	
		if( index < 0 )
        	status = kIOConfigNoEntry;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexOffset(index, value);
    }
	
	if( status == kIOReturnSuccess && text) 
	{
		// textual descriptor is optional
        *text = NULL;
        status = getIndexValue(index+1, *text);
		if( status != kIOFireWireConfigROMInvalid )
			status = kIOReturnSuccess;
	}
    
	return status;
}

// getKeySubdirectories
//
//

IOReturn IOConfigDirectory::getKeySubdirectories(int key, OSIterator *&iterator)
{
    IOReturn status = createIterator((key << kConfigEntryKeyValuePhase) |
							(kConfigDirectoryKeyType << kConfigEntryKeyTypePhase),
								kConfigEntryKeyType | kConfigEntryKeyValue, iterator);
	
	return status;
}

// getType
//
//

int IOConfigDirectory::getType() const 
{
	return fType;
}

// getNumEntries
//
//

int IOConfigDirectory::getNumEntries() const 
{
	return fNumEntries;
}

// getIndexType
//
//

IOReturn IOConfigDirectory::getIndexType(int index, IOConfigKeyType &type)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{	
		const UInt32 * data = lockData();
		entry = data[fStart + 1 + index];
		unlockData();

		type = (IOConfigKeyType)((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase);
    }
	
	return status;
}

// getIndexKey
//
//

IOReturn IOConfigDirectory::getIndexKey(int index, int &key)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();	
		entry = data[fStart + 1 + index];
		unlockData();
	
		key = (IOConfigKeyType)((entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase);
	}
	
    return status;
}

// getIndexValue
//
//

IOReturn IOConfigDirectory::getIndexValue(int index, UInt32 &value)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();	
		entry = data[fStart + 1 + index];
		unlockData();
	
		// Return the value as an integer, whatever it really is.
		value = entry & kConfigEntryValue;
    }
	
	return status;
}

// getIndexValue
//
//

IOReturn IOConfigDirectory::getIndexValue(int index, OSData *&value)
{
	IOReturn status = kIOReturnSuccess;    
    UInt32 entry;
    const UInt32 *data;
    UInt32 offset;
    int len = 0;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();	
		entry = data[fStart + 1 + index];
		unlockData();
	
		if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) != kConfigLeafKeyType)
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexOffset( index, offset );
	}
	
	if( status == kIOReturnSuccess )
	{
		status = updateROMCache( offset, 1 );
	}
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();
		len = ((data[offset] & kConfigLeafDirLength) >> kConfigLeafDirLengthPhase);
		unlockData();

	//	FWKLOG(( "IOConfigDirectory::getIndexValue(OSData) updateROMCache( %ld, %d )\n", offset, len ));
		
		status = updateROMCache( offset + 1, len );
    }
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();
		value = OSData::withBytes(data+offset+1, len*sizeof(UInt32));
		unlockData();
		
		if( value == NULL)
			status = kIOReturnNoMemory;
	}
	
    return status;
}

// getIndexValue
//
//

IOReturn IOConfigDirectory::getIndexValue(int index, OSString *&value)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry = 0;
    const UInt32 *data;
    UInt32 offset;
    int len = 0;
    
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		data = lockData();
		entry = data[fStart + 1 + index];
		unlockData();
	
		if( ((entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase) != kConfigTextualDescriptorKey )
        	status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) != kConfigLeafKeyType )
			status = kIOReturnBadArgument;
	}
    
	if( status == kIOReturnSuccess )
	{
		status = getIndexOffset(index, offset);
	}
	
	if( status == kIOReturnSuccess )
	{
		status = updateROMCache(offset, 1);
	}
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();
		len = (data[offset] & kConfigLeafDirLength) >> kConfigLeafDirLengthPhase;
		unlockData();
	
		// Check for silly length, people are careless with string data!
		if( (len * 4) > 256 ) 
        	status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		FWKLOG(( "IOConfigDirectory::getIndexValue(OSString) updateROMCache( %ld, %d )\n", offset, len ));
    
    	status = updateROMCache(offset + 1,len);
	}
	
	if( status == kIOReturnSuccess )
	{
		data = lockData();
    
		// Skip over length, CRC, spec_type, specifier_ID, language_ID
		char *text = (char *)(&data[offset+3]);
		len -= 2;	// skip spec_type, specifier_ID, language_ID
		len *= sizeof(UInt32);	// Convert from Quads to chars
		// Now skip over leading zeros in string
		while(len && !*text) {
			len--;
			text++;
		}
		if(len) {
			char saved = text[len];
			text[len] = 0;
			value = OSString::withCString(text);
			text[len] = saved;
		}
		else
			value = OSString::withCString("");
		
		unlockData();
	
		if( value == NULL )
			status = kIOReturnNoMemory;
	}
	
    return status;
}

// getIndexValue
//
//

IOReturn IOConfigDirectory::getIndexValue(int index, IOConfigDirectory *&value)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry = 0;
    UInt32 offset;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();
		entry = data[fStart + 1 + index];
		unlockData();
	
		if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) != kConfigDirectoryKeyType)
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = getIndexOffset(index, offset);
	}
	
	if( status == kIOReturnSuccess )
	{
		value = getSubDir( offset, (entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase );
		if( value == NULL )
			status = kIOReturnNoMemory;
	}
    
	return status;
}

// getIndexOffset
//
//

IOReturn IOConfigDirectory::getIndexOffset(int index, FWAddress &value)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry = 0;
    UInt32 offset;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();
		entry = data[fStart + 1 + index];
		unlockData();
	
		if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) == kConfigImmediateKeyType)
        	status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		value.addressHi = kCSRRegisterSpaceBaseAddressHi;
		offset = entry & kConfigEntryValue;
		if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) == kConfigOffsetKeyType) 
		{
			value.addressLo = kCSRRegisterSpaceBaseAddressLo + offset*sizeof(UInt32);
		}
		else 
		{
			offset += fStart + 1 + index;
			value.addressLo = kConfigROMBaseAddress + offset*sizeof(UInt32);
		}
	}
	
    return status;
}

// getIndexOffset
//
//

IOReturn IOConfigDirectory::getIndexOffset(int index, UInt32 &value)
{
	IOReturn status = kIOReturnSuccess;
    UInt32 entry = 0;

	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}

	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();
		entry = data[fStart + 1 + index];
		unlockData();
		
		if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) == kConfigImmediateKeyType)
		{
			status = kIOReturnBadArgument;
		}
		else if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) == kConfigOffsetKeyType) 
		{
			status = kIOReturnBadArgument;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		value = entry & kConfigEntryValue;
		value += fStart + 1 + index;
	}
	
    return status;
}

// getIndexEntry
//
//

IOReturn IOConfigDirectory::getIndexEntry(int index, UInt32 &value)
{
	IOReturn status = kIOReturnSuccess;
	
	status = checkROMState();
	
	if( status == kIOReturnSuccess )
	{
		if( index < 0 || index >= fNumEntries )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		const UInt32 * data = lockData();
		value = data[fStart + 1 + index];
		unlockData();
	}
	
    return status;
}

// getSubdirectories
//
//

IOReturn IOConfigDirectory::getSubdirectories(OSIterator *&iterator)
{
    return createIterator(kConfigDirectoryKeyType << kConfigEntryKeyTypePhase,
                          kConfigEntryKeyType, iterator);
}

