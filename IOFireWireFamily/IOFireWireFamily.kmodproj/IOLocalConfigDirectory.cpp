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

#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSObject.h>
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>


class IOConfigEntry : public OSObject
{
    OSDeclareDefaultStructors(IOConfigEntry);
protected:
    virtual void free();
    
public:
    UInt32 fKey;
    IOConfigKeyType fType;
    OSObject *fData;
    FWAddress fAddr;
    UInt32 fValue;

    unsigned int totalSize();

    static IOConfigEntry* create(UInt32 key, IOConfigKeyType type, OSObject *obj);
    static IOConfigEntry* create(UInt32 key, UInt32 value);
    static IOConfigEntry* create(UInt32 key, FWAddress address);
};

OSDefineMetaClassAndStructors(IOConfigEntry, OSObject);

void IOConfigEntry::free()
{
    if (fData) {
        fData->release();
        fData = NULL;
    }

    OSObject::free();
}

IOConfigEntry* IOConfigEntry::create(UInt32 key, IOConfigKeyType type, OSObject *obj)
{
    IOConfigEntry* entry;
    entry = new IOConfigEntry;
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

IOConfigEntry* IOConfigEntry::create(UInt32 key, UInt32 value)
{
    IOConfigEntry* entry;

    if(value > kConfigEntryValue)
        return NULL;	// Too big to fit!!
    entry = new IOConfigEntry;
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

IOConfigEntry* IOConfigEntry::create(UInt32 key, FWAddress address)
{
    IOConfigEntry* entry;
    entry = new IOConfigEntry;
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
                return 0;	// Oops!
            size = data->getLength() / sizeof(UInt32) + 1;
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOLocalConfigDirectory, IOConfigDirectory);
OSMetaClassDefineReservedUnused(IOLocalConfigDirectory, 0);
OSMetaClassDefineReservedUnused(IOLocalConfigDirectory, 1);
OSMetaClassDefineReservedUnused(IOLocalConfigDirectory, 2);


bool IOLocalConfigDirectory::init()
{
    if(!IOConfigDirectory::initWithOffset(0, 0))
        return false;
    fEntries = OSArray::withCapacity(2);
    if(!fEntries)
        return false;
    return true;
}

void IOLocalConfigDirectory::free()
{
    if(fEntries)
        fEntries->release();
    if(fROM)
        fROM->release();
IOConfigDirectory::free();
}

const UInt32 *IOLocalConfigDirectory::getBase()
{
    if(fROM)
        return ((const UInt32 *)fROM->getBytesNoCopy())+fStart+1;
    else
        return &fHeader;
}

const UInt32 * IOLocalConfigDirectory::lockData( void )
{
	return getBase();
}

void IOLocalConfigDirectory::unlockData( void )
{
	// nothing to do
}

IOConfigDirectory *IOLocalConfigDirectory::getSubDir(int start, int type)
{
    IOConfigDirectory *subDir;
    subDir = OSDynamicCast(IOConfigDirectory, fEntries->getObject(start-fStart-1));
    if(subDir)
        subDir->retain();
    return subDir;
}

IOLocalConfigDirectory *IOLocalConfigDirectory::create()
{
    IOLocalConfigDirectory *dir;
    dir = new IOLocalConfigDirectory;
    if(!dir)
        return NULL;

    if(!dir->init()) {
        dir->release();
        return NULL;
    }
    return dir;
}

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
	
IOReturn IOLocalConfigDirectory::compile(OSData *rom)
{
    UInt32 header;
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
    for(i=0; i<numEntries; i++) {
        IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
        UInt32 val;
        if(!entry)
            return kIOReturnInternalError;	// Oops!

        switch(entry->fType) {
            case kConfigImmediateKeyType:
                if(entry->fKey == kConfigGenerationKey)
                    entry->fValue++;
                val = entry->fValue;
                break;
            case kConfigOffsetKeyType:
                val = (entry->fAddr.addressLo -
                       kConfigROMBaseAddress)/sizeof(UInt32);
                break;
            case kConfigLeafKeyType:
            case kConfigDirectoryKeyType:
                val = numEntries-i+offset;
                offset += entry->totalSize();
                break;
            default:
                return kIOReturnInternalError;	// Oops!
        }
        val |= entry->fKey << kConfigEntryKeyValuePhase;
        val |= entry->fType << kConfigEntryKeyTypePhase;
        crc = FWUpdateCRC16(crc, val);
        tmp->appendBytes(&val, sizeof(UInt32));
    }
    header = numEntries << kConfigLeafDirLengthPhase;
    header |= crc;
    rom->appendBytes(&header, sizeof(UInt32));
    rom->appendBytes(tmp);
    tmp->release();

    // Now (recursively) append each leaf and directory.
    for(i=0; i<numEntries; i++) {
        IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
        UInt32 val;
        if(!entry)
            return kIOReturnInternalError;	// Oops!
        switch(entry->fType) {
            case kConfigImmediateKeyType:
            case kConfigOffsetKeyType:
                break;
            case kConfigLeafKeyType:
            {
                OSData *data = OSDynamicCast(OSData, entry->fData);
                unsigned int len;
                if(!data)
                    return kIOReturnInternalError;	// Oops!
                len = data->getLength() / sizeof(UInt32);
                crc = FWComputeCRC16((const UInt32 *)data->getBytesNoCopy(), len);
                val = len << kConfigLeafDirLengthPhase;
                val |= crc;
                rom->appendBytes(&val, sizeof(UInt32));
                rom->appendBytes(data);
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

IOReturn IOLocalConfigDirectory::addEntry(int key, UInt32 value, OSString *desc = NULL)
{
    IOReturn res;

    IOConfigEntry *entry = IOConfigEntry::create(key, value);
    if(!entry)
        return kIOReturnNoMemory;
    if(!fEntries->setObject(entry))
        res = kIOReturnNoMemory;
    else
        res = kIOReturnSuccess;
    entry->release();	// In array now.
    return res;
}
IOReturn IOLocalConfigDirectory::addEntry(int key, IOLocalConfigDirectory *value,
                          OSString *desc = NULL)
{
    IOReturn res;

    IOConfigEntry *entry = IOConfigEntry::create(key, kConfigDirectoryKeyType, value);
    if(!entry)
        return kIOReturnNoMemory;
    if(!fEntries->setObject(entry))
        res = kIOReturnNoMemory;
    else
        res = kIOReturnSuccess;
    entry->release();	// In array now.
    return res;
}

IOReturn IOLocalConfigDirectory::addEntry(int key, OSData *value, OSString *desc = NULL)
{
    IOReturn res;

    IOConfigEntry *entry = IOConfigEntry::create(key, kConfigLeafKeyType, value);
    if(!entry)
        return kIOReturnNoMemory;
    if(!fEntries->setObject(entry))
        res = kIOReturnNoMemory;
    else
        res = kIOReturnSuccess;
    entry->release();	// In array now.
    return res;
}

IOReturn IOLocalConfigDirectory::addEntry(int key, FWAddress value, OSString *desc = NULL)
{
    IOReturn res;

    IOConfigEntry *entry = IOConfigEntry::create(key, value);
    if(!entry)
        return kIOReturnNoMemory;
    if(!fEntries->setObject(entry))
        res = kIOReturnNoMemory;
    else
        res = kIOReturnSuccess;
    entry->release();	// In array now.
    return res;
}

IOReturn IOLocalConfigDirectory::removeSubDir(IOLocalConfigDirectory *value)
{
    unsigned int i, numEntries;
    numEntries = fEntries->getCount();

    for(i=0; i<numEntries; i++) {
        IOConfigEntry *entry = OSDynamicCast(IOConfigEntry, fEntries->getObject(i));
        if(!entry)
            return kIOReturnInternalError;	// Oops!
        if(entry->fType == kConfigDirectoryKeyType) {
            if(entry->fData == value) {
                fEntries->removeObject(i);
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