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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 21 May 99 wgulland created.
 *
 */

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#ifndef DEBUGLOG
#define DEBUGLOG kprintf
#endif

#include <IOKit/assert.h>

#include <IOKit/IOMessage.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOConfigDirectory.h>
#include "IORemoteConfigDirectory.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireDevice, IOFireWireNub)
OSMetaClassDefineReservedUnused(IOFireWireDevice, 0);
OSMetaClassDefineReservedUnused(IOFireWireDevice, 1);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct RomScan {
    IOFireWireDevice *fDevice;
    UInt32 fROMGeneration;
    UInt32 fROMHdr[6];
};

bool IOFireWireDevice::init(OSDictionary *propTable, const IOFWNodeScan *info)
{
    if(!IOFireWireNub::init(propTable))
       return false;
    if(info->fROMSize > 8) {
        UInt32 maxPackLog =
        ((info->fBuf[2] & kFWBIBMaxRec) >> kFWBIBMaxRecPhase) + 1;
        if(maxPackLog == 1) {
            IOLog("Illegal maxrec, using 1 quad\n");
            maxPackLog = 2;
        }
        // if 1394A bus info block, respect maxROM
        if(info->fBuf[2] & kFWBIBGeneration) {
            if(((info->fBuf[2] & kFWBIBMaxROM) >> kFWBIBMaxROMPhase) == 2)
                fMaxReadROMPackLog = 10;	// 1024 bytes max.
            else
                fMaxReadROMPackLog = 2; // Just quads for ROM reads
        }
        else
            fMaxReadROMPackLog = maxPackLog;
        fMaxReadPackLog = maxPackLog;
        fMaxWritePackLog = maxPackLog;
    }
    else {
        // Play safe, limit to quad requests
        fMaxReadROMPackLog = 2;
        fMaxReadPackLog = 2;
        fMaxWritePackLog = 2;
    }
    fROMLock = IORecursiveLockAlloc();
    return fROMLock != NULL;
}


void IOFireWireDevice::readROMDirGlue(void *refcon, IOReturn status,
                        IOFireWireNub *nub, IOFWCommand *fwCmd)
{
    fwCmd->release();
    if(status == kIOReturnSuccess) {
        IOCreateThread(readROMThreadFunc, refcon);
    }
    else {
        IOLog("read root failed, 0x%x\n", status);
        IOFree(refcon, sizeof(RomScan));
    }
}

void IOFireWireDevice::readROMThreadFunc(void *refcon)
{
    RomScan *romScan = (RomScan *)refcon;
    IOFireWireDevice *device = romScan->fDevice;
    OSData *rom;
    // Make sure there's only one thread scanning for unit directories
    IORecursiveLockLock(device->fROMLock);
    if(romScan->fROMGeneration == device->fROMGeneration) {
        OSData *oldROM = device->fDeviceROM;
        rom = OSData::withBytes(romScan->fROMHdr, sizeof(romScan->fROMHdr));
        if(oldROM) {
            unsigned int oldLength = oldROM->getLength();
            if(oldLength > sizeof(romScan->fROMHdr)) {
                // Read the ROM up to the length we had.
                // Swap the ROM over once we've recached it.
                IOLog("Rereading ROM up to %x quads\n", oldLength/sizeof(UInt32));
                oldLength -= sizeof(romScan->fROMHdr);
                UInt32 *buff = (UInt32 *)IOMalloc(oldLength);
                IOFWReadQuadCommand *cmd;
                IOReturn err;
                
                cmd = device->createReadQuadCommand( FWAddress(kCSRRegisterSpaceBaseAddressHi,
                                    kFWBIBHeaderAddress+sizeof(romScan->fROMHdr)),
                                    buff, oldLength/sizeof(UInt32), NULL, NULL, false);
                err = cmd->submit();
                cmd->release();
                if(err == kIOReturnSuccess) {
                    rom->appendBytes(buff, oldLength);
                }
            }
        }
        device->setProperty(gFireWireROM, rom);
        device->fDeviceROM = rom;
        device->processROM(romScan);
        if(oldROM)
            oldROM->release();
    }
    IORecursiveLockUnlock(device->fROMLock);
    IOFree(romScan, sizeof(RomScan));
}


void IOFireWireDevice::free()
{
    if(fDeviceROM)
        fDeviceROM->release();
    if(fROMLock)
        IORecursiveLockFree(fROMLock);
    IOFireWireNub::free();
}

bool IOFireWireDevice::attach(IOService *provider)
{
    char location[17];
    assert(OSDynamicCast(IOFireWireController, provider));
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = (IOFireWireController *)provider;

    sprintf(location, "%lx%08lx", (UInt32)(fUniqueID >> 32), (UInt32)(fUniqueID & 0xffffffff));
    setLocation(location);
    // Stick device in DeviceTree plane for OpenFirmware
    IOService *parent = provider;
    while(parent) {
        if(parent->inPlane(gIODTPlane))
            break;
        parent = parent->getProvider();
    }
    if(parent) {
        attachToParent(parent, gIODTPlane);
        setName("node", gIODTPlane);
    }

    return(true);
}

bool IOFireWireDevice::finalize( IOOptionBits options )
{
    /*
     * fDirectory has a retain() on this, which it won't release until it is
     * free()ed. So get rid of our retain() on it so eventually both can go.
     */
    if(fDirectory) {
        fDirectory->release();
        fDirectory = NULL;
    }
    // Nuke from device tree
    detachAll(gIODTPlane);
    
    return IOFireWireNub::finalize(options);
}

void IOFireWireDevice::setNodeROM(UInt32 gen, UInt16 localID, const IOFWNodeScan *info)
{
    OSObject *prop;
    
    OSData *rom;
    UInt32 newROMSize;

    fLocalNodeID = localID;
    fGeneration = gen;
    if(info) {
        fNodeID = info->fAddr.nodeID;
    }
    else {
        fNodeID = kFWBadNodeID;
    }
	
	prop = OSNumber::withNumber(fNodeID, 16);
    setProperty(gFireWireNodeID, prop);
    prop->release();

    if(!info) {
        // Notify clients that current state is suspended
        messageClients(kIOMessageServiceIsSuspended);
        return;
    }
    // Store selfIDs
    prop = OSData::withBytes(info->fSelfIDs, info->fNumSelfIDs*sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();

    // Process new ROM

    newROMSize = info->fROMSize;

    if(fDeviceROM && newROMSize <= fDeviceROM->getLength()) {
        if(!bcmp(info->fBuf, fDeviceROM->getBytesNoCopy(), newROMSize)) {
            IOLog("IOFireWireDevice, ROM unchanged 0x%p\n", this);
            // Also check if generation = 0, in which case we can't assume the ROM is the same
            if(info->fBuf[2] & kFWBIBGeneration) {
                messageClients( kIOMessageServiceIsResumed );	// Safe to continue

                return;	// ROM unchanged
            }
            IOLog("IOFireWireDevice 0x%p, ROM generation zero\n", this);
        }
    }
    
    fROMGeneration++;
    
    messageClients( kIOMessageServiceIsResumed );	// Safe to continue
    
    if(newROMSize > 12) {
        UInt32 vendorID = info->fBuf[3] >> 8;
        prop = OSNumber::withNumber(vendorID, 32);
        setProperty(gFireWireVendor_ID, prop);
        prop->release();
    }
    if(newROMSize == 20) {
        // Just Bus Info Block so far
        // Perhaps there is a root directory, but it wasn't covered by the initial CRC.
        IOFWReadQuadCommand *cmd;
        RomScan *romScan = (RomScan *)IOMalloc(sizeof(RomScan));
        if(romScan) {
            romScan->fROMGeneration = fROMGeneration;
            romScan->fDevice = this;
            bcopy(info->fBuf, romScan->fROMHdr, newROMSize);
            cmd = createReadQuadCommand(FWAddress(kCSRRegisterSpaceBaseAddressHi,
                    kFWBIBHeaderAddress+20), &romScan->fROMHdr[5], 1, &readROMDirGlue, romScan, false);
            cmd->submit();
        }
    }
    else {
        rom = OSData::withBytes(info->fBuf, newROMSize);
        setProperty(gFireWireROM, rom);
        if(fDeviceROM) {
            fDeviceROM->release();
        }
        fDeviceROM = rom;
    }
}

void IOFireWireDevice::processROM(RomScan *romScan)
{
    OSObject *prop;
    OSString *vendorName = NULL;
    OSString *modelName = NULL;

    IOConfigDirectory *unit = NULL;
    OSIterator *unitDirs = NULL;
    OSString *t = NULL;
    UInt32 vendorID;
    UInt32 modelID;
    IOReturn err;

    fDeviceROM->retain();

    do {
        if(fDirectory) {
            fDirectory->release();
        }
        fDirectory = IORemoteConfigDirectory::withOwnerOffset(this, fDeviceROM,
                                                    5, kConfigRootDirectoryKey);
        if(!fDirectory) {
            IOLog("whoops, no root directory!!\n");
            break;
        }
        err = fDirectory->getKeyValue(kConfigModuleVendorIdKey, vendorID, &vendorName);
        if(vendorName) {
            setProperty(gFireWireVendor_Name, vendorName);
        }
        err = fDirectory->getKeyValue(kConfigModelIdKey, modelID, &modelName);
        if(modelName) {
            setProperty(gFireWireProduct_Name, modelName);
        }
        err = fDirectory->getKeyValue(kConfigModuleVendorIdKey, unit, &t);
        if(kIOReturnSuccess == err) {
            if(t) {
                if(vendorName)
                    vendorName->release();
                vendorName = t;
                t = NULL;
                setProperty(gFireWireVendor_Name, vendorName);
            }
            err = unit->getKeyValue(kConfigModelIdKey, modelID, &t);
            if(kIOReturnSuccess == err && t) {
                if(modelName)
                    modelName->release();
                modelName = t;
                t = NULL;
                setProperty(gFireWireProduct_Name, modelName);
            }
            unit->release();
        }

        err = fDirectory->getKeySubdirectories(kConfigUnitDirectoryKey, unitDirs);
        if(kIOReturnSuccess == err) {
            while(unit = OSDynamicCast(IOConfigDirectory,
                                            unitDirs->getNextObject())){
                UInt32 unitSpecID = 0;
                UInt32 unitSoftwareVersion = 0;
                OSDictionary * propTable = 0;
                IOFireWireUnit * newDevice = 0;

                err = unit->getKeyValue(kConfigUnitSpecIdKey, unitSpecID);
                err = unit->getKeyValue(kConfigUnitSwVersionKey, unitSoftwareVersion);
                err = unit->getKeyValue(kConfigModelIdKey, modelID, &t);
                if(t) {
                    if(modelName)
                        modelName->release();
                    modelName = t;
                    t = NULL;
                }
                // Add entry to registry.
                do {
                    propTable = OSDictionary::withCapacity(7);
                    if (!propTable)
                        continue;
                    /*
                    * Set the IOMatchCategory so that things that want to connect to
                    * the device still can even if it already has IOFireWireUnits
                    * attached
                    */
                    prop = OSString::withCString("FireWire Unit");
                    propTable->setObject(gIOMatchCategoryKey, prop);
                    prop->release();

                    if(modelName)
                        propTable->setObject(gFireWireProduct_Name, modelName);
                    if(vendorName)
                        propTable->setObject(gFireWireVendor_Name, vendorName);

                    prop = OSNumber::withNumber(unitSpecID, 32);
                    propTable->setObject(gFireWireUnit_Spec_ID, prop);
                    prop->release();
                    prop = OSNumber::withNumber(unitSoftwareVersion, 32);
                    propTable->setObject(gFireWireUnit_SW_Version, prop);
                    prop->release();

                    // Copy over matching properties from Device
                    prop = getProperty(gFireWireVendor_ID);
                    if(prop)
                        propTable->setObject(gFireWireVendor_ID, prop);
                    prop = getProperty(gFireWire_GUID);
                    if(prop)
                        propTable->setObject(gFireWire_GUID, prop);
                    // Check if unit directory already exists
                    OSIterator *childIterator;
                    IOFireWireUnit * found = NULL;
                    childIterator = getClientIterator();
                    if( childIterator) {
                        OSObject *child;
                        while( (child = childIterator->getNextObject())) {
                            found = OSDynamicCast(IOFireWireUnit, child);
                            if(found && found->matchPropertyTable(propTable)) {
                                break;
                            }
                            else
                                found = NULL;
                        }
                        childIterator->release();
                        if(found)
                            break;
                    }

                    newDevice = new IOFireWireUnit;

                    if (!newDevice || !newDevice->init(propTable, unit))
                        break;
                    // Set max packet sizes
                    newDevice->setMaxPackLog(true, false, fMaxWritePackLog);
                    newDevice->setMaxPackLog(false, false, fMaxReadPackLog);
                    newDevice->setMaxPackLog(false, true, fMaxReadROMPackLog);
                    if (!newDevice->attach(this))	
                        break;
                    newDevice->registerService();
                } while (false);
                if(newDevice)
                    newDevice->release();
                if(propTable)
                    propTable->release();
            }
            unitDirs->release();
        }
        else
            IOLog("IOFireWireDevice:Err 0x%x getting UnitDirectory iterator\n",
                err);
    } while (false);
    if(modelName)
        modelName->release();
    if(vendorName)
        vendorName->release();

    fDeviceROM->release();
    
}

IOReturn IOFireWireDevice::cacheROM(OSData *rom, UInt32 offset, const UInt32 *&romBase)
{
    unsigned int romLength;
    IOReturn err = kIOReturnSuccess;
    
    offset++;	// Point past desired quad, not at it.
        
    IORecursiveLockLock(fROMLock);
    romLength = rom->getLength();
    IORecursiveLockUnlock(fROMLock);
    while(offset*sizeof(UInt32) > romLength && kIOReturnSuccess == err) {
        UInt32 *buff;
        int bufLen;
        IOFWReadQuadCommand *cmd;
        IOLog("IOFireWireDevice %p:Need to extend ROM cache from 0x%lx to 0x%lx quads, gen %d\n",
              this, romLength/sizeof(UInt32), offset, fROMGeneration);

        bufLen = offset*sizeof(UInt32) - romLength;
        buff = (UInt32 *)IOMalloc(bufLen);
        cmd = createReadQuadCommand( FWAddress(kCSRRegisterSpaceBaseAddressHi,
                            kFWBIBHeaderAddress+romLength),
                            buff, bufLen/sizeof(UInt32), NULL, NULL, false);
        err = cmd->submit();
        cmd->release();
        if(err == kIOReturnSuccess) {
            unsigned int newLength;
            IORecursiveLockLock(fROMLock);
            newLength = rom->getLength();
            if(romLength == newLength) {
                rom->appendBytes(buff, bufLen);
                newLength += bufLen;
            }
            romLength = newLength;
            IORecursiveLockUnlock(fROMLock);
        }
        else
            IOLog("%p: err 0x%x reading ROM\n", this, err);
        IOFree(buff, bufLen);
    }
    romBase = (const UInt32 *)rom->getBytesNoCopy();
    return err;
}

const UInt32 * IOFireWireDevice::getROMBase()
{
    return (const UInt32 *)fDeviceROM->getBytesNoCopy();
}

IOReturn IOFireWireDevice::message( UInt32 mess, IOService * provider,
                                    void * argument )
{
    // Propagate bus reset start/end messages
    if( kIOFWMessageServiceIsRequestingClose == mess ) 
    {
        messageClients( mess );
        return kIOReturnSuccess;
    }
    
    return IOService::message(mess, provider, argument );
}

/**
 ** Open / Close methods
 **/
 
 // we override these two methods to allow a reference counted open from
 // IOFireWireUnits only.  Exclusive access is enforced for non-Unit clients.

// handleOpen
//
//
 
bool IOFireWireDevice::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
    bool ok = true;
    
    IOFireWireUnit * unitClient = OSDynamicCast( IOFireWireUnit, forClient );
    if( unitClient != NULL )
    {
        // bail if we're already open from the device
        if( fOpenFromDevice )
            return false;
        
        if( fOpenFromUnitCount == 0 )
        {
            // if this is the first open call, actually do the open
            ok = IOService::handleOpen( forClient, options, arg );
            if( ok )
                fOpenFromUnitCount++;
        }
        else
        {
            // otherwise just increase the reference count
            fOpenFromUnitCount++;
        }
    }
    else
    {
        // bail if we're open from a unit
        if( fOpenFromUnitCount != 0 )
            return false;
            
        // try to open
        if( !fOpenFromDevice ) // extra safe
        {
            ok = IOService::handleOpen( forClient, options, arg );
            if( ok )
            {
                fOpenFromDevice = true;
            }
        }
        else
        {
            ok = false; // already open
        }
    }
    
    return ok;
}

// handleClose
//
//

void IOFireWireDevice::handleClose( IOService * forClient, IOOptionBits options )
{
    IOFireWireUnit * unitClient = OSDynamicCast( IOFireWireUnit, forClient );
    if( unitClient != NULL )
    {
        if( fOpenFromUnitCount != 0 )
        {
            fOpenFromUnitCount--;
            
            if( fOpenFromUnitCount == 0 ) // close if we're down to zero
            {
                IOService::handleClose( forClient, options );
                
                // terminate if we're no longer on the bus
                if( fNodeID == kFWBadNodeID && !isInactive() )
                    terminate();
            }
        }
    }
    else
    {
        if( fOpenFromDevice )
        {
            fOpenFromDevice = false;
            IOService::handleClose( forClient, options );
            
            // terminate if we're no longer on the bus
            if( fNodeID == kFWBadNodeID && !isInactive() )
                terminate();
        }
    }
}

// handleIsOpen
//
//

bool IOFireWireDevice::handleIsOpen( const IOService * forClient ) const
{
    if( forClient == NULL )
    {
        return (fOpenFromUnitCount != 0 || fOpenFromDevice);
    }
    
    // are we open from one or more units?
    if( fOpenFromUnitCount != 0 )
    {
        // is the client really a unit?
        IOFireWireUnit * unitClient = OSDynamicCast( IOFireWireUnit, forClient );
        return (unitClient != NULL);
    }
    
    // are we open from the device?
    if( fOpenFromDevice )
    {
        // is the clien tthe one who opened us?
        return IOService::handleIsOpen( forClient );
    }
    
    // we're not open
    return false;
}

/**
 ** Matching methods
 **/
bool IOFireWireDevice::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOFireWireNub::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    return compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
}
