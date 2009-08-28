/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright © 1998-2009 Apple Inc.  All rights reserved.
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


#include <TargetConditionals.h>
#include <libkern/OSByteOrder.h>

extern "C" {
#include <kern/clock.h>
}

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "AppleUSBOHCI.h"
#include "AppleUSBOHCIMemoryBlocks.h"
#include "USBTracepoints.h"

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBControllerV3

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

OSDefineMetaClassAndStructors(AppleUSBOHCI, IOUSBControllerV3)

bool 
AppleUSBOHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  
		return false;
    
    _wdhLock = IOSimpleLockAlloc();
    if (!_wdhLock)
		goto ErrorExit;
	
    _uimInitialized = false;
    _myBusState = kUSBBusStateReset;
    _controllerSpeed = kUSBDeviceSpeedFull;	
    
    // Initialize our consumer and producer counts.  
    //
    _producerCount = 1;
    _consumerCount = 1;
	
    return (true);
	
ErrorExit:
		
	if ( _wdhLock )
		IOSimpleLockFree(_wdhLock);

	return false;
}


bool
AppleUSBOHCI::start( IOService * provider )
{
	uint64_t		currentTime;
	
    USBLog(5,"+AppleUSBOHCI[%p]::start", this);
	
    if( !super::start(provider))
        return false;
	
    // Set our initial time for root hub inactivity
    //
	currentTime = mach_absolute_time();
    _lastCheckedTime = *(AbsoluteTime *)&currentTime;
    USBLog(5,"-AppleUSBOHCI[%p]::start", this);
	
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
AppleUSBOHCI::InitializeOperationalRegisters(void)
{
	UInt32		hcDoneHead;

	// this method initializes the host controller operational registers. It is needed at init time and any 
	// time we reset or sleep the host controller
    // Check to see if the hcDoneHead is not NULL.  If so, then we need to reset the controller
    //
    hcDoneHead = USBToHostLong(_pOHCIRegisters->hcDoneHead);
    if ( hcDoneHead != NULL )
    {
        USBError(1,"AppleUSBOHCI[%p]::InitializeOperationalRegisters Non-NULL hcDoneHead: 0x%x", this, (uint32_t) hcDoneHead );
		
        // Reset it now
        //
        _pOHCIRegisters->hcCommandStatus = USBToHostLong(kOHCIHcCommandStatus_HCR);  // Reset OHCI
        IOSleep(3);
    }
    
    // Restore the Control and Bulk head pointers
    //
    _pOHCIRegisters->hcControlCurrentED = 0;
    _pOHCIRegisters->hcControlHeadED = _pControlHead ? HostToUSBLong ((UInt32) _pControlHead->pPhysical) : 0;
    _pOHCIRegisters->hcBulkHeadED = _pBulkHead ? HostToUSBLong ((UInt32) _pBulkHead->pPhysical) : 0;
    IOSync();
	
    // Write the HCCA
    //
    OSWriteLittleInt32(&_pOHCIRegisters->hcHCCA, 0, _hccaPhysAddr);
    IOSync();
	
    // Set the HC to write the donehead to the HCCA, and enable interrupts
    _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_WDH);
    IOSync();
	
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
	
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		// check hcFmInterval
		newValue = USBToHostLong(_pOHCIRegisters->hcFmInterval);
		while ((count++ < 10) && (newValue != (hcFI | hcFSMPS)))
		{
			USBError(1, "OHCI driver: InitializeOperationalRegisters - hcFmInterval not sticking. Retrying.");
			_pOHCIRegisters->hcFmInterval = HostToUSBLong(hcFI | hcFSMPS);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcFmInterval);
		}
		count = 0;						// reset
		// check hcPeriodicStart
		newValue = USBToHostLong(_pOHCIRegisters->hcPeriodicStart);
		while ((count++ < 10) && (newValue != hcPS))
		{
			USBError(1, "OHCI driver: InitializeOperationalRegisters - hcPeriodicStart not sticking. Retrying.");
			_pOHCIRegisters->hcPeriodicStart = HostToUSBLong(hcPS);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcPeriodicStart);
		}
		
	}
	
    // Initialize the Root Hub registers
    if (_errataBits & kErrataDisableOvercurrent)
        _pOHCIRegisters->hcRhDescriptorA |= HostToUSBLong(kOHCIHcRhDescriptorA_NOCP);
        
    // <rdar://problem/5981624> we used to also initialize the Root Hub Status register here. However, the bus might be in
    // operational state reset at this time, so some OHCI implementations may not allow us to write
    // to it, so we will do that after we make the bus state operational instead
    
	return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::UIMInitialize(IOService * provider)
{
    IOReturn				err = kIOReturnSuccess;
    UInt32					lvalue;
    IOPhysicalAddress		hcDoneHead;
	IODMACommand *			dmaCommand = NULL;
	bool					hccaBufferPrepared = false;
	char *					logicalBytes;
	UInt64					offset = 0;
	IODMACommand::Segment32	segments;
	UInt32					numSegments = 1;
    IOPhysicalAddress		pPhysical= 0;
	
    USBLog(5,"AppleUSBOHCI[%p]::UIMInitialize", this);
	
	// the old do while false loop to prevent goto statements
	do
	{
		if (!_uimInitialized) 
		{
			_device = OSDynamicCast(IOPCIDevice, provider);
			if(_device == NULL)
			{
				err = kIOReturnBadArgument;
				break;
			}
			
			_deviceBase = provider->mapDeviceMemoryWithIndex(0);
			if (!_deviceBase)
			{
				USBError(1,"AppleUSBOHCI[%p]::UIMInitialize unable to get device memory", this);
				err = kIOReturnNoMemory;
				break;
			}
			
			USBLog(3, "AppleUSBOHCI[%p]::UIMInitialize config @ %x (%x)", this, (uint32_t)_deviceBase->getVirtualAddress(), (uint32_t)_deviceBase->getPhysicalAddress());
			
			SetVendorInfo();
			
			// Set up a filter interrupt source (this process both primary (thru filter function) and secondary (thru action function)
			// interrupts.
			//
			_filterInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this, AppleUSBOHCI::InterruptHandler,	 AppleUSBOHCI::PrimaryInterruptFilter, provider );
			
			if ( !_filterInterruptSource )
			{
				USBError(1,"AppleUSBOHCI[%p]::UIMInitialize unable to get filterInterruptEventSource", this);
				err = kIOReturnBadArgument;
				break;
			}
			
			err = _workLoop->addEventSource(_filterInterruptSource);
			if ( err != kIOReturnSuccess )
			{
				USBError(1,"AppleUSBOHCI[%p]::UIMInitialize unable to add filter event source: 0x%x", this, err);
				err = kIOReturnBadArgument;
				break;
			}
			
			/*
			 * Initialize my data and the hardware
			 */
			_errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);
			setProperty("Errata", _errataBits, 32);
			
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
			
			USBLog(5,"AppleUSBOHCI[%p]::UIMInitialize errata bits=0x%x", this, (uint32_t) _errataBits);
			
			_pOHCIRegisters = (OHCIRegistersPtr) _deviceBase->getVirtualAddress();
			
			// leave bus mastering off until we get the setPowerState
			_device->configWrite16(kIOPCIConfigCommand, kIOPCICommandMemorySpace);
			
			// Set up HCCA.
			
			// Use IODMACommand to get the physical address
			dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
			if (!dmaCommand)
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - could not create IODMACommand", this);
				err = kIOReturnInternalError;
				break;
			}
			USBLog(5, "AppleUSBOHCI[%p]::UIMInitialize - got IODMACommand %p", this, dmaCommand);
			
			//
			_hccaBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut | kIOMemoryPhysicallyContiguous, kHCCAsize, kOHCIStructureAllocationPhysicalMask);
			if (_hccaBuffer == NULL) 
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - could not get HCCA buffer", this);
				err = kIOReturnNoMemory;
				break;
			}
			
			err = _hccaBuffer->prepare();
			if (err)
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - _hccaBuffer->prepare failed with status(%p)", this, (void*)err);
				break;
			}
			
			hccaBufferPrepared = true;
			
			err = dmaCommand->setMemoryDescriptor(_hccaBuffer);
			if (err)
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - setMemoryDescriptor returned err (%p)", this, (void*)err);
				break;
			}
			
			offset = 0;
			segments.fIOVMAddr = 0;
			segments.fLength = 0;
			numSegments = 1;
			
			err = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			if (err || (numSegments != 1) || (segments.fLength != kHCCAsize))
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)err, (int)numSegments, (int)segments.fLength);
				dmaCommand->clearMemoryDescriptor();
				err = err ? err : kIOReturnInternalError;
				break;
			}
			
			_pHCCA = (Ptr)_hccaBuffer->getBytesNoCopy();
			_hccaPhysAddr = segments.fIOVMAddr;
			
			USBLog(5, "AppleUSBOHCI[%p]::UIMInitialize - HCCA Buffer pPhysical[%p] logical[%p]", this, (void*)_hccaPhysAddr, _pHCCA);
			
			dmaCommand->clearMemoryDescriptor();
			
			// Release the DMA Command
			if (dmaCommand)
			{
				if (dmaCommand->getMemoryDescriptor())
				{
					USBError(1, "AppleUSBOHCI[%p]::UIMInitialize - dmaCommand still has memory descriptor (%p)", this, dmaCommand->getMemoryDescriptor());
					dmaCommand->clearMemoryDescriptor();
				}
				dmaCommand->release();
				dmaCommand = NULL;
			}
			
			_rootHubFuncAddress = 1;
			
			// set up Interrupt transfer tree
			if ((err = IsochronousInitialize()))	break;
			if ((err = InterruptInitialize()))		break;
			if ((err = BulkInitialize()))			break;
			if ((err = ControlInitialize()))		break;
			
			// <rdar://problem/5981624> this will be done in the setPowerState
			// InitializeOperationalRegisters();
			
			// Work around the Philips part which does weird things when a device is plugged in at boot
			//
			if (_errataBits & kErrataNeedsPortPowerOff)
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMInitialize error, turning off power to ports to clear", this);
				OHCIRootHubPower(0 /* kOff */);
				// No need to turn the power back on here, the reset does that anyway.
			}
			
			// Just so we all start from the same place, reset the OHCI.
			_pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
			IOSync();
			
			// always make sure we stay in reset for at least 50 ms
			IOSleep(50);
			
			// leave the controller in reset until we get the setPowerState
			
			if (_errataBits & kErrataLSHSOpti)
				OptiLSHSFix();
			
			CheckSleepCapability();
			
		}
		_uimInitialized = true;
		_myBusState = kUSBBusStateReset;
		
	} while (false);
	

	if ( kIOReturnSuccess != err )
	{
		if (_hccaBuffer)
		{
			if (hccaBufferPrepared)
				_hccaBuffer->complete();
			_hccaBuffer->release();
			_hccaBuffer = NULL;
		}
		
	}
	
	return err;
}


IOReturn 
AppleUSBOHCI::UIMFinalize(void)
{
    USBLog (3, "AppleUSBOHCI[%p]::UIMFinalize @ 0x%x (0x%x)(shutting down HW)",this, 
			(uint32_t)_deviceBase->getVirtualAddress(),
			(uint32_t)_deviceBase->getPhysicalAddress());
	
#if 0
	// 4930013: JRH - This is a bad thing to do with shared interrupts, and since we turn off interrupts at the source
	// it should be redundant. Let's stop doing it..
    // Disable the interrupt delivery
    //
    _workLoop->disableAllInterrupts();
#endif
	
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
		
		// always make sure we stay in reset for at least 50 ms
        IOSleep(50);
		
        // Take away the controllers ability be a bus master.
        _device->configWrite16(kIOPCIConfigCommand, kIOPCICommandMemorySpace);
		
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
	if (_hccaBuffer)
	{
		_hccaBuffer->complete();
		_hccaBuffer->release();
		_hccaBuffer = NULL;
	}
    
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
        USBLog(3, "AppleUSBOHCI[%p]::doCallback No transfer descriptors!", this);
		return;
    }
    USBLog(6, "AppleUSBOHCI::doCallback: pCurrentTD = %p, pED->pLogicalTailP = %p", pCurrentTD, pED->pLogicalTailP);
    while (pCurrentTD != pED->pLogicalTailP)
    {
        // UnlinkTD! But don't lose the data toggle or halt bit
        //
        pED->pShared->tdQueueHeadPtr = pCurrentTD->pShared->nextTD | (pED->pShared->tdQueueHeadPtr & HostToUSBLong(~kOHCIHeadPointer_headP));
        USBLog(7, "AppleUSBOHCI::doCallback- queueheadptr is now 0x%x", (uint32_t) pED->pShared->tdQueueHeadPtr);
        
        bufferSizeRemaining += findBufferRemaining (pCurrentTD);
		
        // make sure this TD won't be added to any future buffer
		// remaining calculations
        pCurrentTD->pShared->currentBufferPtr = NULL;
		
        if (pCurrentTD->uimFlags & kUIMFlagsCallbackTD)
        {
            IOUSBCompletion completion;
			
			if (transferStatus == kOHCIGTDConditionDataUnderrun)
			{
                USBLog(6, "AppleUSBOHCI::doCallback- found callback TD, setting queuehead to 0x%x", (uint32_t) (pED->pShared->tdQueueHeadPtr & HostToUSBLong(~kOHCIHeadPointer_H)));
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
    UInt32                      pageNumMask;
    UInt32                      bufferSizeRemaining;
	
	
    pageNumMask = ~PAGE_MASK;
	
    if (pCurrentTD->pShared->currentBufferPtr == 0)
    {
        bufferSizeRemaining = 0;
    }
    else if ((USBToHostLong(pCurrentTD->pShared->bufferEnd) & (pageNumMask)) == (USBToHostLong(pCurrentTD->pShared->currentBufferPtr) & (pageNumMask)))
    {
        // we're on the same page
        bufferSizeRemaining = (USBToHostLong (pCurrentTD->pShared->bufferEnd) & PAGE_MASK) - (USBToHostLong (pCurrentTD->pShared->currentBufferPtr) & PAGE_MASK) + 1;
    }
    else
    {
        bufferSizeRemaining = ((USBToHostLong(pCurrentTD->pShared->bufferEnd) & PAGE_MASK) + 1)  + (PAGE_SIZE - (USBToHostLong(pCurrentTD->pShared->currentBufferPtr) & PAGE_MASK));
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
    if ( pED2 == NULL )  
		return kIOReturnNoMemory;
	
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
	_expansionData->_isochMaxBusStall = 25000;									// Set to 25 microseconds for OHCI
	
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
			USBLog(1, "AppleUSBOHCI[%p]::AllocateTD - unable to allocate a new memory block!", this);
			USBTrace( kUSBTOHCI, kTPOHCIAllocateITD, (uintptr_t)this, 0, 0, 1 );
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
		USBLog(7, "AppleUSBOHCI[%p]::AllocateITD - _pFreeITD (%p), _pFreeITD->pPhysical(0x%x), _pFreeITD->pShared (%p), GetITDFromPhysical(%p)", 
			   this, _pFreeITD, (uint32_t) _pFreeITD->pPhysical, _pFreeITD->pShared, AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical(_pFreeITD->pPhysical));
		for (i=1; i < numTDs; i++)
		{
			freeITD = memBlock->GetITD(i);
			if (!freeITD)
			{
				USBLog(1, "AppleUSBOHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block", this);
				USBTrace( kUSBTOHCI, kTPOHCIAllocateITD, (uintptr_t)this, i, numTDs, 2 );
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
			USBLog(1, "AppleUSBOHCI[%p]::AllocateTD - unable to allocate a new memory block!", this);
			USBTrace( kUSBTOHCI, kTPOHCIAllocateTD, (uintptr_t)this, 0, 0, 1 );
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
		USBLog(7, "AppleUSBOHCI[%p]::AllocateTD - _pFreeTD (%p), _pFreeTD->pPhysical(0x%x), _pFreeTD->pShared (%p), GetGTDFromPhysical(%p)", 
			   this, _pFreeTD, (uint32_t) _pFreeTD->pPhysical, _pFreeTD->pShared, AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(_pFreeTD->pPhysical));
		for (i=1; i < numTDs; i++)
		{
			freeTD = memBlock->GetGTD(i);
			if (!freeTD)
			{
				USBLog(1, "AppleUSBOHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block", this);
				USBTrace( kUSBTOHCI, kTPOHCIAllocateTD, (uintptr_t)this, 0, 0, 2 );
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
AppleUSBOHCI::AllocateED()
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
			USBLog(1, "AppleUSBOHCI[%p]::AllocateED - unable to allocate a new memory block!", this);
			USBTrace( kUSBTOHCI, kTPOHCIAllocateED, (uintptr_t)this, 0, 0, 1 );
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
        USBLog(7, "AppleUSBOHCI[%p]::AllocateED - _pFreeED (%p), _pFreeED->pPhysical(0x%x), _pFreeED->pShared (%p)",
               this, _pFreeED, (uint32_t) _pFreeED->pPhysical, _pFreeED->pShared);
		for (i=1; i < numEDs; i++)
		{
			freeED = memBlock->GetED(i);
			if (!freeED)
			{
				USBLog(1, "AppleUSBOHCI[%p]::AllocateED - hmm. ran out of EDs in a memory block", this);
				USBTrace( kUSBTOHCI, kTPOHCIAllocateED, (uintptr_t)this, 0, 0, 2 );
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
	// Remove the TDs and reset the data toggle
    RemoveTDs(pED, true);
	
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
AppleUSBOHCI::RemoveTDs(AppleOHCIEndpointDescriptorPtr pED, bool clearToggle)
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
			
            // Take out TD from list, respecting the clearToggle parameter
			if ( clearToggle )
				pED->pShared->tdQueueHeadPtr = pCurrentTD->pShared->nextTD;
			else
			{
				// Preserve the toggle bit of the current QHead
				UInt32	currentToggle = USBToHostLong(pED->pShared->tdQueueHeadPtr) & (kOHCIHeadPointer_C);
				if ( currentToggle != 0)
				{
					USBLog(6,"AppleUSBOHCI[%p]::RemoveTDs:  Preserving a data toggle of 1 in response to an Abort()!", this);
				}
				
				pED->pShared->tdQueueHeadPtr = pCurrentTD->pShared->nextTD | HostToUSBLong(currentToggle);
			}
			
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
	
    IOUSBIsocFrame *				pFrames;
    IOUSBLowLatencyIsocFrame *		pLLFrames;
    int								i;
    int								frameCount;
    IOReturn						aggregateStatus = kIOReturnSuccess;
    IOReturn						frameStatus;
    UInt32							itdConditionCode;
    bool							hadUnderrun = false;
    UInt32							delta;
    UInt32							curFrame;
    AbsoluteTime					timeStart;
	uint64_t						timeStop;
    UInt64							timeElapsed;
    
    pFrames = pITD->pIsocFrame;
    pLLFrames = (IOUSBLowLatencyIsocFrame *) pITD->pIsocFrame;
    
    frameCount = (USBToHostLong(pITD->pShared->flags) & kOHCIITDControl_FC) >> kOHCIITDControl_FCPhase;
	
    itdConditionCode = (USBToHostLong(pITD->pShared->flags) & kOHCIITDControl_CC) >> kOHCIITDControl_CCPhase;
    
	
    // USBLog(3, "AppleUSBOHCI[%p]::ProcessCompletedITD: filter interrupt duration: %ld", this, (UInt32) timeElapsed);
	
    if (itdConditionCode == kOHCIITDConditionDataOverrun)
    {
		// The OHCI controller sets the status to DATAOVERRUN in the case where the TD could not go out because there was not time
		// left in the frame.  This is called a Time Error and is in Section 4.3.2.3.5.3 of the OHCI spec.  We will return this as a no bandwidth error.
		//
		status = kIOReturnNoBandwidth; 
		
		USBLog(5,"AppleUSBOHCI[%p]::ProcessCompletedITD: Time Error in Isoch xfer (%p) -- not enough time to send the whole TD, returning kIOReturnNoBandwidth (0x%x)", this, pITD, status);
    }
    
    // Do some calculations related to the low latency isoch TDs:
    //
    if ( (_filterInterruptCount != 0 ) &&
         ( (pITD->pType == kOHCIIsochronousInLowLatencyType) || 
           (pITD->pType == kOHCIIsochronousOutLowLatencyType) ) )
    {
		timeStop = mach_absolute_time();

        timeStart = pLLFrames[pITD->frameNum].frTimeStamp;
        SUB_ABSOLUTETIME(&timeStop, &timeStart); 
        absolutetime_to_nanoseconds(*(AbsoluteTime *)&timeStop, &timeElapsed); 
        
        if ( _lowLatencyIsochTDsProcessed != 0 )
        {
            USBLog(6, "AppleUSBOHCI[%p]::ProcessCompletedITD: LowLatency isoch TD's proccessed: %d, framesUpdated: %d, framesError: %d",  this, (uint32_t)_lowLatencyIsochTDsProcessed, (uint32_t)_framesUpdated, (uint32_t)_framesError);
            USBLog(7, "AppleUSBOHCI[%p]::ProcessCompletedITD: delay in microsecs before callback (from hw interrupt time): %d", this, (uint32_t) timeElapsed / 1000);
            
            // SUB_ABSOLUTETIME(&_filterTimeStamp2, &_filterTimeStamp); 
            // absolutetime_to_nanoseconds(_filterTimeStamp2, &timeElapsed); 
            // USBLog(7, "AppleUSBOHCI[%p]::ProcessCompletedITD: filter interrupt duration: %ld", this, (UInt32) timeElapsed / 1000);
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
        if ( (pITD->pType == kOHCIIsochronousInType) || (pITD->pType == kOHCIIsochronousOutType) )
        {
            // Process non-low latency isoch.  Low latench TDs where processed at Filter Interrupt time
            //
            UInt16 offset = USBToHostWord(pITD->pShared->offset[i]);
            
            if ( ((offset & kOHCIITDOffset_CC) >> kOHCIITDOffset_CCPhase) == kOHCIITDOffsetConditionNotAccessed)
            {
                USBLog(6,"AppleUSBOHCI[%p]::ProcessCompletedITD:  Isoch frame not accessed. Frame in request(1 based) %d, IsocFramePtr: %p, ITD: %p, Frames in this TD: %d, Relative frame in TD: %d",  this, (uint32_t)pITD->frameNum + i + 1, pFrames, pITD, frameCount+1, i+1);
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
			
			// If this request originated from a Rosetta client, swap the frameList fields
			if ( pITD->requestFromRosettaClient )
			{
                pFrames[pITD->frameNum + i].frReqCount = OSSwapInt16(pFrames[pITD->frameNum + i].frReqCount);
                pFrames[pITD->frameNum + i].frActCount = OSSwapInt16(pFrames[pITD->frameNum + i].frActCount);
                pFrames[pITD->frameNum + i].frStatus = OSSwapInt32(pFrames[pITD->frameNum + i].frStatus);
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
			
            USBLog(6, "AppleUSBOHCI[%p]::ProcessCompletedITD: Changing isoc completion error from success to 0x%x", this, aggregateStatus);
            
            status = aggregateStatus;
        }
        
        // Zero out handler first than call it
        //
        // USBLog(7,"AppleUSBOHCI[%p]::ProcessCompletedITD: calling completion", this, pITD);
        
        pHandler = pITD->completion.action;
        pITD->completion.action = NULL;
		(*pHandler) (pITD->completion.target,  pITD->completion.parameter, status, pFrames);
		
		_activeIsochTransfers--;
		if ( _activeIsochTransfers < 0 )
		{
			USBLog(1, "AppleUSBOHCI[%p]::ProcessCompletedITD - _activeIsochTransfers went negative (%d).  We lost one somewhere", this, (uint32_t)_activeIsochTransfers);
			USBTrace( kUSBTOHCI, kTPOHCIProcessCompletedITD, (uintptr_t)this, (uint32_t)_activeIsochTransfers, 0, 0 );
		}
		else if (!_activeIsochTransfers && (_expansionData->_isochMaxBusStall != 0))
			requireMaxBusStall(0);										// remove maximum stall restraint on the PCI bus
		
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
        USBLog(3, "AppleUSBOHCI[%p]::DoDoneQueueProcessing  consumer (%d) == producer (%d) Filter count: %d, nothing to process -- weird", this, (uint32_t)cachedConsumer, (uint32_t)cachedProducer, (uint32_t)_filterInterruptCount);
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
            USBLog(5, "AppleUSBOHCI[%p]::DoDoneQueueProcessing nextTD = NULL.  (0x%x, %d, %d, %d)", this, (uint32_t)physicalAddress, (uint32_t)_filterInterruptCount, (uint32_t)cachedProducer, (uint32_t)cachedConsumer);
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
        // USBLog(6, "AppleUSBOHCI[%p]::DoDoneQueueProcessing", this); // print_td(pHCDoneTD);
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
	
    USBLog(6, "AppleUSBOHCI[%p]::ReturnTransactions: (0x%x, 0x%x)", this, (uint32_t) transaction->pPhysical, (uint32_t)tail);
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
                USBLog(1, "AppleUSBOHCI[%p]::ReturnTransactions: Isoc Return queue broken", this);
				USBTrace( kUSBTOHCI, kTPOHCIReturnTransactions, (uintptr_t)this, 0, 0, 1 );
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
                USBLog(1, "AppleUSBOHCI[%p]::ReturnTransactions: Return queue broken", this);
				USBTrace( kUSBTOHCI, kTPOHCIReturnTransactions, (uintptr_t)this, 0, 0, 2 );
                break;
            }
        }
    }
}



void 
AppleUSBOHCI::ReturnOneTransaction(AppleOHCIGeneralTransferDescriptorPtr	transaction,
								   AppleOHCIEndpointDescriptorPtr   		pED,
								   IOReturn									err)
{
    UInt32                          		physicalAddress;
    AppleOHCIGeneralTransferDescriptorPtr	nextTransaction;
    UInt32									something;
    UInt32									tail;
    UInt32									bufferSizeRemaining = 0;
	
    USBLog(2, "+AppleUSBOHCI[%p]::ReturnOneTransaction(%p, %p, %x)", this, transaction, pED, err);
    
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
					USBLog(2, "[%p]::ReturnOneTransaction - found the end of a non-multi transaction(%p)!", this, transaction);
					DeallocateTD(transaction);
					Complete(completion, err, bufferSizeRemaining);
					break;								// we are done
				}
				// this is a multi-TD transaction (control) - check to see if we are at the end of it
				else if (transaction->uimFlags & kUIMFlagsFinalTDinTransaction)
				{
					USBLog(2, "[%p]::ReturnOneTransaction - found the end of a MULTI transaction(%p)!", this, transaction);
					DeallocateTD(transaction);
					Complete(completion, err, bufferSizeRemaining);
					break;								// we are done
				}
				else
				{
					USBLog(2, "[%p]::ReturnOneTransaction - returning the non-end of a MULTI transaction(%p)!", this, transaction);
					DeallocateTD(transaction);
					Complete(completion, err, bufferSizeRemaining);
					// keep going around the loop - this is a multiXfer transaction and we haven't found the end yet
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
		USBLog(2, "AppleUSBOHCI[%p]::ReturnOneTransaction - transaction not at beginning!(0x%x, 0x%x)", this, (uint32_t) transaction->pPhysical, (uint32_t) pED->pShared->tdQueueHeadPtr);
    }
    USBLog(2, "-AppleUSBOHCI[%p]::ReturnOneTransaction - done, new queue head (L%p, P0x%x) V0x%x", this, pED->pLogicalHeadP, (uint32_t) pED->pShared->tdQueueHeadPtr, (uint32_t) ((AppleOHCIGeneralTransferDescriptorPtr)pED->pLogicalHeadP)->pPhysical);
    pED->pShared->flags &= ~HostToUSBLong(kOHCIEDControl_K);	// activate ED again
}



IOReturn 
AppleUSBOHCI::message( UInt32 type, IOService * provider,  void * argument )
{
	if (type == kIOUSBMessageExpressCardCantWake)
	{
		IOService *					nub = (IOService*)argument;
		const IORegistryPlane *		usbPlane = getPlane(kIOUSBPlane);
		IOUSBRootHubDevice *		parentHub = OSDynamicCast(IOUSBRootHubDevice, nub->getParentEntry(usbPlane));
		
		nub->retain();
		USBLog(1, "AppleUSBOHCI[%p]::message - got kIOUSBMessageExpressCardCantWake from driver %s[%p] argument is %s[%p]", this, provider->getName(), provider, nub->getName(), nub);
		USBTrace( kUSBTOHCI, kTPOHCIMessage, (uintptr_t)this, kIOUSBMessageExpressCardCantWake, 0, 0 );
		if (parentHub == _rootHubDevice)
		{
			USBLog(3, "AppleUSBOHCI[%p]::message - device is attached to my root hub (port %d)!!", this, (int)_ExpressCardPort);
			_badExpressCardAttached = true;
		}
		nub->release();
		return kIOReturnSuccess;
	}
	
    USBLog(6, "AppleUSBOHCI[%p]::message type: 0x%x, isInactive = %d", this, (uint32_t)type, isInactive());
    return super::message( type, provider, argument );
    
}



bool 
AppleUSBOHCI::finalize(IOOptionBits options)
{
    USBLog(5, "AppleUSBOHCI[%p]::finalize isInactive = %d", this, isInactive());
    return super::finalize(options);
}



void
AppleUSBOHCI::free()
{
    // Free our locks
    //
    IOSimpleLockFree( _wdhLock );
    
    super::free();
}



IODMACommand*
AppleUSBOHCI::GetNewDMACommand()
{
	return IODMACommand::withSpecification(kIODMACommandOutputHost64, 32, PAGE_SIZE);
}



void 
AppleUSBOHCI::showRegisters(UInt32 level, const char *s)
{
	
    UInt32				descriptorA;
    unsigned int		i, numPorts;
	
	if (!_controllerAvailable)
		return;
		
    descriptorA = USBToHostLong(_pOHCIRegisters->hcRhDescriptorA);

    numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP) >> kOHCIHcRhDescriptorA_NDPPhase);

    USBLog(level,"OHCIUIM -- showRegisters %s", s);
#ifdef SHOW_PCI_REGS
    USBLog(level,"PCI: kIOPCIConfigVendorID=%lx", _device->configRead32(kIOPCIConfigVendorID));
    USBLog(level,"     kIOPCIConfigRevisionID=%lx", _device->configRead32(kIOPCIConfigRevisionID));
    USBLog(level,"     kIOPCIConfigCacheLineSize=%lx", _device->configRead32(kIOPCIConfigCacheLineSize));
    USBLog(level,"     kIOPCIConfigBaseAddress0=%lx", _device->configRead32(kIOPCIConfigBaseAddress0));
    USBLog(level,"     kIOPCIConfigBaseAddress1=%lx", _device->configRead32(kIOPCIConfigBaseAddress1));
    USBLog(level,"     kIOPCIConfigExpansionROMBase=%lx", _device->configRead32(kIOPCIConfigExpansionROMBase));
    USBLog(level,"     kIOPCIConfigInterruptLine=%lx", _device->configRead32(kIOPCIConfigInterruptLine));
    USBLog(level,"     kIOPCIConfigInterruptLine+4=%lx", _device->configRead32(kIOPCIConfigInterruptLine+4));
    USBLog(level,"     kIOPCIConfigCommand=%p", (void*)_device->configRead16(kIOPCIConfigCommand));
    USBLog(level,"     kIOPCIConfigStatus=%p", (void*)_device->configRead16(kIOPCIConfigStatus));
#endif

    USBLog(level,"OHCI: HcRevision=%p", (void*) USBToHostLong((_pOHCIRegisters)->hcRevision));
    USBLog(level,"      HcControl=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcControl));
    USBLog(level,"      HcCommandStatus=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcCommandStatus));
    USBLog(level,"      HcInterruptStatus=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcInterruptStatus));
    USBLog(level,"      hcInterruptEnable=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcInterruptEnable));
    USBLog(level,"      HcFmInterval=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcFmInterval));
    USBLog(level,"      hcRhStatus=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcRhStatus));
    USBLog(level,"      hcRhDescriptorA=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcRhDescriptorA));
    USBLog(level,"      hcRhDescriptorB=%p",  (void*) USBToHostLong((_pOHCIRegisters)->hcRhDescriptorB));
	for (i=0; i < numPorts; i++)
	{
		USBLog(level,"      hcRhPortStatus[%d]=%p",  (int)i, (void*)USBToHostLong((_pOHCIRegisters)->hcRhPortStatus[i]));
	}
}



