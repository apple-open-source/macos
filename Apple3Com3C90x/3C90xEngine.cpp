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

#include <mach/vm_param.h>    // PAGE_SIZE
#include "3C90x.h"

//---------------------------------------------------------------------------
// GetPhysicalFromVirtual

static bool
GetPhysicalFromVirtual( IOBufferMemoryDescriptor * mem,
                        IOVirtualAddress vaddr, IOPhysicalAddress * paddr )
{
    IOByteCount segLength;

    *paddr = mem->getPhysicalSegment(
                     vaddr - (IOVirtualAddress) mem->getBytesNoCopy(),
                     &segLength);

	return ( *paddr != 0 );
}

//---------------------------------------------------------------------------
// allocateDescMemory
//
// Allocate memory for descriptors. Returns true on success.

bool
Apple3Com3C90x::allocateDescMemory( IOBufferMemoryDescriptor ** mem,
                                    IOByteCount                 size )
{
    LOG_DEBUG("%s::%s %ld\n", getName(), __FUNCTION__, size);

    if ( (size == 0) || (size > PAGE_SIZE) || *mem )
    {
        IOLog("%s: unable to allocate descriptor memory\n", getName());
        return false;
    }
    
    *mem = IOBufferMemoryDescriptor::withOptions( kIOMemoryUnshared,
                                                  size, PAGE_SIZE );

    if ( *mem == 0 )
        IOLog("%s: can't allocate %ld bytes of memory\n", getName(), size);

    return (*mem != 0);
}

//---------------------------------------------------------------------------
// freeDescMemory
//
// Free descriptor memory allocated through allocateDescMemory().

void
Apple3Com3C90x::freeDescMemory( IOBufferMemoryDescriptor ** mem )
{
    if ( *mem )
    {
        (*mem)->release();
        *mem = 0;
    }
}

//---------------------------------------------------------------------------
// allocateMemory
//
// Allocate memory for ring descriptors. This method is called only once by
// init(). Returns true on success.

bool Apple3Com3C90x::allocateMemory()
{
    IOByteCount bytes;

    // Make sure the descriptor sizes lends to proper alignment.

    if ( ( (sizeof(TxDescriptor) % kDescriptorAlignment) != 0 ) || 
         ( (sizeof(RxDescriptor) % kDescriptorAlignment) != 0 ) )
    {
        IOLog("%s: descriptor size not multiple of %d bytes!\n",
              getName(), kDescriptorAlignment);
        return false;
    }

    // Allocate memory for TX ring and initialize it.

    bytes = sizeof(TxDescriptor) * _txRingSize;
    if ( allocateDescMemory( &_txRingMem, bytes ) == false )
        return false;

    _txRing = (TxDescriptor *) _txRingMem->getBytesNoCopy();
    bzero( (void *) _txRing, bytes );

    // Allocate memory for RX ring and initialize it.

    bytes = sizeof(RxDescriptor) * _rxRingSize;
    if ( allocateDescMemory( &_rxRingMem, bytes ) == false )
        return false;

    _rxRing = (RxDescriptor *) _rxRingMem->getBytesNoCopy();
	bzero( (void *) _rxRing, bytes );

    _rxRingInited = false;
    _txRingInited = false;

    return true;
}

//---------------------------------------------------------------------------
// updateDescriptor
//
// Attaches a mbuf to a transmit/receive descriptor. Each descriptor
// can point to multiple memory fragments. However, each packet can
// not span across multiple descriptors. In other words, there is a
// fixed one-to-one mapping between packet and descriptor. Boomerang
// allows a maximum of 63 memory fragments per descriptor. But the
// driver will most likely impose a much smaller limit.

bool
Apple3Com3C90x::updateTxDescriptor( TxDescriptor * desc,
                                    struct mbuf *  packet )
{
    UInt32 segmentCount;

    // Generate the physical segments for the mbuf provided.

    segmentCount = _mbufCursor->getPhysicalSegmentsWithCoalesce(
                   /* mbuf     */ packet,
                   /* segments */ (IOPhysicalSegment *) desc->fragments,
                   /* maxSeg   */ kTxFragmentCount );

    if ( segmentCount == 0 )
    {
        LOG_DEBUG("%s: TX IOMbufMemoryCursor error\n", getName());
        return false;
    }

    // Update the header with the total packet length in bytes.
    // Setting the length in the frame header is no longer required
    // for the newer 3C90xB.

    desc->header = SetBitField( TxFragment, Length,
                                packet->m_pkthdr.len );

    // Set dnLastFrag bit on the last fragment.

    desc->fragments[segmentCount - 1].command |= kTxFragmentLastMask;

    return true;
}

bool
Apple3Com3C90x::updateRxDescriptor( RxDescriptor * desc,
                                    struct mbuf *  packet )
{
    UInt32 segmentCount;

    // Generate the physical segments for the mbuf provided.

    segmentCount = _mbufCursor->getPhysicalSegments(
                   /* mbuf     */ packet,
                   /* segments */ (IOPhysicalSegment *) desc->fragments,
                   /* maxSeg   */ kRxFragmentCount );

    if ( segmentCount == 0 )
    {
        LOG_DEBUG("%s: RX IOMbufMemoryCursor error\n", getName());
        return false;
    }

    // Set dnLastFrag bit on the last fragment.

    desc->fragments[segmentCount - 1].command |= kRxFragmentLastMask;

    return true;
}

//---------------------------------------------------------------------------
// initRxRing
//
// Initialize the array of receive descriptors arranged in a ring structure.
// Also allocates cluster mbufs and attach one to each ring element. Returns
// true on success.
//
// Note: This method may be called several times.

bool Apple3Com3C90x::initRxRing()
{
    UInt32 i;

    LOG_DEBUG("%s::%s\n", getName(), __FUNCTION__);

    if ( _rxRingInited ) 
    {
        for ( i = 0; i < _rxRingSize; i++ ) _rxRing[i].status = 0;
        goto ResetPointers;
    }

    for ( i = 0; i < _rxRingSize; i++ )
    {
        // Initialize descriptor, but leave mbuf intact to avoid leaks.

        _rxRing[i].status = 0;
        bzero( &_rxRing[i].fragments, sizeof(_rxRing[i].fragments) );

        // Link current descriptor to next. We skip the last
        // descriptor for now.        

        if ( i < (_rxRingSize - 1) )
        {
            if ( GetPhysicalFromVirtual( _rxRingMem,
                                         (IOVirtualAddress) &_rxRing[i+1],
                                         &_rxRing[i].nextPtr ) != true )
            {
                return false;
            }
        }

        // Attach packet buffer to each descriptor.

        if ( _rxRing[i].drvMbuf == NULL )
        {
            _rxRing[i].drvMbuf = allocatePacket( kRxPacketBufferSize );

            if ( _rxRing[i].drvMbuf == NULL )
            {
                IOLog("%s: initRxRing allocatePacket error\n", getName());
                return false;
            }
        }

        // Link mbuf to descriptor

        if ( updateRxDescriptor( &_rxRing[i], _rxRing[i].drvMbuf) == false )
        {
            IOLog("%s: initRxRing updateDescriptor error", getName());
            return false;
        }

        _rxRing[i].drvNext = &_rxRing[i+1];
    }

    // Wrap the ring by linking the tail to the head.

    if ( GetPhysicalFromVirtual( _rxRingMem,
                                 (IOVirtualAddress) &_rxRing[0],
                                 &_rxRing[i-1].nextPtr ) != true )
    {
        return false;
    }

    _rxRing[i-1].drvNext = &_rxRing[0];

ResetPointers:

    // Initialize tail pointer to point to ring element 0.
    // This is where the driver will start looking when a receive
    // interrupt is received.

    _rxRingTail = _rxRing;
    
    _rxRingInited = true;

    return true;
}

//---------------------------------------------------------------------------
// initTxRing
//
// Initialize the array of transmit descriptors arranged in a ring structure.
// Returns true on success.
//
// Note: This method may be called several times.

bool Apple3Com3C90x::initTxRing()
{
    UInt32 i;

    LOG_DEBUG("%s::%s\n", getName(), __FUNCTION__);

    // Free currently linked mbufs.

    for ( i = 0; i < _txRingSize; i++ )
    {
        if ( _txRing[i].drvMbuf )
        {
            freePacket( _txRing[i].drvMbuf );
            _txRing[i].drvMbuf = 0;
        }
    }

    if ( _txRingInited ) goto ResetPointers;

    // Initialize all TX descriptors.

    bzero( (void *) &_txRing[0], sizeof(TxDescriptor) * _txRingSize );
    
    for ( i = 0; i < _txRingSize; i++ )
    {
        // Cache the physical address of the descriptor.
    
        if ( GetPhysicalFromVirtual( _txRingMem,
                                     (IOVirtualAddress) &_txRing[i],
                                     &_txRing[i].drvPhysAddr ) != true )
        {
            return false;
        }

        _txRing[i].drvNext = &_txRing[i+1];

        if ( i > 0 )
            _txRing[i].drvPrevious = &_txRing[i-1];
    }

    // Wrap the ring.

    _txRing[i-1].drvNext   = &_txRing[0];
    _txRing[0].drvPrevious = &_txRing[i-1];

ResetPointers:

    // Clear ring pointers.
    // Head is where new output packets are inserted. Tail is where
    // the transmit interrupt handler looks for packets to retire.

    _txRingHead = _txRing;
    _txRingTail = _txRing;
    _txRingFree = _txRingSize;

    _txRingInited = true;

    return true;
}

//---------------------------------------------------------------------------
// resetAdapter
//
// Reset the transmitter and receiver.

void Apple3Com3C90x::resetAdapter()
{
    LOG_DEBUG("%s::%s\n", getName(), __FUNCTION__);

    // Reset transmitter and receiver.

    sendCommandWait( TxReset );
    sendCommandWait( RxReset );
}

//---------------------------------------------------------------------------
// resetAndEnableAdapter

bool Apple3Com3C90x::resetAndEnableAdapter( bool enableIRQ )
{
    resetAdapter();

    if ( !initRxRing() || !initTxRing() )
    {
        IOLog("%s: initRings failure\n", getName());
        return false;
    }

    initAdapter();

    enableAdapter( enableIRQ );

    return true;
}

//---------------------------------------------------------------------------
// initTransmitterParameters
//
// Initialize the transmitter thresholds and parameters.

void
Apple3Com3C90x::initTransmitterParameters()
{
    // 3C90x. (first generation - Boomerang)
    //
    // When free space in FIFO exceeds a packet, download will begin.
    // If this value is larger than the FIFO size, then downloading
    // is disabled completely.

    setTxFreeThresh( kTxFreeTreshSizePacket );

    // 3C90xB only.
    //
    // Make a download request when the free space in the FIFO exceed
    // the dnBurstThresh value.

    setDnBurstThresh( _dnBurstThresh >> 5 );

    // 3C90xB only.
    //
    // When the transmitter is about to underrun, and the number of
    // used (valid data) bytes in the TX FIFO drops below dnPriorityThresh,
    // issue a priority bus master request.

    setDnPriorityThresh( _dnPriorityThresh >> 5 );

    // 3C90xB only.
    //
    // Reclaim the FIFO space when txReclaimThresh bytes has been
    // transmitted.

    if ( _adapterInfo->type >= kAdapterType3C90xB )
    {
        sendCommand( SetTxReclaimThresh, _txReclaimThresh >> 4 );
    }
}

//---------------------------------------------------------------------------
// initReceiverParameters
//
// Initializes the receiver's thresholds and parameters.

void
Apple3Com3C90x::initReceiverParameters()
{
    // Set RxEarlyThresh to a large value, which in essence disables it.
    // The Boomerang and Cyclone errata recommend this to avoid a receive
    // bus master upload hang.

    sendCommand( SetRxEarlyThresh, 0x1ffc );
    setDMACtrl( getDMACtrl() | kDMACtrlUpRxEarlyEnableMask );

    // 3C90xB only.
    //
    // Determines when bus master upload requests will be made.

    setUpBurstThresh( _upBurstThresh >> 5 );

    // 3C90xB only.
    //
    // Threshold to control when upload engine makes a priority bus master
    // request.

    setUpPriorityThresh( _upPriorityThresh >> 5 );
}

//---------------------------------------------------------------------------
// initAdapter
//
// Initialize the adapter. This function is called during
// driver initialization and also when a timeout occurs.

bool Apple3Com3C90x::initAdapter()
{
    UInt16  reg16;

    LOG_DEBUG("%s::%s\n", getName(), __FUNCTION__);

    // Update Receive list base (UpListPtr).

    sendCommandWait( UpStall );
    setUpListPtr( _rxRing[ _rxRingSize - 1 ].nextPtr ); // element 0 phys addr
    sendCommand( UpUnStall );

    // Transmit list base (DnListPtr) is left at 0.

    setDnListPtr( 0 );

    initTransmitterParameters();
    initReceiverParameters();

    // Set station address.

    setStationAddress( &_etherAddress );

    // Configure receive filter.

    sendCommand( SetRxFilter, _rxFilterMask );

    // Clear statistics interrupt by reading them.

    sendCommand( StatsDisable );
    updateStatsInterruptHandler();

    // Turn on upperBytesEnable statistics counter. This should reduce
    // the frequency of statistics interrupts.

    reg16 = getNetworkDiagnostic();
    reg16 |= kNetworkDiagnosticUpperByteEnableMask;
    setNetworkDiagnostic( reg16 );

    // Enable statistics reporting.

    sendCommand( StatsEnable );

#if 0
    // WARNING: Do not enable this block of code! Left as a reminder
    // to investigate the definition of Reset Options register, and
    // why setting certain bits will cause bizarre effects following
    // a warm reboot for newer 3C905C based adapters. The symptoms
    // are bogus PCI vendor/device ID, and weird MAC addresses when
    // read from EEPROM.

    // Disable the overrides in ResetOptions.

    reg16 = getResetOptions();
    reg16 &= ~( kResetOptionsDisableAdvFDMask   |
                kResetOptionsDisableAdv100Mask  |
                kResetOptionsDisableAutoNegMask );
    setResetOptions( reg16 );

    LOG_DEBUG("%s::%s ResetOptions = %04x\n", getName(), __FUNCTION__,
              getResetOptions());
#endif

    return true;
}

//---------------------------------------------------------------------------
// disableAdapterInterrupts
//
// Disables all hardware interrupt sources.

void Apple3Com3C90x::disableAdapterInterrupts()
{
    sendCommand( SetIntrEnable, 0 );
    sendCommand( SetIndEnable,  0 );
    _interruptMask = 0;
}

//---------------------------------------------------------------------------
// enableAdapterInterrupts
//
// Enables all interrupt sources that we are interested in servicing.

void Apple3Com3C90x::enableAdapterInterrupts()
{
    // Set up interrupt mask.
    
    _interruptMask = kCommandStatusDnCompleteMask    |  /* transmit   */
                     kCommandStatusUpCompleteMask    |  /* receive    */
                     kCommandStatusTxCompleteMask    |  /* TX Error   */
                     kCommandStatusUpdateStatsMask   |  /* statistics */  
                     kCommandStatusHostErrorMask     |  /* PCI error  */
                     kCommandStatusInterruptLatchMask;  /* don't care */

    // Enable the indications to appear in IntStatus register.

    sendCommand( SetIndEnable, _interruptMask );

    // Ack any pending stale interrupts.

    sendCommand( AckIntr, _interruptMask );

    // Enable interrupts.

    sendCommand( SetIntrEnable, _interruptMask );
}

//---------------------------------------------------------------------------
// disableLinkInterrupt
//
// Disable further LinkEvent interrupts.

void Apple3Com3C90x::disableLinkEventInterrupt()
{
    _interruptMask &= ~kAckLinkEvent;
    sendCommand( SetIndEnable,  _interruptMask );
    sendCommand( SetIntrEnable, _interruptMask );
}

//---------------------------------------------------------------------------
// enableLinkInterrupt
//
// Re-enable LinkEvent interrupts.

void Apple3Com3C90x::enableLinkEventInterrupt()
{
    _interruptMask |= kAckLinkEvent;
    sendCommand( SetIndEnable,  _interruptMask );
    sendCommand( SetIntrEnable, _interruptMask );
}

//---------------------------------------------------------------------------
// enableTransmitter

void Apple3Com3C90x::enableTransmitter()
{
    LOG_DEBUG("%s: enable transmitter\n", getName());

    sendCommand( TxEnable );
}

//---------------------------------------------------------------------------
// enableReceiver

void Apple3Com3C90x::enableReceiver()
{
    LOG_DEBUG("%s: enable receiver\n", getName());

    sendCommand( RxEnable );
}

//---------------------------------------------------------------------------
// enableAdapter

void Apple3Com3C90x::enableAdapter( bool enableInterrupts )
{
	enableTransmitter();
	enableReceiver();

    if ( enableInterrupts )
    {
        enableAdapterInterrupts();
    }
    
    sendCommand( UpUnStall );
    sendCommand( DnUnStall );
}

//---------------------------------------------------------------------------
// outputPacket
//
// Transmit a single packet mbuf.

UInt32
Apple3Com3C90x::outputPacket( struct mbuf * m, void * param )
{
    LOG_TX("%s: transmit [%d]\n", getName(), m->m_pkthdr.len);

    if ( _driverEnableCount == 0 )
    {
        LOG_DEBUG("%s::%s adapter not ready\n", getName(), __FUNCTION__);
        freePacket(m);
        return kIOReturnOutputDropped;
    }

    // Make sure we have space on the descriptor ring for this packet.

    if ( _txRingFree == 0 )
    {
        return kIOReturnOutputStall;
    }

    reserveDebuggerLock();

    // Set up the descriptor.

    if ( _txRingHead->drvMbuf != NULL )
    {
        IOLog("%s::%s drvMbuf not NULL\n", getName(), __FUNCTION__);
        freePacket( _txRingHead->drvMbuf );
    }

    if ( updateTxDescriptor( _txRingHead, m ) == false )
    {
        releaseDebuggerLock();
        freePacket(m);
        _netStats->outputErrors++;
        IOLog("%s::%s updateDescriptor error\n", getName(), __FUNCTION__);
        return kIOReturnOutputDropped;
    }

    _txRingHead->drvMbuf = m;
    _txRingHead->nextPtr = 0;  // Stop after processing this entry

    if ( ++_txRingInt >= _txIntThreshold )
    {
        // generate DnComplete interrupt
        _txRingHead->header |= kTxDescHeaderDnIndicateMask;
        _txRingInt = 0;
    }

    // *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
    // Now do the recommended procedure to attach a descriptor to
    // a descriptor chain/ring.
    //
    // Documented in page 12 of the Boomerang ERS.
    // *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

    // Stall the transmit engine and wait for idle condition.
    
    sendCommandWait( DnStall );

    // link the current descriptor to its previous descriptor.

    _txRingHead->drvPrevious->nextPtr = _txRingHead->drvPhysAddr;

    // Read DownList and update it if necessary.
#if 0
    if ( getDnListPtr() == 0 )
    {
        setDnListPtr( _txRingHead->drvPhysAddr );
    }
#else
    // Boomerang errata claims that writing to DnListPtr while it is
    // not zero will be ignored. Which means we can replace the above
    // one or two I/O operations with a single I/O write.

    setDnListPtr( _txRingHead->drvPhysAddr );
#endif

    // Unstall the transmit engine.

    sendCommand( DnUnStall );

    // *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
    // Descriptor has been attached.
    // *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

    // Advance the ring index to point to the next free descriptor

    _txRingHead = _txRingHead->drvNext;
    _txRingFree--;

    releaseDebuggerLock();
    
    if ( _txRingInt == 0 )
    {
        _newWDCounters[ kWDInterruptsPending ] += 1;
    }

    return kIOReturnOutputSuccess;
}

//---------------------------------------------------------------------------
// transmitInterruptHandler
//
// Service dnComplete interrupts.

void
Apple3Com3C90x::transmitInterruptHandler()
{
    UInt32  listPtrReg;
    UInt32  retiredCount = 0;

    LOG_INT("%s:%s\n", getName(), __FUNCTION__);

    // Find out which transmit descriptor the engine is currently working on.

    listPtrReg = getDnListPtr();

    while ( _txRingFree < _txRingSize )
    {
        // Make sure the transmit engine has finished with this descriptor
        // before retiring it.

        if ( listPtrReg == _txRingTail->drvPhysAddr )
            break;

#if 0        
        if ( _adapterInfo->type == ADAPTER_TYPE_3C90XB )
        {
            if ( ( _txRingTail->header.bits.dnComplete ) == 0 )
            {
                // 3C90xB indicates that download for this packet is
                // not yet complete.
                IOLog("3C90xB: entry %d not yet downloaded!\n", entry);
            }
        }
#endif

        // Return mbuf to the global pool.

        if ( _txRingTail->drvMbuf != NULL )
        {
            freePacket( _txRingTail->drvMbuf );
            _txRingTail->drvMbuf = NULL;
        }

        // Advance ring tail pointer.

        _txRingTail = _txRingTail->drvNext;
        _txRingFree++;
        retiredCount++;
    }

    if ( retiredCount )
    {
        _transmitQueue->service();

        _netStats->outputPackets += retiredCount;
        _newWDCounters[ kWDInterruptsRetired ] += 1;
    }
}

//---------------------------------------------------------------------------
// receiveInterruptHandler
//
// Service receive interrupts.

void
Apple3Com3C90x::receiveInterruptHandler()
{
    struct mbuf * inputPkt;
    UInt32        pktSize;
    UInt32        status;
    bool          replaced;
    UInt32        pktCount = 0;

    LOG_INT("%s::%s index:%d\n", getName(), __FUNCTION__,
            rxRingTail - rxRing);

    for ( status = _rxRingTail->status;
          status & kRxDescStatusUpCompleteMask;
          status = _rxRingTail->status )
    {
        // Packet passed to inputPacket().

        inputPkt = NULL;
        
        // Get received packet length (not including FCS).

        pktSize = GetBitField( RxDescStatus, Length, status );

        // Reject bad packets.

        if ( ( status & kRxDescStatusUpErrorMask ) ||
        	 ( pktSize < (kIOEthernetMinPacketSize - kIOEthernetCRCSize) ) )
        {
            _netStats->inputErrors++;

            if ( status & kRxDescStatusUpOverrunMask )
                _etherStats->dot3RxExtraEntry.overruns++;
            if ( status & kRxDescStatusRuntFrameMask )
                _etherStats->dot3RxExtraEntry.frameTooShorts++;
            if ( status & kRxDescStatusAlignmentErrorMask )
                _etherStats->dot3StatsEntry.alignmentErrors++;
            if ( status & kRxDescStatusCRCErrorMask )
                _etherStats->dot3StatsEntry.fcsErrors++;
            if ( status & kRxDescStatusOversizedFrameMask )
                _etherStats->dot3StatsEntry.frameTooLongs++;
            goto next;
        }

        // Obtain a mbuf to pass up to the networking stack.

        inputPkt = replaceOrCopyPacket( &_rxRingTail->drvMbuf, pktSize,
                                        &replaced );

        if ( inputPkt == 0 )
        {
            _netStats->inputErrors++;
            _etherStats->dot3RxExtraEntry.resourceErrors++;
            goto next;
        }

        // If packet was replaced, we need to recompute the descriptor's
        // phys address/command fields.

        if ( replaced &&
            ( updateRxDescriptor( _rxRingTail,
                                  _rxRingTail->drvMbuf ) == false ) )
        {
            freePacket( _rxRingTail->drvMbuf );  // free the replacement.
            _rxRingTail->drvMbuf = inputPkt;     // get the old packet back.
            inputPkt = 0;                        // pass up nothing.
            _netStats->inputErrors++;
            IOLog("%s: updateDescriptor() error\n", getName());
        }

next:
        // Clear descriptor status.

        _rxRingTail->status = 0;

        // Advance tail to point to the next descriptor.

        _rxRingTail = _rxRingTail->drvNext;

        if ( inputPkt && _netifEnabled )
        {
            _netif->inputPacket( inputPkt, pktSize,
                                 IONetworkInterface::kInputOptionQueuePacket );

            _netStats->inputPackets++;

            pktCount++;
        }
    }

    // Unstall engine, in case the card has stalled.
    
    sendCommand( UpUnStall );

    // Submit all input packets in one shot.

    if ( pktCount )
    {
        _netif->flushInputQueue();
    }
}

//---------------------------------------------------------------------------
// transmitErrorInterruptHandler
//
// Handles various forms of transmitter errors.

void
Apple3Com3C90x::transmitErrorInterruptHandler()
{
    UInt8   txStat;
    UInt32  downListPtr;
    UInt16  intStatus;

    LOG_INT("%s: transmit error interrupt\n", getName());

    txStat      = getTxStatus();
    intStatus   = getCommandStatus();
    downListPtr = getDnListPtr();

    // Acknowledge the txComplete interrupt by writing an arbitrary
    // value to the TxStatus register. This will advance the FIFO.

    setTxStatus( txStat );

    // We need to do different things based on the type of error

    _netStats->outputErrors++;

    if ( txStat & kTxStatusUnderrunMask )
    {
        // Stall the transmitter.

        sendCommandWait( DnStall );

        // Ensure that download and transmission processes are stalled.

        waitForTransmitterIdle();

        // Reset the transmitter.

        sendCommandWait( TxReset );

        _etherStats->dot3TxExtraEntry.underruns++;
        _etherStats->dot3TxExtraEntry.resets++;
    }
    else if ( txStat & kTxStatusJabberMask )
    {
        sendCommandWait( TxReset );

        _etherStats->dot3TxExtraEntry.jabbers++;
        _etherStats->dot3TxExtraEntry.resets++;
    }
    else
    {
        sendCommand( TxEnable );
#if 0
        if ( txStat & kTxStatusMaxCollisionsMask )
        if ( txStat & kTxStatusOverflowMask )
        if ( txStat & kTxStatusReclaimErrorMask )
#endif
        return;
    }

    // Re-enable the transmitter.

    sendCommand( TxEnable );

    // Restore all transmitter parameters following reset.

    initTransmitterParameters();

    // Point DownListPtr at the DPD which experienced the error.

    setDnListPtr( downListPtr );

    // Un-stall the download engine.

    sendCommand( DnUnStall );
}

//---------------------------------------------------------------------------
// hostErrorInterruptHandler
//
// Handles hostError (PCI bus error) interrupts. This is a fatal error
// and a GlobalReset is needed to clear this condition. We assume that
// all interrupt sources in the adapter have been disabled prior to
// calling this method. The hostError interrupt is not cleared until
// a reset is performed.

void
Apple3Com3C90x::hostErrorInterruptHandler()
{
    IOLog("%s: Fatal bus error detected\n", getName());

    // Re-initialize everything.

    if ( resetAndEnable( true ) == false )
    {
        IOLog("%s:%s resetAndEnable error\n", getName(), __FUNCTION__);
    }
}

//---------------------------------------------------------------------------
// linkEventInterruptHandler
//
// Handles 3C90xB LinkEvent interrupts.

void
Apple3Com3C90x::linkEventInterruptHandler()
{
    LOG_INT("%s: linkEventInterruptHandler\n", getName());

    monitorLinkStatus();
}

//---------------------------------------------------------------------------
// updateStatsInterruptHandler
//
// Services updateStats interrupts.

void
Apple3Com3C90x::updateStatsInterruptHandler()
{
    LOG_INT("%s::%s\n", getName(), __FUNCTION__);

    // Fetch the statistics counters from the adapter.
    // This also acks and clears the updateStats interrupt.

    UInt8 lateCollisions   = getLateCollisions();
    UInt8 multiCollisions  = getMultipleCollisions();
    UInt8 singleCollisions = getSingleCollisions();
    UInt8 rxOverruns       = getRxOverruns();
    UInt8 carrierLost      = getCarrierLost();
    UInt8 sqeErrors        = getSqeErrors();

    _netStats->collisions += lateCollisions
                           + multiCollisions
                           + singleCollisions; 
 
    _netStats->outputErrors += carrierLost + sqeErrors;

    _etherStats->dot3StatsEntry.deferredTransmissions   += getFramesDeferred();
    _etherStats->dot3StatsEntry.singleCollisionFrames   += singleCollisions;
    _etherStats->dot3StatsEntry.multipleCollisionFrames += multiCollisions;
    _etherStats->dot3StatsEntry.lateCollisions          += lateCollisions;
    _etherStats->dot3StatsEntry.sqeTestErrors           += sqeErrors;
    _etherStats->dot3StatsEntry.carrierSenseErrors      += carrierLost;
    _etherStats->dot3StatsEntry.missedFrames            += rxOverruns;

    // Not used, but must read them to clear the interrupt.

    getFramesXmittedOk();
    getBytesXmittedOk();

    getFramesRcvdOk();
    getBytesRcvdOk();    

    // Do this last since the BadSSD register in in a different window
    // from the other statistics counters. Minimize window switches.

    _netStats->inputErrors += rxOverruns + getBadSSD();
}

//---------------------------------------------------------------------------
// interruptHandler
//
// Determine the source of the interrupt and dispatch the appropriate
// interrupt service handler.

void
Apple3Com3C90x::interruptHandler( IOInterruptEventSource * src )
{
    UInt16  status;

    // Make sure the driver is ready to service interrupts.

    if ( _driverEnableCount == 0 )
    {
        LOG_INT("%s: unexpected interrupt\n", getName());
        return;
    }

    reserveDebuggerLock();

    while ( 1 )
    {
        // Read interrupt source status.
        
        status = getCommandStatus();

        // Qualify the interrupt with the interrupt mask.

        status &= _interruptMask; 

        // Ack all interrupts. 'updateStats' and 'txComplete' interrupts
        // will require special ack sequences.

        sendCommand( AckIntr, status );

        // Nothing else to service, normal exit from this routine.

        if ( status == 0 ) break;

        // Dispatches the appropriate interrupt handlers to service the
        // interrupt(s).

        if ( status & kCommandStatusUpCompleteMask )
        {
            receiveInterruptHandler();
            _etherStats->dot3RxExtraEntry.interrupts++;
        }

        if ( status & kCommandStatusDnCompleteMask )
        {
            transmitInterruptHandler();
            _etherStats->dot3TxExtraEntry.interrupts++;
        }

        // Low frequency interrupts.

        if ( status & ( kCommandStatusUpdateStatsMask |
                        kCommandStatusTxCompleteMask  |
                        kCommandStatusLinkEventMask   |
                        kCommandStatusHostErrorMask ) )
        {
            if ( status & kCommandStatusUpdateStatsMask )
            {
                updateStatsInterruptHandler();
            }
    
            if ( status & kCommandStatusTxCompleteMask )
            {
                transmitErrorInterruptHandler();
            }
    
            if ( status & kCommandStatusLinkEventMask )
            {
                linkEventInterruptHandler();
            }
            
            if ( status & kCommandStatusHostErrorMask )
            {
                hostErrorInterruptHandler();
            }
        }
    }

    releaseDebuggerLock();
}

//---------------------------------------------------------------------------
// Method: setupMulticastHashFilter
//
// Setup the 3C90xB multicast hash filter using the multicast addresses
// provided.

void
Apple3Com3C90x::setupMulticastHashFilter( IOEthernetAddress * addrs,
                                          UInt32              count )
{
    // First clear the multicast hash filter.

    for ( UInt32 i = 0; i < 256; i++ )
    {
        sendCommand( SetHashFilterBit, i | kHashFilterBitOff );
    }

    // Each Ethernet address is hashed to generate an offset to
    // one of the 256 bit slots.

    for ( UInt32 i = 0; i < count; i++ )
    {
        UInt16 bitSlot = hashMulticastAddress( addrs[i].bytes );
        sendCommand( SetHashFilterBit, bitSlot | kHashFilterBitOn );
    }
}
