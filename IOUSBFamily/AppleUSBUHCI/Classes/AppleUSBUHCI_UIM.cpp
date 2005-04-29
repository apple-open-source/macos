/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/usb/IOUSBLog.h>
#include <kern/clock.h>
#include <machine/limits.h>

#include "AppleUSBUHCI.h"


static char *
USBErrorToString(IOReturn status)
{
    switch (status) {
        case kIOReturnSuccess:
            return "kIOReturnSuccess";
        case kIOReturnError:
            return "kIOReturnError";
        case kIOReturnNotResponding:
            return "kIOReturnNotResponding";
        case kIOUSBPipeStalled:
            return "kIOUSBPipeStalled";
        case kIOReturnOverrun:
            return "kIOReturnOverrun";
        case kIOReturnUnderrun:
            return "kIOReturnUnderrun";
        case kIOUSBLinkErr:
            return "kIOUSBLinkErr";
        case kIOUSBCRCErr:
            return "kIOUSBCRCErr";
        case kIOUSBBitstufErr:
            return "kIOUSBBitstufErr";
        case kIOUSBTransactionReturned:
            return "kIOUSBTransactionReturned";
        case kIOReturnAborted:
            return "kIOReturnAborted";
        case kIOReturnIsoTooNew:
            return "kIOReturnIsoTooNew";
        case kIOReturnIsoTooOld:
            return "kIOReturnIsoTooOld";
        case kIOReturnNoDevice:
            return "kIOReturnNoDevice";
        case kIOReturnBadArgument:
            return "kIOReturnBadArgument";
        case kIOReturnInternalError:
            return "kIOReturnInternalError";
        case kIOReturnNoMemory:
            return "kIOReturnNoMemory";
        case kIOReturnUnsupported:
            return "kIOReturnUnsupported";
        case kIOReturnNoResources:
            return "kIOReturnNoResources";
        case kIOReturnNoBandwidth:
            return "kIOReturnNoBandwidth";
        case kIOReturnIPCError:
            return "kIOReturnIPCError";
        case kIOReturnTimeout:
            return "kIOReturnTimeout";
        case kIOReturnBusy:
            return "kIOReturnBusy";
        case kIOUSBTransactionTimeout:
            return "kIOUSBTransactionTimeout";
        case kIOUSBNotSent1Err:
            return "kIOUSBNotSent1Err";
        case kIOUSBNotSent2Err:
            return "kIOUSBNotSent2Err";
    }
    return "Unknown";
}


/*
 * UIM methods
 */


// ========================================================================
#pragma mark Control
// ========================================================================

IOReturn
AppleUSBUHCI::UIMCreateControlEndpoint(
                                       UInt8				functionNumber,
                                       UInt8				endpointNumber,
                                       UInt16				maxPacketSize,
                                       UInt8				speed,
                                       USBDeviceAddress    		highSpeedHub,
                                       int			        highSpeedPort)
{
    return UIMCreateControlEndpoint(functionNumber, endpointNumber, maxPacketSize, speed);
}


IOReturn
AppleUSBUHCI::UIMCreateControlEndpoint(
                                       UInt8				functionNumber,
                                       UInt8				endpointNumber,
                                       UInt16				maxPacketSize,
                                       UInt8				speed)
{
    UHCIEndpoint *ep;
    
    USBLog(3, "%s[%p]::UIMCreateControlEndpoint (f %d ep %d) max %d spd %d", getName(), this,
           functionNumber, endpointNumber, maxPacketSize, speed);
    
    if (functionNumber == _rootFunctionNumber) {
        return kIOReturnSuccess;
    }
    
    if (maxPacketSize == 0) {
        return kIOReturnBadArgument;
    }
    
    ep = AllocEndpoint(functionNumber, endpointNumber, kUSBNone, speed, maxPacketSize, kUSBControl);
    
    if (ep == NULL)
        return kIOReturnNoMemory;
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::UIMCreateControlTransfer(
                                       short				functionNumber,
                                       short				endpointNumber,
                                       IOUSBCommand*			command,
                                       IOMemoryDescriptor *		CBP,
                                       bool				bufferRounding, // short packet OK
                                       UInt32				bufferSize,
                                       short				direction)
{
    UHCIEndpoint *ep;
    UHCITransaction *tp;
    QH *qh;
    TD *td, *last_td;
    IOReturn status;
    
    USBLog(4, "%s[%p]::UIMCreateControlTransfer (f %d ep %d dir %d) size %d", getName(), this, functionNumber, endpointNumber, direction, bufferSize);
    
    ep = FindEndpoint(functionNumber, endpointNumber, kUSBAnyDirn);
    if (ep == NULL) {
        USBLog(4, "%s[%p]: endpoint not found", getName(), this);
        return kIOUSBEndpointNotFound;
    }
    
    if (ep->stalled) {
        USBLog(4, "%s[%p]: Control pipe stalled", getName(), this);
        return kIOUSBPipeStalled;
    }
    
    /* Here's how we will assemble the transaction:
     * There are up to three parts to a control transaction.  If the command says
     * that it's a multi-part transaction, and this is not the last part of the transaction,
     * assemble the parts in a queue but don't start it yet.
     */
    tp = (UHCITransaction *)command->GetUIMScratch(0);
    USBLog(4, "%s[%p]: scratch TP is %p, stall %d", getName(), this, tp, ep->stalled);
    
    if (tp == NULL) {
        /* This is a new transaction. */
        tp = AllocTransaction(ep);
        if (tp == NULL) {
            return kIOReturnNoMemory;
        }
        USBLog(4, "%s[%p]: creating new transaction %p", getName(), this, tp);
        tp->command = command;
        tp->buf = CBP;
        tp->bufLen = command->GetDataRemaining();
        tp->timeout = command->GetCompletionTimeout();
        if (tp->timeout == 0) {
            tp->timeout = command->GetNoDataTimeout();
        }
        USBLog(4, "%s[%p]: Data timeout is %d, completion timeout is %d",
               getName(), this,
               command->GetNoDataTimeout(), command->GetCompletionTimeout());
        // XXX should deal separately with GetDataTimeout()
        
        /* We rely on the completion function being the same
         * for all phases of the transaction.
         */
        tp->completion = command->GetUSLCompletion();
        
        /* Put this on the pending transaction list. */
        // XXX don't put it here yet.  This transaction will be leaked
        // if we don't get the final phase of the control transaction.
        //queue_enter(&ep->pendingTransactions, tp, UHCITransction *, endpoint_chain);
        
        qh = AllocQH();
        if (qh == NULL) {
            return kIOReturnNoMemory;
        }
        tp->qh = qh;
        command->SetUIMScratch(0, (UInt32)tp);
    }
    
    tp->nCompletions++;

    qh = tp->qh;

    USBLog(4, "%s[%p]: allocating TD chain", getName(), this);
    status = AllocTDChain(ep, CBP, bufferSize, bufferRounding, direction, &td, &last_td, false, true);
    if (status != kIOReturnSuccess) {
        return status;
    }
    
    if (tp->last_td != NULL) {
        tp->last_td->link = td;
        tp->last_td->hw.link = HostToUSBLong(td->paddr | kUHCI_VERTICAL_FLAG);
    } else {
        tp->qh->elink = td;
        tp->qh->hw.elink = HostToUSBLong(td->paddr | kUHCI_VERTICAL_FLAG);
        tp->first_td = td;
    }
    IOSync();
    tp->last_td = last_td;
    
    if (!command->GetMultiTransferTransaction() ||
        command->GetFinalTransferInTransaction()) {
        /* This is the final part of the transaction.
         * Mark the TD to interrupt on completion, and
         * start transaction.
         */
        //DumpTD(td, 2);
        last_td->hw.ctrlStatus |= HostToUSBLong(kUHCI_TD_IOC);
        IOSync();

        // XXX It doesn't go on the pending list until the final transfer is received.
        //queue_remove(&ep->pendingTransactions, tp, UHCITransation *, endpoint_chain);

        if (ep->stalled == false && queue_empty(&ep->activeTransactions)) {
            USBLog(4, "%s[%p]: activating transaction %p", getName(), this, tp);
            
            StartTransaction(tp);
            // Put on either HS or LS queue here.
            QueueControl(tp);
            
        } else {
            USBLog(4, "%s[%p]: pending transaction %p", getName(), this, tp);
            queue_enter(&ep->pendingTransactions, tp, UHCITransaction *, endpoint_chain);
        }
    } 

    return kIOReturnSuccess;
}


// ========================================================================
#pragma mark Bulk
// ========================================================================


IOReturn
AppleUSBUHCI::UIMCreateBulkEndpoint(
                                    UInt8				functionNumber,
                                    UInt8				endpointNumber,
                                    UInt8				direction,
                                    UInt8				speed,
                                    UInt16				maxPacketSize,
                                    USBDeviceAddress    		highSpeedHub,
                                    int			                highSpeedPort)
{
    return UIMCreateBulkEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize);
}

IOReturn
AppleUSBUHCI::UIMCreateBulkEndpoint(
                                    UInt8				functionNumber,
                                    UInt8				endpointNumber,
                                    UInt8				direction,
                                    UInt8				speed,
                                    UInt8				maxPacketSize)
{
    UHCIEndpoint *ep;
    
    USBLog(3, "%s[%p]::UIMCreateBulkEndpoint (fn %d ep %d dir %d) speed %d mp %d", getName(), this,
           functionNumber, endpointNumber, direction, speed, maxPacketSize);
    
    if (maxPacketSize == 0) {
        return kIOReturnBadArgument;
    }
    
    ep = AllocEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize, kUSBBulk);
    
    if (ep == NULL) {
        return kIOReturnNoMemory;
    }
    
    return kIOReturnSuccess;
}


IOReturn
AppleUSBUHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    UHCIEndpoint *ep;
    UHCITransaction *tp;
    IOMemoryDescriptor *mp;
    IOReturn status;
    QH *qh;
    
    USBLog(4, "%s[%p]::UIMCreateBulkTransfer (%d, %d, %d) size %d", getName(), this,
           command->GetAddress(), command->GetEndpoint(), command->GetDirection(),
           command->GetReqCount());
    
    ep = FindEndpoint(command);
    if (ep == NULL) {
        USBLog(4, "%s[%p]: endpoint (fn %d, ep %d, dir %d) not found", getName(), this,
               command->GetAddress(),
               command->GetEndpoint(),
               command->GetDirection()
               );
        return kIOUSBEndpointNotFound;
    }
    
    if (ep->stalled) {
        USBLog(4, "%s[%p]: Bulk pipe stalled", getName(), this);
        return kIOUSBPipeStalled;
    }
    
    tp = AllocTransaction(ep);
    if (tp == NULL) {
        return kIOReturnNoMemory;
    }
    tp->command = command;
    tp->buf = command->GetBuffer();
    tp->bufLen = command->GetReqCount();
    tp->completion = command->GetUSLCompletion();
    tp->timeout = command->GetCompletionTimeout();
    if (tp->timeout == 0) {
        tp->timeout = command->GetNoDataTimeout();
    }
    
    qh = AllocQH();
    if (qh == NULL) {
        return kIOReturnNoMemory;
    }
    tp->qh = qh;
    
    tp->nCompletions = 1;
    
    mp = command->GetBuffer();
    status = AllocTDChain(ep, mp, command->GetReqCount(), command->GetBufferRounding(), command->GetDirection(), &tp->first_td, &tp->last_td);
    if (status != kIOReturnSuccess) {
        USBLog(4, "AllocTDChain returns %d", status);
        return status;
    }
    tp->last_td->hw.ctrlStatus |= HostToUSBLong(kUHCI_TD_IOC);
    IOSync();
    
    qh->elink = tp->first_td;
    qh->hw.elink = HostToUSBLong(tp->first_td->paddr);
    IOSync();
    
    USBLog(4, "%s[%p]: activating bulk transaction %p", getName(), this, tp);
    
    if (ep->stalled == false && queue_empty(&ep->activeTransactions)) {
        USBLog(4, "%s[%p]: activating transaction %p", getName(), this, tp);
        
        StartTransaction(tp);
        // Put on either HS or LS queue here.
        QueueBulk(tp);
        
    } else {
        USBLog(4, "%s[%p]: pending bulk transaction %p", getName(), this, tp);
        queue_enter(&ep->pendingTransactions, tp, UHCITransaction *, endpoint_chain);
    }
    
    return kIOReturnSuccess;
}

// ========================================================================
#pragma mark Interrupt
// ========================================================================

IOReturn
AppleUSBUHCI::UIMCreateInterruptEndpoint(
                                         short				functionNumber,
                                         short				endpointNumber,
                                         UInt8				direction,
                                         short				speed,
                                         UInt16				maxPacketSize,
                                         short				pollingRate,
                                         USBDeviceAddress    		highSpeedHub,
                                         int                 		highSpeedPort)
{
    return UIMCreateInterruptEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize, pollingRate);
}


IOReturn
AppleUSBUHCI::UIMCreateInterruptEndpoint(
                                         short				functionNumber,
                                         short				endpointNumber,
                                         UInt8				direction,
                                         short				speed,
                                         UInt16				maxPacketSize,
                                         short				pollingRate)
{
    UHCIEndpoint *ep;
    int i;
    
    USBLog(3, "%s[%p]::UIMCreateInterruptEndpoint (fn %d, ep %d, dir %d) spd %d pkt %d rate %d", getName(), this,
           functionNumber, endpointNumber, direction, speed,
           maxPacketSize, pollingRate );
    
    if (functionNumber == _rootFunctionNumber) {
        if (endpointNumber != 0 && endpointNumber != 1) {
            return kIOReturnBadArgument;
        }
        return RHCreateInterruptEndpoint(endpointNumber, direction, speed, maxPacketSize, pollingRate);
    }
    
    // If the interrupt already exists, then we need to delete it first, as we're probably trying
    // to change the Polling interval via SetPipePolicy().
    //
    ep = FindEndpoint(functionNumber, endpointNumber, direction);
    if ( ep != NULL )
    {
        IOReturn ret;
        USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint endpoint already existed -- deleting it",getName(), this);
        ret = UIMDeleteEndpoint(functionNumber, endpointNumber, direction);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint deleting endpoint returned %p",getName(), this, ret);
            return ret;
        }
    }
    else
        USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint endpoint does NOT exist",getName(), this);

    
    ep = AllocEndpoint(functionNumber, endpointNumber,
                       direction, speed, maxPacketSize, kUSBInterrupt);
    
    if (ep == NULL) {
        return kIOReturnNoMemory;
    }
        
    /* pollingRate is the maximum ms between transactions. */
    ep->pollingRate = pollingRate;
        
    for (i=kUHCI_NINTR_QHS-1; i>=0; i--) {
        if ((1 << i) <= pollingRate) {
            break;
        }
    }
    if (i<0) {
        i = 0;
    }
    USBLog(3, "%s[%p]: we will use interrupt queue %d, which corresponds to a rate of %d",
           getName(), this, i, (1 << i));
    
    ep->head_qh = _intrQH[i];
            
    return kIOReturnSuccess;
}


// method in 1.8.2
IOReturn
AppleUSBUHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    UHCIEndpoint *ep;
    UHCITransaction *tp;
    QH *qh;
    IOReturn status;
    IOMemoryDescriptor *mp;
    IOByteCount len;

    USBLog(5, "%s[%p]::UIMCreateInterruptTransfer adr=(%d,%d) len %d rounding %d", getName(), this,
           command->GetAddress(), command->GetEndpoint(), command->GetReqCount(),
           command->GetBufferRounding());
    
    if (command->GetAddress() == _rootFunctionNumber)
    {
        return RHCreateInterruptTransfer(command);
    }
    
    ep = FindEndpoint(command);
    if (ep == NULL) {
        return kIOUSBEndpointNotFound;
    }
    
    if (ep->stalled) {
        USBLog(5, "%s[%p]: Interrupt pipe stalled", getName(), this);
        return kIOUSBPipeStalled;
    }
    
    tp = AllocTransaction(ep);
    if (tp == NULL) {
        return kIOReturnNoMemory;
    }
    tp->command = command;
    tp->buf = command->GetBuffer();
    tp->bufLen = command->GetReqCount();
    tp->completion = command->GetUSLCompletion();
    // no timeout for interrupt transactions
    
    qh = AllocQH();
    if (qh == NULL) {
        return kIOReturnNoMemory;
    }
    tp->qh = qh;
    
    tp->nCompletions = 1;
    
    mp = command->GetBuffer();
    len = command->GetReqCount();
    
#define INTERRUPT_TRANSFERS_ONE_PACKET 0
#if INTERRUPT_TRANSFERS_ONE_PACKET
    // Restrict interrupt transfers to one packet only.
    // This seems to help Bluetooth USB adapters work.
    if ((int)len > ep->maxPacketSize) {
        len = ep->maxPacketSize;
    }
#endif
    
    status = AllocTDChain(ep, mp, len, command->GetBufferRounding(), command->GetDirection(),
                          &tp->first_td, &tp->last_td);
    if (status != kIOReturnSuccess) {
        return status;
    }

    tp->last_td->hw.ctrlStatus |= HostToUSBLong(kUHCI_TD_IOC);
    IOSync();
        
    qh->elink = tp->first_td;
    qh->hw.elink = HostToUSBLong(tp->first_td->paddr);
    IOSync();

    StartTransaction(tp);

    QueueInterrupt(tp);
    
    return kIOReturnSuccess;
}


// ========================================================================
#pragma mark Isochronous
// ========================================================================


IOReturn
AppleUSBUHCI::UIMCreateIsochEndpoint(
                                     short				functionNumber,
                                     short				endpointNumber,
                                     UInt32				maxPacketSize,
                                     UInt8				direction,
                                     USBDeviceAddress                   highSpeedHub,
                                     int                                highSpeedPort)
{
    return UIMCreateIsochEndpoint(functionNumber, endpointNumber, maxPacketSize, direction);
}

IOReturn
AppleUSBUHCI::UIMCreateIsochEndpoint(
                                     short				functionNumber,
                                     short				endpointNumber,
                                     UInt32				maxPacketSize,
                                     UInt8				direction)
{
    int i, frame;
    TD *td, *vtd;
    UInt32 token;
    UHCIEndpoint *ep;
    
    USBLog(3, "%s[%p]::UIMCreateIsochEndpoint (fn %d, ep %d, dir %d) mp %d", getName(), this,
           functionNumber, endpointNumber, direction, maxPacketSize);
    
    ep = FindEndpoint(functionNumber, endpointNumber, direction);
    if (ep != NULL) {
        /* The upper layers are adjusting the parameters. */
        USBLog(3, "%s[%p]: the endpoint %p already exists with packet size %d.", getName(), this, ep,
               ep->maxPacketSize);
        
        if (maxPacketSize != ep->maxPacketSize) {
            if (ep->buffersInUse > 0) {
                if (maxPacketSize > ep->maxBufferSize) {
                    /* Sorry, can't increase packet size beyond what we used to allocate buffers
                     * while transactions are outstanding.
                     */
                    USBLog(3, "%s[%p]: trying to change packet size from %d to %d with %d buffers outstanding",
                           getName(), this, ep->maxPacketSize, maxPacketSize, ep->buffersInUse);
                    return kIOReturnNoBandwidth;
                }
                /* Decreasing packet size is OK. */
            }
        }

        if (maxPacketSize > ep->maxPacketSize) {
            /* Trying to get more bandwidth.  See if it is available. */
            if ((maxPacketSize - ep->maxPacketSize) >= _isocBandwidth) {
                USBLog(3, "%s[%p]: only bandwidth available is %d, returning error",
                       getName(), this, _isocBandwidth);
                /* The extra bandwidth is not available. */
                return kIOReturnNoBandwidth;
            }
            _isocBandwidth -= (maxPacketSize - ep->maxPacketSize);
        } else {
            _isocBandwidth += (ep->maxPacketSize - maxPacketSize);
        }
        
        if (ep->buffersInUse == 0) {
            /* There are no buffers, so it's OK to change the buffer size.
             * Free all unused buffers.
             */
            if (EndpointFreeAllBuffers(ep) != kIOReturnSuccess) {
                USBError(1, "%s[%p]: error attempting to free endpoint alignment buffers",
                         getName(), this);
                return kIOReturnNoMemory;
            }
            ep->maxBufferSize = maxPacketSize;
        }
        
        USBLog(3, "%s[%p]: packet size adjusted from %d to %d", 
               getName(), this, ep->maxPacketSize, maxPacketSize);
        ep->maxPacketSize = maxPacketSize;

        return kIOReturnSuccess;
    }

    if (maxPacketSize > _isocBandwidth) {
        USBLog(3, "%s[%p]: requested bandwidth %d greater than available bandwidth %d",
               getName(), this, maxPacketSize, _isocBandwidth);
        return kIOReturnNoBandwidth;
    }

    ep = AllocEndpoint(functionNumber, endpointNumber, direction, 0, maxPacketSize, kUSBIsoc);
    
    if (ep == NULL) {
        return kIOReturnNoMemory;
    }

    if (direction == kUSBIn) {
        token = kUHCI_TD_PID_IN |
        UHCI_TD_SET_MAXLEN(maxPacketSize) |
        UHCI_TD_SET_ENDPT(ep->endpointNumber) |
        UHCI_TD_SET_ADDR(ep->functionNumber);
    } else if (direction == kUSBOut) {
        token = kUHCI_TD_PID_OUT |
        UHCI_TD_SET_MAXLEN(maxPacketSize) |
        UHCI_TD_SET_ENDPT(ep->endpointNumber) |
        UHCI_TD_SET_ADDR(ep->functionNumber);
    } else {
        USBError(1, "%s[%p]: invalid direction %d in creating isoch endpoint", getName(), this, direction);
        FreeEndpoint(ep);
        return kIOReturnBadArgument;
    }
    USBLog(3, "%s[%p]: setting maxpacket %d endpoint %d function %d token = %x",
           getName(), this, 
           maxPacketSize, ep->endpointNumber, ep->functionNumber, token);
    
    _isocBandwidth -= maxPacketSize;

    /* Initialize isoc TDs and insert in schedule.
     */
    for (i=0; i<kUHCI_NVFRAMES; i++) {
        frame = i % kUHCI_NVFRAMES;
        td = ep->isoc_tds[i];
        if (td == NULL) {
            /* This should never happen */
            /* XXX should release remaining TDs. */
            USBError(1, "%s[%p]: NULL td in isoc frame %d", frame);
            return kIOReturnNoMemory;
        }
        USBLog(7, "%s[%p]: inserting isoc td %p in frame %d", getName(), this, td, frame);
        
        td->hw.ctrlStatus = HostToUSBLong(kUHCI_TD_ISO | UHCI_TD_SET_ERRCNT(1) | UHCI_TD_SET_ACTLEN(0));
        td->hw.token = HostToUSBLong(token);
        IOSync();
        
        vtd = _vframes[frame].td;
        td->link = vtd->link;
        td->hw.link = vtd->hw.link;
        IOSync();
        vtd->link = td;
        vtd->hw.link = HostToUSBLong(td->paddr);
        IOSync();
    }
    USBLog(3, "%s[%p]: IsochEndpoint successfully created.", getName(), this);
#if DEBUG
    USBLog(3, "%s[%p]: --isoc---------Dumping Frame 0:---------------", getName(), this);
    DumpFrame();
#endif
    
    return kIOReturnSuccess;
}


IOReturn
AppleUSBUHCI::StartIsochTransfer(
                                 UHCITransaction *tp
                                 )
{
    IOUSBIsocFrame			*pf = (IOUSBIsocFrame *)tp->isoc_frames;
    IOUSBLowLatencyIsocFrame            *pllf = (IOUSBLowLatencyIsocFrame *)tp->isoc_frames;
    UHCIEndpoint                        *ep = tp->endpoint;
    IOPhysicalAddress paddr;
    IOByteCount offset, phys_len;
    unsigned int i;
    unsigned int skipCount;
    UInt32				frameCount = tp->isoc_num_frames;
    SInt32 frameOffset;
    UInt32 frame;
    UInt32 checkFrame;
#if DEBUG
    UInt32 debugFrame1, debugFrame2;
#endif
    UInt32 requestFrameIndex;
    unsigned int len;
    UInt32 status, token;
    TD *td = NULL;
    
    USBLog(6, "%s[%p]::StartIsochTransfer (tp %p)", getName(), this, tp);
    
    // XXX?
    if (ep->stalled) {
        USBLog(6, "%s[%p]::CreateIsochTransfer on stalled endpoint", getName(), this);
        //return kIOUSBPipeStalled;
    }
    
    if (ep->maxPacketSize == 0) {
        return kIOReturnBadArgument;
    }
    
    frame = ReadFrameNumber();
#if DEBUG
    debugFrame1 = frame;
#endif
    frameOffset = tp->isoc_full_start_frame - GetFrameNumber();
    
    // Check frame offset to see if this transaction can be started.
    if ((frameOffset + (SInt32)frameCount) <= (kUHCI_MIN_FRAME_OFFSET - (kUHCI_NVFRAMES * 10))) {
        // No frames would be queued in this case
        IOLog("%s[%p]: requested offset %d is too old, rejecting\n", getName(), this, (int)frameOffset); // XXX
        USBLog(6, "%s[%p]: requested offset %d is too old, rejecting", getName(), this, frameOffset);
        return kIOReturnIsoTooOld;
    }
    
    if ((frameOffset + (SInt32)frameCount) > kUHCI_NVFRAMES) {
        USBLog(6, "%s[%p]: requested offset %d is too new, rejecting", getName(), this, frameOffset);
        return kIOReturnIsoTooNew;
    }
    
    if (frameOffset < kUHCI_MIN_FRAME_OFFSET) {
        USBLog(2, "%s[%p]: warning, requested offset %d will result in dropped frames", getName(), this, frameOffset);
        // Don't even queue requested frames if they are before the current one.
        requestFrameIndex = kUHCI_MIN_FRAME_OFFSET - frameOffset;
        if (requestFrameIndex > frameCount) {
            requestFrameIndex = frameCount;
        }
        USBLog(2, "%s[%p]: UHCI: Frame offset %d < %d, skipping %d\n", getName(), this, frameOffset, kUHCI_MIN_FRAME_OFFSET, requestFrameIndex);
        frameOffset += requestFrameIndex;
        frameCount -= requestFrameIndex;
        /* Mark the old frames as not sent */
        if (tp->isoc_low_latency) {
            for (i=0; i<requestFrameIndex; i++) {
                pllf[i].frStatus = kIOUSBNotSent2Err;
            }
        }
    } else {
        requestFrameIndex = 0;
    }
    
    if (frameCount > 0) {
        frame = frame + frameOffset;
        frame = (frame % kUHCI_NVFRAMES);
#if DEBUG
        debugFrame2 = frame;
#endif
        
        /* Check for overlapping frames */
        checkFrame = frame;
        skipCount = 0;
        for (i=0; i<frameCount; i++) {
            td = ep->isoc_tds[checkFrame];
            
            if (USBToHostLong(td->hw.ctrlStatus) & kUHCI_TD_ACTIVE) {
                skipCount = i + 1;
            }
            if (++checkFrame >= kUHCI_NVFRAMES)
                checkFrame = 0;
        }
        if (skipCount > 0) {
            if (tp->isoc_low_latency) {
                for (i=0; i < skipCount; i++) {
                    pllf[i + requestFrameIndex].frStatus = kIOUSBNotSent2Err;
                }
            }
            frame = frame + skipCount;
            frame = (frame % kUHCI_NVFRAMES);
            frameOffset += skipCount;
            frameCount -= skipCount;
        }
    }
    
    if (frameCount == 0) {
        /* No work to do. */
        //CompleteIsoc(tp->isoc_completion, kIOReturnSuccess, tp->isoc_frames);
        //FreeTransaction(tp);
        tp->isoc_num_frames = 0;
        StartTransaction(tp);
        _interruptSource->signalInterrupt();
        return kIOReturnSuccess;
    }
        
    tp->isoc_start_frame = frame;
    
    // Normally, isochronous requests should not have a timeout,
    // because they are guaranteed to be processed.
    // This timeout ensures they will be cleared in case of
    // some kind of controller failure.
    tp->timeout = frameOffset + frameCount + kUHCI_NVFRAMES;    
        
    /* Set up TDs. */
    
    phys_len = 0;
    offset = 0;
    paddr = 0;
    tp->first_td = NULL;
    
    for (i=0; i<frameCount; i++, requestFrameIndex++) {
        td = ep->isoc_tds[frame];
        
        if (USBToHostLong(td->hw.ctrlStatus) & kUHCI_TD_ACTIVE) {
            UHCITransaction *tp2;
            int count = 0;
            
            USBError(1, "%s[%p]: wrapping %s isoch request for frame %d (%d of %d)", getName(), this, (ep->direction == kUSBIn) ? "IN" : "OUT", frame, i, frameCount);
            for (i=0; i<kUHCI_NVFRAMES; i++) {
                td = ep->isoc_tds[i];
                if (USBToHostLong(td->hw.ctrlStatus) & kUHCI_TD_ACTIVE)
                    count++;
            }
            USBError(1, "%s[%p]: the current frame is %d", getName(), this, ReadFrameNumber());
            USBError(1, "%s[%p]: there are %d/%d isoc TDs active for this endpoint", getName(), this, count, kUHCI_NVFRAMES);
            count = 0;
            queue_iterate(&ep->activeTransactions, tp2, UHCITransaction *, endpoint_chain) {
                count++;
            }
            USBError(1, "%s[%p]: there are %d active transactions for this endpoint", getName(), this, count);

            DumpTransaction(tp, 2);
            USBError(2, "%s[%p]: conflicting transactions:", getName(), this);
            queue_iterate(&_activeTransactions, tp2, UHCITransaction *, active_chain) {
                if (tp != tp2 && tp2->type == kUSBIsoc && (frame >= tp2->isoc_start_frame && frame < (tp2->isoc_start_frame + tp2->isoc_num_frames))) {
                    DumpTransaction(tp2, 2);
                }
            }
            
            //FreeTransaction(tp);
            return kIOReturnIsoTooNew;
        }
        
        if (tp->first_td == NULL) {
            tp->first_td = td;
        }
        
        /* Note that the frames are not soft linked together. */
        
        if (tp->isoc_low_latency) {
            len = pllf[requestFrameIndex].frReqCount;
            pllf[requestFrameIndex].frStatus = kUSBLowLatencyIsochTransferKey;
        } else {
            len = pf[requestFrameIndex].frReqCount;
        }
        
        paddr = tp->buf->getPhysicalSegment(offset, &phys_len);
        
        if (phys_len < len) {
            UHCIAlignmentBuffer *bp;
            
            /* Need to use an alignment buffer. */
            tp->isoc_unaligned = true;
            USBLog(2, "%s[%p]:  ****** Offset %d physical length %d less than transfer length %d! *****",
                   getName(), this, offset, (int)phys_len, len);
            bp = EndpointAllocBuffer(ep);
            if (bp == NULL) {
                USBError(1, "%s[%p]: Could not allocate alignment buffer for isoch transaction", getName(), this);
                //FreeTransaction(tp);
                return kIOReturnNoMemory;
            }
            USBLog(2, "%s[%p]:  ****** using alignment buffer %p vaddr %p", getName(), this, bp, bp->vaddr);
            
            td->buffer = bp;
            bp->userBuffer = tp->buf;
            bp->userOffset = offset;
            bp->userAddr = NULL;
            paddr = bp->paddr;
            
            if (ep->direction != kUSBIn) {                
                if (tp->isoc_map == NULL) {
                    tp->isoc_map = tp->buf->map();
                }
                
                bp->userBuffer = NULL;
                bp->userOffset = 0;
                if (tp->isoc_map == NULL) {
                    USBLog(2, "%s[%p]: null map on unaligned isoc output buffer", getName(), this);
                    bzero((void *)bp->vaddr, len);
                    bp->userAddr = NULL;
                    bp->userLength = 0;
                } else {
                    bp->userAddr = tp->isoc_map->getVirtualAddress() + offset;
                    bp->userLength = len;
                }
                
                USBLog(2, "%s[%p]: copying %d bytes in", getName(), this, len);
                                
                /* Set the IOC bit a couple of frames before this one,
                    * so the filter interrupt routine can copy the data
                    * into the alignment buffer.
                    *
                    * Go ahead and copy the first two frames.  After that,
                    * leave them for the interrupt routine to handle.
                    */
                if (i < 2) {
                    if (bp->userAddr != NULL) {
                        bcopy((void *)bp->userAddr, (void *)bp->vaddr, bp->userLength);
                        /* Mark the buffer so it won't be copied again. */
                        bp->userAddr = NULL;                    
                    }
                } else {
                    unsigned int early_frame;
                    TD *early_td;
                    
                    early_frame = (frame - 2); // This statement ust be split into two parts
                    early_frame = early_frame % kUHCI_NVFRAMES;
                    early_td = ep->isoc_tds[early_frame];
                    if (early_td) {
                        USBLog(6, "%s[%p]: setting IOC bit on frame %d", getName(), this, early_frame);
                        early_td->hw.ctrlStatus |= HostToUSBLong(kUHCI_TD_IOC);
                        IOSync();
                    }
                }
                
            }
        }
        td->hw.buffer = HostToUSBLong(paddr);
        IOSync();
        
        offset += len;
        
        token = USBToHostLong(td->hw.token);
        token &= ~kUHCI_TD_MAXLEN_MASK;
        token |= UHCI_TD_SET_MAXLEN(len);
        td->hw.token = HostToUSBLong(token);
        IOSync();
        status = kUHCI_TD_ISO |
            kUHCI_TD_ACTIVE |
            UHCI_TD_SET_ERRCNT(1) |
            UHCI_TD_SET_ACTLEN(0);
        
        /* Set the interrupt bit on the first and last frame. 
            * This will allow the filter interrupt to 
            * timestamp the first frame.
            */
        if ((i == 0 && tp->isoc_low_latency) || (i == (frameCount - 1))) {
            status |= kUHCI_TD_IOC;
            USBLog(6, "%s[%p]: setting IOC bit on frame %d",
                   getName(), this, frame);
            if (tp->isoc_low_latency) {
                td->fllp = &pllf[requestFrameIndex];
            }
        }
        td->hw.ctrlStatus = HostToUSBLong(status);
        IOSync();
        
        USBLog(6, "%s[%p]: inserting isoc td %p frame %d", 
               getName(), this, td, frame);
        
        
        frame++;
        if (frame >= kUHCI_NVFRAMES) {
            frame = 0;
        }
    }
    tp->last_td = td;
        
#if DEBUG
    if (ReadFrameNumber() != debugFrame1) {
        USBError(1, "%s[%p]: frame number changed (%d/%d) in StartIsochTransfer", getName(), this, debugFrame1, (UInt32)ReadFrameNumber());
        USBError(1, "%s[%p]: started queueing %d, frameCount %d", getName(), this, debugFrame2, frameCount);
    }
#endif /* DEBUG */
    
    StartTransaction(tp);
    USBLog(6, "%s[%p]: activated transaction %p", getName(), this, tp);
    
    USBLog(6, "%s[%p]: all isoc TDs activated", getName(), this);
    
    return kIOReturnSuccess;    
}


IOReturn
AppleUSBUHCI::CreateIsochTransfer(
                                     short				functionNumber,
                                     short				endpointNumber,
                                     IOUSBIsocCompletion		completion,
                                     UInt8				direction,
                                     UInt64				frameStart,
                                     IOMemoryDescriptor *		pBuffer,
                                     UInt32				frameCount,
                                     void *			        pFrames,
                                     UInt32                             updateFrequency,
                                     bool                               isLowLatency)
{
    IOUSBIsocFrame			*pf = (IOUSBIsocFrame *)pFrames;
    IOUSBLowLatencyIsocFrame            *pllf = (IOUSBLowLatencyIsocFrame *)pFrames;
    UHCIEndpoint *ep;
    UHCITransaction *tp;
    IOPhysicalAddress paddr;
    IOByteCount offset, phys_len;
    unsigned int i;
    SInt32 frameOffset;
    UInt32 frame;
    UInt32 requestFrameIndex;
    unsigned int len;
    UInt32 status, token;
    TD *td = NULL;
    
    USBLog(6, "%s[%p]::CreateIsochTransfer (fn %d, ep %d, dir %s) frame %d count %d LL %d", getName(), this,
           functionNumber, endpointNumber,
           direction == kUSBIn ? "IN" : "OUT",
           (int)frameStart, (int)frameCount, isLowLatency);
    
    if (frameCount == 0) {
        return kIOReturnBadArgument;
    }
    
    ep = FindEndpoint(functionNumber, endpointNumber, direction);
    if (ep == NULL) {
        return kIOUSBEndpointNotFound;
    }
    
    // XXX?
    if (ep->stalled) {
        USBLog(2, "%s[%p]::CreateIsochTransfer on stalled endpoint", getName(), this);
        //return kIOUSBPipeStalled;
    }

    if (ep->maxPacketSize == 0) {
        return kIOReturnBadArgument;
    }
        
    /* Allocate and fill in transaction. */
    
    tp = AllocTransaction(ep);
    if (tp == NULL) {
        return kIOReturnNoMemory;
    }

    //USBLog(6, "%s[%p]: this translates into virtual frame %u, tp %p", getName(), this, frame, tp);
    
    tp->isoc_num_frames = frameCount;
    tp->isoc_frames = pFrames;
    tp->isoc_low_latency = isLowLatency;
    tp->isoc_map = NULL;
    //tp->isoc_start_frame = frame;
    tp->first_td = NULL;
    tp->last_td = NULL;
    tp->nCompletions = 1;
    tp->isoc_completion = completion;
    tp->buf = pBuffer;
        
    tp->isoc_full_start_frame = frameStart;
    tp->isoc_request_received = GetFrameNumber();
    
    IOReturn result = StartIsochTransfer(tp);
    if (result != kIOReturnSuccess) {
        USBLog(2, "%s[%p]:StartIsochTransfer failed", getName(), this);
        FreeTransaction(tp);
    }
    return result;
}


IOReturn
AppleUSBUHCI::UIMCreateIsochTransfer(
                                     short				functionNumber,
                                     short				endpointNumber,
                                     IOUSBIsocCompletion		completion,
                                     UInt8				direction,
                                     UInt64				frameStart,
                                     IOMemoryDescriptor *		pBuffer,
                                     UInt32				frameCount,
                                     IOUSBIsocFrame			*pFrames)
{
    return CreateIsochTransfer(functionNumber, endpointNumber, completion, direction,
                               frameStart, pBuffer, frameCount,
                               (void *)pFrames, 0, false);
}


IOReturn 
AppleUSBUHCI::UIMCreateIsochTransfer(
                                     short				functionNumber,
                                     short				endpointNumber,
                                     IOUSBIsocCompletion                completion,
                                     UInt8				direction,
                                     UInt64				frameStart,
                                     IOMemoryDescriptor *		pBuffer,
                                     UInt32				frameCount,
                                     IOUSBLowLatencyIsocFrame		*pFrames,
                                     UInt32				updateFrequency)
{
    return CreateIsochTransfer(functionNumber, endpointNumber, completion, direction,
                               frameStart, pBuffer, frameCount,
                               (void *)pFrames, updateFrequency, true);
}


// ========================================================================
#pragma mark Endpoints
// ========================================================================


void
AppleUSBUHCI::ReturnEndpointTransactions(
                               UHCIEndpoint                     *ep,
                               IOReturn                         status)
{
    UHCITransaction *tp;
    queue_head_t complete;

    queue_init(&complete);
    
    /* Move pending transactions to queue to be completed. */
    while (!queue_empty(&ep->pendingTransactions)) {
        queue_remove_first(&ep->pendingTransactions, tp, UHCITransaction *, endpoint_chain);
        queue_enter(&complete, tp, UHCITransaction *, active_chain);
    }
    
    while (!queue_empty(&ep->activeTransactions)) {
        queue_remove_first(&ep->activeTransactions, tp, UHCITransaction *, endpoint_chain);
        
        if (tp->state == kUHCI_TP_STATE_ACTIVE) {
            queue_remove(&_activeTransactions, tp, UHCITransaction *, active_chain);
            tp->state = kUHCI_TP_STATE_ABORTED;
            /* Remove from hardware queue. */
            HWCompleteTransaction(tp);
        }
        queue_enter(&complete, tp, UHCITransaction *, active_chain);
    }
    
    /* Allow time for the hardware to finish the frame. */
    IOSleep(2);
    
    while (!queue_empty(&complete)) {
        queue_remove_first(&complete, tp, UHCITransaction *, active_chain);
        USBLog(4, "%s[%p]:: returning transaction %p with status %s", getName(), this, tp, USBErrorToString(status));
        CompleteTransaction(tp, status);
    }
}


IOReturn
AppleUSBUHCI::UIMAbortEndpoint(
                               short				functionNumber,
                               short				endpointNumber,
                               short				direction)
{
    UHCIEndpoint *ep;
    UHCITransaction *tp;
    queue_head_t complete;
    
    USBLog(3, "%s[%p]::UIMAbortEndpoint %d %d %d", getName(), this,
           functionNumber, endpointNumber, direction);
    
    if (functionNumber == _rootFunctionNumber) {
        if (endpointNumber != 0 && endpointNumber != 1) {
            return kIOReturnBadArgument;
        }
        return RHAbortEndpoint(endpointNumber, direction);
    }
    
    ep = FindEndpoint(functionNumber, endpointNumber, direction);
    if (ep == NULL) {
        return kIOUSBEndpointNotFound;
    }
    
    USBLog(3, "%s[%p]: aborting endpoint %p type %d", getName(), this, ep, ep->type);
    
    ReturnEndpointTransactions(ep, (ep->type == kUSBIsoc) ? kIOReturnAborted : kIOUSBTransactionReturned);
    
    ep->stalled = false;
    
    USBLog(3, "%s[%p]: finished aborting endpoint %p", getName(), this, ep);
    
    return kIOReturnSuccess;
}


IOReturn
AppleUSBUHCI::UIMDeleteEndpoint(
                                short				functionNumber,
                                short				endpointNumber,
                                short				direction)
{
    UHCIEndpoint *ep;
    int i;
    
    USBLog(3, "%s[%p]::UIMDeleteEndpoint %d %d %d", getName(), this,
           functionNumber, endpointNumber, direction);
    
#if DEBUG
    DumpFrame(0);
#endif
    
    if (functionNumber == _rootFunctionNumber) {
        if (endpointNumber != 0 && endpointNumber != 1) {
            return kIOReturnBadArgument;
        }
        return RHDeleteEndpoint(endpointNumber, direction);
    }
    
    ep = FindEndpoint(functionNumber, endpointNumber, direction);

    if (ep == NULL) {
        return kIOUSBEndpointNotFound;
    }

    USBLog(3, "%s[%p]: deleting endpoint %p", getName(), this, ep);
    
    ReturnEndpointTransactions(ep, kIOUSBTransactionReturned);
    
    if (ep->type == kUSBIsoc) {
        TD *td, *vtd;
        
        USBLog(3, "%s[%p]: deleting isoc TDs from frame list", getName(), this);
        
        /* Make sure all TDs are inactive. */
        for (i=0; i<kUHCI_NVFRAMES; i++) {
            ep->isoc_tds[i]->fllp = NULL;
            ep->isoc_tds[i]->hw.ctrlStatus &= HostToUSBLong(~(kUHCI_TD_ACTIVE|kUHCI_TD_IOC));
            IOSync();
        }
        IOSleep(2);
        
        /* Remove TDs from vframes table. */
        for (i=0; i<kUHCI_NVFRAMES; i++) {
            td = ep->isoc_tds[i];
            for (vtd = _vframes[i].td; vtd != NULL && vtd->link != td; vtd = vtd->link) {
                /* */
            }
            if (vtd == NULL) {
                /* This should never happen. */
                USBError(1, "%s[%p]: frame %d TD %p not found when deleting isoc endpoint %p",
                         getName(), this, i, td, ep);
#if DEBUG
                if (i == 0) {
                    DumpFrame(0);
                }
#endif
                continue;
            }
            USBLog(7, "%s[%p]: removing isoc td %p in frame %d", getName(), this, td, i);
            vtd->link = td->link;
            vtd->hw.link = td->hw.link;
            IOSync();
        }
        
        /* Release bandwidth. */
        _isocBandwidth += ep->maxPacketSize;
    }
        
    queue_remove(&_endpoints, ep, UHCIEndpoint *, chain);
    FreeEndpoint(ep);

    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::UIMClearEndpointStall(
                                    short				functionNumber,
                                    short				endpointNumber,
                                    short				direction)
{
    UHCIEndpoint *ep;
    
    USBLog(3, "%s[%p]::UIMClearEndpointStall %d %d %d", getName(), this,
           functionNumber, endpointNumber, direction);
    
    ep = FindEndpoint(functionNumber, endpointNumber, direction);
    
    if (ep == NULL) {
        return kIOUSBEndpointNotFound;
    }    
    
    ReturnEndpointTransactions(ep, kIOUSBTransactionReturned);
    
    ep->stalled = false;
    ep->lastDBit = true; // start over with DATA0
    
#if DEBUG
    // This seems to help with some drivers that don't correctly clear the endpoint stall
    // after sending the command to a device to clear a halt condition.
    USBLog(7, "%s[%p]::clearing toggle on all (%d, %d, %d)", getName(), this, functionNumber, endpointNumber, direction);
    queue_iterate(&_endpoints, ep, UHCIEndpoint *, chain) {
        if (ep->functionNumber == functionNumber) {
            USBLog(5, "%s[%p]: endpoint %p (%d, %d, %d) getting its D bit reset", getName(), this, ep,
                   ep->functionNumber, ep->endpointNumber, ep->direction);
            ep->lastDBit = true;
        }            
    }
#endif
    
    USBLog(3, "%s[%p]::UIMClearEndpointStall done ep %p", getName(), this, ep);
    
    return kIOReturnSuccess;
}

UHCIEndpoint *
AppleUSBUHCI::FindEndpoint(IOUSBCommand *command)
{
    short functionNumber, endpointNumber;
    UInt8 direction;
    
    functionNumber = command->GetAddress();
    endpointNumber = command->GetEndpoint();
    direction = command->GetDirection();
    return FindEndpoint(functionNumber, endpointNumber, direction);
}

UHCIEndpoint *
AppleUSBUHCI::FindEndpoint(short functionNumber,
                           short endpointNumber,
                           UInt8 direction)
{
    UHCIEndpoint *ep;
    
    USBLog(7, "%s[%p]::FindEndpoint(%d, %d, %d)", getName(), this, functionNumber, endpointNumber, direction);
    queue_iterate(&_endpoints, ep, UHCIEndpoint *, chain) {
        //USBLog(7, "Scanning endpoint (%d, %d, %d)", ep->functionNumber, ep->endpointNumber, ep->direction);
        if (ep->functionNumber == functionNumber
            && ep->endpointNumber == endpointNumber
            && ((direction == kUSBNone && direction != kUSBOut) ||
                (direction == kUSBAnyDirn) ||
                ep->direction == direction)
            ) {
            USBLog(7, "%s[%p]: endpoint %p found", getName(), this, ep);
            return ep;
        }            
    }
    USBLog(7, "%s[%p]: endpoint not found", getName(), this);
    return NULL;
}



// ========================================================================
#pragma mark Transaction starting and completing
// ========================================================================



void AppleUSBUHCI::UIMCheckForTimeouts(void)
{
    AbsoluteTime        currentTime, t;
    UInt64              elapsedTime;
    UHCITransaction     *tp;
    queue_chain_t        save;
    queue_head_t complete;
    UInt64              frameNumber;
    UInt16              status;
    int                 completed;

    if (isInactive() || _powerLevel != kUHCIPowerLevelRunning) {
        return;
    }

    // Check to see if we missed an interrupt.
    completed = ProcessCompletedTransactions();
    if (completed > 0) {
        USBLog(2, "%s[%p]: processed %d completed transactions in UIMCheckForTimeouts", getName(), this, completed);
    }
    
    clock_get_uptime(&currentTime);
    
    status = ioRead16(kUHCI_STS);
    if (status & kUHCI_STS_HCH) {
        /* acknowledge */
        ioWrite16(kUHCI_STS, kUHCI_STS_HCH);
        
        USBError(1, "%s[%p]: Host controller halted, resetting", getName(), this);
        Reset(true);
        Run(true);
    }
    
    /* Adjust 64-bit frame number.
     * This is a side-effect of GetFrameNumber().
     */
    frameNumber = GetFrameNumber();
    
#if DEBUG
    t = currentTime;
    SUB_ABSOLUTETIME(&t, &_lastFrameNumberTime);
    absolutetime_to_nanoseconds(t, &elapsedTime);

    if (frameNumber == _lastTimeoutFrameNumber && elapsedTime > NANOSECOND_TO_MILLISECOND) {
        /* The controller is jammed. */
        USBError(1, "%s[%p]: host controller frame number halted, resetting", getName(), this);
        Reset(true);
        Run(true);
    }
#endif
    
    _lastTimeoutFrameNumber = frameNumber;
    _lastFrameNumberTime = currentTime;
    
    queue_init(&complete);
    
    for (tp = (UHCITransaction *)queue_first(&_activeTransactions), save = tp->active_chain;
         !queue_end(&_activeTransactions, (queue_entry_t)tp);
         tp = (UHCITransaction *)queue_next(&save), save = tp->active_chain) {
        
        if (tp->timeout == 0) {
            continue;
        }
        if (tp->type != kUSBControl && tp->type != kUSBBulk && tp->type != kUSBIsoc) {
            continue;
        }
        
        t = currentTime;
        SUB_ABSOLUTETIME(&t, &tp->timestamp);
        absolutetime_to_nanoseconds(t, &elapsedTime);
        /* Convert to MS. */
        elapsedTime /= NANOSECOND_TO_MILLISECOND;

        if (elapsedTime > tp->timeout) {
            USBLog(4, "%s[%p]: stale transaction %p", getName(), this, tp);
            queue_remove(&_activeTransactions, tp, UHCITransaction *, active_chain);
            tp->state = kUHCI_TP_STATE_ABORTED;

            /* Remove it from endpoint queue also. */
            queue_remove(&tp->endpoint->activeTransactions, tp, UHCITransaction *, endpoint_chain);
            
            queue_enter(&complete, tp, UHCITransaction *, active_chain);
            
            /* Remove from hardware queue. */
            HWCompleteTransaction(tp);
            
#if DEBUG
            if (tp->type == kUSBIsoc) {
                static int dump_count = 0;
                USBLog(4, "%s[%p]: warning: timing out isoc transaction %p timeout %d", getName(), this, tp, tp->timeout);
                USBLog(4, "%s[%p]: tp was queued at %d for frame %d(%d/%d), current frame is %d(%d)", getName(), this,
                       (UInt32)tp->isoc_request_received,
                       (UInt32)tp->isoc_full_start_frame, (UInt32)tp->isoc_full_start_frame % kUHCI_NVFRAMES, tp->isoc_start_frame,
                       (UInt32)GetFrameNumber(), (UInt32)GetFrameNumber() % kUHCI_NVFRAMES);
                if (dump_count < 10) {
                    dump_count++;
                    DumpFrame(tp->isoc_start_frame, 4);
                    DumpTransaction(tp, 4);
                }
            }
#endif
        }
    }
    
    while (!queue_empty(&complete)) {
        queue_remove_first(&complete, tp, UHCITransaction *, active_chain);
        // Try flipping the data toggle bit
        // to re-synchronize.
        if (tp->type == kUSBBulk) {
            tp->endpoint->lastDBit = !tp->endpoint->lastDBit;
        }
        USBLog(4, "%s[%p]: timing out transaction %p", getName(), this, tp);
        DumpTransaction(tp, 2);
        CompleteTransaction(tp, kIOUSBTransactionTimeout);
    }
    
    t = currentTime;
    SUB_ABSOLUTETIME(&t, &_lastTime);
    absolutetime_to_nanoseconds(t, &elapsedTime);
    /* Convert to MS. */
    elapsedTime /= NANOSECOND_TO_MILLISECOND;
    
    //USBLog(5, "%s[%p]: elapsed rh time: %d", getName(), this, elapsedTime);
    
    if (elapsedTime > kUHCICheckForRootHubConnectionsPeriod) {
        
        _lastTime = currentTime;
        
        if (_powerLevel != kUHCIPowerLevelIdleSuspend) {
            
            USBLog(5, "%s[%p]: checking root hub for connections", getName(), this);
            
            /* Check to see if the root hub has no connections. */
            if (RHAreAllPortsDisconnected()) {
                t = currentTime;
                SUB_ABSOLUTETIME(&t, &_rhChangeTime);
                absolutetime_to_nanoseconds(t, &elapsedTime);
                /* Convert to MS. */
                elapsedTime /= NANOSECOND_TO_MILLISECOND;
                
                if (elapsedTime >= kUHCICheckForRootHubInactivityPeriod) {
                    USBLog(5,"%s[%p] Suspending idle root hub", getName(), this);
                    setPowerState( kUHCIPowerLevelIdleSuspend, this);
                }
                // XXX should we set suspend change status bit here?
            }
        }
    }
}


IOReturn
AppleUSBUHCI::TDToUSBError(UInt32 status)
{
    IOReturn result;
    
    status &= kUHCI_TD_ERROR_MASK;
    if (status == 0) {
        result = kIOReturnSuccess;
    } else if (status & kUHCI_TD_CRCTO) {
        /* In this case, the STALLED bit is also set */
        result = kIOReturnNotResponding;
    } else if (status & kUHCI_TD_BABBLE) {
        /* In this case, the STALLED bit is probably also set */
        result = kIOReturnOverrun;
    } else if (status & kUHCI_TD_STALLED) {
        result = kIOUSBPipeStalled;
    } else if (status & kUHCI_TD_DBUF) {
        result = kIOReturnOverrun;
    } else if (status & kUHCI_TD_CRCTO) {
        result = kIOUSBCRCErr;
    } else if (status & kUHCI_TD_BITSTUFF) {
        result = kIOUSBBitstufErr;
    } else {
        result = kIOUSBTransactionReturned;
    }
    return result;
}


void
AppleUSBUHCI::StartTransaction(UHCITransaction *tp)
{
    USBLog(3, "%s[%p]::StartTransaction %p", getName(), this, tp);
    DumpTransaction(tp);
    
    clock_get_uptime(&tp->timestamp);
    clock_get_uptime(&tp->endpoint->timestamp);
    tp->state = kUHCI_TP_STATE_ACTIVE;
    queue_enter(&_activeTransactions, tp, UHCITransaction *, active_chain);
    queue_enter(&tp->endpoint->activeTransactions, tp, UHCITransaction *, endpoint_chain);
}


/* Completion for isochronous transactions. */

void 
AppleUSBUHCI::CompleteIsoc(IOUSBIsocCompletion      completion,
                           IOReturn                 status,
                           void *                   pFrames)
{
    IOUSBIsocCompletionAction action;

    USBLog(6, "%s[%p]::CompleteIsoc status %d (%s) parameter %x pframes %p", getName(), this,
           status, USBErrorToString(status), completion.parameter, pFrames);
    if (completion.action) {
        action = completion.action;
        completion.action = NULL;
        (*action)(completion.target,
                  completion.parameter,
                  status,
                  (IOUSBIsocFrame *)pFrames);
    } else {
        USBLog(2, "%s[%p]:CompleteIsoc has no action!!", getName(), this);
    }
}


/* Completes a transaction with the upper layers.
 */

void
AppleUSBUHCI::CompleteTransaction(UHCITransaction *tp, IOReturn returnCode )
{
    UHCIEndpoint *ep;
    IOReturn result, isoc_result;
    unsigned int i, count, frame;
    UInt32 length, total_length, req_length;
    AbsoluteTime currentTime, delta;
    TD *td;
    UInt32 status, td_status, token, pid;
    IOUSBIsocFrame *fp = (IOUSBIsocFrame *)tp->isoc_frames;
    IOUSBLowLatencyIsocFrame *fllp = (IOUSBLowLatencyIsocFrame *)tp->isoc_frames;
    bool lowLatency = tp->isoc_low_latency;
    UHCIAlignmentBuffer *bp;
    bool needsReset = false;

    USBLog(5, "%s[%p]::CompleteTransaction(%p, %d)", getName(), this, tp, returnCode);
    DumpTransaction(tp, 5);
    
    /* This counts as activity on the endpoint. */
    clock_get_uptime (&currentTime);
    tp->endpoint->timestamp = currentTime;
    
    ep = tp->endpoint;
    isoc_result = kIOReturnSuccess;
    total_length = 0;
    status = 0;
    i = 0;
    
    if (tp->type == kUSBIsoc) {
        TD **isoc_tds = ep->isoc_tds;
        
        // add one millisecond per frame
        nanoseconds_to_absolutetime(NANOSECOND_TO_MILLISECOND, &delta);
        
        // XXX should take timestamp of last frame instead of current time
        
        /* The frames are not linked by the td->link chain.
         * Look through them one by one in the array.
         */
        for (i=0, frame = tp->isoc_start_frame; i<tp->isoc_num_frames; i++) {
            td = isoc_tds[frame];
            
            status = USBToHostLong(td->hw.ctrlStatus);
            token = USBToHostLong(td->hw.token);
            length = UHCI_TD_GET_ACTLEN(status);
            USBLog(6, "%s[%p]: first isoc td %p frame %d length %d", getName(), this, td, frame, length);
            
            if (lowLatency) {
                req_length = fllp[i].frReqCount;
            } else {
                req_length = fp[i].frReqCount;
            }
            
            /* Check for alignment buffer */
            pid = UHCI_TD_GET_PID(token);
            bp = td->buffer;
            if (bp != NULL && bp->userBuffer != NULL && pid == kUHCI_TD_PID_IN) {
                USBLog(6, "%s[%p]: writing %d bytes to alignment buffer at %p", getName(), this, length, bp->vaddr);
                bp->userBuffer->writeBytes(bp->userOffset, (const void *)bp->vaddr, length);
            }
#if DEBUG
            /* XXX Debugging log */
            if (bp != NULL && pid == kUHCI_TD_PID_OUT) {
                if (bp->userAddr != 0) {
                    USBLog(2, "%s[%p]: isoc data out for frame %d wasn't copied at interrupt time",
                           getName(), this, frame);
                }
            }
#endif
            if (bp != NULL) {
                EndpointFreeBuffer(ep, bp);
                td->buffer = NULL;
            }
            
            result = TDToUSBError(status);
            if (result == kIOReturnSuccess) {
                if ((status & kUHCI_TD_ACTIVE) || (length == 0)) {
                    result = kIOUSBNotSent2Err;
                } else if ((ep->direction == kUSBIn) && (length < req_length)) {
                    //USBLog(7, "%s[%p]: underrun for length %d less than req %d",
                    //       getName(), this, length, req_length);
                    result = kIOReturnUnderrun;
                }
            }
            
            if (lowLatency) {
                fllp[i].frStatus = result;
                if (result == kIOReturnSuccess) {
                    fllp[i].frActCount = fllp[i].frReqCount;
                } else {
                    fllp[i].frActCount = length;
                }
                if (i == 0) {
                    if (fllp[i].frStatus == kUSBLowLatencyIsochTransferKey) {
                        /* This is probably because the transaction was aborted. */
                        USBLog(2, "%s[%p]: XXX timestamp missing on first frame (%d) of %s isoc trans %p",
                               getName(), this, frame,
                               (pid == kUHCI_TD_PID_IN ? "IN" : "OUT"), tp);
                        /* Reverse-engineer a time */
                        UInt64 t;
                        absolutetime_to_nanoseconds(currentTime, &t);
                        t -= (NANOSECOND_TO_MILLISECOND * (tp->isoc_num_frames - 1));
                        nanoseconds_to_absolutetime(t, &currentTime);
                        fllp[i].frTimeStamp = currentTime;
                    } else {
                        // start with the timestamp that the FilterInterrupt routine gave us.
                        delta = currentTime;
                        SUB_ABSOLUTETIME(&delta, &fllp[i].frTimeStamp);
                        if (tp->isoc_num_frames > 2) {
                            AbsoluteTime_to_scalar(&delta) = AbsoluteTime_to_scalar(&delta) / (UInt64)(tp->isoc_num_frames - 1);
                        }
                        USBLog(6, "%s[%p]: %d microseconds between frames", 
                               getName(), this, (UInt32)(AbsoluteTime_to_scalar(&delta) / 1000ULL));
                        currentTime = fllp[i].frTimeStamp;
                    }
                } else {
                    fllp[i].frTimeStamp = currentTime;
                }
                ADD_ABSOLUTETIME(&currentTime, &delta);
            } else {
                fp[i].frStatus = result;
                fp[i].frActCount = length;
            }
            if (result != kIOReturnSuccess) {
                if (result != kIOReturnUnderrun) {
                    isoc_result = result;
                } else if (isoc_result == kIOReturnSuccess) {
                    isoc_result = kIOReturnUnderrun;
                }
            }
            
            USBLog(6, "%s[%p]: isoc td %p length %d, req_length %d, result %d", getName(), this,
                   td, length, req_length, result);
            if (result != kIOReturnSuccess && result != kIOReturnUnderrun) {
                DumpTD(td, 6);
            }
            
            frame++;
            if (frame >= kUHCI_NVFRAMES) {
                frame = 0;
            }
        }
        
        if (tp->isoc_map != NULL) {
            tp->isoc_map->release();
            tp->isoc_map = NULL;
        }
    } else /* Non-Isoc transaction */ {
        bool d_bit_of_last_td = ep->lastDBit;
        
        for (td = tp->first_td; td; td = td->link) {
            td_status = USBToHostLong(td->hw.ctrlStatus);
            token = USBToHostLong(td->hw.token);
            USBLog(5, "Checking TD %p", td);
            
            //DumpTD(td);
            
            bp = td->buffer;
            
            if ((_errataBits & kUHCIResetAfterBabble) != 0 && (td_status & kUHCI_TD_BABBLE) != 0)
                needsReset = true;

            if (td_status & kUHCI_TD_ACTIVE) {
                //USBLog(5, "%s[%p]: TD is active, not updating status", getName(), this);
                /* Status returned is that from the last finished TD.
                 * We will take the data toggle bit from the last finished TD.
                 */
            } else {
                status = td_status;
                
                pid = UHCI_TD_GET_PID(token);
                
                if (pid != kUHCI_TD_PID_SETUP) {
                    length = UHCI_TD_GET_ACTLEN(status);
                    if (bp != NULL && bp->userBuffer != NULL && pid == kUHCI_TD_PID_IN) {
                        USBLog(5, "%s[%p]: writing to alignment buffer at %p", getName(), this, bp->vaddr);
                        bp->userBuffer->writeBytes(bp->userOffset, (const void *)bp->vaddr, length);
                    }
                    if (length > 0 && UHCI_TD_GET_MAXLEN(token) > 0) {
                        d_bit_of_last_td = (token & kUHCI_TD_D) ? true : false;
                    }
                } else {
                    length = 0;
                }
                total_length += length;
            }
            
            if (bp != NULL) {
                EndpointFreeBuffer(ep, bp);
                td->buffer = NULL;
            }
            
            if (td == tp->last_td) {
                break;
            }
        }
        
        /* Fix data toggle bit, if necessary. */
        ep->lastDBit = d_bit_of_last_td;
    }

    /* Convert TD status to USB error. */
    if (returnCode != kIOReturnSuccess) {
        result = returnCode;
    } else if (tp->type == kUSBIsoc) {
        result = isoc_result;
    } else {
        result = TDToUSBError(status);
    }
    
    /* Don't accept new transactions if endpoint is stalled */
    if (status & kUHCI_TD_STALLED) {
        USBLog(5, "%s[%p]: tp %p result makes endpoint stalled", getName(), this, tp);
        tp->endpoint->stalled = true;
    }
    
    USBLog(5, "%s[%p]: tp %p final result %d (%s), bufLen %d, length %d", getName(), this,
           tp, result, USBErrorToString(result), tp->bufLen, total_length);
    if (result != kIOReturnSuccess && result != kIOReturnUnderrun) {
        DumpTransaction(tp, 5);
    }
    
    count = tp->nCompletions;
    for (i=0; i<count; i++) {
        if (tp->type == kUSBIsoc) {
            CompleteIsoc(tp->isoc_completion, result, tp->isoc_frames);
        } else {
            USBLog(5, "%s[%p]: Calling %d Complete(x, 0x%x (%s), %d) tp %p", getName(), this,
                   i, result, USBErrorToString(result), (tp->bufLen - total_length), tp);
            Complete(tp->completion, result, tp->bufLen - total_length);
#if DEBUG
            DumpFrame(ReadFrameNumber() % kUHCI_NVFRAMES );
#endif
        }
    }
    FreeTransaction(tp);
    
    if (needsReset) {
        USBError(1, "%s[%p]: Resetting controller due to errors in transaction", getName(), this);
        Reset(true);
        Run(true);
    }
    
    /* Check to see if there are any other transactions to start on this endpoint. */
    if (!queue_empty(&ep->pendingTransactions)) {
        queue_remove_last(&ep->pendingTransactions, tp, UHCITransaction *, endpoint_chain);
        
        USBLog(5, "%s[%p]: starting pended control transaction %p", getName(), this, tp);
        
        StartTransaction(tp);

        if (tp->type == kUSBControl) {
            QueueControl(tp);
        } else if (tp->type == kUSBBulk) {
            QueueBulk(tp);
        } else {
            USBError(1, "%s[%p]: Unexpected transaction type %d on pending queue!",
                     getName(), this, tp->type);
        }
    }
}


// ========================================================================
#pragma mark Transaction descriptors
// ========================================================================


IOReturn
AppleUSBUHCI::AllocTDChain(UHCIEndpoint *ep,
                           IOMemoryDescriptor *mp,
                           IOByteCount len,
                           bool shortOK,
                           short direction,
                           TD **start_p, TD **end_p,
                           bool setVFlag,
                           bool isControlTransfer)
{
    TD *td = NULL, *prev_td;
    UInt32 status;
    IOByteCount pkt_len;
    IOPhysicalAddress paddr;
    IOByteCount offset, phys_len;
    bool d_bit;
    UInt32 token;
    
    USBLog(7, "%s[%p]::AllocTDChain mp %p len %d short %d dir %d pktsize %d", getName(), this,
           mp, len, shortOK, direction, ep->maxPacketSize);
    
    d_bit = ep->lastDBit;
    
    status = kUHCI_TD_ACTIVE | UHCI_TD_SET_ERRCNT(3) | UHCI_TD_SET_ACTLEN(0);
    // If device is low speed, set LS bit in status.
    if (ep->speed == kUSBDeviceSpeedLow) {
        status |= kUHCI_TD_LS;
    }
    if (shortOK) {
        //status |= kUHCI_TD_SPD;
    }
    
    prev_td = NULL;
    offset = 0;
    /* Build the list forward. */
    if (isControlTransfer && len == 0) {
        /* Standalone packet with no data. */
        pkt_len = 0;
        /* Construct packet. */
        td = AllocTD();
        if (td == NULL) {
            return kIOReturnNoMemory;
        }
        td->hw.ctrlStatus = HostToUSBLong(status);
        IOSync();
        if (direction == kUSBIn) {
            /* This must be a Status phase. */
            token = kUHCI_TD_PID_IN |
            UHCI_TD_SET_MAXLEN(pkt_len) |
            UHCI_TD_SET_ENDPT(ep->endpointNumber) |
            UHCI_TD_SET_ADDR(ep->functionNumber);
            // Set D, but don't change the state of the next D bit.
            token |= kUHCI_TD_D;
        } else if (direction == kUSBOut) {
            /* This must be a Status phase. */
            token = kUHCI_TD_PID_OUT |
            UHCI_TD_SET_MAXLEN(pkt_len) |
            UHCI_TD_SET_ENDPT(ep->endpointNumber) |
            UHCI_TD_SET_ADDR(ep->functionNumber);
            // Set D, but don't change the state of the next D bit.
            token |= kUHCI_TD_D;
        } else /* Just assume it's Setup. */ {
            token = kUHCI_TD_PID_SETUP |
            UHCI_TD_SET_MAXLEN(pkt_len) |
            UHCI_TD_SET_ENDPT(ep->endpointNumber) |
            UHCI_TD_SET_ADDR(ep->functionNumber);
            d_bit = 1;
            // Next packet should be DATA0.
        }
        td->hw.token = HostToUSBLong(token);
        td->hw.buffer = 0;
        IOSync();
        *start_p = td;
    } else {
        pkt_len = ep->maxPacketSize;
        /* Go through loop once even if length == 0. */
        do {
            if (len > 0) {
                paddr = mp->getPhysicalSegment(offset, &phys_len);
                if (paddr == 0 || phys_len == 0) {
                    USBError(1, "%s[%p]: bad physical memory when allocating TDs", getName(), this);
                    return kIOReturnNoMemory;
                }
                USBLog(7, "%s[%p]: physical segment at offset %d = %p len %d",
                       getName(), this, offset, paddr, phys_len);
                if (phys_len > len) {
                    phys_len = len;
                }
            } else {
                paddr = 0;
                phys_len = 0;
            }
            
            if (len < pkt_len) {
                /* This will be the last packet. */
                pkt_len = len;
            }
            
            /* Construct packet. */
            td = AllocTD();
            if (td == NULL) {
                return kIOReturnNoMemory;
            }
            
            /* Set up physical pointer in buffer. */
            if (len > 0 && (phys_len < len)) {
                UHCIAlignmentBuffer *bp;
                
                /* Use alignment buffer. */
                bp = EndpointAllocBuffer(ep);
                USBLog(7, "%s[%p]: using alignment buffer %p at paddr %p instead of %p", getName(), this, bp->vaddr, bp->paddr, paddr);
                td->buffer = bp;
                td->hw.buffer = HostToUSBLong(bp->paddr);
                if (direction != kUSBIn) {
                    mp->readBytes(offset, (void *)bp->vaddr, pkt_len);
                }
                bp->userBuffer = mp;
                bp->userOffset = offset;
                bp->userAddr = NULL;
            } else {
                td->hw.buffer = HostToUSBLong(paddr);
            }
            IOSync();
            
            if (prev_td == NULL) {
                *start_p = td;
            } else {
                if (setVFlag) {
                    prev_td->hw.link = HostToUSBLong(td->paddr | kUHCI_TD_VF);
                } else {
                    prev_td->hw.link = HostToUSBLong(td->paddr);
                }
                IOSync();
                prev_td->link = td;
            }
            if (direction == kUSBIn) {
                if (ep->type == kUSBIsoc) {
                    d_bit = false;
                } else {
                    d_bit = !d_bit;
                }
                token = kUHCI_TD_PID_IN |
                UHCI_TD_SET_MAXLEN(pkt_len) |
                UHCI_TD_SET_ENDPT(ep->endpointNumber) |
                UHCI_TD_SET_ADDR(ep->functionNumber);
                // SPD is required to avoid stalling on short transfers.
                status |= HostToUSBLong(kUHCI_TD_SPD);
                if (d_bit) {
                    token |= kUHCI_TD_D;
                }
            } else if (direction == kUSBOut) {
                if (ep->type == kUSBIsoc) {
                    d_bit = false;
                } else {
                    d_bit = !d_bit;
                }
                token = kUHCI_TD_PID_OUT |
                    UHCI_TD_SET_MAXLEN(pkt_len) |
                    UHCI_TD_SET_ENDPT(ep->endpointNumber) |
                    UHCI_TD_SET_ADDR(ep->functionNumber);
                // SPD is undefined for output packets.
                if (d_bit) {
                    token |= kUHCI_TD_D;
                }
            } else /* Just assume it's Setup. */ {
                token = kUHCI_TD_PID_SETUP |
                UHCI_TD_SET_MAXLEN(pkt_len) |
                UHCI_TD_SET_ENDPT(ep->endpointNumber) |
                UHCI_TD_SET_ADDR(ep->functionNumber);
                d_bit = 0;
                // Reset state of D bit.
                // Next packet should be DATA1.
            }
            td->hw.token = HostToUSBLong(token);
            td->hw.ctrlStatus = HostToUSBLong(status);
            IOSync();
            
            USBLog(7, "%s[%p]: TD paddr %p len 0x%x", getName(), this, paddr, pkt_len);
            /* Update data pointers and lengths. */
            prev_td = td;
            paddr += pkt_len;
            phys_len -= pkt_len;
            len -= pkt_len;
            offset += pkt_len;
        } while (len > 0);
    }
    *end_p = td;
    td->hw.link = HostToUSBLong(kUHCI_TD_T);
    IOSync();
    td->link = NULL;
    ep->lastDBit = d_bit;
    
    USBLog(7, "%s[%p]: AllocTDChain finished", getName(), this);
    return kIOReturnSuccess;
}

void
AppleUSBUHCI::FreeTDChain(TD *td)
{
    TD *next;
    
    while (td != NULL) {
        next = td->link;
        FreeTD(td);
        td = next;
    }
}



