/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include "IOFireWireAVCUnit.h"
#include "IOFireWireAVCCommand.h"
#include "IOFireWireAVCConsts.h"
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include "IOFireWirePCRSpace.h"

const OSSymbol *gIOFireWireAVCUnitType;
const OSSymbol *gIOFireWireAVCSubUnitType;
const OSSymbol *gIOFireWireAVCSubUnitCount[kAVCNumSubUnitTypes];

OSDefineMetaClass(IOFireWireAVCNub, IOService)
OSDefineAbstractStructors(IOFireWireAVCNub, IOService)
//OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 3);

OSDefineMetaClassAndStructors(IOFireWireAVCUnit, IOFireWireAVCNub)
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 3);

UInt32 IOFireWireAVCUnit::AVCResponse(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                    FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon requestRefcon)
{
    IOFireWireAVCUnit *me = (IOFireWireAVCUnit *)refcon;
    UInt32 res = kFWResponseAddressError;
    
    // copy the status bytes from fPseudoSpace if this is for us
    if(addr.addressLo == kFCPResponseAddress && me->fCommand) {
        res = me->fCommand->handleResponse(nodeID, len, buf);
    }
    return res;
}

void IOFireWireAVCUnit::rescanSubUnits(void *arg)
{
    IOFireWireAVCUnit *me = (IOFireWireAVCUnit *)arg;
    
    me->updateSubUnits(false);
}

void IOFireWireAVCUnit::updateSubUnits(bool firstTime)
{
    IOReturn res;
    UInt32 size;
    UInt8 cmd[8],response[8];
    OSObject *prop;
    bool hasFCP = true;
// Get SubUnit info
    cmd[kAVCCommandResponse] = kAVCStatusInquiryCommand;
    cmd[kAVCAddress] = kAVCUnitAddress;
    cmd[kAVCOpcode] = kAVCSubunitInfoOpcode;
    cmd[kAVCOperand0] = 7;
    cmd[4] = cmd[5] = cmd[6] = cmd[7] = 0xff;
    size = 8;
    
    res = AVCCommand(cmd, 8, response, &size);
    
    if(res != kIOReturnSuccess || response[kAVCCommandResponse] != kAVCImplementedStatus) {
        if(firstTime) {
            // Sony convertor box doesn't do AVC, make it look like a camcorder.
            // Panasonic NV-C5 doesn't support SubunitInfo query but does support VCR commands
            if(res != kIOReturnSuccess)
                hasFCP = false;
            
            response[kAVCOperand2] = 0x20;	// One VCR
            response[kAVCOperand3] = 0xff;
            response[kAVCOperand4] = 0xff;
            response[kAVCOperand5] = 0xff;
        }
        else
            return;	// No update necessary
    }

    if(firstTime)
        setProperty("supportsFCP", hasFCP);
    
    // Zero count of subunits before updating with new counts
    bzero(fSubUnitCount, sizeof(fSubUnitCount));
    for(int i=0; i<kAVCNumSubUnitTypes; i++) {
        removeProperty(gIOFireWireAVCSubUnitCount[i]);
    }
    
    for(int i=0; i<4; i++) {
        UInt8 val = response[kAVCOperand1+i];
        if(val != 0xff) {
            UInt8 type, num;
            type = val >> 3;
            num = (val & 0x7)+1;
            fSubUnitCount[type] = num;
            //IOLog("Subunit type %x, num %d\n", type, num);
            setProperty(gIOFireWireAVCSubUnitCount[type]->getCStringNoCopy(), num, 8);
            
            // Create sub unit nub if it doesn't exist
            IOFireWireAVCSubUnit *sub = NULL;
            OSDictionary * propTable = 0;
            do {
                propTable = OSDictionary::withCapacity(6);
                if(!propTable)
                    break;
                prop = OSNumber::withNumber(type, 32);
                propTable->setObject(gIOFireWireAVCSubUnitType, prop);
                prop->release();
                if(!firstTime) {
                    OSIterator *childIterator;
                    IOFireWireAVCSubUnit * found = NULL;
                    childIterator = getClientIterator();
                    if(childIterator) {
                        OSObject *child;
                        while( (child = childIterator->getNextObject())) {
                            found = OSDynamicCast(IOFireWireAVCSubUnit, child);
                            if(found && found->matchPropertyTable(propTable)) {
                                break;
                            }
                            else
                                found = NULL;
                        }
                        childIterator->release();
                        if(found) {
                            break;
                        }
                    }
                }
                sub = new IOFireWireAVCSubUnit;
                if(!sub)
                    break;

                if (!sub->init(propTable, this))
                    break;
                if (!sub->attach(this))	
                    break;
                sub->setProperty("supportsFCP", hasFCP);

                sub->registerService();
                
            } while (0);
            if(sub)
                sub->release();
            if(propTable)
                propTable->release();
        }
    }
    
    // Prune sub units that have gone away.
    if(!firstTime) {
        OSIterator *childIterator;
        IOFireWireAVCSubUnit * sub = NULL;
        childIterator = getClientIterator();
        if(childIterator) {
            OSObject *child;
            while( (child = childIterator->getNextObject())) {
                sub = OSDynamicCast(IOFireWireAVCSubUnit, child);
                if(sub) {
                    OSNumber *type;
                    type = OSDynamicCast(OSNumber, sub->getProperty(gIOFireWireAVCSubUnitType));
                    if(type && !fSubUnitCount[type->unsigned32BitValue()])
                        sub->terminate();
                }
            }
            childIterator->release();
        }
    }
}

bool IOFireWireAVCUnit::start(IOService *provider)
{
    OSObject *prop;
    UInt32 type;

    fDevice = OSDynamicCast(IOFireWireNub, provider);
    if(!fDevice)
        return false;
    if(!gIOFireWireAVCUnitType)
        gIOFireWireAVCUnitType = OSSymbol::withCString("Unit_Type");
    if(!gIOFireWireAVCUnitType)
        return false;
    if(!gIOFireWireAVCSubUnitType)
        gIOFireWireAVCSubUnitType = OSSymbol::withCString("SubUnit_Type");
    if(!gIOFireWireAVCSubUnitType)
        return false;

    for(int i=0; i<kAVCNumSubUnitTypes; i++) {
        char buff[16];
        if(!gIOFireWireAVCSubUnitCount[i]) {
            sprintf(buff, "AVCSubUnit_%x", i);
            gIOFireWireAVCSubUnitCount[i] = OSSymbol::withCString(buff);
            if(!gIOFireWireAVCSubUnitCount[i])
                return false;
        }
    }
    
    if( !IOService::start(provider))
        return (false);

    fFCPResponseSpace = fDevice->getBus()->createInitialAddressSpace(kFCPResponseAddress, 512,
                                                                        NULL, AVCResponse, this);
    if(!fFCPResponseSpace)
        return false;
    fFCPResponseSpace->activate();
    
    avcLock = IOLockAlloc();
    if (avcLock == NULL) {
        IOLog("IOAVCUnit::start avcLock failed\n");
        return false;
    }
    
    cmdLock = IOLockAlloc();
    if (cmdLock == NULL) {
        IOLog("IOAVCUnit::start avcLock failed\n");
        return false;
    }
    
// Get Unit type
    IOReturn res;
    UInt32 size;
    UInt8 cmd[8],response[8];

    cmd[kAVCCommandResponse] = kAVCStatusInquiryCommand;
    cmd[kAVCAddress] = kAVCUnitAddress;
    cmd[kAVCOpcode] = kAVCUnitInfoOpcode;
    cmd[3] = cmd[4] = cmd[5] = cmd[6] = cmd[7] = 0xff;
    size = 8;
    res = AVCCommand(cmd, 8, response, &size);
    if(kIOReturnSuccess != res) {
        IOSleep(2000);	// two seconds, give device time to get it's act together
        size = 8;
        res = AVCCommand(cmd, 8, response, &size);
    }
    if(kIOReturnSuccess != res || response[kAVCCommandResponse] != kAVCImplementedStatus)
        type = kAVCVideoCamera;	// Anything that doesn't implement AVC properly is probably a camcorder!
    else
        type = IOAVCType(response[kAVCOperand1]);

    // Copy over matching properties from FireWire Unit
    prop = provider->getProperty(gFireWireVendor_ID);
    if(prop)
        setProperty(gFireWireVendor_ID, prop);
    prop = provider->getProperty(gFireWire_GUID);
    if(prop)
        setProperty(gFireWire_GUID, prop);
    prop = provider->getProperty(gFireWireProduct_Name);
    if(prop)
        setProperty(gFireWireProduct_Name, prop);
    
    setProperty("Unit_Type", type, 32);
    
	// mark ourselves as started, this allows us to service resumed messages
	// resumed messages after this point should be safe.
	fStarted = true;
	
    updateSubUnits(true);
    
    // Finally enable matching on this object.
    registerService();
    
    return true;
}

void IOFireWireAVCUnit::free(void)
{
    if (fFCPResponseSpace) {
        fFCPResponseSpace->deactivate();
        fFCPResponseSpace->release();
    }
    if (avcLock) {
        IOLockFree(avcLock);
    }
    IOService::free();
}

/**
 ** Matching methods
 **/
bool IOFireWireAVCUnit::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.
    

    bool res = compareProperty(table, gIOFireWireAVCUnitType) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
        
    if(res) {
        // Also see if requested subunits are available.
        int i;
        //OLog("Checking subunit foo\n");
        for(i=0; i<kAVCNumSubUnitTypes; i++) {
            OSNumber *	value;
            value = OSDynamicCast(OSNumber, table->getObject( gIOFireWireAVCSubUnitCount[i] ));
            if(value) {
                // make sure we have at least the requested number of subunits of the requested type
                //IOLog("Want %d AVCSubUnit_%x, got %d\n", value->unsigned8BitValue(), i, fSubUnitCount[i]);
                res = value->unsigned8BitValue() <= fSubUnitCount[i];
                if(!res)
                    break;
            }
        }
        //IOLog("After Checking subunit foo, match is %d\n", res);
    }
    return res;
}

IOReturn IOFireWireAVCUnit::AVCCommand(const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
    IOReturn res;
    IOFireWireAVCCommand *cmd;
    if(len == 0 || len > 512) {
        IOLog("Loopy AVCCmd, len %ld, respLen %ld\n", len, *size);
        return kIOReturnBadArgument;
    }
    cmd = IOFireWireAVCCommand::withNub(fDevice, in, len, out, size);
    if(!cmd)
        return kIOReturnNoMemory;

    // lock avc space
    IOTakeLock(avcLock);
    fCommand = cmd;
    
    res = fCommand->submit();
    if(res != kIOReturnSuccess) {
        //IOLog("AVCCommand returning 0x%x\n", res);
        //IOLog("command %x\n", *(UInt32 *)in);
    }
    IOTakeLock(cmdLock);
    fCommand = NULL;
    IOUnlock(cmdLock);
    cmd->release();
    IOUnlock(avcLock);

    return res;
}

IOReturn IOFireWireAVCUnit::AVCCommandInGeneration(UInt32 generation, const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
    IOReturn res;
    IOFireWireAVCCommand *cmd;
    if(len == 0 || len > 512) {
        IOLog("Loopy AVCCmd, len %ld, respLen %ld\n", len, *size);
        return kIOReturnBadArgument;
    }
    cmd = IOFireWireAVCCommand::withNub(fDevice, generation, in, len, out, size);
    if(!cmd)
        return kIOReturnNoMemory;

    // lock avc space
    IOTakeLock(avcLock);
    fCommand = cmd;
    
    res = fCommand->submit();
    if(res != kIOReturnSuccess) {
        //IOLog("AVCCommand returning 0x%x\n", res);
        //IOLog("command %x\n", *(UInt32 *)in);
    }
    IOTakeLock(cmdLock);
    fCommand = NULL;
    IOUnlock(cmdLock);
    cmd->release();
    IOUnlock(avcLock);

    return res;
}

//
// handleOpen / handleClose
//

bool IOFireWireAVCUnit::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fDevice->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

void IOFireWireAVCUnit::handleClose( IOService * forClient, IOOptionBits options )
{
	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fDevice->close(this, options);
	}
}

IOReturn IOFireWireAVCUnit::message(UInt32 type, IOService *provider, void *argument)
{
    if( fStarted == true && type == kIOMessageServiceIsResumed ) {
        IOCreateThread(rescanSubUnits, this);
    }
    messageClients(type);
    
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUnit::updateAVCCommandTimeout()
{
    IOTakeLock(cmdLock);
    if(fCommand != NULL)
        fCommand->resetInterimTimeout();
    IOUnlock(cmdLock);

    return kIOReturnSuccess;    
}

/* -------------------------------------------- AVC SubUnit -------------------------------------------- */

OSDefineMetaClassAndStructors(IOFireWireAVCSubUnit, IOFireWireAVCNub)
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 3);

bool IOFireWireAVCSubUnit::init(OSDictionary *propTable, IOFireWireAVCUnit *provider)
{
    OSObject *prop;

    if(!IOFireWireAVCNub::init(propTable))
        return false;
    fAVCUnit = provider;
    if(!fAVCUnit)
        return false;
    fDevice = fAVCUnit->getDevice();
    if(!fDevice)
        return false;
    
    // Copy over matching properties from AVC Unit
    prop = provider->getProperty(gFireWireVendor_ID);
    if(prop)
        setProperty(gFireWireVendor_ID, prop);
    prop = provider->getProperty(gFireWire_GUID);
    if(prop)
        setProperty(gFireWire_GUID, prop);
    prop = provider->getProperty(gFireWireProduct_Name);
    if(prop)
        setProperty(gFireWireProduct_Name, prop);

    // Copy over user client properties
    prop = provider->getProperty(gIOUserClientClassKey);
    if(prop)
        setProperty(gIOUserClientClassKey, prop);
    prop = provider->getProperty(kIOCFPlugInTypesKey);
    if(prop)
        setProperty(kIOCFPlugInTypesKey, prop);
    
    return true;
}

/**
 ** Matching methods
 **/
bool IOFireWireAVCSubUnit::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.
    

    return compareProperty(table, gIOFireWireAVCSubUnitType) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
}

IOReturn IOFireWireAVCSubUnit::AVCCommand(const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
    return fAVCUnit->AVCCommand(in, len, out, size);
}

IOReturn IOFireWireAVCSubUnit::AVCCommandInGeneration(UInt32 generation, const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
    return fAVCUnit->AVCCommandInGeneration(generation, in, len, out, size);
}

IOReturn IOFireWireAVCSubUnit::updateAVCCommandTimeout()
{
    return fAVCUnit->updateAVCCommandTimeout();
}

//
// handleOpen / handleClose
//

bool IOFireWireAVCSubUnit::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fAVCUnit->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

void IOFireWireAVCSubUnit::handleClose( IOService * forClient, IOOptionBits options )
{
	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fAVCUnit->close(this, options);
	}
}

IOReturn IOFireWireAVCSubUnit::message(UInt32 type, IOService *provider, void *argument)
{
    messageClients(type);
    
    return kIOReturnSuccess;
}

