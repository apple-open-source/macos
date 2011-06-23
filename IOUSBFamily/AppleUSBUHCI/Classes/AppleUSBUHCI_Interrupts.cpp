/*
 * Copyright © 2005-2009 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



#include <libkern/OSByteOrder.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include "AppleUSBUHCI.h"
#include "AppleUHCIListElement.h"
#include "USBTracepoints.h"

#define super IOUSBControllerV3
#define self this

// ========================================================================
#pragma mark Interrupts
// ========================================================================

void 
AppleUSBUHCI::PollInterrupts(IOUSBCompletionAction safeAction)
{
    USBLog(1, "AppleUSBUHCI[%p]::PollInterrupts (unused)", this);
    // Not used
}

void 
AppleUSBUHCI::UpdateFrameNumberWithTime(void)
{
	
	// 
	// This routine is called at software interrupt time(DPC to me) to update the anchor values returned
	// for GetFrameNumberWithTime. These values are calculated at the time of the actual hardware 
	// interrupt in (FilterInterrupt) and stored in shadow registers.
	// 
		
	// update when we detect a change to irq cached values
    if (_anchorFrame != _tempAnchorFrame)
    {
		
		// copy the temporary variables over to the real thing, 
		// we do this because this method is protected by the workloop gate whereas the FilterInterrupt one is not
		_anchorTime = _tempAnchorTime;
		_anchorFrame = _tempAnchorFrame;
	}

}



//Called at hardware interrupt time.
bool 
AppleUSBUHCI::PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
    register AppleUSBUHCI	*controller = (AppleUSBUHCI *)owner;
    bool					result = true;
	
    // If we our controller has gone away, or it's going away, or if we're on a PC Card and we have been ejected,
    // then don't process this interrupt.
    //
    if (!controller || controller->isInactive() || !controller->_controllerAvailable)
	{
#if UHCI_USE_KPRINTF
		kprintf("AppleUSBUHCI[%p]::PrimaryInterruptFilter - not available - ignoring\n", controller);
#endif
        return false;
	}

    // Process this interrupt
    //
    controller->_filterInterruptActive = true;
    result = controller->FilterInterrupt();
    controller->_filterInterruptActive = false;
    return result;
}



void 
AppleUSBUHCI::InterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
    AppleUSBUHCI *controller = (AppleUSBUHCI *)owner;
    
    //USBLog(7, "AppleUSBUHCI::InterruptHandler");
	
    //
    // Interrupt source callouts are not blocked by _workLoop->sleep()
    // and driver checks _uhciBusState to prevent touching hardware when
    // UHCI has powered down, and another device sharing the interrupt
    // line has it asserted.
    //
    // Driver must ensure that the device cannot generate an interrupt
    // while in suspend state to prevent an interrupt storm.
    //
	
    if (controller && !controller->isInactive() && controller->_controllerAvailable) 
	{
        controller->HandleInterrupt();
    }
#if UHCI_USE_KPRINTF
	else
	{
		kprintf("AppleUSBUHCI[%p]::InterruptHandler - not available - ignoring\n", controller);
	}
#endif
}



// ========= Interrupt handling ================

//
// This is similar to the code in GetFrameNumber that is only called at hw irq time.  It uses separate vars since it can preempt the 
// real GetFrameNumber function.  It does not use the _frameLock mutex because filterinterrupt cannot be preempted.
//
UInt64
AppleUSBUHCI::GetFrameNumberInternal(void)
{
    UInt32				lastIrqFrameLow;
	UInt32				currentIrqFrameLow;
    UInt64				lastIrqFrameHi;
	UInt64              currentFrame;
	
	//	
	// ** only call this function from FilterInterrupt or if the UHCI interrupts are disabled ** 
	//
	// This code attempts to record the time of the SOF. According to the UHCI 1.1 spec
	// the IRQ happens at EOF (end of frame) time but we will treat this like SOF time for our purposes.  
	//
	// *NOTE: debug log messages are disabled. The debug messages can only be enabled if we are using kprintf or some 
	// logging feature that is safe to call at FilterInterrupt time.

	// If the controller is halted, then we should just bail out
	if (ioRead16(kUHCI_STS) & kUHCI_STS_HCH)
	{
		if (_myPowerState == kUSBPowerStateOn)
		{
			// do not use USBLog from here, since this method can be called in interrupt context
			// USBLog(1, "AppleUSBUHCI[%p]::GetFrameNumber called but controller is halted",  this);
			USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsGetFrameNumberInternal, (uintptr_t)this, kUHCI_STS_HCH, 0, 0);
		}
		return 0;
	}
	
	//USBLog(7, "AppleUSBUHCI[%p]::GetFrameNumberInternal - check frame number", this);
	
	// _lastIrqFrame preserves the last 64 bit frame number we recorded
	lastIrqFrameLow = _lastIrqFrameLow;
	lastIrqFrameHi = _lastIrqFrame & ~0x7ff;
	
	currentIrqFrameLow = ReadFrameNumberRegister();
	
	// check for overflow bit(10) change
	if (currentIrqFrameLow <= lastIrqFrameLow) 
	{
		uint64_t		tempTime;
		
		// 11 bit overflow (bit 10 changed)
		// bump high part
			
		lastIrqFrameHi += 0x800;
		
		//
		// if the frame list rolled over or we don't have a value yet then update our cached copy of frame_with_time
		// the frame counter is 11 bits wide, bit 10 will change every 1000ms (1 sec). 
		//
		// note: we may make an extra update if we happen to take an interrupt when the 64 bit value rolls over but 
		// this is once in a blue moon and won't cause a problem. 
		//
		
		currentFrame = lastIrqFrameHi + ((UInt64) currentIrqFrameLow);
			
		_tempAnchorFrame = currentFrame;
		tempTime = mach_absolute_time();
		_tempAnchorTime = *(AbsoluteTime*)&tempTime;

	} else 
	{
		currentFrame = lastIrqFrameHi + ((UInt64) currentIrqFrameLow);
	}
	
	_lastIrqFrameLow = currentIrqFrameLow;
	_lastIrqFrame = currentFrame;
		
    // USBLog(7, "AppleUSBUHCI[%p]:: GetFrameNumberInternal - frame number is %qx (%qd) %qd .sec", this, currentFrame, currentFrame, currentFrame/1000);
	
    return (currentFrame);
}



// Called at hardware interrupt time
bool
AppleUSBUHCI::FilterInterrupt(void)
{
	UInt16						activeInterrupts;
    Boolean						needSignal = false;
	UInt64						currentFrame;
	uint64_t					timeStamp;
	
	// we leave all interrupts enabled, so see which ones are active
	activeInterrupts = ioRead16(kUHCI_STS) & kUHCI_STS_INTR_MASK;
	
	if (activeInterrupts != 0) 
	{
		USBTrace( kUSBTUHCIInterrupts, kTPUHCIInterruptsFilterInterrupt , (uintptr_t)this, activeInterrupts, 0, 3 );

		if (activeInterrupts & kUHCI_STS_HCPE)
		{
			// Host Controller Process Error - usually a bad data structure on the list
			_hostControllerProcessInterrupt = kUHCI_STS_HCPE;
			ioWrite16(kUHCI_STS, kUHCI_STS_HCPE);
			needSignal = true;
			//USBLog(1, "AppleUSBUHCI[%p]::FilterInterrupt - HCPE error - legacy reg = %p", this, (void*)_device->configRead16(kUHCI_PCI_LEGKEY));
			USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsFilterInterrupt, (uintptr_t)this, _hostControllerProcessInterrupt, _device->configRead16(kUHCI_PCI_LEGKEY), 1 );
		}
		if (activeInterrupts & kUHCI_STS_HSE)
		{
			// Host System Error - usually a PCI issue
			_hostSystemErrorInterrupt = kUHCI_STS_HSE;
			ioWrite16(kUHCI_STS, kUHCI_STS_HSE);
			needSignal = true;
			//USBLog(1, "AppleUSBUHCI[%p]::FilterInterrupt - HSE error - legacy reg = %p", this, (void*)_device->configRead16(kUHCI_PCI_LEGKEY));
			USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsFilterInterrupt, (uintptr_t)this, _hostSystemErrorInterrupt, _device->configRead16(kUHCI_PCI_LEGKEY), 2 );
		}
		if (activeInterrupts & kUHCI_STS_RD)
		{
			// Resume Detect - remote wakeup
			_resumeDetectInterrupt = kUHCI_STS_RD;
			ioWrite16(kUHCI_STS, kUHCI_STS_RD);
			needSignal = true;
		}
		if (activeInterrupts & kUHCI_STS_EI)
		{
			// USB Error Interrupt - transaction error (CRC, timeout, etc)
			_usbErrorInterrupt = kUHCI_STS_EI;
			ioWrite16(kUHCI_STS, kUHCI_STS_EI);
			needSignal = true;
		}
		if (activeInterrupts & kUHCI_STS_INT)
		{
			// Normal IOC interrupt - we need to check out low latency Isoch as well
			timeStamp = mach_absolute_time();
			ioWrite16(kUHCI_STS, kUHCI_STS_INT);
			needSignal = true;
						
			// This function will give us the current frame number and check for rollover at the same time
			// since we are calling from filterInterrupts we will not be preempted, it will also update the 
			// cached copy of the frame_number_with_time 
			GetFrameNumberInternal();
			
			// we need to check the periodic list to see if there are any Isoch TDs which need to come off
			// and potentially have their frame lists updated (for Low Latency) we will place them in reverse
			// order on a "done queue" which will be looked at by the isoch scavanger
			// only do this if the periodic schedule is enabled
			if (!_inAbortIsochEP  && (_outSlot < kUHCI_NVFRAMES))
			{
				AppleUHCIIsochTransferDescriptor	*cachedHead;
				UInt32								cachedProducer;
				UInt16								curSlot, testSlot, nextSlot;
				
				curSlot = (ReadFrameNumber() & kUHCI_NVFRAMES_MASK);
				
				cachedHead = (AppleUHCIIsochTransferDescriptor*)_savedDoneQueueHead;
				cachedProducer = _producerCount;
				testSlot = _outSlot;
				
				while (testSlot != curSlot)
				{
					IOUSBControllerListElement				*thing, *nextThing;
					AppleUHCIIsochTransferDescriptor		*isochTD;
					
					nextSlot = (testSlot+1) & kUHCI_NVFRAMES_MASK;
					thing = _logicalFrameList[testSlot];
					while (thing != NULL)
					{
						nextThing = thing->_logicalNext;
						isochTD = OSDynamicCast(AppleUHCIIsochTransferDescriptor, thing);
						
						if (!isochTD)
							break;						// only care about Isoch in this list - if we get here we are at the interrupt TDs
												
						// need to unlink this TD
						_logicalFrameList[testSlot] = nextThing;
						_frameList[testSlot] = HostToUSBLong(thing->GetPhysicalLink());
						
						if (isochTD->_lowLatency)
							isochTD->frStatus = isochTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);
						// place this guy on the backward done queue
						// the reason that we do not use the _logicalNext link is that the done queue is not a null terminated list
						// and the element linked "last" in the list might not be a true link - trust me
						isochTD->_doneQueueLink = cachedHead;
						cachedHead = isochTD;
						cachedProducer++;
						if (isochTD->_pEndpoint)
						{
							isochTD->_pEndpoint->onProducerQ++;
							OSDecrementAtomic( &(isochTD->_pEndpoint->scheduledTDs));
						}
						
						thing = nextThing;
					}
					testSlot = nextSlot;
					_outSlot = testSlot;
				}
				IOSimpleLockLock( _wdhLock );
				
				_savedDoneQueueHead = cachedHead;	// updates the shadow head
				_producerCount = cachedProducer;	// Validates _producerCount;
				
				IOSimpleLockUnlock( _wdhLock );

			}
		
			// 8394970:  Make sure we set the flag AFTER we have incremented our producer count.
			_usbCompletionInterrupt = kUHCI_STS_INT;
		}
	}
	
	// We will return false from this filter routine,
	// but will indicate that there the action routine should be called
	// by calling _filterInterruptSource->signalInterrupt(). 
	// This is needed because IOKit will disable interrupts for a level interrupt
	// after the filter interrupt is run, until the action interrupt is called.
	// We want to be able to have our filter interrupt routine called
	// before the action routine runs, if needed.  That is what will enable
	// low latency isoch transfers to work, as when the
	// system is under heavy load, the action routine can be delayed for tens of ms.
	//
	if (needSignal)
		_interruptSource->signalInterrupt();
	
	return false;
}



// Called at software interrupt time
void
AppleUSBUHCI::HandleInterrupt(void)
{
	UInt16					status;
	UInt32					intrStatus;
	bool					needReset = false;
		
	status = ioRead16(kUHCI_STS);

	USBTrace_Start( kUSBTUHCIInterrupts, kTPUHCIInterruptsHandleInterrupt,  (uintptr_t)this, 0, 0, 0);

	if (_hostControllerProcessInterrupt & kUHCI_STS_HCPE)
	{
		_hostControllerProcessInterrupt = 0;
		USBLog(1, "AppleUSBUHCI[%p]::HandleInterrupt - Host controller process error", this);
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, 0, 0, 1 );
		needReset = true;
	}
	if (_hostSystemErrorInterrupt & kUHCI_STS_HSE)
	{
		_hostSystemErrorInterrupt = 0;
		USBLog(1, "AppleUSBUHCI[%p]::HandleInterrupt - Host controller system error(CMD:%p STS:%p INTR:%p PORTSC1:%p PORTSC2:%p FRBASEADDR:%p ConfigCMD:%p)", this,(void*)ioRead16(kUHCI_CMD), (void*)ioRead16(kUHCI_STS), (void*)ioRead16(kUHCI_INTR), (void*)ioRead16(kUHCI_PORTSC1), (void*)ioRead16(kUHCI_PORTSC2), (void*)ioRead32(kUHCI_FRBASEADDR), (void*)_device->configRead16(kIOPCIConfigCommand));
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, ioRead16(kUHCI_CMD), 0, 2  );
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, ioRead16(kUHCI_STS), ioRead16(kUHCI_INTR), ioRead16(kUHCI_PORTSC1), 3  );
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, ioRead16(kUHCI_PORTSC2), ioRead32(kUHCI_FRBASEADDR), _device->configRead16(kIOPCIConfigCommand), 4 );
		needReset = true;
	}
	if (_resumeDetectInterrupt & kUHCI_STS_RD) 
	{
		_resumeDetectInterrupt = 0;
		USBLog(2, "AppleUSBUHCI[%p]::HandleInterrupt - Host controller resume detected - calling EnsureUsability", this);
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, 0, 0, 6);
		EnsureUsability();		
	}
	if (_usbErrorInterrupt & kUHCI_STS_EI) 
	{
		_usbErrorInterrupt = 0;
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, 0, 0, 7);
		USBLog(6, "AppleUSBUHCI[%p]::HandleInterrupt - Host controller error interrupt", this);
	}
	if (_usbCompletionInterrupt & kUHCI_STS_INT)
	{
		_usbCompletionInterrupt = 0;
		
		// USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, 0, 0, 8);
		// updates hardware interrupt time from shadow vars that are se in the real irq handler
		UpdateFrameNumberWithTime();

		USBLog(7, "AppleUSBUHCI[%p]::HandleInterrupt - Normal interrupt", this);
		if (_consumerCount != _producerCount)
		{	
			USBLog(7, "AppleUSBUHCI[%p]::HandleInterrupt - Isoch was handled", this);
		}
	}
	
	// this code probably doesn't work
	if (needReset) 
	{
		IOSleep(1000);
		USBLog(1, "AppleUSBUHCI[%p]::HandleInterrupt - Resetting controller due to errors detected at interrupt time (0x%x)", this, status);
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, status, needReset, 5 );
		Reset(true);
		Run(true);
	}

	if (_myPowerState == kUSBPowerStateOn)
	{
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, status, 0, 9);
		ProcessCompletedTransactions();
	
		// Check for root hub status change
		RHCheckStatus();
	}
	else
	{
		USBLog(2, "AppleUSBUHCI[%p]::HandleInterrupt - deferring further processing until we are running again", this);
		USBTrace( kUSBTUHCIInterrupts,  kTPUHCIInterruptsHandleInterrupt, (uintptr_t)this, 0, 0, 10);
	}
	USBTrace_End( kUSBTUHCIInterrupts, kTPUHCIInterruptsHandleInterrupt,  (uintptr_t)this, 0, 0, 0);
}



