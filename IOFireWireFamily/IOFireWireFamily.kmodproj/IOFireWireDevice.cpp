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

#include "FWDebugging.h"

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#ifndef DEBUGLOG
#define DEBUGLOG kprintf
#endif

#include <IOKit/assert.h>

#include <IOKit/IOMessage.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/firewire/IOFireWireLink.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOConfigDirectory.h>
#include "IORemoteConfigDirectory.h"
#include "IOFireWireROMCache.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireDevice, IOFireWireNub)
OSMetaClassDefineReservedUnused(IOFireWireDevice, 0);
OSMetaClassDefineReservedUnused(IOFireWireDevice, 1);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// RomScan
//
// A little struct for keeping track of our this pointer and generation
// when transitioning to a second thread during the ROM scan.

struct RomScan 
{
    IOFireWireDevice *		fDevice;
    UInt32 					fROMGeneration;
};

// IOFireWireUnitInfo
//
// A little class for keeping track of unit directories and prop tables
// between the try and commit phases of the ROM scan.  It's an OSObject
// so we can keep it in an OSSet.

class IOFireWireUnitInfo : public OSObject
{
    OSDeclareDefaultStructors(IOFireWireUnitInfo);

private:
    OSDictionary * fPropTable;
    IOConfigDirectory * fDirectory;
    
protected:
    virtual void free();
    
public:

	static IOFireWireUnitInfo * create( void );

	void setPropTable( OSDictionary * propTable );
	OSDictionary * getPropTable( void );
	
	void setDirectory( IOConfigDirectory * directory );
	IOConfigDirectory * getDirectory( void );
	
};

OSDefineMetaClassAndStructors(IOFireWireUnitInfo, OSObject);

// create
//
//

IOFireWireUnitInfo * IOFireWireUnitInfo::create( void )
{
    IOFireWireUnitInfo * me;
	
    me = new IOFireWireUnitInfo;
	
	return me;
}

// free
//
//

void IOFireWireUnitInfo::free()
{
    if( fPropTable != NULL ) 
    {
        fPropTable->release();
        fPropTable = NULL;
    }
    
    if( fDirectory != NULL )
    {
    	fDirectory->release();
    	fDirectory = NULL;
    }

    OSObject::free();
}

// setPropTable
//
//

void IOFireWireUnitInfo::setPropTable( OSDictionary * propTable )
{
	OSDictionary * oldPropTable = fPropTable;
	
	propTable->retain();
	fPropTable = propTable;
	
	if( oldPropTable )
		fPropTable->release();
}

// getPropTable
//
//

OSDictionary * IOFireWireUnitInfo::getPropTable( void )
{
	return fPropTable;	
}

// setDirectory
//
//

void IOFireWireUnitInfo::setDirectory( IOConfigDirectory * directory )
{
	IOConfigDirectory * oldDirectory = fDirectory;
	
	directory->retain();
	fDirectory = directory;
	
	if( oldDirectory )
		oldDirectory->release();
}

// getDirectory
//
//

IOConfigDirectory * IOFireWireUnitInfo::getDirectory( void )
{
	return fDirectory;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFireWireDevice::init(OSDictionary *propTable, const IOFWNodeScan *info)
{
    if(!IOFireWireNub::init(propTable))
       return false;
       
    // Terminator...
    // fUniqueID = (UInt64)this;
    
    
    if(info->fROMSize > 8) {
        UInt32 maxPackLog =
        ((info->fBuf[2] & kFWBIBMaxRec) >> kFWBIBMaxRecPhase) + 1;
        if(maxPackLog == 1) {
            IOLog("Illegal maxrec, using 512 bytes\n");
            maxPackLog = 9;
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
	// unused
}

void IOFireWireDevice::terminateDevice(void *refcon)
{
    IOFireWireDevice *me = (IOFireWireDevice *)refcon;
    
	me->fControl->closeGate();
	
    //IOLog("terminating FW device %p\n", me);
    
    // Make sure we should still terminate.
    me->lockForArbitration();
    if( me->fNodeID == kFWBadNodeID && !me->isInactive() && !me->isOpen() ) 
	{
		if( me->fDeviceROM )
		{
			me->fDeviceROM->setROMState( IOFireWireROMCache::kROMStateInvalid );
        }

		// release arbitration lock before terminating.
        // this leaves a small hole of someone opening the device right here,
        // which shouldn't be too bad - the client will just get terminated too.
        me->unlockForArbitration();
		
		me->terminate();

    }
	else
	{
		me->unlockForArbitration();
    }
	
	//IOLog("terminated FW device %p\n", me);

	me->fControl->openGate();
}


void IOFireWireDevice::free()
{
	FWKLOG(( "IOFireWireDevice@0x%08lx::free()\n", (UInt32)this ));		
    
	if( fDeviceROM )
	{
		fDeviceROM->setROMState( IOFireWireROMCache::kROMStateInvalid );
        fDeviceROM->release();
		fDeviceROM = NULL;
	}
	
	if(fROMLock)
    {
	    IORecursiveLockFree(fROMLock);
    }
	
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

// setNeedsRegisterServiceState
//
//

void IOFireWireDevice::setRegistrationState( RegistrationState state )
{
	fRegistrationState = state;
}

// setNodeROM
//
//

void IOFireWireDevice::setNodeROM(UInt32 gen, UInt16 localID, const IOFWNodeScan *info)
{
    OSObject *prop;
    
    IOFireWireROMCache *	rom;
		
	// setNodeROM is called twice on a bus reset
	//
	// once when the bus is suspended with a nil info pointer.  at this point
	// we set our nodeID to kFWBadNodeID
	//
	// once when the bus is resumed with a valid info pointer. at this point
	// we set our node id to the node id in the info struct
	
	// node ids and generation must be set up here, else we won't be
	// able to scan the ROM later
	
	fLocalNodeID = localID;
    fGeneration = gen;
		
    if( info ) 
	{
        fNodeID = info->fAddr.nodeID;
    }
    else 
	{
        fNodeID = kFWBadNodeID;
    }

	
	FWKLOG(( "IOFireWireDevice@0x%08lx::setNodeROM entered with nodeID = 0x%04x\n", (UInt32)this, fNodeID ));
	
	prop = OSNumber::withNumber( fNodeID, 16 );
    setProperty( gFireWireNodeID, prop );
    prop->release();

	//
	// if we've just be resumed, reconfigure our node 
	//
	
    if( fNodeID != kFWBadNodeID )
    {
		configureNode();  // configure node based on node flags
	}
    
	//
	// if we've just be suspended, send the suspended message and return
	//
	
    if( !info ) 
	{
        // Notify clients that the current state is suspended
		
		// the device rom may already be suspended if a ROM read on another
		// thread received a bus reset error, double suspending is fine
		fDeviceROM->setROMState( IOFireWireROMCache::kROMStateSuspended );
        
		messageClients( kIOMessageServiceIsSuspended );
        
		return;  	// node is suspended
    }
	
	//
    // store selfIDs
	//
	
    prop = OSData::withBytes( info->fSelfIDs, info->fNumSelfIDs*sizeof(UInt32) );
    setProperty( gFireWireSelfIDs, prop );
    prop->release();

	//
    // if the ROM has not changed, send the resume message and return
	//
	
	UInt32 newROMSize = info->fROMSize;
	
	bool rom_changed = true;
	
	if( fDeviceROM != NULL )
	{
		rom_changed = fDeviceROM->hasROMChanged( info->fBuf, newROMSize );
	}
	
	if( !rom_changed )
	{
		fDeviceROM->setROMState( IOFireWireROMCache::kROMStateResumed, fGeneration );
		messageClients( kIOMessageServiceIsResumed );	// Safe to continue
		
		#if IOFIREWIREDEBUG > 0
		IOLog("IOFireWireDevice, ROM unchanged 0x%p\n", this);
		#endif
		
		FWKLOG(( "IOFireWireDevice@0x%08lx::setNodeROM exited - ROM unchanged\n", (UInt32)this ));
	
		return;		// ROM unchanged, node resumed
	}
	
	//
	// at this point we know we are resumed and our ROM has changed,
	// so we increment the ROM generation.  This will eventually cause 
	// any threads that have been scheduled but not yet run to exit
	//
	
	fROMGeneration++;
	
	// if the ROM changed and we hadn't registered this device before
	// lets try it now
	
	if( fRegistrationState == kDeviceNotRegistered )
	{
		setRegistrationState( kDeviceNeedsRegisterService );
		adjustBusy( 1 );
	}
	
	//
	// if we have the third quad of the BIB, extract the vendor id
	// and publish it in the registry
	//
	
    if( newROMSize > 12 ) 
	{
        UInt32 vendorID = info->fBuf[3] >> 8;
        prop = OSNumber::withNumber( vendorID, 32 );
        setProperty( gFireWireVendor_ID, prop );
        prop->release();
    }
    
	//
	// create new ROM cache
	//
	
	rom = IOFireWireROMCache::withOwnerAndBytes( this, info->fBuf, newROMSize, fGeneration );
    setProperty( gFireWireROM, rom );

	// release and invalidate the old one if necessary
	if( fDeviceROM ) 
	{
		fDeviceROM->setROMState( IOFireWireROMCache::kROMStateInvalid );
		fDeviceROM->release();
	}

	fDeviceROM = rom;
	
	//
	// if we've got a full BIB, create a thread to read the ROM.  
	// this thread will go on to create or resume the units on this device
	//
	
	if( newROMSize == 20 ) 
	{
		RomScan *romScan = (RomScan *)IOMalloc( sizeof(RomScan) );
		if( romScan ) 
		{
			romScan->fROMGeneration = fROMGeneration;
			romScan->fDevice = this;
			IOCreateThread( readROMThreadFunc, romScan );
		}
	}
	else
	{
		// if it is only a minimal config ROM, we won't be calling registerService
		// on this device, so clear that state and reset the busy count
		
		if( fRegistrationState == kDeviceNeedsRegisterService )
		{
			setRegistrationState( kDeviceNotRegistered );
			adjustBusy( -1 );
		}
	}
	
			
	FWKLOG(( "IOFireWireDevice@0x%08lx::setNodeROM exited\n", (UInt32)this ));	
}

// readROMThreadFunc
//
//

void IOFireWireDevice::readROMThreadFunc( void *refcon )
{
    RomScan * 				romScan = (RomScan *)refcon;
    IOFireWireDevice * 		device = romScan->fDevice;
	
//	IOLog( "IOFireWireDevice::readROMThreadFunc entered\n" );
	
	// Make sure there's only one of these threads running at a time
    IORecursiveLockLock(device->fROMLock);
    
	device->processROM( romScan );
	
	IORecursiveLockUnlock(device->fROMLock);
	IOFree(romScan, sizeof(RomScan));

//	IOLog( "IOFireWireDevice::readROMThreadFunc exited\n" );
}

// processROM
//
//

void IOFireWireDevice::processROM( RomScan *romScan )
{
	IOReturn status = kIOReturnSuccess;
	
	IOConfigDirectory *		directory = NULL;
	IOFireWireROMCache *	rom = NULL;
		
	OSSet *			unitSet = NULL;
	OSDictionary *  rootPropTable = NULL;
	
	//
	// atomically get the current rom cache and its generation
	//
	
	fControl->closeGate();
	
	rom = fDeviceROM;
	rom->retain();
	
	UInt32 generation = fROMGeneration;
	
	fControl->openGate();


	FWKLOG(( "IOFireWireDevice@0x%08lx::processROM generation %ld entered\n", (UInt32)this, generation ));

	//
	// bail if we're on a ROM scan thread for a different generation
	//
	
	if( romScan->fROMGeneration != generation )
	{
		FWKLOG(( "IOFireWireDevice@0x%08lx::processROM generation %ld != romScan->fROMGeneration\n", (UInt32)this, generation ));
		status = kIOReturnError;
	}
	
	//
	// create a config directory for the device
	//
	
	if( status == kIOReturnSuccess )
	{
		directory = IORemoteConfigDirectory::withOwnerOffset( rom, 5, kConfigRootDirectoryKey );
		if( directory == NULL ) 
		{
			#if IOFIREWIREDEBUG > 0
			IOLog("whoops, no root directory!!\n");
			#endif
			
			status = kIOReturnNoMemory;
		}
	}
	
	//
	// read and publish values for the device
	//
	
	if( status == kIOReturnSuccess )
	{
		rootPropTable = OSDictionary::withCapacity(7);
		if( rootPropTable == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = readRootDirectory( directory, rootPropTable );
	}
	
	//
	// look for unit directories
	//
	
	if( status == kIOReturnSuccess )
	{
		unitSet = OSSet::withCapacity(2);
		if( unitSet == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = readUnitDirectories( directory, unitSet );
	}
	
	//
	// at this point we should have read all the values we need to 
	// initialize the device and units
	//
	
	if( status == kIOReturnSuccess )
	{
		preprocessDirectories( rootPropTable, unitSet );
	}
	
	//
	// update the device's config directory
	//
	
	if( status == kIOReturnSuccess )
	{
		fControl->closeGate();

		RegistrationState registrationState = fRegistrationState;
		
		// the following routines aren't supposed to fail
		
		status = setConfigDirectory( directory );

		FWKLOGASSERT( status == kIOReturnSuccess );

		status = processRootDirectory( rootPropTable );

		FWKLOGASSERT( status == kIOReturnSuccess );

		status = processUnitDirectories( unitSet );

		FWKLOGASSERT( status == kIOReturnSuccess );
		
		// we don't want to lower our busy count until we've called registerService
		// on all the units, however processRootDirectory may reset the registrationState 
		// so we latch it before processRootDirectory
		
		if( registrationState == kDeviceNeedsRegisterService )
		{
			adjustBusy( -1 );
//			FWKLOG(( "IOFireWireDevice@0x%08lx::processROM adjustBusy(-1) generation %ld\n", (UInt32)this, generation ));
		}
		
		messageClients( kIOMessageServiceIsResumed );	// Safe to continue
		
		fControl->openGate();
	}
	else if( status != kIOFireWireConfigROMInvalid )
	{
		fControl->closeGate();
		
		// if there was an error reading the ROM then we need to reset our busy state
		// to stay consistent
		
		// if the read failed because the ROM went invalid, then we don't need to do this
		// because we will come back to the ROM reading code on a new thread
		
		if( fRegistrationState == kDeviceNeedsRegisterService )
		{
			setRegistrationState( kDeviceNotRegistered );
			adjustBusy( -1 );
		}
		
		fControl->openGate();
	}
	
	//
	// clean up
	//
	
	if( unitSet != NULL )
	{
		unitSet->release();
	}
	
	if( rootPropTable != NULL )
	{
		rootPropTable->release();
	}
	
	if( directory != NULL )
	{
		directory->release();
	}
	
	if( rom != NULL )
	{
		rom->release();
	}
	
	FWKLOG(( "IOFireWireDevice@0x%08lx::processROM generation %ld exited\n", (UInt32)this, generation ));
}

// preprocessDirectories
//
//

void IOFireWireDevice::preprocessDirectories( OSDictionary * rootPropTable, OSSet * unitSet )
{
	OSObject * modelNameProperty = rootPropTable->getObject( gFireWireProduct_Name );
	OSObject * vendorNameProperty = rootPropTable->getObject( gFireWireVendor_Name );

	OSIterator * iterator = OSCollectionIterator::withCollection( unitSet );
	iterator->reset();
	
	IOFireWireUnitInfo * info = NULL;
	while( (info = (IOFireWireUnitInfo *) iterator->getNextObject()) )
	{
		OSDictionary * propTable = info->getPropTable();
		
		// if the unit doesn't have a model name property, but the device does
		// then copy the property from the device
		OSObject * unitModelNameProperty = propTable->getObject( gFireWireProduct_Name );
		if( unitModelNameProperty == NULL && modelNameProperty != NULL )
		{
			propTable->setObject( gFireWireProduct_Name, modelNameProperty );
		}

		// copy the vendor name (if any) from the device to the unit
		if( vendorNameProperty )
		{
			propTable->setObject( gFireWireVendor_Name, vendorNameProperty );
		}
	}
}

// readRootDirectory
//
//

IOReturn IOFireWireDevice::readRootDirectory( IOConfigDirectory * directory, OSDictionary * propTable )
{
	IOReturn status = kIOReturnSuccess;
	
	OSString * modelName = NULL;
	OSString * vendorName = NULL;

	FWKLOG(( "IOFireWireDevice@0x%08lx::readRootDirectory entered\n", (UInt32)this ));
	
	//
	// read device keys
	//
	
	// vendor name
	if( status == kIOReturnSuccess )
	{
		IOReturn 	result = kIOReturnSuccess;
		UInt32 		vendorID = 0;
	
		result = directory->getKeyValue( kConfigModuleVendorIdKey, vendorID, &vendorName );

		if( result == kIOFireWireConfigROMInvalid )
			status = result;
	}
	
	// model name
	if( status == kIOReturnSuccess )
	{
		IOReturn 	result = kIOReturnSuccess;
		UInt32		modelID = 0;
		
		result = directory->getKeyValue( kConfigModelIdKey, modelID, &modelName );

		if( result == kIOFireWireConfigROMInvalid )
			status = result;
	}

	// model and vendor
	if( status == kIOReturnSuccess )
	{
		IOReturn 	result = kIOReturnSuccess;

	    OSString *			t = NULL;
		IOConfigDirectory *	unit = NULL;

		result = directory->getKeyValue( kConfigModuleVendorIdKey, unit, &t );

		if( result == kIOFireWireConfigROMInvalid )
			status = result;

		if( result == kIOReturnSuccess && t != NULL )
		{
			if( vendorName )
				vendorName->release();
			vendorName = t;
			t = NULL;
		} 
		
		if( result == kIOReturnSuccess )
		{
			UInt32		modelID = 0;
			
			result = unit->getKeyValue( kConfigModelIdKey, modelID, &t );
	
			if( result == kIOFireWireConfigROMInvalid )
				status = result;
	
			if( result == kIOReturnSuccess && t != NULL )
			{
				if( modelName )
					modelName->release();
				modelName = t;
				t = NULL;
			} 
			
			unit->release();
		}
	}
	
	//
	// store values in a prop table for later processing
	//
	
	if( modelName != NULL )
	{
		if( status == kIOReturnSuccess )
			propTable->setObject( gFireWireProduct_Name, modelName );
        
		modelName->release();
    }
	
	if( vendorName != NULL )
	{
		if( status == kIOReturnSuccess )
			propTable->setObject( gFireWireVendor_Name, vendorName  );
			
		vendorName->release();
	}
	
	FWKLOG(( "IOFireWireDevice@0x%08lx::readRootDirectory returned status = 0x%08lx\n", (UInt32)this, (UInt32)status ));
		
	return status;
}

// processRootDirectory
//
// merge properties into the device registry entry
//
// called with the workloop lock held

IOReturn IOFireWireDevice::processRootDirectory( OSDictionary * propTable )
{	
	IOReturn status = kIOReturnSuccess;
	
	OSSymbol * key	= NULL;
	OSObject * property = NULL;

	OSCollectionIterator * iterator = OSCollectionIterator::withCollection( propTable );
	while( NULL != (key = OSDynamicCast(OSSymbol, iterator->getNextObject())) )
	{
		property = propTable->getObject( key );		
		setProperty( key, property );
	}	
	iterator->release();
	
	// if this is the first time through, we need to call registerService
	// on this device

	if( fRegistrationState == kDeviceNeedsRegisterService )
	{
		setRegistrationState( kDeviceRegistered );
		registerService();
	}
	
	return status;
}

// readUnitDirectories
//
//

IOReturn IOFireWireDevice::readUnitDirectories( IOConfigDirectory * directory, OSSet * unitInfo )
{
	IOReturn status = kIOReturnSuccess;
	
	OSIterator *	unitDirs = NULL;

	OSString *		modelName = NULL;

	FWKLOG(( "IOFireWireDevice@0x%08lx::readUnitDirectory entered\n", (UInt32)this ));

	if( status == kIOReturnSuccess )
	{
		IOReturn result = kIOReturnSuccess;
		
		result = directory->getKeySubdirectories( kConfigUnitDirectoryKey, unitDirs );
        
		//IOLog( "IOFireWireDevice::processROM getKeyValue getKeySubdirectories result = 0x%08lx\n", (UInt32)result );
		
		if( result == kIOFireWireConfigROMInvalid )
			status = result;
	
		if( result == kIOReturnSuccess ) 
		{
			IOConfigDirectory * unit = NULL;
		
            while( unit = OSDynamicCast( IOConfigDirectory, unitDirs->getNextObject() ) )
			{
                UInt32 		unitSpecID = 0;
                UInt32 		unitSoftwareVersion = 0;
                UInt32		modelID = 0;
				OSString *	t = NULL;
		
                result = unit->getKeyValue(kConfigUnitSpecIdKey, unitSpecID);
				if( result == kIOReturnSuccess )
					result = unit->getKeyValue(kConfigUnitSwVersionKey, unitSoftwareVersion);
                
				if( result == kIOReturnSuccess )
					result = unit->getKeyValue(kConfigModelIdKey, modelID, &t);
                
				if( result == kIOFireWireConfigROMInvalid )
					status = result;
			
				if( result == kIOReturnSuccess && t != NULL ) 
				{
                    if( modelName )
                        modelName->release();
                    modelName = t;
                    t = NULL;
                }
				
				if( status == kIOReturnSuccess )
				{
					OSDictionary * propTable = 0;
					IOFireWireUnit * newDevice = 0;

					// Add entry to registry.
					do 
					{
						OSObject * prop;
						
						propTable = OSDictionary::withCapacity(7);
						if( !propTable )
							continue;
							
						/*
						* Set the IOMatchCategory so that things that want to connect to
						* the device still can even if it already has IOFireWireUnits
						* attached
						*/
						
						prop = OSString::withCString("FireWire Unit");
						propTable->setObject(gIOMatchCategoryKey, prop);
						prop->release();
	
						if( modelName )
							propTable->setObject(gFireWireProduct_Name, modelName);
		
						prop = OSNumber::withNumber(unitSpecID, 32);
						propTable->setObject(gFireWireUnit_Spec_ID, prop);
						prop->release();
						prop = OSNumber::withNumber(unitSoftwareVersion, 32);
						propTable->setObject(gFireWireUnit_SW_Version, prop);
						prop->release();
	
						// Copy over matching properties from Device
						prop = getProperty(gFireWireVendor_ID);
						if( prop )
							propTable->setObject(gFireWireVendor_ID, prop);
						prop = getProperty(gFireWire_GUID);
						if( prop )
							propTable->setObject(gFireWire_GUID, prop);
						
						IOFireWireUnitInfo * info = IOFireWireUnitInfo::create();
						info->setDirectory( unit );
						info->setPropTable( propTable );
						unitInfo->setObject( info );
						info->release();
					} 
					while( false );
					
					if( newDevice != NULL )
						newDevice->release();
						
					if( propTable != NULL )
						propTable->release();
				}
	
				if( modelName != NULL )
                                {
                                    modelName->release();
                                    modelName = NULL;
                                }
			}
			
			unitDirs->release();
		}
	}

	FWKLOG(( "IOFireWireDevice@0x%08lx::readUnitDirectory returned status = 0x%08lx\n", (UInt32)this, (UInt32)status ));
	
	return status;
}

// processUnitDirectories
//
// called with the workloop lock held

IOReturn IOFireWireDevice::processUnitDirectories( OSSet * unitSet )
{
	IOReturn status = kIOReturnSuccess;
	
	OSIterator * iterator = OSCollectionIterator::withCollection( unitSet );
	iterator->reset();
	
	IOFireWireUnitInfo * info = NULL;
	while( (info = (IOFireWireUnitInfo *) iterator->getNextObject()) )
	{
		IOFireWireUnit * newDevice = 0;
		
		OSDictionary *	propTable = info->getPropTable();
		IOConfigDirectory * unit = info->getDirectory();
		
		// Check if unit directory already exists
		do 
		{
			OSIterator *		childIterator;
			IOFireWireUnit * 	found = NULL;
			
			childIterator = getClientIterator();
			if( childIterator ) 
			{
				OSObject *child;
				while( (child = childIterator->getNextObject()) ) 
				{
					found = OSDynamicCast(IOFireWireUnit, child);
					if( found && found->matchPropertyTable(propTable) ) 
					{
						break;
					}
					else
					{
						found = NULL;
					}
				}
				
				childIterator->release();
				if(found)
				{
					found->setConfigDirectory( unit );
					break;
				}
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
		
		}
		while( false );
		
		if( newDevice != NULL )
		{
			newDevice->release();
			newDevice = NULL;
		}
	}
	
	if( iterator != NULL )
	{
		iterator->release();
	}
	
	return status;
}

IOReturn IOFireWireDevice::cacheROM(OSData *rom, UInt32 offset, const UInt32 *&romBase)
{
	// unsupported
	
	return kIOReturnError;
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
            ok = IOService::handleOpen( this, options, arg );
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
                IOService::handleClose( this, options );
                
                // terminate if we're no longer on the bus and haven't already been terminated.
                if( fNodeID == kFWBadNodeID && !isInactive() ) {
                    IOCreateThread(terminateDevice, this);
                }
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
                IOCreateThread(terminateDevice, this);
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

void IOFireWireDevice::setNodeFlags( UInt32 flags )
{
	
	fControl->closeGate();
	
    fNodeFlags |= flags;
    
    // IOLog( "IOFireWireNub::setNodeFlags fNodeFlags = 0x%08lx\n", fNodeFlags );
    
	configureNode();
	
	fControl->openGate();
}

void IOFireWireDevice::clearNodeFlags( UInt32 flags )
{
	
	fControl->closeGate();
	
    fNodeFlags &= ~flags;
    
    // IOLog( "IOFireWireNub::setNodeFlags fNodeFlags = 0x%08lx\n", fNodeFlags );
    
	configureNode();
	
	fControl->openGate();
}


UInt32 IOFireWireDevice::getNodeFlags( void )
{
    return fNodeFlags;
}

IOReturn IOFireWireDevice::configureNode( void )
{
	if( fNodeID != kFWBadNodeID )
    {
		if( fNodeFlags & kIOFWDisableAllPhysicalAccess )
        {
            IOFireWireLink * fwim = fControl->getLink();
            fwim->setNodeIDPhysicalFilter( kIOFWAllPhysicalFilters, false );
        }
        else if( fNodeFlags & kIOFWDisablePhysicalAccess )
        {
            IOFireWireLink * fwim = fControl->getLink();
            fwim->setNodeIDPhysicalFilter( fNodeID & 0x3f, false );
        }

		if( fNodeFlags & kIOFWEnableRetryOnAckD )
		{
			IOFireWireLink * fwim = fControl->getLink();
			fwim->setNodeFlags( fNodeID & 0x3f, kIOFWNodeFlagRetryOnAckD );
		}
    }
	
	return kIOReturnSuccess;
}
