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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 21 May 99 wgulland created.
 *
 */

#import "FWDebugging.h"

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#ifndef DEBUGLOG
#define DEBUGLOG kprintf
#endif

#import <IOKit/assert.h>

#import <IOKit/IOMessage.h>
#import <IOKit/IODeviceTreeSupport.h>

#import <IOKit/firewire/IOFireWireLink.h>
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFireWireUnit.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOConfigDirectory.h>
#import "IORemoteConfigDirectory.h"
#import "IOFireWireROMCache.h"

OSDefineMetaClassAndStructors(IOFireWireDeviceAux, IOFireWireNubAux);
OSMetaClassDefineReservedUnused(IOFireWireDeviceAux, 0);
OSMetaClassDefineReservedUnused(IOFireWireDeviceAux, 1);
OSMetaClassDefineReservedUnused(IOFireWireDeviceAux, 2);
OSMetaClassDefineReservedUnused(IOFireWireDeviceAux, 3);

#pragma mark -

// init
//
//

bool IOFireWireDeviceAux::init( IOFireWireDevice * primary )
{
	bool success = true;		// assume success
	
	// init super
	
    if( !IOFireWireNubAux::init( primary ) )
        success = false;
	
	if( success )
	{
		fTerminated = false;
	}
	
	return success;
}

// isTerminated
//
//

bool IOFireWireDeviceAux::isTerminated( void )
{
	return (fTerminated || fPrimary->isInactive());
}

// isTerminated
//
//

void IOFireWireDeviceAux::setTerminated( bool terminated )
{
	fTerminated = terminated;
}

// free
//
//

void IOFireWireDeviceAux::free()
{	    
	IOFireWireNubAux::free();
}

#pragma mark -
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
		oldPropTable->release();
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

#pragma mark -

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

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireNubAux * IOFireWireDevice::createAuxiliary( void )
{
	IOFireWireDeviceAux * auxiliary;
    
	auxiliary = new IOFireWireDeviceAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
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
    fControl->retain();

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
    // mark the ROM as invalid
    if(fDeviceROM)
        fDeviceROM->setROMState( IOFireWireROMCache::kROMStateInvalid );
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

#pragma mark -

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
            retain();	// retain ourself for the thread to use.
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

void IOFireWireDevice::readROMDirGlue(void *refcon, IOReturn status,
                        IOFireWireNub *nub, IOFWCommand *fwCmd)
{
	// unused
}

// readROMThreadFunc
//
//

void IOFireWireDevice::readROMThreadFunc( void *refcon )
{
    RomScan * 				romScan = (RomScan *)refcon;
    IOFireWireDevice * 		device = romScan->fDevice;
	
	//IOLog( "IOFireWireDevice::readROMThreadFunc %p entered\n", romScan );
	
	// Make sure there's only one of these threads running at a time
    IORecursiveLockLock(device->fROMLock);
    
	device->processROM( romScan );
	
	IORecursiveLockUnlock(device->fROMLock);
	IOFree(romScan, sizeof(RomScan));
    device->release();
	//IOLog( "IOFireWireDevice::readROMThreadFunc %p exited\n", romScan );
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
	OSObject * modelIDProperty = rootPropTable->getObject( gFireWireModel_ID );

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

		// if the unit doesn't have a model ID property, but the device does
		// then copy the property from the device
		OSObject * unitModelIDProperty = propTable->getObject( gFireWireModel_ID );
		if( unitModelIDProperty == NULL && modelIDProperty != NULL )
		{
			propTable->setObject( gFireWireModel_ID, modelIDProperty );
		}
		
		// copy the vendor name (if any) from the device to the unit
		if( vendorNameProperty )
		{
			propTable->setObject( gFireWireVendor_Name, vendorNameProperty );
		}
	}
	
	// always release iterators :)
	iterator->release();
}

// readRootDirectory
//
//

IOReturn IOFireWireDevice::readRootDirectory( IOConfigDirectory * directory, OSDictionary * propTable )
{
	IOReturn status = kIOReturnSuccess;
	
	UInt32 	modelID = 0;
	bool	modelIDPresent = false;
	
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
        if(result == kIOReturnSuccess) {
            OSNumber *num = OSNumber::withNumber(vendorID, 32);
            if(num) {
                propTable->setObject( gFireWireVendor_ID,  num);
                num->release();
            }
        }
	}
	
	// model name
	if( status == kIOReturnSuccess )
	{
		IOReturn 	result = kIOReturnSuccess;
		
		result = directory->getKeyValue( kConfigModelIdKey, modelID, &modelName );

		if( result == kIOFireWireConfigROMInvalid )
			status = result;
		
		if( result == kIOReturnSuccess )
			modelIDPresent = true;
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
			result = unit->getKeyValue( kConfigModelIdKey, modelID, &t );
	
			if( result == kIOFireWireConfigROMInvalid )
				status = result;
	
			if( result == kIOReturnSuccess )
				modelIDPresent = true;
				
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
	
	if( modelIDPresent )
	{
		if( status == kIOReturnSuccess )
		{
			OSObject *prop = OSNumber::withNumber(modelID, 32);
			propTable->setObject(gFireWireModel_ID, prop);
			prop->release();
		}
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
		
            while( (unit = OSDynamicCast(IOConfigDirectory, unitDirs->getNextObject())) )
			{
                UInt32 		unitSpecID = 0;
                UInt32 		unitSoftwareVersion = 0;
                UInt32		modelID = 0;
				bool		modelIDPresent = false;
				OSString *	t = NULL;
		
                result = unit->getKeyValue(kConfigUnitSpecIdKey, unitSpecID);
				if( result == kIOReturnSuccess )
					result = unit->getKeyValue(kConfigUnitSwVersionKey, unitSoftwareVersion);
                
				if( result == kIOReturnSuccess )
					result = unit->getKeyValue(kConfigModelIdKey, modelID, &t);
                
				if( result == kIOReturnSuccess )
					modelIDPresent = true;
					
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
		
						if( modelIDPresent )
						{
							prop = OSNumber::withNumber(modelID, 32);
							propTable->setObject(gFireWireModel_ID, prop);
							prop->release();
						}
						
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
					
					}  while( false );
					
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
	
	OSIterator * 	iterator = NULL;
	OSIterator * 	clientIterator = NULL;
	OSSet *			clientSet = NULL;
	OSObject *		client = NULL;
	
	//
	// make a local copy of the client set
	//
		
	if( status == kIOReturnSuccess )
	{
		clientSet = OSSet::withCapacity(2);
		if( clientSet == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		clientIterator = getClientIterator();
		if( clientIterator == NULL )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		clientIterator->reset();
		while( (client = clientIterator->getNextObject()) ) 
		{
			clientSet->setObject( client );
		}
		clientIterator->release();
	}
	
	if( status == kIOReturnSuccess )
	{
		clientIterator = OSCollectionIterator::withCollection( clientSet );
		if( clientIterator == NULL )
			status = kIOReturnError;
	}
	
	//
	// loop through all discovered units
	//
	
	if( status == kIOReturnSuccess )
	{	
		iterator = OSCollectionIterator::withCollection( unitSet );
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
				IOFireWireUnit * 	found = NULL;
				
				clientIterator->reset();
				while( (client = clientIterator->getNextObject()) ) 
				{
					found = OSDynamicCast(IOFireWireUnit, client);
					if( found && found->matchPropertyTable(propTable) ) 
					{
						break;
					}
					else
					{
						found = NULL;
					}
				}
				
				if( found )
				{
					found->setConfigDirectory( unit );
					clientSet->removeObject( found );
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
			
			}
			while( false );
			
			if( newDevice != NULL )
			{
				newDevice->release();
				newDevice = NULL;
			}
		}
	}
		
	if( iterator != NULL )
	{
		iterator->release();
	}
	
	if( clientIterator != NULL )
	{
		clientIterator->release();
	}
		
	if( clientSet != NULL )
	{
		clientSet->release();
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

// setNeedsRegisterServiceState
//
//

void IOFireWireDevice::setRegistrationState( RegistrationState state )
{
	fRegistrationState = state;
}

// matchPropertyTable
//
//

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

#pragma mark -

IOReturn IOFireWireDevice::message( UInt32 mess, IOService * provider,
                                    void * argument )
{
    // Propagate bus reset start/end messages
    if( kIOFWMessageServiceIsRequestingClose == mess ) 
    {
        messageClients( mess );
        return kIOReturnSuccess;
    }

	if( kIOFWMessagePowerStateChanged == mess )
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
                if( fNodeID == kFWBadNodeID && !isTerminated() ) 
				{
					setTerminated( true );
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
            if( fNodeID == kFWBadNodeID && !isTerminated() )
			{
				setTerminated( true );
                IOCreateThread(terminateDevice, this);
			}
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

#pragma mark -

// setNodeFlags
//
//

void IOFireWireDevice::setNodeFlags( UInt32 flags )
{
	
	fControl->closeGate();
	
    fNodeFlags |= flags;
    
    // IOLog( "IOFireWireNub::setNodeFlags fNodeFlags = 0x%08lx\n", fNodeFlags );
    
	configureNode();
	
	fControl->openGate();
}

// clearNodeFlags
//
//

void IOFireWireDevice::clearNodeFlags( UInt32 flags )
{
	
	fControl->closeGate();
	
    fNodeFlags &= ~flags;
    
    // IOLog( "IOFireWireNub::clearNodeFlags fNodeFlags = 0x%08lx\n", fNodeFlags );
    
	configureNode();
	
	fControl->openGate();
}

// getNodeFlags
//
//

UInt32 IOFireWireDevice::getNodeFlags( void )
{
    return fNodeFlags;
}

// configureNode
//
//

IOReturn IOFireWireDevice::configureNode( void )
{
	if( fNodeID != kFWBadNodeID )
    {
		// handle physical filter configuration
		configurePhysicalFilter();

		if( fNodeFlags & kIOFWEnableRetryOnAckD )
		{
			IOFireWireLink * fwim = fControl->getLink();
			fwim->setNodeFlags( fNodeID & 0x3f, kIOFWNodeFlagRetryOnAckD );
		}
    }
	
	return kIOReturnSuccess;
}

// configurePhysicalFilter
//
// set up physical filters for this node.  this is broken out into its
// own function because it's not only called by configureNode above, but
// by the controller when controller-wide physical access is enabled

void IOFireWireDevice::configurePhysicalFilter( void )
{
	if( fNodeID != kFWBadNodeID )
    {
		if( fNodeFlags & kIOFWDisableAllPhysicalAccess )
        {
			// kIOFWPhysicalAccessDisabledForGeneration only disables physical
			// access until the next bus reset at which point we will this code
			// will be reexecuted provided this device is still on the bus
			
			// kIOFWPhysicalAccessDisabled lasts across bus resets, therefore
			// it takes priority over kIOFWPhysicalAccessDisabledForGeneration
			
			fControl->setPhysicalAccessMode( kIOFWPhysicalAccessDisabledForGeneration );
        }
		
        if( (fNodeFlags & kIOFWDisablePhysicalAccess) )
        {
			fControl->setNodeIDPhysicalFilter( fNodeID & 0x3f, false );
        }
		else
		{
			// if the physical access mode has been set to a disabled state
			// then enabling this node's physical filter will have no
			// effect.
			
			fControl->setNodeIDPhysicalFilter( fNodeID & 0x3f, true );
		}
    }
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////
// address spaces
//

/*
 * Create local FireWire address spaces for the device to access
 */

IOFWPhysicalAddressSpace * IOFireWireDevice::createPhysicalAddressSpace(IOMemoryDescriptor *mem)
{
    IOFWPhysicalAddressSpace * space = fControl->createPhysicalAddressSpace(mem);
	
	if( space != NULL )
	{
		space->addTrustedNode( this );
	}
	
	return space;
}

IOFWPseudoAddressSpace * IOFireWireDevice::createPseudoAddressSpace(FWAddress *addr, UInt32 len, 
				FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    IOFWPseudoAddressSpace * space = fControl->createPseudoAddressSpace(addr, len, reader, writer, refcon);

	if( space != NULL )
	{
		space->addTrustedNode( this );
	}
	
	return space;

}
