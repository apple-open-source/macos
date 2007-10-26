/*
 * Copyright (c) 1998-2003, 2006 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2001 Realtek Semiconductor Corp.  All rights reserved. 
 *
 * RTL8139Private.cpp
 *
 * HISTORY
 *
 * 09-Jul-01	Owen Wei at Realtek Semiconductor Corp. created for Realtek
 *		RTL8139 family NICs.
 */

#include "RTL8139.h"

	//---------------------------------------------------------------------------
	// Check the transmit status register. Inform the NIC to resend the aborted
	// packet. And change the status of device->send_table, device->ack_index,
	// device->send_count.

void RTL8139::transmitterInterrupt( bool *reclaimed )
{
	UInt32	txStatus;
	UInt8	collisions;
	UInt32	tcr;			// Transmit Control Register 0x40

    while ( fTxSendCount )
    {
        txStatus = csrRead32( RTL_TSD0 + fTxAckIndex * sizeof( UInt32 ) );

        if ( (txStatus & (R_TSD_TABT | R_TSD_TOK | R_TSD_TUN)) == 0 )
            break; /// ??? not owned by host (check OWN bit instead?)

		ELG( fTxAckIndex, txStatus, ' Tx+', "RTL8139::transmitterInterrupt" );
        collisions = (txStatus & R_TSD_NCC) >> 24;

        if ( txStatus & R_TSD_TABT ) // transmit abort
        {
			ALRT( fTxAckIndex, txStatus, 'TxA-', "RTL8139::transmitterInterrupt - transmit abort" );
            BUMP_NET_COUNTER( outputErrors );

            tcr = csrRead32( RTL_TCR );
            tcr |= R_TCR_CLRABT;		// resend packet 
            csrWrite32( RTL_TCR, tcr );

				// Record reason for the transmit abort.

            if ( collisions == 0xF )	BUMP_ETHER_COUNTER( excessiveCollisions );
            if ( txStatus & R_TSD_OWC )	BUMP_ETHER_COUNTER( lateCollisions );
            if ( txStatus & R_TSD_CRS )	BUMP_ETHER_COUNTER( carrierSenseErrors );
            break;  // wait for retransmission
        }

			// The chip will retry if the FIFO was exhausted during the
			// transmission of a packet. The R_TSD_TUN bit will be set.

        if ( txStatus & R_TSD_TUN )
        {
			ALRT( fTxAckIndex, txStatus, 'TxU-', "RTL8139::transmitterInterrupt - transmit underrun" );
            BUMP_ETHER_TX_COUNTER( underruns );
        }

        if ( collisions )
        {
			ALRT( fTxAckIndex, txStatus, 'TxC-', "RTL8139::transmitterInterrupt - transmit collisions" );
            netStats->collisions += collisions;
            if ( collisions > 1 )
                 BUMP_ETHER_COUNTER( multipleCollisionFrames );
            else BUMP_ETHER_COUNTER( singleCollisionFrames );
        }

        fTxSendCount--;
        fTxBufOwnership[ fTxAckIndex ] = kOwnedByHost;

        if ( ++fTxAckIndex >= kTxBufferCount )
			 fTxAckIndex = 0;

        *reclaimed = true;
    }/* end WHILE */
    
    BUMP_ETHER_TX_COUNTER( interrupts );
	return;
}/* end transmitterInterrupt */

	//---------------------------------------------------------------------------

void RTL8139::receiverInterrupt( bool *queued )
{
	rbuf_hdr_t    rbh;		// 4 byte Rx packet header
	mbuf_t        pkt;
	UInt16        pktLen;
	UInt32		totalBytes = 0;

//	ELG( fRxOffset, OSReadLittleInt32( fpRxBuffer, fRxOffset ), 'Rx I', "RTL8139::receiverInterrupt" );
    while ( (csrRead8( RTL_CM ) & R_CM_BUFE) == 0 )
    {
		if ( totalBytes > 0x10000 )		// protect against flood of small packets.
		{
			ELG( 0, totalBytes, 'RxLm', "RTL8139::receiverInterrupt - reached loop limit." );
			break;
		}

        *(UInt32*)&rbh = OSReadLittleInt32( fpRxBuffer, fRxOffset );
        pktLen = rbh.rb_count - 4;  // get Rx'd frame length deducting 4 FCS bytes
		fRxOffset += sizeof( rbh );
		totalBytes += pktLen;

			// Update receiver error counters:

        if ( rbh.rb_status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG | R_RSR_RUNT | R_RSR_ISE) )
        {
			ALRT( fRxOffset, *(UInt32*)&rbh, ' Rx-', "RTL8139::receiverInterrupt" );
            BUMP_NET_COUNTER( inputErrors );

            if ( rbh.rb_status & R_RSR_FAE )	BUMP_ETHER_COUNTER( alignmentErrors );
            if ( rbh.rb_status & R_RSR_CRC )	BUMP_ETHER_COUNTER( fcsErrors );
            if ( rbh.rb_status & R_RSR_LONG )	BUMP_ETHER_COUNTER( frameTooLongs );
            if ( rbh.rb_status & R_RSR_RUNT )	BUMP_ETHER_RX_COUNTER( frameTooShorts );
            if ( rbh.rb_status & R_RSR_ISE )	BUMP_ETHER_RX_COUNTER( phyErrors );

				// Will the assertion of any error bits clear the
				// R_RSR_ROK bit? We make it so.

            rbh.rb_status &= ~R_RSR_ROK;
        }

        if ( (rbh.rb_status & R_RSR_ROK) == 0 )
        {
			ALRT( fRxOffset, *(UInt32*)&rbh, 'Rok-', "RTL8139::receiverInterrupt - not OK" );
				// Restart the receiver when an error is encountered.
            restartReceiver();
            BUMP_ETHER_RX_COUNTER( resets );
            DEBUG_LOG( "%s: restarted receiver\n", getName() );
            break;
        }

        if ( (pktLen < MINPACK - 4) || (pktLen > MAXPACK - 4) )
        {
				// Invalid Rx'd frame length, skip it:

			ALRT( fRxOffset, *(UInt32*)&rbh, 'RxL-', "RTL8139::receiverInterrupt - length bad" );
            DEBUG_LOG( "%s: bad packet length (%d)\n", getName(), pktLen );
            BUMP_NET_COUNTER( inputErrors );
            if ( pktLen < MINPACK )
                 BUMP_ETHER_RX_COUNTER( frameTooShorts );
            else BUMP_ETHER_COUNTER(    frameTooLongs  );
        }
        else		/*** Good frame, pass it up:	***/
        {
            pkt = allocatePacket( pktLen );
            if ( !pkt )
            {
				ALRT( 0, pktLen, 'RxP-', "RTL8139::receiverInterrupt - packet not allocated" );
                BUMP_ETHER_RX_COUNTER( resourceErrors );
            }
            else
            {
				ELG( fRxOffset, pktLen, 'RxP+', "RTL8139::receiverInterrupt - goodness" );
                assert ( netif );

				if ( fRxOffset < RX_BUF_SIZE - pktLen )
				{
					bcopy( fpRxBuffer + fRxOffset, mbuf_data( pkt ), pktLen );
				}
				else
				{
					ELG( fRxOffset, pktLen, 'Wrap', "RTL8139::receiverInterrupt - Rx buffer wrapped" );
					bcopy( fpRxBuffer + fRxOffset, mbuf_data( pkt ), RX_BUF_SIZE - fRxOffset );
					bcopy( fpRxBuffer, (UInt8*)(mbuf_data( pkt )) + (RX_BUF_SIZE - fRxOffset), pktLen - (RX_BUF_SIZE - fRxOffset) );
				}

                netif->inputPacket( pkt, pktLen, IONetworkInterface:: kInputOptionQueuePacket );
                BUMP_NET_COUNTER( inputPackets );
                *queued = true;
            }
        }/* end if/ELSE good frame */

			// Advance Rx offset to start of the next packet:

        fRxOffset += IORound( pktLen + 4, 4 );			// account for 4 FCS bytes
	///	csrWrite16Slow( RTL_CAPR, fRxOffset - 0x10 );	// leave a gap
        csrWrite16( RTL_CAPR, fRxOffset - 0x10 );		// leave a gap
    }/* end WHILE buffer not empty */

    BUMP_ETHER_RX_COUNTER( interrupts );
	return;
}/* end receiverInterrupt */

	//---------------------------------------------------------------------------

void RTL8139::restartReceiver()
{
    UInt8	tmp_cm;

	tmp_cm = csrRead8( RTL_CM );
	ELG( 0000, tmp_cm, 'ReRx', "RTL8139::restartReceiver" );

    tmp_cm &= ~R_CM_RE;
    csrWrite8( RTL_CM, tmp_cm );
    IOSleep( 10 );

		/* any timing specifications on stopping the engine? */

    fRxOffset = 0;

    csrWrite32( RTL_RBSTART, fRxBufferPhys );

    tmp_cm |= R_CM_RE;
    csrWrite8( RTL_CM, tmp_cm );

	csrWrite32( RTL_RCR, reg_rcr );
	return;
}/* end restartReceiver */


void RTL8139::enableHardwareInterrupts()
{
	UInt16	imr;	// always use the Tx interrupt

	ELG( 0, fSpeed100, 'eHWI', "RTL8139::enableHardwareInterrupts" );

	if ( fSpeed100 )
	{		// Use Tx interrupts; poll for Rx frames on the Realtek timer:
		csrWrite32( RTL_TIMER_INT, 32768 );	// set about 1 ms in the timer value
		csrWrite32( RTL_TCTR, 0 );			// start the timer
		imr = R_ISR_TMOUT | R_ISR_TOK | R_ISR_TER;
	}
	else	// 10 Mbps:
	{
		imr = R_ISR_ALL;
		csrWrite32( RTL_TIMER_INT, 0 );	// turn off the timer
	}

	csrWrite16( RTL_IMR, imr );
    interruptEnabled = true;
	return;
}/* end enableHardwareInterrupts */


void RTL8139::disableHardwareInterrupts()
{
	ELG( 0, 0, 'dHWI', "RTL8139::disableHardwareInterrupts" );
    csrWrite16( RTL_IMR, R_ISR_NONE ); 
    interruptEnabled = false;
	return;
}/* end disableHardwareInterrupts */


bool RTL8139::allocateDescriptorMemory()
{
    IOByteCount		len;
	int				size;

		// Allocate Tx and Rx memory:

	size = (TX_BUF_SIZE * kTxBufferCount) + (RX_BUF_SIZE + PAGE_SIZE);

	ELG( 0, size, 'AlDM', "RTL8139::allocateDescriptorMemory" );

    fpTxRxMD = IOBufferMemoryDescriptor::withOptions( 
								/* options   */	kIOMemoryPhysicallyContiguous,
								/* capacity  */	size,
								/* alignment */	PAGE_SIZE );

    if ( !fpTxRxMD || fpTxRxMD->prepare() != kIOReturnSuccess )
    {
        IOLog("%s: Can't allocate %d contiguous bytes\n", getName(), size ); 
        return false;
    }
    fpTxRxBuffers	= (UInt8*)fpTxRxMD->getBytesNoCopy();
    fTxRxPhysical	= fpTxRxMD->getPhysicalSegment( 0, &len );

	ELG( fTxRxPhysical, fpTxRxBuffers, 'TxRx', "allocateDescriptorMemory - Tx and Rx buffers" );

		// Put the Rx buffer after the 4 Tx buffers:

    fpRxBuffer		= fpTxRxBuffers   + TX_BUF_SIZE * kTxBufferCount;
    fRxBufferPhys	= fTxRxPhysical   + TX_BUF_SIZE * kTxBufferCount;

    return true;
}/* end allocateDescriptorMemory */

	//---------------------------------------------------------------------------

bool RTL8139::initAdapter( IOOptionBits options )
{
	ELG( 0, options, 'i Ad', "RTL8139::initAdapter" );
    DEBUG_LOG( "initAdapter() ===>\n");

///	disableHardwareInterrupts();

    csrWrite8( RTL_CM, R_CM_RST );	// Issue a software reset.

///	disableHardwareInterrupts();
    IOSleep(10);

    if ( csrRead8( RTL_CM ) & R_CM_RST )
    {
			// FIXME: need more robust recovery (retry?)
        IOLog( "%s: chip reset timed out\n", getName() );
        return false;
    }

		// If all that's needed is a chip reset, then we're done.

    if ( options & kResetChip )
		return true;

		// Clear the multicast hash:

    reg_mar0 = reg_mar4 = 0;
    csrWrite32( RTL_MAR0, reg_mar0 );
    csrWrite32( RTL_MAR4, reg_mar4 );

		// Save config1 register (not used).

    reg_config1 = csrRead8( RTL_CONFIG1 );	

		// Update the physical address of the Rx buffer:

    csrWrite32( RTL_RBSTART, fRxBufferPhys );
    fRxOffset = 0;

		// Update the physical address of the Tx buffers:

    for ( int i = 0; i < kTxBufferCount; i++ )
    {
        csrWrite32( RTL_TSAD0 + i * sizeof( UInt32 ), fTxRxPhysical + i * TX_BUF_SIZE );
		fTxBufOwnership[i] = kOwnedByHost;
    }
	fTxSendIndex = fTxSendCount = fTxAckIndex = 0;

		// Enable transmitter and receiver. Seems odd to do this before
		// configuring the transmitter and receiver, but this is how it
		// was in the Realtek source.

    csrWrite8( RTL_CM, R_CM_RE | R_CM_TE );

		// TCR - transmit configuration register.

    csrWrite32( RTL_TCR, R_TCR_MXDMA | R_TCR_IFG );

		// RCR - receive configuration register. Save the value to be used
		// later if the receiver is restarted, or multicast/promiscuous mode
		// changes.

    reg_rcr = R_RCR_RBLEN_64K | R_RCR_AB    | R_RCR_APM   | R_RCR_AM
							  | R_RCR_ERTH  | R_RCR_RXFTH | R_RCR_MXDMA;
    csrWrite32( RTL_RCR, reg_rcr );

    DEBUG_LOG( "initAdapter() <===\n");

    return true;
}/* end initAdapter */

	/* interruptOccurred is called by real Tx interrupts and	*/
	/* by invocations from timeoutOccurred for Rx.				*/

void RTL8139::interruptOccurred( IOInterruptEventSource *src, int count )
{
	bool				flushInputQ    = false;
	bool				serviceOutputQ = false;
	UInt16				isr;
	IODebuggerLockState lockState;

		// PCI drivers must be prepared to handle spurious interrupts when the
		// interrupt line is shared with other devices. The interruptEnabled
		// flag prevents the driver from touching hardware before it is ready. 

    if ( !interruptEnabled )
	{
		ELG( 0, 0, 'Int-', "RTL8139::interruptOccurred - not ready" );
		return;
	}

    lockState = IODebuggerLock( this );		// Keep KDP out

	isr = csrRead16( RTL_ISR );
	ELG( 0, isr, 'Int+', "RTL8139::interruptOccurred" );

	if ( isr )
	{
		if ( isr & R_ISR_TMOUT )
			csrWrite32( RTL_TCTR, 0 );	// reset the timer.

		if ( isr &  (R_ISR_FOVW | R_ISR_RXOVW) )	// If either FIFO overflow or Buffer overflow,
			 isr |= (R_ISR_FOVW | R_ISR_RXOVW);		// ack both. /// mlj - I don't know why.

		csrWrite16( RTL_ISR, isr );		// Acknowledge pending interrupt sources

		if ( isr & (R_ISR_TOK | R_ISR_TER) )
		{
			BUMP_ETHER_TX_COUNTER( interrupts );
			transmitterInterrupt( &serviceOutputQ );
		}
	}

	receiverInterrupt( &flushInputQ );	// Always check for Rx frames.

#if NOT_YET
		// Packet underrun or link change interrupt:

	if ( isr & R_ISR_PUN )
	{
		UInt16 tmp_rtl74 = csrRead16( RTL_74 );
		if ( tmp_rtl74 & SIZE16_BIT11 ) // link change
		{
			if ( tmp_rtl74 & SIZE16_BIT10 ) // link ok
			{
			}
			else // link down
			{
			}
		}
		else
		{
		}
	}
#endif

		// If debugger reclaimed the Tx buffer, then we will still expect a
		// hardware interrupt after returning from the debugger, but the Tx
		// interrupt handler will not reclaim any buffer space. If the output
		// queue was previously stalled, then that could spell trouble. To
		// prevent this, service the output queue when this condition exists.

    if ( fTxBufReclaimed )
    {
        serviceOutputQ	= true;
        fTxBufReclaimed	= false;
    }

    IODebuggerUnlock( lockState );

		// Flush all inbound packets and pass them to the network stack.
		// Interrupts are not enabled until the network interface is enabled
		// by BSD, so netif must be valid.

    assert( netif );
    if ( flushInputQ )
	{
		BUMP_ETHER_RX_COUNTER( interrupts );
		netif->flushInputQueue();
	}

		// Call service() without holding the debugger lock to prevent a
		// deadlock when service() calls our outputPacket() function.

    if ( serviceOutputQ )
		fTransmitQueue->service();
	return;
}/* end interruptOccurred */

	//---------------------------------------------------------------------------
	// Note: This function must not block. Otherwise it may expose
	// re-entrancy issues in the BSD networking stack.
	// Note also that this method is usually called on the clien'ts thread not
	// the workloop.

UInt32 RTL8139::outputPacket( mbuf_t pkt, void *param )
{
    mbuf_t              mn;
    long		        pktLen;
	UInt32				tsd;	// Tx Status of Descriptor
    UInt8				*pDest;
    IODebuggerLockState lockState;

    DEBUG_LOG( "outputPacket() ===>\n" );

    lockState = IODebuggerLock( this );

	tsd = csrRead32( RTL_TSD0 + fTxSendIndex * sizeof( UInt32 ) );

    if ( kOwnedByChip == fTxBufOwnership[ fTxSendIndex ] )
    {
		ELG( fTxSendIndex, tsd, 'Stal', "RTL8139::outputPacket - Tx stall" );
			// Stall the output queue until the ack by the interrupt handler.
        DEBUG_LOG( "outputPacket() <===\n" );
        IODebuggerUnlock( lockState );
        return kIOReturnOutputStall;
    }

	ELG( fTxSendIndex, mbuf_pkthdr_len( pkt ), 'outP', "RTL8139::outputPacket" );

///	if ( tsd & (R_TSD_CRS | R_TSD_TABT | R_TSD_OWC | R_TSD_CDH | R_TSD_TUN) )
	if ( tsd & (R_TSD_CRS | R_TSD_TABT | R_TSD_OWC			   | R_TSD_TUN) )
	{
		BUMP_NET_COUNTER( outputErrors );
		ALRT( fTxSendIndex, tsd, ' Tx-', "RTL8139::outputPacket - Tx Error" );
	}

    pDest = fpTxRxBuffers + TX_BUF_SIZE * fTxSendIndex;

    pktLen = mbuf_pkthdr_len( pkt );

    for ( mn = pkt; mn; mn = mbuf_next( mn ) )
    {
        bcopy( mbuf_data( mn ), pDest, mbuf_len( mn ) );
        pDest += mbuf_len( mn );
    }

		// Software padding of small frames. Hardware doesn't do this.

    if ( pktLen < MINPACK - 4 )				// account for 4 FCS bytes
    {
        memset( pDest, 0x55, MINPACK - 4 - pktLen );
        pktLen = MINPACK - 4;
    }
		// Start the Tx by setting the frame length and clearing ownership:

    csrWrite32( RTL_TSD0 + fTxSendIndex * sizeof( UInt32 ), pktLen | fTSD_ERTXTH );

	fTxSendCount++;
	fTxBufOwnership[ fTxSendIndex ] = kOwnedByChip;

    if ( ++fTxSendIndex >= kTxBufferCount )
		fTxSendIndex = 0;

	IODebuggerUnlock( lockState );

    freePacket( pkt );

    BUMP_NET_COUNTER( outputPackets );

	DEBUG_LOG( "outputPacket() <===\n" );

    return kIOReturnOutputSuccess;
}/* end outputPacket */


	//---------------------------------------------------------------------------
	// Send KDP packets in polled mode.

void RTL8139::sendPacket( void *pkt, UInt32 pktLen )	// KDP
{
	UInt8	*pDest;

    if ( pktLen > MAXPACK - 4 )	// account for 4 FCS bytes
		return;

		// Poll until a transmit buffer becomes available.

	while ( kOwnedByChip == fTxBufOwnership[ fTxSendIndex ] )
        reclaimTransmitBuffer();

    pDest = fpTxRxBuffers + fTxSendIndex * TX_BUF_SIZE;

    bcopy( pkt, pDest, pktLen );	// Copy debugger packet to the Tx buffer

		// Pad small frames:

    if ( pktLen < MINPACK - 4 )		// account for 4 FCS bytes
    {
        memset( pDest + pktLen, 0x55, MINPACK - 4 - pktLen );
        pktLen = MINPACK - 4;
    }
		// Start the Tx by setting the frame length and clearing ownership:

    csrWrite32( RTL_TSD0 + fTxSendIndex * sizeof( UInt32 ), pktLen | fTSD_ERTXTH );

	fTxSendCount++;
	fTxBufOwnership[ fTxSendIndex ] = kOwnedByChip;

    if ( ++fTxSendIndex >= kTxBufferCount )
		fTxSendIndex = 0;

//	kprintf( "RTL8139: sendPacket len %d\n", pktLen );
	return;
}/* end sendPacket KDP */


void RTL8139::reclaimTransmitBuffer()	// KDP only
{
    UInt32  txStatus;
    UInt32  u32;

    fTxBufReclaimed = true;

	while ( fTxSendCount )
    {
        txStatus = csrRead32( RTL_TSD0 + fTxAckIndex * sizeof( UInt32 ) );

        if ( (txStatus & (R_TSD_TABT | R_TSD_TOK | R_TSD_TUN)) == 0 )
            break;  // not owned by host (check OWN bit instead?)

        if ( txStatus & R_TSD_TABT )		// Tx abort
        {
			ALRT( fTxAckIndex, txStatus, 'TxA-', "RTL8139::reclaimTransmitBuffer - abort" );
            u32 = csrRead32( RTL_TCR );
            u32 |= R_TCR_CLRABT;			// resend packet 
            csrWrite32( RTL_TCR, u32 );
            break;							// wait for retransmission
        }

        fTxSendCount--;
        fTxBufOwnership[ fTxAckIndex ] = kOwnedByHost;

        if ( ++fTxAckIndex >= kTxBufferCount )
			fTxAckIndex = 0;
    }/* end WHILE */

	return;
}/* end reclaimTransmitBuffer */


	//---------------------------------------------------------------------------
	// Receive (KDP) packets in polled mode. This is essentially a simplified
	// version of the receiverInterrupt() function.

void RTL8139::receivePacket( void *pkt, UInt32 *pktLen, UInt32 timeout )	// KDP
{
	rbuf_hdr_t  rbh;     // 4 byte Rx packet header
	UInt16	    rxLen;

    *pktLen = 0;
    timeout *= 1000;  // convert ms to us

    while ( timeout && *pktLen == 0 )
    {
        if ( (csrRead8( RTL_CM ) & R_CM_BUFE) == R_CM_BUFE )
        {		// Receive buffer empty, wait and retry.
            IODelay( 50 );
            timeout -= 50;
            continue;
        }

        *(UInt32*)&rbh	= OSReadLittleInt32( fpRxBuffer, fRxOffset );
        rxLen			= rbh.rb_count;		// includes 4 bytes of FCS
		fRxOffset	   += sizeof( rbh );

	//	kprintf( "RTL8139::receivePacket status %x len %d\n", rbh.rb_status, rxLen );

        if ( rbh.rb_status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG | R_RSR_RUNT | R_RSR_ISE) )
             rbh.rb_status &= ~R_RSR_ROK;

        if ( (rbh.rb_status & R_RSR_ROK) == 0 )
        {
            restartReceiver();
            continue;
        }

        if ( rxLen >= MINPACK && rxLen <= MAXPACK ) 
        {
            bcopy( fpRxBuffer + fRxOffset, pkt, rxLen );
            *pktLen = rxLen;
        }

			// Advance the Rx ring buffer to the start of the next packet:

        fRxOffset += IORound( rxLen, 4 );
        csrWrite16( RTL_CAPR, fRxOffset - 0x10 );	// leave a small gap
    }/* end WHILE */
	return;
}/* end receivePacket KDP */

#pragma mark -
#pragma mark еее Put EEPROM contents in IORegistry еее
#pragma mark -


	/*  register 0x50 RTL_9346CR bits: */

#define RTL_9346CR_PROGRAM	0x80

#define RTL_9346CR_EECS		0x08	/* EEPROM Chip Select							*/
#define RTL_9346CR_EESK		0x04	/* EEPROM Shift Clock							*/
#define RTL_9346CR_EEDI		0x02	/* EEPROM Data In  from EEPROM's perspective	*/
#define RTL_9346CR_EEDO		0x01	/* EEPROM Data Out from EEPROM's perspective	*/

	/* These EEPROM commands include the Start bit 0x4 and	*/
	/* are shifted left 6 for the address field:			*/

#define OP_9346_EWDS	0x100	// Erase/Write Disable
#define OP_9346_EWEN	0x130	// Erase/Write ENable
#define OP_9346_WRITE	0x140
#define OP_9346_READ	0x180
#define OP_9346_ERASE	0x1C0

#define kEEPROM_SIZE	0X80


void RTL8139::registerEEPROM()
{
	OSArray			*arrayStrings;
	OSString		*tmpString;
	char			printBuffer[ 100 ];
	UInt16			contents[ kEEPROM_SIZE / sizeof( UInt16 ) ];
	UInt16			i, x, checksum;

	ELG( 0, 0, 'PROM', "RTL8139::registerEEPROM" );
#if USE_ELG
	UInt32	flag = fpELG->evLogFlag;	// Temporarily disable event logging
	fpELG->evLogFlag = 0;				// because bit bashing floods it.
#endif // USE_ELG

	checksum = 0;

	for ( i = 0; i < kEEPROM_SIZE / sizeof( UInt16 ); ++i )	// For each Word
	{
		x = inEEPROM16( i );
		checksum += x;
		contents[ i ] = x;
	}/* end For each Word */

//	if ( checksum != 0 )
	//	IOLog( "RTL8139::fixEnetFlowControl *** Checksum is incorrect. %04x, %04x\n", contents[ 0x32 / 2 ], checksum );

	if ( (contents[ 6 ] & 0x0002) != 0 )	// Check Flow Control bit
		IOLog( "RTL8139::fixEnetFlowControl - Flow Control is disabled\n" );

		/* put the contents in the IORegistry:	*/

	arrayStrings = OSArray::withCapacity( kEEPROM_SIZE / 0x10 );	// 8 rows of data

	for ( i = 0 ; i < (kEEPROM_SIZE / sizeof( UInt16 )); i += 8 )	// 16 bytes per row
	{
		sprintf( printBuffer, "%04x %04x %04x %04x   %04x %04x %04x %04x",
			contents[ i+0 ], contents[i+1], contents[i+2], contents[i+3],
			contents[ i+4 ], contents[i+5], contents[i+6], contents[i+7] );
				///	IOLog( "RTL8139::registerEEPROM: %s\n", printBuffer );
		tmpString = OSString::withCString( printBuffer );
		arrayStrings->setObject( tmpString );
		tmpString->release();
	}

	setProperty( "EEPROM words", arrayStrings );
	arrayStrings->release();

#ifdef CRAP
	contents[ 6 ] &= ~0x0002;					// Turn off the disable Flow Control bit
	contents[ 0x32 / 2 ] = 0;					// init the new checksum
	checksum = 0;

	for ( i = 0; i < kEEPROM_SIZE / 2; ++i )		// recalc the checksum
		checksum += contents[ i ];

	outEEPROM16( 6, contents[ 6 ] );				// do it.
	outEEPROM16( 0x32 / 2, 0x10000 - checksum );	// adjust the checksum
	
	IOLog( "RTL8139::fixEnetFlowControl - Ethernet Flow Control is now enabled.\n" );
#endif // CRAP

#if USE_ELG
	fpELG->evLogFlag = flag;			// Reenable event logging if it was on.
#endif // USE_ELG

	return;
}/* end fixEnetFlowControl */


void RTL8139::write9346CR( UInt8 value )
{
	csrWrite8( RTL_9346CR, value );
	IOSleep( 1 );

	return;
}/* end write9346CR */


#ifdef CRAP
void RTL8139::outEEPROM16( int location, UInt16 value )
{
	int			i;
	UInt16		outData16 = value;

	write9346CR( RTL_9346CR_PROGRAM );

	outCommand( OP_9346_EWEN );								// enable Erase/Write

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Clk Lo
	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// CS
	outCommand( OP_9346_ERASE | location );					// Erase the location

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Clk Lo
	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	outCommand( OP_9346_WRITE | location );					// start the Write

	for ( i = 16; i > 0; i-- )
	{
		outBit( outData16 );
		outData16 <<= 1;
	}

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Clk Lo
	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// CS
	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	outCommand( OP_9346_EWDS );								// disable Erase/Write

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Clk Lo
	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	return;
}/* end outEEPROM16 */
#endif // CRAP


int RTL8139::inEEPROM16( int location )
{
	int			i;
	UInt16		inData16 = 0;

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// CS Hi

	outCommand( OP_9346_READ | location );					// Issue the command

		/* Get 16 data bits:	*/

	for ( i = 16; i > 0; i-- )
	{
		inData16 <<= 1;
		inData16 |= inBit();
	}

	write9346CR( RTL_9346CR_PROGRAM );						// CS Lo

	return inData16;
}/* end inEEPROM16 */


void RTL8139::outCommand( UInt16 cmd )
{
	int		i;

	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Chip Select, Clk Lo

		// output the 10 bit command: 1 Zero, 1 Start, 2 OP, 6 Address:

	cmd <<= 6;
	for ( i = 10; i > 0; --i )
	{
		outBit( cmd );
		cmd <<= 1;
	}
	write9346CR( RTL_9346CR_PROGRAM | RTL_9346CR_EECS );	// Chip Select, Clk Lo
	return;
}/* end outCommand */


void RTL8139::outBit( UInt16 bit )
{
	UInt8	u8 = RTL_9346CR_PROGRAM | RTL_9346CR_EECS;

	u8 |= (bit & 0x8000) ? RTL_9346CR_EEDI : 0;

	write9346CR( u8 );						// toggle clock Lo
	write9346CR( u8 | RTL_9346CR_EESK );	// toggle clock Hi
	return;
}/* end outBit */


int RTL8139::inBit()
{
	UInt8	u8 = RTL_9346CR_PROGRAM | RTL_9346CR_EECS;

	write9346CR( u8 | RTL_9346CR_EESK );		// clock Hi
	write9346CR( u8 );							// clock Lo

	u8 = csrRead8( RTL_9346CR );

	return (u8 & RTL_9346CR_EEDO) ? 1 : 0;
}/* end inBit */
