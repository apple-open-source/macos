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
        for ( status = _rxRingTail->status;
              status & kRxDescStatusUpCompleteMask;
              status = _rxRingTail->status )
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
					bcopy( _rxRingTail->drvMbuf->m_data, pkt, *pkt_len );

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

void Apple3Com3C90x::sendPacket( void * pkt, UInt32 pkt_len )
{
#define kSendPacketDelayLoops  100000
#define kSendPacketSleepLoops  100

	int       i;
	const int maxLoops = (_linkTest) ? kSendPacketSleepLoops :
                                       kSendPacketDelayLoops;

	// Any of the following conditions would cause us to silently give up.

	if ( ( _txRingFree == 0 ) || ( pkt_len > MCLBYTES ) )
		return;

    if ( !_linkTest && ( _driverEnableCount == 0 ) )
		return;		/* can't transmit right now */

	// Poll until transmitter is idle.

    for ( i = 0; (i < maxLoops) && getDnListPtr(); i++ )
    {
		if (_linkTest) IOSleep( 10 );
		else           IODelay( 10 );
    }
	if ( i >= maxLoops )
    {
		IOLog("sendPacket: idle poll timed out\n");
		return;
    }

	// Update watchdog.

    _newWDCounters[ kWDInterruptsRetired ] += 1;

	if ( _txRingHead->drvMbuf != NULL )
    {
		LOG_DEBUG("sendPacket: mbuf not NULL\n");
    }

	// Attach the input buffer to the debugger mbuf.

	_kdpMbuf->m_next = 0;
    bcopy( pkt, mtod(_kdpMbuf, void *), pkt_len );
	_kdpMbuf->m_pkthdr.len = _kdpMbuf->m_len = pkt_len;

    // Better not coalesce!!!

	if ( updateTxDescriptor( _txRingHead, _kdpMbuf ) == false )
    {
        LOG_DEBUG("sendPacket: updateTxDescriptor error\n");
        return;
    }

	_txRingHead->nextPtr = 0;  // Stop after processing this entry

	// Stall the transmit engine.

	sendCommandWait( DnStall );

	// Link the current descriptor to the previous descriptor.

	_txRingHead->drvPrevious->nextPtr = _txRingHead->drvPhysAddr;
	
	// Update DnListPtr.

    setDnListPtr( _txRingHead->drvPhysAddr );

	// Unstall the transmit engine.

	sendCommand( DnUnStall );

	// Poll for transmit completion.

	for ( i = 0; (i < maxLoops) && getDnListPtr(); i++ )
    {
		if (_linkTest) IOSleep( 10 );
		else           IODelay( 10 );
    }
    if ( i >= maxLoops )
    {
		IOLog("sendPacket: idle poll timed out\n");
    }
}
