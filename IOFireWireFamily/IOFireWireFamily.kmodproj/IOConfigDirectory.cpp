/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
#include <IOConfigDirectory.h>
#include <IOFireWireDevice.h>

#include "IORemoteConfigDirectory.h"

static int findIndex(const UInt32* base, int size, int key,
                     UInt32 type = kInvalidConfigROMEntryType);


class IOConfigDirectoryIterator : public OSIterator
{
    OSDeclareDefaultStructors(IOConfigDirectoryIterator)

protected:
    IOConfigDirectory *	fCurrent;
    IOConfigDirectory *	fOwner;
    int			fPos;
    UInt32		fTestVal;
    UInt32		fTestMask;
    
    virtual void free();

public:
    virtual bool init(IOConfigDirectory *owner, UInt32 testVal, UInt32 testMask);
    virtual void reset();

    virtual bool isValid();

    virtual OSObject *getNextObject();
};

OSDefineMetaClassAndStructors(IOConfigDirectoryIterator, OSIterator)

bool IOConfigDirectoryIterator::init(IOConfigDirectory *owner,
                                  UInt32 testVal, UInt32 testMask)
{
    if(!OSIterator::init())
        return false;
    fTestVal = testVal;
    fTestMask = testMask;
    fOwner = owner;
    fOwner->retain();

    return true;
}

void IOConfigDirectoryIterator::free()
{
    fOwner->release();
    if(fCurrent)
        fCurrent->release();
    OSIterator::free();
}

void IOConfigDirectoryIterator::reset()
{
    fPos = 0;
}

bool IOConfigDirectoryIterator::isValid()
{
    return true;
}

OSObject *IOConfigDirectoryIterator::getNextObject()
{
    IOConfigDirectory *next = NULL;
    if(fCurrent)
        fCurrent->release();

    while(fPos < fOwner->getNumEntries()) {
        UInt32 value;
        fOwner->getIndexEntry(fPos, value);
        if( (value & fTestMask) == fTestVal) {
            fOwner->getIndexValue(fPos++, next);
            break;
        }
        fPos++;
    }
    fCurrent = next;
    return next;
}

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

bool
IOConfigDirectory::initWithOffset(int start, int type)
{
    IOReturn err;
    const UInt32 *data;
    if(!OSObject::init())
        return false;

    fStart = start;
    fType = type;
    err = update(start, data);
    if(kIOReturnSuccess != err)
        return false;
    fNumEntries = (data[start] & kConfigLeafDirLength) >>
        kConfigLeafDirLengthPhase;
    err = update(start+fNumEntries, data);
    if(kIOReturnSuccess != err)
        return false;
    return true;
}

IOReturn IOConfigDirectory::createIterator(UInt32 testVal, UInt32 testMask,
                                        OSIterator *&iterator)
{
    IOConfigDirectoryIterator *iter = NULL;
    iter = new IOConfigDirectoryIterator;
    if(!iter)
        return kIOReturnNoMemory;

    if(!iter->init(this, testVal, testMask)) {
        iter->release();
        return kIOReturnNoMemory;
    }
    iterator = iter;
    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getKeyType(int key, IOConfigKeyType &type)
{
    int index;
    index = findIndex(getBase(), fNumEntries, key);
    if(index < 0)
        return kIOConfigNoEntry;
    
    return getIndexType(index, type);
}

IOReturn IOConfigDirectory::getKeyValue(int key, UInt32 &value, OSString** text)
{
    int index;
    IOReturn err;
    index = findIndex(getBase(), fNumEntries, key);
    if(index < 0)
        return kIOConfigNoEntry;

    err = getIndexValue(index, value);
    if(kIOReturnSuccess == err && text) {
        *text = NULL;
        getIndexValue(index+1, *text);
    }
    return err;
}

IOReturn IOConfigDirectory::getKeyValue(int key, OSData *&value, OSString** text)
{
    int index;
    IOReturn err;
    index = findIndex(getBase(), fNumEntries, key, kConfigLeafKeyType);
    if(index < 0)
        return kIOConfigNoEntry;

    err = getIndexValue(index, value);
    if(kIOReturnSuccess == err && text) {
        *text = NULL;
        getIndexValue(index+1, *text);
    }
    return err;
}

IOReturn IOConfigDirectory::getKeyValue(int key, IOConfigDirectory *&value,
                                                        OSString** text)
{
    int index;
    IOReturn err;
    index = findIndex(getBase(), fNumEntries, key, kConfigDirectoryKeyType);
    if(index < 0)
        return kIOConfigNoEntry;

    err = getIndexValue(index, value);
    if(kIOReturnSuccess == err && text) {
        *text = NULL;
        getIndexValue(index+1, *text);
    }
    return err;
}

IOReturn IOConfigDirectory::getKeyOffset(int key, FWAddress &value, OSString** text)
{
    int index;
    IOReturn err;
    index = findIndex(getBase(), fNumEntries, key, kConfigOffsetKeyType);
//	IOLog("IOConfigDirectory::getKeyOffset: this=0x%08X, fNumEntries=%u, index=%u\n", this, fNumEntries, index) ;
    if(index < 0)
        return kIOConfigNoEntry;

    err = getIndexOffset(index, value);
//	IOLog("IOConfigDirectory::getKeyOffset: value=%04X:%08lX\n", value.addressHi, value.addressLo) ;
    if(kIOReturnSuccess == err && text) {
        *text = NULL;
        getIndexValue(index+1, *text);
    }
    return err;
}

IOReturn IOConfigDirectory::getKeySubdirectories(int key, OSIterator *&iterator)
{
    IOReturn result = createIterator((key << kConfigEntryKeyValuePhase) |
							(kConfigDirectoryKeyType << kConfigEntryKeyTypePhase),
								kConfigEntryKeyType | kConfigEntryKeyValue, iterator);
	
	return result;
}

IOReturn IOConfigDirectory::getIndexType(int index, IOConfigKeyType &type)
{
    UInt32 entry;
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;
    
    entry = getBase()[index];
    type = (IOConfigKeyType)((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase);
    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getIndexKey(int index, int &key)
{
    UInt32 entry;
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    key = (IOConfigKeyType)((entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase);
    return kIOReturnSuccess;
}


IOReturn IOConfigDirectory::getIndexValue(int index, UInt32 &value)
{
    UInt32 entry;
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    // Return the value as an integer, whatever it really is.
    value = entry & kConfigEntryValue;
    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getIndexValue(int index, OSData *&value)
{
    UInt32 entry;
    IOReturn err;
    const UInt32 *data;
    UInt32 offset;
    int len;


    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) !=
        kConfigLeafKeyType)
        return kIOReturnBadArgument;

    err = getIndexOffset(index, offset);
    if(kIOReturnSuccess != err)
        return err;

    err = update(offset, data);
    if(kIOReturnSuccess != err)
        return err;

    len = (data[offset] & kConfigLeafDirLength) >> kConfigLeafDirLengthPhase;
    err = update(offset+len, data);
    if(kIOReturnSuccess != err)
        return err;

    value = OSData::withBytes(data+offset+1, len*sizeof(UInt32));
    	
    return value ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn IOConfigDirectory::getIndexValue(int index, OSString *&value)
{
    UInt32 entry;
    IOReturn err;
    const UInt32 *data;
    UInt32 offset;
    int len;
    
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    if( ((entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase) !=
                                            kConfigTextualDescriptorKey)
        return kIOReturnBadArgument;

    if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) !=
        kConfigLeafKeyType)
        return kIOReturnBadArgument;

    
    err = getIndexOffset(index, offset);
    if(kIOReturnSuccess != err)
        return err;


    err = update(offset, data);
    if(kIOReturnSuccess != err)
        return err;

    len = (data[offset] & kConfigLeafDirLength) >> kConfigLeafDirLengthPhase;

    // Check for silly length, people are careless with string data!
    if(len > 256) {
        return kIOReturnBadArgument;
    }
    
    err = update(offset+len, data);
    if(kIOReturnSuccess != err)
        return err;

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
    return value ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn IOConfigDirectory::getIndexValue(int index, IOConfigDirectory *&value)
{
    UInt32 entry;
    IOReturn err;
    UInt32 offset;

    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    if( ((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) !=
                                                kConfigDirectoryKeyType)
        return kIOReturnBadArgument;

    err = getIndexOffset(index, offset);
    if(kIOReturnSuccess != err)
        return err;

    value = getSubDir(offset,
                (entry & kConfigEntryKeyValue) >> kConfigEntryKeyValuePhase);
    return value ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn IOConfigDirectory::getIndexOffset(int index, FWAddress &value)
{
	//IOLog("IOConfigDirectory::getIndexOffset: this=0x%08X, fNumEntries=%u, index=%u\n", this, fNumEntries, index) ;

    UInt32 entry;
    UInt32 offset;
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) ==
					kConfigImmediateKeyType)
        return kIOReturnBadArgument;

    value.addressHi = kCSRRegisterSpaceBaseAddressHi;
    offset = entry & kConfigEntryValue;
    if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) ==
       kConfigOffsetKeyType) {
        value.addressLo = kCSRCoreRegistersBaseAddress + offset*sizeof(UInt32);
    }
    else {
        offset += fStart + 1 + index;
        value.addressLo = kConfigROMBaseAddress + offset*sizeof(UInt32);
    }

    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getIndexOffset(int index, UInt32 &value)
{
    UInt32 entry;
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    entry = getBase()[index];
    if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) ==
                                        kConfigImmediateKeyType)
        return kIOReturnBadArgument;
    else if(((entry & kConfigEntryKeyType) >> kConfigEntryKeyTypePhase) ==
       kConfigOffsetKeyType) {
        return kIOReturnBadArgument;
    }

    value = entry & kConfigEntryValue;
    value += fStart + 1 + index;

    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getIndexEntry(int index, UInt32 &value)
{
    if(index < 0 || index >= fNumEntries)
        return kIOReturnBadArgument;

    value = getBase()[index];
    return kIOReturnSuccess;
}

IOReturn IOConfigDirectory::getSubdirectories(OSIterator *&iterator)
{
    return createIterator(kConfigDirectoryKeyType << kConfigEntryKeyTypePhase,
                          kConfigEntryKeyType, iterator);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IORemoteConfigDirectory, IOConfigDirectory)
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 0);
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 1);
OSMetaClassDefineReservedUnused(IORemoteConfigDirectory, 2);


bool
IORemoteConfigDirectory::initWithOwnerOffset(IOFireWireDevice *owner, OSData *rom,
                         int start, int type)
{

    // Do this first so that init can load ROM
    fOwner = owner;
    fOwner->retain();
    fROM = rom;
    fROM->retain();

    if(!IOConfigDirectory::initWithOffset(start, type)) {
        fOwner->release();
        fOwner = NULL;
        return false;       
    }

    return true;
}

void
IORemoteConfigDirectory::free()
{
    if(fOwner)
        fOwner->release();
    if(fROM)
        fROM->release();
    IOConfigDirectory::free();
}

IOConfigDirectory *
IORemoteConfigDirectory::withOwnerOffset(IOFireWireDevice *owner, OSData *rom,
                                           int start, int type)
{
    IORemoteConfigDirectory *dir;

    dir = new IORemoteConfigDirectory;
    if(!dir)
        return NULL;

    if(!dir->initWithOwnerOffset(owner, rom, start, type)) {
        dir->release();
        dir = NULL;
    }
    return dir;
}

const UInt32 *IORemoteConfigDirectory::getBase()
{
    return ((const UInt32 *)fROM->getBytesNoCopy())+fStart+1;
}

IOReturn IORemoteConfigDirectory::update(UInt32 offset, const UInt32 *&romBase)
{
    return fOwner->cacheROM(fROM, offset, romBase);
}

IOConfigDirectory *
IORemoteConfigDirectory::getSubDir(int start, int type)
{
    return withOwnerOffset(fOwner, fROM, start, type);
}

