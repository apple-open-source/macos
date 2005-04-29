/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/OSByteOrder.h>

extern "C" {
#include <kern/clock.h>
}

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/pccard/IOPCCard.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBOHCI.h"
#include "AppleUSBOHCIMemoryBlocks.h"

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController

#define NUM_BUFFER_PAGES	9   // 54
#define NUM_TDS			255 // 1500
#define NUM_EDS			256 // 1500
#define NUM_ITDS		192 // 1300

// TDs  per page == 85
// EDs  per page == 128
// ITDs per page == 64

static int GetEDType(AppleOHCIEndpointDescriptorPtr pED);
extern void print_td(AppleOHCIGeneralTransferDescriptorPtr pTD);
extern void print_itd(AppleOHCIIsochTransferDescriptorPtr x);

OSDefineMetaClassAndStructors(AppleUSBOHCI, IOUSBControllerV2)

bool 
AppleUSBOHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;

    _ohciBusState = kOHCIBusStateOff;
    _ohciAvailable = true;
    
    _intLock = IOLockAlloc();
    if (!_intLock)
        return(false);

    _wdhLock = IOSimpleLockAlloc();
    if (!_wdhLock)
        return(false);

    _uimInitialized = false;
    
    // Initialize our consumer and producer counts.  
    //
    _producerCount = 1;
    _consumerCount = 1;

    _controllerSpeed = kUSBDeviceSpeedFull;	

    return (true);
}


bool
AppleUSBOHCI::start( IOService * provider )
{
    UInt32	ext1;
    
    USBLog(5,"+%s[%p]::start", getName(), this);
    if( !super::start(provider))
        return false;

    // super::start sets _device or it fails
    initForPM(_device);

    // Set our initial time for root hub inactivity
    //
    clock_get_uptime(&_lastCheckedTime);
    
    USBLog(5,"-%s[%p]::start", getName(), this);

    return true;
}



void 
AppleUSBOHCI::SetVendorInfo(void)
{
    OSData		*vendProp, *deviceProp, *revisionProp;

    // get this chips vendID, deviceID, revisionID
    vendProp     = OSDynamicCast(OSData, _device->getProperty( "vendor-id" ));
    if (vendProp)
        _vendorID = *((UInt32 *) vendProp->getBytesNoCopy());
    deviceProp   = OSDynamicCast(OSData, _device->getProperty( "device-id" ));
    if (deviceProp)
        _deviceID = *((UInt32 *) deviceProp->getBytesNoCopy());
    revisionProp = OSDynamicCast(OSData, _device->getProperty( "revision-id" ));
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
}



IOReturn 
AppleUSBOHCI::UIMInitialize(IOService * provider)
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		lvalue;
    IOPhysicalAddress 	hcDoneHead;

    USBLog(5,"%s[%p]: initializing UIM", getName(), this);

    _device = OSDynamicCast(IOPCIDevice, provider);
    if(_device == NULL)
        return kIOReturnBadArgument;

    do {

        if (!(_deviceBase = provider->mapDeviceMemoryWithIndex(0)))
        {
            USBError(1,"%s[%p]: unable to get device memory", getName(), this);
            break;
        }

        USBLog(3, "%s: config @ %lx (%lx)", getName(),
              (long)_deviceBase->getVirtualAddress(),
              _deviceBase->getPhysicalAddress());

        SetVendorInfo();

        // Set up a filter interrupt source (this process both primary (thru filter function) and secondary (thru action function)
        // interrupts.
        //
        _filterInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                            AppleUSBOHCI::InterruptHandler,	
                                                                            AppleUSBOHCI::PrimaryInterruptFilter,
                                                                            provider );
                                                                            
        if ( !_filterInterruptSource )
        {
             USBError(1,"%s[%p]: unable to get filterInterruptEventSource", getName(), this);
             break;
        }
        
        err = _workLoop->addEventSource(_filterInterruptSource);
        if ( err != kIOReturnSuccess )
        {
             USBError(1,"%s[%p]: unable to add filter event source: 0x%x", getName(), this, err);
             break;
        }

        _genCursor = IONaturalMemoryCursor::withSpecification(PAGE_SIZE, PAGE_SIZE);
        if(!_genCursor)
            break;

        _isoCursor = IONaturalMemoryCursor::withSpecification(kUSBMaxFSIsocEndpointReqCount,  kUSBMaxFSIsocEndpointReqCount);
        if(!_isoCursor)
            break;

        /*
         * Initialize my data and the hardware
         */
        _errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);
	if (_errataBits & kErrataLucentSuspendResume)
        {
            OSData	*suspendProp;
            UInt32	portBitmap = 0;
            
            // We need to check to see if there are ports that we really should suspend
            //
            suspendProp     = OSDynamicCast(OSData, provider->getProperty( "AAPL,SuspendablePorts" ));
            if (suspendProp)
            {
                // Only allow suspend on certain ports
                //
                portBitmap = *((UInt32 *) suspendProp->getBytesNoCopy());
                _disablePortsBitmap = (0xffffffff & (~portBitmap));
            }
            else
                _disablePortsBitmap = (0xffffffff);
        }
        
        USBLog(5,"%s: errata bits=%lx", getName(), _errataBits);

        _pageSize = PAGE_SIZE;
        _pOHCIRegisters = (OHCIRegistersPtr) _deviceBase->getVirtualAddress();

#if (DEBUGGING_LEVEL > 2)
        dumpRegs();
#endif
        
        // enable the card
        lvalue = _device->configRead32(cwCommand);
        _device->configWrite32(cwCommand, (lvalue & 0xffff0000) | (cwCommandEnableBusMaster | cwCommandEnableMemorySpace));

        // Check to see if the hcDoneHead is not NULL.  If so, then we need to reset the controller
        //
        hcDoneHead = USBToHostLong(_pOHCIRegisters->hcDoneHead);
        if ( hcDoneHead != NULL )
        {
            USBError(1,"%s[%p]::UIMInitialize Non-NULL hcDoneHead: %p", getName(), this, hcDoneHead );

            // Reset it now
            //
            _pOHCIRegisters->hcCommandStatus = USBToHostLong(kOHCIHcCommandStatus_HCR);  // Reset OHCI
            IOSleep(3);
        }
        
        _pOHCIRegisters->hcControlCurrentED = 0;
        _pOHCIRegisters->hcControlHeadED = 0;
        IOSync();

        // Set up HCCA.
        _pHCCA = (Ptr) IOMallocContiguous(kHCCAsize, kHCCAalignment, &_hccaPhysAddr);
        if (!_pHCCA)
        {
            USBError(1,"%s[%p]: Unable to allocate memory (2)", getName(), this);
            err = kIOReturnNoMemory;
            break;
        }

        OSWriteLittleInt32(&_pOHCIRegisters->hcHCCA, 0, _hccaPhysAddr);
        IOSync();

      // Set the HC to write the donehead to the HCCA, and enable interrupts
        _pOHCIRegisters->hcInterruptStatus = HostToUSBLong(kOHCIHcInterrupt_WDH);
        IOSync();

	// Enable the interrupt delivery.
	_workLoop->enableAllInterrupts();

        _rootHubFuncAddress = 1;

        // set up Interrupt transfer tree
        if ((err = IsochronousInitialize()))	break;
        if ((err = InterruptInitialize()))	break;
	if ((err = BulkInitialize()))		break;
        if ((err = ControlInitialize()))	break;

        // Set up hcFmInterval.
        UInt32	hcFSMPS;				// in register hcFmInterval
        UInt32	hcFI;					// in register hcFmInterval
        UInt32	hcPS;					// in register hcPeriodicStart
        
        hcFI = USBToHostLong(_pOHCIRegisters->hcFmInterval) & kOHCIHcFmInterval_FI;
        // this formula is from the OHCI spec, section 5.4
        hcFSMPS = ((((hcFI-kOHCIMax_OverHead) * 6)/7) << kOHCIHcFmInterval_FSMPSPhase);
        hcPS = (hcFI * 9) / 10;			// per spec- 90%
	_pOHCIRegisters->hcFmInterval = HostToUSBLong(hcFI | hcFSMPS);
        _pOHCIRegisters->hcPeriodicStart = HostToUSBLong(hcPS);

        IOSync();

        // Work around the Philips part which does weird things when a device is plugged in at boot
        //
        if (_errataBits & kErrataNeedsPortPowerOff)
        {
            USBError(1, "%s[%p]::UIMInitialize error, turning off power to ports to clear", getName(), this);
            OHCIRootHubPower(0 /* kOff */);
            // No need to turn the power back on here, the reset does that anyway.
        }
        
        // Just so we all start from the same place, reset the OHCI.
        _pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
        IOSync();

      // Set OHCI to operational state and enable processing of control list.
        _pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
					    | kOHCIHcControl_CLE | kOHCIHcControl_BLE
					    | kOHCIHcControl_PLE | kOHCIHcControl_IE);
        IOSync();

        // Initialize the Root Hub registers
	if (_errataBits & kErrataDisableOvercurrent)
	    _pOHCIRegisters->hcRhDescriptorA |= HostToUSBLong(kOHCIHcRhDescriptorA_NOCP);
	_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE); // should be SRWE which should be identical to DRWE
        IOSync();

        OHCIRootHubPower(1 /* kOn */);
	
	// enable interrupts
        _pOHCIRegisters->hcInterruptEnable = HostToUSBLong (kOHCIHcInterrupt_MIE | kOHCIDefaultInterrupts);
        IOSync();
        
        if (_errataBits & kErrataLSHSOpti)
            OptiLSHSFix();

        _uimInitialized = true;
        
        return(kIOReturnSuccess);

    } while (false);

    USBError(1, "%s[%p]::UIMInitialize error(%x)", getName(), this, err);
    UIMFinalize();

    if (_filterInterruptSource) 
    {
        _filterInterruptSource->release();
        _filterInterruptSource = NULL;
    }

    return(err);
}



IOReturn 
AppleUSBOHCI::UIMFinalize(void)
{
    USBLog (3, "%s[%p]: @ %lx (%lx)(shutting down HW)",getName(),this, 
              (long)_deviceBase->getVirtualAddress(),
              _deviceBase->getPhysicalAddress());

    // Disable the interrupt delivery
    //
    _workLoop->disableAllInterrupts();

    // If we are NOT being terminated, then talk to the OHCI controller and
    // set up all the registers to be off
    //
    if ( !isInactive() )
    {
        // Disable All OHCI Interrupts
        _pOHCIRegisters->hcInterruptDisable = HostToUSBLong(kOHCIHcInterrupt_MIE);
        IOSync();
    
        // Place the USB bus into the Reset State
        _pOHCIRegisters->hcControl = HostToUSBLong((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
        IOSync();
    
        //  need to wait at least 1ms here
        IOSleep(2);
    
        // Take away the controllers ability be a bus master.
        _device->configWrite32(cwCommand, cwCommandEnableMemorySpace);
    
        // Clear all Processing Registers
        _pOHCIRegisters->hcHCCA = 0;
        _pOHCIRegisters->hcControlHeadED = 0;
        _pOHCIRegisters->hcControlCurrentED = 0;
        _pOHCIRegisters->hcBulkHeadED = 0;
        _pOHCIRegisters->hcBulkCurrentED = 0;
        IOSync();
    
        // turn off the global power
        // FIXME check for per-port vs. Global power control
        OHCIRootHubPower(0 /* kOff */);
	
	// go ahead and reset the controller
	_pOHCIRegisters->hcCommandStatus = HostToUSBLong(kOHCIHcCommandStatus_HCR);  	// Reset OHCI
	IOSync();
	IOSleep(1);			// the spec says 10 microseconds
    }

    _pFreeITD = NULL;
    _pLastFreeITD = NULL;
    if (_itdMBHead)
    {
	AppleUSBOHCIitdMemoryBlock *curBlock = _itdMBHead;
	AppleUSBOHCIitdMemoryBlock *nextBlock;
	
	_itdMBHead = NULL;
	while (curBlock)
	{
	    nextBlock = curBlock->GetNextBlock();
	    curBlock->release();
	    curBlock = nextBlock;
	}
    }
    
    _pFreeTD = NULL;
    _pLastFreeTD = NULL;
    if (_gtdMBHead)
    {
	AppleUSBOHCIgtdMemoryBlock *curBlock = _gtdMBHead;
	AppleUSBOHCIgtdMemoryBlock *nextBlock;
	
	_gtdMBHead = NULL;
	while (curBlock)
	{
	    nextBlock = curBlock->GetNextBlock();
	    curBlock->release();
	    curBlock = nextBlock;
	}
    }
    
    _pFreeED = NULL;
    _pLastFreeED = NULL;
    if (_edMBHead)
    {
	AppleUSBOHCIedMemoryBlock *curBlock = _edMBHead;
	AppleUSBOHCIedMemoryBlock *nextBlock;
	
	_edMBHead = NULL;
	while (curBlock)
	{
	    nextBlock = curBlock->GetNextBlock();
	    curBlock->release();
	    curBlock = nextBlock;
	}
    }
    
    // Free the HCCA memory
    //
    IOFree ( _pHCCA, kHCCAsize );
    
    // Remove the interruptEventSource we created
    //
    if ( _filterInterruptSource )
    {
        _workLoop->removeEventSource(_filterInterruptSource);
        _filterInterruptSource->release();
        _filterInterruptSource = NULL;
    }
    
    // Release the memory cursors
    //
    if ( _genCursor )
    {
        _genCursor->release();
        _genCursor = NULL;
    }
    
    if ( _isoCursor )
    {
        _isoCursor->release();
        _isoCursor = NULL;
    }

    _uimInitialized = false;
    
    return(kIOReturnSuccess);
}



/*
 * got an error on a TD with no completion routine.
 * Search for a later TD on the same end point which does have one,
 * so we can tell upper layes of the error.
 */
void 
AppleUSBOHCI::doCallback(AppleOHCIGeneralTransferDescriptorPtr	nextTD,
                           UInt32			    	transferStatus,
                           UInt32			   	 bufferSizeRemaining)
{
    AppleOHCIGeneralTransferDescriptorPtr	pCurrentTD, pTempTD;
    AppleOHCIEndpointDescriptorPtr		pED;
    IOPhysicalAddress				PhysAddr;

    pED = nextTD->pEndpoint;
    pED->pShared->flags |= HostToUSBLong(kOHCIEDControl_K);				// mark endpoint as skipped
    PhysAddr = (IOPhysicalAddress) USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask;
    nextTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(PhysAddr);

    pCurrentTD = nextTD;
    if(pCurrentTD == NULL) 
    {
        USBLog(3, "%s[%p]::doCallback No transfer descriptors!", getName(), this);
	return;
    }
    USBLog(5, "AppleUSBOHCI::doCallback: pCurrentTD = %p, pED->pLogicalTailP = %p", pCurrentTD, pED->pLogicalTailP);
    while (pCurrentTD != pED->pLogicalTailP)
    {
        // UnlinkTD! But don't lose the data toggle or halt bit
        //
        pED->pShared->tdQueueHeadPtr = pCurrentTD->pShared->nextTD | (pED->pShared->tdQueueHeadPtr & HostToUSBLong(~kOHCIHeadPointer_headP));
        USBLog(5, "AppleUSBOHCI::doCallback- queueheadptr is now %p", pED->pShared->tdQueueHeadPtr);
        
        bufferSizeRemaining += findBufferRemaining (pCurrentTD);

        // make sure this TD won't be added to any future buffer
	// remaining calculations
        pCurrentTD->pShared->currentBufferPtr = NULL;

        if (pCurrentTD->uimFlags & kUIMFlagsCallbackTD)
        {
            IOUSBCompletion completion;
	    
	    if (transferStatus == kOHCIGTDConditionDataUnderrun)
	    {
                USBLog(5, "AppleUSBOHCI::doCallback- found callback TD, setting queuehead to  %p", pED->pShared->tdQueueHeadPtr & HostToUSBLong(~kOHCIHeadPointer_H));
		pED->pShared->tdQueueHeadPtr = pED->pShared->tdQueueHeadPtr & HostToUSBLong(~kOHCIHeadPointer_H);
                transferStatus = 0;
	    }
            // zero out callback first then call it
            completion = pCurrentTD->command->GetUSLCompletion();
            pCurrentTD->uimFlags &= ~kUIMFlagsCallbackTD;
            DeallocateTD(pCurrentTD);
            pED->pShared->flags &= ~HostToUSBLong(kOHCIEDControl_K);				// mark endpoint as not skipped
            Complete(completion,
                     TranslateStatusToUSBError(transferStatus),
                     bufferSizeRemaining);
            bufferSizeRemaining = 0;
            return;
        }

        pTempTD = pCurrentTD->pLogicalNext;
        DeallocateTD(pCurrentTD);
        pCurrentTD = pTempTD;
    }
}



// FIXME add page size to param list
UInt32 
AppleUSBOHCI::findBufferRemaining (AppleOHCIGeneralTransferDescriptorPtr pCurrentTD)
{
    UInt32                      pageMask;
    UInt32                      bufferSizeRemaining;


    pageMask = ~(_pageSize - 1);

    if (pCurrentTD->pShared->currentBufferPtr == 0)
    {
        bufferSizeRemaining = 0;
    }
    else if ((USBToHostLong(pCurrentTD->pShared->bufferEnd) & (pageMask)) ==
             (USBToHostLong(pCurrentTD->pShared->currentBufferPtr)& (pageMask)))
    {
        // we're on the same page
        bufferSizeRemaining =
        (USBToHostLong (pCurrentTD->pShared->bufferEnd) & ~pageMask) -
        (USBToHostLong (pCurrentTD->pShared->currentBufferPtr) & ~pageMask) + 1;
    }
    else
    {
        bufferSizeRemaining =
        ((USBToHostLong(pCurrentTD->pShared->bufferEnd) & ~pageMask) + 1)  +
        (_pageSize - (USBToHostLong(pCurrentTD->pShared->currentBufferPtr) & ~pageMask));
    }

    return (bufferSizeRemaining);
}



IOReturn 
AppleUSBOHCI::ControlInitialize(void)
{
    AppleOHCIEndpointDescriptorPtr   pED, pED2;

    // Create ED, mark it skipped and assign it to Control tail
    //
    pED = AllocateED();
    if ( pED == NULL )
        return kIOReturnNoMemory;
    pED->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    pED->pShared->nextED = 0;	// End of list
    _pControlTail = pED;

    // Create ED, mark it skipped and assign it to Control head
    //
    pED2 = AllocateED();
    if ( pED2 == NULL )
        return kIOReturnNoMemory;
    pED2->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    _pControlHead = pED2;
    _pOHCIRegisters->hcControlHeadED = HostToUSBLong ((UInt32) pED2->pPhysical);

    // Have Control head ED point to Control tail ED
    //
    pED2->pShared->nextED = HostToUSBLong ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::BulkInitialize (void)
{
    AppleOHCIEndpointDescriptorPtr   pED, pED2;

    // Create ED, mark it skipped and assign it to Bulk tail
    //
    pED = AllocateED();
    if ( pED == NULL )
        return kIOReturnNoMemory;
    pED->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    pED->pShared->nextED = NULL;	// End of list
    _pBulkTail = pED;

    // Create ED, mark it skipped and assign it to Bulk head
    //
    pED2 = AllocateED();
    if ( pED2 == NULL )  return kIOReturnNoMemory;
    pED2->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    _pBulkHead = pED2;
    _pOHCIRegisters->hcBulkHeadED = HostToUSBLong ((UInt32) pED2->pPhysical);

    // Have Bulk head ED point to Bulk tail ED
    //
    pED2->pShared->nextED = HostToUSBLong ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    
    return kIOReturnSuccess;

}



IOReturn 
AppleUSBOHCI::IsochronousInitialize(void)
{
    AppleOHCIEndpointDescriptorPtr   pED, pED2;

    // Create ED mark it skipped and assign it to Isoch tail
    //
    pED = AllocateED();
    if ( pED == NULL )
        return kIOReturnNoMemory;
    pED->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    pED->pShared->nextED = NULL;	// End of list
    _pIsochTail = pED;

    // Create ED mark it skipped and assign it to Isoch head
    //
    pED2 = AllocateED();
    if ( pED2 == NULL )
        return kIOReturnNoMemory;
    pED2->pShared->flags = HostToUSBLong (kOHCIEDControl_K);
    _pIsochHead = pED2;


    // have Isoch head ED point to Isoch tail ED
    //
    pED2->pShared->nextED = HostToUSBLong ((UInt32) pED->pPhysical);
    pED2->pLogicalNext = pED;
    _isochBandwidthAvail = kUSBMaxFSIsocEndpointReqCount;

    return kIOReturnSuccess;
}



//Initializes the HCCA Interrupt list with statically
//disabled ED's to form the Interrupt polling queues
IOReturn 
AppleUSBOHCI::InterruptInitialize (void)
{
    UInt32                      dummyControl;
    int                         i, p, q, z;
    AppleOHCIEndpointDescriptorPtr   pED, pIsochHead;

    // create UInt32 with same dword0 for use with searching and
    // tracking, skip should be set, and open area should be marked
    dummyControl = kOHCIEDControl_K;
    dummyControl |= 0;   //should be kOHCIFakeED
    dummyControl = HostToUSBLong (dummyControl);
    pIsochHead = (AppleOHCIEndpointDescriptorPtr) _pIsochHead;

    // do 31 times
    // change to 65 and make isoch head the last one.?????
    for (i = 0; i < 63; i++)
    {
        // allocate Endpoint descriptor
        pED = AllocateED();
        if (pED == NULL)
        {
            return (kIOReturnNoMemory);
        }
        // mark skipped,some how mark as a False endpoint zzzzz
        else
        {
            pED->pShared->flags = dummyControl;
            pED->pShared->nextED = NULL;	// End of list
            _pInterruptHead[i].pHead = pED;
            _pInterruptHead[i].pHeadPhysical = pED->pPhysical;
            _pInterruptHead[i].nodeBandwidth = 0;
        }

        if (i < 32)
            ((UInt32 *)_pHCCA)[i] = (UInt32) HostToUSBLong((UInt32) _pInterruptHead[i].pHeadPhysical);
    }

    p = 0;
    q = 32;
    // FIXME? ERIC
    for (i = 0; i < (32 +16 + 8 + 4 + 2); i++)
    {
        if (i < q/2+p)
            z = i + q;
        else
            z = i + q/2;
        if (i == p+q-1)
        {
            p = p + q;
            q = q/2;
        }
        // point endpoint descriptor to corresponding 8ms descriptor
        pED = _pInterruptHead[i].pHead;
        pED->pShared->nextED =  HostToUSBLong (_pInterruptHead[z].pHeadPhysical);
        pED->pLogicalNext = _pInterruptHead[z].pHead;
        _pInterruptHead[i].pTail = (AppleOHCIEndpointDescriptorPtr) pED->pLogicalNext;
    }
    i = 62;
    pED = _pInterruptHead[i].pHead;
    pED->pShared->nextED = HostToUSBLong (pIsochHead->pPhysical);
    pED->pLogicalNext = _pIsochHead;
    _pInterruptHead[i].pTail = (AppleOHCIEndpointDescriptorPtr) pED->pLogicalNext;

    // point Isochronous head to last endpoint
    return kIOReturnSuccess;
}


AppleOHCIIsochTransferDescriptorPtr 
AppleUSBOHCI::AllocateITD(void)
{
    AppleOHCIIsochTransferDescriptorPtr freeITD;

    // pop a TD off of FreeITD list
    //
    freeITD = _pFreeITD;

    if (freeITD == NULL)
    {
	// i need to allocate another page of EDs
	AppleUSBOHCIitdMemoryBlock 	*memBlock;
	UInt32				numTDs, i;
	
	memBlock = AppleUSBOHCIitdMemoryBlock::NewMemoryBlock();
	if (!memBlock)
	{
	    USBLog(1, "%s[%p]::AllocateTD - unable to allocate a new memory block!", getName(), this);
	    return NULL;
	}
	// link it in to my list of ED memory blocks
	memBlock->SetNextBlock(_itdMBHead);
	_itdMBHead = memBlock;
	numTDs = memBlock->NumITDs();
	_pLastFreeITD = memBlock->GetITD(0);
	_pFreeITD = _pLastFreeITD;
	_pFreeITD->pPhysical = memBlock->GetSharedPhysicalPtr(0);
	_pFreeITD->pShared = memBlock->GetSharedLogicalPtr(0);
	USBLog(7, "%s[%p]::AllocateITD - _pFreeITD (%p), _pFreeITD->pPhysical(%p), _pFreeITD->pShared (%p), GetITDFromPhysical(%p)", 
		getName(), this, _pFreeITD, _pFreeITD->pPhysical, _pFreeITD->pShared, AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical(_pFreeITD->pPhysical));
	for (i=1; i < numTDs; i++)
	{
	    freeITD = memBlock->GetITD(i);
	    if (!freeITD)
	    {
		USBLog(1, "%s[%p]::AllocateTD - hmm. ran out of TDs in a memory block", getName(), this);
		freeITD = _pFreeITD;
		break;
	    }
	    freeITD->pLogicalNext = _pFreeITD;
	    freeITD->pPhysical = memBlock->GetSharedPhysicalPtr(i);
	    freeITD->pShared = memBlock->GetSharedLogicalPtr(i);
	    _pFreeITD = freeITD;
	    // in a normal loop termination, freeQH and _pFreeQH are the same, just like when we don't use this code
	}
    }
    
    for(int i=0; i<8; i++)
    {
        freeITD->pShared->offset[i] = 0;
    }

    _pFreeITD = freeITD->pLogicalNext;
    freeITD->pLogicalNext = NULL;
    freeITD->uimFlags = 0;

    return freeITD;
}



AppleOHCIGeneralTransferDescriptorPtr 
AppleUSBOHCI::AllocateTD(void)
{
    AppleOHCIGeneralTransferDescriptorPtr freeTD;

    // pop a TD off of FreeTD list
    //if FreeTD == NULL return NULL
    // should we check if ED is full and if not access that????
    freeTD = _pFreeTD;

    if (freeTD == NULL)
    {
	// i need to allocate another page of EDs
	AppleUSBOHCIgtdMemoryBlock 	*memBlock;
	UInt32				numTDs, i;
	
	memBlock = AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock();
	if (!memBlock)
	{
	    USBLog(1, "%s[%p]::AllocateTD - unable to allocate a new memory block!", getName(), this);
	    return NULL;
	}
	// link it in to my list of ED memory blocks
	memBlock->SetNextBlock(_gtdMBHead);
	_gtdMBHead = memBlock;
	numTDs = memBlock->NumGTDs();
	_pLastFreeTD = memBlock->GetGTD(0);
	_pFreeTD = _pLastFreeTD;
	_pFreeTD->pPhysical = memBlock->GetSharedPhysicalPtr(0);
	_pFreeTD->pShared = memBlock->GetSharedLogicalPtr(0);
	USBLog(7, "%s[%p]::AllocateTD - _pFreeTD (%p), _pFreeTD->pPhysical(%p), _pFreeTD->pShared (%p), GetGTDFromPhysical(%p)", 
		getName(), this, _pFreeTD, _pFreeTD->pPhysical, _pFreeTD->pShared, AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(_pFreeTD->pPhysical));
	for (i=1; i < numTDs; i++)
	{
	    freeTD = memBlock->GetGTD(i);
	    if (!freeTD)
	    {
		USBLog(1, "%s[%p]::AllocateTD - hmm. ran out of TDs in a memory block", getName(), this);
		freeTD = _pFreeTD;
		break;
	    }
	    freeTD->pLogicalNext = _pFreeTD;
	    freeTD->pPhysical = memBlock->GetSharedPhysicalPtr(i);
	    freeTD->pShared = memBlock->GetSharedLogicalPtr(i);
	    _pFreeTD = freeTD;
	    // in a normal loop termination, freeQH and _pFreeQH are the same, just like when we don't use this code
	}
    }
    
    _pFreeTD = freeTD->pLogicalNext;
    freeTD->pLogicalNext = NULL;
    freeTD->uimFlags = 0;
    freeTD->lastFrame = 0;		// used in timeout logic
    freeTD->lastRemaining = 0;		// used in timeout logic
    
    return freeTD;
}



AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::AllocateED(void)
{
    AppleOHCIEndpointDescriptorPtr freeED;

    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeED = _pFreeED;

    if (freeED == NULL)
    {
	// i need to allocate another page of EDs
	AppleUSBOHCIedMemoryBlock 	*memBlock;
	UInt32				numEDs, i;
	
	memBlock = AppleUSBOHCIedMemoryBlock::NewMemoryBlock();
	if (!memBlock)
	{
	    USBLog(1, "%s[%p]::AllocateED - unable to allocate a new memory block!", getName(), this);
	    return NULL;
	}
	// link it in to my list of ED memory blocks
	memBlock->SetNextBlock(_edMBHead);
	_edMBHead = memBlock;
	numEDs = memBlock->NumEDs();
	_pLastFreeED = memBlock->GetED(0);
	_pFreeED = _pLastFreeED;
	_pFreeED->pPhysical = memBlock->GetSharedPhysicalPtr(0);
	_pFreeED->pShared = memBlock->GetSharedLogicalPtr(0);
        USBLog(7, "%s[%p]::AllocateED - _pFreeED (%p), _pFreeED->pPhysical(%p), _pFreeED->pShared (%p)",
               getName(), this, _pFreeED, _pFreeED->pPhysical, _pFreeED->pShared);
	for (i=1; i < numEDs; i++)
	{
	    freeED = memBlock->GetED(i);
	    if (!freeED)
	    {
		USBLog(1, "%s[%p]::AllocateED - hmm. ran out of EDs in a memory block", getName(), this);
		freeED = _pFreeED;
		break;
	    }
	    freeED->pLogicalNext = _pFreeED;
	    freeED->pPhysical = memBlock->GetSharedPhysicalPtr(i);
	    freeED->pShared = memBlock->GetSharedLogicalPtr(i);
	    _pFreeED = freeED;
	    // in a normal loop termination, freeQH and _pFreeQH are the same, just like when we don't use this code
	}
    }
    _pFreeED = freeED->pLogicalNext;
    freeED->pLogicalNext = NULL;
    return freeED;
}



IOReturn 
AppleUSBOHCI::DeallocateITD (AppleOHCIIsochTransferDescriptorPtr pTD)
{
    UInt32		physical;

    // zero out all unnecessary fields
    physical = pTD->pPhysical;
    //bzero(pTD, sizeof(*pTD));
    pTD->pLogicalNext = NULL;
    pTD->pPhysical = physical;
    if (_pFreeITD)
    {
        _pLastFreeITD->pLogicalNext = pTD;
        _pLastFreeITD = pTD;
    }
    else
    {
        // list is currently empty
        _pLastFreeITD = pTD;
        _pFreeITD = pTD;
    }
    return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::DeallocateTD (AppleOHCIGeneralTransferDescriptorPtr pTD)
{
    UInt32		physical;

    //zero out all unnecessary fields
    physical = pTD->pPhysical;
    //bzero(pTD, sizeof(*pTD));
    pTD->pLogicalNext = NULL;
    pTD->pPhysical = physical;

    if (_pFreeTD)
    {
        _pLastFreeTD->pLogicalNext = pTD;
        _pLastFreeTD = pTD;
    } else {
        // list is currently empty
        _pLastFreeTD = pTD;
        _pFreeTD = pTD;
    }
    return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::DeallocateED (AppleOHCIEndpointDescriptorPtr pED)
{
    UInt32		physical;

    //zero out all unnecessary fields
    physical = pED->pPhysical;
    //bzero(pED, sizeof(*pED));
    pED->pPhysical = physical;
    pED->pLogicalNext = NULL;

    if (_pFreeED){
        _pLastFreeED->pLogicalNext = pED;
        _pLastFreeED = pED;
    } else {
        // list is currently empty
        _pLastFreeED = pED;
        _pFreeED = pED;
    }
    return (kIOReturnSuccess);
}



int 
GetEDType(AppleOHCIEndpointDescriptorPtr pED)
{
    return ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_F) >> kOHCIEDControl_FPhase);
}



IOReturn 
AppleUSBOHCI::RemoveAllTDs (AppleOHCIEndpointDescriptorPtr pED)
{
    RemoveTDs(pED);

    if (GetEDType(pED) == kOHCIEDFormatGeneralTD) {
        // remove the last "dummy" TD
        DeallocateTD(
                            (AppleOHCIGeneralTransferDescriptorPtr) pED->pLogicalTailP);
    }
    else
    {
        DeallocateITD(
                             (AppleOHCIIsochTransferDescriptorPtr) pED->pLogicalTailP);
    }
    pED->pLogicalTailP = NULL;

    return (0);
}



//removes all but the last of the TDs
IOReturn 
AppleUSBOHCI::RemoveTDs(AppleOHCIEndpointDescriptorPtr pED)
{
    AppleOHCIGeneralTransferDescriptorPtr	pCurrentTD, lastTD;
    UInt32					bufferSizeRemaining = 0;
    AppleOHCIIsochTransferDescriptorPtr		pITD, pITDLast;
	
    if (GetEDType(pED) == kOHCIEDFormatGeneralTD)
    {
        //process and deallocate GTD's
        pCurrentTD = (AppleOHCIGeneralTransferDescriptorPtr) (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
        pCurrentTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical((IOPhysicalAddress) pCurrentTD);

        lastTD = (AppleOHCIGeneralTransferDescriptorPtr) pED->pLogicalTailP;
        pED->pLogicalHeadP = pED->pLogicalTailP;

        while (pCurrentTD != lastTD)
        {
            if (pCurrentTD == NULL)
                return (-1);

            //take out TD from list
            pED->pShared->tdQueueHeadPtr = pCurrentTD->pShared->nextTD;
            pED->pLogicalHeadP = pCurrentTD->pLogicalNext;	

            bufferSizeRemaining += findBufferRemaining(pCurrentTD);

            // if (pCurrentTD->completion.action != NULL)
            if (pCurrentTD->uimFlags & kUIMFlagsCallbackTD)
            {
                IOUSBCompletion completion = pCurrentTD->command->GetUSLCompletion();
                // remove callback flag before calling
                pCurrentTD->uimFlags &= ~kUIMFlagsCallbackTD;
                Complete(completion, kIOReturnAborted, bufferSizeRemaining);
                bufferSizeRemaining = 0;
            }

            DeallocateTD(pCurrentTD);
            pCurrentTD = (AppleOHCIGeneralTransferDescriptorPtr) pED->pLogicalHeadP;		
        }		
    }
    else
    {
        UInt32 phys;
        phys = (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
        pITD = AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical(phys);
        pITDLast = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;

        while (pITD != pITDLast)
        {
            AppleOHCIIsochTransferDescriptorPtr pPrevITD;
            if (pITD == NULL)
                return (-1);

            //take out TD from list             
            pED->pShared->tdQueueHeadPtr = pITD->pShared->nextTD;
            pED->pLogicalHeadP = pITD->pLogicalNext;

            ProcessCompletedITD (pITD, kIOReturnAborted);
            pPrevITD = pITD;
            pITD = pITD->pLogicalNext;
            // deallocate td
            DeallocateITD(pPrevITD);
        }
    }

    return (0);
}



void 
AppleUSBOHCI::ProcessCompletedITD (AppleOHCIIsochTransferDescriptorPtr pITD, IOReturn status)
{

    IOUSBIsocFrame *	pFrames;
    IOUSBLowLatencyIsocFrame * pLLFrames;
    int			i;
    int			frameCount;
    IOReturn		aggregateStatus = kIOReturnSuccess;
    IOReturn		frameStatus;
    UInt32		itdConditionCode;
    bool		hadUnderrun = false;
    UInt32		delta;
    UInt32		curFrame;
    AbsoluteTime	timeStop, timeStart;
    UInt64       	timeElapsed;
    
    pFrames = pITD->pIsocFrame;
    pLLFrames = (IOUSBLowLatencyIsocFrame *) pITD->pIsocFrame;
    
    frameCount = (USBToHostLong(pITD->pShared->flags) & kOHCIITDControl_FC) >> kOHCIITDControl_FCPhase;
        
    itdConditionCode = (USBToHostLong(pITD->pShared->flags) & kOHCIITDControl_CC) >> kOHCIITDControl_CCPhase;
    

    // USBLog(3, "%s[%p]::ProcessCompletedITD: filter interrupt duration: %ld", getName(), this, (UInt32) timeElapsed);

    if (itdConditionCode == kOHCIITDConditionDataOverrun)
    {
		// The OHCI controller sets the status to DATAOVERRUN in the case where the TD could not go out because there was not time
		// left in the frame.  This is called a Time Error and is in Section 4.3.2.3.5.3 of the OHCI spec.  We will return this as a no bandwidth error.
		//
		status = kIOReturnNoBandwidth; 
		
		USBLog(5,"%s[%p]::ProcessCompletedITD: Time Error in Isoch xfer (%p) -- not enough time to send the whole TD, returning kIOReturnNoBandwidth (0x%x)", getName(), this, pITD, status);
    }
    
    // Do some calculations related to the low latency isoch TDs:
    //
    if ( (_filterInterruptCount != 0 ) &&
         ( (pITD->pType == kOHCIIsochronousInLowLatencyType) || 
           (pITD->pType == kOHCIIsochronousOutLowLatencyType) ) )
    {
        clock_get_uptime (&timeStop);
        timeStart = pLLFrames[pITD->frameNum].frTimeStamp;
        SUB_ABSOLUTETIME(&timeStop, &timeStart); 
        absolutetime_to_nanoseconds(timeStop, &timeElapsed); 
        
        if ( _lowLatencyIsochTDsProcessed != 0 )
        {
            USBLog(6, "%s[%p]::ProcessCompletedITD: LowLatency isoch TD's proccessed: %d, framesUpdated: %d, framesError: %d",  getName(), this, _lowLatencyIsochTDsProcessed, _framesUpdated, _framesError);
            USBLog(7, "%s[%p]::ProcessCompletedITD: delay in microsecs before callback (from hw interrupt time): %ld", getName(), this, (UInt32) timeElapsed / 1000);
            
            // SUB_ABSOLUTETIME(&_filterTimeStamp2, &_filterTimeStamp); 
            // absolutetime_to_nanoseconds(_filterTimeStamp2, &timeElapsed); 
            // USBLog(7, "%s[%p]::ProcessCompletedITD: filter interrupt duration: %ld", getName(), this, (UInt32) timeElapsed / 1000);
        }
        
        // These need to be updated only when we process a TD due to an interrupt
        //
        _lowLatencyIsochTDsProcessed = 0;
        _framesUpdated = 0;
        _framesError = 0; 
    }
    
    for (i=0; i <= frameCount; i++)
    {
        // Need to process low latency isoch differently than other isoc, as the frameList has an extra parameter (use pLLFrame instead of pFrame )
        //
        if ( (pITD->pType == kOHCIIsochronousInLowLatencyType) || (pITD->pType == kOHCIIsochronousOutLowLatencyType) )
        {
            UInt16 offset = USBToHostWord(pITD->pShared->offset[i]);
            
            // Check to see if we really processed this itd before:
            // 
            if ( (_filterInterruptCount != 0 ) &&
                    ( (pITD->pType == kOHCIIsochronousInLowLatencyType) || 
                    (pITD->pType == kOHCIIsochronousOutLowLatencyType) ) )
            {
                if ( (pLLFrames[pITD->frameNum + i].frStatus != (IOReturn) kUSBLowLatencyIsochTransferKey) ) 
                {
                    // USBLog(7,"%s[%p]::ProcessCompletedITD: frame processed at hw interrupt time: frame: %d, frReqCount: %d, frActCount: %d, frStatus: 0x%x frTimeStamp.lo: 0x%x",  getName(), this, i, pLLFrames[pITD->frameNum + i].frReqCount, pLLFrames[pITD->frameNum + i].frActCount, pLLFrames[pITD->frameNum + i].frStatus, pLLFrames[pITD->frameNum + i].frTimeStamp.lo);
                    USBLog(6,"%s[%p]::ProcessCompletedITD: frame processed at hw interrupt time: frame: %d,  frTimeStamp.lo: 0x%x",  getName(), this, i, pLLFrames[pITD->frameNum + i].frTimeStamp.lo);
                    
                }
            }
            
            
            if ( ((offset & kOHCIITDOffset_CC) >> kOHCIITDOffset_CCPhase) == kOHCIITDOffsetConditionNotAccessed)
            {
                USBLog(6,"%s[%p]::ProcessCompletedITD:  Isoch frame not accessed. Frame in request(1 based) %d, IsocFramePtr: %p, ITD: %p, Frames in this TD: %d, Relative frame in TD: %d",  getName(), this, pITD->frameNum + i + 1, pLLFrames, pITD, frameCount+1, i+1);
                pLLFrames[pITD->frameNum + i].frActCount = 0;
                pLLFrames[pITD->frameNum + i].frStatus = kOHCIITDConditionNotAccessedReturn;
            }
            else
            {
                pLLFrames[pITD->frameNum + i].frStatus = (offset & kOHCIITDPSW_CC) >> kOHCIITDPSW_CCPhase;
                
                // Successful isoch transmit sets the size field to zero,
                // successful receive sets size to actual packet size received.
                if ( (kIOReturnSuccess == pLLFrames[pITD->frameNum + i].frStatus) && 
                    ( (pITD->pType == kOHCIIsochronousOutType) || (pITD->pType == kOHCIIsochronousOutLowLatencyType) ) )
                    pLLFrames[pITD->frameNum + i].frActCount = pLLFrames[pITD->frameNum + i].frReqCount;
                else
                    pLLFrames[pITD->frameNum + i].frActCount = offset & kOHCIITDPSW_Size;
            }
            
            // Translate the OHCI Condition to one of the appropriate USB errors.  We use aggregateStatus to determine
            // later on whether there was an error in any of the frames.  If there was, then we set the completion error
            // to that reported in the aggregateStatus.  There is no priority in the aggregateStatus except that if there
            // is a data underrun AND another type of error, then we report the "other" error.
            //
            frameStatus = pLLFrames[pITD->frameNum + i].frStatus;
            
            
            if ( frameStatus != kIOReturnSuccess )
            {
                pLLFrames[pITD->frameNum + i].frStatus =  TranslateStatusToUSBError(frameStatus);
                
                if ( pLLFrames[pITD->frameNum + i].frStatus == kIOReturnUnderrun )
                    hadUnderrun = true;
                else
                    aggregateStatus = pLLFrames[pITD->frameNum + i].frStatus;
            }
        }    
        else
        {
            // Process non-low latency isoch 
            //
            UInt16 offset = USBToHostWord(pITD->pShared->offset[i]);
            
            if ( ((offset & kOHCIITDOffset_CC) >> kOHCIITDOffset_CCPhase) == kOHCIITDOffsetConditionNotAccessed)
            {
                USBLog(6,"%s[%p]::ProcessCompletedITD:  Isoch frame not accessed. Frame in request(1 based) %d, IsocFramePtr: %p, ITD: %p, Frames in this TD: %d, Relative frame in TD: %d",  getName(), this, pITD->frameNum + i + 1, pFrames, pITD, frameCount+1, i+1);
                pFrames[pITD->frameNum + i].frActCount = 0;
                pFrames[pITD->frameNum + i].frStatus = kOHCIITDConditionNotAccessedReturn;
            }
            else
            {
                pFrames[pITD->frameNum + i].frStatus = (offset & kOHCIITDPSW_CC) >> kOHCIITDPSW_CCPhase;
                
                // Successful isoch transmit sets the size field to zero,
                // successful receive sets size to actual packet size received.
                if ( (kIOReturnSuccess == pFrames[pITD->frameNum + i].frStatus) && 
                    ( (pITD->pType == kOHCIIsochronousOutType) || (pITD->pType == kOHCIIsochronousOutLowLatencyType) ) )
                    pFrames[pITD->frameNum + i].frActCount = pFrames[pITD->frameNum + i].frReqCount;
                else
                    pFrames[pITD->frameNum + i].frActCount = offset & kOHCIITDPSW_Size;
            }
            
            // Translate the OHCI Condition to one of the appropriate USB errors.  We use aggregateStatus to determine
            // later on whether there was an error in any of the frames.  If there was, then we set the completion error
            // to that reported in the aggregateStatus.  There is no priority in the aggregateStatus except that if there
            // is a data underrun AND another type of error, then we report the "other" error.
            //
            frameStatus = pFrames[pITD->frameNum + i].frStatus;
            
            
            if ( frameStatus != kIOReturnSuccess )
            {
                pFrames[pITD->frameNum + i].frStatus =  TranslateStatusToUSBError(frameStatus);
                
                if ( pFrames[pITD->frameNum + i].frStatus == kIOReturnUnderrun )
                    hadUnderrun = true;
                else
                    aggregateStatus = pFrames[pITD->frameNum + i].frStatus;
            }
        }
    }
    
    // call callback
    //
    if (pITD->completion.action)
    {
        IOUSBIsocCompletionAction pHandler;
        
       // If we had an error in any of the frames, then report that error as the status for this framelist
        //
        if ( (status == kIOReturnSuccess) && ( (aggregateStatus != kIOReturnSuccess) || hadUnderrun) )
        {
            // If we don't have an aggregateStatus but we did have an underrun, then report the underrun
            //
            if ( (aggregateStatus == kIOReturnSuccess) && hadUnderrun )
                aggregateStatus = kIOReturnUnderrun;
                
            USBLog(6, "%s[%p]::ProcessCompletedITD: Changing isoc completion error from success to 0x%x", getName(), this, aggregateStatus);
            
            status = aggregateStatus;
        }
        
        // Zero out handler first than call it
        //
        // USBLog(7,"%s[%p]::ProcessCompletedITD: calling completion", getName(), this, pITD);
        
        pHandler = pITD->completion.action;
        pITD->completion.action = NULL;
       (*pHandler) (pITD->completion.target,  pITD->completion.parameter, status, pFrames);
    }
}


void 
AppleUSBOHCI::UIMProcessDoneQueue(IOUSBCompletionAction safeAction)
{
    UInt32				interruptStatus;
    IOPhysicalAddress			PhysAddr;
    AppleOHCIGeneralTransferDescriptorPtr 	pHCDoneTD;
    UInt32				cachedProducer;
    IOPhysicalAddress			cachedWriteDoneQueueHead;
    IOInterruptState			intState;
    

    // Get the values of the Done Queue Head and the producer count.  We use a lock and disable interrupts
    // so that the filter routine does not preempt us and updates the values while we're trying to read them.
    //
    intState = IOSimpleLockLockDisableInterrupt( _wdhLock );
    
    cachedWriteDoneQueueHead = _savedDoneQueueHead;
    cachedProducer = _producerCount;
    
    IOSimpleLockUnlockEnableInterrupt( _wdhLock, intState );
    
    // OK, now that we have a valid queue head in cachedWriteDoneQueueHead, let's process the list
    //
    DoDoneQueueProcessing( cachedWriteDoneQueueHead, cachedProducer, safeAction);

    return;

}


IOReturn
AppleUSBOHCI::DoDoneQueueProcessing(IOPhysicalAddress cachedWriteDoneQueueHead, UInt32 cachedProducer, IOUSBCompletionAction safeAction)
{
    UInt32					control, transferStatus;
    long					bufferSizeRemaining;
    AppleOHCIGeneralTransferDescriptorPtr	pHCDoneTD, prevTD, nextTD;
    IOPhysicalAddress				physicalAddress;
    UInt32					pageMask;
    AppleOHCIEndpointDescriptorPtr		tempED;
    AppleOHCIIsochTransferDescriptorPtr		pITD, testITD;
    volatile UInt32				cachedConsumer;
    UInt32					numTDs = 0;
    // This should never happen
    //
    if (cachedWriteDoneQueueHead == NULL)
        return kIOReturnSuccess;

    // Cache our consumer count
    //
    cachedConsumer = _consumerCount;
    
    // If for some reason our cachedConsumer and cachedProducer are the same, then we need to bail out, as we
    // don't have anything to process
    //
    if ( cachedConsumer == cachedProducer)
    {
        USBLog(3, "%s[%p]::DoDoneQueueProcessing  consumer (%d) == producer (%d) Filter count: %d", getName(), this, cachedConsumer, cachedProducer, _filterInterruptCount);
        return kIOReturnSuccess;
    }
    
    // Get the logical address for our cachedQueueHead
    //
    pHCDoneTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(cachedWriteDoneQueueHead);
    
	if ( pHCDoneTD == NULL )
		return kIOReturnSuccess;
		
    // Now, reverse the queue.  We know how many TD's to process, not by the last one pointing to NULL,
    // but by the fact that cachedConsumer != cachedProducer.  So, go through the loop and increment consumer
    // until they are equal, taking care or the wraparound case.
    //
    prevTD = NULL;

    while ( true )
    {
        pHCDoneTD->pLogicalNext = prevTD;
        prevTD = pHCDoneTD;
        numTDs++;
        
        // Increment our consumer count.  If we wrap around, then increment again.  If we reach
        // the end (both counts are equal, then brake out of the loop
        // 
        cachedConsumer++;
           
        if ( cachedProducer == cachedConsumer)
            break;
    
        physicalAddress = USBToHostLong(pHCDoneTD->pShared->nextTD) & kOHCIHeadPMask;
        nextTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(physicalAddress);
        if ( nextTD == NULL )
        {
            USBLog(5, "%s[%p]::DoDoneQueueProcessing nextTD = NULL.  (%p, %d, %d, %d)", getName(), this, physicalAddress, _filterInterruptCount, cachedProducer, cachedConsumer);
            break;
        }
        
        pHCDoneTD = nextTD;
            
    }


    // New done queue head
    //
    pHCDoneTD = prevTD;
    
    // Update our consumer count
    //
    _consumerCount = cachedConsumer;
    
    // Now, we have a new done queue head.  Now process this reversed list in LOGICAL order.  That
    // means that we can look for a NULL termination
    //
    while (pHCDoneTD != NULL)
    {
        // USBLog(6, "%s[%p]::DoDoneQueueProcessing", getName(), this); // print_td(pHCDoneTD);
        IOReturn errStatus;
        
        // find the next one
        nextTD	= pHCDoneTD->pLogicalNext;

        control = USBToHostLong(pHCDoneTD->pShared->ohciFlags);
        transferStatus = (control & kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;
        errStatus = TranslateStatusToUSBError(transferStatus);
        if (_OptiOn && (pHCDoneTD->pType == kOHCIOptiLSBug))
        {
            // clear any bad errors
            tempED = (AppleOHCIEndpointDescriptorPtr) pHCDoneTD->pEndpoint;
            pHCDoneTD->pShared->ohciFlags = pHCDoneTD->pShared->ohciFlags & HostToUSBLong(kOHCIGTDClearErrorMask);
            tempED->pShared->tdQueueHeadPtr &=  HostToUSBLong(kOHCIHeadPMask);
            pHCDoneTD->pShared->nextTD = tempED->pShared->tdQueueTailPtr & HostToUSBLong(kOHCIHeadPMask);
            tempED->pShared->tdQueueTailPtr = HostToUSBLong(pHCDoneTD->pPhysical);
            _pOHCIRegisters->hcCommandStatus = HostToUSBLong (kOHCIHcCommandStatus_CLF);

            // For CMD Buffer Underrun Errata
        }
        else if ((transferStatus == kOHCIGTDConditionBufferUnderrun) &&
                 (pHCDoneTD->pType == kOHCIBulkTransferOutType) &&
                 (_errataBits & kErrataRetryBufferUnderruns))
        {
            tempED = (AppleOHCIEndpointDescriptorPtr) pHCDoneTD->pEndpoint;
            pHCDoneTD->pShared->ohciFlags = pHCDoneTD->pShared->ohciFlags & HostToUSBLong(kOHCIGTDClearErrorMask);
            pHCDoneTD->pShared->nextTD = tempED->pShared->tdQueueHeadPtr & HostToUSBLong(kOHCIHeadPMask);
            pHCDoneTD->pLogicalNext = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(USBToHostLong(tempED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);

            tempED->pShared->tdQueueHeadPtr = HostToUSBLong(pHCDoneTD->pPhysical) | (tempED->pShared->tdQueueHeadPtr & HostToUSBLong( kOHCIEDToggleBitMask));
            _pOHCIRegisters->hcCommandStatus = HostToUSBLong(kOHCIHcCommandStatus_BLF);
        }
        else if ( (pHCDoneTD->pType == kOHCIIsochronousInType) || (pHCDoneTD->pType == kOHCIIsochronousOutType) || (pHCDoneTD->pType == kOHCIIsochronousInLowLatencyType) || (pHCDoneTD->pType == kOHCIIsochronousOutLowLatencyType) )
        {
            // cast to a isoc type
            pITD = (AppleOHCIIsochTransferDescriptorPtr) pHCDoneTD;
            ProcessCompletedITD(pITD, errStatus);
            // deallocate td
            DeallocateITD(pITD);
        }
        else
        {
            bufferSizeRemaining = findBufferRemaining (pHCDoneTD);
            // if (pHCDoneTD->completion.action != NULL)
            if (pHCDoneTD->uimFlags & kUIMFlagsCallbackTD)
            {
                IOUSBCompletion completion = pHCDoneTD->command->GetUSLCompletion();
                if(!safeAction || (safeAction == completion.action)) 
		{
                    // remove flag before completing
                    pHCDoneTD->uimFlags &= ~kUIMFlagsCallbackTD;
                    Complete(completion, errStatus, bufferSizeRemaining);
                    DeallocateTD(pHCDoneTD);
                }
                else {
                    if(_pendingHead)
                        _pendingTail->pLogicalNext = pHCDoneTD;
                    else
                        _pendingHead = pHCDoneTD;
                    _pendingTail = pHCDoneTD;
                }
            }
            else
	    {
		if (errStatus != kIOReturnSuccess)
                {
                    USBLog(5, "AppleUSBOHCI::DoDoneQueueProcessing - with error (0x%x)", errStatus);
                    doCallback(pHCDoneTD, transferStatus, bufferSizeRemaining);
                }
                DeallocateTD(pHCDoneTD);
            }
        }
        pHCDoneTD = nextTD;	/* New qHead */
    }

    return(kIOReturnSuccess);
}




void 
AppleUSBOHCI::finishPending()
{
    while(_pendingHead) 
    {
        AppleOHCIGeneralTransferDescriptorPtr next = _pendingHead->pLogicalNext;
        long bufferSizeRemaining = findBufferRemaining (_pendingHead);
        UInt32 transferStatus = (USBToHostLong(_pendingHead->pShared->ohciFlags) & kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;

	if (_pendingHead->uimFlags & kUIMFlagsCallbackTD)
	{
	    IOUSBCompletion completion = _pendingHead->command->GetUSLCompletion();
	    _pendingHead->uimFlags &= ~kUIMFlagsCallbackTD;
	    Complete(completion, TranslateStatusToUSBError(transferStatus), bufferSizeRemaining);
	}
        DeallocateTD(_pendingHead);
        _pendingHead = next;
    }
}



UInt32 
AppleUSBOHCI::GetBandwidthAvailable()
{
    return _isochBandwidthAvail;
}



UInt64 
AppleUSBOHCI::GetFrameNumber()
{
    UInt64	bigFrameNumber;
    UInt16	framenumber16;

    
    framenumber16 = USBToHostWord(*(UInt16*)(_pHCCA + 0x80));
    bigFrameNumber = _frameNumber + framenumber16;
    if (framenumber16 < 200)
        if (_pOHCIRegisters->hcInterruptStatus & HostToUSBLong(kOHCIHcInterrupt_FNO))
            bigFrameNumber += kOHCIFrameOverflowBit;
    return bigFrameNumber;
}



UInt32 
AppleUSBOHCI::GetFrameNumber32()
{
    UInt16	framenumber16;
    UInt32	largishFrameNumber;
    
    framenumber16 = USBToHostWord(*(UInt16*)(_pHCCA + 0x80));
    largishFrameNumber = ((UInt32)_frameNumber) + framenumber16;
    if (framenumber16 < 200)
        if (_pOHCIRegisters->hcInterruptStatus & HostToUSBLong(kOHCIHcInterrupt_FNO))
            largishFrameNumber += kOHCIFrameOverflowBit;
    return largishFrameNumber;
}


// #ifdef DEBUG
void 
AppleUSBOHCI::dumpRegs(void)
{
    UInt32		lvalue;
    
    lvalue = _device->configRead32(cwVendorID);
    USBLog(5,"OHCI: cwVendorID=%lx", lvalue);

    lvalue = _device->configRead32(clClassCodeAndRevID);
    USBLog(5,"OHCI: clClassCodeAndRevID=%lx", lvalue);
    lvalue = _device->configRead32(clHeaderAndLatency);
    USBLog(5,"OHCI: clHeaderAndLatency=%lx", lvalue);
    lvalue = _device->configRead32(clBaseAddressZero);
    USBLog(5,"OHCI: clBaseAddressZero=%lx", lvalue);
    lvalue = _device->configRead32(clBaseAddressOne);
    USBLog(5,"OHCI: clBaseAddressOne=%lx", lvalue);
    lvalue = _device->configRead32(clExpansionRomAddr);
    USBLog(5,"OHCI: clExpansionRomAddr=%lx", lvalue);
    lvalue = _device->configRead32(clLatGntIntPinLine);
    USBLog(5,"OHCI: clLatGntIntPinLine=%lx", lvalue);
    lvalue = _device->configRead32(clLatGntIntPinLine+4);
    USBLog(5,"OHCI: clLatGntIntPinLine+4=%lx", lvalue);
    lvalue = _device->configRead32(cwCommand);
    USBLog(5,"OHCI: cwCommand=%lx", lvalue & 0x0000ffff);
    USBLog(5,"OHCI: cwStatus=%lx", lvalue & 0xffff0000);

    lvalue = _device->configRead32(cwCommand);
    _device->configWrite32(cwCommand, lvalue);
    _device->configWrite32(cwCommand, (lvalue & 0xffff0000) |
                          (cwCommandEnableBusMaster |
                           cwCommandEnableMemorySpace));
    lvalue = _device->configRead32(cwCommand);
    USBLog(5,"OHCI: cwCommand=%lx", lvalue & 0x0000ffff);
    USBLog(5,"OHCI: cwStatus=%lx", lvalue & 0xffff0000);

    USBLog(5,"OHCI: HcRevision=%lx", USBToHostLong((_pOHCIRegisters)->hcRevision));
    USBLog(5,"      HcControl=%lx", USBToHostLong((_pOHCIRegisters)->hcControl));
    USBLog(5,"      HcFmInterval=%lx", USBToHostLong((_pOHCIRegisters)->hcFmInterval));
    USBLog(5,"      hcRhDescriptorA=%lx", USBToHostLong((_pOHCIRegisters)->hcRhDescriptorA));
    USBLog(5,"      hcRhDescriptorB=%lx", USBToHostLong((_pOHCIRegisters)->hcRhDescriptorB));
}
// #endif /* DEBUG */



IOReturn 
AppleUSBOHCI::TranslateStatusToUSBError(UInt32 status)
{
    static const UInt32 statusToErrorMap[] = {
        /* OHCI Error */     /* USB Error */
        /*  0 */		kIOReturnSuccess,
        /*  1 */		kIOUSBCRCErr,
        /*  2 */ 		kIOUSBBitstufErr,
        /*  3 */ 		kIOUSBDataToggleErr,
        /*  4 */ 		kIOUSBPipeStalled,
        /*  5 */ 		kIOReturnNotResponding,
        /*  6 */ 		kIOUSBPIDCheckErr,
        /*  7 */ 		kIOUSBWrongPIDErr,
        /*  8 */ 		kIOReturnOverrun,
        /*  9 */ 		kIOReturnUnderrun,
        /* 10 */ 		kIOUSBReserved1Err,
        /* 11 */ 		kIOUSBReserved2Err,
        /* 12 */ 		kIOUSBBufferOverrunErr,
        /* 13 */ 		kIOUSBBufferUnderrunErr,
        /* 14 */		kIOUSBNotSent1Err,
        /* 15 */		kIOUSBNotSent2Err
    };

    if (status > 15) return(kIOReturnInternalError);
    return(statusToErrorMap[status]);
}



void 
AppleUSBOHCI::ReturnTransactions(
            AppleOHCIGeneralTransferDescriptorPtr	transaction,
            UInt32					tail)
{
    UInt32                          		physicalAddress;
    AppleOHCIGeneralTransferDescriptorPtr	nextTransaction;
    AppleOHCIIsochTransferDescriptorPtr		isochTransaction = ( AppleOHCIIsochTransferDescriptorPtr) transaction;
    AppleOHCIIsochTransferDescriptorPtr		nextIsochTransaction = NULL;

    USBLog(6, "%s[%p]::ReturnTransactions: (0x%x, 0x%x)", getName(), this, (UInt32) transaction->pPhysical, tail);
    if ( (transaction->pType == kOHCIIsochronousInType) || (transaction->pType == kOHCIIsochronousOutType) || (transaction->pType == kOHCIIsochronousInLowLatencyType) || (transaction->pType == kOHCIIsochronousOutLowLatencyType))
    {
        // We have an isoc transaction
        //
        while(isochTransaction->pPhysical != tail)
        {
            if (isochTransaction->completion.action != NULL) 
            {
                ProcessCompletedITD(isochTransaction, kIOUSBTransactionReturned);
            }
            /* walk the physically-addressed list */
            physicalAddress = USBToHostLong(isochTransaction->pShared->nextTD) & kOHCIHeadPMask;
            nextIsochTransaction = AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical (physicalAddress);
            DeallocateITD(isochTransaction);
            isochTransaction = nextIsochTransaction;
            if(isochTransaction == NULL)
            {
                USBLog(1, "%s[%p]::ReturnTransactions: Isoc Return queue broken", getName(), this);
                break;
            }
        }
    }
    else
    {
        // Deal with non-isoc transactions
        //
        while(transaction->pPhysical != tail)
        {
            // if (transaction->completion.action != NULL) 
            if (transaction->uimFlags & kUIMFlagsCallbackTD) 
            {
                IOUSBCompletion completion  = transaction->command->GetUSLCompletion();
                transaction->uimFlags &= ~kUIMFlagsCallbackTD;
                Complete(completion, kIOUSBTransactionReturned, 0);
            }
            /* walk the physically-addressed list */
            physicalAddress = USBToHostLong(transaction->pShared->nextTD) & kOHCIHeadPMask;
            nextTransaction = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(physicalAddress);
            DeallocateTD(transaction);
            transaction = nextTransaction;
            if(transaction == NULL)
            {
                USBLog(1, "%s[%p]::ReturnTransactions: Return queue broken", getName(), this);
                break;
            }
        }
    }
}



void 
AppleUSBOHCI::ReturnOneTransaction(
            AppleOHCIGeneralTransferDescriptorPtr	transaction,
            AppleOHCIEndpointDescriptorPtr   		pED,
	    IOReturn					err)
{
    UInt32                          		physicalAddress;
    AppleOHCIGeneralTransferDescriptorPtr	nextTransaction;
    UInt32					something;
    UInt32					tail;
    UInt32					bufferSizeRemaining = 0;

    USBLog(2, "+%s[%p]::ReturnOneTransaction(%p, %p, %x)", getName(), this, transaction, pED, err);
    
    // first mark the pED as skipped so we don't conflict
    pED->pShared->flags |= HostToUSBLong (kOHCIEDControl_K);
    
    // We used to wait for a SOF interrupt here.  Now just sleep for 1 ms.
    //
    IOSleep(1);
    
    // make sure we are still on the same transaction
    if (transaction->pPhysical == (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask))
    {
	tail = USBToHostLong(pED->pShared->tdQueueTailPtr);
	while(transaction->pPhysical != tail)
	{
	    // walk the physically-addressed list
	    physicalAddress = HostToUSBLong(transaction->pShared->nextTD) & kOHCIHeadPMask;
	    nextTransaction = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(physicalAddress);
            
            // take out TD from list
	    pED->pShared->tdQueueHeadPtr = transaction->pShared->nextTD;
	    pED->pLogicalHeadP = nextTransaction;

            bufferSizeRemaining += findBufferRemaining(transaction);

	    if (transaction->uimFlags & kUIMFlagsCallbackTD) 
	    {
		IOUSBCompletion completion  = transaction->command->GetUSLCompletion();
		transaction->uimFlags &= ~kUIMFlagsCallbackTD;
		if (!(transaction->uimFlags & kUIMFlagsMultiTDTransaction))
		{
		    USBLog(2, "%s(%p)::ReturnOneTransaction - found the end of a non-multi transaction(%p)!", getName(), this, transaction);
		    DeallocateTD(transaction);
		    Complete(completion, err, bufferSizeRemaining);
		    break;
		}
		// this is a multi-TD transaction (control) - check to see if we are at the end of it
		else if (transaction->uimFlags & kUIMFlagsFinalTDinTransaction)
		{
		    USBLog(2, "%s(%p)::ReturnOneTransaction - found the end of a MULTI transaction(%p)!", getName(), this, transaction);
		    DeallocateTD(transaction);
		    Complete(completion, err, bufferSizeRemaining);
		    break;
		}
		else
		{
		    USBLog(2, "%s(%p)::ReturnOneTransaction - returning the non-end of a MULTI transaction(%p)!", getName(), this, transaction);
		    DeallocateTD(transaction);
		    Complete(completion, err, bufferSizeRemaining);
		    // keep going around the loop
		}
	    }
	    else
		DeallocateTD(transaction);

	    transaction = nextTransaction;
	    if(transaction == NULL)
	    {
		USBError(1, "ReturnOneTransaction: Return queue broken");
		break;
	    }
	}
    }
    else
    {
	USBLog(2, "%s[%p]::ReturnOneTransaction - transaction not at beginning!(%p, %p)", getName(), this, transaction->pPhysical, pED->pShared->tdQueueHeadPtr);
    }
    USBLog(2, "-%s[%p]::ReturnOneTransaction - done, new queue head (L%p, P%p) V%p", getName(), this, pED->pLogicalHeadP, pED->pShared->tdQueueHeadPtr, ((AppleOHCIGeneralTransferDescriptorPtr)pED->pLogicalHeadP)->pPhysical);
    pED->pShared->flags &= ~HostToUSBLong(kOHCIEDControl_K);	// activate ED again
}



IOReturn 
AppleUSBOHCI::message( UInt32 type, IOService * provider,  void * argument )
{
    cs_event_t	pccardevent;

    // Let our superclass decide handle this method
    // messages
    //
    if ( type == kIOPCCardCSEventMessage)
    {
        pccardevent = (UInt32) argument;

        if ( pccardevent == CS_EVENT_CARD_REMOVAL )
        {
            USBLog(5,"%s[%p]: Received kIOPCCardCSEventMessage Need to return all transactions",getName(),this);
            _pcCardEjected = true;
            IOSleep(5);
            ReturnAllTransactionsInEndpoint(_pControlHead, _pControlTail);
            ReturnAllTransactionsInEndpoint(_pBulkHead, _pBulkTail);
            IOSleep(5);
        }
    }

    USBLog(6, "%s[%p]::message type: 0x%x, isInactive = %d", getName(), this, type, isInactive());
    return super::message( type, provider, argument );
    
}



void 
AppleUSBOHCI::stop(IOService * provider)
{
    USBLog(5, "%s[%p]::stop isInactive = %d", getName(), this, isInactive());
    
	super::stop(provider);
}



bool 
AppleUSBOHCI::finalize(IOOptionBits options)
{
    USBLog(5, "%s[%p]::finalize isInactive = %d", getName(), this, isInactive());
    return super::finalize(options);
}

//=============================================================================================
//
//  UIMInitializeForPowerUp
//
//  This routine is called for a controller that cannot survive sleep (mostly because it loses
//  power across sleep.  It will re-intialize the OHCI registers and do everything needed to get
//  the card up.  The one thing that it does not do is do any memory allocations, as those have
//  already been done and we don't need to do it again.  This routine is essentially the same as
//  UIMInitialize w/out the memory allocations.
//
//  To Do:  Verify that we do need to call the ControlInintialize(), BulkInitialize(), etc.  I'm
//	    not sure that we do.
//
//=============================================================================================
//
IOReturn 
AppleUSBOHCI::UIMInitializeForPowerUp(void)
{
    UInt32		commandRegister; 
    IOPhysicalAddress 	hcDoneHead;

    USBLog(5, "%s[%p]: initializing UIM for PowerUp @ %lx (%lx)", getName(), this,
            (long)_deviceBase->getVirtualAddress(),
            _deviceBase->getPhysicalAddress());

#if (DEBUGGING_LEVEL > 2)
    dumpRegs();
#endif

    // Enable the controller
    //
    commandRegister = _device->configRead32(cwCommand);
    _device->configWrite32(cwCommand, (commandRegister & 0xffff0000) | (cwCommandEnableBusMaster | cwCommandEnableMemorySpace));

    // Check to see if the hcDoneHead is not NULL.  If so, then we need to reset the controller
    //
    hcDoneHead = USBToHostLong(_pOHCIRegisters->hcDoneHead);
    if ( hcDoneHead != NULL )
    {
        USBError(1,"%s[%p]::UIMInitializeForPowerUp Non-NULL hcDoneHead: %p", getName(), this, hcDoneHead );

        // Reset it now
        //
        _pOHCIRegisters->hcCommandStatus = USBToHostLong(kOHCIHcCommandStatus_HCR);  // Reset OHCI
        IOSleep(3);
    }
    
    // Restore the Control and Bulk head pointers
    //
    _pOHCIRegisters->hcControlCurrentED = 0;
    _pOHCIRegisters->hcControlHeadED = HostToUSBLong ((UInt32) _pControlHead->pPhysical);
    _pOHCIRegisters->hcBulkHeadED = HostToUSBLong ((UInt32) _pBulkHead->pPhysical);
    IOSync();

    // Write the HCCA
    //
    OSWriteLittleInt32(&_pOHCIRegisters->hcHCCA, 0, _hccaPhysAddr);
    IOSync();

    // Set the HC to write the donehead to the HCCA, and enable interrupts
    _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_WDH);
    IOSync();

    // Enable the interrupt delivery.
    _workLoop->enableAllInterrupts();


    // Set up hcFmInterval.
    UInt32	hcFSMPS;				// in register hcFmInterval
    UInt32	hcFI;					// in register hcFmInterval
    UInt32	hcPS;					// in register hcPeriodicStart

    hcFI = USBToHostLong(_pOHCIRegisters->hcFmInterval) & kOHCIHcFmInterval_FI;
    // this formula is from the OHCI spec, section 5.4
    hcFSMPS = ((((hcFI-kOHCIMax_OverHead) * 6)/7) << kOHCIHcFmInterval_FSMPSPhase);
    hcPS = (hcFI * 9) / 10;			// per spec- 90%
    _pOHCIRegisters->hcFmInterval = HostToUSBLong(hcFI | hcFSMPS);
    _pOHCIRegisters->hcPeriodicStart = HostToUSBLong(hcPS);

    IOSync();

    // Just so we all start from the same place, reset the OHCI.
    _pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
    IOSync();

    // Set OHCI to operational state and enable processing of control list.
    _pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
                                                | kOHCIHcControl_CLE | kOHCIHcControl_BLE
                                                | kOHCIHcControl_PLE | kOHCIHcControl_IE);
    IOSync();

    // Initialize the Root Hub registers
    if (_errataBits & kErrataDisableOvercurrent)
        _pOHCIRegisters->hcRhDescriptorA |= HostToUSBLong(kOHCIHcRhDescriptorA_NOCP);
    _pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE); // should be SRWE which should be identical to DRWE
    
    OHCIRootHubPower(1 /* kOn */);

    // Enable interrupts
    //
    _pOHCIRegisters->hcInterruptEnable = HostToUSBLong (kOHCIHcInterrupt_MIE | kOHCIDefaultInterrupts);
    IOSync();

    _uimInitialized = true;
    
    return kIOReturnSuccess;

}

//=============================================================================================
//
//  UIMFinalizeForPowerDown
//
//  This routine is called for a controller that cannot survive sleep (mostly because the power
//  is turned off across sleep.  It will disable the OHCI controller and turn off its power.
//  It will NOT deallocate any memory.  This routine is very similar to UIMFinalize(), except that
//  it does not deallocate memory.
//
//  To do:  Make sure that all transactions have been completed or aborted.  This should happen
//	    as a result of the terminate issued to the root hub device, but we need to make sure.
//
//=============================================================================================
//
IOReturn 
AppleUSBOHCI::UIMFinalizeForPowerDown(void)
{

    USBLog (3, "%s[%p]: @ %lx (%lx)(turning off HW)",getName(), this,
              (long)_deviceBase->getVirtualAddress(),
              _deviceBase->getPhysicalAddress());

#if (DEBUGGING_LEVEL > 2)
    dumpRegs();
#endif
    
    // Disable the interrupt delivery
    //
    _workLoop->disableAllInterrupts();
    
    // Disable All OHCI Interrupts
    _pOHCIRegisters->hcInterruptDisable = HostToUSBLong(kOHCIHcInterrupt_MIE);
    IOSync();
    
    // Place the USB bus into the Reset State
    _pOHCIRegisters->hcControl = HostToUSBLong((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
    IOSync();

    //  need to wait at least 1ms here
    IOSleep(2);

    // Take away the controllers ability be a bus master.
    _device->configWrite32(cwCommand, cwCommandEnableMemorySpace);

    // Clear all Processing Registers
    _pOHCIRegisters->hcHCCA = 0;
    _pOHCIRegisters->hcControlHeadED = 0;
    _pOHCIRegisters->hcControlCurrentED = 0;
    _pOHCIRegisters->hcBulkHeadED = 0;
    _pOHCIRegisters->hcBulkCurrentED = 0;
    IOSync();

    // turn off the global power
    OHCIRootHubPower(0 /* kOff */);

    _uimInitialized = false;
    
    // go ahead and reset the controller
    _pOHCIRegisters->hcCommandStatus = HostToUSBLong(kOHCIHcCommandStatus_HCR);  	// Reset OHCI
    IOSync();
    IOSleep(1);			// the spec says 10 microseconds
    
    return kIOReturnSuccess;
}


void
AppleUSBOHCI::free()
{
    // Free our locks
    //
    IOLockFree( _intLock );
    IOSimpleLockFree( _wdhLock );
    
    super::free();
}

