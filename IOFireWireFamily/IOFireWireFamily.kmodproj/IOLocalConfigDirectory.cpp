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
#import "FWDebugging.h"

#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOLocalConfigDirectory.h>
#import <IOKit/firewire/IOFWUtils.h>
#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOFireWireBus.h>

// private
#import "IOConfigEntry.h"
#import "IOFireWireUserClient.h"
#import "IOFWUserObjectExporter.h"


// system
#import <libkern/c++/OSIterator.h>
#import <libkern/c++/OSData.h>
#import <libkern/c++/OSArray.h>
#import <libkern/c++/OSObject.h>
#import <libkern/c++/OSString.h>
#import <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(IOLocalConfigDirectory, IOConfigDirectory);
OSMetaClassDefineReservedUsed(IOLocalConfigDirectory, 0);
OSMetaClassDefineReservedUnused(IOLocalConfigDirectory, 1);
OSMetaClassDefineReservedUnused(IOLocalConfigDirectory, 2);

// init
//
//

bool IOLocalConfigDirectory::init()
{
    if(!IOConfigDirectory::initWithOffset(0, 0))
        return false;
    fEntries = OSArray::withCapacity(2);
    if(!fEntries)
        return false;
    return true;
}

// free
//
//

void IOLocalConfigDirectory::free()
{
    if(fEntries)
        fEntries->release();
    if(fROM)
        fROM->release();
IOConfigDirectory::free();
}

// getBase
//
//

const UInt32 *IOLocalConfigDirectory::getBase()
{
    if(fROM)
        return ((const UInt32 *)fROM->getBytesNoCopy()) ;//+fStart+1;
    else
        return &fHeader;
}

IOConfigDirectory *IOLocalConfigDirectory::getSubDir(int start, int type)
{
	return NULL;
}

// lockData
//
//

const UInt32 * IOLocalConfigDirectory::lockData( void )
{
	return getBase();
}

// unlockData
//
//

void IOLocalConfigDirectory::unlockData( void )
{
	// nothing to do
}

// create
//
//

IOLocalConfigDirectory *IOLocalConfigDirectory::create()
{
    IOLocalConfigDirectory *dir;
    dir = OSTypeAlloc( IOLocalConfigDirectory );
    if(!dir)
        return NULL;

    if(!dir->init()) {
        dir->release();
        return NULL;
    }
    return dir;
}

// update
//
//

IOReturn IOLocalConfigDirectory::update(UInt32 offset, const UInt32 *&romBase)
{
    IOReturn res = kIOReturnSuccess;
    if(!fROM) {
        if(offset == 0)
            romBase = &fHeader;
        else
            res = kIOReturnNoMemory;
    }
    else {
        if(offset*sizeof(UInt32) <= fROM->getLength())
            romBase = (const UInt32 *)fROM->getBytesNoCopy();
        else
            res = kIOReturnNoMemory;
    }
    return res;
}

// updateROMCache
//
//

IOReturn IOLocalConfigDirectory::updateROMCache( UInt32 offset, UInt32 length )
{
	return kIOReturnSuccess;
}

// checkROMState
//
//

IOReturn IOLocalConfigDirectory::checkROMState( void )
{
	return kIOReturnSuccess;
}

// incrementGeneration
//
//

IOReturn IOLocalConfigDirectory::incrementGeneration( void )
{
	IOReturn status = kIOReturnSuccess;
	
	unsigned int numEntries = fEntries->getCount();

    unsigned int i;
    for( i = 0; i < numEntries; i++ ) 
	{
		IOConfigEntry * entry = OSDynamicCast( IOConfigEntry, fEntries->getObject(i) );
        if( entry == NULL )
		{
			IOLog( __FILE__" %d internal error!\n", __LINE__ );
            status = kIOReturnInternalError;
			break;
		}

        if( (entry->fType == kConfigImmediateKeyType) && (entry->fKey == kConfigGenerationKey) )
		{
			entry->fValue++;
		}
	}

	return status;
}

// compile
//
//
	
IOReturn IOLocalConfigDirectory::compile(OSData *rom)
{
    UInt32 header;
    UInt32 big_header;
	UInt16 crc = 0;
    OSData *tmp;	// Temporary data for directory entries.
    unsigned int size;
    unsigned int numEntries;
    unsigned int i;
    unsigned int offset = 0;
    if(fROM)
        fROM->release();
    fROM = rom;
    rom->retain();
    size = fROM->getLength();
    fStart = size/sizeof(UInt32);
    numEntries = fEntries->getCount();
    
    /*
     * We can't just compile into the rom, because the CRC for the directory
     * depends on the entry data, and we can't (legally) overwrite data in an
     * OSData (it needs an overwriteBytes() method).
     * So compile into tmp, then calculate crc, then append lenth|crc and tmp.
     */

    rom->ensureCapacity(size + sizeof(UInt32)*(1+numEntries));
    tmp = OSData::withCapacity(sizeof(UInt32)*(numEntries));
    for( i = 0; i < numEntries; i++ ) 
	{
        IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
        UInt32 val;
		UInt32 big_val;
        if(!entry)
		{
			IOLog(__FILE__" %d internal error!\n", __LINE__ )  ;
            return kIOReturnInternalError;	// Oops!
		}
		
        switch(entry->fType) 
		{
            case kConfigImmediateKeyType:
				val = entry->fValue;
                break;
            case kConfigOffsetKeyType:
                val = (entry->fAddr.addressLo-kCSRRegisterSpaceBaseAddressLo)/sizeof(UInt32);
                break;
            case kConfigLeafKeyType:
            case kConfigDirectoryKeyType:
                val = numEntries-i+offset;
                offset += entry->totalSize();
                break;
            default:
				IOLog(__FILE__" %d internal error!\n", __LINE__ )  ;
                return kIOReturnInternalError;	// Oops!
        }
		
        val |= entry->fKey << kConfigEntryKeyValuePhase;
        val |= entry->fType << kConfigEntryKeyTypePhase;
		
		big_val = OSSwapHostToBigInt32( val );
        crc = FWUpdateCRC16(crc, big_val);
		
        tmp->appendBytes(&big_val, sizeof(UInt32));
    }
	
    header = numEntries << kConfigLeafDirLengthPhase;
    header |= crc;
	big_header = OSSwapHostToBigInt32( header );
    rom->appendBytes(&big_header, sizeof(UInt32));
    rom->appendBytes(tmp);
    tmp->release();

    // Now (recursively) append each leaf and directory.
    for(i=0; i<numEntries; i++) 
	{
        IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
        UInt32 val;
		UInt32 big_val;
        if(!entry)
		{
            return kIOReturnInternalError;	// Oops!
        }
		switch(entry->fType) 
		{
            case kConfigImmediateKeyType:
            case kConfigOffsetKeyType:
                break;
            case kConfigLeafKeyType:
            {
                OSData *data = OSDynamicCast(OSData, entry->fData);
                const void *buffer;
                unsigned int len, pad;
                if(data) 
				{
                    len = data->getLength();
                    pad = (4 - (len & 3)) & 3;
                    if(pad) 
					{
                        len += pad;
                        // Make sure the buffer is big enough for the CRC calc.
                        data->ensureCapacity(len);
                    }
                    buffer = data->getBytesNoCopy();
                }
                else
				{
                    return kIOReturnInternalError;	// Oops!
                }
				
				crc = FWComputeCRC16((const UInt32 *)buffer, len / 4);
                val = (len/4) << kConfigLeafDirLengthPhase;
                val |= crc;
				big_val = OSSwapHostToBigInt32( val );
				rom->appendBytes(&big_val, sizeof(UInt32));
                rom->appendBytes(buffer, len);
                break;
            }
            case kConfigDirectoryKeyType:
            {
                IOReturn res;
                IOLocalConfigDirectory *dir = OSDynamicCast(IOLocalConfigDirectory,
                                                         	entry->fData);
                if(!dir)
                    return kIOReturnInternalError;	// Oops!
                res = dir->compile(rom);
                if(kIOReturnSuccess != res)
                    return res;
                break;
            }
            default:
                return kIOReturnInternalError;	// Oops!
       }
    }
    return kIOReturnSuccess;                           
}

// addEntry
//
//

IOReturn IOLocalConfigDirectory::addEntry(int key, UInt32 value, OSString* desc )
{
    IOReturn res;

	IOConfigEntry *entry = IOConfigEntry::create(key, value);
	
	if(!entry)
	{
		return kIOReturnNoMemory;
	}
	if(!fEntries->setObject(entry))
	{
		res = kIOReturnNoMemory;
	}
	else
	{
		res = kIOReturnSuccess;
	}
	
	entry->release();	// In array now.

	if(desc) 
	{
		addEntry(desc);
	}
	
	// keep our count current...
	fNumEntries = fEntries->getCount() ;
	
	return res;
}

// addEntry
//
//

IOReturn IOLocalConfigDirectory::addEntry( int key, IOLocalConfigDirectory *value, OSString* desc )
{
    IOReturn res;

	IOConfigEntry *entry = IOConfigEntry::create(key, kConfigDirectoryKeyType, value);
	if(!entry)
	{
		return kIOReturnNoMemory;
	}
	if(!fEntries->setObject(entry))
	{
		res = kIOReturnNoMemory;
	}
	else
	{
		res = kIOReturnSuccess;
	}

	entry->release();	// In array now.
	if(desc) 
	{
		addEntry(desc);
	}

	// keep our count current...
	fNumEntries = fEntries->getCount() ;

	return res;
}

// addEntry
//
//

IOReturn IOLocalConfigDirectory::addEntry(int key, OSData *value, OSString* desc )
{
    IOReturn res;

	// copying the OSData makes us robust against clients 
	// which modify the OSData after they pass it in to us.
	
	OSData * valueCopy = OSData::withData( value );
	if( valueCopy == NULL )
		return kIOReturnNoMemory;
		
	IOConfigEntry *entry = IOConfigEntry::create(key, kConfigLeafKeyType, valueCopy );
	if( entry == NULL )
	{
		return kIOReturnNoMemory;
	}
	
	valueCopy->release();
	valueCopy = NULL;
	
	if(!fEntries->setObject(entry))
	{
		res = kIOReturnNoMemory;
	}
	else
	{
		res = kIOReturnSuccess;
	}
	
	entry->release();	// In array now.
	
	if(desc) 
	{
		addEntry(desc);
	}
	
	// keep our count current...
	fNumEntries = fEntries->getCount() ;

	return res;
}

// addEntry
//
//

IOReturn IOLocalConfigDirectory::addEntry( int key, FWAddress value, OSString* desc )
{
    IOReturn res;

	IOConfigEntry *entry = IOConfigEntry::create(key, value);
	if(!entry)
	{
		return kIOReturnNoMemory;
	}
	if(!fEntries->setObject(entry))
	{
		res = kIOReturnNoMemory;
	}
	else
	{
		res = kIOReturnSuccess;
	}
	entry->release();	// In array now.
	if(desc) 
	{
		addEntry(desc);
	}

	// keep our count current...
	fNumEntries = fEntries->getCount() ;

	return res;
}

// addEntry
//
//

IOReturn IOLocalConfigDirectory::addEntry(OSString *desc)
{
    IOReturn res;
    OSData * value;

	UInt64 zeros = 0;    
	
    int stringLength = desc->getLength();
	int paddingLength = (4 - (stringLength & 3)) & 3;
	int headerLength = 8;
	
	// make an OSData containing the string
	value = OSData::withCapacity( headerLength + stringLength + paddingLength );
	if( !value )
	{
		return kIOReturnNoMemory;
	}

	// append zeros for header
 	value->appendBytes( &zeros, headerLength );

	// append the string
    value->appendBytes( desc->getCStringNoCopy(), stringLength );
	
	// append zeros to pad to nearest quadlet
	value->appendBytes( &zeros, paddingLength );

	res = addEntry( kConfigTextualDescriptorKey, value );

	value->release(); 	// In ROM now
	desc->release();	// call eats a retain count

	// keep our count current...
	fNumEntries = fEntries->getCount() ;

    return res;
}

// removeSubDir
//
//

IOReturn IOLocalConfigDirectory::removeSubDir(IOLocalConfigDirectory *value)
{
    unsigned int i, numEntries;
    numEntries = fEntries->getCount();

	for(i=0; i<numEntries; ++i) 
	{
		IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
		if(!entry)
		{
			return kIOReturnInternalError;	// Oops!
		}
		if(entry->fType == kConfigDirectoryKeyType) 
		{
			if(entry->fData == value) 
			{
				fEntries->removeObject(i);

				// keep our count current...
				fNumEntries = fEntries->getCount() ;

				return kIOReturnSuccess;
			}
		}
	}
	return kIOConfigNoEntry;
}

// getEntries
//
//

const OSArray * IOLocalConfigDirectory::getEntries() const
{ 
	return fEntries; 
}

// getIndexValue
//
//

IOReturn 
IOLocalConfigDirectory::getIndexValue(int index, IOConfigDirectory *&value)
{
	IOReturn error = checkROMState();
	if ( error )
	{
		return error ;
	}
	
	if( index < 0 || index >= fNumEntries )
	{
		return kIOReturnBadArgument;
	}

	{
		lockData();

		value = OSDynamicCast( IOConfigDirectory, ((IOConfigEntry*)fEntries->getObject( index ) )->fData ) ;

		unlockData();

		if ( value )
		{
			value->retain() ;
		}
		else
		{
			error = kIOReturnBadArgument ;
		}
		
	}
    
	return error ;
}

void
IOLocalConfigDirectory::exporterCleanup( const OSObject * self, IOFWUserObjectExporter * exporter )
{
	IOLocalConfigDirectory * me = (IOLocalConfigDirectory*)self;
	((IOFireWireUserClient*)exporter->getOwner())->getOwner()->getBus()->RemoveUnitDirectory( me ) ;
}
