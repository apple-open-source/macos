/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOPEFLibraries.h"
#include "IONDRV.h"

#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/ndrvsupport/IONDRVSupport.h>
#include <IOKit/ndrvsupport/IONDRVLibraries.h>
#include <IOKit/graphics/IOGraphicsPrivate.h>

#include <IOKit/assert.h>

#include <pexpert/pexpert.h>
#include <string.h>

extern "C"
{
#include <kern/debug.h>

extern void printf(const char *, ...);
extern void *kern_os_malloc(size_t size);
extern void kern_os_free(void * addr);

#define LOG		if(1) IOLog
#define LOGNAMEREG	0

#define CHECK_INTERRUPT(s)					\
if( ml_at_interrupt_context()) {				\
    /* IOLog("interrupt:%s(%s)\n", __FUNCTION__, s); */		\
    return( nrLockedErr );					\
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//#define EXP(s)	_e ## s
#define EXP(s)	s

UInt32 EXP(EndianSwap32Bit)( UInt32 data )
{
    return (OSReadSwapInt32(&data, 0));
}

UInt16 EXP(EndianSwap16Bit)( UInt16 data )
{
    return (OSReadSwapInt16(&data, 0));
}

OSErr EXP(ExpMgrConfigReadLong)( RegEntryID * entryID, LogicalAddress offset, UInt32 * value )
{
    IORegistryEntry *	regEntry;
    IOPCIDevice *	ioDevice;
    UInt32		adj;

    REG_ENTRY_TO_OBJ( entryID, regEntry)

    ioDevice = OSDynamicCast( IOPCIDevice, regEntry );
    if (!ioDevice)
	ioDevice = OSDynamicCast( IOPCIDevice, regEntry->getParentEntry( gIODTPlane) );
    if (!ioDevice)
	return (nrNotSlotDeviceErr);

    adj = ioDevice->configRead32( (UInt32) offset );
#if 0
    IOMemoryMap *	map = 0;
    if ((offset >= kIOPCIConfigBaseAddress2)
	    && (offset <= kIOPCIConfigBaseAddress5))
    {
	if ((map = ioDevice->mapDeviceMemoryWithRegister(offset, kIOMapReference)))
	{
	    adj = (adj & 3) | (map->getVirtualAddress());
	    map->release();
	}
    }
#endif
    *value = adj;

    return (noErr);
}

OSErr EXP(ExpMgrConfigWriteLong)( RegEntryID * entryID, LogicalAddress offset, UInt32 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->configWrite32( (UInt32) offset, value);

    return (noErr);
}


OSErr EXP(ExpMgrConfigReadWord)( RegEntryID * entryID, LogicalAddress offset, UInt16 * value )
{
    IORegistryEntry *	regEntry;
    IOPCIDevice *	ioDevice;

    REG_ENTRY_TO_OBJ( entryID, regEntry)

    ioDevice = OSDynamicCast( IOPCIDevice, regEntry );
    if (!ioDevice)
	ioDevice = OSDynamicCast( IOPCIDevice, regEntry->getParentEntry( gIODTPlane) );
    if (!ioDevice)
	return (nrNotSlotDeviceErr);

    *value = ioDevice->configRead16( (UInt32) offset );

    return (noErr);
}

OSErr EXP(ExpMgrConfigWriteWord)( RegEntryID * entryID, LogicalAddress offset, UInt16 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->configWrite16( (UInt32) offset, value);

    return (noErr);
}

OSErr EXP(ExpMgrConfigReadByte)( RegEntryID * entryID, LogicalAddress offset, UInt8 * value )
{
    IORegistryEntry *	regEntry;
    IOPCIDevice *	ioDevice;

    REG_ENTRY_TO_OBJ( entryID, regEntry)

    ioDevice = OSDynamicCast( IOPCIDevice, regEntry );
    if (!ioDevice)
	ioDevice = OSDynamicCast( IOPCIDevice, regEntry->getParentEntry( gIODTPlane) );
    if (!ioDevice)
	return (nrNotSlotDeviceErr);

    *value = ioDevice->configRead8( (UInt32) offset );

    return (noErr);
}

OSErr EXP(ExpMgrConfigWriteByte)( RegEntryID * entryID, LogicalAddress offset, UInt8 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->configWrite8( (UInt32) offset, value);

    return (noErr);
}

OSErr EXP(ExpMgrIOReadLong)( RegEntryID * entryID, LogicalAddress offset, UInt32 * value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    *value = ioDevice->ioRead32( (UInt32) offset );

    return (noErr);
}

OSErr EXP(ExpMgrIOWriteLong)( RegEntryID * entryID, LogicalAddress offset, UInt32 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->ioWrite32( (UInt32) offset, value );

    return (noErr);
}

OSErr EXP(ExpMgrIOReadWord)( RegEntryID * entryID, LogicalAddress offset, UInt16 * value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    *value = ioDevice->ioRead16( (UInt32) offset );

    return (noErr);
}

OSErr EXP(ExpMgrIOWriteWord)( RegEntryID * entryID, LogicalAddress offset, UInt16 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->ioWrite16( (UInt32) offset, value );

    return (noErr);
}

OSErr EXP(ExpMgrIOReadByte)( RegEntryID * entryID, LogicalAddress offset, UInt8 * value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    *value = ioDevice->ioRead8( (UInt32) offset );

    return (noErr);
}

OSErr EXP(ExpMgrIOWriteByte)( RegEntryID * entryID, LogicalAddress offset, UInt8 value )
{
    REG_ENTRY_TO_SERVICE( entryID, IOPCIDevice, ioDevice)

    ioDevice->ioWrite8( (UInt32) offset, value );

    return (noErr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSStatus EXP(RegistryEntryIDCopy)( const RegEntryID * entryID, RegEntryID * to )
{
    bcopy( entryID, to, sizeof( RegEntryID) );
    return (noErr);
}

OSStatus EXP(RegistryEntryIDInit)( RegEntryID * entryID )
{
    MAKE_REG_ENTRY( entryID, 0);
    return (noErr);
}

OSStatus EXP(RegistryEntryIDDispose)
    (RegEntryID * entryID)
{
    return (noErr);
}

OSStatus EXP(RegistryEntryDelete)
    (const RegEntryID * entryID)
{
    return (noErr);
}

/*
    * Compare EntryID's for equality or if invalid
    *
    * If a NULL value is given for either id1 or id2, the other id 
    * is compared with an invalid ID.  If both are NULL, the id's 
    * are consided equal (result = true). 
    *   note: invalid != uninitialized
    */
Boolean EXP(RegistryEntryIDCompare)( const RegEntryID * entryID1, const RegEntryID * entryID2 )
{
    IORegistryEntry *	regEntry1;
    IORegistryEntry *	regEntry2;

    if (entryID1)
    {
	REG_ENTRY_TO_OBJ_RET( entryID1, regEntry1, false)
    }
    else
	regEntry1 = 0;

    if (entryID2)
    {
	REG_ENTRY_TO_OBJ_RET( entryID2, regEntry2, false)
    }
    else
	regEntry2 = 0;

    return (regEntry1 == regEntry2);
}

OSStatus EXP(RegistryPropertyGetSize)( const RegEntryID * entryID,
                                       const RegPropertyName * propertyName,
                                       RegPropertyValueSize * propertySize )
{
    OSStatus		err = noErr;
    OSData *		prop;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    prop = OSDynamicCast( OSData, regEntry->getProperty( propertyName));
    if (prop)
	*propertySize = prop->getLength();
    else
	err = nrNotFoundErr;

#if LOGNAMEREG
    LOG("RegistryPropertyGetSize: %s : %d\n", propertyName, err);
#endif
    return (err);
}

OSStatus EXP(RegistryPropertyGet)(const RegEntryID * entryID, 
                                    const RegPropertyName * propertyName, void *propertyValue,
                                    RegPropertyValueSize * propertySize)
{
    OSStatus		err = noErr;
    OSData *		prop;
    UInt32		len;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    prop = OSDynamicCast( OSData, regEntry->getProperty( propertyName));
    if (prop)
    {
	len = *propertySize;
	*propertySize = prop->getLength();
	len = (len > prop->getLength()) ? prop->getLength() : len;
	bcopy( prop->getBytesNoCopy(), propertyValue, len);
#if LOGNAMEREG
	LOG("value: %08x ", *propertyValue);
#endif

    }
    else
	err = nrNotFoundErr;

#if LOGNAMEREG
    LOG("RegistryPropertyGet: %s : %d\n", propertyName, err);
#endif
    return (err);
}

OSStatus EXP(RegistryPropertyCreate)( const RegEntryID * entryID, const RegPropertyName * propertyName,
				      const void * propertyValue, RegPropertyValueSize propertySize )
{
    OSStatus		err = noErr;
    OSData *		prop;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    prop = OSData::withBytes( propertyValue, propertySize );

    if (prop)
    {
	regEntry->setProperty( propertyName, prop);
	prop->release();
    }
    else
	err = nrNotCreatedErr;

#if LOGNAMEREG
    LOG("RegistryPropertyCreate: %s : %d\n", propertyName, err);
#endif
    return (err);
}

OSStatus EXP(RegistryPropertyDelete)( const RegEntryID * entryID, const RegPropertyName * propertyName )
{
    OSStatus		err = noErr;
    OSObject *		old;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    old = regEntry->getProperty(propertyName);
    if (old)
	regEntry->removeProperty(propertyName);
    else
	err = nrNotFoundErr;

#if LOGNAMEREG
    LOG("RegistryPropertyDelete: %s : %d\n", propertyName, err);
#endif
    return (err);
}

void IONDRVSetNVRAMPropertyName( IORegistryEntry * regEntry,
				    const OSSymbol * sym )
{
    regEntry->setProperty( "IONVRAMProperty", sym );
}

static IOReturn IONDRVSetNVRAMPropertyValue( IORegistryEntry * regEntry,
	const OSSymbol * name, OSData * value )
{
    IOReturn			err;
    IODTPlatformExpert *	platform =
	(IODTPlatformExpert *) IOService::getPlatform();

    err = platform->writeNVRAMProperty( regEntry, name, value );

    return (err);
}

OSStatus EXP(RegistryPropertySet)( const RegEntryID * entryID, 
                                    const RegPropertyName * propertyName, 
                                    const void * propertyValue, RegPropertyValueSize propertySize )
{
    OSStatus			err = noErr;
    OSData *			prop;
    const OSSymbol *		sym;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    sym = OSSymbol::withCString( propertyName );
    if (!sym)
	return (kIOReturnNoMemory);

    prop = OSDynamicCast( OSData, regEntry->getProperty( sym ));
    if (0 == prop)
	err = nrNotFoundErr;

    else if ((prop = OSData::withBytes(propertyValue, propertySize)))
    {
	regEntry->setProperty( sym, prop);

	if ((sym == (const OSSymbol *)
		regEntry->getProperty("IONVRAMProperty")))
	    err = IONDRVSetNVRAMPropertyValue( regEntry, sym, prop );
	prop->release();
    }
    else
	err = nrNotCreatedErr;

    sym->release();

#if LOGNAMEREG
    LOG("RegistryPropertySet: %s : %d\n", propertyName, err);
#endif
    return (err);
}

OSStatus EXP(RegistryPropertyGetMod)(const RegEntryID * entryID,
                                     const RegPropertyName * propertyName, 
				     RegPropertyModifiers * mod)
{
    const OSSymbol *	sym;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    if ((sym = OSDynamicCast(OSSymbol,
				regEntry->getProperty("IONVRAMProperty")))
	    && (0 == strcmp(propertyName, sym->getCStringNoCopy())))

	*mod = kNVRAMProperty;
    else
	*mod = 0;

    return (noErr);
}

OSStatus EXP(RegistryPropertySetMod)(const RegEntryID * entryID,
                                    const RegPropertyName * propertyName,
				    RegPropertyModifiers mod )
{
    OSStatus		err = noErr;
    OSData *		data;
    const OSSymbol *	sym;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    if ((mod & kNVRAMProperty)
	    && (sym = OSSymbol::withCString(propertyName)))
    {
	if ((data = OSDynamicCast(OSData, regEntry->getProperty(sym))))
	{
	    err = IONDRVSetNVRAMPropertyValue( regEntry, sym, data );
	    if (kIOReturnSuccess == err)
		IONDRVSetNVRAMPropertyName( regEntry, sym );
	}
	sym->release();
    }

    return (err);
}

OSStatus EXP(VSLSetDisplayConfiguration)(RegEntryID * entryID,
					char *	propertyName,
					void *	configData,
					long	configDataSize)
{
    IOReturn		err = nrNotCreatedErr;
    IORegistryEntry *	options;
    const OSSymbol *	sym = 0;
    OSData *		data = 0;
    enum {		kMaxDisplayConfigDataSize = 64 };

    if ((configDataSize > kMaxDisplayConfigDataSize)
	    || (strlen(propertyName) > kRegMaximumPropertyNameLength))
	return (nrNotCreatedErr);

    do
    {
	options = IORegistryEntry::fromPath( "/options", gIODTPlane);
	if (!options)
	    continue;
	data = OSData::withBytes( configData, configDataSize );
	if (!data)
	    continue;
	sym = OSSymbol::withCString( propertyName );
	if (!sym)
	    continue;
	if (!options->setProperty(sym, data))
	    continue;
	err = kIOReturnSuccess;
    }
    while (false);

    if (options)
	options->release();
    if (data)
	data->release();
    if (sym)
	sym->release();

    return (err);
}

OSStatus EXP(RegistryPropertyIterateCreate)( const RegEntryID * entryID,
	OSIterator ** cookie)
{
    CHECK_INTERRUPT("")
    REG_ENTRY_TO_PT( entryID, regEntry)

    // NB. unsynchronized. But should only happen on an owned nub!
    // Should non OSData be filtered out?

    OSDictionary * dict = regEntry->dictionaryWithProperties();
    *cookie = OSCollectionIterator::withCollection(dict);
    if (dict)
	dict->release();

    if (*cookie)
	return (noErr);
    else
	return (nrNotEnoughMemoryErr);
}

OSStatus EXP(RegistryPropertyIterateDispose)( OSIterator ** cookie)
{
    if (*cookie)
    {
	(*cookie)->release();
	*cookie = NULL;
	return (noErr);
    }
    else
	return (nrIterationDone);
}

OSStatus EXP(RegistryPropertyIterate)( OSIterator ** cookie,
				    char * name, Boolean * done )
{
    const OSSymbol *	key;

    key = (const OSSymbol *) (*cookie)->getNextObject();
    if (key)
	strncpy( name, key->getCStringNoCopy(), kRegMaximumPropertyNameLength);

    // Seems to be differences in handling "done".
    // ATI assumes done = true when getting the last property.
    // The Book says done is true after last property.
    // ATI does check err, so this will work.
    // Control ignores err and checks done.

    *done = (key == 0);

    if (0 != key)
	return (noErr);
    else
	return (nrIterationDone);
}

OSStatus
EXP(RegistryEntryIterateCreate)( IORegistryIterator ** cookie)
{
    *cookie = IORegistryIterator::iterateOver( gIODTPlane );
    if (*cookie)
	return (noErr);
    else
	return (nrNotEnoughMemoryErr);
}

OSStatus
EXP(RegistryEntryIterateDispose)( IORegistryIterator ** cookie)
{
    if (*cookie)
    {
	(*cookie)->release();
	*cookie = NULL;
	return (noErr);
    }
    else
	return (nrIterationDone);
}

OSStatus EXP(RegistryEntryIterateSet)( IORegistryIterator ** cookie,
				    const RegEntryID *startEntryID)
{
    IORegistryEntry *	regEntry;

    REG_ENTRY_TO_OBJ( startEntryID, regEntry)

    if (*cookie)
	(*cookie)->release();
    *cookie = IORegistryIterator::iterateOver( regEntry, gIODTPlane );
    if (*cookie)
	return (noErr);
    else
	return (nrNotEnoughMemoryErr);
}

OSStatus
EXP(RegistryEntryIterate)( IORegistryIterator **	cookie,
			UInt32		/* relationship */,
			RegEntryID * 	foundEntry,
			Boolean *	done)
{
    IORegistryEntry *	regEntry;

    // TODO: check requested type of iteration
    regEntry = (*cookie)->getNextObjectRecursive();

    MAKE_REG_ENTRY( foundEntry, regEntry);
    *done = (0 == regEntry);

#if LOGNAMEREG
    if (regEntry)
	LOG("RegistryEntryIterate: %s\n", regEntry->getName( gIODTPlane ));
#endif

    if (regEntry)
	return (noErr);
    else
	return (nrNotFoundErr);
}

OSStatus
EXP(RegistryCStrEntryToName)( const RegEntryID *	entryID,
			    RegEntryID *		parentEntry,
			    char *			nameComponent,
			    Boolean *			done )
{
    IORegistryEntry *	regEntry;

    REG_ENTRY_TO_OBJ( entryID, regEntry)

    strncpy( nameComponent, regEntry->getName( gIODTPlane ), kRegMaximumPropertyNameLength );
    nameComponent[ kRegMaximumPropertyNameLength ] = 0;

    regEntry = regEntry->getParentEntry( gIODTPlane );
    if (regEntry)
    {
	MAKE_REG_ENTRY( parentEntry, regEntry);
	*done = false;
    }
    else
	*done = true;

    return (noErr);
}

OSStatus
EXP(RegistryCStrEntryLookup)( const RegEntryID *	parentEntry,
			      const RegCStrPathName *	path,
			      RegEntryID *		newEntry)
{
    IOReturn		err;
    IORegistryEntry *	regEntry = 0;
    char *		buf;
    char *		cvtPath;
    char		c;
#define kDTRoot		"Devices:device-tree:"
#define kMacIORoot	"Devices:device-tree:pci:mac-io:"

    if (parentEntry)
    {
	REG_ENTRY_TO_OBJ( parentEntry, regEntry)
    }
    else
	regEntry = 0;

    buf = IONew( char, 512 );
    if (!buf)
	return (nrNotEnoughMemoryErr);

    cvtPath = buf;
    if (':' == path[0])
	path++;
    else if (0 == strncmp(path, kMacIORoot, strlen(kMacIORoot)))
    {
	path += strlen( kMacIORoot ) - 7;
	regEntry = 0;
    }
    else if (0 == strncmp(path, kDTRoot, strlen(kDTRoot)))
    {
	path += strlen( kDTRoot ) - 1;
	regEntry = 0;
    }

    do
    {
	c = *(path++);
	if (':' == c)
	    c = '/';
	*(cvtPath++) = c;
    }
    while (c != 0);

    if (regEntry)
	regEntry = regEntry->childFromPath( buf, gIODTPlane );
    else
	regEntry = IORegistryEntry::fromPath( buf, gIODTPlane );

    if (regEntry)
    {
	MAKE_REG_ENTRY( newEntry, regEntry);
	regEntry->release();
	err = noErr;
    }
    else
	err = nrNotFoundErr;

    IODelete( buf, char, 512 );

    return (err);
}


OSStatus
EXP(RegistryCStrEntryCreate)( const RegEntryID *	parentEntry,
			      const RegCStrPathName *	name,
			      RegEntryID *		newEntry)
{
    IORegistryEntry *	newDev;
    IORegistryEntry *	parent;

    REG_ENTRY_TO_OBJ( parentEntry, parent)

    // NOT published

    newDev = new IORegistryEntry;
    if (newDev && (false == newDev->init()))
	newDev = 0;

    if (newDev)
    {
	newDev->attachToParent( parent, gIODTPlane );
	if (':' == name[0])
	    name++;
	newDev->setName( name );
    }

    MAKE_REG_ENTRY( newEntry, newDev);

    if (newDev)
	return (noErr);
    else
	return (nrNotCreatedErr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C"
{
    // platform expert
    extern vm_offset_t
	PEResidentAddress( vm_offset_t address, vm_size_t length );
};

enum {
    kProcessorCacheModeDefault		= 0,
    kProcessorCacheModeInhibited 	= 1,
    kProcessorCacheModeWriteThrough 	= 2,
    kProcessorCacheModeCopyBack		= 3
};

OSStatus EXP(SetProcessorCacheMode)( UInt32 /* space */, void * /* addr */,
				    UInt32 /* len */, UInt32 /* mode */ )
{
#if 0
    struct phys_entry*	pp;
    vm_offset_t 	spa;
    vm_offset_t 	epa;
    int			wimg;

    // This doesn't change any existing kernel mapping eg. BAT changes etc.
    // but this is enough to change user level mappings for DPS etc.
    // Should use a kernel service when one is available.

    spa = kvtophys( (vm_offset_t)addr);
    if (spa == 0)
    {
	spa = PEResidentAddress( (vm_offset_t)addr, len);
	if (spa == 0)
	    return (kIOReturnVMError);
    }
    epa = (len + spa + 0xfff) & 0xfffff000;
    spa &=  0xfffff000;

    switch (mode)
    {
	case kProcessorCacheModeWriteThrough:
	    wimg = PTE_WIMG_WT_CACHED_COHERENT_GUARDED;
	    break;
	case kProcessorCacheModeCopyBack:
	    wimg = PTE_WIMG_CB_CACHED_COHERENT_GUARDED;
	    break;
	default:
	    wimg = PTE_WIMG_UNCACHED_COHERENT_GUARDED;
	    break;
    }

    while (spa < epa)
    {
	pp = pmap_find_physentry(spa);
	if (pp != PHYS_NULL)
	    pp->pte1.bits.wimg = wimg;
	spa += PAGE_SIZE;
    }
#endif
    OSSynchronizeIO();
    return (noErr);
}

Boolean EXP(CompareAndSwap)(
  UInt32    oldVvalue,
  UInt32    newValue,
  UInt32 *  OldValueAdr)
{
    return(OSCompareAndSwap(oldVvalue, newValue, OldValueAdr));
}

void EXP(SynchronizeIO)(void)
{
    OSSynchronizeIO();
}

void EXP(BlockCopy)(
  const void *  srcPtr,
  void *        destPtr,
  Size          byteCount)
{
    return(bcopy_nc((void *) srcPtr, destPtr, byteCount));
}

void EXP(BlockMove)(
  const void *  srcPtr,
  void *        destPtr,
  Size          byteCount)
{
    return(bcopy_nc((void *) srcPtr, destPtr, byteCount));
}

void EXP(BlockMoveData)(
  const void *  srcPtr,
  void *        destPtr,
  Size          byteCount)
{
    return(bcopy_nc((void *) srcPtr, destPtr, byteCount));
}

void EXP(BlockMoveDataUncached)(
  const void *  srcPtr,
  void *        destPtr,
  Size          byteCount)
{
    return(bcopy_nc((void *) srcPtr, destPtr, byteCount));
}

void EXP(BlockMoveUncached)(
  const void *  srcPtr,
  void *        destPtr,
  Size          byteCount)
{
    return(bcopy_nc((void *) srcPtr, destPtr, byteCount));
}

void EXP(BlockZero)(
  const void *  srcPtr,
  Size          byteCount)
{
    return(bzero_nc((void *) srcPtr,byteCount));
}

void EXP(BlockZeroUncached)(
  const void *  srcPtr,
  Size          byteCount)
{
    return(bzero_nc((void *) srcPtr,byteCount));
}

char * EXP(CStrCopy)( char * dst, const char * src)
{
    return(strcpy(dst,src));
}

SInt16 EXP(CStrCmp)(
  const char *  s1,
  const char *  s2)
{
    return(strcmp(s1, s2));
}

UInt32 EXP(CStrLen)(const char * src)
{
    return(strlen(src));
}

char * EXP(CStrCat)(
  char *        dst,
  const char *  src)
{
    return(strcat(dst, src));
}

char * EXP(CStrNCopy)(
  char *        dst,
  const char *  src,
  UInt32        max)
{
    return(strncpy(dst, src, max));
}

SInt16 EXP(CStrNCmp)(
  const char *  s1,
  const char *  s2,
  UInt32        max)
{
    return(strncmp(s1, s2, max));
}

char * EXP(CStrNCat)(
  char *        dst,
  const char *  src,
  UInt32        max)
{
    return(strncat(dst, src, max));
}

char * EXP(PStrCopy)( char *to, const char *from )
{
    UInt32	len;
    char   *	copy;

    copy = to;
    len = *(from++);
    *(copy++) = len;
    bcopy( from, copy, len);
    return (to);
}

void EXP(PStrToCStr)( char *to, const char *from )
{
    UInt32	len;

    len = *(from++);
    bcopy( from, to, len);
    *(to + len) = 0;
}

void EXP(CStrToPStr)( char *to, const char *from )
{
    UInt32	len;

    len = strlen(from);
    *to = len;
    bcopy( from, to + 1, len);
}

LogicalAddress EXP(PoolAllocateResident)(ByteCount byteSize, Boolean clear)
{
    UInt32 * mem;

    mem = (UInt32 *) kern_os_malloc( (size_t) byteSize + sizeof(UInt32) );

    if (mem)
    {
	mem[0] = 'mllc';
	mem++;
    }
    if (clear && mem)
	memset( (void *) mem, 0, byteSize);

    return ((LogicalAddress) mem);
}

OSStatus EXP(PoolDeallocate)( LogicalAddress address )
{
    UInt32 * mem = (UInt32 *) address;

    if (!mem)
	return (nrNotEnoughMemoryErr);

    mem--;
    if (mem[0] != 'mllc')
    {
//	IOLog("PoolDeallocate invalid address %08lx\n", mem + 1);
//	IOPanic("PoolDeallocate invalid address");
	return (nrInvalidNodeErr);
    }
    kern_os_free( (void *) mem );
    return (noErr);
}

UInt32	EXP(CurrentExecutionLevel)(void)
{
    if (ml_at_interrupt_context())
	return (6);		// == kTaskLevel, HWInt == 6
    else
	return (0);		// == kTaskLevel, HWInt == 6
}

// don't expect any callers of this
OSErr EXP(IOCommandIsComplete)( IOCommandID commandID, OSErr result)
{
    LOG("IOCommandIsComplete\n");
    return (result);		// !!??!!
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <kern/clock.h>


AbsoluteTime EXP(UpTime)( void )
{
    AbsoluteTime    result;

    clock_get_uptime( &result);

    return (result);
}

AbsoluteTime EXP(AddAbsoluteToAbsolute)(AbsoluteTime left, AbsoluteTime right)
{
    AbsoluteTime    result = left;

    ADD_ABSOLUTETIME( &result, &right);

    return (result);
}


AbsoluteTime EXP(SubAbsoluteFromAbsolute)(AbsoluteTime left, AbsoluteTime right)
{
    AbsoluteTime    result = left;

    // !! ATI bug fix here:
    // They expect the 64-bit result to be signed. The spec says < 0 => 0
    // To workaround, make sure this routine takes 10 us to execute.
    IODelay( 10);

    if (CMP_ABSOLUTETIME(&result, &right) < 0)
    {
	AbsoluteTime_to_scalar( &result ) = 0;
    }
    else
    {
	result = left;
	SUB_ABSOLUTETIME( &result, &right);
    }

    return (result);
}


AbsoluteTime    EXP(DurationToAbsolute)( Duration theDuration)
{
    AbsoluteTime    result;

    if (theDuration > 0)
    {
	clock_interval_to_absolutetime_interval( theDuration, kMillisecondScale,
		&result );
    }
    else
    {
	clock_interval_to_absolutetime_interval( (-theDuration), kMicrosecondScale,
		&result );
    }

    return (result);
}

AbsoluteTime EXP(AddDurationToAbsolute)( Duration duration, AbsoluteTime absolute )
{
    return (EXP(AddAbsoluteToAbsolute)(EXP(DurationToAbsolute)(duration), absolute));
}

#define UnsignedWideToUInt64(x)		(*(UInt64 *)(x))
#define UInt64ToUnsignedWide(x)		(*(UnsignedWide *)(x))

AbsoluteTime    EXP(NanosecondsToAbsolute) ( UnsignedWide theNanoseconds)
{
    AbsoluteTime result;
    UInt64	nano = UnsignedWideToUInt64(&theNanoseconds);

    nanoseconds_to_absolutetime( nano, &result);

    return (result);
}

UnsignedWide    EXP(AbsoluteToNanoseconds)( AbsoluteTime absolute )
{
    UnsignedWide result;
    UInt64	nano;

    absolutetime_to_nanoseconds( absolute, &nano);
    result = UInt64ToUnsignedWide( &nano );

    return (result);
}

Duration    EXP(AbsoluteDeltaToDuration)( AbsoluteTime left, AbsoluteTime right )
{
    Duration		dur;
    AbsoluteTime	result;
    UInt64		nano;

    if (CMP_ABSOLUTETIME(&left, &right) < 0)
	return (0);

    result = left;
    SUB_ABSOLUTETIME( &result, &right);
    absolutetime_to_nanoseconds( result, &nano);

    if (nano >= ((1ULL << 31) * 1000ULL))
    {
	// +ve milliseconds
	if (nano >= ((1ULL << 31) * 1000ULL * 1000ULL))
	    dur = 0x7fffffff;
	else
	    dur = nano / 1000000ULL;
    }
    else
    {
	// -ve microseconds
	dur = -(nano / 1000ULL);
    }

    return (dur);
}

Duration    EXP(AbsoluteToDuration)( AbsoluteTime result )
{
    Duration		dur;
    UInt64		nano;

    absolutetime_to_nanoseconds( result, &nano);

    if (nano >= ((1ULL << 31) * 1000ULL))
    {
	// +ve milliseconds
	if (nano >= ((1ULL << 31) * 1000ULL * 1000ULL))
	    dur = 0x7fffffff;
	else
	    dur = nano / 1000000ULL;
    }
    else
    {
	// -ve microseconds
	dur = -(nano / 1000ULL);
    }

    return (dur);
}

OSStatus    EXP(DelayForHardware)( AbsoluteTime time )
{
    AbsoluteTime	deadline;

    clock_absolutetime_interval_to_deadline( time, &deadline );
#ifdef __ppc__
    clock_delay_until( deadline );
#else
    {
	AbsoluteTime now;
	do {
	    clock_get_uptime(&now);
	} while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));
    }
#endif

    return (noErr);
}

OSStatus    EXP(DelayFor)( Duration theDuration )
{
#if 1

    // In Marconi, DelayFor uses the old toolbox Delay routine
    // which is based on the 60 Hz timer. Durations are not
    // rounded up when converting to ticks. Yes, really.
    // Some ATI drivers call DelayFor(1) 50000 times starting up.
    // There is some 64-bit math there so we'd better reproduce
    // the overhead of that calculation.

#define DELAY_FOR_TICK_NANO		16666666
#define DELAY_FOR_TICK_MILLI		17
#define NANO32_MILLI			4295

    UnsignedWide	nano;
    AbsoluteTime	abs;
    unsigned int	ms;

    abs = EXP(DurationToAbsolute)( theDuration);
    nano = EXP(AbsoluteToNanoseconds)( abs);

    ms = (nano.lo / DELAY_FOR_TICK_NANO) * DELAY_FOR_TICK_MILLI;
    ms += nano.hi * NANO32_MILLI;
    if (ms)
	IOSleep( ms);

#else
    // Accurate, but incompatible, version

#define SLEEP_THRESHOLD		5000

    if (theDuration < 0)
    {
	// us duration
	theDuration -= theDuration;
	if (theDuration > SLEEP_THRESHOLD)
	    IOSleep( (theDuration + 999) / 1000);
	else
	    IODelay( theDuration);
    }
    else
    {
	// ms duration
	if (theDuration > (SLEEP_THRESHOLD / 1000))
	    IOSleep( theDuration );                     	// ms
	else
	    IODelay( theDuration * 1000);			// us
    }
#endif

    return (noErr);
}

void EXP(SysDebug)(void)
{
}

void EXP(SysDebugStr)( const char * from )
{
    char format[8];
    static bool kprt = FALSE;
    static bool parsed = FALSE;

    sprintf( format, "%%%ds", from[0]);

    if (!parsed)
    {
	int	debugFlags;

	kprt = PE_parse_boot_arg("debug", &debugFlags) && (DB_KPRT & debugFlags);
	parsed = TRUE;
    }

    if (kprt)
	kprintf( format, from + 1);
    else
	printf( format, from + 1);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOReturn
ApplePMUSendMiscCommand( UInt32 command,
			    IOByteCount sendLength, UInt8 * sendBuffer,
			    IOByteCount * readLength, UInt8 * readBuffer )
{
    struct SendMiscCommandParameterBlock
    {
	int command;
	IOByteCount sLength;
	UInt8 *sBuffer;
	IOByteCount *rLength;
	UInt8 *rBuffer;
    };
    IOReturn ret = kIOReturnError;
    static IOService * pmu;

    // See if ApplePMU exists
    if (!pmu)
    {
	OSIterator * iter;
	iter = IOService::getMatchingServices(IOService::serviceMatching("ApplePMU"));
	if (iter)
	{
	    pmu = (IOService *) iter->getNextObject();
	    iter->release();
	}
    }

    SendMiscCommandParameterBlock params = { command, sendLength, sendBuffer,
					    readLength, readBuffer };
    if (pmu)
	ret = pmu->callPlatformFunction( "sendMiscCommand", true,
					    (void*)&params, NULL, NULL, NULL );
    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define	readExtSwitches	0xDC

extern IOOptionBits gIOFBLastClamshellState;
extern bool	    gIOFBDesktopModeAllowed;

OSStatus EXP(VSLGestalt)( VSLGestaltType selector, UInt32 * response )
{
    IOReturn ret;

    if (!response)
	return (paramErr);

    *response = 0;

    switch (selector)
    {
	case kVSLClamshellStateGestaltType:
	    {
		UInt8 bootEnvIntData[32];
		IOByteCount iLen = sizeof(UInt8);

		ret = ApplePMUSendMiscCommand(readExtSwitches, 0, NULL, &iLen, &bootEnvIntData[0]);
		if (kIOReturnSuccess == ret)
		{
		    gIOFBLastClamshellState = bootEnvIntData[0] & 1;
		    if (gIOFBDesktopModeAllowed)
			*response = bootEnvIntData[0];
		}
		break;
	    }
	default:
	    ret = gestaltUndefSelectorErr;
	    break;
    }

    return (ret);
}

OSErr
EXP(VSLNewInterruptService)(
  RegEntryID *            serviceDevice,
  InterruptServiceType    serviceType,
  InterruptServiceIDPtr   serviceID)
{
    return(IONDRVFramebuffer::VSLNewInterruptService(serviceDevice, serviceType, serviceID));
}

OSErr
EXP(VSLDisposeInterruptService)(InterruptServiceIDType serviceID)
{
    return(IONDRVFramebuffer::VSLDisposeInterruptService(serviceID));
}

OSErr
EXP(VSLDoInterruptService)(InterruptServiceIDType serviceID)
{
    return(IONDRVFramebuffer::VSLDoInterruptService(serviceID));
}

Boolean
EXP(VSLPrepareCursorForHardwareCursor)(
  void *                        cursorRef,
  HardwareCursorDescriptorPtr   hardwareDescriptor,
  HardwareCursorInfoPtr         hwCursorInfo)
{
    return(IONDRVFramebuffer::VSLPrepareCursorForHardwareCursor(cursorRef, 
				hardwareDescriptor, hwCursorInfo));
}

OSStatus _eCallOSTrapUniversalProc( UInt32 /* theProc */,
				    UInt32 procInfo, UInt32 trap, UInt8 * pb )
{
    OSStatus    err = -40;
#ifdef __ppc__
    struct PMgrOpParamBlock
    {
	SInt16	pmCommand;
	SInt16	pmLength;
	UInt8 *	pmSBuffer;
	UInt8 *	pmRBuffer;
	UInt8	pmData[4];
    };
    UInt8	bootEnvIntData[32];

    if ((procInfo == 0x133822)
	    && (trap == 0xa085))
    {
	PMgrOpParamBlock * pmOp = (PMgrOpParamBlock *) pb;

	if ((readExtSwitches == pmOp->pmCommand) && pmOp->pmRBuffer)
	{
#if 1
	    IOReturn ret;
	    IOByteCount iLen = sizeof(UInt8);
	    ret = ApplePMUSendMiscCommand(readExtSwitches, 0, NULL, &iLen, &bootEnvIntData[0]);
	    if (kIOReturnSuccess == ret)
		*pmOp->pmRBuffer = (bootEnvIntData[0] & 1);
	    else
#endif

	    {
		OSNumber * num = OSDynamicCast(OSNumber,
						IOService::getPlatform()->getProperty("AppleExtSwitchBootState"));
		if (num)
		    *pmOp->pmRBuffer = (num->unsigned32BitValue() & 1);
		else
		    *pmOp->pmRBuffer = 0;
	    }
	    gIOFBLastClamshellState = *pmOp->pmRBuffer;
	    err = noErr;
	}
    }
    else if ((procInfo == 0x133822)
		&& (trap == 0xa092))
    {
	UInt8 addr, reg, data;

	addr = pb[ 2 ];
	reg = pb[ 3 ];
	pb = *( (UInt8 **) ((UInt32) pb + 8));
	data = pb[ 1 ];
	(*PE_write_IIC)( addr, reg, data );
	err = noErr;
    }
#endif
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

const UInt32 * _eGetKeys( void )
{
    static const UInt32 zeros[] = { 0, 0, 0, 0 };

    return (zeros);
}

UInt32 _eGetIndADB( void * adbInfo, UInt32 /* index */)
{
    bzero( adbInfo, 10);
    return (0);		// orig address
}

char * _eLMGetPowerMgrVars( void )
{
    static char * powerMgrVars = NULL;

    if (powerMgrVars == NULL)
    {
	powerMgrVars = (char *) IOMalloc( 0x3c0);
	if (powerMgrVars)
	    bzero( powerMgrVars, 0x3c0);
    }
    return (powerMgrVars);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSStatus _eNoErr( void )
{
    return (noErr);
}

OSStatus _eFail( void )
{
    return (-40);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// fix this!

#define 	heathrowID		((volatile UInt32 *)0xf3000034)
#define 	heathrowTermEna		(1 << 3)
#define 	heathrowTermDir		(1 << 0)

#define 	heathrowFeatureControl	((volatile UInt32 *)0xf3000038)
#define 	heathrowMBRES		(1 << 24)

#define 	heathrowBrightnessControl ((volatile UInt8 *)0xf3000032)
#define		defaultBrightness	144
#define 	heathrowContrastControl ((volatile UInt8 *)0xf3000033)
#define		defaultContrast		183

#define 	gossamerSystemReg1	((volatile UInt16 *)0xff000004)
#define		gossamerAllInOne	(1 << 4)

void EXP(ATISetMBRES)( UInt32 state )
{
    UInt32	value;

    value = *heathrowFeatureControl;

    if (state == 0)
	value &= ~heathrowMBRES;
    else if (state == 1)
	value |= heathrowMBRES;

    *heathrowFeatureControl = value;
    eieio();
}

void EXP(ATISetMonitorTermination)( Boolean enable )
{
    UInt32	value;

    value = *heathrowID;

    value |= heathrowTermEna;
    if (enable)
	value |= heathrowTermDir;
    else
	value &= ~heathrowTermDir;

    *heathrowID = value;
    eieio();
}

Boolean EXP(ATIIsAllInOne)( void )
{
    Boolean	rtn;
    static bool	didBrightness;

    rtn = (0 == ((*gossamerSystemReg1) & gossamerAllInOne));
    if (rtn && !didBrightness)
    {
	*heathrowBrightnessControl = defaultBrightness;
	eieio();
	*heathrowContrastControl = defaultContrast;
	eieio();
	didBrightness = true;
    }
    return (rtn);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void IONDRVInterruptAction( OSObject * target, void * refCon,
				    IOService * provider, int index )
{
    IONDRVInterruptSet *	set;
    IONDRVInterruptSource *	source;
    SInt32			result;

    set = (IONDRVInterruptSet *) target;
    index++;

    do
    {
	assert( (UInt32) index <= set->count);
	if ((UInt32) index > set->count)
		break;

	source = set->sources + index;
	result = CallTVector( set, (void *) index, source->refCon, 0, 0, 0,
				    source->handler );

	switch (result)
	{
	    case kIONDRVIsrIsNotComplete:
		index++;
	    case kIONDRVIsrIsComplete:
		break;

	    case kIONDRVMemberNumberParent:
		assert( false );
		break;

	    default:
		index = result;
		set = set->child;
		break;
	}
    }
    while (result != kIONDRVIsrIsComplete);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static SInt32 IONDRVStdInterruptHandler( IONDRVInterruptSetMember setMember,
	void *refCon, UInt32 theIntCount )
{
    //    assert( false );

    return (kIONDRVIsrIsComplete);
}

static bool IONDRVStdInterruptDisabler( IONDRVInterruptSetMember setMember,
					void *refCon )
{
    IONDRVInterruptSet *	set;
    IONDRVInterruptSource *	source;
    bool			was;

    set = (IONDRVInterruptSet *) setMember.setID;
    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    assert( setMember.member <= set->count );
    source = set->sources + setMember.member;

    was = source->enabled;
    source->enabled = false;

    assert( set->provider );
    set->provider->disableInterrupt( setMember.member - 1 );

    return (was);
}

static void IONDRVStdInterruptEnabler( IONDRVInterruptSetMember setMember,
					void *refCon )
{
    IONDRVInterruptSet *	set;
    IONDRVInterruptSource *	source;

    set = (IONDRVInterruptSet *) setMember.setID;
    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    assert( setMember.member <= set->count );
    source = set->sources + setMember.member;

    source->enabled = true;

    assert( set->provider );

    if (!source->registered)
    {
	source->registered = true;
	set->provider->registerInterrupt( setMember.member - 1, set,
						    &IONDRVInterruptAction, (void *) 0x53 );
    }

    set->provider->enableInterrupt( setMember.member - 1 );
}

static IOTVector tvIONDRVStdInterruptHandler  = { (void *) IONDRVStdInterruptHandler,  0 };
static IOTVector tvIONDRVStdInterruptEnabler  = { (void *) IONDRVStdInterruptEnabler,  0 };
static IOTVector tvIONDRVStdInterruptDisabler = { (void *) IONDRVStdInterruptDisabler, 0 };


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSStatus
_eGetInterruptFunctions(
  InterruptSetID          set,
  InterruptMemberNumber   member,
  void **                 refCon,
  IOTVector **		  handler,
  IOTVector **            enabler,
  IOTVector **            disabler )
{
    IONDRVInterruptSource *	source;
    OSStatus			err = noErr;

    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    assert( member <= set->count );
    source = set->sources + member;

    if (refCon)
	*refCon   = source->refCon;
    if (handler)
	*handler  = source->handler;
    if (enabler)
	*enabler  = source->enabler;
    if (disabler)
	*disabler = source->disabler;

    return (err);
}

OSStatus
GetInterruptFunctions(
  InterruptSetID          set,
  InterruptMemberNumber   member,
  void **                 refCon,
  InterruptHandler *      handlerFunction,
  InterruptEnabler *      enableFunction,
  InterruptDisabler *     disableFunction)
{
    IONDRVInterruptSource *	source;
    OSStatus			err = noErr;

    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    assert( member <= set->count );
    source = set->sources + member;

    if (refCon)
	*refCon   = source->refCon;
    if (handlerFunction)
	*handlerFunction = (InterruptHandler)  source->handler->pc;
    if (enableFunction)
	*enableFunction  = (InterruptEnabler)  source->enabler->pc;
    if (disableFunction)
	*disableFunction = (InterruptDisabler) source->disabler->pc;

    return (err);
}

IOReturn
IONDRVInstallInterruptFunctions(void *		setID,
				UInt32 		member,
				void *		refCon,
				IOTVector *	handler,
				IOTVector *	enabler,
				IOTVector *	disabler )
{
    IONDRVInterruptSet *	set;
    IONDRVInterruptSource *	source;
    OSStatus			err = noErr;

    set = (IONDRVInterruptSet *) setID;
    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    if (member > set->count)
	return (paramErr);
    source = set->sources + member;

    source->refCon = refCon;
    if (handler)
	source->handler  = handler;
    if (enabler)
	source->enabler  = enabler;
    if (disabler)
	source->disabler = disabler;

    return (err);
}

OSStatus
_eInstallInterruptFunctions(
  InterruptSetID          setID,
  InterruptMemberNumber   member,
  void *                  refCon,
  IOTVector *             handler,
  IOTVector *             enabler,
  IOTVector *             disabler )
{
    return (IONDRVInstallInterruptFunctions(setID, member, refCon,
					    handler, enabler, disabler));
}

OSStatus
InstallInterruptFunctions(
  InterruptSetID          set,
  InterruptMemberNumber   member,
  void *                  refCon,
  InterruptHandler        handlerFunction,
  InterruptEnabler        enableFunction,
  InterruptDisabler       disableFunction)
{
    IONDRVInterruptSource *	source;
    OSStatus			err = noErr;

    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    if ((UInt32)member > set->count)
	return (paramErr);
    source = set->sources + member;

    source->refCon = refCon;
    if (handlerFunction)
    {
	source->handlerStore.pc = (void *) handlerFunction;
	source->handler = &source->handlerStore;
    }
    if (enableFunction)
    {
	source->enablerStore.pc = (void *) enableFunction;
	source->enabler = &source->enablerStore;
    }
    if (disableFunction)
    {
	source->disablerStore.pc = (void *) disableFunction;
	source->disabler = &source->disablerStore;
    }

    return (err);
}

OSStatus
EXP(CreateInterruptSet)(
  InterruptSetID          set,
  InterruptMemberNumber   parentMember,
  InterruptMemberNumber   setSize,
  InterruptSetID *        setID,
  InterruptSetOptions     options)
{
    IONDRVInterruptSet *	newSet;
    IONDRVInterruptSource *	source;
    OSStatus			err = noErr;

    assert( OSDynamicCast( IONDRVInterruptSet, set ));
    assert( parentMember <= set->count );
    source = set->sources + parentMember;

    newSet = IONDRVInterruptSet::with( 0, options, setSize );
    assert( newSet );

    if (newSet)
    {
	for (UInt32 i = 1; i <= (UInt32) setSize; i++)
	{
	    source = newSet->sources + i;
	    source->handler 	= &tvIONDRVStdInterruptHandler;
	    source->enabler 	= &tvIONDRVStdInterruptEnabler;
	    source->disabler 	= &tvIONDRVStdInterruptDisabler;
	}
    }
    set->child = newSet;
    *setID = (InterruptSetID) newSet;

    return (err);
}

OSStatus
EXP(DeleteInterruptSet)( InterruptSetID set )
{
    OSStatus			err = noErr;

    assert( OSDynamicCast( IONDRVInterruptSet, set ));

    set->release();

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define MAKEFUNC(s,e) { s, (void *) e, 0 }

static FunctionEntry PCILibFuncs[] =
    {
	MAKEFUNC( "ExpMgrConfigReadLong", EXP(ExpMgrConfigReadLong)),
	MAKEFUNC( "ExpMgrConfigReadWord", EXP(ExpMgrConfigReadWord)),
	MAKEFUNC( "ExpMgrConfigReadByte", EXP(ExpMgrConfigReadByte)),
	MAKEFUNC( "ExpMgrConfigWriteLong", EXP(ExpMgrConfigWriteLong)),
	MAKEFUNC( "ExpMgrConfigWriteWord", EXP(ExpMgrConfigWriteWord)),
	MAKEFUNC( "ExpMgrConfigWriteByte", EXP(ExpMgrConfigWriteByte)),

	MAKEFUNC( "ExpMgrIOReadLong", EXP(ExpMgrIOReadLong)),
	MAKEFUNC( "ExpMgrIOReadWord", EXP(ExpMgrIOReadWord)),
	MAKEFUNC( "ExpMgrIOReadByte", EXP(ExpMgrIOReadByte)),
	MAKEFUNC( "ExpMgrIOWriteLong", EXP(ExpMgrIOWriteLong)),
	MAKEFUNC( "ExpMgrIOWriteWord", EXP(ExpMgrIOWriteWord)),
	MAKEFUNC( "ExpMgrIOWriteByte", EXP(ExpMgrIOWriteByte)),

	MAKEFUNC( "EndianSwap16Bit", EXP(EndianSwap16Bit)),
	MAKEFUNC( "EndianSwap32Bit", EXP(EndianSwap32Bit))
    };

static FunctionEntry VideoServicesLibFuncs[] =
    {
	MAKEFUNC( "VSLPrepareCursorForHardwareCursor",
		    EXP(VSLPrepareCursorForHardwareCursor)),
	MAKEFUNC( "VSLNewInterruptService", EXP(VSLNewInterruptService)),
	MAKEFUNC( "VSLDisposeInterruptService", EXP(VSLDisposeInterruptService)),
	MAKEFUNC( "VSLDoInterruptService", EXP(VSLDoInterruptService)),
	MAKEFUNC( "VSLSetDisplayConfiguration", EXP(VSLSetDisplayConfiguration)),
	MAKEFUNC( "VSLGestalt", EXP(VSLGestalt))
    };

static FunctionEntry NameRegistryLibFuncs[] =
    {
	MAKEFUNC( "RegistryEntryIDCopy", EXP(RegistryEntryIDCopy)),
	MAKEFUNC( "RegistryEntryIDInit", EXP(RegistryEntryIDInit)),
	MAKEFUNC( "RegistryEntryIDDispose", EXP(RegistryEntryIDDispose)),
	MAKEFUNC( "RegistryEntryIDCompare", EXP(RegistryEntryIDCompare)),
	MAKEFUNC( "RegistryPropertyGetSize", EXP(RegistryPropertyGetSize)),
	MAKEFUNC( "RegistryPropertyGet", EXP(RegistryPropertyGet)),
	MAKEFUNC( "RegistryPropertyGetMod", EXP(RegistryPropertyGetMod)),
	MAKEFUNC( "RegistryPropertySetMod", EXP(RegistryPropertySetMod)),

	MAKEFUNC( "RegistryPropertyIterateCreate", EXP(RegistryPropertyIterateCreate)),
	MAKEFUNC( "RegistryPropertyIterateDispose", EXP(RegistryPropertyIterateDispose)),
	MAKEFUNC( "RegistryPropertyIterate", EXP(RegistryPropertyIterate)),

	MAKEFUNC( "RegistryEntryIterateCreate", EXP(RegistryEntryIterateCreate)),
	MAKEFUNC( "RegistryEntryIterateDispose", EXP(RegistryEntryIterateDispose)),
	MAKEFUNC( "RegistryEntryIterate", EXP(RegistryEntryIterate)),
	MAKEFUNC( "RegistryEntryIterateSet", EXP(RegistryEntryIterateSet)),
	MAKEFUNC( "RegistryCStrEntryToName", EXP(RegistryCStrEntryToName)),
	MAKEFUNC( "RegistryCStrEntryLookup", EXP(RegistryCStrEntryLookup)),

	MAKEFUNC( "RegistryCStrEntryCreate", EXP(RegistryCStrEntryCreate)),
	MAKEFUNC( "RegistryEntryDelete", EXP(RegistryEntryDelete)),

	MAKEFUNC( "RegistryPropertyCreate", EXP(RegistryPropertyCreate)),
	MAKEFUNC( "RegistryPropertyDelete", EXP(RegistryPropertyDelete)),
	MAKEFUNC( "RegistryPropertySet", EXP(RegistryPropertySet))
    };


static FunctionEntry DriverServicesLibFuncs[] =
    {
	MAKEFUNC( "SynchronizeIO", EXP(SynchronizeIO)),
	MAKEFUNC( "SetProcessorCacheMode", EXP(SetProcessorCacheMode)),
	MAKEFUNC( "BlockCopy", EXP(BlockCopy)),
	MAKEFUNC( "BlockMove", EXP(BlockMove)),
	MAKEFUNC( "BlockMoveData", EXP(BlockMoveData)),

	MAKEFUNC( "BlockMoveDataUncached", EXP(BlockMoveDataUncached)),
	MAKEFUNC( "BlockMoveUncached", EXP(BlockMoveUncached)),

	MAKEFUNC( "BlockZero", EXP(BlockZero)),
	MAKEFUNC( "BlockZeroUncached", EXP(BlockZeroUncached)),

	MAKEFUNC( "CStrCopy", EXP(CStrCopy)),
	MAKEFUNC( "CStrCmp", EXP(CStrCmp)),
	MAKEFUNC( "CStrLen", EXP(CStrLen)),
	MAKEFUNC( "CStrCat", EXP(CStrCat)),
	MAKEFUNC( "CStrNCopy", EXP(CStrNCopy)),
	MAKEFUNC( "CStrNCmp", EXP(CStrNCmp)),
	MAKEFUNC( "CStrNCat", EXP(CStrNCat)),
	MAKEFUNC( "PStrCopy", EXP(PStrCopy)),
	MAKEFUNC( "PStrToCStr", EXP(PStrToCStr)),
	MAKEFUNC( "CStrToPStr", EXP(CStrToPStr)),

	MAKEFUNC( "PoolAllocateResident", EXP(PoolAllocateResident)),
	MAKEFUNC( "MemAllocatePhysicallyContiguous", EXP(PoolAllocateResident)),
	MAKEFUNC( "PoolDeallocate", EXP(PoolDeallocate)),

	MAKEFUNC( "UpTime", EXP(UpTime)),
	MAKEFUNC( "AbsoluteDeltaToDuration", EXP(AbsoluteDeltaToDuration)),
	MAKEFUNC( "AbsoluteToDuration", EXP(AbsoluteToDuration)),
	MAKEFUNC( "AddAbsoluteToAbsolute", EXP(AddAbsoluteToAbsolute)),
	MAKEFUNC( "SubAbsoluteFromAbsolute", EXP(SubAbsoluteFromAbsolute)),
	MAKEFUNC( "AddDurationToAbsolute", EXP(AddDurationToAbsolute)),
	MAKEFUNC( "NanosecondsToAbsolute", EXP(NanosecondsToAbsolute)),
	MAKEFUNC( "AbsoluteToNanoseconds", EXP(AbsoluteToNanoseconds)),
	MAKEFUNC( "DurationToAbsolute", EXP(DurationToAbsolute)),
	MAKEFUNC( "DelayForHardware", EXP(DelayForHardware)),
	MAKEFUNC( "DelayFor", EXP(DelayFor)),

	MAKEFUNC( "CurrentExecutionLevel", EXP(CurrentExecutionLevel)),
	MAKEFUNC( "IOCommandIsComplete", EXP(IOCommandIsComplete)),

	MAKEFUNC( "SysDebugStr", EXP(SysDebugStr)),
	MAKEFUNC( "SysDebug", EXP(SysDebug)),

	MAKEFUNC( "CompareAndSwap", EXP(CompareAndSwap)),

	MAKEFUNC( "CreateInterruptSet", EXP(CreateInterruptSet)),
	MAKEFUNC( "DeleteInterruptSet", EXP(DeleteInterruptSet)),
	MAKEFUNC( "GetInterruptFunctions", _eGetInterruptFunctions),
	MAKEFUNC( "InstallInterruptFunctions", _eInstallInterruptFunctions)
    };

static FunctionEntry ATIUtilsFuncs[] =
    {
	// Gossamer onboard ATI
	MAKEFUNC( "ATISetMBRES", EXP(ATISetMBRES)),
	MAKEFUNC( "ATISetMonitorTermination", EXP(ATISetMonitorTermination)),
	MAKEFUNC( "ATIIsAllInOne", EXP(ATIIsAllInOne))
    };

// These are all out of spec

static FunctionEntry InterfaceLibFuncs[] =
    {
	// Apple control : XPRam and EgretDispatch
	MAKEFUNC( "CallUniversalProc", _eFail),
	MAKEFUNC( "CallOSTrapUniversalProc", _eCallOSTrapUniversalProc),
	MAKEFUNC( "BlockZero", EXP(BlockZero)),
	MAKEFUNC( "BlockZeroUncached", EXP(BlockZeroUncached)),

	// Apple chips65550
	//    MAKEFUNC( "NewRoutineDescriptor", _eCallOSTrapUniversalProc),
	//    MAKEFUNC( "DisposeRoutineDescriptor", _eNoErr),
	//    MAKEFUNC( "InsTime", _eInsTime),
	//    MAKEFUNC( "PrimeTime", _ePrimeTime),

	// Radius PrecisionColor 16
	MAKEFUNC( "CountADBs", _eNoErr),
	MAKEFUNC( "GetIndADB", _eGetIndADB),
	MAKEFUNC( "GetKeys", _eGetKeys)
    };

static FunctionEntry PrivateInterfaceLibFuncs[] =
    {
	// Apple chips65550
	MAKEFUNC( "LMGetPowerMgrVars", _eLMGetPowerMgrVars )
    };

#define NUMLIBRARIES	7
const ItemCount IONumNDRVLibraries = NUMLIBRARIES;
LibraryEntry IONDRVLibraries[ NUMLIBRARIES ] =
    {
	{ "PCILib", sizeof(PCILibFuncs) / sizeof(FunctionEntry), PCILibFuncs },
	{ "VideoServicesLib", sizeof(VideoServicesLibFuncs) / sizeof(FunctionEntry), VideoServicesLibFuncs },
	{ "NameRegistryLib", sizeof(NameRegistryLibFuncs) / sizeof(FunctionEntry), NameRegistryLibFuncs },
	{ "DriverServicesLib", sizeof(DriverServicesLibFuncs) / sizeof(FunctionEntry), DriverServicesLibFuncs },

	// G3
	{ "ATIUtils", sizeof(ATIUtilsFuncs) / sizeof(FunctionEntry), ATIUtilsFuncs },

	// out of spec stuff
	{ "InterfaceLib", sizeof(InterfaceLibFuncs) / sizeof(FunctionEntry), InterfaceLibFuncs },
	{ "PrivateInterfaceLib", sizeof(PrivateInterfaceLibFuncs) / sizeof(FunctionEntry), PrivateInterfaceLibFuncs }
    };
} /* extern "C" */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super OSObject

OSDefineMetaClassAndStructors(IONDRVInterruptSet, OSObject)

IONDRVInterruptSet * IONDRVInterruptSet::with(IOService * provider,
        IOOptionBits options, SInt32 count )
{
    IONDRVInterruptSet * set;

    set = new IONDRVInterruptSet;
    if (set && !set->init())
    {
        set->release();
        set = 0;
    }

    if (set)
    {
        set->provider	= provider;
        set->options	= options;
        set->count	= count;

        count++;
        set->sources = IONew( IONDRVInterruptSource, count );
        assert( set->sources );
        bzero( set->sources, count * sizeof( IONDRVInterruptSource));
    }

    return (set);
}

void IONDRVInterruptSet::free()
{
    IONDRVInterruptSource * source;

    for (UInt32 i = 0; i <= count; i++)
    {
	source = sources + i; 
	if (source->registered)
	{
	    source->registered = false;
	    provider->unregisterInterrupt(i - 1);
	}
	if (source->enabled)
	{
	    source->registered = false;
	    provider->disableInterrupt(i - 1);
	}
    }

    if (sources)
        IODelete( sources, IONDRVInterruptSource, count + 1 );

    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if NDRVLIBTEST

static void IONDRVLibrariesTest( IOService * provider )
{
    UInt64 nano;
    UnsignedWide nano2;
    AbsoluteTime abs1, abs2;

    nano = 1000ULL;
    abs1 = EXP(NanosecondsToAbsolute(UInt64ToUnsignedWide(&nano));
    IOLog("_eNanosecondsToAbsolute %08lx:%08lx\n", abs1.hi, abs1.lo);
    nano2 = EXP(AbsoluteToNanoseconds(abs1);
    IOLog("_eAbsoluteToNanoseconds %08lx:%08lx\n", nano2.hi, nano2.lo);
    AbsoluteTime_to_scalar(&abs2) = 0;
    IOLog("EXP(AbsoluteDeltaToDuration %ld\n", EXP(AbsoluteDeltaToDuration(abs1,abs2));

    nano = 0x13161b000ULL;
    abs1 = EXP(NanosecondsToAbsolute(UInt64ToUnsignedWide(&nano));
    IOLog("_eNanosecondsToAbsolute %08lx:%08lx\n", abs1.hi, abs1.lo);
    nano2 = EXP(AbsoluteToNanoseconds(abs1);
    IOLog("_eAbsoluteToNanoseconds %08lx:%08lx\n", nano2.hi, nano2.lo);
    AbsoluteTime_to_scalar(&abs2) = 0;
    IOLog("_eAbsoluteDeltaToDuration %ld\n", EXP(AbsoluteDeltaToDuration(abs1,abs2));

    nano = 0x6acfc00000000ULL;
    abs1 = EXP(NanosecondsToAbsolute(UInt64ToUnsignedWide(&nano));
    IOLog("_eNanosecondsToAbsolute %08lx:%08lx\n", abs1.hi, abs1.lo);
    nano2 = EXP(AbsoluteToNanoseconds(abs1);
    IOLog("_eAbsoluteToNanoseconds %08lx:%08lx\n", nano2.hi, nano2.lo);
    AbsoluteTime_to_scalar(&abs2) = 0;
    IOLog("_eAbsoluteDeltaToDuration %ld\n", EXP(AbsoluteDeltaToDuration(abs1,abs2));

    abs1 = EXP(UpTime();
    IODelay(10);
    abs2 = EXP(UpTime();
    IOLog("10us duration %ld\n", EXP(AbsoluteDeltaToDuration(abs2,abs1));

    abs1 = EXP(UpTime();
    for (int i =0; i < 50000; i++)
        EXP(DelayFor(1);
    abs2 = EXP(UpTime();
    IOLog("50000 DelayFor(1) %ld\n", EXP(AbsoluteDeltaToDuration(abs2,abs1));

    abs1 = EXP(UpTime();
    EXP(DelayFor(50);
    abs2 = EXP(UpTime();
    IOLog("DelayFor(50) %ld\n", EXP(AbsoluteDeltaToDuration(abs2,abs1));

    abs1 = EXP(DurationToAbsolute( -10);
    IOLog("_eDurationToAbsolute(-10) %08lx:%08lx\n", abs1.hi, abs1.lo);
    abs1 = EXP(DurationToAbsolute( 10);
    IOLog("_eDurationToAbsolute(10) %08lx:%08lx\n", abs1.hi, abs1.lo);
}
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" IOReturn _IONDRVLibrariesFinalize( IOService * provider )
{
    provider->removeProperty(kIONDRVISTPropertyName);
    provider->removeProperty("AAPL,ndrv-interrupt-set");
    provider->removeProperty("AAPL,address");
    provider->removeProperty("AAPL,maps");

    return (kIOReturnSuccess);
}

extern "C" IOReturn _IONDRVLibrariesInitialize( IOService * provider )
{
    IODTPlatformExpert *	platform;
    const OSSymbol *		sym;
    OSData *			data;
    OSArray *			intSpec;
    unsigned int		len, i;

#if NDRVLIBTEST
    IONDRVLibrariesTest( provider );
#endif

    // copy nvram property

    if ((platform = OSDynamicCast(IODTPlatformExpert,
                                  IOService::getPlatform())))
    {
        //	IOService::waitForService( IOService::resourceMatching( "IONVRAM" ));

        if (kIOReturnSuccess == platform->readNVRAMProperty(provider,
                &sym, &data))
        {
            IONDRVSetNVRAMPropertyName( provider, sym );
            provider->setProperty( sym, data);
            data->release();
            sym->release();
        }
    }

    // create interrupt properties, if none present

    if ((intSpec = (OSArray *)provider->getProperty(gIOInterruptSpecifiersKey))
            && (0 == provider->getProperty(gIODTAAPLInterruptsKey)))
    {
        // make AAPL,interrupts property if not present (NW)
        for (i = 0, len = 0; i < intSpec->getCount(); i++)
        {
            data = (OSData *) intSpec->getObject(i);
            assert( data );
            len += data->getLength();
        }
        if (len)
            data = OSData::withCapacity( len );
        if (data)
        {
            for (i = 0; i < intSpec->getCount(); i++)
                data->appendBytes( (OSData *) intSpec->getObject(i));
            provider->setProperty( gIODTAAPLInterruptsKey, data );
            data->release();
        }
    }

    // make NDRV interrupts

    IONDRVInterruptSetMember 	setMember;
    IONDRVInterruptSet *	set;
    IONDRVInterruptSource *	source;

    set = IONDRVInterruptSet::with( provider, 0,
                                        kIONDRVISTPropertyMemberCount );
    if (set)
    {
	data = OSData::withCapacity( kIONDRVISTPropertyMemberCount
				    * sizeof(IONDRVInterruptSetMember));
	if (data)
	{
	    for (i = 1; i <= kIONDRVISTPropertyMemberCount; i++)
	    {
		source = set->sources + i;
		source->handler 	= &tvIONDRVStdInterruptHandler;
		source->enabler 	= &tvIONDRVStdInterruptEnabler;
		source->disabler 	= &tvIONDRVStdInterruptDisabler;
    
		setMember.setID 	= (void *) set;
		setMember.member	= i;
		data->appendBytes( &setMember, sizeof( setMember));
	    }
	    provider->setProperty( kIONDRVISTPropertyName, data );
	    data->release();
	}
        provider->setProperty( "AAPL,ndrv-interrupt-set", set );
        set->release();
    }

    // map memory

    IOItemCount numMaps = provider->getDeviceMemoryCount();
    OSArray *   maps = 0;
    data = 0;

    for (i = 0; i < numMaps; i++)
    {
	IODeviceMemory * mem;
	IOMemoryMap *    map;
	IOVirtualAddress virtAddress;

	mem = provider->getDeviceMemoryWithIndex(i);
	if (!mem)
	    continue;
        if (!maps)
	    maps = OSArray::withCapacity(numMaps);
        if (!maps)
            continue;

	map = mem->map();
	if (!map)
	{
//	    IOLog("%s: map[%ld] failed\n", provider->getName(), i);
	    maps->setObject(maps);		// placeholder
	    continue;
	}

	maps->setObject(map);
	map->release();

	virtAddress = map->getVirtualAddress();
	mem->setMapping(kernel_task, virtAddress, kIOMapInhibitCache);
        if (!data)
            data = OSData::withCapacity( numMaps * sizeof( IOVirtualAddress));
        if (!data)
            continue;
	data->appendBytes( &virtAddress, sizeof( IOVirtualAddress));
	kprintf("ndrv base = %lx\n", virtAddress);
    }

    // NDRV aperture vectors
    if (maps)
    {
	provider->setProperty( "AAPL,maps", maps );
        maps->release();
    }
    if (data)
    {
        provider->setProperty( "AAPL,address", data );
        data->release();
    }

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if !__ppc__

OSStatus    CallTVector( 
	    void * p1, void * p2, void * p3, void * p4, void * p5, void * p6,
	    struct IOTVector * entry )
{
    typedef OSStatus (*Proc)(void * p1, void * p2, void * p3, void * p4, void * p5, void * p6);

    Proc proc = (Proc) entry->pc;

    return( (*proc)(p1, p2, p3, p4, p5, p6) );
}

#endif	/* !__ppc__ */


