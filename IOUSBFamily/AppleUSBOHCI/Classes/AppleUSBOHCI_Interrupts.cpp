/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#include "AppleUSBOHCI.h"
#include <libkern/OSByteOrder.h>

#include <IOKit/usb/IOUSBLog.h>

#define nil (0)
#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController
#define self this

void AppleUSBOHCI::PollInterrupts(IOUSBCompletionAction safeAction)
{
    register UInt32		activeInterrupts;
    register UInt32		interruptEnable;

    interruptEnable = USBToHostLong(_pOHCIRegisters->hcInterruptEnable);

    activeInterrupts = interruptEnable & USBToHostLong(_pOHCIRegisters->hcInterruptStatus);

    if ((interruptEnable & kOHCIHcInterrupt_MIE) && (activeInterrupts != 0))
    {
        /*
         * SchedulingOverrun Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_SO)
        {
            _errors.scheduleOverrun++;
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_SO);
            IOSync();

            USBLog(3,"%s[%p] SchedulingOverrun Interrupt on bus %d", getName(), this, _busNumber );
        }
        /*
         * WritebackDoneHead Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_WDH)
        {
            UIMProcessDoneQueue(safeAction);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<WritebackDoneHead Interrupt>\n");
#endif
        }
        /*
         * StartofFrame Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_SF)
        {
            // Clear the interrrupt
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_SF);

            // and mask it off so it doesn't happen again.
            // will have to be turned on manually to happen again.
            _pOHCIRegisters->hcInterruptDisable = USBToHostLong(kOHCIHcInterrupt_SF);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<Frame Interrupt>\n");
#endif
            // FIXME? ERIC performCommand(ROOT_HUB_FRAME, (void *)0);
        }
        /*
         * ResumeDetected Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_RD)
        {
	    //setPowerState(1, self);
	    _remote_wakeup_occurred = true; //needed by ::callPlatformFunction()
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_RD);
            USBLog(1,"%s[%p] ResumeDetected Interrupt on bus %d", getName(), this, _busNumber );
            if ( _idleSuspend )
                setPowerState(kOHCISetPowerLevelRunning,self);
        }
        /*
         * Unrecoverable Error Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_UE)
        {
            _errors.unrecoverableError++;
            // Let's do a SW reset to recover from this condition.
            // We could make sure all OCHI registers and in-memory
            // data structures are valid, too.
            _pOHCIRegisters->hcCommandStatus = USBToHostLong(kOHCIHcCommandStatus_HCR);
            delay(10 * MICROSECOND);
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_UE);
            // zzzz - note I'm leaving the Control/Bulk list processing off
            // for now.  FIXME? ERIC

            _pOHCIRegisters->hcControl = USBToHostLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase) | kOHCIHcControl_PLE);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<Unrecoverable Error Interrupt>\n");
#endif
        }
        /*
         * FrameNumberOverflow Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_FNO)
        {
            // not really an error, but close enough
            _errors.frameNumberOverflow++;
            if ((USBToHostWord(*(UInt16*)(_pHCCA + 0x80)) & kOHCIFmNumberMask) < kOHCIBit15)
		_frameNumber += kOHCIFrameOverflowBit;
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_FNO);
#if (DEBUGGING_LEVEL > 0)
            IOLog("<FrameNumberOverflow Interrupt>\n");
#endif
        }
        /*
         * RootHubStatusChange Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_RHSC)
        {
            // Clear status change.
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_RHSC);
	    _remote_wakeup_occurred = true; //needed by ::callPlatformFunction()

#if (DEBUGGING_LEVEL > 0)
            IOLog("<RHSC Interrupt>\n");
#endif

            UIMRootHubStatusChange( false );
            LastRootHubPortStatusChanged ( true );
        }
        /*
         * OwnershipChange Interrupt
         */
        if (activeInterrupts & kOHCIHcInterrupt_OC)
        {
            // well, we certainly weren't expecting this!
            _errors.ownershipChange++;
            _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_OC);

#if (DEBUGGING_LEVEL > 0)
            IOLog("<OwnershipChange Interrupt>\n");
#endif
        }
    }
}

void AppleUSBOHCI::InterruptHandler(OSObject *owner,
                                        IOInterruptEventSource * /*source*/,
                                        int /*count*/)
{
    register AppleUSBOHCI		*controller = (AppleUSBOHCI *) owner;

    if (!controller || controller->isInactive() || (controller->_onCardBus && controller->_pcCardEjected) )
        return;
        
    // Finish pending transactions first.
    controller->finishPending();
    controller->PollInterrupts();
}
