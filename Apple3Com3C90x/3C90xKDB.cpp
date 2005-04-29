/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#include "3C90x.h"

//---------------------------------------------------------------------------
// receivePacket
//
// Receive packets in polled mode (not interrupt driven).

void
Apple3Com3C90x::receivePacket( void * pkt, UInt32 * pkt_len, UInt32 timeout )
{
    UInt32  status;
    UInt32  pktLength;
    SInt32  us_timeout = timeout * 1000;

    *pkt_len = 0;

    if ( !_linkTest && ( _driverEnableCount == 0 ) ) return;

    do {
        for ( status = OSReadLittleInt32( &_rxRingTail->status, 0 );
              status & kRxDescStatusUpCompleteMask;
              status = OSReadLittleInt32( &_rxRingTail->status, 0 ) )
        {
            pktLength = GetBitField( RxDescStatus, Length, status );

            // Check for bad packets.

            if ( ( status & kRxDescStatusUpErrorMask ) ||
                 ( pktLength < ( kIOEthernetMinPacketSize - kIOEthernetCRCSize ) ) )
            {
                LOG_DEBUG("receivePacket: bad packet\n");

                _rxRingTail->status = 0;
                _rxRingTail = _rxRingTail->drvNext;

                sendCommand( UpUnStall );
            }
            else
            {
                *pkt_len = pktLength;

                if ( pkt )
                    bcopy( mbuf_data(_rxRingTail->drvMbuf), pkt, *pkt_len );

                _rxRingTail->status = 0;
                _rxRingTail = _rxRingTail->drvNext;

                sendCommand( UpUnStall );

                return;
            }
        }

        if ( _linkTest )
        {
            // Don't busy wait for linkTest. If timeout is 0, then
            // run the loop once without pause.

            if ( timeout ) IOSleep( 10 );
            us_timeout -= 10 * 1000;
        }
        else
        {
            IODelay( 50 );
            us_timeout -= 50;
        }
    }
    while ( us_timeout > 0 );
}

//---------------------------------------------------------------------------
// sendPacket
//
// Send a packet in polled mode. This method returns after the packet
// has been downloaded by the DMA engine. The hardware is polled, the
// download complete interrupt is not used.

#define kSendPacketDelayLoops  100000
#define kSendPacketSleepLoops  100

void Apple3Com3C90x::sendPacket( void * pkt, UInt32 pkt_len )
{
    int            i;
    TxDescriptor * txDesc;
    const int      maxLoops = (_linkTest) ? kSendPacketSleepLoops :
                                            kSendPacketDelayLoops;

    if (pkt_len > kIOEthernetMaxPacketSize)
        return;

    if (!_linkTest && (_driverEnableCount == 0))
        return;

    // Recycle used descriptors when ring is full.

    if (_txRingFree == 0)
    {
        UInt32 listPtrReg = getDnListPtr();

        while (_txRingFree < _txRingSize)
        {
            if (listPtrReg == _txRingTail->drvPhysAddr)
                break;

            if (_txRingTail->drvMbuf)
            {
                _kdpPacketQueue->enqueue(_txRingTail->drvMbuf);
                _txRingTail->drvMbuf = 0;
            }

            _txRingTail = _txRingTail->drvNext;
            _txRingFree++;
        }

        if (_txRingFree == 0)
            return;
    }

    // Wait for NIC download engine idle.

    for (i = 0; (i < maxLoops) && getDnListPtr(); i++)
    {
        if (_linkTest) IOSleep( 10 );
        else           IODelay( 10 );
    }
    if (i >= maxLoops)
    {
        kprintf("sendPacket: idle poll timed out\n");
        return;
    }

    // Update watchdog.

    _newWDCounters[ kWDInterruptsRetired ] += 1;

    if (_txRingHead->drvMbuf != NULL)
    {
        LOG_DEBUG("sendPacket: mbuf not NULL\n");
    }

    // Copy from source KDP buffer to driver DMA buffer.

    txDesc = _txRingHead;
    bcopy(pkt, mbuf_data(_kdpMbuf), pkt_len);

    // Set total packet size in descriptor header field.

    OSWriteLittleInt32(&txDesc->header, 0,
                       SetBitField(TxFragment, Length, pkt_len));

    // Make fragment-0 point to DMA buffer.
    // Pre-computed PhysicalSegment is in little-endian format.

    txDesc->fragments[0].address = _kdpMbufSeg.location;
    txDesc->fragments[0].command =
        OSSwapHostToLittleInt32(kTxFragmentLastMask | pkt_len);

    txDesc->nextPtr = 0;  // Stop after processing this entry
    _txRingFree--;

    // Stall the transmit engine before linking descriptors.

    sendCommandWait( DnStall );

    // Link the current descriptor to the previous descriptor.

    OSWriteLittleInt32( &txDesc->drvPrevious->nextPtr,
                        0, txDesc->drvPhysAddr );

    // Update DnListPtr and point to descriptor with KDP data.

    setDnListPtr( txDesc->drvPhysAddr );

    // Unstall the transmit engine.

    sendCommand( DnUnStall );

    // Poll for send completion.

    for (i = 0; (i < maxLoops) && getDnListPtr(); i++)
    {
        if (_linkTest) IOSleep( 10 );
        else           IODelay( 10 );
    }
    if (i >= maxLoops)
    {
        kprintf("sendPacket: idle poll timed out\n");
    }
}
