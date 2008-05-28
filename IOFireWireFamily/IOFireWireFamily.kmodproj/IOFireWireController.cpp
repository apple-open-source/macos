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
/*
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 27 April 99 wgulland created.
 *
 */

// public
#import <IOKit/firewire/IOFWUtils.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOLocalConfigDirectory.h>
#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFWLocalIsochPort.h>
#import <IOKit/firewire/IOFWDCLProgram.h>
#import <IOKit/firewire/IOFireWirePowerManager.h>
#import <IOKit/firewire/IOFWSimpleContiguousPhysicalAddressSpace.h>
#import <IOKit/pwr_mgt/RootDomain.h>
#import <IOKit/firewire/IOFWAsyncStreamListener.h>
#import <IOKit/firewire/IOFWPHYPacketListener.h>

#import "IOFWAsyncStreamReceiver.h"

// protected
#import <IOKit/firewire/IOFWWorkLoop.h>
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "FWDebugging.h"
#import "IOFireWireLocalNode.h"
#import "IOFWQEventSource.h"
#import "IOFireWireIRM.h"

// system
#import <IOKit/IOKitKeys.h>
#import <IOKit/IOBufferMemoryDescriptor.h>
#import <IOKit/IODeviceTreeSupport.h>
#import <IOKit/IOMessage.h>
#import <IOKit/IOTimerEventSource.h>

///////////////////////////////////////////////////////////////////////////////////
// timing constants
//

// 100 mSec delay after bus reset before scanning bus
// to make first generation Sony cameras happy
#define kScanBusDelay				100	

// 35000 mSec delay for a bus reset to become allowed after one was requested
#define kBusResetStartTimeout		35000

// 1000 mSec delay for a bus reset to occur after one was requested
#define kBusResetTimeout			1000

// 100 mSec delay for self ids to arrive after a bus reset
#define kSelfIDTimeout				1000

// 1000 mSec delay before pruning devices
// this will end up being kNormalDevicePruneDelay + kRepeatResetDelay because of gap count optimization
#define kNormalDevicePruneDelay		1000

// 2000 mSec delay between bus resets
// from the 1394a spec
#define kRepeatResetDelay			2000

// 3000 mSec delay before pruning last device 
// should generally equal kNormalDevicePruneDelay + kRepeatResetDelay
#define kOnlyNodeDevicePruneDelay	3000

// 15000 mSec delay before pruning devices after wake
// needs to be at least long enough for the iPod to reboot into disk mode
#define kWakeDevicePruneDelay		15000

// the maximum amount of time we will allow a device to exist undiscovered
#define kDeviceMaximuPruneTime		45000

///////////////////////////////////////////////////////////////////////////////////

#define kFireWireGenerationID		"FireWire Generation ID"
#define kFireWireLoggingMode		"Logging Mode Enabled"
#define kFWBusScanInProgress		"-1"

#define FWAddressToID(addr) (addr & 63)

enum requestRefConBits 
{
    kRequestLabel = kFWAsynchTTotal-1,	// 6 bits
    kRequestExtTCodeShift = 6,
    kRequestExtTCodeMask = 0x3fffc0,	// 16 bits
    kRequestIsComplete = 0x20000000,	// Was write request already ack-complete?
    kRequestIsLock = 0x40000000,
    kRequestIsQuad = 0x80000000
};

enum
{
	kFWInvalidPort = 0xffffffff
};

const OSSymbol *gFireWireROM;
const OSSymbol *gFireWireNodeID;
const OSSymbol *gFireWireSelfIDs;
const OSSymbol *gFireWireUnit_Spec_ID;
const OSSymbol *gFireWireUnit_SW_Version;
const OSSymbol *gFireWireVendor_ID;
const OSSymbol *gFireWire_GUID;
const OSSymbol *gFireWireSpeed;
const OSSymbol *gFireWireVendor_Name;
const OSSymbol *gFireWireProduct_Name;
const OSSymbol *gFireWireModel_ID;
const OSSymbol *gFireWireTDM;

const IORegistryPlane * IOFireWireBus::gIOFireWirePlane = NULL;

#if __ppc__

// FireWire bus has two power states, off and on
#define number_of_power_states 2

enum
{
    kFWPMSleepState = 0,
    kFWPMWakeState = 1
};

// Note: This defines two states. off and on.
static IOPMPowerState ourPowerStates[number_of_power_states] = 
{
	{1,0,0,0,0,0,0,0,0,0,0,0},
	{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#else

// FireWire bus has two power states, off and on
#define number_of_power_states 3

enum
{
    kFWPMSleepState = 0,
    kFWPMWakeState = 2
};

// Note: This defines two states. off and on.
static IOPMPowerState ourPowerStates[number_of_power_states] = 
{
	{1,0,0,0,0,0,0,0,0,0,0,0},
	{1,kIOPMDoze,kIOPMDoze,kIOPMDoze,0,0,0,0,0,0,0,0},
	{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#endif

OSDefineMetaClassAndStructors(IOFireWireDuplicateGUIDList, OSObject);

IOFireWireDuplicateGUIDList * IOFireWireDuplicateGUIDList::create( void )
{
    IOFireWireDuplicateGUIDList * me;
	
    me = new IOFireWireDuplicateGUIDList;
	
	return me;
}

void IOFireWireDuplicateGUIDList::free()
{
	IOFWDuplicateGUIDRec		*	GUIDRec;
	IOFWDuplicateGUIDRec		*	GUIDtoFree;
	
	GUIDRec = fFirstGUID;
	
	while( GUIDRec )
	{
		GUIDtoFree = GUIDRec;
		
		GUIDRec = GUIDRec->fNextGUID;

		IOFree( GUIDtoFree, sizeof( IOFWDuplicateGUIDRec ) );
	}

    OSObject::free();
}

void IOFireWireDuplicateGUIDList::addDuplicateGUID( CSRNodeUniqueID guid, UInt32 gen )
{
	IOFWDuplicateGUIDRec		* 	newGUID;
	
	if( !guid || findDuplicateGUID( guid, gen ) )
		return;	// Already found this one.
	
	newGUID = (IOFWDuplicateGUIDRec *) IOMalloc( sizeof(IOFWDuplicateGUIDRec));
	
	FWKLOG(("addDuplicateGUID adding GUID %08x %08x.\n",(unsigned int )(guid >> 32),(unsigned int )(guid & 0xffffffff)));

	IOLog("FireWire Error: Devices with identical unique ID: %08x %08x cannot be used.\n",(unsigned int )(guid >> 32),(unsigned int )(guid & 0xffffffff));

	newGUID->fGUID = guid;
	newGUID->fLastGenSeen = gen;
	newGUID->fNextGUID = fFirstGUID;
	fFirstGUID = newGUID;	
}

void IOFireWireDuplicateGUIDList::removeDuplicateGUID( CSRNodeUniqueID guid )
{
	IOFWDuplicateGUIDRec		*	GUIDRec;
	IOFWDuplicateGUIDRec		*	prevGUID;
	
	GUIDRec = fFirstGUID;
	prevGUID = NULL;
	
	while( GUIDRec )
	{
		if( GUIDRec->fGUID == guid )
		{
			if( prevGUID )
				prevGUID->fNextGUID = GUIDRec->fNextGUID;
			else
				fFirstGUID = GUIDRec->fNextGUID;
			
			FWKLOG(("removeDuplicateGUID removing GUID %08x %08x.\n",(unsigned int )(guid >> 32),(unsigned int )(guid & 0xffffffff)));

			IOFree( GUIDRec, sizeof( IOFWDuplicateGUIDRec ) );
			
			break;
		}
		prevGUID = GUIDRec;
		GUIDRec = GUIDRec->fNextGUID;
	}
}

bool IOFireWireDuplicateGUIDList::findDuplicateGUID( CSRNodeUniqueID guid, UInt32 gen )
{
	IOFWDuplicateGUIDRec		*	GUIDRec;
	
	//FWKLOG(("findDuplicateGUID looking for GUID %08x %08x.\n",(unsigned int )(guid >> 32),(unsigned int )(guid & 0xffffffff)));

	GUIDRec = fFirstGUID;
	while( GUIDRec )
	{
		if( GUIDRec->fGUID == guid )
		{

			FWKLOG(("findDuplicateGUID found GUID %08x %08x.\n",(unsigned int )(guid >> 32),(unsigned int )(guid & 0xffffffff)));


			GUIDRec->fLastGenSeen = gen;
			
			return( true );
		}
		GUIDRec = GUIDRec->fNextGUID;			
	}
	
	return( false );
}

OSDefineMetaClassAndStructors(IOFireWireControllerAux, IOFireWireBusAux);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 0);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 1);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 2);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 3);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 4);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 5);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 6);
OSMetaClassDefineReservedUnused(IOFireWireControllerAux, 7);

#pragma mark -

// init
//
//

bool IOFireWireControllerAux::init( IOFireWireController * primary )
{
	bool success = true;		// assume success

	success = IOFireWireBusAux::init();
	
	fPrimary = primary;
	
	return success;
}

// free
//
//

void IOFireWireControllerAux::free()
{	    
	IOFireWireBusAux::free();
}

IOFWDCLPool *
IOFireWireControllerAux::createDCLPool ( unsigned capacity ) const
{
	return fPrimary->getLink()->createDCLPool( capacity ) ;
}

// getMaxRec
//
//

UInt8 IOFireWireControllerAux::getMaxRec( void )
{
	return fMaxRec;
}

IOFWBufferFillIsochPort *
IOFireWireControllerAux::createBufferFillIsochPort() const
{
	return fPrimary->getLink()->createBufferFillIsochPort() ;
}

// getFireWirePhysicalAddressMask
//
//

UInt64 IOFireWireControllerAux::getFireWirePhysicalAddressMask( void )
{
	// all current FW hardware is 32 bit
	return 0x00000000FFFFFFFFULL;
}

// getFireWirePhysicalAddressBits
//
//

UInt32 IOFireWireControllerAux::getFireWirePhysicalAddressBits( void )
{
	// all current FW hardware is 32 bit
	return 32;
}

// getFireWirePhysicalBufferMask
//
//

UInt64 IOFireWireControllerAux::getFireWirePhysicalBufferMask( void )
{
	// all current FW hardware is 32 bit
	return 0x00000000FFFFFFFFULL;
}

// getFireWirePhysicalBufferBits
//
//

UInt32 IOFireWireControllerAux::getFireWirePhysicalBufferBits( void )
{
	// all current FW hardware is 32 bit
	return 32;
}

// createSimpleContiguousPhysicalAddressSpace
//
//

IOFWSimpleContiguousPhysicalAddressSpace * 
IOFireWireControllerAux::createSimpleContiguousPhysicalAddressSpace( vm_size_t size, IODirection direction )
{
    IOFWSimpleContiguousPhysicalAddressSpace * space;
    space = new IOFWSimpleContiguousPhysicalAddressSpace;
    if( !space )
        return NULL;
    
	if( !space->init( fPrimary, size, direction ) ) 
	{
        space->release();
        space = NULL;
    }
	
    return space;
}

// createSimplePhysicalAddressSpace
//
//

IOFWSimplePhysicalAddressSpace * 
IOFireWireControllerAux::createSimplePhysicalAddressSpace( vm_size_t size, IODirection direction )
{
    IOFWSimplePhysicalAddressSpace * space;
    space = new IOFWSimplePhysicalAddressSpace;
    if( !space )
        return NULL;
    
	if( !space->init( fPrimary, size, direction ) ) 
	{
        space->release();
        space = NULL;
    }
	
    return space;
}

#pragma mark -

OSDefineMetaClassAndStructors( IOFireWireController, IOFireWireBus )
OSMetaClassDefineReservedUnused(IOFireWireController, 0);
OSMetaClassDefineReservedUnused(IOFireWireController, 1);
OSMetaClassDefineReservedUnused(IOFireWireController, 2);
OSMetaClassDefineReservedUnused(IOFireWireController, 3);
OSMetaClassDefineReservedUnused(IOFireWireController, 4);
OSMetaClassDefineReservedUnused(IOFireWireController, 5);
OSMetaClassDefineReservedUnused(IOFireWireController, 6);
OSMetaClassDefineReservedUnused(IOFireWireController, 7);
OSMetaClassDefineReservedUnused(IOFireWireController, 8);

#pragma mark -

/////////////////////////////////////////////////////////////////////////////

// init
//
//

bool IOFireWireController::init( IOFireWireLink *fwim )
{
	bool success = true;
	
    if( !IOFireWireBus::init() )
    {
		success = false;
	}
	
	if( success )
	{
		fAuxiliary = createAuxiliary();
		if( fAuxiliary == NULL )
		{
			success = false;
		}
	}

	if( success )
	{
		
		fwim->retain() ;
		fFWIM = fwim;
		fHubPort = kFWInvalidPort;	// assume no hub
		
		fOutOfTLabels			= 0;
		fOutOfTLabels10S		= 0;
		fOutOfTLabelsThreshold	= 0;
		fWaitingForSelfID		= 0;
	}

	//
	// Create firewire symbols.
	//
	
	if( success )
	{		
		gFireWireROM = OSSymbol::withCString("FireWire Device ROM");
		if( gFireWireROM == NULL )
			success = false;
	}

	if( success )
	{			
		gFireWireNodeID = OSSymbol::withCString("FireWire Node ID");
		if( gFireWireNodeID == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireSelfIDs = OSSymbol::withCString("FireWire Self IDs");
		if( gFireWireSelfIDs == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireUnit_Spec_ID = OSSymbol::withCString("Unit_Spec_ID");
		if( gFireWireUnit_Spec_ID == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireUnit_SW_Version = OSSymbol::withCString("Unit_SW_Version");
		if( gFireWireUnit_SW_Version == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireVendor_ID = OSSymbol::withCString("Vendor_ID");
		if( gFireWireVendor_ID == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWire_GUID = OSSymbol::withCString("GUID");
		if( gFireWire_GUID == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireSpeed = OSSymbol::withCString("FireWire Speed");
		if( gFireWireSpeed == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireVendor_Name = OSSymbol::withCString("FireWire Vendor Name");
		if( gFireWireVendor_Name == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireProduct_Name = OSSymbol::withCString("FireWire Product Name");
		if( gFireWireProduct_Name == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireModel_ID = OSSymbol::withCString("Model_ID");
		if( gFireWireModel_ID == NULL )
			success = false;
	}

	if( success )
	{		
		gFireWireTDM = OSSymbol::withCString("TDM");
		if( gFireWireTDM == NULL )
			success = false;
	}

	if( success )
	{
		if( gIOFireWirePlane == NULL ) 
		{
			gIOFireWirePlane = IORegistryEntry::makePlane( kIOFireWirePlane );
		}
		
		if( gIOFireWirePlane == NULL )
			success = false;
	}
	
	//
	// create sets
	//
	
	if( success )
	{
		fLocalAddresses = OSSet::withCapacity( 5 );	// Local ROM + CSR registers + SBP-2 ORBs + FCP + PCR
		if( fLocalAddresses == NULL )
			success = false;
	}
	
	if( success )
	{	
		fSpaceIterator =  OSCollectionIterator::withCollection( fLocalAddresses );
		if( fSpaceIterator == NULL )
			success = false;
	}
	
	if( success )
	{	
		fPHYPacketListeners = OSSet::withCapacity( 2 );
		if( fPHYPacketListeners == NULL )
			success = false;
	}
	
	if( success )
	{	
		fPHYPacketListenersIterator =  OSCollectionIterator::withCollection( fPHYPacketListeners );
		if( fPHYPacketListenersIterator == NULL )
			success = false;
	}

	if( success )
	{	
		fLocalAsyncStreamReceivers = OSSet::withCapacity(5);
		if( fLocalAsyncStreamReceivers == NULL )
			success = false;
	}
	
	if( success )
	{	
		fAsyncStreamReceiverIterator = OSCollectionIterator::withCollection(fLocalAsyncStreamReceivers);
		if( fAsyncStreamReceiverIterator == NULL )
			success = false;
	}
		
	if( success )
	{	
		fAllocatedChannels = OSSet::withCapacity(1);	// DV channel.
		if( fAllocatedChannels == NULL )
			success = false;
	}

	if( success )
	{	
		fAllocChannelIterator =  OSCollectionIterator::withCollection(fAllocatedChannels);
		if( fAllocChannelIterator == NULL )
			success = false;
	}

	if( success )
	{	
		fIRMAllocations = OSSet::withCapacity(1);	
		if( fIRMAllocations == NULL )
			success = false;
	}

	if( success )
	{	
		fIRMAllocationsIterator =  OSCollectionIterator::withCollection(fIRMAllocations);
		if( fIRMAllocationsIterator == NULL )
			success = false;
	}
		
	if( success )
	{				
		fLastTrans = kMaxPendingTransfers-1;
		fDevicePruneDelay = kNormalDevicePruneDelay;

		UInt32 bad = OSSwapHostToBigInt32(0xdeadbabe);
		fBadReadResponse = IOBufferMemoryDescriptor::withBytes(&bad, sizeof(bad), kIODirectionOutIn);
		if( fBadReadResponse == NULL )
			success = false;
	}
	
	if( success )
	{				
		fDelayedStateChangeCmdNeedAbort = false;
		fDelayedStateChangeCmd = createDelayedCmd(1000 * kScanBusDelay, delayedStateChange, NULL);
		if( fDelayedStateChangeCmd == NULL )
			success = false;
	}

	if( success )
	{				
		fBusResetStateChangeCmd = createDelayedCmd(1000 * kRepeatResetDelay, resetStateChange, NULL);
		if( fBusResetStateChangeCmd == NULL )
			success = false;
	}
		
	if( success )
	{				
		fGUIDDups = IOFireWireDuplicateGUIDList::create();
		if( fGUIDDups == NULL )
			success = false;
	}

	//
	// create the bus power manager
	//
		
	if( success )
	{				
		fBusPowerManager = IOFireWirePowerManager::createWithController( this );
		if( fBusPowerManager == NULL )
		{
			IOLog( "IOFireWireController::start - failed to create bus power manager!\n" );
			success = false;
		}
	}
	
	if( success )
	{
		fNodeMustBeRootFlag		= false;
		fNodeMustNotBeRootFlag	= false;
		fForcedGapFlag			= false;
	}	
		
	return success;
}

// createAuxiliary
//
// virtual method for creating auxiliary object.  subclasses needing to subclass 
// the auxiliary object can override this.

IOFireWireBusAux * IOFireWireController::createAuxiliary( void )
{
	IOFireWireControllerAux * auxiliary;
    
	auxiliary = new IOFireWireControllerAux;

    if( auxiliary != NULL && !auxiliary->init(this) ) 
	{
        auxiliary->release();
        auxiliary = NULL;
    }
	
    return auxiliary;
}

// free
//
//

void IOFireWireController::free()
{
	closeGate() ;
	
	fInstantiated = false;

    // Release everything I can think of.
	fNodeMustBeRootFlag		= false;
	fNodeMustNotBeRootFlag	= false;
	fForcedGapFlag			= false;
	
	if( fIRM != NULL )
	{
		fIRM->release();
		fIRM = NULL;
	}
	
	if( fBusPowerManager != NULL )
	{
		fBusPowerManager->release();
		fBusPowerManager = NULL;
	}
	
    if( fROMAddrSpace != NULL ) 
	{
        fROMAddrSpace->release();
		fROMAddrSpace = NULL;
	}

    if( fRootDir != NULL )
    {
	    fRootDir->release();
		fRootDir = NULL;
	}
	    
    if( fBadReadResponse != NULL )
    {
	    fBadReadResponse->release();
		fBadReadResponse = NULL;
	}
	    
    if( fDelayedStateChangeCmd != NULL )
	{
        fDelayedStateChangeCmd->release();
		fDelayedStateChangeCmd = NULL;
	}
	
    if( fBusResetStateChangeCmd != NULL )
	{
        fBusResetStateChangeCmd->release();
		fBusResetStateChangeCmd = NULL;
	}
	
    if( fSpaceIterator != NULL ) 
	{
        fSpaceIterator->release();
		fSpaceIterator = NULL;
    }
        
    if( fLocalAddresses != NULL )
	{
        fLocalAddresses->release();
		fLocalAddresses = NULL;
	}

	if( fPHYPacketListenersIterator != NULL ) 
	{
        fPHYPacketListenersIterator->release();
		fPHYPacketListenersIterator = NULL;
    }
        
    if( fPHYPacketListeners != NULL )
	{
        fPHYPacketListeners->release();
		fPHYPacketListeners = NULL;
	}

	
	if( fAsyncStreamReceiverIterator != NULL)
	{
		fAsyncStreamReceiverIterator->release();
		fAsyncStreamReceiverIterator = NULL;
	}

	if( fLocalAsyncStreamReceivers != NULL )
	{
		fLocalAsyncStreamReceivers->release();
		fLocalAsyncStreamReceivers = NULL;
	}
		
    if( fAllocChannelIterator != NULL ) 
	{
        fAllocChannelIterator->release();
		fAllocChannelIterator = NULL;
	}

    if( fAllocatedChannels != NULL )
	{
        fAllocatedChannels->release();
		fAllocatedChannels = NULL;
    }

	if( fIRMAllocationsIterator != NULL ) 
	{
        fIRMAllocationsIterator->release();
		fIRMAllocationsIterator = NULL;
	}
	
    if( fIRMAllocations != NULL )
	{
        fIRMAllocations->release();
		fIRMAllocations = NULL;
    }
	
	{
		IOFireWireLink * fwim = fFWIM ;
		fFWIM = NULL ;
		fwim->release() ;
	}
	
	openGate() ;
	
	destroyTimeoutQ();
	destroyPendingQ();
	
	if( fWorkLoop != NULL )
	{
		fWorkLoop->release();
		fWorkLoop = NULL;
	}
	
	if( fAuxiliary != NULL )
	{
		fAuxiliary->release();
		fAuxiliary = NULL;
	}
	
	if( fGUIDDups != NULL )
	{
		fGUIDDups->release();
		fGUIDDups = NULL;
	}
	
	
	
    IOFireWireBus::free();
}

// start
//
//

bool IOFireWireController::start( IOService * provider )
{
	fStarted = false;

    if (!IOService::start(provider))
    {
	    return false;
    }
	
#ifndef __i386__ // x86 device tree is different
    // blow away device tree children from where we've taken over
    // Note we don't add ourself to the device tree.
    IOService *parent = this;
    while(parent) {
        if(parent->inPlane(gIODTPlane))
            break;
        parent = parent->getProvider();
    }
    if(parent) {
        IORegistryEntry *    child;
        IORegistryIterator * children;

        children = IORegistryIterator::iterateOver(parent, gIODTPlane);
        if ( children != 0 ) {
            // Get all children before we start altering the plane!
            OSOrderedSet * set = children->iterateAll();
            if(set != 0) {
                OSIterator *iter = OSCollectionIterator::withCollection(set);
                if(iter != 0) {
                    while ( (child = (IORegistryEntry *)iter->getNextObject()) ) {
                        child->detachAll(gIODTPlane);
                    }
                    iter->release();
                }
                set->release();
            }
            children->release();
        }
    }
#endif /* !__i386__ */

    fWorkLoop = fFWIM->getFireWireWorkLoop();
    fWorkLoop->retain();	// make sure workloop lives at least as long as we do.

	// workloop must be set up before creating queues
	// pending queue creates the command gate used by setPowerState()
	createPendingQ();
	createTimeoutQ();

	// process boot-args - we may want to make this a seperate function...
	if ( !PE_parse_boot_arg("fwdebug_ignorenode", &fDebugIgnoreNode) ) {
		// fDebugIgnoreNode can be in 0xFFCx or regular form
		fDebugIgnoreNode = kFWDebugIgnoreNodeNone;
	} else {
		// IOLog might not make it to sys.log, but seen in verbose mode
		IOLog("FireWire: Will ignore node 0x%lx per boot-args.\n", fDebugIgnoreNode);
	}
	// we should define "fwdebug" for debug purposes...

	fInstantiated = true;
	fStartStatus = kIOReturnSuccess;

    // register ourselves with superclass policy-maker
    PMinit();
    provider->joinPMtree(this);
    registerPowerDriver(this, ourPowerStates, number_of_power_states);
	
    // No idle sleep
    changePowerStateTo( kFWPMWakeState );

	// poweredStart will be called once the wake transition occurs
	
    return (fStartStatus == kIOReturnSuccess);
}

// poweredStart
//
//

IOReturn IOFireWireController::poweredStart( void )
{	
	IOReturn status = kIOReturnSuccess;
	
	fStarted = true;

    CSRNodeUniqueID guid = fFWIM->getGUID();
    
	IOService * provider = getProvider();
    if( provider->getProperty("DelegateCycleMaster") )
    	fDelegateCycleMaster = true;

	// look up the hub property
	OSData * hub_port_property = (OSData*)fFWIM->getProvider()->getProperty( "fwhub" );
	if( hub_port_property )
	{
		fHubPort = *((UInt32 *)hub_port_property->getBytesNoCopy());
	}
	
//	IOLog( "IOFireWireController::poweredStart = hub_port_property = 0x%08lx fHubPort = %d\n", hub_port_property, fHubPort );
	
	//
	// setup initial security mode and state change notification
	//
	
	initSecurity();
	
    // Build local device ROM
    // Allocate address space for Configuration ROM and fill in Bus Info
    // block.
    fROMHeader[1] = OSSwapHostToBigInt32( kFWBIBBusName );
	
	UInt32 characteristics =  fFWIM->getBusCharacteristics();	
	
	//IOLog( "IOFireWireController:start - characteristics = 0x%08lx\n", characteristics );
	
	// zero out generation and store in local rom header
    fROMHeader[2] = OSSwapHostToBigInt32( characteristics & ~kFWBIBGeneration );

    // Get max speed
	((IOFireWireControllerAux*)fAuxiliary)->fMaxRec = ((characteristics & kFWBIBMaxRec) >> kFWBIBMaxRecPhase);
    fMaxRecvLog = ((characteristics & kFWBIBMaxRec) >> kFWBIBMaxRecPhase)+1;
    fMaxSendLog = fFWIM->getMaxSendLog();
    fROMHeader[3] = OSSwapHostToBigInt32( guid >> 32 );
    fROMHeader[4] = OSSwapHostToBigInt32( guid & 0xffffffff );

    // Create root directory in FWIM data.//zzz should we have one for each FWIM or just one???
    fRootDir = IOLocalConfigDirectory::create();
    if(!fRootDir)
	{
        return kIOReturnError;
	}
	
    // Set our Config ROM generation.
    fRootDir->addEntry(kConfigGenerationKey, (UInt32)1);
    // Set our module vendor ID.
    fRootDir->addEntry(kConfigModuleVendorIdKey, kConfigUnitSpecAppleA27, OSString::withCString("Apple Computer, Inc."));
    fRootDir->addEntry(kConfigModelIdKey, kConfigUnitSWVersMacintosh10, OSString::withCString("Macintosh"));
    // Set our capabilities.
    fRootDir->addEntry(kConfigNodeCapabilitiesKey, 0x000083C0);

    // Set our node unique ID.
	UInt64 guid_big = OSSwapHostToBigInt64( guid );
    OSData *t = OSData::withBytes(&guid_big, sizeof(guid_big));
    fRootDir->addEntry(kConfigNodeUniqueIdKey, t);
        
    fTimer->enable();

    // Create local node
    IOFireWireLocalNode *localNode = new IOFireWireLocalNode;
    
	localNode->setConfigDirectory( fRootDir );
	
    OSDictionary *propTable;
    do {
        OSObject * prop;
        propTable = OSDictionary::withCapacity(8);
        if (!propTable)
            continue;

        prop = OSNumber::withNumber(guid, 8*sizeof(guid));
        if(prop) {
            propTable->setObject(gFireWire_GUID, prop);
            prop->release();
        }
        
        localNode->init(propTable);
        localNode->attach(this);
        localNode->registerService();
    } while (false);
    if(propTable)
        propTable->release();	// done with it after init

#ifdef LEGACY_SHUTDOWN
	// install power change handler
	fPowerEventNotifier = registerPrioritySleepWakeInterest( systemShutDownHandler, this );
	FWKLOGASSERT( fPowerEventNotifier != NULL );
#endif

	fIRM = IOFireWireIRM::create(this);
	FWPANICASSERT( fIRM != NULL );
    
	fWorkLoop->enableAllInterrupts();	// Enable the interrupt delivery.
	
    registerService();			// Enable matching with this object

	return status;
}

// stop
//
//

void IOFireWireController::stop( IOService * provider )
{
	closeGate() ;
	
    // Fake up disappearance of entire bus
    processBusReset();
	suspendBus();
    
	// tear down security state change notification
	freeSecurity();
		    
    PMstop();

#ifdef LEGACY_SHUTDOWN
   if( fPowerEventNotifier ) 
	{
        fPowerEventNotifier->remove();
        fPowerEventNotifier = NULL;
    }
#endif

    if(fBusState == kAsleep) {
        IOReturn sleepRes;
        sleepRes = fWorkLoop->wake(&fBusState);
        if(sleepRes != kIOReturnSuccess) {
            IOLog("IOFireWireController::stop - Can't wake FireWire workloop, error 0x%x\n", sleepRes);
        }
        
		openGate();
    }
	
	freeAllAsyncStreamReceiver();
	
    IOService::stop(provider);
	
	openGate() ;
}

// finalize
//
//

bool IOFireWireController::finalize( IOOptionBits options )
{
    bool res;
	
	closeGate() ;
	
    res = IOService::finalize(options);

	openGate() ;
	
    return res;
}

// requestTerminate
//
// send our custom kIOFWMessageServiceIsRequestingClose to clients

bool IOFireWireController::requestTerminate( IOService * provider, IOOptionBits options )
{
    OSIterator *childIterator;

    childIterator = getClientIterator();
    if( childIterator ) 
	{
        OSObject *child;
        while( (child = childIterator->getNextObject()) ) 
		{
            IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
			
			// don't need to sync with open/close routines when checking for kNotTerminated
			if(found && (found->getTerminationState() == kNotTerminated) && found->isOpen()) 
			{
                // send our custom requesting close message
                messageClient( kIOFWMessageServiceIsRequestingClose, found );
            }
        }
		
        childIterator->release();
    }

    // delete local node
    IOFireWireLocalNode *localNode = getLocalNode(this);
    if(localNode) {
        localNode->release();
    }

    return IOService::requestTerminate(provider, options);
}

// setPowerState
//
//

IOReturn IOFireWireController::setPowerState( unsigned long powerStateOrdinal,
                                                IOService* whatDevice )
{
    IOReturn res;
    IOReturn sleepRes;

	if( !fStarted && (powerStateOrdinal != kFWPMWakeState) )
	{
		// if we're not started yet wait for a wake state
		return IOPMAckImplied;
	}
	
	if( !fInstantiated )
	{
		// we're being torn down, bail
		return IOPMAckImplied;
	}
		
    // use gate to keep other threads off the hardware,
    // Either close gate or wake workloop.
    // First time through, we aren't really asleep.
    if( fBusState != kAsleep ) 
	{
        closeGate();
    }
    else 
	{
        sleepRes = fWorkLoop->wake(&fBusState);
        if(sleepRes != kIOReturnSuccess) 
		{
            IOLog("Can't wake FireWire workloop, error 0x%x\n", sleepRes);
        }
    }
    // Either way, we have the gate closed against invaders/lost sheep

	// <rdar://problem/4435647> Yellow: iChat freezes after hot unplug a FireWire iSight during system asleep.
	// Change condition from 'powerStateOrdinal != kFWPMSleepState' to 'powerStateOrdinal == kFWPMWakeState'
	// -ng
	
	if( powerStateOrdinal == kFWPMWakeState )
    {
		fDevicePruneDelay = kWakeDevicePruneDelay;
        fBusState = kRunning;	// Will transition to a bus reset state.
        if( fDelayedStateChangeCmdNeedAbort )
        {
            fDelayedStateChangeCmdNeedAbort = false;
            fDelayedStateChangeCmd->cancel(kIOReturnAborted);
        }
    }
    
    // Reset bus if we're sleeping, before turning hw off.
    if(powerStateOrdinal == kFWPMSleepState )
    {
        fFWIM->setContender(false); 
		fFWIM->setRootHoldOff(false); 
        fFWIM->resetBus();
 		IOSleep(10);		// Reset bus may not be instantaneous anymore. Wait a bit to be sure
 							// it happened before turning off hardware.
 							// zzz how long to wait?
 	}  	
        
    res = fFWIM->setLinkPowerState(powerStateOrdinal);

	if( (powerStateOrdinal == kFWPMWakeState) && !fStarted )
	{
		// fStartStatus will be returned from IOFireWireController::start
		if( res != IOPMAckImplied )
		{
			fStartStatus = kIOReturnError;
			res = IOPMAckImplied;
		}
		else
		{
			fStartStatus = poweredStart();
		}
	}
	
	// reset bus 
	if( (powerStateOrdinal == kFWPMWakeState) && 
		(res == IOPMAckImplied) && 
		(fStartStatus == kIOReturnSuccess) )
	{
		if ( kIOReturnSuccess != UpdateROM() )
			IOLog(" %s %u: UpdateROM() got error\n", __FILE__, __LINE__ ) ;
	
		fFWIM->resetBus();	// Don't do this on startup until Config ROM built.
	}
	
	// Update power state, keep gate closed while we sleep.
	if( powerStateOrdinal == kFWPMSleepState ) 
	{
		// Pretend we had a bus reset - we'll have a real one when we wake up.
		if( delayedStateCommandInUse() )
		{
			fDelayedStateChangeCmdNeedAbort = true;
		}
		
		if( fBusResetState == kResetStateDisabled )
		{
			// we'll cause a reset when we wake up and reset the state machine anyway
			fBusResetStateChangeCmd->cancel( kIOReturnAborted );
			fBusResetState = kResetStateResetting;
		}
		
		fBusState = kAsleep;
	}
	
	if( (fBusState == kAsleep) && (OSSwapBigToHostInt32(fROMHeader[1]) == kFWBIBBusName) ) 
	{
		sleepRes = fWorkLoop->sleep(&fBusState);
        if(sleepRes != kIOReturnSuccess) 
		{
            IOLog("Can't sleep FireWire workloop, error 0x%x\n", sleepRes);
        }
    }
    else 
	{
        openGate();
    }

    return res;
}

#ifdef LEGACY_SHUTDOWN

// systemShutDownHandler
//
//

IOReturn IOFireWireController::systemShutDownHandler( void * target, void * refCon,
                                    UInt32 messageType, IOService * service,
                                    void * messageArgument, vm_size_t argSize )
{
	
    IOReturn status = kIOReturnSuccess;

	IOFireWireController * me = (IOFireWireController*)target;
	
	me->closeGate();

    switch( messageType ) 
	{
        case kIOMessageSystemWillPowerOff:
//			IOLog( "IOFireWireController::systemShutDownHandler - kIOMessageSystemWillPowerOff\n" );
			
			me->fFWIM->handleSystemShutDown( messageType );
			status = kIOReturnSuccess;
			break;
			
        case kIOMessageSystemWillRestart:
//			IOLog( "IOFireWireController::systemShutDownHandler - kIOMessageSystemWillRestart\n" );
			
			me->fFWIM->handleSystemShutDown( messageType );
			status = kIOReturnSuccess;
            break;

        default:
            status = kIOReturnUnsupported;
            break;
    }

	me->openGate();

	 // 30 second delay for debugging
	 // this will allow you to see IOLogs at shutdown when verbose booted
	 
//	IOSleep( 30000 ); 

    return status;
}

#else

// systemWillShutdown
//
//

void IOFireWireController::systemWillShutdown( IOOptionBits specifier )
{
	closeGate();

    switch( specifier ) 
	{
        case kIOMessageSystemWillPowerOff:
//			IOLog( "IOFireWireController::systemWillShutdown - kIOMessageSystemWillPowerOff\n" );
			
			fFWIM->handleSystemShutDown( specifier );
			break;
			
        case kIOMessageSystemWillRestart:
//			IOLog( "IOFireWireController::systemWillShutdown - kIOMessageSystemWillRestart\n" );
			
			fFWIM->handleSystemShutDown( specifier );
            break;

        default:
            break;
    }

	openGate();

	 // 30 second delay for debugging
	 // this will allow you to see IOLogs at shutdown when verbose booted
	 
//	IOSleep( 30000 ); 
	
	IOService::systemWillShutdown( specifier );
}

#endif

// resetBus
//
//

IOReturn IOFireWireController::resetBus()
{
    IOReturn res = kIOReturnSuccess;

	closeGate();
	
	switch( fBusResetState )
	{
		case kResetStateDisabled:
			// always schedule resets during the first 2 seconds
			fBusResetScheduled = true;
			break;
			
		case kResetStateArbitrated:
			if( fBusResetDisabledCount == 0 )
			{
				// cause a reset if no one has disabled resets
				doBusReset();
			}
			else if( !fBusResetScheduled )
			{
				// schedule the reset if resets are disabled
				fBusResetScheduled = true;
				
				//
				// start bus reset timer
				//
				
				if( delayedStateCommandInUse() )
				{
					fDelayedStateChangeCmd->cancel( kIOReturnAborted );
				}
				
				fBusState = kWaitingBusResetStart;
				fDelayedStateChangeCmd->reinit( 1000 * kBusResetStartTimeout, delayedStateChange, NULL );
				fDelayedStateChangeCmd->submit();	
			}
			break;
		
		case kResetStateResetting:
			// we're in the middle of a reset now, no sense in doing another
			// fBusResetScheduled would be cleared on the transition out of this state anyway
		default:
			break;
	}

	openGate();
	
    return res;
}


// resetStateChange
//
// called 2 seconds after a bus reset to transition from the disabled state
// to the arbitrated state

void IOFireWireController::resetStateChange(void *refcon, IOReturn status,
								IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    IOFireWireController *me = (IOFireWireController *)bus;
	
    if( status == kIOReturnTimeout ) 
	{
		// we should only transition here from the disabled state
#ifdef FWKLOGASSERT
		FWKLOGASSERT( me->fBusResetState == kResetStateDisabled );
#endif
		// transition to the arbitrated reset state
		me->fBusResetState = kResetStateArbitrated;
		
		// reset now if we need to and no one has disabled resets
		if( me->fBusResetScheduled )
		{
			if( me->fBusResetDisabledCount == 0 )
			{
				me->doBusReset();
			}
			else
			{
				//
				// start bus reset timer
				//
				
				if( me->delayedStateCommandInUse() )
				{
					me->fDelayedStateChangeCmd->cancel( kIOReturnAborted );
				}
				
				me->fBusState = kWaitingBusResetStart;
				me->fDelayedStateChangeCmd->reinit( 1000 * kBusResetStartTimeout, delayedStateChange, NULL );
				me->fDelayedStateChangeCmd->submit();	
			}
		}
	}
}

// doBusReset
//
//

void IOFireWireController::doBusReset( void )
{
	IOReturn 	status = kIOReturnSuccess;
	bool		useIBR = false;
	
	if( fDelayedPhyPacket )
	{
		fFWIM->sendPHYPacket( fDelayedPhyPacket );
		fDelayedPhyPacket = 0x00000000;

		// If the gap counts were mismatched and we're optimizing the
		// gap count, use in IBR instead of an ISBR
		
		// This works around a problem with certain devices when they
		// see an ISBR followed by an IBR due to ISBR promotion.
		
		// When this occurs the device is supposed to throw out the IBR.
		// Devices that don't do this latch the gap count with the ISBR
		// and then reset it with the subsequent IBR.
		
		// This gets us into a loop where we keep trying to optimize the
		// gap count only to find it reset on the next bus generation
		
		// Using an IBR avoids the problem.
		
		if( fGapCountMismatch )
		{
			useIBR = true;
		}
	}

	FWKLOG(( "IOFireWireController::doBusReset\n" ));

	fBusResetState = kResetStateResetting;
	
	//
	// start bus reset timer
	//
	
	if( delayedStateCommandInUse() )
	{
		fDelayedStateChangeCmd->cancel(kIOReturnAborted);
	}
	
	fBusState = kWaitingBusReset;
	fDelayedStateChangeCmd->reinit(1000 * kBusResetTimeout, delayedStateChange, NULL);
    fDelayedStateChangeCmd->submit();
	
	status = fFWIM->resetBus( useIBR );

	if( fWaitingForSelfID > kMaxWaitForValidSelfID )
	{
		fFWIM->notifyInvalidSelfIDs();
		fWaitingForSelfID = 0;
	}
}

// enterBusResetDisabledState
//
//

void IOFireWireController::enterBusResetDisabledState( )
{	
	if( fBusResetState == kResetStateDisabled )
		fBusResetStateChangeCmd->cancel( kIOReturnAborted );
	
	// start the reset disabled state
	fBusResetState = kResetStateDisabled;
	fBusResetStateChangeCmd->reinit( 1000 * kRepeatResetDelay, resetStateChange, NULL );
    fBusResetStateChangeCmd->submit();
}

// disableSoftwareBusResets
//
//

IOReturn IOFireWireController::disableSoftwareBusResets( void )
{
	IOReturn status = kIOReturnSuccess;
	
	closeGate();

	switch( fBusResetState )
	{
		case kResetStateDisabled:
			// bus resets have no priority in this state
			// we will always allow this call to succeed in these states
			fBusResetDisabledCount++;
			break;
			
		case kResetStateResetting:
			// we're in the process of bus resetting, but processBusReset has
			// not yet been called
			
			// if we allowed this call to succeed the client would receive a
			// bus reset notification after the disable call which is
			// probably not what the caller expects
			status = kIOFireWireBusReset;
			break;
			
		case kResetStateArbitrated:
			// bus resets have an equal priority in this state
			// this call will fail if we need to cause a reset in this state
			if( !fBusResetScheduled )
			{
				fBusResetDisabledCount++;
			}
			else
			{
				// we're trying to get a bus reset out, we're not allowing
				// any more disable calls
				status = kIOFireWireBusReset;
			}
			break;
		
		default:
			break;
	}
	
//	IOLog( "IOFireWireController::disableSoftwareBusResets = 0x%08lx\n", (UInt32)status );
			
	openGate();
	
	return status;
}

// enableSoftwareBusResets
//
//

void IOFireWireController::enableSoftwareBusResets( void )
{
	closeGate();

#ifdef FWKLOGASSERT
	FWKLOGASSERT( fBusResetDisabledCount != 0 );
#endif

	// do this in any state
	if( fBusResetDisabledCount > 0 )
	{
		fBusResetDisabledCount--;
//		IOLog( "IOFireWireController::enableSoftwareBusResets\n" );
	}

	switch( fBusResetState )
	{
		case kResetStateArbitrated:
			// this is the only state we're allowed to cause resets in
			if( fBusResetDisabledCount == 0 && fBusResetScheduled )
			{
				// reset the bus if the disabled count is down 
				// to zero and a bus reset is scheduled
				doBusReset();
			}
			break;
			
		case kResetStateResetting:
		case kResetStateDisabled:		
		default:
			break;
	}
	
	openGate();
}

// beginIOCriticalSection
//
//

IOReturn IOFireWireController::beginIOCriticalSection( void )
{
	IOReturn status = kIOReturnSuccess;
	
	// disable software initiated bus resets
	status = disableSoftwareBusResets();	
	
	// check success of bus reset disabling
	// critical section can be denied if someone has already requested a bus reset
	
	if( status == kIOReturnSuccess )
	{
		if( fIOCriticalSectionCount == 0 )
		{
			fFWIM->configureAsyncRobustness( true );
		}

		fIOCriticalSectionCount++;
	}
	
	return status;
}

// endIOCriticalSection
//
//

void IOFireWireController::endIOCriticalSection( void )
{
	if( fIOCriticalSectionCount != 0 )
	{
		enableSoftwareBusResets();
		
		fIOCriticalSectionCount--;
		
		if( fIOCriticalSectionCount == 0 )
		{
			fFWIM->configureAsyncRobustness( false );
		}
	}
}

// delayedStateChange
//
//

void IOFireWireController::delayedStateChange(void *refcon, IOReturn status,
                                IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    IOFireWireController *me = (IOFireWireController *)bus;

    if(status == kIOReturnTimeout) {
        switch (me->fBusState) {
		case kWaitingBusResetStart:
			// timed out waiting for a bus reset to be alllowed
//			IOLog( "IOFireWireController::delayedStateChange - timed out waiting for a bus reset to be allowed - resetting bus\n" );
			me->doBusReset();
			break;
		case kWaitingBusReset:
			// timed out waiting for a bus reset
//			IOLog( "IOFireWireController::delayedStateChange - timed out waiting for a bus reset - resetting bus\n" );
			me->checkProgress();
			me->enterBusResetDisabledState();
			me->fBusState = kRunning;
			me->resetBus();
			break;
		case kWaitingSelfIDs:
			// timed out waiting for self ids
//			IOLog( "IOFireWireController::delayedStateChange - timed out waiting for self ids - resetting bus\n" );
			me->fBusState = kRunning;
			me->fWaitingForSelfID++;
			me->resetBus();
			break;
		case kWaitingScan:
			me->fWaitingForSelfID = 0;
            me->fBusState = kScanning;
            me->startBusScan();
            break;
        case kWaitingPrune:
            me->fBusState = kRunning;
            me->updatePlane();
            break;
        default:
            IOLog("State change timeout, state is %d\n", me->fBusState);
            break;
        }        
    }
}

// scanningBus
//
// Are we currently scanning the bus?

bool IOFireWireController::scanningBus() const
{
	return fBusState == kWaitingSelfIDs || fBusState == kWaitingScan || fBusState == kScanning;
}

// delayedStateCommandInUse
//
//

bool IOFireWireController::delayedStateCommandInUse() const
{
	return (fBusState == kWaitingBusReset) || (fBusState == kWaitingSelfIDs) || (fBusState == kWaitingScan) || (fBusState == kWaitingPrune);
}

// getResetTime
//
//

const AbsoluteTime * IOFireWireController::getResetTime() const 
{
	return &fResetTime;
}

// checkProgress
//
//

void IOFireWireController::checkProgress( void )
{
	// check progress on reset queue
	fAfterResetHandledQ.checkProgress();

	// check progress on disconnected devices
	OSIterator * childIterator = getClientIterator();
    if( childIterator ) 
	{
        OSObject *child;
        while( (child = childIterator->getNextObject())) 
		{
            IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
			
			if( found && (found->getTerminationState() == kNotTerminated) && found->fNodeID == kFWBadNodeID )  
			{
				AbsoluteTime now;
				UInt32 milliDelta;
				UInt64 nanoDelta;
				
				AbsoluteTime resume_time = found->getResumeTime();
				clock_get_uptime( &now );
				SUB_ABSOLUTETIME( &now, &resume_time );
				absolutetime_to_nanoseconds( now, &nanoDelta );
				milliDelta = nanoDelta / 1000000;
				
				if( milliDelta > kDeviceMaximuPruneTime )
				{
					// it's been too long, kill him
					terminateDevice( found );
            	}
			}
			
        }
        childIterator->release();
    }

}

// processBusReset
//
// Hardware detected a bus reset.
// At this point we don't know what the hardware addresses are

void IOFireWireController::processBusReset( void )
{
	FWKLOG(( "IOFireWireController::suspendBus\n" ));

	clock_get_uptime(&fResetTime);	// Update even if we're already processing a reset

	// we got our bus reset, cancel any reset work in progress
	fBusResetScheduled = false;

	enterBusResetDisabledState();
		
	if( delayedStateCommandInUse() )
	{
		fDelayedStateChangeCmd->cancel( kIOReturnAborted );
	}
	
	fBusState = kWaitingSelfIDs;
	
	checkProgress();
	
	// set a timer
	fDelayedStateChangeCmd->reinit( 1000 * kSelfIDTimeout, delayedStateChange, NULL );
    fDelayedStateChangeCmd->submit();
}

// suspendBus
//
//

void IOFireWireController::suspendBus( void )
{
	FWKLOG(( "IOFireWireController::suspendBus\n" ));
	
	// set generation property to kFWBusScanInProgress ("-1")
	FWKLOG(("IOFireWireController::suspendBus set generation to '%s' realGen=0x%lx\n", kFWBusScanInProgress, fBusGeneration));
	setProperty( kFireWireGenerationID, kFWBusScanInProgress );
		
	fRequestedHalfSizePackets = false;
	fNodeMustNotBeRootFlag	  = false;
	fNodeMustBeRootFlag		  = false;
	fForcedGapFlag			  = false;
	
	unsigned int i;
	UInt32 oldgen = fBusGeneration;

	// Set all current device nodeIDs to something invalid
	fBusGeneration++;
	
	// Reset all these information only variables
	fOutOfTLabels			= 0;
	fOutOfTLabels10S		= 0;
	fOutOfTLabelsThreshold	= 0;

	OSIterator * childIterator = getClientIterator();
	if( childIterator ) 
	{
		OSObject * child;
		while( (child = childIterator->getNextObject()) ) 
		{
			IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
			
			// don't need to sync with open/close routines when checking for kNotTerminated
			if( found && (found->getTerminationState() == kNotTerminated) )
			{
				found->setNodeROM( oldgen, kFWBadNodeID, NULL );
			}
			else if( OSDynamicCast(IOFireWireLocalNode, child) ) 
			{
				// forceStop  should take care of this deactivateAsyncStreamReceivers();
				((IOFireWireLocalNode *)child)->messageClients(kIOMessageServiceIsSuspended);
			}
		}

		childIterator->release();
	}

	// reset physical filters if necessary
	physicalAccessProcessBusReset();

	// IOLog("FireWire Bus Generation now %d\n", fBusGeneration);
	
	// Invalidate current topology and speed map
	bzero( fSpeedVector, sizeof(fSpeedVector) );
	
	// Zap all outstanding async requests
	for( i=0; i<kMaxPendingTransfers; i++ ) 
	{
		AsyncPendingTrans *t = &fTrans[i];
		if( t->fHandler ) 
		{
			IOFWAsyncCommand * cmd = t->fHandler;
			cmd->gotPacket(kFWResponseBusResetError, NULL, 0);
		}
		else if( t->fAltHandler )
		{
			IOFWAsyncPHYCommand * cmd = OSDynamicCast( IOFWAsyncPHYCommand, t->fAltHandler );
			if( cmd )
			{
				cmd->gotPacket( kFWResponseBusResetError );
			}
		}
	}

	// Clear out the old firewire plane
	if( fNodes[fRootNodeID] ) 
	{
		fNodes[fRootNodeID]->detachAll(gIOFireWirePlane);
	}
	
	for( i=0; i<=fRootNodeID; i++ ) 
	{
		if( fScans[i] )
		{
			fScans[i]->fCmd->release();
			fScans[i]->fLockCmd->release();
			IOFree(fScans[i], sizeof(*fScans[i]));
			fScans[i] = NULL;
		}
		
		if(fNodes[i]) 
		{
			fNodes[i]->release();
			fNodes[i] = NULL;
		}
	}
	
	// Cancel all commands in timeout queue that want to complete on bus reset
	fTimeoutQ.busReset();
}

// processSelfIDs
//
//

void IOFireWireController::processSelfIDs(UInt32 *IDs, int numIDs, UInt32 *ownIDs, int numOwnIDs)
{
    int i;
    UInt32 id;
    UInt32 nodeID;
    UInt16 irmID;
    UInt16 ourID;
    IOFireWireLocalNode * localNode;

	FWKLOG(( "IOFireWireController::processSelfIDs entered\n" ));

#if (DEBUGGING_LEVEL > 0)
for(i=0; i<numIDs; i++)
    IOLog("ID %d: 0x%x <-> 0x%x\n", i, IDs[2*i], ~IDs[2*i+1]);

for(i=0; i<numOwnIDs; i++)
    IOLog("Own ID %d: 0x%x <-> 0x%x\n", i, ownIDs[2*i], ~ownIDs[2*i+1]);
#endif

	if( fBusState != kWaitingSelfIDs )
	{
		// If not processing a reset, then we should be
		// This can happen if we get two resets in quick successio
        processBusReset();
    }
	
	// we should now be in the kWaitingSelfIDs state

	suspendBus();
	
	if( fBusState != kWaitingSelfIDs )
	{
		IOLog( "IOFireWireController::processSelfIDs - fBusState != kWaitingSelfIDs\n" );
	}
	
	if( fBusState == kWaitingSelfIDs )
	{
		// cancel self id timeout
		fDelayedStateChangeCmd->cancel( kIOReturnAborted );
	}

	fBusState = kWaitingScan;
                      
    // Initialize root node to be our node, we'll update it below to be the highest node ID.
    fRootNodeID = ourID = (OSSwapBigToHostInt32(*ownIDs) & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;
    fLocalNodeID = ourID | (kFWLocalBusAddress>>kCSRNodeIDPhase);

	fGapCountMismatch = false;
	
    // check for mismatched gap counts
    UInt32 gap_count = (OSSwapBigToHostInt32(*ownIDs) & kFWSelfID0GapCnt) >> kFWSelfID0GapCntPhase;
    for( i = 0; i < numIDs; i++ )
    {
        UInt32 current_id = OSSwapBigToHostInt32(IDs[2*i]);
        if( (current_id & kFWSelfIDPacketType) == 0 &&
            ((current_id & kFWSelfID0GapCnt) >> kFWSelfID0GapCntPhase) != gap_count )
        {
            // set the gap counts to 0x3F, if any gap counts are mismatched
            fFWIM->sendPHYPacket( (kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                                  (0x3f << kFWPhyConfigurationGapCntPhase) | kFWPhyConfigurationT );
			
			fGapCountMismatch = true;
			FWKLOG(( "IOFireWireController::processSelfIDs Found Gap Count Mismatch!\n" ));
            break;
        }
    }

    // Update the registry entry for our local nodeID,
    // which will have been updated by the device driver.
    // Find the local node (just avoiding adding a member variable)
    localNode = getLocalNode(this);
    if(localNode)
	{
        localNode->setNodeProperties(fBusGeneration, fLocalNodeID, ownIDs, numOwnIDs,fFWIM->getPhySpeed() );
        fNodes[ourID] = localNode;
        localNode->retain();
    }
    
    // Copy over the selfIDs, checking validity and merging in our selfIDs if they aren't
    // already in the list.
    SInt16 prevID = -1;	// Impossible ID.
    UInt32 *idPtr = fSelfIDs;

	// zero out fNodeIDs so any gaps in the received self IDs will be
	// apparent later...
	bzero( fNodeIDs, sizeof(fNodeIDs) ) ;

	fNumSelfIDs = numIDs;
    for(i=0; i<numIDs; i++)
	{
        UInt32 id = OSSwapBigToHostInt32(IDs[2*i]);
        UInt16 currID = (id & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;

 		UInt32 id_inverse = ~OSSwapBigToHostInt32( IDs[2*i+1] );
        if(id != id_inverse)
		{
            IOLog("Bad SelfID packet %d: 0x%lx != 0x%lx!\n", i, id, id_inverse);
            resetBus();	// Could wait a bit in case somebody else spots the bad packet
			FWKLOG(( "IOFireWireController::processSelfIDs exited\n" ));
            return;
        }

        if(currID != prevID)
		{
            // Check for ownids not in main list
            if( prevID < ourID && currID > ourID )
			{
				int j;
				fNodeIDs[ourID] = idPtr;
				for(j=0; j<numOwnIDs; j++)
				{
					*idPtr++ = ownIDs[2*j];
				}
				fNumSelfIDs += numOwnIDs;
			}
			fNodeIDs[currID] = idPtr;
			prevID = currID;
			if(fRootNodeID < currID)
				fRootNodeID = currID;
        }
		*idPtr++ = IDs[2*i];
    }
	
    // Check for ownids at end & not in main list
    if(prevID < ourID)
	{
        int j;
        fNodeIDs[ourID] = idPtr;
        for(j=0; j<numOwnIDs; j++)
		{
            *idPtr++ = ownIDs[2*j];
		}
		fNumSelfIDs += numOwnIDs;
	}
    // Stick a known elephant at the end.
    fNodeIDs[fRootNodeID+1] = idPtr;

    // Check nodeIDs are monotonically increasing from 0.
    for(i = 0; i<=fRootNodeID; i++)
	{
        if ( NULL == fNodeIDs[i] )
		{
			IOLog("Missing self ID for node %d!\n", i ) ;
			resetBus();        	// Could wait a bit in case somebody else spots the bad packet

			return;				// done.
		}

		UInt32 host_id = OSSwapBigToHostInt32(*fNodeIDs[i]);
		if( ((host_id & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase) != (UInt32)i)
		{
			IOLog("No FireWire node %d (got ID packet 0x%lx)!\n", i, host_id);
			resetBus();        // Could wait a bit in case somebody else spots the bad packet

			return;				// done.
		}
    }
    
    // Store selfIDs
    OSObject * prop = OSData::withBytes( fSelfIDs, fNumSelfIDs * sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();
    
    buildTopology(false);
	
#if (DEBUGGING_LEVEL > 0)
    for(i=0; i<numIDs; i++) {
        id = IDs[2*i];
        if(id != ~IDs[2*i+1]) {
            DEBUGLOG("Bad SelfID: 0x%x <-> 0x%x\n", id, ~IDs[2*i+1]);
            continue;
        }
	DEBUGLOG("SelfID: 0x%x\n", id);
    }
    DEBUGLOG("Our ID: 0x%x\n", *ownIDs);
#endif
    // Find isochronous resource manager, if there is one
    irmID = 0;
    for(i=0; i<=fRootNodeID; i++) {
        id = OSSwapBigToHostInt32(*fNodeIDs[i]);
        // Get nodeID.
        nodeID = (id & kFWSelfIDPhyID) >> kFWSelfIDPhyIDPhase;
        nodeID |= kFWLocalBusAddress>>kCSRNodeIDPhase;

        if((id & (kFWSelfID0C | kFWSelfID0L)) == (kFWSelfID0C | kFWSelfID0L)) {
#if (DEBUGGING_LEVEL > 0)
            IOLog("IRM contender: %lx\n", nodeID);
#endif
            if(nodeID > irmID)
		irmID = nodeID;
	}
    }
    if(irmID != 0)
        fIRMNodeID = irmID;
    else
        fIRMNodeID = kFWBadNodeID;

    fBusMgr = false;	// Update if we find one.
    
	// inform IRM of updated bus generation and node ids
	fIRM->processBusReset( fLocalNodeID, fIRMNodeID, fBusGeneration );
	
    // OK for stuff that only needs our ID and the irmID to continue.
    if(localNode) {
        localNode->messageClients(kIOMessageServiceIsResumed);
		// force stop should take care of this activateAsyncStreamReceivers();
    }
    fDelayedStateChangeCmd->reinit(1000 * kScanBusDelay, delayedStateChange, NULL);
    fDelayedStateChangeCmd->submit();

	FWKLOG(( "IOFireWireController::processSelfIDs exited\n" ));
}

void IOFireWireController::processCycle64Int()
{
	if( fOutOfTLabels10S++ > 1 )
	{
		fOutOfTLabels10S = 0;
		
		if( fOutOfTLabels > fOutOfTLabelsThreshold )
		{
			IOLog("IOFireWireController:: %d Out of Transaction Labels in last 3 minutes.\n",(int) fOutOfTLabels);
			fOutOfTLabelsThreshold = fOutOfTLabels;
		}
		else
			fOutOfTLabelsThreshold = fOutOfTLabelsThreshold >> 1; // back off by half
		
		fOutOfTLabels = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// AssignCycleMaster
//
//   
//

bool IOFireWireController::AssignCycleMaster( )
{
	IOReturn					status = kIOReturnSuccess;
	int							i;
	UInt32						contender, linkOn, otherContenderID = 0, newRoot = 0, data1;
	Boolean						otherContender = false, localContender = false, needReset = false, badIRM = false;
		
	if( ((fRootNodeID & 63) == (fLocalNodeID & 63)) && fNodeMustBeRootFlag ) 
	{
		status = fFWIM->setContender( false );

		if(status == kIOReturnSuccess)
		{

			data1 = (kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
						((fForcedRootNodeID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR;
					
			// Send phy packet to set root hold off bit for node
			status = fFWIM->sendPHYPacket(data1);
					
			if(kIOReturnSuccess == status)
				needReset = true;
		}
	}
    else if( fDelegateCycleMaster || fBadIRMsKnown || fNodeMustNotBeRootFlag )
	{                        
		for( i = 0; i <= fRootNodeID; i++ ) 
		{
			contender = (OSSwapBigToHostInt32(*fNodeIDs[i]) >> 11) & 0x1;
			linkOn = (OSSwapBigToHostInt32(*fNodeIDs[i]) >> 22) & 0x1;

			if (contender && linkOn )
			{
				if ( i == (fLocalNodeID & 63) )
				{
					if (contender)
						localContender = true;
				}
				else
				{
					if( fScans[i] )
					{
						if( (not fScans[i]->fMustNotBeRoot) && (not fScans[i]->fIRMisBad) ) // if designated as root
						{
							otherContender = true;
							otherContenderID = i;		// any one will do (use highest)
						}
					}
				}
			}
		}

		if (otherContender)
		{
			if( (fRootNodeID & 63) == (fLocalNodeID & 63) && fDelegateCycleMaster )
			{
				// We are root, but we don't really want to be and somebody else can do the job.
				// We're doing this in response to self-IDs, so we have not scanned
				// the bus yet - presumably it is safe to use the asynch transmitter
				// because nobody else could be using it.

				//zzz might want to wait for it to settle down in case it was running
				// when the bus reset happened

				status = fFWIM->setContender( false );

				if( status == kIOReturnSuccess )
				{
					// force root
					fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
								((otherContenderID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR );

					needReset = true;
				}
			}
		}
		else if( ((fRootNodeID & 63) != (fLocalNodeID & 63)) && (localContender) && (not fDelegateCycleMaster))
		{
			status = fFWIM->setContender( true );

			// Set RHB for our soon to be root node and clear everyone else
			fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
						((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR );

			needReset = true;
		}
		
		if( status == kIOReturnSuccess && fBadIRMsKnown )
		{	
			// Check for unresponsive IRM
			badIRM = (!fScans[fIRMNodeID & 63]) && ((fLocalNodeID & 63) != (fIRMNodeID & 63));
			
			if(  fScans[fIRMNodeID & 63] && fScans[fIRMNodeID & 63]->fIRMisBad )
				badIRM = true;

			// If Bad IRM or no IRM then find somebody to do the job
			if( badIRM || (!localContender) && (!otherContender) )
			{
			
				if( !fDelegateCycleMaster )
				{
					// Mac which wants to be cyclemaster gets priority
					newRoot = fLocalNodeID & 63;
					fFWIM->setContender( true );
					fFWIM->setRootHoldOff(true);
				}
				else if( otherContender )
					newRoot = otherContenderID;
				else
				{
					// Oh well, nobody else can do it. Make Mac root. Make sure C is set and set our RHB
					newRoot = fLocalNodeID & 63;
					fFWIM->setContender( true );
					fFWIM->setRootHoldOff(true);
				}

				if( badIRM )
					IOLog("IOFireWireController unresponsive IRM at node %lx, forcing root to node %lx\n", (UInt32) fIRMNodeID & 63, newRoot );

				// Set RHB for our soon to be root node and clear everyone else
				fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
							((newRoot & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR );

				needReset = true;
			}
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		if( needReset )
		{
			//FWLogEvent ("HackAvoidBeingRoot Reset Bus\n");
			/* zzzzzzzzz edit comment after checking where this gets called from */
			//  It is important to re-enable bus reset interrupts when we issue a bus reset.
			//  Otherwise if the PHY->Link interface is really confused about self-ID
			//  streams we might never see the self ID interrupts. (LSI/Lucent problem).
			//  By re-enabling bus reset interrupts we see the next interrupt and 
			//  FWProcessBusReset will queue its selfID timer. If we don't get self
			//  ids then FSL will reset the bus until we get real ones which should
			//  happen eventually.
			//
			//  Since this function is always called from the SelfIDDeferredTask we know
			//  that bus reset interrupts are enabled and thus don't have to do anything.

			// Cause bus reset
			fFWIM->resetBus();

			IOSleep( 10 );												// sleep for 10 ms
		}
	}
		
	return( needReset );
}


// startBusScan
//
//

void IOFireWireController::startBusScan() 
{
    int i;

	FWKLOG(( "IOFireWireController::startBusScan entered\n" ));

	OSObject * existProp = fFWIM->getProperty( "FWDSLimit" );
	
	if( existProp )
	{
		fDSLimited = true;
	}
	else
	{
		fDSLimited = false;
	}
	
    // Send global resume packet
	fFWIM->sendPHYPacket(((fLocalNodeID & 0x3f) << kFWPhyPacketPhyIDPhase) | 0x003c0000);

    // Tell all active isochronous channels to re-allocate bandwidth
    IOFWIsochChannel *found;
    fAllocChannelIterator->reset();
    while( (found = (IOFWIsochChannel *)fAllocChannelIterator->getNextObject()) ) 
	{
        found->handleBusReset();
    }

	IOFireWireIRMAllocation *irmAllocationfound;
    fIRMAllocationsIterator->reset();
    while( (irmAllocationfound = (IOFireWireIRMAllocation *)fIRMAllocationsIterator->getNextObject()) ) 
	{
        irmAllocationfound->handleBusReset(fBusGeneration);
    }
	
	fNumROMReads = fRootNodeID+1;
	for(i=0; i<=fRootNodeID; i++) {
        UInt16 nodeID;
        UInt32 id;
        id = OSSwapBigToHostInt32(*fNodeIDs[i]);
        // Get nodeID.
        nodeID = (id & kFWSelfIDPhyID) >> kFWSelfIDPhyIDPhase;
        nodeID |= kFWLocalBusAddress>>kCSRNodeIDPhase;
        if(nodeID == fLocalNodeID)
 {
			fNumROMReads--;
			continue;	// Skip ourself!
		}
		
		// ??? maybe we should add an fwdebug bit to be strict on scanning only nodes with link bit?
	
        // Read ROM header if link is active (MacOS8 turns link on, why?)
        if(true) { //id & kFWSelfID0L) {
            IOFWNodeScan *scan;
            scan = (IOFWNodeScan *)IOMalloc(sizeof(*scan));

            scan->fControl = this;
            scan->fAddr.nodeID = nodeID;
            scan->fAddr.addressHi = kCSRRegisterSpaceBaseAddressHi;
            scan->fAddr.addressLo = kConfigBIBHeaderAddress;
            scan->fSelfIDs = fNodeIDs[i];
            scan->fNumSelfIDs = fNodeIDs[i+1] - fNodeIDs[i];
            scan->fRead = 0;
            scan->generation = fBusGeneration;
			scan->fRetriesBumped = 0;
            scan->fCmd = new IOFWReadQuadCommand;
 			scan->fLockCmd = new IOFWCompareAndSwapCommand; 
           
           // Read an IRM Register if node is a contender
            if( (id & (kFWSelfID0C | kFWSelfID0L)) == (kFWSelfID0C | kFWSelfID0L) )
            {
				// IOLog("Node 0x%x is Contender\n",nodeID);
            	scan->fContenderNeedsChecking = true;
            }
            else
				scan->fContenderNeedsChecking = false;
			scan->fIRMisBad = false;
			scan->fIRMCheckingRead = false;
			scan->fIRMCheckingLock = false;

 			FWKLOG(( "IOFireWireController::startBusScan node %lx speed was %lx\n",(UInt32)nodeID,(UInt32)FWSpeed( nodeID ) ));	
           	
			if ( fDebugIgnoreNode != kFWDebugIgnoreNodeNone && ((fDebugIgnoreNode & kFWMaxNodesPerBus ) == (nodeID & kFWMaxNodesPerBus )) )
			{
				// we do not want to speedcheck an ignored node.
            	scan->speedChecking = false;
				
				// If current speed to node is masked (ie speedchecking necessary), set to s100.
				// This is because we treat an "ignored" node as a unresponsive node (eg hub)
				// and if we were to speedcheck it, we would step-down to s100, unmasked.
				if( FWSpeed( nodeID ) & kFWSpeedUnknownMask ) 
					setNodeSpeed(scan->fAddr.nodeID, kFWSpeed100MBit);
				// else, do not change current speed to node, which is concurrent with
				// the non-ignore, non-speedchecking code below.
				
				// using kIOReturnNotPermitted because, simply, it's different than
				// success, timeout, or bus reset.
				scan->fControl->readDeviceROM(scan, kIOReturnNotPermitted);
			}
			else
			{
				if( FWSpeed( nodeID ) & kFWSpeedUnknownMask ) 
				{
					setNodeSpeed(scan->fAddr.nodeID, fLocalNodeID, (FWSpeed(scan->fAddr.nodeID, fLocalNodeID) & ~kFWSpeedUnknownMask));

					scan->fCmd->initAll(this, fBusGeneration, scan->fAddr, scan->fBuf, 1,
													&readROMGlue, scan);

					FWKLOG(( "IOFireWireController::startBusScan speedchecking\n" ));	
					scan->speedChecking = true;	// May need to try speeds slower than s800 if this fails
												// zzz What about s1600?
				}
				else
				{
					scan->fCmd->initAll(this, fBusGeneration, scan->fAddr, scan->fBuf, 1,
													&readROMGlue, scan);

					scan->fCmd->setMaxSpeed( kFWSpeed100MBit );
					scan->speedChecking = false;
					FWKLOG(( "IOFireWireController::startBusScan not speedchecking\n" ));
				}	
				
				scan->fIRMBitBucketNew = 0xffffffff;
				scan->fIRMBitBucketOld = 0xffffffff;
				
				scan->fLockCmd->initAll(this, fBusGeneration, scan->fAddr, &scan->fIRMBitBucketOld, &scan->fIRMBitBucketNew, 1, &readROMGlue, scan);

				scan->fCmd->setRetries(kFWCmdZeroRetries);  // don't need to bump kRetriesBumped here
				scan->fCmd->submit();
			}
        }
    }
	
    if(fNumROMReads == 0) {
        finishedBusScan();
    }
	
	FWKLOG(( "IOFireWireController::startBusScan exited\n" ));	
}

// readROMGlue
//
//

void IOFireWireController::readROMGlue(void *refcon, IOReturn status,
			IOFireWireNub *device, IOFWCommand *fwCmd)
{
    IOFWNodeScan *scan = (IOFWNodeScan *)refcon;
    scan->fControl->readDeviceROM(scan, status);
}

// readDeviceROM
//
//

void IOFireWireController::readDeviceROM(IOFWNodeScan *scan, IOReturn status)
{
    bool done = true;
	
	FWKLOG(( "IOFireWireController::readDeviceROM entered\n" ));

    if(status != kIOReturnSuccess) 
	{
		// If status isn't bus reset, make a dummy registry entry.
        
		if(status == kIOFireWireBusReset) 
		{
            scan->fCmd->release();
            scan->fLockCmd->release();
            IOFree(scan, sizeof(*scan));
			FWKLOG(( "IOFireWireController::readDeviceROM exited\n" ));
			return;
        }
        
        
        if( scan->fIRMCheckingRead || scan->fIRMCheckingLock )
		{
 			// IOLog("Bad IRM for Node 0x%x Rd %d Lk %d\n",scan->fAddr.nodeID,scan->fIRMCheckingRead,scan->fIRMCheckingLock);
			if( (scan->fAddr.nodeID & 63) == (fIRMNodeID & 63) )
				fBadIRMsKnown = true;
				
			scan->fIRMisBad = true;
			scan->fContenderNeedsChecking = false;
		}
		else 
		{
		
			// "Naughty Camera Workaround"
			// If a "slow" device responds AckPending to the first attempt of the first BIB ReadQuad during
			// BusScan AND TIMES OUT (no ReadResp), we increase the number of times that ReadQuad may retry it's command
			// because we know the device is there, but not ready to respond to ConfigROM reads.
			
			// This "if" should run after trying the command once, but before it retries.
			// Check if command timed out and we haven't increased the retries for this speed
			if( status == kIOReturnTimeout && scan->fRetriesBumped == 0 )
			{
				// Limit workaround to BIBHeaderAddress to avoid long delay caused by a really slow device
				// (e.g. hung node) ackPending any attempt to read any address. This will make sure we don't wait
				// "too" long while trying to collect all GUIDs, like lose our time to reconnect to an SBP2 device.
				if ( scan->fCmd->getAckCode() == kFWAckPending && scan->fAddr.addressLo == kConfigBIBHeaderAddress )
				{
					FWKLOG(( "IOFireWireController::readDeviceROM Node 0x%x timed out on ack %d, setting setMaxRetries = %d\n", scan->fAddr.nodeID, scan->fCmd->getAckCode(), kFWCmdIncreasedRetries));
					
					// increase retries and set RetriesBumped flag
					scan->fCmd->setRetries(kFWCmdIncreasedRetries);
					scan->fRetriesBumped++;
					
					// re-submit command for increased number of retries
					scan->fCmd->reinit(scan->fAddr, scan->fBuf, 1, &readROMGlue, scan, true);
					scan->fCmd->submit();
					return;
				} 
				else
				{
					// increase retries to normal-1 and set RetriesBumped flag
					scan->fCmd->setRetries(kFWCmdReducedRetries);
					scan->fRetriesBumped++;
					
					// re-submit command for the rest of the retries
					scan->fCmd->reinit(scan->fAddr, scan->fBuf, 1, &readROMGlue, scan, true);
					scan->fCmd->submit();
					return;
				}
			}
			
			
			// Speed checking for 1394b compatibility 
			FWKLOG(( "IOFireWireController::readDeviceROM speedcheck %lx ; speed %lx\n", (UInt32)scan->speedChecking, (UInt32)FWSpeed( scan->fAddr.nodeID ) ));
			if( scan->speedChecking && FWSpeed( scan->fAddr.nodeID ) > kFWSpeed100MBit )
			{
				if( scan->generation == fBusGeneration )
				{
					FWKLOG(( "IOFireWireController::readDeviceROM reseting speed for node %lx from local %lx\n", (UInt32)scan->fAddr.nodeID, (UInt32)fLocalNodeID));
					if( fDSLimited )
					{
						setNodeSpeed(scan->fAddr.nodeID, fLocalNodeID, kFWSpeed100MBit);				
					}
					else
					{
						setNodeSpeed(scan->fAddr.nodeID, fLocalNodeID, (FWSpeed(scan->fAddr.nodeID, fLocalNodeID) - 1));				
					}
					
					// Retry command at slower speed
					scan->fCmd->reinit(scan->fAddr, scan->fBuf, 1, &readROMGlue, scan, true);
					
					if ( scan->fAddr.addressLo == kConfigBIBHeaderAddress )
					{
						// Reset to run Naughty Camera check on first try of QRead at next speed
						scan->fCmd->setRetries(kFWCmdZeroRetries);
						scan->fRetriesBumped = 0;
					}
					else
					{
						scan->fCmd->setRetries(kFWCmdDefaultRetries);
						scan->fRetriesBumped = 0;
					}
					
					scan->fCmd->submit();
					return;
				}
			}
	
			if( (scan->fAddr.nodeID & 63) == (fIRMNodeID & 63) )
				fBadIRMsKnown = true;	
	
			UInt32 nodeID = FWAddressToID(scan->fAddr.nodeID);
			fNodes[nodeID] = createDummyRegistryEntry( scan );
			
			fNumROMReads--;
			if(fNumROMReads == 0) 
			{
				finishedBusScan();
			}
	
			scan->fCmd->release();
			scan->fLockCmd->release();
			IOFree(scan, sizeof(*scan));
			FWKLOG(( "IOFireWireController::readDeviceROM exited\n" ));
			return;
		}
    }
	
    if( scan->fRead == 0 ) 
	{
		UInt32 bib_quad = OSSwapBigToHostInt32( scan->fBuf[0] );
		if( ((bib_quad & kConfigBusInfoBlockLength) >> kConfigBusInfoBlockLengthPhase) == 1) 
		{
            // Minimal ROM
            scan->fROMSize = 4;
            done = true;
		}
		else 
		{
            scan->fROMSize = 20;	// Just read bus info block
            scan->fRead = 8;
            scan->fBuf[1] = OSSwapHostToBigInt32( kFWBIBBusName );	// no point reading this!
            scan->fAddr.addressLo = kConfigROMBaseAddress+8;
            scan->fCmd->reinit(scan->fAddr, scan->fBuf+2, 1,
                                                        &readROMGlue, scan, true);
            scan->fCmd->setMaxSpeed( kFWSpeed100MBit );
			scan->fCmd->setRetries( kFWCmdDefaultRetries );
			scan->fCmd->submit();
            done = false;
		}
    }
    else if( scan->fRead < 16 ) 
	{		
        if(scan->fROMSize > scan->fRead) 
		{
            scan->fRead += 4;
            scan->fAddr.addressLo = kConfigROMBaseAddress+scan->fRead;
            scan->fCmd->reinit(scan->fAddr, scan->fBuf+scan->fRead/4, 1,
                                                        &readROMGlue, scan, true);
            scan->fCmd->setMaxSpeed( kFWSpeed100MBit );
			scan->fCmd->setRetries(kFWCmdDefaultRetries);
			scan->fCmd->submit();
            done = false;
        }
        else
        {
           		done = true;
        }
    }
    else if( scan->fContenderNeedsChecking && (FWSpeed( scan->fAddr.nodeID ) > kFWSpeed100MBit) )
	{
		// Only do this check if node is > 100 MBit. s100 devices are nearly always camcorders and not likely to have
		// non-functional IRMs.
		
		// Need to know if this is a capable IRM if this node is already IRM or if Mac doesn't
		// want to be IRM and this is a candidate.
		
		if( ((scan->fAddr.nodeID & 63) == (fIRMNodeID & 63)) || fDelegateCycleMaster )
		{
			if( !scan->fIRMCheckingRead )
			{
				scan->fIRMCheckingRead = true;				// Doing the Read check this go-around

				scan->fAddr.addressLo = kCSRChannelsAvailable63_32;
				scan->fCmd->reinit(scan->fAddr, &scan->fIRMBitBucketOld, 1,
															&readROMGlue, scan, true);
				scan->fCmd->setMaxSpeed( kFWSpeed100MBit );
				scan->fCmd->setRetries(kFWCmdDefaultRetries);
				scan->fCmd->submit();
				done = false;
			}
			else
			{
			
				// IOLog("Checking IRM for Node 0x%x\n",scan->fAddr.nodeID);
				scan->fIRMCheckingLock = true;				// Doing the Lock check this go-around
				scan->fContenderNeedsChecking = false;		// Don't check again
				scan->fAddr.addressLo = kCSRChannelsAvailable63_32;
				
				scan->fLockCmd->reinit( scan->generation, scan->fAddr, &scan->fIRMBitBucketOld, &scan->fIRMBitBucketNew, 1, &readROMGlue, scan);
													
				scan->fLockCmd->setMaxSpeed( kFWSpeed100MBit );
				scan->fLockCmd->setRetries(kFWCmdDefaultRetries);
				scan->fLockCmd->submit();
				done = false;
			}
		}
	}

	
	
    if( done ) 
	{
		UInt32	nodeID = FWAddressToID(scan->fAddr.nodeID);
		
		FWKLOG(( "IOFireWireController::readDeviceROM scan for ID %lx is %lx\n",nodeID,(long) scan ));
		fScans[nodeID] = scan;
		
 		updateDevice( scan );
       	
       	fNumROMReads--;
        if(fNumROMReads == 0)
		{
            finishedBusScan();
        }

    }
	
	FWKLOG(( "IOFireWireController::readDeviceROM exited\n" ));
}

// checkForDuplicateGUID
//
//
bool IOFireWireController::checkForDuplicateGUID(IOFWNodeScan *scan, CSRNodeUniqueID *currentGUIDs )
{
	CSRNodeUniqueID guid;
	UInt32 nodeID;
	UInt32 i;
		
	nodeID = FWAddressToID(scan->fAddr.nodeID);

	if(scan->fROMSize >= 20)
	{
		UInt32 guid_hi = OSSwapBigToHostInt32( scan->fBuf[3] );
		UInt32 guid_lo = OSSwapBigToHostInt32( scan->fBuf[4] );
			
		guid = ((CSRNodeUniqueID)guid_hi << 32) | guid_lo;
	}
	else
	{
		currentGUIDs[nodeID] = 0;
		return false;	// not a real ROM, don't care.
	}

	currentGUIDs[nodeID] = guid;
	
	if( !guid || fGUIDDups->findDuplicateGUID( guid, fBusGeneration ) )
		return false;	// Already found or zero, don't add it. Return false so caller doesn't reset the bus again

	for( i = 0; i< nodeID; i++ )
	{
		if( currentGUIDs[i] == guid )
		{
			fGUIDDups->addDuplicateGUID( guid, fBusGeneration );
			return true;
		}
	}
	
	return false;
}

// updateDevice
//
//

void IOFireWireController::updateDevice(IOFWNodeScan *scan )
{
	// See if this is a bus manager
	UInt32 bib_quad = OSSwapBigToHostInt32( scan->fBuf[2] );
	if( !fBusMgr )
	{
		fBusMgr = bib_quad & kFWBIBBmc;
	}
	
// Check if node exists, if not create it
#if (DEBUGGING_LEVEL > 0)
	IOLog("Update Device Finished reading ROM for node 0x%x\n", scan->fAddr.nodeID);
#endif
	IOFireWireDevice *	newDevice = NULL;
	do 
	{
		CSRNodeUniqueID guid;
		OSIterator *childIterator;
		UInt32 nodeID;
		bool duplicate;
		bool minimal = false;
		
		nodeID = FWAddressToID(scan->fAddr.nodeID);
	
		if(scan->fROMSize >= 20)
        {
			UInt32 guid_hi = OSSwapBigToHostInt32( scan->fBuf[3] );
			UInt32 guid_lo = OSSwapBigToHostInt32( scan->fBuf[4] );
			
		    guid = ((CSRNodeUniqueID)guid_hi << 32) | guid_lo;
		}
		else
		{
			minimal = true;
			guid = 0;
		}

		//
		// GUID zero is not a valid GUID. Unfortunately some devices return this as
		// their GUID until they're fully powered up.
		//
		//

		// Also check nodes for known bad GUIDs and don't bother with them
		//
		
		duplicate = fGUIDDups->findDuplicateGUID( guid, fBusGeneration );
				
		if( (guid == 0) || duplicate )
		{
			fNodes[nodeID] = createDummyRegistryEntry( scan );
			if( minimal )
			{
				OSObject * prop = OSData::withBytes( &scan->fBuf[0], scan->fROMSize );
				if( prop != NULL ) 
				{
					fNodes[nodeID]->setProperty( gFireWireROM, prop );
					prop->release();
				}
			}
			else
			{
				OSObject * prop = OSNumber::withNumber( guid, 64 );
				if( prop != NULL ) 
				{
					fNodes[nodeID]->setProperty( gFireWire_GUID, prop );
					prop->release();
					
				}
			}
// 			if( duplicate )
// 			{
// 				
// 				OSString * prop = OSString::withCString("Device with illegal duplicate GUID");
// 				if( prop2 != NULL ) 
// 				{				
// 					fNodes[nodeID]->setProperty( gFireWireProduct_Name, prop );
// 					prop->release();
// 				}
// 			}
			continue;
		}
		
		childIterator = getClientIterator();
		if( childIterator) 
		{
			OSObject *child;
			while( (child = childIterator->getNextObject())) 
			{
				IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
				if( found ) 
				{
					// sync with open / close routines on device
					found->lockForArbitration();
					
					if( found->fUniqueID == guid && (found->getTerminationState() != kTerminated) ) 
					{
						newDevice = found;
						
						// arbitration lock still held
						
						break;
					}
					
					found->unlockForArbitration();
				}
			}
			childIterator->release();
		}

		if(newDevice) 
		{
			// Just update device properties.
			#if IOFIREWIREDEBUG > 0
				IOLog("UpdateDevice Found old device 0x%p\n", newDevice);
			#endif
			
			// arbitration lock still held
			
			if( newDevice->getTerminationState() == kNeedsTermination )
			{
				newDevice->setTerminationState( kNotTerminated );
			}			
			
			newDevice->unlockForArbitration();
			
			newDevice->setNodeROM(fBusGeneration, fLocalNodeID, scan);
			newDevice->retain();	// match release, since not newly created.
		}
		else 
		{
			newDevice = fFWIM->createDeviceNub(guid, scan);
			if (!newDevice)
				continue;
				
			#if IOFIREWIREDEBUG > 0
				IOLog("Update Device Creating new device 0x%p\n", newDevice);
			#endif
			
			// we must stay busy until we've called registerService on the device
			// and all of its units
			
			newDevice->adjustBusy( 1 ); // device
			adjustBusy( 1 );  // controller
			
			FWKLOG(( "IOFireWireController@0x%08lx::updateDevice adjustBusy(1)\n", (UInt32)this ));
			
			// hook this device into the device tree now
			
			// we won't rediscover this device after a bus reset if the device is
			// not in the registry.  if we attached later and got a bus reset before
			// we had attached the device we would leak the device object
			
			if( !newDevice->attach(this) )
			{
				// if we failed to attach, I guess we're not busy anymore
				newDevice->adjustBusy( -1 );  // device
				adjustBusy( -1 );  // controller
				FWKLOG(( "IOFireWireController@0x%08lx::updateDevice adjustBusy(-1)\n", (UInt32)this ));
				continue;
			}
			
			// we will register this service once we finish reading the config rom
			newDevice->setRegistrationState( IOFireWireDevice::kDeviceNeedsRegisterService );

			// this will start the config ROM read
			newDevice->setNodeROM( fBusGeneration, fLocalNodeID, scan );

		}
		
		fNodes[nodeID] = newDevice;
		fNodes[nodeID]->retain();
	
	} while (false);
	
	if (newDevice)
		newDevice->release();
	
}



// createDummyRegistryEntry
//
// if we are unable to successfully read the BIB of a a device, we create
// a dummy registry entry by calling this routine

IORegistryEntry * IOFireWireController::createDummyRegistryEntry( IOFWNodeScan *scan )
{
	OSDictionary *propTable;
	OSObject * prop;
	propTable = OSDictionary::withCapacity(3);
	prop = OSNumber::withNumber(scan->fAddr.nodeID, 16);
	propTable->setObject(gFireWireNodeID, prop);
	prop->release();

	prop = OSNumber::withNumber((OSSwapBigToHostInt32(scan->fSelfIDs[0]) & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
	propTable->setObject(gFireWireSpeed, prop);
	prop->release();

	prop = OSData::withBytes(scan->fSelfIDs, scan->fNumSelfIDs*sizeof(UInt32));
	propTable->setObject(gFireWireSelfIDs, prop);
	prop->release();

	IORegistryEntry * newPhy;
	newPhy = new IORegistryEntry;
	if(newPhy) 
	{
		if(!newPhy->init(propTable)) 
		{
			newPhy->release();	
			newPhy = NULL;
		}
        if(getSecurityMode() == kIOFWSecurityModeNormal) {
            // enable physical access for FireBug and other debug tools without a ROM
            setNodeIDPhysicalFilter( scan->fAddr.nodeID & 0x3f, true );
        }
        if(propTable)
            propTable->release();
	}
	
	return newPhy;
}

// finishedBusScan
//
//

void IOFireWireController::finishedBusScan()
{
    // These magic numbers come from P1394a, draft 4, table C-2.
    // This works for cables up to 4.5 meters and PHYs with
    // PHY delay up to 144 nanoseconds.  Note that P1394a PHYs
    // are allowed to have delay >144nS; we don't cope yet.
    static UInt32 gaps[26] = {63, 5, 7, 8, 10, 13, 16, 18, 21,
                            24, 26, 29, 32, 35, 37, 40, 43, 
                            46, 48, 51, 54, 57, 59, 62, 63};
  	int		i;
                            
	if(	AssignCycleMaster() )
		return;
    
    fBadIRMsKnown = false; 	// If we got here we're happy with the IRM/CycleMaster. No need to read the IRM registers for all nodes
    
    // Go update all the devices now that we've read their ROMs.
    {
 		CSRNodeUniqueID			currentGUIDs[kFWMaxNodesPerBus];
 		
 		// First check for duplicate GUIDs   	
		for( i=0; i<=fRootNodeID; i++ ) 
		{
			if( fScans[i] )
			{
				if( checkForDuplicateGUID( fScans[i], currentGUIDs ) )
				{
					// Whoops, duplicate GUID! Reset Bus and bail.
					// From now on if UpdateDevice is called with this GUID we won't
					// reconnect to it.
				
                	resetBus();
					return;			// We'll be right back after these messages from our sponsor
				}
    			
				fScans[i]->fCmd->release();
				fScans[i]->fLockCmd->release();
				IOFree(fScans[i], sizeof(*fScans[i]));
				fScans[i] = NULL;

			}
  			else
  			{
  				currentGUIDs[i] = 0;
  			}
  		}
  	}
	
	// tell FWIM to stop timing transmits
	fFWIM->setPingTransmits( false );
			    
    // Now do simple bus manager stuff, if there isn't a better candidate.
    // This might cause us to issue a bus reset...
    // Skip if we're about to reset anyway, since we might be in the process of setting
    // another node to root.
    if( !fBusResetScheduled && !fBusMgr && fLocalNodeID == fIRMNodeID) 
	{
  		UInt32 * pingTimes;
        int maxHops;
  	 	UInt32	maxPing = 0;
 		UInt32	pingGap, hopGap,newGap;
 		bool	retoolGap = false;

		if ( not fForcedGapFlag )
		{
			// Set the gap count based on maximum ping time. This assumes the Mac is not in the middle
			// of a star with long haul's going out in two different directions from the Mac. To obtain
			// the gap cound we use the following algorithm:
			//
			// 		Gap = GapTable[ (MaxPing - 20) / 9 ]
			
			// This result is then compared to the value arrived at with the standard hop count based
			// table lookup. The higher value is used for the new gap.

			// If we don't have ping time information (such as with a Lynx FWIM that doesn't support
			// ping timimg) then we use the maximum hop count to index into the table.        
			 
			// Do lazy gap count optimization.  Assume the bus is a daisy-chain (worst
			// case) so hop count == root ID.

			// a little note on the gap count optimization - all I do is set an internal field to our optimal
			// gap count and then reset the bus. my new soft bus reset code sets the gap count before resetting
			// the bus (for another reason) and I just rely on that fact.

			pingTimes = fFWIM->getPingTimes();
		
			for( i=0; i<=fRootNodeID; i++ ) 
			{
				//IOLog("IOFireWireController node 0x%lx ping 0x%lx\n",i,pingTimes[i]);
			
				if( pingTimes[i] > maxPing )
					maxPing = pingTimes[i];
			}
			
			maxHops = fRootNodeID;
			if (maxHops > 25)
				maxHops = 25;
		   
			if( maxPing > 245 )
				maxPing = 245;
		   
			if( maxPing >= 29 )
				pingGap = gaps [(maxPing - 20) / 9];	// Assumes Mac is NOT in the middle of 2 long haul subnets
			else
				pingGap = 5;
			
			hopGap = gaps[maxHops];
			
			if( hopGap > pingGap )
				newGap = hopGap;
			else
				newGap = pingGap;
			
			// IOLog("IOFireWireController MaxPingTime: 0x%lx PingGap: 0x%lx HopGap: 0x%lx Setting Gap to 0x%lx\n",maxPing, pingGap, hopGap, newGap);
			fGapCount = newGap << kFWPhyConfigurationGapCntPhase;
		}
		else 
		{
			if( fPreviousGap != (fForcedGapCount << kFWPhyConfigurationGapCntPhase)  )
			{
				fGapCount = fForcedGapCount << kFWPhyConfigurationGapCntPhase; 
				retoolGap = true;
				IOLog( "IOFireWireController::finishedBusScan retoold GAP %d\n", __LINE__ );
			}
		}

       	FWKLOG(("IOFireWireController MaxPingTime: 0x%lx PingGap: 0x%lx HopGap: 0x%lx Setting Gap to 0x%lx\n",maxPing, pingGap, hopGap, newGap));
        
		fGapCount = ( fForcedGapFlag ) ? fForcedGapCount << kFWPhyConfigurationGapCntPhase : newGap << kFWPhyConfigurationGapCntPhase;
				
        if(fRootNodeID == 0) 
        {
            // If we're the only node, clear root hold off.
            fFWIM->setRootHoldOff(false);
        }
        else 
        {
            // Send phy packet to get rid of other root hold off nodes
            // Set gap count too.
            fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                        ((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR );
            fFWIM->setRootHoldOff(true);

            // If we aren't root, issue a bus reset so that we will be.
            if(fRootNodeID != (fLocalNodeID & 63)) 
            {
				// IOLog( "IOFireWireController::finishedBusScan - make us root\n" );
                resetBus();
				FWKLOG(( "IOFireWireController::finishedBusScan exited\n" ));
				return;			// We'll be back...
            }
        }

        // If we got here we're root, so turn on cycle master
        fFWIM->setCycleMaster(true);

        // Finally set gap count if any nodes don't have the right gap.
        // Only bother if we aren't the only node on the bus.
        // To avoid changing the gap due to ping time jitter we check to see that the gaps
        // are both consistent and either the same as we last set it or the same as the new gap.
        
        
        if(fRootNodeID != 0)
        {
           	// is the gap count consistent?
            for( i = 1; i <= fRootNodeID; i++ )
            {
                if( (OSSwapBigToHostInt32(*fNodeIDs[i]) & kFWSelfID0GapCnt) != (OSSwapBigToHostInt32(*fNodeIDs[i - 1]) & kFWSelfID0GapCnt) ) 
				{
                	//IOLog( "IOFireWireController::finishedBusScan inconsistent gaps!\n");
                	retoolGap = true;
                	break;
                }
            }
            
            if( !retoolGap && !fDSLimited )
            {
				// is the gap something we set?
				for( i = 0; i <= fRootNodeID; i++ )
				{
					if( ((OSSwapBigToHostInt32(*fNodeIDs[i]) & kFWSelfID0GapCnt) != fPreviousGap 
									&& (OSSwapBigToHostInt32(*fNodeIDs[i]) & kFWSelfID0GapCnt) != fGapCount)
							|| ((OSSwapBigToHostInt32(*fNodeIDs[i]) & kFWSelfID0GapCnt) == 0) ) 
					{
                		//IOLog( "IOFireWireController::finishedBusScan need new gap count\n");
						retoolGap = true;
						break;
					}
				}
			}
            
            if( retoolGap )
            {
            
            	//IOLog( "IOFireWireController::finishedBusScan prev: %08lx new: %08lx node: %08lx\n",fPreviousGap, fGapCount, (*fNodeIDs[i] & kFWSelfID0GapCnt) );

            	fPreviousGap = fGapCount;
            	
				// send phy config packet and do bus reset.
				fDelayedPhyPacket = (kFWConfigurationPacketID << kFWPhyPacketIDPhase) | 
									((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) | 
									kFWPhyConfigurationR | fGapCount | kFWPhyConfigurationT;
				//	IOLog( "IOFireWireController::finishedBusScan - set gap count\n" );
				resetBus();
				FWKLOG(( "IOFireWireController::finishedBusScan exited\n" ));
				return;			// We'll be back...
            }
        }
    }


	//
    // Don't change to the waiting prune state if we're about to bus reset again anyway.
    //
	
	if(fBusState == kScanning) 
	{
		bool 			wouldTerminateDevice = false;
		OSIterator *	childIterator;
		
		//
		// check if we would terminate a device if the prune timer ran right now
		//
		
		childIterator = getClientIterator();
		if( childIterator ) 
		{
			OSObject *child;
			while( (child = childIterator->getNextObject())) 
			{
				IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
				
				// don't need to sync with open/close routines when checking for kNotTerminated
				if( found && (found->getTerminationState() == kNotTerminated) && found->fNodeID == kFWBadNodeID ) 
				{
					wouldTerminateDevice = true;
				}
			}
			childIterator->release();
		}
		
		//
		// if we found all of our devices, set the prune delay to normal
		//

		if( (fRootNodeID == 0) && (fDevicePruneDelay < kOnlyNodeDevicePruneDelay) )
		{
			// if we're the only node increase the prune delay
			// because we won't be causing a bus reset for gap 
			// count optimization.  
			
			fDevicePruneDelay = kOnlyNodeDevicePruneDelay;
		}	
				
		if( !wouldTerminateDevice )
		{
			fDevicePruneDelay = kNormalDevicePruneDelay;
		}
		
        fBusState = kWaitingPrune; 	// Indicate end of bus scan
        fDelayedStateChangeCmd->reinit(1000 * fDevicePruneDelay, delayedStateChange, NULL); // One second
        fDelayedStateChangeCmd->submit();
    }

    // Run all the commands that are waiting for reset processing to end
    IOFWCmdQ &resetQ(getAfterResetHandledQ());
    resetQ.executeQueue(true);

    // Anything on queue now is associated with a device not on the bus, I think...
    IOFWCommand *cmd;
    while( (cmd = resetQ.fHead) ) 
	{
        cmd->cancel(kIOReturnTimeout);
    }

	FWKLOG(( "IOFireWireController::finishedBusScan exited\n" ));
}

// countNodeIDChildren
//
//

UInt32 IOFireWireController::countNodeIDChildren( UInt16 nodeID, int hub_port, int * hubChildRemainder, bool * hubParentFlag )
{
	UInt32 id0, idn;
	UInt32 *idPtr;
	int i;
	int children = 0;
	int ports;
	UInt32 port;
	int mask, shift;
	
	if( hub_port > 2 )
	{
		// we currently only look at self id type 0 since we don't currently ship >3 port phys 
		IOLog( "IOFireWireController::countNodeIDChildren - hub_port = %d out of range.\n", hub_port );
	}
	
	// get type 0 self id
	i = nodeID & 63;
	idPtr = fNodeIDs[i];
	id0 = OSSwapBigToHostInt32(*idPtr++);
	mask = kFWSelfID0P0;
	shift = kFWSelfID0P0Phase;
		
	// count children
	// 3 ports in type 0 self id
	for(ports=0; ports<3; ports++) 
	{
		port = (id0 & mask) >> shift;
		if(port == kFWSelfIDPortStatusChild)
		{
			children++;
		}

		if( ports == hub_port )
		{
			if( port == kFWSelfIDPortStatusChild )
			{
				// when the topology builder gets down to the current child count
				// then we are at our hub
				if( hubChildRemainder != NULL )
					*hubChildRemainder = children;
			}
			else if( port == kFWSelfIDPortStatusParent )
			{
				// the hub us our parent
				if( hubParentFlag != NULL )
					*hubParentFlag = true;
			}
		}
		
		mask >>= 2;
		shift -= 2;
	}

	// any more self ids for this node?
	if(fNodeIDs[i+1] > idPtr) 
	{
		// get type 1 self id
		idn = OSSwapBigToHostInt32(*idPtr++);
		mask = kFWSelfIDNPa;
		shift = kFWSelfIDNPaPhase;
		
		// count children
		// 8 ports in type 1 self id
		for(ports=0; ports<8; ports++) 
		{
			port = (idn & mask) >> shift;
			if(port == kFWSelfIDPortStatusChild)
				children++;
			mask >>= 2;
			shift -= 2;
		}
		
		// any more self ids for this node?
		if(fNodeIDs[i+1] > idPtr) 
		{
			// get type 2 self id
			idn = OSSwapBigToHostInt32(*idPtr++);
			mask = kFWSelfIDNPa;
			shift = kFWSelfIDNPaPhase;
			
			// count children
			// 5 ports in type 2 self id
			for(ports=0; ports<5; ports++) 
			{
				port = (idn & mask) >> shift;
				if(port == kFWSelfIDPortStatusChild)
					children++;
				mask >>= 2;
				shift -= 2;
			}
		}
	}
	
	return children;
}

// getPortNumberFromIndex
//
//

UInt32 IOFireWireController::getPortNumberFromIndex( UInt16 index )
{
	UInt32 id0, idn;
	UInt32 *idPtr;
	int i;
	int children = 0;
	int ports;
	UInt32 port;
	int mask, shift;
	
	// get type 0 self id
	i = fLocalNodeID & 63;
	idPtr = fNodeIDs[i];
	id0 = OSSwapBigToHostInt32(*idPtr++);
	mask = kFWSelfID0P0;
	shift = kFWSelfID0P0Phase;
	
	// count children
	// 3 ports in type 0 self id
	for(ports=0; ports<3; ports++) 
	{
		port = (id0 & mask) >> shift;
		if(port == kFWSelfIDPortStatusChild)
		{
			if( index == children )
				return ports;
			children++;
		}
		mask >>= 2;
		shift -= 2;
	}

	// any more self ids for this node?
	if(fNodeIDs[i+1] > idPtr) 
	{
		// get type 1 self id
		idn = OSSwapBigToHostInt32(*idPtr++);
		mask = kFWSelfIDNPa;
		shift = kFWSelfIDNPaPhase;
		
		// count children
		// 8 ports in type 1 self id
		for(ports=0; ports<8; ports++) 
		{
		if(port == kFWSelfIDPortStatusChild)
			{
				//zzz shouldn't ports be returned + 3?
				if( index == children )
					return ports;
				children++;
			}
			mask >>= 2;
			shift -= 2;
		}
	}
	
	return 0xFFFFFFFF;
}

// buildTopology
//
//

void IOFireWireController::buildTopology(bool doFWPlane)
{
    int i, maxDepth;
    IORegistryEntry *root;
    struct FWNodeScan
    {
        int nodeID;
        int childrenRemaining;
		int hubChildRemainder;
		bool hubParentFlag;
        IORegistryEntry *node;
    };
    FWNodeScan scanList[kFWMaxNodesPerBus];
    FWNodeScan *level;
    maxDepth = 0;
    root = fNodes[fRootNodeID];
    level = scanList;

    // First build the topology.
	
	// iterate over all self ids starting with root id
	for(i=fRootNodeID; i>=0; i--) 
	{
        UInt32 id0;
        UInt8 speedCode;
        IORegistryEntry *node = fNodes[i];
        int children = 0;
    
		// count the children for this self id
		if( (i == (fLocalNodeID & 63)) && (fHubPort != kFWInvalidPort) )
		{
			// we use hubChildRemainder here to find the port connected to the builtin hub.
			// eg. if childrenRemaining is 3 and the hub is the 3rd child, hubChildRemainder will be 1.
			// obvious, no? it's weird like this because at this point in the project I don't want 
			// to disturb the underlying enumeration algorithm which uses childrenRemaining for iteration.
			
			bool hubParent = false;
			int hubChildRemainder = 0;
						
			children = countNodeIDChildren( i, fHubPort, &hubChildRemainder, &hubParent );

//			IOLog( "FireWire - buildtopology - node = %d, children = %d, hubPort = %d, hubChildRem = %d, hubParent = %d\n", i, children, fHubPort, hubChildRemainder, hubParent );
			
			// Add node to bottom of tree
			level->nodeID = i;
			level->childrenRemaining = children;
			level->hubChildRemainder = hubChildRemainder;
			level->hubParentFlag = hubParent;
			level->node = node;
		}
		else
		{
			children = countNodeIDChildren( i );

			// Add node to bottom of tree
			level->nodeID = i;
			level->childrenRemaining = children;
			level->hubChildRemainder = 0;
			level->hubParentFlag = false;
			level->node = node;
		}
		
        // Add node's self speed to speedmap
		id0 = OSSwapBigToHostInt32(*fNodeIDs[i]);
		speedCode = (id0 & kFWSelfID0SP) >> kFWSelfID0SPPhase;
                
        if( !doFWPlane )
        {
        	if( speedCode == kFWSpeedReserved )
        		speedCode = kFWSpeed800MBit | kFWSpeedUnknownMask;	// Remember that we don't know how fast it is
        }
  
		if( fDSLimited && !(speedCode & kFWSpeedUnknownMask) )
      	{
			speedCode = kFWSpeed100MBit;
		}
		
        setNodeSpeed(i, i, speedCode);
		
        // Add to parent
        // Compute rest of this node's speed map entries unless it's the root.
        // We only need to compute speeds between this node and all higher node numbers.
        // These speeds will be the minimum of this node's speed and the speed between
        // this node's parent and the other higher numbered nodes.
        if (i != fRootNodeID) 
		{
            int parentNodeNum, scanNodeNum;
            parentNodeNum = (level-1)->nodeID;
            if(doFWPlane)
            {
				FWNodeScan * parent_level = (level-1);
				
				if( parent_level->hubChildRemainder == parent_level->childrenRemaining )
				{
					if( parent_level->childrenRemaining == 0 )
					{
						// this should be impossible
						IOLog( "IOFireWireController::buildTopology - parent child count is 0!\n" );
					}
					else
					{
						// we are the hub
						node->setProperty( "Built-in Hub", true );
					}
				}
				else if( level->hubParentFlag )
				{
					// our parent is the hub
					parent_level->node->setProperty( "Built-in Hub", true );
				}
  			
				if( (node != NULL) && (parent_level->node != NULL) )
				{
					node->attachToParent( parent_level->node, gIOFireWirePlane );
				}
			}
           	else
           	{
				for (scanNodeNum = i + 1; scanNodeNum <= fRootNodeID; scanNodeNum++)
				{
					// Get speed code between parent and scan node.
					IOFWSpeed fwspeed = FWSpeed(parentNodeNum, scanNodeNum);
					if ( fwspeed > 0xFF )
						ErrorLog("Found oversized speed map entry during speed checking\n");
					
					UInt8 scanSpeedCode = (UInt8)fwspeed;
					
					UInt8 calcSpeedCode = scanSpeedCode;
					// Set speed map entry to minimum of scan speed and node's speed.
					if ( (speedCode & ~kFWSpeedUnknownMask) < (scanSpeedCode & ~kFWSpeedUnknownMask) )
					{
						calcSpeedCode = speedCode;
					}
					
					if( (speedCode & kFWSpeedUnknownMask) || (scanSpeedCode & kFWSpeedUnknownMask) )
					{
						calcSpeedCode |= kFWSpeedUnknownMask;
					}
					
					setNodeSpeed(i, scanNodeNum, calcSpeedCode);
				}
			}
        }
		
        // Find next child port.
        if (i > 0) 
		{
            while (level->childrenRemaining == 0) 
			{
                // Go up one level in tree.
                level--;
                if(level < scanList) 
				{
                    IOLog("SelfIDs don't build a proper tree (missing selfIDS?)!!\n");
                    return;
                }
                // One less child to scan.
                level->childrenRemaining--;
            }
            // Go down one level in tree.
            level++;
            if(level - scanList > maxDepth) 
			{
                maxDepth = level - scanList;
            }
        }
    }

	// Clear out the unknown speed mask for the local node. Not needed once we get here.
	// Other nodes with this flag will get cleared once we've decided to speed scan them.
	// We never speed scan the local node which means we'll never clear it otherwise.
	setNodeSpeed(fLocalNodeID, fLocalNodeID, (FWSpeed(fLocalNodeID, fLocalNodeID) & ~kFWSpeedUnknownMask));		

#if (DEBUGGING_LEVEL > 0)
	IOLog("MaxDepth:%d LocalNodeID:%x\n", maxDepth, fLocalNodeID);
	IOLog("FireWire Speed map:\n");
	for(i=0; i <= fRootNodeID; i++) {
		int j;
		for(j=0; j <= fRootNodeID; j++) {
			IOLog("%-2x ", (unsigned int)FWSpeed(i, j) );
		}
		IOLog("\n");
	}
#endif
#if 0
    if( doFWPlane ) 
	{
        IOLog( "FireWire Hop Counts:\n" );
        for( i=0; i <= fRootNodeID; i++ ) 
		{
            int j;
            for( j=0; j <= fRootNodeID; j++ ) 
			{
				if ( j < i )
					IOLog("_ ");
				else
					IOLog("%-2lu ", hopCount(i,j));
            }
            IOLog( "\n" );
        }
    }
#endif

    // Finally attach the full topology into the IOKit registry
    if(doFWPlane)
        root->attachToParent(IORegistryEntry::getRegistryRoot(), gIOFireWirePlane);
}

// updatePlane
//
//

void IOFireWireController::updatePlane()
{
    OSIterator *childIterator;
	 bool foundTDM = false;
	 
	fDevicePruneDelay = kNormalDevicePruneDelay;

    childIterator = getClientIterator();
    if( childIterator ) 
	{
        OSObject *child;
        while( (child = childIterator->getNextObject())) 
		{
            IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);

			// don't need to sync with open/close routines when checking for kNotTerminated
			if( found && (found->getTerminationState() == kNotTerminated) )
            {
				if( found->fNodeID == kFWBadNodeID )  
				{
					terminateDevice( found );
				}
				else
				{
					OSString * tdm_string = OSDynamicCast( OSString, found->getProperty( gFireWireTDM ) );
					
					if( (tdm_string != NULL) && 
						(strncmp( "PPC", tdm_string->getCStringNoCopy(), 4 ) == 0) )
					{
						foundTDM = true;
					}
				}
			}
        }
        childIterator->release();
    }

	OSObject * existProp = fFWIM->getProperty( "FWDSLimit" );
	
	if( existProp && !foundTDM )
	{
		fFWIM->setLinkMode( kIOFWSetDSLimit, 0 );
		
		// Make sure medicine takes effect
		resetBus();
	}
	
    buildTopology(true);
	
	messageClients( kIOFWMessageTopologyChanged );
	
	// reset generation property to current FireWire Generation
	char busGenerationStr[32];
	snprintf(busGenerationStr, sizeof(busGenerationStr), "%lx", fBusGeneration);
	setProperty( kFireWireGenerationID, busGenerationStr);
	FWKLOG(("IOFireWireController::updatePlane reset generation to '%s'\n", busGenerationStr));
	
	fUseHalfSizePackets = fRequestedHalfSizePackets;
}

// terminateDevice
//
//

void IOFireWireController::terminateDevice( IOFireWireDevice * device )
{
	if( device->isOpen() )
	{
		//IOLog( "IOFireWireController : message request close device object %p\n", found);
		// send our custom requesting close message
		device->lockForArbitration();
		device->setTerminationState( kNeedsTermination );
		device->unlockForArbitration();
		messageClient( kIOFWMessageServiceIsRequestingClose, device );
	}
	else
	{	
		device->lockForArbitration();
		device->setTerminationState( kTerminated );
		device->unlockForArbitration();
		IOFireWireDevice::terminateDevice( device );
	}
}

#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// physical access
//

// setPhysicalAccessMode
//
//

void IOFireWireController::setPhysicalAccessMode( IOFWPhysicalAccessMode mode )
{
	closeGate();

	//
	// enable physical access in normal security mode
	//
	
	if( mode == kIOFWPhysicalAccessEnabled &&
		getSecurityMode() == kIOFWSecurityModeNormal )
	{
		fPhysicalAccessMode = kIOFWPhysicalAccessEnabled;

		FWKLOG(( "IOFireWireController::setPhysicalAccessMode - enable physical access\n" ));

		//
		// disabling physical accesses will have cleared all previous filter state
		//
		// when physical access is enabled we mimic the filter configuration process 
		// after a bus reset. we let each node enable its physical filter if desired
		//
		
		OSIterator * iterator = getClientIterator();
		OSObject * child = NULL;
		while( (child = iterator->getNextObject()) ) 
		{
			IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
			
			// don't need to sync with open/close routines when checking for kNotTerminated
			if( found && (found->getTerminationState() == kNotTerminated) )
			{
				// if we found an active device, ask it to reconfigure it's
				// physical filter settings
				found->configurePhysicalFilter();
			}
		}
		
		iterator->release();
	}
	
	//
	// disable physical access
	//
	
	if( mode == kIOFWPhysicalAccessDisabled )
	{
		fPhysicalAccessMode = kIOFWPhysicalAccessDisabled;

		FWKLOG(( "IOFireWireController::setPhysicalAccessMode - disable physical access\n" ));
			
		// shut them all down!
		fFWIM->setNodeIDPhysicalFilter( kIOFWAllPhysicalFilters, false );
	}
	
	//
	// disable physical access for this bus generation if physical access
	// is not already permanently disabled
	//
	
	if( mode == kIOFWPhysicalAccessDisabledForGeneration &&
		fPhysicalAccessMode != kIOFWPhysicalAccessDisabled )
	{
		fPhysicalAccessMode = kIOFWPhysicalAccessDisabledForGeneration;

		FWKLOG(( "IOFireWireController::setPhysicalAccessMode - disable physical access for this bus generation\n" ));
	
		// shut them all down!
		fFWIM->setNodeIDPhysicalFilter( kIOFWAllPhysicalFilters, false );
	}
	
	openGate();
}

// getPhysicalAccessMode
//
//

IOFWPhysicalAccessMode IOFireWireController::getPhysicalAccessMode( void )
{
	return fPhysicalAccessMode;
}

// physicalAccessProcessBusReset
//
//

void IOFireWireController::physicalAccessProcessBusReset( void )
{
	//
	// reenable physical access if it was only disabled for a generation
	// and we're in normal security mode
	//
	
	if( fPhysicalAccessMode == kIOFWPhysicalAccessDisabledForGeneration && 
		getSecurityMode() == kIOFWSecurityModeNormal )
	{
		fPhysicalAccessMode = kIOFWPhysicalAccessEnabled;

		FWKLOG(( "IOFireWireController::physicalAccessProcessBusReset - re-enable physical access because of bus reset\n" ));
		
		// we don't reconfigure the physical filters here because :
		// 1. a bus reset has just occured and all node ids are set to kFWBadNodeID
		// 2. reconfiguring filters is done automatically after receiving self-ids
	}
}

// setNodeIDPhysicalFilter
//
//

void IOFireWireController::setNodeIDPhysicalFilter( UInt16 nodeID, bool state )
{
	// only configure node filters if the family is allowing physical access
	if( fPhysicalAccessMode == kIOFWPhysicalAccessEnabled )
	{
		FWKLOG(( "IOFireWireController::setNodeIDPhysicalFilter - set physical access for node 0x%x to %d\n", nodeID, state ));

		fFWIM->setNodeIDPhysicalFilter( nodeID, state );
	}
}

// isPhysicalAccessEnabledForNodeID
//
//

bool IOFireWireController::isPhysicalAccessEnabledForNodeID( UInt16 nodeID )
{
	return fFWIM->isPhysicalAccessEnabledForNodeID( nodeID );	
}
	
#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// security
//

// findKeyswitchDevice
//
//

IOService *IOFireWireController::findKeyswitchDevice( void )
{
    IORegistryIterator * iter;
    IORegistryEntry *    entry = 0;

    iter = IORegistryIterator::iterateOver( gIODTPlane,
                                            kIORegistryIterateRecursively );

    if ( iter )
    {
        while (( entry = iter->getNextObject() ))
		{
			if (( strncmp( entry->getName(), "keySwitch-gpio", 16 ) == 0 ) or
				( strncmp( entry->getName(), "keySwitch", 11  ) == 0 ) or
				( strncmp( entry->getName(), "KYLK", 5 ) == 0 ) )
                break;
		}
        iter->release();
    }
	
    return OSDynamicCast( IOService, entry );
}

// initSecurity
//
//
void IOFireWireController::initSecurity( void )
{
	bool	waitForKeyswitch	= false;
	
	if( findKeyswitchDevice() )
		waitForKeyswitch = true;
		
	//
	// assume security mode is normal
	//
				
	IOFWSecurityMode mode = kIOFWSecurityModeNormal;
				
	//
	// check OpenFirmware security mode
	//
	
	{
		char matchPath[32];	// IODeviceTree:/:options
		OSDictionary * optionsMatchDictionary = IOOFPathMatching( "/options", matchPath, 32 ); // need to release
		
		mach_timespec_t t = { 10, 0 };	//wait 10 secs
		IOService * options = IOService::waitForService( optionsMatchDictionary, &t );	// consumes dict ref, don't release options
		
		if( options != NULL )
		{
			OSString * securityModeProperty = OSDynamicCast( OSString, options->getProperty("security-mode") );
	
			if( securityModeProperty != NULL && strncmp( "none", securityModeProperty->getCStringNoCopy(), 5 ) != 0 )
			{
				// set security mode to secure/permanent
				mode = kIOFWSecurityModeSecurePermanent;
			}
		}
		else
		{			
			ErrorLog("FireWire unable to determine security-mode; defaulting to full-secure.\n");
			// turn security on because we can't determine security-mode
			mode = kIOFWSecurityModeSecurePermanent;
		}
	}
	
	//
	// handle secruity keyswitch
	//
	
	if( mode != kIOFWSecurityModeSecurePermanent )
	{

		//
		// check state of secruity keyswitch
		//
		UInt8	retryCount = 5;
		
		do
		{
			OSIterator *	iterator		= NULL;
			OSBoolean *		keyswitchState	= NULL;
				
			iterator = getMatchingServices( nameMatching("AppleKeyswitch") );
			if( iterator != NULL )
			{
				OSObject * obj = NULL;
				waitForKeyswitch = false;

				if( (obj = iterator->getNextObject()) )
				{
					IOService *	service = (IOService*)obj;
					keyswitchState = OSDynamicCast( OSBoolean, service->getProperty( "Keyswitch" ) );
					
					if( keyswitchState->isTrue() )
					{
						// set security mode to secure
						mode = kIOFWSecurityModeSecure;
					}
				}
				
				iterator->release();
				iterator = NULL;
			}

			if( retryCount == 0 )
				waitForKeyswitch = false;

			retryCount--;

			if( waitForKeyswitch )
			{
				IOLog("Waiting for AppleKeyswitch ...\n");
				IOSleep(1000);
			}

		}while( waitForKeyswitch );
		
		//
		// add notification for changes to secruity keyswitch
		//
		
		
		fKeyswitchNotifier = addNotification( gIOMatchedNotification, nameMatching( "AppleKeyswitch" ),
											  (IOServiceNotificationHandler)&IOFireWireController::serverKeyswitchCallback,
											  this, 0 );
		
	}

	//
	// now that we've determined our security mode, set it
	//
	
	setSecurityMode( mode );

}

// freeSecurity
//
//

void IOFireWireController::freeSecurity( void )
{
	// remove notification
										  
	if( fKeyswitchNotifier != NULL )
	{
		fKeyswitchNotifier->remove();
		fKeyswitchNotifier = NULL;
	}
}

// serverKeyswitchCallback
//
//

bool IOFireWireController::serverKeyswitchCallback( void * target, void * refCon, IOService * service )
{
	OSBoolean *				keyswitchState	= NULL;
	IOFireWireController *	me				= NULL;
	
	keyswitchState = OSDynamicCast( OSBoolean, service->getProperty( "Keyswitch" ) );
	
	me = OSDynamicCast( IOFireWireController, (OSObject *)target );
	
	if( keyswitchState != NULL && me != NULL )
	{
		// Is the key unlocked?
		
		if( keyswitchState->isFalse() )
		{
			// Key is unlocked, set security mode to normal
			
			me->setSecurityMode( kIOFWSecurityModeNormal );
		}
		else if( keyswitchState->isTrue() )
		{
			// Key is locked, set security mode to secure
	
			me->setSecurityMode( kIOFWSecurityModeSecure );
		}
		
	}
	
	return true;	
}

// setSecurityMode
//
//

void IOFireWireController::setSecurityMode( IOFWSecurityMode mode )
{
	closeGate();

	fSecurityMode = mode;
	
	switch( fSecurityMode )
	{
		case kIOFWSecurityModeNormal:
			
			FWKLOG(( "IOFireWireController::setSecurityMode - set mode to normal\n" ));
	
			// enable physical access
			fFWIM->setSecurityMode( mode );
			setPhysicalAccessMode( kIOFWPhysicalAccessEnabled );
			break;
			
		case kIOFWSecurityModeSecure:
		case kIOFWSecurityModeSecurePermanent:
		
			FWKLOG(( "IOFireWireController::setSecurityMode - set mode to secure\n" ));
	
			// disable physical access
			fFWIM->setSecurityMode( mode );
			setPhysicalAccessMode( kIOFWPhysicalAccessDisabled );
			break;
			
		default:
			IOLog( "IOFireWireController::setSecurityMode - illegal security mode = 0x%x\n", fSecurityMode );
			break;
	}
	
	openGate();
}

// getSecurityMode
//
//

IOFWSecurityMode IOFireWireController::getSecurityMode( void )
{
	return fSecurityMode;
}

#pragma mark -

/////////////////////////////////////////////////////////////////////////////
// local config rom
//

// getRootDir
//
//

IOLocalConfigDirectory * IOFireWireController::getRootDir() const 
{ 
	return fRootDir; 
}

// AddUnitDirectory
//
//

IOReturn IOFireWireController::AddUnitDirectory(IOLocalConfigDirectory *unitDir)
{
    IOReturn res;
    
	closeGate();
	
	if ( isInactive() )
	{
		openGate() ;
		return kIOReturnOffline ;
	}
    
	getRootDir()->addEntry(kConfigUnitDirectoryKey, unitDir);

    res = UpdateROM();
    if(res == kIOReturnSuccess)
        res = resetBus();
    
	openGate();
    
	return res;
}

// RemoveUnitDirectory
//
//

IOReturn IOFireWireController::RemoveUnitDirectory(IOLocalConfigDirectory *unitDir)
{
    IOReturn res;
    
	closeGate();
    
	getRootDir()->removeSubDir(unitDir);

    res = UpdateROM();
    if(res == kIOReturnSuccess)
        res = resetBus();
    
	openGate();
    
	return res;
}

// UpdateROM()
//
//   Instantiate the local Config ROM.
//   Always causes at least one bus reset.

IOReturn IOFireWireController::UpdateROM()
{
    UInt32 *				hack;
    UInt32 					crc;
    unsigned int 			numQuads;
    OSData *				rom;
    IOReturn				ret;
    UInt32					generation;
    IOFireWireLocalNode *	localNode;

    // Increment the 4 bit generation field, make sure it is at least two.
	UInt32 bib_quad = OSSwapBigToHostInt32( fROMHeader[2] );
    generation = bib_quad & kFWBIBGeneration;
    generation += (1 << kFWBIBGenerationPhase);
    generation &= kFWBIBGeneration;
    if(generation < (2 << kFWBIBGenerationPhase))
        generation = (2 << kFWBIBGenerationPhase);

    fROMHeader[2] = OSSwapHostToBigInt32((bib_quad & ~kFWBIBGeneration) | generation);
    
    rom = OSData::withBytes(&fROMHeader, sizeof(fROMHeader));
	fRootDir->incrementGeneration();
	fRootDir->compile(rom);

    // Now hack in correct CRC and length.
    hack = (UInt32 *)rom->getBytesNoCopy();
    UInt32 bibQuads = sizeof(fROMHeader)/sizeof(UInt32) - 1;
    crc = FWComputeCRC16 (hack + 1, bibQuads);
    *hack = OSSwapHostToBigInt32(
				(((sizeof(fROMHeader)/sizeof(UInt32)-1) << kConfigBusInfoBlockLengthPhase) & kConfigBusInfoBlockLength) |
		        ((bibQuads << kConfigROMCRCLengthPhase) & kConfigROMCRCLength) |
				((crc << kConfigROMCRCValuePhase) & kConfigROMCRCValue) );

    localNode = getLocalNode(this);
    if(localNode)
        localNode->setProperty(gFireWireROM, rom);
    
    numQuads = rom->getLength()/sizeof(UInt32) - 1;
	
#if 0
    {
        unsigned int i;
        IOLog("--------- FW Local ROM: --------\n");
        for(i=0; i<numQuads+1; i++)
            IOLog("ROM[%d] = 0x%x\n", i, OSSwapBigToHostInt32(hack[i]));
    }
#endif

    if(fROMAddrSpace) 
	{
        freeAddress( fROMAddrSpace );
        fROMAddrSpace->release();
        fROMAddrSpace = NULL;
    }
 
    fROMAddrSpace = IOFWPseudoAddressSpace::simpleReadFixed( this,
        FWAddress(kCSRRegisterSpaceBaseAddressHi, kConfigROMBaseAddress),
        (numQuads+1)*sizeof(UInt32), rom->getBytesNoCopy());
    ret = allocAddress(fROMAddrSpace);
    if(kIOReturnSuccess == ret) 
	{
        ret = fFWIM->updateROM(rom);
    }
    rom->release();
    return ret ;
}

#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// async request transmit
//

// allocTrans
//
//

AsyncPendingTrans *IOFireWireController::allocTrans(IOFWAsyncCommand *cmd)
{
    return allocTrans( cmd, NULL );
}

// allocTrans
//
//

AsyncPendingTrans *IOFireWireController::allocTrans( IOFWAsyncCommand * cmd, IOFWCommand * altcmd )
{
    unsigned int i;
    unsigned int tran;

    tran = fLastTrans;
    for(i=0; i<kMaxPendingTransfers; i++) {
        AsyncPendingTrans *t;
        tran++;
        if(tran >= kMaxPendingTransfers)
            tran = 0;
        t = &fTrans[tran];
        if(!t->fInUse) {
            t->fHandler = cmd;
			t->fAltHandler = altcmd;
            t->fInUse = true;
            t->fTCode = tran;
            fLastTrans = tran;
            return t;
        }
    }
	
	// Print only if its a first time
	if ( fOutOfTLabels == 0 && fOutOfTLabelsThreshold == 0 )
		IOLog("IOFireWireController:: Out of Transaction Labels\n");
		
	// Out of TLabels counter (Information Only)
	fOutOfTLabels++;
	
    return NULL;
}

// freeTrans
//
//

void IOFireWireController::freeTrans(AsyncPendingTrans *trans)
{
    // No lock needed - can't have two users of a tcode.
    trans->fHandler = NULL;
	trans->fAltHandler = NULL;
    trans->fInUse = false;
}

// asyncRead
//
//

IOReturn IOFireWireController::asyncRead(	UInt32 				generation, 
											UInt16 				nodeID, 
											UInt16 				addrHi, 
											UInt32 				addrLo,
											int 				speed, 
											int 				label, 
											int 				size, 
											IOFWAsyncCommand *	cmd)
{
	return asyncRead(	generation,
						nodeID,
						addrHi,
						addrLo,
						speed,
						label,
						size,
						cmd,
						kIOFWReadFlagsNone );
}

// asyncRead
//
//

// Route packet sending to FWIM if checks out OK
IOReturn IOFireWireController::asyncRead(	UInt32 				generation, 
											UInt16 				nodeID, 
											UInt16 				addrHi, 
											UInt32 				addrLo,
											int 				speed, 
											int 				label, 
											int 				size, 
											IOFWAsyncCommand *	cmd,
											IOFWReadFlags		flags )
{
    if( !checkGeneration(generation) ) 
	{
        return kIOFireWireBusReset;
    }

    // Check if local node

    if( nodeID == fLocalNodeID ) 
	{
        UInt32 rcode;
        IOMemoryDescriptor *buf;
        IOByteCount offset;
        IOFWSpeed temp = (IOFWSpeed)speed;
        
		rcode = doReadSpace(	nodeID, 
								temp, 
								FWAddress(addrHi, addrLo), 
								size,
								&buf, 
								&offset,
								NULL,
								(IOFWRequestRefCon)label );
								
        if(rcode == kFWResponseComplete)
		{
			void * bytes = IOMalloc( size );
			
			buf->readBytes( offset, bytes, size );
            
			cmd->gotPacket( rcode, bytes, size );
			
			IOFree( bytes, size );
        }
		else
        {
		    cmd->gotPacket( rcode, NULL, 0 );
        }
		
		return kIOReturnSuccess;
    }
    else
	{
		// reliabilty is more important than speed for IRM access
		// perform IRM access at s100
		
		int actual_speed = speed;
		if( addrHi == kCSRRegisterSpaceBaseAddressHi )
        {
			if( (addrLo == kCSRBandwidthAvailable) ||
				(addrLo == kCSRChannelsAvailable31_0) ||
				(addrLo == kCSRChannelsAvailable63_32) ||
				(addrLo == kCSRBusManagerID) )
			{
				actual_speed = kFWSpeed100MBit;
			}
		}
		
        return fFWIM->asyncRead( nodeID, addrHi, addrLo, actual_speed, label, size, cmd, flags );
	}
}

// asyncWrite
//
// DEPRECATED

IOReturn IOFireWireController::asyncWrite(	UInt32 				generation, 
											UInt16 				nodeID, 
											UInt16 				addrHi, 
											UInt32 				addrLo,
											int 				speed, 
											int 				label, 
											void *				data, 
											int 				size, 
											IOFWAsyncCommand *	cmd)
{
	IOLog( "IOFireWireController::asyncWrite : DEPRECATED API\n" );
	return kIOReturnUnsupported;
}

// asyncWrite
//
//

IOReturn IOFireWireController::asyncWrite(	UInt32 					generation, 
											UInt16 					nodeID, 
											UInt16 					addrHi, 
											UInt32 					addrLo,
											int 					speed, 
											int 					label, 
											IOMemoryDescriptor *	buf, 
											IOByteCount 			offset,
											int 					size, 
											IOFWAsyncCommand *		cmd )
{
	return asyncWrite(	generation,
						nodeID,
						addrHi,
						addrLo,
						speed,
						label,
						buf,
						offset,
						size,
						cmd,
						kIOFWWriteFlagsNone );
}

// asyncWrite
//
//

IOReturn IOFireWireController::asyncWrite(	UInt32 					generation, 
											UInt16 					nodeID, 
											UInt16 					addrHi, 
											UInt32 					addrLo,
											int 					speed, 
											int 					label, 
											IOMemoryDescriptor *	buf, 
											IOByteCount 			offset,
											int 					size, 
											IOFWAsyncCommand *		cmd,
											IOFWWriteFlags 			flags )
{
//	IOLog( "IOFireWireController::asyncWrite\n" );

    if( !checkGeneration(generation) ) 
	{
        return kIOFireWireBusReset;
    }

    // Check if local node
    if( nodeID == fLocalNodeID ) 
	{
        UInt32 rcode;
        IOFWSpeed temp = (IOFWSpeed)speed;
        
		void * bytes = IOMalloc( size );
			
		buf->readBytes( offset, bytes, size );
            
		rcode = doWriteSpace(	nodeID, 
								temp, 
								FWAddress( addrHi, addrLo ), 
								size,
								bytes, 
								(IOFWRequestRefCon)label );
        
		IOFree( bytes, size );
			
		cmd->gotPacket(rcode, NULL, 0);
        
		return kIOReturnSuccess;
    }
    else
	{
		// reliabilty is more important than speed for IRM access
		// perform IRM access at s100
	
		// actually writes to the IRM are not allowed, so we are doing 
		// this more for consistency than necessity
		
		int actual_speed = speed;
		if( addrHi == kCSRRegisterSpaceBaseAddressHi )
        {
			if( (addrLo == kCSRBandwidthAvailable) ||
				(addrLo == kCSRChannelsAvailable31_0) ||
				(addrLo == kCSRChannelsAvailable63_32) ||
				(addrLo == kCSRBusManagerID) )
			{
				actual_speed = kFWSpeed100MBit;
			}
		}
		
        return fFWIM->asyncWrite( 	nodeID, 
									addrHi, 
									addrLo, 
									actual_speed, 
									label, 
									buf, 
									offset, 
									size, 
									cmd,
									flags );
	}
}

// asyncPHYPacket
//
//

IOReturn IOFireWireController::asyncPHYPacket( UInt32 generation, UInt32 data, UInt32 data2, IOFWAsyncPHYCommand * cmd )
{
	IOReturn status = kIOReturnSuccess;
	
    if( !checkGeneration(generation) ) 
	{
        status = kIOFireWireBusReset;
    }
	
	if( status == kIOReturnSuccess )
	{
		if( !(data & 0x40000000) )
		{
			// not VersaPHY
			status = kIOReturnBadArgument;
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		status = fFWIM->asyncPHYPacket( data, data2, cmd );
	}
	
	return status;
}

// asyncLock
//
// DEPRECATED

IOReturn IOFireWireController::asyncLock(	UInt32 				generation, 
											UInt16 				nodeID, 
											UInt16 				addrHi, 
											UInt32 				addrLo,
											int 				speed, 
											int 				label, 
											int 				type, 
											void *				data, 
											int 				size, 
											IOFWAsyncCommand *	cmd )
{
	IOLog( "IOFireWireController::asyncLock : DEPRECATED API\n" );
	return kIOReturnUnsupported;
}

// asyncLock
//
//

IOReturn IOFireWireController::asyncLock(	UInt32 					generation, 
											UInt16 					nodeID, 
											UInt16 					addrHi, 
											UInt32 					addrLo,
											int 					speed, 
											int 					label, 
											int 					type, 
											IOMemoryDescriptor *	buf, 
											IOByteCount 			offset,
											int 					size, 
											IOFWAsyncCommand *		cmd )
					
{
    if( !checkGeneration(generation) ) 
	{
        return kIOFireWireBusReset;
    }

    // Check if local node
    if( nodeID == fLocalNodeID ) 
	{
        UInt32 rcode;
        UInt32 retVals[2];
        UInt32 retSize = size / 2;
        
		IOFWSpeed temp = (IOFWSpeed)speed;
        IOFWRequestRefCon refcon = (IOFWRequestRefCon)(label | kRequestIsLock | (type << kRequestExtTCodeShift));
        
		void * bytes = IOMalloc( size );
			
		buf->readBytes( offset, bytes, size );
            
		rcode = doLockSpace(	nodeID, 
								temp, 
								FWAddress(addrHi, addrLo), 
								size,
								(const UInt32*)bytes, 
								retSize, 
								retVals, 
								type, 
								refcon );
		
		IOFree( bytes, size );
								
        cmd->gotPacket( rcode, retVals, retSize );
        
		return kIOReturnSuccess;
    }
    else
	{
		// reliabilty is more important than speed for IRM access
		// perform IRM access at s100
		
		int actual_speed = speed;
		if( addrHi == kCSRRegisterSpaceBaseAddressHi )
        {
			if( (addrLo == kCSRBandwidthAvailable) ||
				(addrLo == kCSRChannelsAvailable31_0) ||
				(addrLo == kCSRChannelsAvailable63_32) ||
				(addrLo == kCSRBusManagerID) )
			{
				actual_speed = kFWSpeed100MBit;
			}
		}
		
		return fFWIM->asyncLock(	nodeID, 
									addrHi, 
									addrLo, 
									actual_speed, 
									label, 
									type, 
									buf,
									offset,
									size, 
									cmd );
	}
}

// handleAsyncTimeout
//
//

IOReturn IOFireWireController::handleAsyncTimeout(IOFWAsyncCommand *cmd)
{
    return fFWIM->handleAsyncTimeout(cmd);
}


// handleAsyncCompletion
//
//

IOReturn IOFireWireController::handleAsyncCompletion( IOFWCommand *cmd, IOReturn status )
{
    return fFWIM->handleAsyncCompletion( cmd, status );
}

// asyncStreamWrite
//
//

IOReturn IOFireWireController::asyncStreamWrite(UInt32 generation,
                    int speed, int tag, int sync, int channel,
                    IOMemoryDescriptor *buf, IOByteCount offset,
                	int size, IOFWAsyncStreamCommand *cmd)
{
    if(!checkGeneration(generation)) {
        return (kIOFireWireBusReset);
    }

	return fFWIM->asyncStreamTransmit((UInt32)channel, speed, (UInt32) sync, (UInt32) tag, buf, offset, size, cmd);
}

// createAsyncStreamCommand
//
//

IOFWAsyncStreamCommand * IOFireWireController::createAsyncStreamCommand( UInt32 generation,
    			UInt32 channel, UInt32 sync, UInt32 tag, IOMemoryDescriptor *hostMem,
    			UInt32 size, int speed, FWAsyncStreamCallback completion, void *refcon)
{
    IOFWAsyncStreamCommand * cmd;

    cmd = new IOFWAsyncStreamCommand;
    if(cmd) {
        if(!cmd->initAll(this, generation, channel, sync, tag, hostMem,size,speed,
                         completion, refcon)) {
            cmd->release();
            cmd = NULL;
		}
    }
    return cmd;
}

// createAsyncPHYCommand
//
//

IOFWAsyncPHYCommand * IOFireWireController::createAsyncPHYCommand(	UInt32				generation,
																	UInt32				data1, 
																	UInt32				data2, 
																	FWAsyncPHYCallback	completion, 
																	void *				refcon, 
																	bool 				failOnReset )

{
    IOFWAsyncPHYCommand * cmd;

    cmd = new IOFWAsyncPHYCommand;
    if( cmd ) 
	{
        if( !cmd->initAll( this, generation, data1, data2, completion, refcon, failOnReset ) ) 
		{
            cmd->release();
            cmd = NULL;
		}
    }
	
    return cmd;
}

/////////////////////////////////////////////////////////////////////////////
// async receive
//

// processRcvPacket
//
// dispatch received Async packet based on tCode.

void IOFireWireController::processRcvPacket(UInt32 *data, int size, IOFWSpeed speed )
{
#if 0
    int i;
kprintf("Received packet 0x%x size %d\n", data, size);
    for(i=0; i<size; i++) {
	kprintf("0x%x ", OSSwapBigToHostInt32(data[i]));
    }
    kprintf("\n");
#endif
    UInt32	tCode, tLabel;
    UInt32	quad0;
    UInt16	sourceID;
    UInt16	destID;

    // Get first quad.
    quad0 = *data;

    tCode = (quad0 & kFWPacketTCode) >> kFWPacketTCodePhase;
    tLabel = (quad0 & kFWAsynchTLabel) >> kFWAsynchTLabelPhase;
    sourceID = (data[1] & kFWAsynchSourceID) >> kFWAsynchSourceIDPhase;
	destID = (data[0] & kFWAsynchDestinationID) >> kFWAsynchDestinationIDPhase;

    // Dispatch processing based on tCode.
    switch (tCode)
    {
        case kFWTCodeWriteQuadlet :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteQuadlet: addr 0x%x -> 0x%x:0x%x:0x%x\n", sourceID, destID,
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[3], 4, speed);
            break;

		case kFWTCodePHYPacket:
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG( "PHY Packet: 0x%08lx %08lx\n", data[1], data[2] );
#endif
			processPHYPacket( data[1], data[2] );
			break;
			
        case kFWTCodeWriteBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteBlock: addr 0x%x -> 0x%x:0x%x:0x%x\n", sourceID, destID,
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase, speed);
            break;

        case kFWTCodeWriteResponse :
            if(fTrans[tLabel].fHandler) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
				FWAddress commandAddress = cmd->getAddress();
				
            	if( sourceID == commandAddress.nodeID ){
            		cmd->setResponseSpeed( speed );
					cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase, 0, 0);
				}
				else{
#if (DEBUGGING_LEVEL > 0)
					DEBUGLOG( "Response from wrong node ID!\n" );
#endif
				}
            }
            else {
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteResponse: label %d isn't in use!!, data1 = 0x%x\n",
                     tLabel, data[1]);
#endif
            }
            break;

        case kFWTCodeReadQuadlet :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("ReadQuadlet: addr 0x%x -> 0x%x:0x%x:0x%x\n", sourceID, destID, 
		(data[1] & kFWAsynchDestinationOffsetHigh) >>
                     kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            {
                UInt32 ret;
                FWAddress addr((data[1] & kFWAsynchDestinationOffsetHigh) >>  kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOMemoryDescriptor *buf = NULL;
				IOByteCount offset;
                
				ret = doReadSpace(sourceID, speed, addr, 4,
                                    &buf, &offset, NULL, (IOFWRequestRefCon)(tLabel | kRequestIsQuad));
               
                if(ret == kFWResponsePending)
                    break;
                
				if( NULL != buf ) 
				{
					UInt32 quad = OSSwapHostToBigInt32(0xdeadbeef);
						
					buf->readBytes( offset, &quad, 4 );
					
					if ( destID != 0xffff )	// we should not respond to broadcast reads
						fFWIM->asyncReadQuadResponse(sourceID, speed, tLabel, ret, quad );
					else
						DebugLog("Skipped asyncReadQuadResponse because destID=0x%x\n", destID);
                }
                else 
				{
                    if ( destID != 0xffff )	// we should not respond to broadcast reads
						fFWIM->asyncReadQuadResponse(sourceID, speed, tLabel, ret, OSSwapHostToBigInt32(0xdeadbeef));
					else
						DebugLog("Skipped asyncReadQuadResponse because destID=0x%x\n", destID);
                }
            }
            break;

        case kFWTCodeReadBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("ReadBlock: addr 0x%x -> 0x%x:0x%x:0x%x\n", sourceID, destID, 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2],
		(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);
#endif
            {
                IOReturn ret;
                int 					length = (data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase ;
                FWAddress 	addr((data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOMemoryDescriptor *	buf = NULL;
				IODMACommand * dma_command = NULL;
				IOByteCount offset;
				
                ret = doReadSpace(sourceID, speed, addr, length, &buf, &offset, &dma_command, (IOFWRequestRefCon)(tLabel));

                if(ret == kFWResponsePending)
                    break;
					
				if(NULL != buf) {
                    if ( destID != 0xffff )	// we should not respond to broadcast reads
						fFWIM->asyncReadResponse(sourceID, speed, tLabel, ret, buf, offset, length, dma_command );
					else
						DebugLog("Skipped asyncReadResponse because destID=0x%x\n", destID);
                }
                else {
                    if ( destID != 0xffff )	// we should not respond to broadcast reads
						fFWIM->asyncReadResponse(sourceID, speed, tLabel, ret, fBadReadResponse, 0, 4, NULL);
					else
						DebugLog("Skipped asyncReadResponse because destID=0x%x\n", destID);
                }
            }
            break;

        case kFWTCodeReadQuadletResponse :
            if(fTrans[tLabel].fHandler) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
				FWAddress commandAddress = cmd->getAddress();
				
            	if( sourceID == commandAddress.nodeID )
            	{
            		cmd->setResponseSpeed( speed );
            	
					cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
										(const void*)(data+3), 4);
				}
				else
				{
#if (DEBUGGING_LEVEL > 0)
					DEBUGLOG( "Response from wrong node ID!\n" );
#endif
				}
				
            }
            else {
#if (DEBUGGING_LEVEL > 0)
		DEBUGLOG("ReadQuadletResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeReadBlockResponse :
        case kFWTCodeLockResponse :
            if(fTrans[tLabel].fHandler) {
            	
				IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
				FWAddress commandAddress = cmd->getAddress();
				
            	if( sourceID == commandAddress.nodeID )
            	{
            		cmd->setResponseSpeed( speed );
            	
					cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
					(const void*)(data+4), (data[3] & kFWAsynchDataLength)>>kFWAsynchDataLengthPhase);
				}
				else
				{
#if (DEBUGGING_LEVEL > 0)
					DEBUGLOG( "Response from wrong node ID!\n" );
#endif
				}
            }
            else {
#if (DEBUGGING_LEVEL > 0)
				DEBUGLOG("ReadBlock/LockResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeLock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Lock type %d: addr 0x%x -> 0x%x:0x%x:0x%x\n", 
		(data[3] & kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase, sourceID, destID,
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase,
		data[2]);
#endif
            processLockRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase, speed);

            break;

        case kFWTCodeIsochronousBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Async Stream Packet\n");
#endif
            break;

        default :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Unexpected tcode in Asyncrecv: %d\n", tCode);
#endif
            break;
    }	
}

/////////////////////////////////////////////////////////////////////////////
// async request receive
//

// createPhysicalAddressSpace
//
//

IOFWPhysicalAddressSpace *
IOFireWireController::createPhysicalAddressSpace(IOMemoryDescriptor *mem)
{
    IOFWPhysicalAddressSpace *space;
    space = new IOFWPhysicalAddressSpace;
    if(!space)
        return NULL;
    if(!space->initWithDesc(this, mem)) {
        space->release();
        space = NULL;
    }
    return space;
}

// createAsyncStreamListener
//
//

IOFWAsyncStreamListener *
IOFireWireController::createAsyncStreamListener( UInt32 channel, FWAsyncStreamReceiveCallback proc, void *refcon )
{
    IOFWAsyncStreamListener * listener = new IOFWAsyncStreamListener;
	
    if( listener )
	{
	    if(not listener->initAll( this, channel, proc, refcon )) 
		{
			listener->release();
			listener = NULL;
		}
	}
	
    return listener;
}

void
IOFireWireController::removeAsyncStreamListener(IOFWAsyncStreamListener *listener)
{
	IOFWAsyncStreamReceiver *receiver = listener->getReceiver();
	
	receiver->removeListener(listener);
}

// createPseudoAddressSpace
//
//

IOFWPseudoAddressSpace *
IOFireWireController::createPseudoAddressSpace(FWAddress *addr, UInt32 len,
                            FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    IOFWPseudoAddressSpace *space;
    space = new IOFWPseudoAddressSpace;
    if(!space)
        return NULL;
    if(!space->initAll(this, addr, len, reader, writer, refcon)) {
        space->release();
        space = NULL;
    }
    return space;
}

// createInitialAddressSpace
//
//

IOFWPseudoAddressSpace *
IOFireWireController::createInitialAddressSpace(UInt32 addressLo, UInt32 len,
                            FWReadCallback reader, FWWriteCallback writer, void *refcon)
{
    IOFWPseudoAddressSpace *space;
    space = new IOFWPseudoAddressSpace;
    if(!space)
        return NULL;
    if(!space->initFixed(this, FWAddress(kCSRRegisterSpaceBaseAddressHi, addressLo),
            len, reader, writer, refcon)) {
        space->release();
        space = NULL;
    }
    return space;
}

// getAddressSpace
//
//

IOFWAddressSpace *
IOFireWireController::getAddressSpace(FWAddress address)
{
    closeGate();
    
	IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        if(found->contains(address))
            break;
    }
    
	openGate();
    
	return found;
}


// allocAsyncStreamReceiver
//
//
IOFWAsyncStreamReceiver * 
IOFireWireController::allocAsyncStreamReceiver(UInt32	channel, FWAsyncStreamReceiveCallback clientProc, void	*refcon)
{
	closeGate();

    IOFWAsyncStreamReceiver * receiver = new IOFWAsyncStreamReceiver;
	
    if( receiver )
	{
		if( receiver->initAll( this, channel ) ) 
		{
			if( fLocalAsyncStreamReceivers->setObject( receiver ))
				receiver->release();
		}
		else
		{
			receiver->release();
			receiver = NULL;
		}
	}
			
	openGate();
    
	return receiver;
}

// getAsyncStreamReceiver
//
//
IOFWAsyncStreamReceiver *
IOFireWireController::getAsyncStreamReceiver( UInt32 channel )
{
    closeGate();
    
	IOFWAsyncStreamReceiver * found;
    fAsyncStreamReceiverIterator->reset();
    while( (found = (IOFWAsyncStreamReceiver *) fAsyncStreamReceiverIterator->getNextObject())) {
        if(found->listens(channel))
            break;
    }
    
	openGate();
    
	return found;
}

// removeAsyncStreamReceiver
//
//
void
IOFireWireController::removeAsyncStreamReceiver( IOFWAsyncStreamReceiver *receiver )
{
    closeGate();

	fLocalAsyncStreamReceivers->removeObject(receiver);
    
	openGate();
}

// activateAsyncStreamReceivers
//
//
void
IOFireWireController::activateAsyncStreamReceivers( )
{
    closeGate();
    
	IOFWAsyncStreamReceiver * found;
    fAsyncStreamReceiverIterator->reset();
    while( (found = (IOFWAsyncStreamReceiver *) fAsyncStreamReceiverIterator->getNextObject())) 
        found->activate( getBroadcastSpeed() );
    
	openGate();
}

// deactivateAsyncStreamReceivers
//
//
void
IOFireWireController::deactivateAsyncStreamReceivers( )
{
    closeGate();
    
	IOFWAsyncStreamReceiver * found;
    fAsyncStreamReceiverIterator->reset();
    while( (found = (IOFWAsyncStreamReceiver *) fAsyncStreamReceiverIterator->getNextObject())) 
        found->deactivate();
    
	openGate();
}

// freeAllAsyncStreamReceiver
//
//
void 
IOFireWireController::freeAllAsyncStreamReceiver()
{
    closeGate();
    
	IOFWAsyncStreamReceiver * found;
    fAsyncStreamReceiverIterator->reset();
    while( (found = (IOFWAsyncStreamReceiver *) fAsyncStreamReceiverIterator->getNextObject())) 
		fLocalAsyncStreamReceivers->removeObject(found);
	
	openGate();
}

// allocAddress
//
//

IOReturn IOFireWireController::allocAddress(IOFWAddressSpace *space)
{
    /*
     * Lots of scope for optimizations here, perhaps building a hash table for
     * addresses etc.
     * Drivers may want to override this if their hardware can match addresses
     * without CPU intervention.
     */
    IOReturn result = kIOReturnSuccess;
    
	closeGate();
 
	// enforce exclusivity
	IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) 
	{
		// if either of the conflicting address spaces wants to be exclusive
		if( space->isExclusive() || found->isExclusive() )
		{
			// check if they intersect
			if( found->intersects( space ) )
			{
				// we have a problem
				result = kIOReturnExclusiveAccess;
			}
		}
	}
	
	if( result == kIOReturnSuccess )
	{
		if(!fLocalAddresses->setObject(space))
			result = kIOReturnNoMemory;
		else
			result = kIOReturnSuccess;
    }
	
	openGate();
    
	return result;
}

// freeAddress
//
//

void IOFireWireController::freeAddress(IOFWAddressSpace *space)
{
    closeGate();
	
	fLocalAddresses->removeObject(space);
	
	openGate();
}

// allocatePseudoAddress
//
//

IOReturn IOFireWireController::allocatePseudoAddress(FWAddress *addr, UInt32 lenDummy)
{
    unsigned int i, len;
    UInt8 * data;
    UInt8 used = 1;
    
    closeGate();
    
    if( fAllocatedAddresses == NULL ) 
    {
        fAllocatedAddresses = OSData::withCapacity(4);	// SBP2 + some spare
        fAllocatedAddresses->appendBytes(&used, 1);	// Physical always allocated
    }
    
    if( !fAllocatedAddresses )
    {   
        openGate();
        return kIOReturnNoMemory;
    }
    
    len = fAllocatedAddresses->getLength();
    data = (UInt8*)fAllocatedAddresses->getBytesNoCopy();
    for( i=0; i<len; i++ ) 
    {
        if( data[i] == 0 ) 
        {
            data[i] = 1;
            addr->addressHi = i;
            addr->addressLo = 0;
        
            openGate();
            return kIOReturnSuccess;
        }
    }
    
    if( len >= 0xfffe )
    {
        openGate();
		return kIOReturnNoMemory;
    }
    
    if( fAllocatedAddresses->appendBytes(&used, 1)) 
    {
        addr->addressHi = len;
        addr->addressLo = 0;
    
        openGate();
        return kIOReturnSuccess;
    }

    openGate();
    return kIOReturnNoMemory;      
}

// freePseudoAddress
//
//

void IOFireWireController::freePseudoAddress(FWAddress addr, UInt32 lenDummy)
{
    unsigned int len;
    UInt8 * data;
    
    closeGate();
    
    assert( fAllocatedAddresses != NULL);
    
    len = fAllocatedAddresses->getLength();
    assert(addr.addressHi < len);
    data = (UInt8*)fAllocatedAddresses->getBytesNoCopy();
    assert(data[addr.addressHi]);
    data[addr.addressHi] = 0;
    
    openGate();
}

// processWriteRequest
//
// process quad and block writes.

void IOFireWireController::processWriteRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len, IOFWSpeed speed)
{
    UInt32 ret = kFWResponseAddressError;
    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);
    IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doWrite(sourceID, speed, addr, len, buf, (IOFWRequestRefCon)tLabel);
        if(ret != kFWResponseAddressError)
            break;
    }
	
    if ( ((hdr[0] & kFWAsynchDestinationID) >> kFWAsynchDestinationIDPhase) != 0xffff )	// we should not respond to broadcast writes
		fFWIM->asyncWriteResponse(sourceID, speed, tLabel, ret, addr.addressHi);
	else
		DebugLog("Skipped asyncWriteResponse because destID=0x%lx\n", ((hdr[0] & kFWAsynchDestinationID) >> kFWAsynchDestinationIDPhase));
}

// processLockRequest
//
// process 32 and 64 bit locks.

void IOFireWireController::processLockRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len, IOFWSpeed speed)
{
    UInt32 oldVal[2];
    UInt32 ret;
    UInt32 outLen =sizeof(oldVal);
    int type = (hdr[3] &  kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase;

    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);

    IOFWRequestRefCon refcon = (IOFWRequestRefCon)(tLabel | kRequestIsLock | (type << kRequestExtTCodeShift));

    ret = doLockSpace(sourceID, speed, addr, len, (const UInt32 *)buf, outLen, oldVal, type, refcon);
    if(ret != kFWResponsePending)
    {
        if ( ((hdr[0] & kFWAsynchDestinationID) >> kFWAsynchDestinationIDPhase) != 0xffff )	// we should not respond to broadcast locks
			fFWIM->asyncLockResponse(sourceID, speed, tLabel, ret, type, oldVal, outLen);
		else
			DebugLog("Skipped asyncLockResponse because destID=0x%lx\n", ((hdr[0] & kFWAsynchDestinationID) >> kFWAsynchDestinationIDPhase));
    }
}

// doReadSpace
//
//

UInt32 IOFireWireController::doReadSpace(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                                IOMemoryDescriptor **buf, IOByteCount * offset, IODMACommand **dma_command,
                                                IOFWRequestRefCon refcon)
{
    IOFWAddressSpace * found;
    UInt32 ret = kFWResponseAddressError;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doRead(nodeID, speed, addr, len, buf, offset,
                            refcon);
        if(ret != kFWResponseAddressError)
            break;
    }

	// hack to pass the IODMACommand for the phys address space to the FWIM
	
	if( (dma_command != NULL) && (ret != kFWResponseAddressError) )
	{
		IOFWPhysicalAddressSpace * phys_space = OSDynamicCast( IOFWPhysicalAddressSpace, found );
		
		if( phys_space )
		{
			*dma_command = phys_space->getDMACommand();
		}
	}
	
    return ret;
}

// doWriteSpace
//
//

UInt32 IOFireWireController::doWriteSpace(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                            const void *buf, IOFWRequestRefCon refcon)
{
    IOFWAddressSpace * found;
    UInt32 ret = kFWResponseAddressError;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doWrite(nodeID, speed, addr, len, buf, refcon);
        if(ret != kFWResponseAddressError)
            break;
    }
    return ret;
}

// doLockSpace
//
//

UInt32 IOFireWireController::doLockSpace(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 inLen,
                                         const UInt32 *newVal,  UInt32 &outLen, UInt32 *oldVal, UInt32 type,
                                                IOFWRequestRefCon refcon)
{
    IOFWAddressSpace * found;
    UInt32 ret = kFWResponseAddressError;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doLock(nodeID, speed, addr, inLen, newVal, outLen, oldVal, type, refcon);
        if(ret != kFWResponseAddressError)
            break;
    }

    if(ret != kFWResponseComplete) {
        oldVal[0] = OSSwapHostToBigInt32(0xdeadbabe);
        outLen = 4;
    }
    return ret;
}

// handleARxReqIntComplete
//
//

void IOFireWireController::handleARxReqIntComplete( void )
{
    IOFWAddressSpace * found;

    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject()) ) 
	{
		IOFWPseudoAddressSpace * space = OSDynamicCast( IOFWPseudoAddressSpace, found );
		if( space != NULL )
		{
			space->handleARxReqIntComplete();
		}
    }
}

// isLockRequest
//
//

bool IOFireWireController::isLockRequest(IOFWRequestRefCon refcon)
{
    return ((UInt32)refcon) & kRequestIsLock;
}

// isQuadRequest
//
//

bool IOFireWireController::isQuadRequest(IOFWRequestRefCon refcon)
{
    return ((UInt32)refcon) & kRequestIsQuad;
}

// isCompleteRequest
//
//

bool IOFireWireController::isCompleteRequest(IOFWRequestRefCon refcon)
{
    return ((UInt32)refcon) & kRequestIsComplete;
}

// getExtendedTCode
//
//

UInt32 IOFireWireController::getExtendedTCode(IOFWRequestRefCon refcon)
{
    return((UInt32)refcon & kRequestExtTCodeMask) >> kRequestExtTCodeShift;
}

/////////////////////////////////////////////////////////////////////////////
// async response transmit
//

// asyncReadResponse
//
// Send async read response packets
// useful for pseudo address spaces that require servicing outside the FireWire work loop.

IOReturn IOFireWireController::asyncReadResponse(	UInt32 					generation, 
													UInt16 					nodeID, 
													int 					speed,
													IOMemoryDescriptor *	buf, 
													IOByteCount 			offset, 
													int 					size,
													IOFWRequestRefCon 		refcon )
{
    IOReturn result;
    UInt32 params = (UInt32)refcon;
    UInt32 label = params & kRequestLabel;

    closeGate();
    
	if( !checkGeneration(generation) )
	{
        result = kIOFireWireBusReset;
    }
	else if( params & kRequestIsQuad )
	{
		UInt32 quad = OSSwapHostToBigInt32(0xdeadbeef);
								
		buf->readBytes( offset, &quad, 4 );

		result = fFWIM->asyncReadQuadResponse(	nodeID, 
												speed, 
												label, 
												kFWResponseComplete,
												quad );
    }
	else
    {
	    result = fFWIM->asyncReadResponse(	nodeID, 
											speed, 
											label, 
											kFWResponseComplete, 
											buf, 
											offset, 
											size,
											NULL );
    }
	
	openGate();

    return result;
}

// asyncLockResponse
//
// Send async lock response packets
// useful for pseudo address spaces that require servicing outside the FireWire work loop.

IOReturn IOFireWireController::asyncLockResponse( 	UInt32 					generation, 
													UInt16 					nodeID, 
													int 					speed,
													IOMemoryDescriptor *	buf, 
													IOByteCount 			offset, 
													int 					size,
													IOFWRequestRefCon 		refcon )
{
    IOReturn result;
    UInt32 params = (UInt32)refcon;
    UInt32 label = params & kRequestLabel;

    closeGate();
    
	if( !checkGeneration(generation) )
	{
        result = kIOFireWireBusReset;
    }
	else
    {	
		void * bytes = IOMalloc( size );
			
		buf->readBytes( offset, bytes, size );
    
		result = fFWIM->asyncLockResponse(	nodeID, 
											speed, 
											label, 
											kFWResponseComplete, 
											getExtendedTCode(refcon), 
											bytes, 
											size );

		IOFree( bytes, size );

    }
    
    openGate();

    return result;
}

/////////////////////////////////////////////////////////////////////////////
// timer command
//

// createDelayedCmd
//
//

IOFWDelayCommand * IOFireWireController::createDelayedCmd(UInt32 uSecDelay, FWBusCallback func, void *refcon)
{
    IOFWDelayCommand *delay;
    //IOLog("Creating delay of %d\n", uSecDelay);
    delay = new IOFWDelayCommand;
    if(!delay)
        return NULL;

    if(!delay->initWithDelay(this, uSecDelay, func, refcon)) 
	{
		delay->release();
        return NULL;
    }
	
    return delay;
}

#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// isoch
//

// createIsochChannel
//
//

IOFWIsochChannel *IOFireWireController::createIsochChannel(	bool 		doIRM, 
															UInt32 		bandwidth, 
															IOFWSpeed 	prefSpeed,
															FWIsochChannelForceStopNotificationProc	stopProc, 
															void *		stopRefCon )
{
	// NOTE: if changing this code, must also change IOFireWireUserClient::isochChannelAllocate()

    IOFWIsochChannel *channel;

    channel = new IOFWIsochChannel;
    if(!channel)
	{
		return NULL;
	}
	
    if( !channel->init(this, doIRM, bandwidth, prefSpeed, stopProc, stopRefCon) ) 
	{
		channel->release();
		channel = NULL;
    }
	
    return channel;
}

// createLocalIsochPort
//
//

IOFWLocalIsochPort *IOFireWireController::createLocalIsochPort(	bool 			talking,
																DCLCommand *	opcodes, 
																DCLTaskInfo *	info,
																UInt32 			startEvent, 
																UInt32 			startState, 
																UInt32 			startMask )
{
    IOFWLocalIsochPort *port;
    IODCLProgram *program;
	
    program = fFWIM->createDCLProgram( talking, opcodes, info, startEvent, startState, startMask );
    if(!program)
		return NULL;

    port = new IOFWLocalIsochPort;
    if( !port ) 
	{
		program->release();
		return NULL;
    }

    if(!port->init(program, this)) 
	{
		port->release();
		port = NULL;
    }

    return port;
}

// addAllocatedChannel
//
//

void IOFireWireController::addAllocatedChannel(IOFWIsochChannel *channel)
{
    closeGate();
    
	fAllocatedChannels->setObject(channel);
    
	openGate();
}

// removeAllocatedChannel
//
//

void IOFireWireController::removeAllocatedChannel(IOFWIsochChannel *channel)
{
    closeGate();
    
	fAllocatedChannels->removeObject(channel);
    
	openGate();
}

// addIRMAllocation
//
//

void IOFireWireController::addIRMAllocation(IOFireWireIRMAllocation *irmAllocation)
{
    closeGate();
    
	fIRMAllocations->setObject(irmAllocation);
    
	openGate();
}

// removeIRMAllocation
//
//

void IOFireWireController::removeIRMAllocation(IOFireWireIRMAllocation *irmAllocation)
{
    closeGate();
    
	fIRMAllocations->removeObject(irmAllocation);
    
	openGate();
}

// allocateIRMBandwidthInGeneration
//
//
IOReturn IOFireWireController::allocateIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation)
{
	IOReturn res;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	IOFWCompareAndSwapCommand * fLockCmd;
	FWAddress addr;
	UInt32 expectedOldVal, newVal; 
	UInt32 actualOldVal;
	bool lockSuccessful;
	UInt32 retries = 2; 
	
	// Get the current IRM and Generation
	getIRMNodeID(irmGeneration, irmNodeID);
	
	// Check the current generation 
	if (irmGeneration != generation)
		return kIOFireWireBusReset;
	
	// Initialize the address for the current IRM's bandwidth available register
	addr.nodeID = irmNodeID;
	addr.addressHi = 0xFFFF;
	addr.addressLo = 0xF0000220;
	
	// Start with the default, no-bandwidth allocated value for the old val!
	expectedOldVal = OSSwapHostToBigInt32(0x00001333);	
	
	// Create a compare/swap command
	fLockCmd = new IOFWCompareAndSwapCommand;
	if (!fLockCmd)
		return kIOReturnNoMemory;
	
	// Pre-initialize the compare/swap command. Reinit will take place before use!
	if (!fLockCmd->initAll(this,generation,addr, NULL, NULL,0,NULL,NULL))
	{
		fLockCmd->release();
		return kIOReturnError;
	}
	
	while (retries > 0)
	{
		// Make sure, in the request, we don't set the newVal to a negative number
		if (bandwidthUnits > OSSwapBigToHostInt32(expectedOldVal))
		{
			res = kIOFireWireIsochBandwidthNotAvailable;
			break;
		}
		
		// Calculate the new val for the compare/swap command
		newVal = OSSwapHostToBigInt32(OSSwapBigToHostInt32(expectedOldVal) - bandwidthUnits);
		
		// Reinitialize the compare/swap command. If this fails, bail out of the retry loop!
		res = fLockCmd->reinit(generation,addr, &expectedOldVal, &newVal,1,NULL,NULL); 
		if (res != kIOReturnSuccess)
			break;
		
		// Submit the compare/swap command. Will not return until complete
		res = fLockCmd->submit();
		
		// Check results, including actualOldVal
		if (res == kIOReturnSuccess)
			lockSuccessful = fLockCmd->locked(&actualOldVal);
		else
			lockSuccessful = false;
		
		DebugLog("allocateIRMBandwidthInGeneration: res = 0x%08X, expectedOldVal = 0x%08X, newVal = 0x%08X, lockSuccessful = %d, actualOldVal = 0x%08X\n",
			  res, OSSwapBigToHostInt32(expectedOldVal),OSSwapBigToHostInt32(newVal),lockSuccessful,OSSwapBigToHostInt32(actualOldVal));
		
		// If we got a bus reset (i.e. wrong generation), no point on retrying
		if (res == kIOFireWireBusReset)
			break;
		
		// If we succeeded, we don't need to retry	
		else if (lockSuccessful)
			break;
		
		else
		{
			// Decrement the retry count
			retries -= 1;
			
			// If we don't have an error, but we're here, it's because
			// the compare-swap didn't succeed. Set an error code, in case we're
			// out of retries.
			if (res == kIOReturnSuccess)
				res = kIOFireWireIsochBandwidthNotAvailable;
			
			// Change our expected old value for the retry.	
			expectedOldVal = actualOldVal;
		}
	}
	
	// Release the lock command
	fLockCmd->release();
	
	return res;
}

// releaseIRMBandwidthInGeneration
//
//
IOReturn IOFireWireController::releaseIRMBandwidthInGeneration(UInt32 bandwidthUnits, UInt32 generation)
{
	IOReturn res;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	IOFWCompareAndSwapCommand * fLockCmd;
	FWAddress addr;
	UInt32 expectedOldVal, newVal; 
	UInt32 actualOldVal;
	bool lockSuccessful;
	UInt32 retries = 2; 
	
	// Get the current IRM and Generation
	getIRMNodeID(irmGeneration, irmNodeID);
	
	// Check the current generation 
	if (irmGeneration != generation)
		return kIOFireWireBusReset;
	
	// Initialize the address for the current IRM's bandwidth available register
	addr.nodeID = irmNodeID;
	addr.addressHi = 0xFFFF;
	addr.addressLo = kCSRBandwidthAvailable;
	
	// Start with the default, no-bandwidth available value for the old val!
	expectedOldVal = OSSwapHostToBigInt32(0);	
	
	// Create a compare/swap command
	fLockCmd = new IOFWCompareAndSwapCommand;
	if (!fLockCmd)
		return kIOReturnNoMemory;
	
	// Pre-initialize the compare/swap command. Reinit will take place before use!
	if (!fLockCmd->initAll(this,generation,addr, NULL, NULL,0,NULL,NULL))
	{
		fLockCmd->release();
		return kIOReturnError;
	}
	
	while (retries > 0)
	{

#if 0		
		// Make sure, in the request, we don't set an illegally large value
		if ((bandwidthUnits + OSSwapBigToHostInt32(expectedOldVal)) > 0x00001333 )
		{
			res = kIOReturnNoResources;
			break;
		}
#endif
		
		// Calculate the new val for the compare/swap command
		newVal = OSSwapHostToBigInt32(OSSwapBigToHostInt32(expectedOldVal) + bandwidthUnits);
		
		// Reinitialize the compare/swap command. If this fails, bail out of the retry loop!
		res = fLockCmd->reinit(generation,addr, &expectedOldVal, &newVal,1,NULL,NULL); 
		if (res != kIOReturnSuccess)
			break;
		
		// Submit the compare/swap command. Will not return until complete
		res = fLockCmd->submit();
		
		// Check results, including actualOldVal
		if (res == kIOReturnSuccess)
			lockSuccessful = fLockCmd->locked(&actualOldVal);
		else
			lockSuccessful = false;
		
		DebugLog("releaseIRMBandwidthInGeneration: res = 0x%08X, expectedOldVal = 0x%08X, newVal = 0x%08X, lockSuccessful = %d, actualOldVal = 0x%08X\n",
		res, OSSwapBigToHostInt32(expectedOldVal),OSSwapBigToHostInt32(newVal),lockSuccessful,OSSwapBigToHostInt32(actualOldVal));
		
		// If we got a bus reset (i.e. wrong generation), no point on retrying
		if (res == kIOFireWireBusReset)
			break;
		
		// If we succeeded, we don't need to retry	
		else if (lockSuccessful)
			break;
		
		else
		{
			// Decrement the retry count
			retries -= 1;
			
			// If we don't have an error, but we're here, it's because
			// the compare-swap didn't succeed. Set an error code, in case we're
			// out of retries.
			if (res == kIOReturnSuccess)
				res = kIOReturnNoResources;
			
			// Change our expected old value for the retry.	
			expectedOldVal = actualOldVal;
		}
	}
	
	// Release the lock command
	fLockCmd->release();
	
	return res;
}

// allocateIRMChannelInGeneration
//
//
IOReturn IOFireWireController::allocateIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation)
{
	IOReturn res;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	IOFWCompareAndSwapCommand * fLockCmd;
	FWAddress addr;
	UInt32 newValMask;
	UInt32 expectedOldVal, newVal; 
	UInt32 actualOldVal;
	bool lockSuccessful;
	UInt32 retries = 2; 
	
	// Get the current IRM and Generation
	getIRMNodeID(irmGeneration, irmNodeID);
	
	// Check the current generation 
	if (irmGeneration != generation)
		return kIOFireWireBusReset;
	
	// Initialize the address for the current IRM's bandwidth available register
	addr.nodeID = irmNodeID;
	addr.addressHi = 0xFFFF;
	
	if( isochChannel < 32 )
	{
		addr.addressLo = kCSRChannelsAvailable31_0;
		newValMask = OSSwapHostToBigInt32(~(1<<(31 - isochChannel)));
	}
	else if( isochChannel < 64 )
	{
		addr.addressLo = kCSRChannelsAvailable63_32;
		newValMask = OSSwapHostToBigInt32(~(1 << (63 - isochChannel)));
	}
	else
		return kIOReturnBadArgument;
	
	// Start with the default, no channels allocated value for the old val!
	expectedOldVal = OSSwapHostToBigInt32(0xFFFFFFFF);	
	
	// Create a compare/swap command
	fLockCmd = new IOFWCompareAndSwapCommand;
	if (!fLockCmd)
		return kIOReturnNoMemory;
	
	// Pre-initialize the compare/swap command. Reinit will take place before use!
	if (!fLockCmd->initAll(this,generation,addr, NULL, NULL,0,NULL,NULL))
	{
		fLockCmd->release();
		return kIOReturnError;
	}
	
	while (retries > 0)
	{
		// Make sure the channel is not already allocated
		if ((expectedOldVal & ~newValMask) == 0)
		{
			res = kIOFireWireChannelNotAvailable;
			break;
		}
		
		// Calculate the new val for the compare/swap command
		newVal = expectedOldVal & newValMask;
		
		// Reinitialize the compare/swap command. If this fails, bail out of the retry loop!
		res = fLockCmd->reinit(generation,addr, &expectedOldVal, &newVal,1,NULL,NULL); 
		if (res != kIOReturnSuccess)
			break;
		
		// Submit the compare/swap command. Will not return until complete
		res = fLockCmd->submit();
		
		// Check results, including actualOldVal
		if (res == kIOReturnSuccess)
			lockSuccessful = fLockCmd->locked(&actualOldVal);
		else
			lockSuccessful = false;
		
		DebugLog("allocateIRMChannelInGeneration: res = 0x%08X, expectedOldVal = 0x%08X, newVal = 0x%08X, lockSuccessful = %d, actualOldVal = 0x%08X\n",
		res, OSSwapBigToHostInt32(expectedOldVal),OSSwapBigToHostInt32(newVal),lockSuccessful,OSSwapBigToHostInt32(actualOldVal));
		
		// If we got a bus reset (i.e. wrong generation), no point on retrying
		if (res == kIOFireWireBusReset)
			break;
		
		// If we succeeded, we don't need to retry	
		else if (lockSuccessful)
			break;
		
		else
		{
			// Decrement the retry count
			retries -= 1;
			
			// If we don't have an error, but we're here, it's because
			// the compare-swap didn't succeed. Set an error code, in case we're
			// out of retries.
			if (res == kIOReturnSuccess)
				res = kIOFireWireChannelNotAvailable;
			
			// Change our expected old value for the retry.	
			expectedOldVal = actualOldVal;
		}
	}
	
	// Release the lock command
	fLockCmd->release();
	
	return res;
}

// releaseIRMChannelInGeneration
//
//
IOReturn IOFireWireController::releaseIRMChannelInGeneration(UInt8 isochChannel, UInt32 generation)
{
	IOReturn res;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	IOFWCompareAndSwapCommand * fLockCmd;
	FWAddress addr;
	UInt32 newValMask;
	UInt32 expectedOldVal, newVal; 
	UInt32 actualOldVal;
	bool lockSuccessful;
	UInt32 retries = 2; 
	
	// Get the current IRM and Generation
	getIRMNodeID(irmGeneration, irmNodeID);
	
	// Check the current generation 
	if (irmGeneration != generation)
		return kIOFireWireBusReset;
	
	// Initialize the address for the current IRM's bandwidth available register
	addr.nodeID = irmNodeID;
	addr.addressHi = 0xFFFF;
	
	if( isochChannel < 32 )
	{
		addr.addressLo = kCSRChannelsAvailable31_0;
		newValMask = OSSwapHostToBigInt32(1<<(31 - isochChannel));
	}
	else if( isochChannel < 64 )
	{
		addr.addressLo = kCSRChannelsAvailable63_32;
		newValMask = OSSwapHostToBigInt32(1 << (63 - isochChannel));
	}
	else
		return kIOReturnBadArgument;
	
	// Start with the default, all channels allocated value for the old val!
	expectedOldVal = OSSwapHostToBigInt32(0x00000000);	
	
	// Create a compare/swap command
	fLockCmd = new IOFWCompareAndSwapCommand;
	if (!fLockCmd)
		return kIOReturnNoMemory;
	
	// Pre-initialize the compare/swap command. Reinit will take place before use!
	if (!fLockCmd->initAll(this,generation,addr, NULL, NULL,0,NULL,NULL))
	{
		fLockCmd->release();
		return kIOReturnError;
	}
	
	while (retries > 0)
	{
		// Make sure the channel is not already free
		if ((expectedOldVal & newValMask) == newValMask)
		{
			res = kIOReturnNoResources;
			break;
		}
		
		// Calculate the new val for the compare/swap command
		newVal = expectedOldVal | newValMask;
		
		// Reinitialize the compare/swap command. If this fails, bail out of the retry loop!
		res = fLockCmd->reinit(generation,addr, &expectedOldVal, &newVal,1,NULL,NULL); 
		if (res != kIOReturnSuccess)
			break;
		
		// Submit the compare/swap command. Will not return until complete
		res = fLockCmd->submit();
		
		// Check results, including actualOldVal
		if (res == kIOReturnSuccess)
			lockSuccessful = fLockCmd->locked(&actualOldVal);
		else
			lockSuccessful = false;
		
		DebugLog("releaseIRMChannelInGeneration: res = 0x%08X, expectedOldVal = 0x%08X, newVal = 0x%08X, lockSuccessful = %d, actualOldVal = 0x%08X\n",
		res, OSSwapBigToHostInt32(expectedOldVal),OSSwapBigToHostInt32(newVal),lockSuccessful,OSSwapBigToHostInt32(actualOldVal));
		
		// If we got a bus reset (i.e. wrong generation), no point on retrying
		if (res == kIOFireWireBusReset)
			break;
		
		// If we succeeded, we don't need to retry	
		else if (lockSuccessful)
			break;
		
		else
		{
			// Decrement the retry count
			retries -= 1;
			
			// If we don't have an error, but we're here, it's because
			// the compare-swap didn't succeed. Set an error code, in case we're
			// out of retries.
			if (res == kIOReturnSuccess)
				res = kIOReturnNoResources;
			
			// Change our expected old value for the retry.	
			expectedOldVal = actualOldVal;
		}
	}
	
	// Release the lock command
	fLockCmd->release();
	
	return res;
}

// createIRMAllocation
//
//
IOFireWireIRMAllocation * IOFireWireController::createIRMAllocation(Boolean releaseIRMResourcesOnFree, 
											IOFireWireIRMAllocation::AllocationLostNotificationProc allocationLostProc,
											void *pLostNotificationProcRefCon)
{
	IOFireWireIRMAllocation * pIRMAllocation;
	pIRMAllocation = new IOFireWireIRMAllocation;
    if( !pIRMAllocation )
        return NULL;
    
	if( !pIRMAllocation->init(this,releaseIRMResourcesOnFree,allocationLostProc,pLostNotificationProcRefCon))
 	{
        pIRMAllocation->release();
        pIRMAllocation = NULL;
    }
	
	return pIRMAllocation;
}

#pragma mark -
////////////////////////////////////////////////////////////////////////////

// activatePHYPacketListener
//
//

IOReturn IOFireWireController::activatePHYPacketListener( IOFWPHYPacketListener * listener )
{
    IOReturn status = kIOReturnSuccess;
    
	closeGate();
 
	if( status == kIOReturnSuccess )
	{
		OSObject * prop = fFWIM->getProperty( "RcvPhyPkt" );
		if( prop )
		{
			UInt32 value = ((OSNumber*)prop)->unsigned32BitValue();
			if( value == 0 )
			{
				status = kIOReturnUnsupported;
			}
		}
	}
	
	if( status == kIOReturnSuccess )
	{
		if( !fPHYPacketListeners->setObject( listener ) )
		{
			status = kIOReturnNoMemory;
		}
	}
	
	openGate();
    
	return status;
}

// deactivatePHYPacketListener
//
//

void IOFireWireController::deactivatePHYPacketListener( IOFWPHYPacketListener * listener )
{
    closeGate();
	
	fPHYPacketListeners->removeObject( listener );
	
	openGate();
}

// processPHYPacket
//
// route incoming phy packets to all listeners

void IOFireWireController::processPHYPacket( UInt32 data1, UInt32 data2 )
{
	// only process versaphy packets
	if(  data1 & 0x40000000 )
	{
		IOFWPHYPacketListener * listener;
		fPHYPacketListenersIterator->reset();
		while( (listener = (IOFWPHYPacketListener *)fPHYPacketListenersIterator->getNextObject()) ) 
		{
			listener->processPHYPacket( data1, data2 );
		}
	}
}

// createPHYPacketListener
//
//

IOFWPHYPacketListener * 
IOFireWireController::createPHYPacketListener( FWPHYPacketCallback proc, void * refcon )
{
    IOFWPHYPacketListener * listener = IOFWPHYPacketListener::createWithController( this );	
    if( listener )
	{
		listener->setCallback( proc );
		listener->setRefCon( refcon );
	}
	
    return listener;
}

#pragma mark -
////////////////////////////////////////////////////////////////////////////

// getLocalNode
//
// static method to fetch the local node for a controller

IOFireWireLocalNode * IOFireWireController::getLocalNode(IOFireWireController *control)
{
    OSIterator *childIterator;
    IOFireWireLocalNode *localNode = NULL;
    childIterator = control->getClientIterator();
    if( childIterator) {
        OSObject * child;
        while( (child = childIterator->getNextObject())) {
            localNode = OSDynamicCast(IOFireWireLocalNode, child);
            if(localNode) {
                break;
            }
        }
        childIterator->release();
    }
    return localNode;
}

// getBusPowerManager
//
//

IOFireWirePowerManager * IOFireWireController::getBusPowerManager( void )
{
	return fBusPowerManager;
}

// getWorkLoop
//
//

IOWorkLoop * IOFireWireController::getWorkLoop() const
{
    return fWorkLoop;
}

// getLink
//
//

IOFireWireLink * IOFireWireController::getLink() const 
{ 
	return fFWIM;
}

// getCycleTime
//
//

IOReturn IOFireWireController::getCycleTime(UInt32 &cycleTime)
{
	IOReturn res;
    
	res = fFWIM->getCycleTime(cycleTime);
    
	return res;
}

// getCycleTimeAndUpTime
//
//

IOReturn IOFireWireController::getCycleTimeAndUpTime( UInt32 &cycleTime, UInt64 &uptime )
{
	IOReturn res;
    
	res = fFWIM->getCycleTimeAndUpTime( cycleTime, uptime );
        
	return res;
}

// getBusCycleTime
//
//

IOReturn IOFireWireController::getBusCycleTime(UInt32 &busTime, UInt32 &cycleTime)
{
    // Have to take workloop lock, in case the hardware is sleeping.
    IOReturn res;
    UInt32 cycleSecs;
    
	closeGate();
    
	res = fFWIM->getBusCycleTime(busTime, cycleTime);
    
	openGate();
    
	if(res == kIOReturnSuccess) {
        // Bottom 7 bits of busTime should be same as top 7 bits of cycle time.
        // However, link only updates bus time every few seconds,
        // so use cycletime for overlapping bits and check for cycletime wrap
        cycleSecs = cycleTime >> 25;
        // Update bus time.
        if((busTime & 0x7F) > cycleSecs) {
            // Must have wrapped, increment top part of busTime.
            cycleSecs += 0x80;
        }
        busTime = (busTime & ~0x7F) + cycleSecs;            
    }
    return res;
}

// hopCount
//
//

UInt32 IOFireWireController::hopCount(UInt16 nodeAAddress, UInt16 nodeBAddress )
{	
	nodeAAddress &= kFWMaxNodesPerBus;
	nodeBAddress &= kFWMaxNodesPerBus;
	
	UInt16 lowNode = nodeAAddress > nodeBAddress ? nodeBAddress : nodeAAddress;
	
    struct FWNodeScan
    {
        int nodeID;
        int childrenRemaining;
    };
	
    FWNodeScan scanList[fRootNodeID];
    FWNodeScan *level;
	level = scanList;
	
	const unsigned int maxNodes = fRootNodeID+1;
	UInt8 hopArray[maxNodes*(maxNodes+1)];
	bzero(hopArray, sizeof(hopArray));
	
	closeGate();
	
	// this is the same basic algorithm used in buildTopology
    int i;
	// stop once we've found the lower node because there's enough info in the hop count table at that point
	for( i = fRootNodeID; i >= lowNode; i-- )
	{
		// Add node to bottom of tree
		level->nodeID = i;
		level->childrenRemaining = countNodeIDChildren( i );
		
		if (i != fRootNodeID) 
		{
			int parentNodeNum, scanNodeNum;
			parentNodeNum = (level-1)->nodeID;
			for (scanNodeNum = i + 1; scanNodeNum <= fRootNodeID; scanNodeNum++)
			{
				UInt8 hops = hopArray[(maxNodes + 1)*parentNodeNum + scanNodeNum];
				
				// fill out entire table so we don't have to use extra math to use a triangular matrix
				hopArray[(maxNodes + 1)*i + scanNodeNum] = hops + 1;
				hopArray[(maxNodes + 1)*scanNodeNum + i] = hops + 1;
				//IOLog("Calc Hops %d-%d: %d dervied from %d-%d:%d\n", i, scanNodeNum, hops+1, parentNodeNum, scanNodeNum, hops);
			}
		}
		
		// Find next child port.
		if (i > 0) 
		{
			while (level->childrenRemaining == 0) 
			{
				// Go up one level in tree.
				level--;
				if(level < scanList) 
				{
					ErrorLog("FireWire: SelfIDs don't build a proper tree for hop counts (missing selfIDS?)!!\n");
					return 0xFFFFFFFF;	// this seems like the best thing to return here, impossibly large
				}
				// One less child to scan.
				level->childrenRemaining--;
			}
			// Go down one level in tree.
			level++;
		}
	}

	openGate();
	
	// since both sides of matrix are filled in, no special math is necessary
	return hopArray[(maxNodes + 1)*nodeAAddress + nodeBAddress];
}

// hopCount
//
//

UInt32 IOFireWireController::hopCount( UInt16 nodeAAddress )
{
	//closeGate();
	
	UInt32 hops = hopCount( nodeAAddress, fLocalNodeID );
	
	//openGate();
	
	return hops;
}

// FWSpeed
// Returns pre-determined speed between nodeAddress and localNode
//

IOFWSpeed IOFireWireController::FWSpeed(UInt16 nodeAddress) const
{
	return FWSpeed(nodeAddress, fLocalNodeID);
}

// FWSpeed
// Returns pre-determined speed between nodeA and nodeB
//

IOFWSpeed IOFireWireController::FWSpeed(UInt16 nodeA, UInt16 nodeB) const
{
	unsigned int txSpeed = kFWSpeedInvalid;
	nodeA &= kFWMaxNodesPerBus;
	nodeB &= kFWMaxNodesPerBus;
	
	if ( nodeA < kFWMaxNodesPerBus && nodeB < kFWMaxNodesPerBus )
	{
		if ( nodeA < nodeB )
			txSpeed = fSpeedVector[nodeA + ((nodeB * (nodeB + 1))/2)];
		else
			txSpeed = fSpeedVector[nodeB + ((nodeA * (nodeA + 1))/2)];
	}
	else
	{
		// Get the broadcast speed.
		// there's only one "broadcast speed", so looking for the slowest speed between
		// the localNode and all others is sufficient for the speed between any node
		// and the "broadcast node"
		unsigned int remoteNodeSpeed = kFWSpeedInvalid;
		int i;
		
		for ( i=0; i <= fRootNodeID; i++ )
		{
			remoteNodeSpeed = (unsigned int)FWSpeed(fLocalNodeID, i);
			//IOLog("IOFireWireController::FWSpeed(%x,%x) getting broadcast speed %x-%x=0x%x\n", nodeA, nodeB, fLocalNodeID, i, remoteNodeSpeed);
			txSpeed = txSpeed > remoteNodeSpeed ? remoteNodeSpeed : txSpeed;
		}
	}
	//DebugLog("IOFireWireController::FWSpeed( %d, %d ) = %d\n", nodeA, nodeB, txSpeed);
	
	return (IOFWSpeed)txSpeed;
}

// setNodeSpeed
//
//

void IOFireWireController::setNodeSpeed( UInt16 nodeA, UInt16 nodeB, UInt8 speed )
{
	nodeA &= kFWMaxNodesPerBus;
	nodeB &= kFWMaxNodesPerBus;
	
	if ( nodeA < kFWMaxNodesPerBus && nodeB < kFWMaxNodesPerBus )
	{
		if ( nodeA < nodeB )
			fSpeedVector[nodeA + ((nodeB * (nodeB + 1))/2)] = speed;
		else
			fSpeedVector[nodeB + ((nodeA * (nodeA + 1))/2)] = speed;
	}
	//IOLog("setNodeSpeed( A:%d, B:%d, Speed:0x%x)\n", nodeA, nodeB, speed);
}

void IOFireWireController::setNodeSpeed( UInt16 nodeAddress, UInt8 speed )
{
	setNodeSpeed(nodeAddress, fLocalNodeID, speed);
}

void IOFireWireController::setNodeSpeed( UInt16 nodeAddress, IOFWSpeed speed )
{
	if ( speed > 0xFF )
		ErrorLog("FireWire: Trying to set speed map entry larger than 8 bits.\n");
	
	setNodeSpeed(nodeAddress, fLocalNodeID, (UInt8)speed);
}

// maxPackLog
//
// How big (as a power of two) can packets sent to/received from the node be?

int IOFireWireController::maxPackLog(bool forSend, UInt16 nodeAddress) const
{
    int log;
	
    log = 9+FWSpeed(nodeAddress);
    if( forSend ) 
	{
        if( log > fMaxSendLog )
		{
            log = fMaxSendLog;
		}
	}
    else 
	{
		if( log > fMaxSendLog )
		{
			log = fMaxRecvLog;
		}
	}
	
	if( fUseHalfSizePackets )
	{
		if( log > 1 )
		{
			log--;
		}
	}
	
	return log;
}

// maxPackLog
//
// How big (as a power of two) can packets sent from A to B be?

int IOFireWireController::maxPackLog(UInt16 nodeA, UInt16 nodeB) const
{
    return 9+FWSpeed(nodeA, nodeB);
}

// nodeIDtoDevice
//
//

IOFireWireDevice * IOFireWireController::nodeIDtoDevice(UInt32 generation, UInt16 nodeID)
{
    OSIterator *childIterator;
    IOFireWireDevice * found = NULL;

    if(!checkGeneration(generation))
    {
	    return NULL;
    }

    childIterator = getClientIterator();

    if( childIterator) 
	{
        OSObject *child;
        while( (child = childIterator->getNextObject())) 
		{
            found = OSDynamicCast(IOFireWireDevice, child);
			
            // don't need to sync with open/close routines when checking for kNotTerminated
			if( found && (found->getTerminationState() == kNotTerminated) && found->fNodeID == nodeID )
			{
				break;
			}
		}
        childIterator->release();
    }
    return found;
}

// getGeneration
//
//

UInt32 IOFireWireController::getGeneration() const 
{
	return fBusGeneration;
}

// checkGeneration
//
//

bool IOFireWireController::checkGeneration(UInt32 gen) const 
{
	return gen == fBusGeneration;
}


// getLocalNodeID
//
//

UInt16 IOFireWireController::getLocalNodeID() const 
{
	return fLocalNodeID;
}

// getIRMNodeID
//
//

IOReturn IOFireWireController::getIRMNodeID(UInt32 &generation, UInt16 &id)
{
	closeGate();
 
	generation = fBusGeneration; 
	id = fIRMNodeID; 

	openGate();

	return kIOReturnSuccess;
}

// clipMaxRec2K
//
//

IOReturn IOFireWireController::clipMaxRec2K( Boolean clipMaxRec )
{
    IOReturn res;

	closeGate();
    
	//IOLog("IOFireWireController::clipMaxRec2K\n");

	res = fFWIM->clipMaxRec2K(clipMaxRec);
    
	openGate();
	
	return res;
}

// makeRoot
//
//

IOReturn IOFireWireController::makeRoot(UInt32 generation, UInt16 nodeID)
{
    IOReturn res = kIOReturnSuccess;
    nodeID &= 63;
    
	closeGate();
    
	if(!checkGeneration(generation))
        res = kIOFireWireBusReset;
    else if( fRootNodeID != nodeID ) {
        // Send phy packet to set root hold off bit for node
        res = fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                    (nodeID << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR);
        if(kIOReturnSuccess == res)
		{
	//		IOLog( "IOFireWireController::makeRoot resetBus\n" );
            res = resetBus();
		}
	}
    
    openGate();

    return res;
}

// nodeMustBeRoot
//
//
void IOFireWireController::nodeMustBeRoot( UInt32 nodeID )
{
	fForcedRootNodeID	= nodeID;
	fNodeMustBeRootFlag = true;
}

// nodeMustNotBeRoot
//
//
void IOFireWireController::nodeMustNotBeRoot( UInt32 nodeID )
{
	nodeID &= nodeID & 0x3f;

	if( not fDelegateCycleMaster )
	{
		if( fScans[nodeID] )
		{
			fScans[nodeID]->fMustNotBeRoot = true;
			fNodeMustNotBeRootFlag = true;
		}
	}
}

// setGapCount
//
//
void IOFireWireController::setGapCount( UInt32 gapCount )
{
	fForcedGapCount = gapCount;
	fForcedGapFlag	= true;
}

// useHalfSizePackets
//
//

void IOFireWireController::useHalfSizePackets( void )
{
	fUseHalfSizePackets = true;
	fRequestedHalfSizePackets = true;
}

// disablePhyPortForNodeIDOnSleep
//
//

void IOFireWireController::disablePhyPortOnSleepForNodeID( UInt32 nodeID )
{
	UInt32					childNodeID;
	UInt32					childNumber = 0;
	UInt32					childPort;
	
	for( childNodeID = 0; childNodeID < (UInt32)(fLocalNodeID & 63); childNodeID++ )
	{
		if( childNodeID == nodeID )
		{
			// Found it. Now, which port is it connected to?
			childPort = getPortNumberFromIndex( childNumber );
			
			if( childPort != 0xFFFFFFFF ) {
				fFWIM->disablePHYPortOnSleep( 1 << childPort );
				break;
			}
		}
		if( hopCount( childNodeID, fLocalNodeID ) == 1 )
		{
			childNumber++;
		}
	}
}
 
#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// workloop lock
//

// openGate
//
//

void IOFireWireController::openGate()		
{
	fPendingQ.fSource->openGate();
}

// closeGate
//
//

void IOFireWireController::closeGate()		
{
	fPendingQ.fSource->closeGate();
}

// inGate
//
//

bool IOFireWireController::inGate()		
{
	return fPendingQ.fSource->inGate();
}

#pragma mark -
/////////////////////////////////////////////////////////////////////////////
// queues
//

// getTimeoutQ
//
//

IOFWCmdQ& IOFireWireController::getTimeoutQ() 
{ 
	return fTimeoutQ; 
}

// getPendingQ
//
//

IOFWCmdQ& IOFireWireController::getPendingQ() 
{ 
	return fPendingQ; 
}

// getAfterResetHandledQ
//
//

IOFWCmdQ &IOFireWireController::getAfterResetHandledQ() 
{ 
	return fAfterResetHandledQ;
}

// IOFireWireController
//
//

void IOFireWireController::enterLoggingMode( void )
{	
	// set controller IOReg property to signify we're in logging mode
	if ( fFWIM->enterLoggingMode() )
	{
		setProperty(kFireWireLoggingMode, true);
		setProperty( kFireWireGenerationID, "Suspended" );	//zzz set GenID so SysProf does timeout 
	}
}


