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
 * 27 April 99 wgulland created.
 *
 */

#include <IOKit/assert.h>

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG IOLog

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMessage.h>

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFWDCLProgram.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>
#include <IOKit/firewire/IOFireWireLink.h>

// 100 mSec delay after bus reset before scanning bus
// 1000 mSec delay before pruning devices
// 2000 mSec delay between bus resets (1394a)
#define kScanBusDelay		100	
#define kDevicePruneDelay	1000
#define kRepeatResetDelay	2000

#define FWAddressToID(addr) (addr & 63)

enum requestRefConBits {
    kRequestLabel = kFWAsynchTTotal-1,
    kRequestExtTCodeShift = 6,
    kRequestExtTCodeMask = 0x3fffc0,	// 16 bits
    kRequestIsLock = 0x40000000,
    kRequestIsQuad = 0x80000000
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

const IORegistryPlane * IOFireWireBus::gIOFireWirePlane = NULL;

// FireWire bus has two power states, off and on
#define number_of_power_states 2

// Note: This defines two states. off and on.
static IOPMPowerState ourPowerStates[number_of_power_states] = {
  {1,0,0,0,0,0,0,0,0,0,0,0},
{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// magic matching nub (mainly for protocol user clients to hook up to)
class IOFireWireMagicMatchingNub : public IOService
{
    OSDeclareDefaultStructors(IOFireWireMagicMatchingNub)

public:
    virtual bool matchPropertyTable( OSDictionary * table );
    
};

OSDefineMetaClassAndStructors(IOFireWireMagicMatchingNub, IOService)

bool IOFireWireMagicMatchingNub::matchPropertyTable( OSDictionary * table )
{
    OSObject *clientClass;
    clientClass = table->getObject("IOClass");
    if(!clientClass)
        return false;
        
    return clientClass->isEqualTo( getProperty( "IODesiredChild" ) );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// Local Device object (mainly for user clients to hook up to)
class IOFireWireLocalNode : public IOFireWireNub
{
    OSDeclareDefaultStructors(IOFireWireLocalNode)
/*------------------Useful info about device (also available in the registry)--------*/
protected:
/*-----------Methods provided to FireWire device clients-------------*/
public:
    // Set up properties affected by bus reset
    virtual void setNodeProperties(UInt32 generation, UInt16 nodeID, UInt32 *selfIDs, int numIDs);
    
    /*
     * Standard nub initialization
     */
    virtual bool init(OSDictionary * propTable);
    virtual bool attach(IOService * provider );

	virtual void handleClose(   IOService *	  forClient,
                            IOOptionBits	  options ) ;
	virtual bool handleOpen( 	IOService *	  forClient,
                            IOOptionBits	  options,
                            void *		  arg ) ;

    /*
     * Trick method to create protocol user clients
     */
    virtual IOReturn setProperties( OSObject * properties );

 protected:
	UInt32	fOpenCount ;
};

OSDefineMetaClassAndStructors(IOFireWireLocalNode, IOFireWireNub)

bool IOFireWireLocalNode::init(OSDictionary * propTable)
{
    if(!IOFireWireNub::init(propTable))
       return false;
    fMaxReadROMPackLog = 11;
    fMaxReadPackLog = 11;
    fMaxWritePackLog = 11;
    return true;
}

bool IOFireWireLocalNode::attach(IOService * provider )
{
    assert(OSDynamicCast(IOFireWireController, provider));
    if( !IOFireWireNub::attach(provider))
        return (false);
    fControl = (IOFireWireController *)provider;

    return(true);
}


void IOFireWireLocalNode::setNodeProperties(UInt32 gen, UInt16 nodeID,
                                        UInt32 *selfIDs, int numSelfIDs)
{
    OSObject *prop;
    
    fLocalNodeID = fNodeID = nodeID;
    fGeneration = gen;
	
	prop = OSNumber::withNumber(nodeID, 16);
    setProperty(gFireWireNodeID, prop);
    prop->release();

    // Store selfIDs
    prop = OSData::withBytes(selfIDs, numSelfIDs*sizeof(UInt32));
    setProperty(gFireWireSelfIDs, prop);
    prop->release();

    prop = OSNumber::withNumber((selfIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
    setProperty(gFireWireSpeed, prop);
    prop->release();
}

bool IOFireWireLocalNode::handleOpen( 	IOService *	  forClient,
                            IOOptionBits	  options,
                            void *		  arg )
{
	bool ok = true ;

	if ( fOpenCount == 0)
		ok = IOFireWireNub::handleOpen( this, 0, NULL ) ;
	
	if ( ok )
		fOpenCount++ ;

    return ok;
}

void IOFireWireLocalNode::handleClose(   IOService *	  forClient,
                            IOOptionBits	  options )
{
	if ( fOpenCount )
	{
		fOpenCount-- ;
		if ( fOpenCount == 0)
			IOFireWireNub::handleClose( this, 0 );
	}
}

IOReturn IOFireWireLocalNode::setProperties( OSObject * properties )
{
    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    OSDictionary *summon;
    if(!dict)
        return kIOReturnUnsupported;
    summon = OSDynamicCast(OSDictionary, dict->getObject("SummonNub"));
    if(!summon) {
        return kIOReturnBadArgument;
    }
    IOFireWireMagicMatchingNub *nub = NULL;
    IOReturn ret = kIOReturnBadArgument;
    do {
        nub = new IOFireWireMagicMatchingNub;
        if(!nub->init(summon))
            break;
        if (!nub->attach(this))	
            break;
        nub->registerService(kIOServiceSynchronous);
        // Kill nub if nothing matched
        if(!nub->getClient()) {
            nub->detach(this);
        }
        ret = kIOReturnSuccess;
    } while (0);
    if(nub)
        nub->release();
    return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// IOFWQEventSource
OSDefineMetaClassAndStructors(IOFWQEventSource, IOEventSource)

bool IOFWQEventSource::checkForWork()
{
    return fQueue->executeQueue(false);
}

bool IOFWQEventSource::init(IOFireWireController *owner)
{
    fQueue = &owner->getPendingQ();
    return IOEventSource::init(owner);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
//Utility functions

IOFireWireLocalNode *getLocalNode(IOFireWireController *control)
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

void IOFireWireController::readROMGlue(void *refcon, IOReturn status,
			IOFireWireNub *device, IOFWCommand *fwCmd)
{
    IOFWNodeScan *scan = (IOFWNodeScan *)refcon;
    scan->fControl->readDeviceROM(scan, status);
}

void IOFireWireController::clockTick(OSObject *obj, IOTimerEventSource *src)
{
    IOFireWireController *me = (IOFireWireController *)obj;

    // Check the list of pending commands
    me->processTimeout(src);
}

void IOFireWireController::delayedStateChange(void *refcon, IOReturn status,
                                IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    IOFireWireController *me = (IOFireWireController *)bus;
    if(status == kIOReturnTimeout) {
        switch (me->fBusState) {
        case kWaitingScan:
            me->fBusState = kScanning;
            me->startBusScan();
            break;
        case kWaitingPrune:
            me->fBusState = kRunning;
            me->updatePlane();
            break;
        case kPendingReset:
            me->fBusState = kRunning;
            me->fFWIM->resetBus();
            break;
        default:
            IOLog("State change timeout, state is %d\n", me->fBusState);
            break;
        }        
    }
}

void IOFireWireController::processTimeout(IOTimerEventSource *src)
{
    // complete() might take significant time, enough to cause
    // a later command to timeout too, so we loop here until there is no timeout.
    while (fTimeoutQ.fHead) {
        AbsoluteTime now, dead;
        clock_get_uptime(&now);
#if 0
        IOLog("processTimeout, time is %lx:%lx\n", now.hi, now.lo);
        {
            IOFWCommand *t = fTimeoutQ.fHead;
            while(t) {
                AbsoluteTime d = t->getDeadline();
                IOLog("%s:%p deadline %lx:%lx\n",
                    t->getMetaClass()->getClassName(), t, d.hi, d.lo);
                t = t->getNext();
            }
        }
#endif
        dead = fTimeoutQ.fHead->getDeadline();
        if(CMP_ABSOLUTETIME(&dead, &now) == 1)
            break;	// Command with earliest deadline is OK.
        // Make sure there isn't a packet waiting.
        fFWIM->handleInterrupts(1);
        // Which may have changed the queue - see if earliest deadline has changed.
        if(!fTimeoutQ.fHead)
            break;
        if(CMP_ABSOLUTETIME(&dead, &fTimeoutQ.fHead->getDeadline()) != 0)
            continue;
        //IOLog("Cmd 0x%x timing out\r", fTimeoutQ.fHead);
        fTimeoutQ.fHead->cancel(kIOReturnTimeout);
    };
    if(fTimeoutQ.fHead) {
        src->wakeAtTime(fTimeoutQ.fHead->getDeadline());
        //AbsoluteTime now;
        //clock_get_uptime(&now);
        //IOLog("processTimeout, timeoutQ waketime %lx:%lx (now %lx:%lx)\n",
        //        fTimeoutQ.fHead->getDeadline().hi, fTimeoutQ.fHead->getDeadline().lo, now.hi, now.lo);
    }
    else {
        //IOLog("processTimeout, timeoutQ empty\n");
        src->cancelTimeout();
    }
}

void IOFireWireController::timeoutQ::headChanged(IOFWCommand *oldHead)
{
#if 0
    {
        IOFWCommand *t = fHead;
        if(oldHead)
            IOLog("IOFireWireController::timeoutQ::headChanged(%s:%p)\n",
                oldHead->getMetaClass()->getClassName(), oldHead);
        else
            IOLog("IOFireWireController::timeoutQ::headChanged(0)\n");
            
        while(t) {
            AbsoluteTime d = t->getDeadline();
            IOLog("%s:%p deadline %lx:%lx\n",
                t->getMetaClass()->getClassName(), t, d.hi, d.lo);
            t = t->getNext();
        }
    }
#endif
    if(!fHead) {
        //IOLog("timeoutQ empty\n");
        fTimer->cancelTimeout();
    }
    else {
        fTimer->wakeAtTime(fHead->getDeadline());
        //AbsoluteTime now;
        //clock_get_uptime(&now);
        //IOLog("timeoutQ waketime %lx:%lx (now %lx:%lx)\n",
        //        fHead->getDeadline().hi, fHead->getDeadline().lo, now.hi, now.lo);
    }
}
void IOFireWireController::timeoutQ::busReset()
{
#if 0
    {
        IOFWCommand *t = fHead;
        if(oldHead)
            IOLog("IOFireWireController::timeoutQ::headChanged(%s:%p)\n",
                oldHead->getMetaClass()->getClassName(), oldHead);
        else
            IOLog("IOFireWireController::timeoutQ::headChanged(0)\n");
            
        while(t) {
            AbsoluteTime d = t->getDeadline();
            IOLog("%s:%p deadline %lx:%lx\n",
                t->getMetaClass()->getClassName(), t, d.hi, d.lo);
            t = t->getNext();
        }
    }
#endif
    IOFWCommand *cmd;
    cmd = fHead;
    while(cmd) {
        IOFWCommand *next;
        next = cmd->getNext();
        if(cmd->cancelOnReset()) {
            cmd->cancel(kIOFireWireBusReset);
        }
        cmd = next;
    }
}


void IOFireWireController::pendingQ::headChanged(IOFWCommand *oldHead)
{
    if(fHead) {
        fSource->signalWorkAvailable();
    }
}

bool IOFireWireController::init(IOFireWireLink *fwim)
{
    if(!IOFireWireBus::init())
        return false;
    fFWIM = fwim;
    
    // Create firewire symbols.
    gFireWireROM = OSSymbol::withCString("FireWire Device ROM");
    gFireWireNodeID = OSSymbol::withCString("FireWire Node ID");
    gFireWireSelfIDs = OSSymbol::withCString("FireWire Self IDs");
    gFireWireUnit_Spec_ID = OSSymbol::withCString("Unit_Spec_ID");
    gFireWireUnit_SW_Version = OSSymbol::withCString("Unit_SW_Version");
    gFireWireVendor_ID = OSSymbol::withCString("Vendor_ID");
    gFireWire_GUID = OSSymbol::withCString("GUID");
    gFireWireSpeed = OSSymbol::withCString("FireWire Speed");
    gFireWireVendor_Name = OSSymbol::withCString("FireWire Vendor Name");
    gFireWireProduct_Name = OSSymbol::withCString("FireWire Product Name");

    if(NULL == gIOFireWirePlane) {
        gIOFireWirePlane = IORegistryEntry::makePlane( kIOFireWirePlane );
    }

    fLocalAddresses = OSSet::withCapacity(5);	// Local ROM + CSR registers + SBP-2 ORBs + FCP + PCR
    if(fLocalAddresses)
        fSpaceIterator =  OSCollectionIterator::withCollection(fLocalAddresses);

    fAllocatedChannels = OSSet::withCapacity(1);	// DV channel.
    if(fAllocatedChannels)
        fAllocChannelIterator =  OSCollectionIterator::withCollection(fAllocatedChannels);

    fLastTrans = kMaxPendingTransfers-1;

    UInt32 bad = 0xdeadbabe;
    fBadReadResponse = IOBufferMemoryDescriptor::withBytes(&bad, sizeof(bad), kIODirectionOutIn);

    fDelayedStateChangeCmdNeedAbort = false;
    fDelayedStateChangeCmd = createDelayedCmd(1000 * kScanBusDelay, delayedStateChange, NULL);

    return (gFireWireROM != NULL &&  gFireWireNodeID != NULL &&
        gFireWireUnit_Spec_ID != NULL && gFireWireUnit_SW_Version != NULL && 
	fLocalAddresses != NULL && fSpaceIterator != NULL &&
            fAllocatedChannels != NULL && fAllocChannelIterator != NULL &&
            fBadReadResponse != NULL);
}

IOReturn IOFireWireController::setPowerState( unsigned long powerStateOrdinal,
                                                IOService* whatDevice )
{
    IOReturn res;
    IOReturn sleepRes;
    
    // use gate to keep other threads off the hardware,
    // Either close gate or wake workloop.
    // First time through, we aren't really asleep.
    if(fBusState != kAsleep || fROMHeader[1] != kFWBIBBusName) {
        fPendingQ.fSource->closeGate();
    }
    else {
        sleepRes = fWorkLoop->wake(&fBusState);
        if(sleepRes != kIOReturnSuccess) {
            IOLog("Can't wake FireWire workloop, error 0x%x\n", sleepRes);
        }
    }
    // Either way, we have the gate closed against invaders/lost sheep
    if(powerStateOrdinal != 0)
    {
        fBusState = kRunning;	// Will transition to a bus reset state.
        if( fDelayedStateChangeCmdNeedAbort )
        {
            fDelayedStateChangeCmdNeedAbort = false;
            fDelayedStateChangeCmd->cancel(kIOReturnAborted);
        }
    }
    
    // Reset bus if we're sleeping, before turning hw off.
    if(powerStateOrdinal == 0)
        fFWIM->resetBus();
        
    res = fFWIM->setLinkPowerState(powerStateOrdinal);
    
    // Reset bus if we're waking, not if we're starting up.
    if(powerStateOrdinal == 1 && res == IOPMAckImplied && fROMHeader[1] == kFWBIBBusName)
	{
		if ( kIOReturnSuccess != UpdateROM() )
			IOLog(" %s %u: UpdateROM() got error\n", __FILE__, __LINE__ ) ;
	
        fFWIM->resetBus();	// Don't do this on startup until Config ROM built.
	}
	
    // Update power state, keep gate closed while we sleep.
    if(powerStateOrdinal == 0) {
        // Pretend we had a bus reset - we'll have a real one when we wake up.
        //processBusReset();
        if(fBusState == kWaitingPrune || fBusState == kWaitingScan || fBusState == kPendingReset)
            fDelayedStateChangeCmdNeedAbort = true;
            
        fBusState = kAsleep;
    }
    if((fBusState == kAsleep) && (fROMHeader[1] == kFWBIBBusName)) {
         sleepRes = fWorkLoop->sleep(&fBusState);
        if(sleepRes != kIOReturnSuccess) {
            IOLog("Can't sleep FireWire workloop, error 0x%x\n", sleepRes);
        }
    }
    else {
        fPendingQ.fSource->openGate();
    }

    return res;
}

bool IOFireWireController::start(IOService *provider)
{
    UInt16 crc16;
    IOReturn res;

    if (!IOService::start(provider))
        return false;
    
    CSRNodeUniqueID guid = fFWIM->getGUID();
    
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
    
    // Create Timer Event source and queue event source,
    // do before power management so the PM code can access the workloop
    fTimer = IOTimerEventSource::timerEventSource(this, clockTick);
    if(!fTimer)
	return false;

    fTimeoutQ.fTimer = fTimer;

    IOFWQEventSource *q;
    q = new IOFWQEventSource;
    fPendingQ.fSource = q;
    q->init(this);

    fWorkLoop = fFWIM->getFireWireWorkLoop();
    fWorkLoop->addEventSource(fTimer);
    fWorkLoop->addEventSource(fPendingQ.fSource);

    // register ourselves with superclass policy-maker
    PMinit();
    provider->joinPMtree(this);
    registerPowerDriver(this, ourPowerStates, number_of_power_states);
    
    // No idle sleep
    res = changePowerStateTo(1);
    IOLog("Local FireWire GUID = 0x%lx:0x%lx\n", (UInt32)(guid >> 32), (UInt32)(guid & 0xffffffff));

    // Build local device ROM
    // Allocate address space for Configuration ROM and fill in Bus Info
    // block.
    fROMHeader[1] = kFWBIBBusName;
    fROMHeader[2] = fFWIM->getBusCharacteristics();
    fMaxRecvLog = ((fROMHeader[2] & kFWBIBMaxRec) >> kFWBIBMaxRecPhase)+1;
    fMaxSendLog = fFWIM->getMaxSendLog();
    fROMHeader[3] = guid >> 32;
    fROMHeader[4] = guid & 0xffffffff;

    crc16 = FWComputeCRC16 (&fROMHeader[1], 4);
    fROMHeader[0] = 0x04040000 | crc16;

    // Create root directory in FWIM data.//zzz should we have one for each FWIM or just one???
    fRootDir = IOLocalConfigDirectory::create();
    if(!fRootDir)
        return false;

    // Set our Config ROM generation.
    fRootDir->addEntry(kConfigGenerationKey, (UInt32)0);
    // Set our module vendor ID.
    fRootDir->addEntry(kConfigModuleVendorIdKey, 0x00A040);
    // Set our capabilities.
    fRootDir->addEntry(kConfigNodeCapabilitiesKey, 0x000083C0);

    // Set our node unique ID.
    OSData *t = OSData::withBytes(&guid, sizeof(guid));
    fRootDir->addEntry(kConfigNodeUniqueIdKey, t);
        
    fTimer->enable();

    // Create local node
    IOFireWireLocalNode *localNode = new IOFireWireLocalNode;
    
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

    res = UpdateROM();
    if(res == kIOReturnSuccess)
        res = fFWIM->resetBus();

    fWorkLoop->enableAllInterrupts();	// Enable the interrupt delivery.

    registerService();			// Enable matching with this object

    return res == kIOReturnSuccess;
}

void IOFireWireController::stop( IOService * provider )
{

    // Fake up disappearance of entire bus
    processBusReset();
        
    PMstop();

    if(fBusState == kAsleep) {
        IOReturn sleepRes;
        sleepRes = fWorkLoop->wake(&fBusState);
        if(sleepRes != kIOReturnSuccess) {
            IOLog("IOFireWireController::stop - Can't wake FireWire workloop, error 0x%x\n", sleepRes);
        }
        fPendingQ.fSource->openGate();
    }
    fWorkLoop->removeEventSource(fTimer);
    fWorkLoop->removeEventSource(fPendingQ.fSource);

    IOService::stop(provider);
}

bool IOFireWireController::finalize( IOOptionBits options )
{
    bool res;
    fTimer->release();
    fPendingQ.fSource->release();
    
    res = IOService::finalize(options);
    return res;
}

// Override requestTerminate()
// to send our custom kIOFWMessageServiceIsRequestingClose to clients
bool IOFireWireController::requestTerminate( IOService * provider, IOOptionBits options )
{
    OSIterator *childIterator;
    childIterator = getClientIterator();
    if( childIterator) {
        OSObject *child;
        while( (child = childIterator->getNextObject())) {
            IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
            if(found && !found->isInactive() && found->isOpen()) {
                IOLog( "IOFireWireController : message request close device object %p\n", found);
                // send our custom requesting close message
                messageClient( kIOFWMessageServiceIsRequestingClose, found );
            }
        }
    }
    return IOService::requestTerminate(provider, options);
}


IOWorkLoop *IOFireWireController::getWorkLoop() const
{
    return fWorkLoop;
}

AsyncPendingTrans *IOFireWireController::allocTrans(IOFWAsyncCommand *cmd)
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
            t->fInUse = true;
            t->fTCode = tran;
            fLastTrans = tran;
            return t;
        }
    }
    IOLog("Out of FireWire transaction codes!\n");
    return NULL;
}

void IOFireWireController::freeTrans(AsyncPendingTrans *trans)
{
    // No lock needed - can't have two users of a tcode.
    trans->fHandler = NULL;
    trans->fInUse = false;
}


void IOFireWireController::readDeviceROM(IOFWNodeScan *scan, IOReturn status)
{
    bool done = true;
    if(status != kIOReturnSuccess) {
	// If status isn't bus reset, make a dummy registry entry.
//IOLog("readDeviceRom for 0x%x, cmd 0x%x, result is 0x%x\n", scan, scan->fCmd, status);
        if(status == kIOFireWireBusReset) {
            scan->fCmd->release();
            IOFree(scan, sizeof(*scan));
            return;
        }

        OSDictionary *propTable;
        UInt32 nodeID = FWAddressToID(scan->fAddr.nodeID);
        OSObject * prop;
        propTable = OSDictionary::withCapacity(3);
        prop = OSNumber::withNumber(scan->fAddr.nodeID, 16);
        propTable->setObject(gFireWireNodeID, prop);
        prop->release();

        prop = OSNumber::withNumber((scan->fSelfIDs[0] & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
        propTable->setObject(gFireWireSpeed, prop);
        prop->release();

        prop = OSData::withBytes(scan->fSelfIDs, scan->fNumSelfIDs*sizeof(UInt32));
        propTable->setObject(gFireWireSelfIDs, prop);
        prop->release();

        IORegistryEntry * newPhy;
        newPhy = new IORegistryEntry;
	if(newPhy) {
            if(!newPhy->init(propTable)) {
                newPhy->release();	
		newPhy = NULL;
            }
	}
        fNodes[nodeID] = newPhy;
        if(propTable)
            propTable->release();
        fNumROMReads--;
        if(fNumROMReads == 0) {
            finishedBusScan();
        }

        scan->fCmd->release();
        IOFree(scan, sizeof(*scan));
        return;
    }

    if(scan->fRead == 0) {
	if( ((scan->fBuf[0] & kConfigBusInfoBlockLength) >> kConfigBusInfoBlockLengthPhase) == 1) {
            // Minimal ROM
            scan->fROMSize = 4;
            done = true;
	}
	else {
            scan->fROMSize = 4*((scan->fBuf[0] & kConfigROMCRCLength) >> kConfigROMCRCLengthPhase) + 4;
            if(scan->fROMSize > 20)
                scan->fROMSize = 20;	// Just read bus info block
            scan->fRead = 8;
            scan->fBuf[1] = kFWBIBBusName;	// No point reading this!
            scan->fAddr.addressLo = kConfigROMBaseAddress+8;
            scan->fCmd->reinit(scan->fAddr, scan->fBuf+2, 1,
                                                        &readROMGlue, scan, true);
            scan->fCmd->submit();
            done = false;
	}
    }
    else if(scan->fRead < 16) {
        if(scan->fROMSize > scan->fRead) {
            scan->fRead += 4;
            scan->fAddr.addressLo = kConfigROMBaseAddress+scan->fRead;
            scan->fCmd->reinit(scan->fAddr, scan->fBuf+scan->fRead/4, 1,
                                                        &readROMGlue, scan, true);
            scan->fCmd->submit();
            done = false;
        }
        else
            done = true;
    }
    if(done) {
        // See if this is a bus manager
        if(!fBusMgr)
            fBusMgr = scan->fBuf[2] & kFWBIBBmc;
        
	// Check if node exists, if not create it
#if (DEBUGGING_LEVEL > 0)
        DEBUGLOG("Finished reading ROM for node 0x%x\n", scan->fAddr.nodeID);
#endif
        IOFireWireDevice *	newDevice = NULL;
        do {
            CSRNodeUniqueID guid;
            OSIterator *childIterator;
            if(scan->fROMSize >= 20)
            	guid = *(CSRNodeUniqueID *)(scan->fBuf+3);
            else
                guid = scan->fBuf[0];	// Best we can do.
            
            childIterator = getClientIterator();
            if( childIterator) {
                OSObject *child;
                while( (child = childIterator->getNextObject())) {
                    IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
                    if(found && found->fUniqueID == guid && !found->isInactive()) {
                        newDevice = found;
                        break;
                    }
                }
                childIterator->release();
            }

            if(newDevice) {
		// Just update device properties.
                IOLog("Found old device 0x%p\n", newDevice);
		newDevice->setNodeROM(fBusGeneration, fLocalNodeID, scan);
		newDevice->retain();	// match release, since not newly created.
            }
            else {
                newDevice = fFWIM->createDeviceNub(guid, scan);
                if (!newDevice)
                    continue;
                IOLog("Creating new device 0x%p\n", newDevice);

                if (!newDevice->attach(this))
                    continue;
                newDevice->setNodeROM(fBusGeneration, fLocalNodeID, scan);
                newDevice->registerService();
            }
            UInt32 nodeID = FWAddressToID(scan->fAddr.nodeID);
            fNodes[nodeID] = newDevice;
            fNodes[nodeID]->retain();
	} while (false);
        if (newDevice)
            newDevice->release();
        scan->fCmd->release();
        IOFree(scan, sizeof(*scan));
        fNumROMReads--;
        if(fNumROMReads == 0) {
            finishedBusScan();
        }
    }
}


//
// Hardware detected a bus reset.
// At this point we don't know what the hardware addresses are
void IOFireWireController::processBusReset()
{
    clock_get_uptime(&fResetTime);	// Update even if we're already processing a reset
    if(fBusState != kWaitingSelfIDs) {
        if(fBusState == kWaitingPrune || fBusState == kWaitingScan || fBusState == kPendingReset)
            fDelayedStateChangeCmd->cancel(kIOReturnAborted);
        fBusState = kWaitingSelfIDs;
        unsigned int i;
        UInt32 oldgen = fBusGeneration;
        // Set all current device nodeIDs to something invalid
        fBusGeneration++;
        OSIterator *childIterator;
        childIterator = getClientIterator();
        if( childIterator) {
            OSObject * child;
            while( (child = childIterator->getNextObject())) {
                IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
                if(found && !found->isInactive())
                    found->setNodeROM(oldgen, kFWBadNodeID, NULL);
                else if(OSDynamicCast(IOFireWireLocalNode, child)) {
                    ((IOFireWireLocalNode *)child)->messageClients(kIOMessageServiceIsSuspended);
                }
            }
            childIterator->release();
        }


        // Invalidate current topology and speed map
//IOLog("FireWire Bus Generation now %d\n", fBusGeneration);
        bzero(fSpeedCodes, sizeof(fSpeedCodes));

        // Zap all outstanding async requests
        for(i=0; i<kMaxPendingTransfers; i++) {
            AsyncPendingTrans *t = &fTrans[i];
            if(t->fHandler) {
                IOFWAsyncCommand * cmd = t->fHandler;
                cmd->gotPacket(kFWResponseBusResetError, NULL, 0);
            }
        }

        // Clear out the old firewire plane
        if(fNodes[fRootNodeID]) {
            fNodes[fRootNodeID]->detachAll(gIOFireWirePlane);
        }
        for(i=0; i<=fRootNodeID; i++) {
            if(fNodes[i]) {
                fNodes[i]->release();
                fNodes[i] = NULL;
            }
        }
        
        // Cancel all commands in timeout queue that want to complete on bus reset
        fTimeoutQ.busReset();
    }
}

//
// SelfID packets received after reset.
void IOFireWireController::processSelfIDs(UInt32 *IDs, int numIDs, UInt32 *ownIDs, int numOwnIDs)
{
    int i;
    UInt32 id;
    UInt32 nodeID;
    UInt16 irmID;
    UInt16 ourID;
    IOFireWireLocalNode * localNode;
    
#if (DEBUGGING_LEVEL > 0)
for(i=0; i<numIDs; i++)
    IOLog("ID %d: 0x%x <-> 0x%x\n", i, IDs[2*i], ~IDs[2*i+1]);

for(i=0; i<numOwnIDs; i++)
    IOLog("Own ID %d: 0x%x <-> 0x%x\n", i, ownIDs[2*i], ~ownIDs[2*i+1]);
#endif
    // If not processing a reset, then we should be
    // This can happen if we get two resets in quick succession
    if(fBusState != kWaitingSelfIDs)
        processBusReset();
    fBusState = kWaitingScan;
                      
    // Initialize root node to be our node, we'll update it below to be the highest node ID.
    fRootNodeID = ourID = (*ownIDs & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;
    fLocalNodeID = ourID | (kFWLocalBusAddress>>kCSRNodeIDPhase);

    // check for mismatched gap counts
    UInt32 gap_count = (*ownIDs & kFWSelfID0GapCnt) >> kFWSelfID0GapCntPhase;
    for( i = 0; i < numIDs; i++ )
    {
        UInt32 current_id = IDs[2*i];
        if( (current_id & kFWSelfIDPacketType) == 0 &&
            ((current_id & kFWSelfID0GapCnt) >> kFWSelfID0GapCntPhase) != gap_count )
        {
            // set the gap counts to 0x3F, if any gap counts are mismatched
            fFWIM->sendPHYPacket( (kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                                  (0x3f << kFWPhyConfigurationGapCntPhase) | kFWPhyConfigurationT );            
            break;
        }
    }

    // Update the registry entry for our local nodeID,
    // which will have been updated by the device driver.
    // Find the local node (just avoiding adding a member variable)
    localNode = getLocalNode(this);
    if(localNode) {
        localNode->setNodeProperties(fBusGeneration, fLocalNodeID, ownIDs, numOwnIDs);
        fNodes[ourID] = localNode;
        localNode->retain();
    }
    
    // Copy over the selfIDs, checking validity and merging in our selfIDs if they aren't
    // already in the list.
    SInt16 prevID = -1;	// Impossible ID.
    UInt32 *idPtr = fSelfIDs;
    for(i=0; i<numIDs; i++) {
        UInt32 id = IDs[2*i];
        UInt16 currID = (id & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase;

        if(id != ~IDs[2*i+1]) {
            IOLog("Bad SelfID packet %d: 0x%lx != 0x%lx!\n", i, id, ~IDs[2*i+1]);
            resetBus();	// Could wait a bit in case somebody else spots the bad packet
            return;
        }
        if(currID != prevID) {
            // Check for ownids not in main list
            if(prevID < ourID && currID > ourID) {
		int j;
		fNodeIDs[ourID] = idPtr;
                for(j=0; j<numOwnIDs; j++)
                    *idPtr++ = ownIDs[2*j];
            }
            fNodeIDs[currID] = idPtr;
            prevID = currID;
            if(fRootNodeID < currID)
                fRootNodeID = currID;
        }
	*idPtr++ = id;
    }
    // Check for ownids at end & not in main list
    if(prevID < ourID) {
        int j;
        fNodeIDs[ourID] = idPtr;
        for(j=0; j<numOwnIDs; j++)
            *idPtr++ = ownIDs[2*j];
    }
    // Stick a known elephant at the end.
    fNodeIDs[fRootNodeID+1] = idPtr;

    // Check nodeIDs are monotonically increasing from 0.
    for(i = 0; i<=fRootNodeID; i++) {
        if( ((*fNodeIDs[i] & kFWPhyPacketPhyID) >> kFWPhyPacketPhyIDPhase) != (UInt32)i) {
            IOLog("No FireWire Node %d (got ID packet 0x%lx)!\n", i, *fNodeIDs[i]);
            resetBus();        // Could wait a bit in case somebody else spots the bad packet
            return;
        }
    }
    
    // Store selfIDs
    OSObject * prop = OSData::withBytes( fSelfIDs, (fRootNodeID+1) * sizeof(UInt32));
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
        id = *fNodeIDs[i];
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
    
    // OK for stuff that only needs our ID and the irmID to continue.
    if(localNode) {
        localNode->messageClients(kIOMessageServiceIsResumed);
    }
    fDelayedStateChangeCmd->reinit(1000 * kScanBusDelay, delayedStateChange, NULL);
    fDelayedStateChangeCmd->submit();
}

void IOFireWireController::startBusScan() {
    int i;
    UInt32 gap;
    // First check that gap count is consistent, if not set to 3f for now.
    gap = *fNodeIDs[0] & kFWPhyConfigurationGapCnt;
    for(i=1; i<=fRootNodeID; i++) {
        UInt32 id;
        id = *fNodeIDs[i];
        if(gap != (id & kFWPhyConfigurationGapCnt)) {
            //IOLog("Inconsistent gap counts 0x%x<->0x%x\n", gap, id & kFWPhyConfigurationGapCnt);
            fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                                ((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) |
                                (0x3f << kFWPhyConfigurationGapCntPhase) | kFWPhyConfigurationT);
            
            break;
        }
    }

    fNumROMReads = 0;
    for(i=0; i<=fRootNodeID; i++) {
        UInt16 nodeID;
        UInt32 id;
        id = *fNodeIDs[i];
        // Get nodeID.
        nodeID = (id & kFWSelfIDPhyID) >> kFWSelfIDPhyIDPhase;
        nodeID |= kFWLocalBusAddress>>kCSRNodeIDPhase;
        if(nodeID == fLocalNodeID)
            continue;	// Skip ourself!

        // Read ROM header if link is active (MacOS8 turns link on, why?)
        if(true) { //id & kFWSelfID0L) {
            IOFWNodeScan *scan;
            scan = (IOFWNodeScan *)IOMalloc(sizeof(*scan));
            fNumROMReads++;

            scan->fControl = this;
            scan->fAddr.nodeID = nodeID;
            scan->fAddr.addressHi = kCSRRegisterSpaceBaseAddressHi;
            scan->fAddr.addressLo = kConfigBIBHeaderAddress;
            scan->fSelfIDs = fNodeIDs[i];
            scan->fNumSelfIDs = fNodeIDs[i+1] - fNodeIDs[i];
            scan->fRead = 0;
            scan->fCmd = new IOFWReadQuadCommand;
            scan->fCmd->initAll(this, fBusGeneration, scan->fAddr, scan->fBuf, 1,
                                                &readROMGlue, scan);
            scan->fCmd->submit();
        }
    }
    if(fNumROMReads == 0) {
        finishedBusScan();
    }
}

void IOFireWireController::finishedBusScan()
{
    // These magic numbers come from P1394a, draft 4, table C-2.
    // This works for cables up to 4.5 meters and PHYs with
    // PHY delay up to 144 nanoseconds.  Note that P1394a PHYs
    // are allowed to have delay >144nS; we don't cope yet.
    static UInt32 gaps[17] = {63, 5, 7, 8, 10, 13, 16, 18, 21,
                            24, 26, 29, 32, 35, 37, 40, 43};
    
    // Now do simple bus manager stuff, if there isn't a better candidate.
    // This might cause us to issue a bus reset...
    // Skip if we're about to reset anyway, since we might be in the process of setting
    // another node to root.
    if(fBusState != kPendingReset && !fBusMgr && fLocalNodeID == fIRMNodeID) {
        int maxHops;
        int i;
        // Do lazy gap count optimization.  Assume the bus is a daisy-chain (worst
        // case) so hop count == root ID.

        // a little note on the gap count optimization - all I do is set an internal field to our optimal
        // gap count and then reset the bus. my new soft bus reset code sets the gap count before resetting
        // the bus (for another reason) and I just rely on that fact.

        maxHops = fRootNodeID;
        if (maxHops > 16) maxHops = 16;
        fGapCount = gaps[maxHops] << kFWPhyConfigurationGapCntPhase;
        if(fRootNodeID == 0) {
            // If we're the only node, clear root hold off.
            fFWIM->setRootHoldOff(false);
        }
        else {
            
            // Send phy packet to get rid of other root hold off nodes
            // Set gap count too.
            fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                        ((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR );
            fFWIM->setRootHoldOff(true);

            // If we aren't root, issue a bus reset so that we will be.
            if(fRootNodeID != (fLocalNodeID & 63)) {
                resetBus();
                return;			// We'll be back...
            }
        }

        // If we got here we're root, so turn on cycle master
        fFWIM->setCycleMaster(true);

        // Finally set gap count if any nodes don't have the right gap.
        // Only bother if we aren't the only node on the bus.
        if(fRootNodeID != 0) {
            for( i = 0; i <= fRootNodeID; i++ ) {
                // is the gap count set to what we want it to be?
                if( (*fNodeIDs[i] & kFWSelfID0GapCnt) != fGapCount ) {
                    // Nope, send phy config packet and do bus reset.
                    fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                                ((fLocalNodeID & 63) << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR |
                                         fGapCount | kFWPhyConfigurationT);
                    resetBus();
                    return;			// We'll be back...
                }
            }
        }
    }


    // Don't change to the waiting prune state if we're about to bus reset again anyway.
    if(fBusState == kScanning) {
        fBusState = kWaitingPrune; 	// Indicate end of bus scan
        fDelayedStateChangeCmd->reinit(1000 * kDevicePruneDelay, delayedStateChange, NULL); // One second
        fDelayedStateChangeCmd->submit();
    }

    // Run all the commands that are waiting for reset processing to end
    IOFWCmdQ &resetQ(getAfterResetHandledQ());
    resetQ.executeQueue(true);

    // Tell all active isochronous channels to re-allocate bandwidth
    IOFWIsochChannel *found;
    fAllocChannelIterator->reset();
    while( (found = (IOFWIsochChannel *) fAllocChannelIterator->getNextObject())) {
        found->handleBusReset();
    }

    // Anything on queue now is associated with a device not on the bus, I think...
    IOFWCommand *cmd;
    while(cmd = resetQ.fHead) {
        cmd->cancel(kIOReturnTimeout);
    }

}

void IOFireWireController::buildTopology(bool doFWPlane)
{
    int i, maxDepth;
    IORegistryEntry *root;
    struct FWNodeScan
    {
        int nodeID;
        int childrenRemaining;
        IORegistryEntry *node;
    };
    FWNodeScan scanList[kFWMaxNodeHops];
    FWNodeScan *level;
    maxDepth = 0;
    root = fNodes[fRootNodeID];
    level = scanList;

    // First build the topology.
    for(i=fRootNodeID; i>=0; i--) {
        UInt32 id0, idn;
        UInt32 *idPtr;
        UInt8 speedCode;
        IORegistryEntry *node = fNodes[i];
        int children = 0;
        int ports;
        UInt32 port;
        int mask, shift;

        idPtr = fNodeIDs[i];
        id0 = *idPtr++;
        mask = kFWSelfID0P0;
        shift = kFWSelfID0P0Phase;
        for(ports=0; ports<3; ports++) {
            port = (id0 & mask) >> shift;
            if(port == kFWSelfIDPortStatusChild)
                children++;
            mask >>= 2;
            shift -= 2;
        }

        if(fNodeIDs[i+1] > idPtr) {
            // More selfIDs. 8 ports in ID1
            idn = *idPtr++;
            mask = kFWSelfIDNPa;
            shift = kFWSelfIDNPaPhase;
            for(ports=0; ports<8; ports++) {
                port = (idn & mask) >> shift;
                if(port == kFWSelfIDPortStatusChild)
                    children++;
                mask >>= 2;
                shift -= 2;
            }
            if(fNodeIDs[i+1] > idPtr) {
                // More selfIDs. 5 ports in ID2
                idn = *idPtr++;
                mask = kFWSelfIDNPa;
                shift = kFWSelfIDNPaPhase;
                for(ports=0; ports<5; ports++) {
                    port = (idn & mask) >> shift;
                    if(port == kFWSelfIDPortStatusChild)
                        children++;
                    mask >>= 2;
                    shift -= 2;
                }

            }
        }

        // Add node to bottom of tree
        level->nodeID = i;
        level->childrenRemaining = children;
        level->node = node;

        // Add node's self speed to speedmap
        speedCode = (id0 & kFWSelfID0SP) >> kFWSelfID0SPPhase;
        fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + i] = speedCode;

        // Add to parent
        // Compute rest of this node's speed map entries unless it's the root.
        // We only need to compute speeds between this node and all higher node numbers.
        // These speeds will be the minimum of this node's speed and the speed between
        // this node's parent and the other higher numbered nodes.
        if (i != fRootNodeID) {
            int parentNodeNum, scanNodeNum;
            parentNodeNum = (level-1)->nodeID;
            if(doFWPlane)
                node->attachToParent((level-1)->node, gIOFireWirePlane);
            for (scanNodeNum = i + 1; scanNodeNum <= fRootNodeID; scanNodeNum++) {
                UInt8 scanSpeedCode;
                // Get speed code between parent and scan node.
                scanSpeedCode =
                        fSpeedCodes[(kFWMaxNodesPerBus + 1)*parentNodeNum + scanNodeNum];

                // Set speed map entry to minimum of scan speed and node's speed.
                if (speedCode < scanSpeedCode)
                        scanSpeedCode = speedCode;
                fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + scanNodeNum] = scanSpeedCode;
                fSpeedCodes[(kFWMaxNodesPerBus + 1)*scanNodeNum + i] = scanSpeedCode;
            }
        }
        // Find next child port.
        if (i > 0) {
            while (level->childrenRemaining == 0) {
                // Go up one level in tree.
                level--;
                if(level < scanList) {
                    IOLog("SelfIDs don't build a proper tree (missing selfIDS?)!!\n");
                    return;
                }
                // One less child to scan.
                level->childrenRemaining--;
            }
            // Go down one level in tree.
            level++;
            if(level - scanList > maxDepth) {
                maxDepth = level - scanList;
            }
        }
    }


#if (DEBUGGING_LEVEL > 0)
    if(doFWPlane) {
        IOLog("max FireWire tree depth is %d\n", maxDepth);
        IOLog("FireWire Speed map:\n");
        for(i=0; i <= fRootNodeID; i++) {
            int j;
            for(j=0; j <= i; j++) {
                IOLog("%d ", fSpeedCodes[(kFWMaxNodesPerBus + 1)*i + j]);
            }
            IOLog("\n");
        }
    }
#endif
    // Finally attach the full topology into the IOKit registry
    if(doFWPlane)
        root->attachToParent(IORegistryEntry::getRegistryRoot(), gIOFireWirePlane);
}

void IOFireWireController::updatePlane()
{
    OSIterator *childIterator;
    childIterator = getClientIterator();
    if( childIterator) {
        OSObject *child;
        while( (child = childIterator->getNextObject())) {
            IOFireWireDevice * found = OSDynamicCast(IOFireWireDevice, child);
            if(found && !found->isInactive() && found->fNodeID == kFWBadNodeID) {
                if( found->isOpen() )
                {
                    //IOLog( "IOFireWireController : message request close device object %p\n", found);
                    // send our custom requesting close message
                    messageClient( kIOFWMessageServiceIsRequestingClose, found );
                }
                else
                {	
                    //IOLog( "IOFireWireController : terminate device object %p\n", found);
                    found->terminate();
                }
            }
        }
        childIterator->release();
    }

    buildTopology(true);
}

////////////////////////////////////////////////////////////////////////////////
//
// processWriteRequest
//
//   process quad and block writes.
//
void IOFireWireController::processWriteRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len)
{
    UInt32 ret = kFWResponseAddressError;
    IOFWSpeed speed = FWSpeed(sourceID);
    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);
    IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        ret = found->doWrite(sourceID, speed, addr, len, buf, (IOFWRequestRefCon)tLabel);
        if(ret != kFWResponseAddressError)
            break;
    }
    fFWIM->asyncWriteResponse(sourceID, speed, tLabel, ret, addr.addressHi);
}

////////////////////////////////////////////////////////////////////////////////
//
// processLockRequest
//
//   process 32 and 64 bit locks.
//
void IOFireWireController::processLockRequest(UInt16 sourceID, UInt32 tLabel,
			UInt32 *hdr, void *buf, int len)
{
    UInt32 oldVal[2];
    UInt32 ret;
    UInt32 outLen =sizeof(oldVal);
    int type = (hdr[3] &  kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase;
    FWAddress addr((hdr[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, hdr[2]);
    IOFWSpeed speed = FWSpeed(sourceID);

    IOFWRequestRefCon refcon = (IOFWRequestRefCon)(tLabel | kRequestIsLock | (type << kRequestExtTCodeShift));

    ret = doLockSpace(sourceID, speed, addr, len, (const UInt32 *)buf, outLen, oldVal, type, refcon);

    fFWIM->asyncLockResponse(sourceID, speed, tLabel, ret, type, oldVal, outLen);
}

UInt32 IOFireWireController::doReadSpace(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                                IOMemoryDescriptor **buf, IOByteCount * offset,
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
    return ret;
}

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

UInt32 IOFireWireController::doLockSpace(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 inLen,
                                         const UInt32 *newVal,  UInt32 &outLen, UInt32 *oldVal,UInt32 type,
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
        oldVal[0] = 0xdeadbabe;
        outLen = 4;
    }
    return ret;
}


////////////////////////////////////////////////////////////////////////////////
//
// processRcvPacket
//
//   Dispatch received Async packet based on tCode.
//
void IOFireWireController::processRcvPacket(UInt32 *data, int size)
{
#if 0
    int i;
kprintf("Received packet 0x%x size %d\n", data, size);
    for(i=0; i<size; i++) {
	kprintf("0x%x ", data[i]);
    }
    kprintf("\n");
#endif
    UInt32	tCode, tLabel;
    UInt32	quad0;
    UInt16	sourceID;

    // Get first quad.
    quad0 = *data;

    tCode = (quad0 & kFWPacketTCode) >> kFWPacketTCodePhase;
    tLabel = (quad0 & kFWAsynchTLabel) >> kFWAsynchTLabelPhase;
    sourceID = (data[1] & kFWAsynchSourceID) >> kFWAsynchSourceIDPhase;

    // Dispatch processing based on tCode.
    switch (tCode)
    {
        case kFWTCodeWriteQuadlet :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteQuadlet: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[3], 4);
            break;

        case kFWTCodeWriteBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("WriteBlock: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            processWriteRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);
            break;

        case kFWTCodeWriteResponse :
            if(fTrans[tLabel].fHandler) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
		cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase, 0, 0);
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
            DEBUGLOG("ReadQuadlet: addr 0x%x:0x%x\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >>
                     kFWAsynchDestinationOffsetHighPhase, data[2]);
#endif
            {
                UInt32 ret;
                FWAddress addr((data[1] & kFWAsynchDestinationOffsetHigh) >>
                                        kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOFWSpeed speed = FWSpeed(sourceID);
                IOMemoryDescriptor *buf = NULL;
		IOByteCount offset;
                ret = doReadSpace(sourceID, speed, addr, 4,
                                    &buf, &offset, (IOFWRequestRefCon)(tLabel | kRequestIsQuad));
               
                if(ret == kFWResponsePending)
                    break;
                if(NULL != buf) {
                    IOByteCount lengthOfSegment;
                    fFWIM->asyncReadQuadResponse(sourceID, speed, tLabel, ret,
			*(UInt32 *)buf->getVirtualSegment(offset, &lengthOfSegment));
                }
                else {
                    fFWIM->asyncReadQuadResponse(sourceID, speed, tLabel, ret, 0xdeadbeef);
                }
            }
            break;

        case kFWTCodeReadBlock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("ReadBlock: addr 0x%x:0x%x len %d\n", 
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2],
		(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);
#endif
            {
                IOReturn ret;
                int 					length = (data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase ;
                FWAddress 	addr((data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase, data[2]);
                IOFWSpeed 				speed = FWSpeed(sourceID);
                IOMemoryDescriptor *	buf = NULL;
				IOByteCount offset;

                ret = doReadSpace(sourceID, speed, addr, length,
                                    &buf, &offset, (IOFWRequestRefCon)(tLabel));
                if(ret == kFWResponsePending)
                    break;
                if(NULL != buf) {
                    fFWIM->asyncReadResponse(sourceID, speed,
                                       tLabel, ret, buf, offset, length);
                }
                else {
                    fFWIM->asyncReadResponse(sourceID, speed,
                                       tLabel, ret, fBadReadResponse, 0, 4);
                }
            }
            break;

        case kFWTCodeReadQuadletResponse :
            if(fTrans[tLabel].fHandler) {
                IOFWAsyncCommand * cmd = fTrans[tLabel].fHandler;
		cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
									(const void*)(data+3), 4);
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
		cmd->gotPacket((data[1] & kFWAsynchRCode)>>kFWAsynchRCodePhase,
                 (const void*)(data+4), (data[3] & kFWAsynchDataLength)>>kFWAsynchDataLengthPhase);
            }
            else {
#if (DEBUGGING_LEVEL > 0)
		DEBUGLOG("ReadBlock/LockResponse: label %d isn't in use!!\n", tLabel);
#endif
            }
            break;

        case kFWTCodeLock :
#if (DEBUGGING_LEVEL > 0)
            DEBUGLOG("Lock type %d: addr 0x%x:0x%x\n", 
		(data[3] & kFWAsynchExtendedTCode) >> kFWAsynchExtendedTCodePhase,
		(data[1] & kFWAsynchDestinationOffsetHigh) >> kFWAsynchDestinationOffsetHighPhase,
		data[2]);
#endif
            processLockRequest(sourceID, tLabel, data, &data[4],
			(data[3] & kFWAsynchDataLength) >> kFWAsynchDataLengthPhase);

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

IOReturn IOFireWireController::getCycleTime(UInt32 &cycleTime)
{
    // Have to take workloop lock, in case the hardware is sleeping.
    IOReturn res;
    fPendingQ.fSource->closeGate();
    res = fFWIM->getCycleTime(cycleTime);
    fPendingQ.fSource->openGate();
    return res;
}

IOReturn IOFireWireController::getBusCycleTime(UInt32 &busTime, UInt32 &cycleTime)
{
    // Have to take workloop lock, in case the hardware is sleeping.
    IOReturn res;
    UInt32 cycleSecs;
    fPendingQ.fSource->closeGate();
    res = fFWIM->getBusCycleTime(busTime, cycleTime);
    fPendingQ.fSource->openGate();
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

IOReturn IOFireWireController::allocAddress(IOFWAddressSpace *space)
{
    /*
     * Lots of scope for optimizations here, perhaps building a hash table for
     * addresses etc.
     * Drivers may want to override this if their hardware can match addresses
     * without CPU intervention.
     */
    IOReturn res;
    fPendingQ.fSource->closeGate();
    if(!fLocalAddresses->setObject(space))
        res = kIOReturnNoMemory;
    else
        res = kIOReturnSuccess;
    fPendingQ.fSource->openGate();
    return res;
}

void IOFireWireController::freeAddress(IOFWAddressSpace *space)
{
    fPendingQ.fSource->closeGate();
    fLocalAddresses->removeObject(space);
    fPendingQ.fSource->openGate();
}

void IOFireWireController::addAllocatedChannel(IOFWIsochChannel *channel)
{
    fPendingQ.fSource->closeGate();
    fAllocatedChannels->setObject(channel);
    fPendingQ.fSource->openGate();
}

void IOFireWireController::removeAllocatedChannel(IOFWIsochChannel *channel)
{
    fPendingQ.fSource->closeGate();
    fAllocatedChannels->removeObject(channel);
    fPendingQ.fSource->openGate();
}


IOFireWireDevice * IOFireWireController::nodeIDtoDevice(UInt32 generation, UInt16 nodeID)
{
    OSIterator *childIterator;
    IOFireWireDevice * found = NULL;

    if(!checkGeneration(generation))
        return NULL;
    
    childIterator = getClientIterator();

    if( childIterator) {
        OSObject *child;
        while( (child = childIterator->getNextObject())) {
            found = OSDynamicCast(IOFireWireDevice, child);
            if(found && !found->isInactive() && found->fNodeID == nodeID)
		break;
        }
        childIterator->release();
    }
    return found;
}


IOFWIsochChannel *IOFireWireController::createIsochChannel(
	bool doIRM, UInt32 bandwidth, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProc stopProc, void *stopRefCon)
{
	// NOTE: if changing this code, must also change IOFireWireUserClient::isochChannelAllocate()

    IOFWIsochChannel *channel;

    channel = new IOFWIsochChannel;
    if(!channel)
	return NULL;

    if(!channel->init(this, doIRM, bandwidth, prefSpeed, stopProc, stopRefCon)) {
	channel->release();
	channel = NULL;
    }
    return channel;
}

IOFWLocalIsochPort *IOFireWireController::createLocalIsochPort(bool talking,
        DCLCommandStruct *opcodes, DCLTaskInfo *info,
	UInt32 startEvent, UInt32 startState, UInt32 startMask)
{
    IOFWLocalIsochPort *port;
    IODCLProgram *program;

    program = fFWIM->createDCLProgram(talking, opcodes, info, startEvent, startState, startMask);
    if(!program)
	return NULL;

    port = new IOFWLocalIsochPort;
    if(!port) {
	program->release();
	return NULL;
    }

    if(!port->init(program, this)) {
	port->release();
	port = NULL;
    }

    return port;
}

IOFWDelayCommand *
IOFireWireController::createDelayedCmd(UInt32 uSecDelay, FWBusCallback func, void *refcon)
{
    IOFWDelayCommand *delay;
    //IOLog("Creating delay of %d\n", uSecDelay);
    delay = new IOFWDelayCommand;
    if(!delay)
        return NULL;

    if(!delay->initWithDelay(this, uSecDelay, func, refcon)) {
	delay->release();
        return NULL;
    }
    return delay;
}

/*
 * Create local FireWire address spaces for devices to access
 */
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

IOFWAddressSpace *
IOFireWireController::getAddressSpace(FWAddress address)
{
    fPendingQ.fSource->closeGate();
    IOFWAddressSpace * found;
    fSpaceIterator->reset();
    while( (found = (IOFWAddressSpace *) fSpaceIterator->getNextObject())) {
        if(found->contains(address))
            break;
    }
    fPendingQ.fSource->openGate();
    return found;
}

IOReturn IOFireWireController::AddUnitDirectory(IOLocalConfigDirectory *unitDir)
{
    IOReturn res;
    fPendingQ.fSource->closeGate();
    getRootDir()->addEntry(kConfigUnitDirectoryKey, unitDir);

    res = UpdateROM();
    if(res == kIOReturnSuccess)
        res = resetBus();
    fPendingQ.fSource->openGate();
    return res;
}

IOReturn IOFireWireController::RemoveUnitDirectory(IOLocalConfigDirectory *unitDir)
{
    IOReturn res;
    fPendingQ.fSource->closeGate();
    getRootDir()->removeSubDir(unitDir);

    res = UpdateROM();
    if(res == kIOReturnSuccess)
        res = resetBus();
    fPendingQ.fSource->openGate();
    return res;
}


////////////////////////////////////////////////////////////////////////////////
//
// UpdateROM()
//
//   Instantiate the local Config ROM.
//   Always causes at least one bus reset.
//

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
    generation = fROMHeader[2] & kFWBIBGeneration;
    generation += (1 << kFWBIBGenerationPhase);
    generation &= kFWBIBGeneration;
    if(generation < (2 << kFWBIBGenerationPhase))
        generation = (2 << kFWBIBGenerationPhase);

    fROMHeader[2] = (fROMHeader[2] & ~kFWBIBGeneration) | generation;
    
    rom = OSData::withBytes(&fROMHeader, sizeof(fROMHeader));
    fRootDir->compile(rom);

    // Now hack in correct CRC and length.
    hack = (UInt32 *)rom->getBytesNoCopy();
    numQuads = rom->getLength()/sizeof(UInt32) - 1;
    crc = FWComputeCRC16 (hack + 1, numQuads);
    *hack = (((sizeof(fROMHeader)/sizeof(UInt32)-1) <<
              kConfigBusInfoBlockLengthPhase) &
                                        kConfigBusInfoBlockLength) |
        ((numQuads << kConfigROMCRCLengthPhase) & kConfigROMCRCLength) |
        ((crc << kConfigROMCRCValuePhase) & kConfigROMCRCValue);

    localNode = getLocalNode(this);
    if(localNode)
        localNode->setProperty(gFireWireROM, rom);
    
#if 0
    {
        unsigned int i;
        IOLog("--------- FW Local ROM: --------\n");
        for(i=0; i<numQuads+1; i++)
            IOLog("ROM[%d] = 0x%x\n", i, hack[i]);
    }
#endif
    if(fROMAddrSpace) {
        freeAddress(fROMAddrSpace);
        fROMAddrSpace->release();
        fROMAddrSpace = NULL;
    }
    
    fROMAddrSpace = IOFWPseudoAddressSpace::simpleReadFixed(this,
        FWAddress(kCSRRegisterSpaceBaseAddressHi, kConfigROMBaseAddress),
        (numQuads+1)*sizeof(UInt32), rom->getBytesNoCopy());
    ret = allocAddress(fROMAddrSpace);
    if(kIOReturnSuccess == ret) {
        ret = fFWIM->updateROM(rom);
    }
    rom->release();
    return (ret);
}

// Route packet sending to FWIM if checks out OK
IOReturn IOFireWireController::asyncRead(UInt32 generation, UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
                            int speed, int label, int size, IOFWAsyncCommand *cmd)
{
    if(!checkGeneration(generation)) {
        return (kIOFireWireBusReset);
    }

    // Check if local node

    if(nodeID == fLocalNodeID) {
        UInt32 rcode;
        IOMemoryDescriptor *buf;
        IOByteCount offset, lengthOfSegment;
        IOFWSpeed temp = (IOFWSpeed)speed;
        rcode = doReadSpace(nodeID, temp, FWAddress(addrHi, addrLo), size,
                              &buf, &offset, (IOFWRequestRefCon)label);
        if(rcode == kFWResponseComplete)
            cmd->gotPacket(rcode, buf->getVirtualSegment(offset, &lengthOfSegment), size);
        else
            cmd->gotPacket(rcode, NULL, 0);
        return kIOReturnSuccess;
    }
    else
        return fFWIM->asyncRead(nodeID, addrHi, addrLo, speed, label, size, cmd);
}

IOReturn IOFireWireController::asyncWrite(UInt32 generation, UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
            int speed, int label, IOMemoryDescriptor *buf, IOByteCount offset,
            int size, IOFWAsyncCommand *cmd)
{
    if(!checkGeneration(generation)) {
        return (kIOFireWireBusReset);
    }

    // Check if local node
    if(nodeID == fLocalNodeID) {
        UInt32 rcode;
        IOByteCount lengthOfSegment;
        IOFWSpeed temp = (IOFWSpeed)speed;
        rcode = doWriteSpace(nodeID, temp, FWAddress(addrHi, addrLo), size,
                             buf->getVirtualSegment(offset, &lengthOfSegment), (IOFWRequestRefCon)label);
        cmd->gotPacket(rcode, NULL, 0);
        return kIOReturnSuccess;
    }
    else
        return fFWIM->asyncWrite(nodeID, addrHi, addrLo, speed, label, buf, offset, size, cmd);
}

IOReturn IOFireWireController::asyncWrite(UInt32 generation, UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
                            int speed, int label, void *data, int size, IOFWAsyncCommand *cmd)
{
    if(!checkGeneration(generation)) {
        return (kIOFireWireBusReset);
    }

    // Check if local node
    if(nodeID == fLocalNodeID) {
        UInt32 rcode;
        IOFWSpeed temp = (IOFWSpeed)speed;
        rcode = doWriteSpace(nodeID, temp, FWAddress(addrHi, addrLo), size,
                             data, (IOFWRequestRefCon)label);
        cmd->gotPacket(rcode, NULL, 0);
        return kIOReturnSuccess;
    }
    else
        return fFWIM->asyncWrite(nodeID, addrHi, addrLo, speed, label, data, size, cmd);
}

IOReturn IOFireWireController::asyncLock(UInt32 generation, UInt16 nodeID, UInt16 addrHi, UInt32 addrLo,
                    int speed, int label, int type, void *data, int size, IOFWAsyncCommand *cmd)
{
    if(!checkGeneration(generation)) {
        return (kIOFireWireBusReset);
    }

    // Check if local node
    if(nodeID == fLocalNodeID) {
        UInt32 rcode;
        UInt32 retVals[2];
        UInt32 retSize = sizeof(retVals);
        IOFWSpeed temp = (IOFWSpeed)speed;
        IOFWRequestRefCon refcon = (IOFWRequestRefCon)(label | kRequestIsLock | (type << kRequestExtTCodeShift));
        rcode = doLockSpace(nodeID, temp, FWAddress(addrHi, addrLo), size,
                             (const UInt32 *)data, retSize, retVals, type, refcon);
        cmd->gotPacket(rcode, retVals, retSize);
        return kIOReturnSuccess;
    }
    else
        return fFWIM->asyncLock(nodeID, addrHi, addrLo, speed, label, type, data, size, cmd);
}


// Send async read response packets
// useful for pseudo address spaces that require servicing outside the FireWire work loop.
IOReturn IOFireWireController::asyncReadResponse(UInt32 generation, UInt16 nodeID, int speed,
                                   IOMemoryDescriptor *buf, IOByteCount offset, int size,
                                                 IOFWRequestRefCon refcon)
{
    IOReturn result;
    UInt32 params = (UInt32)refcon;
    UInt32 label = params & kRequestLabel;
    IOByteCount lengthOfSegment;

    fPendingQ.fSource->closeGate();
    if(!checkGeneration(generation))
        result = kIOFireWireBusReset;
    else if(params & kRequestIsQuad)
        result = fFWIM->asyncReadQuadResponse(nodeID, speed, label, kFWResponseComplete,
                                    *(UInt32 *)buf->getVirtualSegment(offset, &lengthOfSegment));
    else
        result = fFWIM->asyncReadResponse(nodeID, speed, label, kFWResponseComplete, buf, offset, size);
    fPendingQ.fSource->openGate();

    return result;
}

// How big (as a power of two) can packets sent to/received from the node be?
int IOFireWireController::maxPackLog(bool forSend, UInt16 nodeAddress) const
{
    int log;
    log = 9+FWSpeed(nodeAddress);
    if(forSend) {
        if(log > fMaxSendLog)
            log = fMaxSendLog;
    }
    else if(log > fMaxSendLog)
        log = fMaxRecvLog;
    return log;
}

// How big (as a power of two) can packets sent from A to B be?
int IOFireWireController::maxPackLog(UInt16 nodeA, UInt16 nodeB) const
{
    return 9+FWSpeed(nodeA, nodeB);
}

IOReturn IOFireWireController::handleAsyncTimeout(IOFWAsyncCommand *cmd)
{
    return fFWIM->handleAsyncTimeout(cmd);
}

IOReturn IOFireWireController::resetBus()
{
    IOReturn res = kIOReturnSuccess;

    AbsoluteTime now;
    UInt32 milliDelay;
    UInt64 nanoDelay;

    fPendingQ.fSource->closeGate();
    if(fBusState != kPendingReset) {
        clock_get_uptime(&now);
        SUB_ABSOLUTETIME(&now, &fResetTime);
        absolutetime_to_nanoseconds(now, &nanoDelay);
        milliDelay = nanoDelay/1000000;
        //IOLog("%ld milliSecs after last reset, state %d\n", milliDelay, fBusState);
    
        if(milliDelay < 2000) {
            if(fBusState == kWaitingPrune || fBusState == kWaitingScan)
                fDelayedStateChangeCmd->cancel(kIOReturnAborted);
                
            fBusState = kPendingReset;
            fDelayedStateChangeCmd->reinit(1000 * (2000-milliDelay), delayedStateChange, NULL);
            res = fDelayedStateChangeCmd->submit();
        }
        else
            res = fFWIM->resetBus();
    }
    fPendingQ.fSource->openGate();

    return res;
}

IOReturn IOFireWireController::makeRoot(UInt32 generation, UInt16 nodeID)
{
    IOReturn res = kIOReturnSuccess;
    nodeID &= 63;
    fPendingQ.fSource->closeGate();
    if(!checkGeneration(generation))
        res = kIOFireWireBusReset;
    else if( fRootNodeID != nodeID ) {
        // Send phy packet to set root hold off bit for node
        res = fFWIM->sendPHYPacket((kFWConfigurationPacketID << kFWPhyPacketIDPhase) |
                    (nodeID << kFWPhyPacketPhyIDPhase) | kFWPhyConfigurationR);
        if(kIOReturnSuccess == res)
            res = resetBus();
    }
    
    fPendingQ.fSource->openGate();

    return res;
}

bool IOFireWireController::isLockRequest(IOFWRequestRefCon refcon)
{
    return ((UInt32)refcon) & kRequestIsLock;
}

bool IOFireWireController::isQuadRequest(IOFWRequestRefCon refcon)
{
    return ((UInt32)refcon) & kRequestIsQuad;
}

UInt32 IOFireWireController::getExtendedTCode(IOFWRequestRefCon refcon)
{
    return((UInt32)refcon & kRequestExtTCodeMask) >> kRequestExtTCodeShift;
}
