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

#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOHibernatePrivate.h>

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

extern void *kern_os_malloc(size_t size);
extern void  kern_os_free(void * addr);

#define LOG             if(1) IOLog
#define LOGNAMEREG      0

#define CHECK_INTERRUPT(s)                                      \
if( ml_at_interrupt_context()) {                                \
    /* IOLog("interrupt:%s(%s)\n", __FUNCTION__, s); */         \
    return( nrLockedErr );                                      \
}

extern "C" IOReturn _IONDRVLibrariesMappingInitialize( IOService * provider );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//#define EXP(s)        _e ## s
#define EXP(s)  s


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
    IORegistryEntry *   regEntry1;
    IORegistryEntry *   regEntry2;

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
    OSStatus            err = noErr;
    OSData *            prop;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    if (!strcmp(kPropertyAAPLAddress, propertyName))
        _IONDRVLibrariesMappingInitialize((IOService *) regEntry);

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
    OSStatus            err = noErr;
    OSData *            prop;
    UInt32              len;

    CHECK_INTERRUPT(propertyName)
    REG_ENTRY_TO_PT( entryID, regEntry)

    if (!strcmp(kPropertyAAPLAddress, propertyName))
        _IONDRVLibrariesMappingInitialize((IOService *) regEntry);

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
    OSStatus            err = noErr;
    OSData *            prop;

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
    OSStatus            err = noErr;
    OSObject *          old;

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
    regEntry->setProperty( "IONVRAMProperty", /*(OSObject *)*/ sym );
}

static IOReturn IONDRVSetNVRAMPropertyValue( IORegistryEntry * regEntry,
        const OSSymbol * name, OSData * value )
{
    IOReturn                    err;
    IODTPlatformExpert *        platform;

    if ((platform = OSDynamicCast(IODTPlatformExpert,
                                  IOService::getPlatform())))
        err = platform->writeNVRAMProperty( regEntry, name, value );
    else
        err = kIOReturnUnsupported;

    return (err);
}

OSStatus EXP(RegistryPropertySet)( const RegEntryID * entryID, 
                                    const RegPropertyName * propertyName, 
                                    const void * propertyValue, RegPropertyValueSize propertySize )
{
    OSStatus                    err = noErr;
    OSData *                    prop;
    const OSSymbol *            sym;

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

        if (sym == (const OSSymbol *)
                regEntry->getProperty("IONVRAMProperty"))
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
    const OSSymbol *    sym;

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
    OSStatus            err = noErr;
    OSData *            data;
    const OSSymbol *    sym;

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
    const OSSymbol *    key;

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

OSStatus
EXP(RegistryEntryIterate)( IORegistryIterator **        cookie,
                        UInt32          /* relationship */,
                        RegEntryID *    foundEntry,
                        Boolean *       done)
{
    IORegistryEntry *   regEntry;

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
EXP(RegistryCStrEntryToName)( const RegEntryID *        entryID,
                            RegEntryID *                parentEntry,
                            char *                      nameComponent,
                            Boolean *                   done )
{
    IORegistryEntry *   regEntry;

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
EXP(RegistryCStrEntryLookup)( const RegEntryID *        parentEntry,
                              const RegCStrPathName *   path,
                              RegEntryID *              newEntry)
{
    IOReturn            err;
    IORegistryEntry *   regEntry = 0;
    char *              buf;
    char *              cvtPath;
    char                c;
#define kDTRoot         "Devices:device-tree:"
#define kMacIORoot      "Devices:device-tree:pci:mac-io:"

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


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSStatus EXP(VSLGestalt)( VSLGestaltType selector, UInt32 * response )
{
    IOReturn ret;

    if (!response)
        return (paramErr);

    *response = 0;

    switch (selector)
    {
        case kVSLClamshellStateGestaltType:
            ret = IOGetHardwareClamshellState(response);
            break;
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
  IOHardwareCursorDescriptor *  hardwareDescriptor,
  IOHardwareCursorInfo *        hwCursorInfo)
{
    return(IONDRVFramebuffer::VSLPrepareCursorForHardwareCursor(cursorRef, 
                                hardwareDescriptor, hwCursorInfo));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" IOReturn _IONDRVLibrariesFinalize( IOService * provider )
{
    provider->removeProperty(kPropertyAAPLAddress);
    provider->removeProperty("AAPL,maps");

    return (kIOReturnSuccess);
}

extern "C" IOReturn _IONDRVLibrariesMappingInitialize( IOService * provider )
{
    // map memory
    OSData *     data;
    unsigned int i;
    IOItemCount  numMaps;
    OSArray *    maps = 0;
    data = 0;

    if (provider->getProperty(kPropertyAAPLAddress))
        return (kIOReturnSuccess);

    numMaps = provider->getDeviceMemoryCount();
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
//          IOLog("%s: map[%ld] failed\n", provider->getName(), i);
            maps->setObject(kOSBooleanFalse);           // placeholder
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
    }

    // NDRV aperture vectors
    if (maps)
    {
        provider->setProperty( "AAPL,maps", maps );
        maps->release();
    }
    if (data)
    {
        provider->setProperty(kPropertyAAPLAddress, data );
        data->release();
    }
    return (kIOReturnSuccess);
}

extern "C" IOReturn _IONDRVLibrariesInitialize( IOService * provider )
{
    IODTPlatformExpert *        platform;
    const OSSymbol *            sym;
    OSData *                    data;

#if NDRVLIBTEST
    IONDRVLibrariesTest( provider );
#endif

    // copy nvram property

    if ((platform = OSDynamicCast(IODTPlatformExpert,
                                  IOService::getPlatform())))
    {
        //      IOService::waitForService( IOService::resourceMatching( "IONVRAM" ));

        if (kIOReturnSuccess == platform->readNVRAMProperty(provider,
                &sym, &data))
        {
            IONDRVSetNVRAMPropertyName( provider, sym );
            provider->setProperty( sym, data);
            data->release();
            sym->release();
        }
    }

#if VERSION_MAJOR < 9
    _IONDRVLibrariesMappingInitialize(provider);
#endif

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
