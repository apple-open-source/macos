/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

void RTL8139::transmitterInterrupt( bool * reclaimed )
{
    UInt32  tx_status;
    UInt8   collisions;
    UInt32  tcr;

    while ( tx_send_count )
    {
        tx_status = csrRead32( RTL_TSD0 + tx_ack_index * 4 );

        if ( (tx_status & (R_TSD_TABT | R_TSD_TOK | R_TSD_TUN)) == 0 )
            break; // not owned by host (check OWN bit instead?)

        collisions = ( tx_status & R_TSD_NCC ) >> 24;

        if ( tx_status & R_TSD_TABT ) // transmit abort
        {
            BUMP_NET_COUNTER( outputErrors );

            tcr = csrRead32( RTL_TCR );
            tcr |= R_TCR_CLRABT;   // resend packet 
            csrWrite32( RTL_TCR, tcr );

            // Record reason for the transmit abort.

            if ( collisions == 0xF )
                BUMP_ETHER_COUNTER( excessiveCollisions );
            if ( tx_status & R_TSD_OWC )
                BUMP_ETHER_COUNTER( lateCollisions );
            if ( tx_status & R_TSD_CRS )
                BUMP_ETHER_COUNTER( carrierSenseErrors );
            break;  // wait for retransmission
        }

        // The chip will retry if the FIFO was exhausted during the
        // transmission of a packet. The TSD<TUN> bit will be set.

        if ( tx_status & R_TSD_TUN )
        {
            BUMP_ETHER_TX_COUNTER( underruns );
        }

        if ( collisions )
        {
            netStats->collisions += collisions;
            if ( collisions > 1 )
                BUMP_ETHER_COUNTER( multipleCollisionFrames );
            else
                BUMP_ETHER_COUNTER( singleCollisionFrames );
        }

        tx_send_count--;
        tx_buf_ownership[tx_ack_index] = kOwnedByHost;

        if ( 4 == ++tx_ack_index ) tx_ack_index = 0;
        
        *reclaimed = true;
    }
    
    BUMP_ETHER_TX_COUNTER( interrupts );
}

//---------------------------------------------------------------------------

void RTL8139::receiverInterrupt( bool * queued )
{
	rbuf_hdr_t    rbh;     // 4 byte rx packet packet
    UInt16        offset;
	UInt16        tmp_capr;
    mbuf_t        pkt;
	UInt16        pkt_len;

    while ( ( csrRead8( RTL_CM ) & R_CM_BUFE ) == 0 )
    {
        *((UInt32 *)&rbh) = OSReadLittleInt32( rx_buf_ptr, 0 );

        pkt_len = rbh.rb_count;  // get received frame length

        // Update receiver error counters.

        if ( rbh.rb_status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG |
                              R_RSR_RUNT | R_RSR_ISE) )
        {
            BUMP_NET_COUNTER( inputErrors );

            if ( rbh.rb_status & R_RSR_FAE )
                BUMP_ETHER_COUNTER( alignmentErrors );
            if ( rbh.rb_status & R_RSR_CRC )
                BUMP_ETHER_COUNTER( fcsErrors );
            if ( rbh.rb_status & R_RSR_LONG )
                BUMP_ETHER_COUNTER( frameTooLongs );
            if ( rbh.rb_status & R_RSR_RUNT )
                BUMP_ETHER_RX_COUNTER( frameTooShorts );
            if ( rbh.rb_status & R_RSR_ISE )
                BUMP_ETHER_RX_COUNTER( phyErrors );

            // Will the assertion of any error bits clear the
            // R_RSR_ROK bit? We make it so.

            rbh.rb_status &= ~R_RSR_ROK;
        }

        if ( (rbh.rb_status & R_RSR_ROK) == 0 )
        {
            // Restart the receiver when an error is encountered.
            restartReceiver();
            BUMP_ETHER_RX_COUNTER( resets );
            DEBUG_LOG("%s: restarted receiver\n", getName());
            break;
        }

        if ( (pkt_len < MINPACK + 4) || (pkt_len > MAXPACK + 4) ) 
        {
            // Invalid received frame length, skip it.

            DEBUG_LOG("%s: bad packet length (%d)\n", getName(), pkt_len);
            BUMP_NET_COUNTER( inputErrors );
            if ( pkt_len < MINPACK + 4 )
                BUMP_ETHER_RX_COUNTER( frameTooShorts );
            else
                BUMP_ETHER_COUNTER( frameTooLongs );
        }
        else
        {
            // Good frame, pass it up.

            pkt = allocatePacket( pkt_len );
            if ( 0 == pkt )
            {
                BUMP_ETHER_RX_COUNTER( resourceErrors );
            }
            else
            {
                assert ( netif );
                bcopy( rx_buf_ptr + sizeof(rbh), mbuf_data(pkt), pkt_len );
                netif->inputPacket( pkt, pkt_len, IONetworkInterface::
                                    kInputOptionQueuePacket );
                BUMP_NET_COUNTER( inputPackets );
                *queued = true;
            }
        }

        // Advance the receive ring buffer to the start of the next packet.

        offset = IORound(pkt_len, 4) + sizeof(rbh);
        rx_buf_ptr += offset;

        if (rx_buf_ptr >= (rx_buf_end - 1))
            rx_buf_ptr -= RX_BUF_SIZE;

        tmp_capr = csrRead16( RTL_CAPR );
        tmp_capr += offset;
        csrWrite16Slow( RTL_CAPR, tmp_capr );        
    }

    BUMP_ETHER_RX_COUNTER( interrupts );
}

//---------------------------------------------------------------------------

void RTL8139::restartReceiver( void )
{
    UInt8 tmp_cm = csrRead8( RTL_CM );

    tmp_cm &= ~R_CM_RE;
    csrWrite8( RTL_CM, tmp_cm );
    IOSleep(10);

    /* any timing specifications on stopping the engine? */

    rx_buf_ptr = rx_buf_start;

    csrWrite32( RTL_RBSTART, rx_buf_phys );

    tmp_cm |= R_CM_RE;
    csrWrite8( RTL_CM, tmp_cm );

	csrWrite32( RTL_RCR, reg_rcr );
}

//---------------------------------------------------------------------------

void RTL8139::enableHardwareInterrupts( void )
{
    csrWrite16( RTL_IMR, R_ISR_ALL );
    interruptEnabled = true;
}

void RTL8139::disableHardwareInterrupts( void )
{
    csrWrite16( RTL_IMR, R_ISR_NONE ); 
    interruptEnabled = false;
}

//---------------------------------------------------------------------------

bool RTL8139::allocateDescriptorMemory( void )
{
    IOByteCount len;

    // Allocate receiver memory.

    rx_md = IOBufferMemoryDescriptor::withOptions( 
                      /* options   */ kIOMemoryPhysicallyContiguous,
                      /* capacity  */ RX_BUF_SIZE + (4 * 1024),
                      /* alignment */ PAGE_SIZE );

    if ( 0 == rx_md || rx_md->prepare() != kIOReturnSuccess )
    {
        IOLog("%s: Can't allocate %d contiguous bytes\n",
              getName(), RX_BUF_SIZE + (4 * 1024));
        return false;
    }

    rx_buf_start = (UInt8 *) rx_md->getBytesNoCopy();
    rx_buf_end   = rx_buf_start + RX_BUF_SIZE;
    rx_buf_phys  = rx_md->getPhysicalSegment( 0, &len );

    DEBUG_LOG("Rx Buffer len = %d virt = %p phys = 0x%lx\n",
              rx_md->getCapacity(), rx_buf_start, rx_buf_phys);

    // Allocate transmitter memory.

    for ( int i = 0; i < kTxBufferCount; i++ )
    {
        tx_md[i] = IOBufferMemoryDescriptor::withOptions(
                             /* options   */ 0,
                             /* capacity  */ PAGE_SIZE,
                             /* alignment */ PAGE_SIZE );

        if ( 0 == tx_md[i] || tx_md[i]->prepare() != kIOReturnSuccess )
        {
            IOLog("%s: Can't allocate %dth Tx buffer\n", getName(), i);
            return false;
        }

        tx_buf_ptr[i]  = (UInt8 *) tx_md[i]->getBytesNoCopy();
        tx_buf_phys[i] = tx_md[i]->getPhysicalSegment( 0, &len );
        
        DEBUG_LOG("Tx Buffer %d len = %d virt = %p phys = 0x%lx\n",
                  i, PAGE_SIZE, tx_buf_ptr[i], tx_buf_phys[i]);
    }
    
    return true;
}

//---------------------------------------------------------------------------

bool RTL8139::initAdapter( IOOptionBits options )
{
    DEBUG_LOG("initAdapter() ===>\n");

    disableHardwareInterrupts();

    // Issue a software reset.

    csrWrite8( RTL_CM, R_CM_RST );

    disableHardwareInterrupts();
    IOSleep(10);

    if ( csrRead8( RTL_CM ) & R_CM_RST )
    {
        // FIXME: need more robust recovery (retry?)
        IOLog("%s: chip reset timed out\n", getName());
        return false;
    }

    // If all thats needed is a chip reset, then we're done.

    if ( options & kResetChip ) return true;

    // Clear the multicast hash.

    reg_mar0 = reg_mar4 = 0;
    csrWrite32( RTL_MAR0, reg_mar0 );
    csrWrite32( RTL_MAR4, reg_mar4 );

    // Save config1 register (not used).

    reg_config1 = csrRead8( RTL_CONFIG1 );	

    // Update the physical address of the receiver buffer.

    rx_buf_ptr = rx_buf_start;
    rx_buf_ptr[0x1001] = 0x9f; // why? for debugging?

    csrWrite32( RTL_RBSTART, rx_buf_phys );

    // Update the physical address of the transmit buffers.

    for ( int i = 0; i < kTxBufferCount; i++ )
    {
        csrWrite32( RTL_TSAD0 + i * 4, tx_buf_phys[i] );
        tx_buf_ownership[i] = kOwnedByHost;
    }

    tx_send_index = 0;
    tx_send_count = 0;
    tx_ack_index  = 0;

    // Enable transmitter and receiver. Seems odd to do this before
    // configuring the transmitter and receiver, but this is how it
    // was in the Realtek source.

    csrWrite8( RTL_CM, R_CM_RE | R_CM_TE );

    // TCR - transmit configuration register.

    csrWrite32( RTL_TCR, R_TCR_MXDMA | R_TCR_IFG );

    // RCR - receive configuration register. Save the value to be used
    // later if the receiver is restarted, or multicast/promiscuous mode
    // changes.

    reg_rcr = R_RCR_RBLEN_16K | R_RCR_AB | R_RCR_APM | R_RCR_AM |
              R_RCR_ERTH + R_RCR_RXFTH + R_RCR_MXDMA + R_RCR_WRAP;

    csrWrite32( RTL_RCR, reg_rcr );

    DEBUG_LOG("initAdapter() <===\n");

    return true;
}

//---------------------------------------------------------------------------

void RTL8139::interruptOccurred( IOInterruptEventSource * src, int count )
{
    bool    flushInputQ    = false;
    bool	serviceOutputQ = false;
    UInt16  isr;
    IODebuggerLockState lockState;

    // PCI drivers must be prepared to handle spurious interrupts when the
    // interrupt line is shared with other devices. The interruptEnabled
    // flag prevents the driver from touching hardware before it is ready. 

    if ( false == interruptEnabled ) return;

    lockState = IODebuggerLock( this );

    while ( 1 )
    {
        isr = csrRead16( RTL_ISR );
        if ( ( isr & R_ISR_ALL ) == 0 )
        {
            // May want to consider a filter interrupt source when
            // sharing interrupts to avoid scheduling the work loop.
            break;  // exit interrupt loop...
        }

        if ( isr & ( R_ISR_FOVW | R_ISR_RXOVW) )
            isr |= ( R_ISR_FOVW | R_ISR_RXOVW);

        // Acknowledge pending interrupt sources.

        csrWrite16( RTL_ISR, isr );

        // Transmitter OK, or error due to excessive collisions.

        if ( isr & (R_ISR_TOK | R_ISR_TER) )
        {
            transmitterInterrupt( &serviceOutputQ );
        }

#if NOT_YET
        // Packet underrun or link change interrupt.
    
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

        if ( isr & (R_ISR_ROK | R_ISR_RER | R_ISR_RXOVW | R_ISR_FOVW) ) 
        {
            receiverInterrupt( &flushInputQ );
        }
    }

    // If debugger reclaimed the tx buffer, then we will still expect a
    // hardware interrupt after returning from the debugger, but the tx
    // interrupt handler will not reclaim any buffer space. If the output
    // queue was previously stalled, then that could spell trouble. To
    // prevent this, service the output queue when this condition exists.

    if ( tx_buf_reclaimed )
    {
        serviceOutputQ = true;
        tx_buf_reclaimed = false;
    }

    IODebuggerUnlock( lockState );

    // Flush all inbound packets and pass them to the network stack.
    // Interrupts are not enabled until the network interface is enabled
    // by BSD, so netif must be valid.

    assert( netif );
    if ( flushInputQ ) netif->flushInputQueue();

    // Call service() without holding the debugger lock to prevent a
    // deadlock when service() calls our outputPacket() function.

    if ( serviceOutputQ ) transmitQueue->service();
}

//---------------------------------------------------------------------------
// Note: This function must not block. Otherwise it may expose
// re-entrancy issues in the BSD networking stack.

UInt32 RTL8139::outputPacket( mbuf_t pkt, void * param )
{
    UInt8 *             des_ptr_ub;
    long		        pkt_len;
    mbuf_t              mn;
    IODebuggerLockState lockState;

    DEBUG_LOG("outputPacket() ===>\n");

    lockState = IODebuggerLock( this );

    if ( kOwnedByChip == tx_buf_ownership[tx_send_index] )
    {
        // Stall the output queue until the ack by the interrupt handler.
        DEBUG_LOG("outputPacket() <===\n");
        IODebuggerUnlock( lockState );
        return kIOReturnOutputStall;
    }

    des_ptr_ub = tx_buf_ptr[tx_send_index];

    pkt_len = mbuf_pkthdr_len(pkt);

    for ( mn = pkt; mn; mn = mbuf_next(mn) )
    {
        bcopy( mbuf_data(mn), des_ptr_ub, mbuf_len(mn) );
        des_ptr_ub += mbuf_len(mn);
    }

    // Software padding of small frames. Hardware doesn't do this.

    if ( pkt_len < MINPACK )
    {
        memset( des_ptr_ub, 0x55, MINPACK - pkt_len );
        pkt_len = MINPACK;
    }

    csrWrite32( RTL_TSD0 + (4 * tx_send_index), pkt_len | R_TSD_ERTXTH );

    tx_send_count++;
    tx_buf_ownership[tx_send_index] = kOwnedByChip;

    if ( 4 == ++tx_send_index ) tx_send_index = 0;

	IODebuggerUnlock( lockState );

    freePacket( pkt );

    BUMP_NET_COUNTER( outputPackets );

	DEBUG_LOG("outputPacket() <===\n");

    return kIOReturnOutputSuccess;
}

//---------------------------------------------------------------------------
// Send KDP packets in polled mode.

void RTL8139::sendPacket( void * pkt, UInt32 pkt_len )
{
    UInt8 * dest_ptr;

    if ( pkt_len > MAXPACK ) return;

    // Poll until a transmit buffer becomes available.

    while ( kOwnedByChip == tx_buf_ownership[tx_send_index] )
    {
        reclaimTransmitBuffer();
        // kprintf("RTL8139: transmitterInterrupt poll\n");
    }

    dest_ptr = tx_buf_ptr[ tx_send_index ];

    // Copy the debugger packet to the transmit buffer.
    
    bcopy( pkt, dest_ptr, pkt_len );

    // Pad small frames.

    if ( pkt_len < MINPACK )
    {
        memset( dest_ptr + pkt_len, 0x55, MINPACK - pkt_len );
        pkt_len = MINPACK;
    }

    csrWrite32( RTL_TSD0 + (4 * tx_send_index), pkt_len | R_TSD_ERTXTH );

    tx_send_count++;
    tx_buf_ownership[tx_send_index] = kOwnedByChip;

    if ( 4 == ++tx_send_index ) tx_send_index = 0;
    
    // kprintf("RTL8139: sendPacket len %d\n", pkt_len);
}

void RTL8139::reclaimTransmitBuffer( void )
{
    UInt32  tx_status;
    UInt32  tmp_ul;

    tx_buf_reclaimed = true;

    while ( tx_send_count )
    {
        tx_status = csrRead32( RTL_TSD0 + tx_ack_index * 4 );

        if ( (tx_status & (R_TSD_TABT | R_TSD_TOK | R_TSD_TUN)) == 0 )
            break;  // not owned by host (check OWN bit instead?)

        if ( tx_status & R_TSD_TABT ) // transmit abort
        {
            tmp_ul = csrRead32( RTL_TCR );
            tmp_ul |= R_TCR_CLRABT;   // resend packet 
            csrWrite32( RTL_TCR, tmp_ul );
            break;  // wait for retransmission
        }

        tx_send_count--;
        tx_buf_ownership[tx_ack_index] = kOwnedByHost;

        if ( 4 == ++tx_ack_index ) tx_ack_index = 0;
    }
}

//---------------------------------------------------------------------------
// Receive (KDP) packets in polled mode. This is essentially a simplified
// version of the receiverInterrupt() function.

void RTL8139::receivePacket( void * pkt, UInt32 * pkt_len, UInt32 timeout )
{
	rbuf_hdr_t  rbh;     // 4 byte rx packet packet
    UInt16      offset;
	UInt16	    capr;
	UInt16	    rcv_len;

    *pkt_len = 0;
    timeout *= 1000;  // convert ms to us

    while ( timeout && *pkt_len == 0 )
    {
        if ( (csrRead8( RTL_CM ) & R_CM_BUFE) == R_CM_BUFE )
        {
            // Receive buffer empty, wait and retry.
            IODelay( 50 );
            timeout -= 50;
            continue;
        }

        *((UInt32 *)&rbh) = OSReadLittleInt32( rx_buf_ptr, 0 );

        rcv_len = rbh.rb_count;

        //kprintf("RTL8139::receivePacket status %x len %d\n",
        //        rbh.rb_status, rcv_len);

        if ( rbh.rb_status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG |
                              R_RSR_RUNT | R_RSR_ISE) )
        {
            rbh.rb_status &= ~R_RSR_ROK;
        }

        if ( (rbh.rb_status & R_RSR_ROK) == 0 )
        {
            restartReceiver();
            continue;
        }

        if ( (rcv_len >= MINPACK + 4) && (rcv_len <= MAXPACK + 4) ) 
        {
            bcopy( rx_buf_ptr + sizeof(rbh), pkt, rcv_len );
            *pkt_len = rcv_len;
        }

        // Advance the receive ring buffer to the start of the next packet.

        offset = IORound(rcv_len, 4) + sizeof(rbh);
        rx_buf_ptr += offset;

        if (rx_buf_ptr >= (rx_buf_end - 1))
            rx_buf_ptr -= RX_BUF_SIZE;

        capr = csrRead16( RTL_CAPR );
        capr += offset;
        csrWrite16Slow( RTL_CAPR, capr );
    }
}
