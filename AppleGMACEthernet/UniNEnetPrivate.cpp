/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-2002 Apple Computer
 *
 * Implementation for hardware dependent (relatively) code 
 * for the Sun GEM Ethernet controller. 
 *
 * HISTORY
 *
 * 10-Sept-97        
 *  Created.
 *
 */

#include <libkern/OSByteOrder.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOKitKeys.h>				// kIODeviceTreePlane
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "UniNEnet.h"
#include "UniNEnetMII.h"



bool UniNEnet::allocateMemory()
{
	UInt32		rxRingSize, txRingSize;

 
		/* Allocate memory for DMA ring elements - Tx ring first:	*/

    txRingSize		= fTxRingElements * sizeof( TxDescriptor );
    fTxRingMemDesc	= IOBufferMemoryDescriptor::withOptions(	kIOMemoryPhysicallyContiguous,
																txRingSize,
																PAGE_SIZE );
    if ( !fTxRingMemDesc )
	{
        ALRT( 0, txRingSize, 'TRM-', "UniNEnet::allocateMemory - failed to alloc Tx Ring memory" );
		return false;
	}
	fTxRingMemDesc->prepare( kIODirectionOutIn );
	fTxDescriptorRing		= (TxDescriptor*)fTxRingMemDesc->getBytesNoCopy();	// get virtual address
	fTxDescriptorRingPhys	= fTxRingMemDesc->getPhysicalAddress();				// may be DART address

    if ( fTxDescriptorRingPhys == 0 )
	{
        ALRT( 0, txRingSize, 'TRP-', "UniNEnet::allocateMemory - Bad Tx Ring" );
		return false;
	}
	ELG( txRingSize, fTxDescriptorRing, '=TxR', "UniNEnet::allocateMemory - Tx Ring alloc'd" );


		/* Now do the Rx ring:	*/


    rxRingSize = fRxRingElements * sizeof( RxDescriptor );
    fRxRingMemDesc	= IOBufferMemoryDescriptor::withOptions(	kIOMemoryPhysicallyContiguous,
																rxRingSize,
																PAGE_SIZE );
	if ( !fRxRingMemDesc )
	{
		ALRT( 0, rxRingSize, 'RRM-', "UniNEnet::allocateMemory - failed to alloc Rx Ring" );
		return false;
	}
	fRxRingMemDesc->prepare( kIODirectionOutIn );
	fRxDescriptorRing		= (RxDescriptor*)fRxRingMemDesc->getBytesNoCopy();	// get virtual address
	fRxDescriptorRingPhys	= fRxRingMemDesc->getPhysicalAddress();				// may be DART address

	if ( fRxDescriptorRingPhys == 0 )
    {
        ALRT( 0, rxRingSize, 'RRP-', "UniNEnet::allocateMemory - Bad Rx Ring" );
        return false;
    }
	ELG( rxRingSize, fRxDescriptorRing, '=RxR', "UniNEnet::allocateMemory - Rx Ring alloc'd" );


		/* set up the Tx and Rx mBuf pointer arrays:	*/

	fTxMbuf = (mbuf_t*)IOMalloc( sizeof( mbuf_t ) * fTxRingElements );
	if ( !fTxMbuf )
	{
		ALRT( 0, 0, 'mpT-', "UniNEnet::allocateMemory - alloc Tx mbuf pointers failed" );
		return false;
	}
	bzero( fTxMbuf, sizeof( mbuf_t ) * fTxRingElements );	// clear out all the fTxMbuf pointers

	fRxMbuf = (mbuf_t*)IOMalloc( sizeof( mbuf_t ) * fRxRingElements );
	if ( !fRxMbuf )
	{
		ALRT( 0, 0, 'mpR-', "UniNEnet::allocateMemory - alloc Rx mbuf pointers failed" );
		return false;
	}
	bzero( fRxMbuf, sizeof( mbuf_t ) * fRxRingElements );	// clear out all the fRxMbuf pointers

	ELG( fTxMbuf, fRxMbuf, 'arys', "UniNEnet::allocateMemory - mbuf pointer arrays" );
    return true;
}/* end allocateMemory */


	/*-------------------------------------------------------------------------
	 *
	 * Setup the Transmit Ring - called by monitorLinkStatus()
	 * -----------------------
	 * Each transmit ring entry consists of two words to transmit data from buffer
	 * segments (possibly) spanning a page boundary. This is followed by two DMA commands 
	 * which read transmit frame status and interrupt status from the UniN chip. The last
	 * DMA command in each transmit ring entry generates a host interrupt.
	 * The last entry in the ring is followed by a DMA branch to the first
	 * entry.
	 *-------------------------------------------------------------------------*/

bool UniNEnet::initTxRing()
{
	UInt32		     i;


	ELG( this, fTxDescriptorRing, 'ITxR', "UniNEnet::initTxRing" );

		/* Clear the transmit DMA command memory	*/

    bzero( (void*)fTxDescriptorRing, sizeof( TxDescriptor ) * fTxRingElements );
    txCommandHead = 0;
    txCommandTail = 0;
 
	for ( i = 0; i < fTxRingElements; i++ )
	{
		if ( fTxMbuf[ i ] )
		{
			ELG( i, fTxMbuf[ i ], 'txpf', "UniNEnet::initTxRing - free the packet" );
			if ( fTxMbuf[ i ] == txDebuggerPkt )
				txDebuggerPktInUse = false;
			else
				freePacket( fTxMbuf[ i ] );
			fTxMbuf[ i ] = 0;
        }
	}

    fTxIntCnt	= 0;
    txWDCount	= 0;

    return true;
}/* end initTxRing */


	/* initRxRing - Setup the Receive ring.										*/
	/* Each receive ring entry consists of two DMA commands to receive data		*/
	/* into a network buffer (possibly) spanning a page boundary. The second	*/
	/* DMA command in each entry generates a host interrupt. The last entry		*/
	/* in the ring is followed by a DMA branch to the first entry.				*/

bool UniNEnet::initRxRing()
{
    UInt32   i;
    bool     status;


	ELG( fRxMbuf, fRxDescriptorRing, 'IRxR', "UniNEnet::initRxRing" );

		/* Clear the Rx DMA commands:	*/

	bzero( (void*)fRxDescriptorRing, sizeof( RxDescriptor ) * fRxRingElements );

		/* Allocate a receive buffer for each entry in the Receive ring.	*/
		/* Make the last one be the one for the bit bucket.					*/

	for ( i = 0; i < fRxRingElements; i++ )
    {
        if ( fRxMbuf[i] == NULL )
        {
            fRxMbuf[i] = allocatePacket( NETWORK_BUFSIZE );
            if ( fRxMbuf[i] == NULL )
            {
                IOLog( "UniNEnet::initRxRing - NULL packet\n" );
                return false;
            }
		///	ELG( i, fRxMbuf[i], '=RxP', "UniNEnet::initRxRing" );
        }

			/* Set the DMA commands for the ring entry to transfer data	*/
			/* to the mbuf.												*/
        status = genRxDescriptor( i );
        if ( status == false )
        {
            IOLog( "UniNEnet::initRxRing genRxDescriptor error\n" );
            return false;
        }
    }/* end FOR Rx ring length */

		/* Set the receive queue head to point to the first entry in the ring.	*/

    rxCommandHead = 0;

    return true;
}/* end initRxRing */


void UniNEnet::flushRings( bool flushTx, bool flushRx )
{
	UInt32		i;


	ELG( fTxRingElements, fRxRingElements, 'FluR', "UniNEnet::flushRings" );

	if ( flushTx )			// Free all mbufs from the transmit ring:
	{
		for ( i = 0; i < fTxRingElements; i++ )
		{
			if ( fTxMbuf[ i ] )
			{
				ELG( i, fTxMbuf[ i ], 'flTx', "UniNEnet::flushRings - Tx ring" );
				if ( fTxMbuf[ i ] == txDebuggerPkt )
					txDebuggerPktInUse = false;
				else
					freePacket( fTxMbuf[ i ] );
				fTxMbuf[ i ] = 0;
			}
		}
	}

	if ( flushRx )			// Free all mbufs from the receive ring:
	{
		for ( i = 0; i < fRxRingElements; i++ )
		{
			if ( fRxMbuf[ i ] )
			{
				ELG( i, fRxMbuf[ i ], 'flRx', "UniNEnet::flushRings - Rx ring" );
				freePacket( fRxMbuf[ i ] );
				fRxMbuf[i] = 0;
			}
		}
	}
	return;
}/* end flushRings */


	/* startChip is called only by monitorLinkStatus.	*/

void UniNEnet::startChip()
{
	ELG( this, 0, 'SChp', "startChip" );

	fXIFConfiguration |= kXIFConfiguration_Tx_MII_OE;
	WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );

	fTxConfiguration |= kTxConfiguration_Tx_DMA_Enable;		// Tx DMA enable
	WRITE_REGISTER( TxConfiguration, fTxConfiguration );

	fRxConfiguration |= kRxConfiguration_Rx_DMA_Enable;		// Rx DMA enable
	WRITE_REGISTER( RxConfiguration, fRxConfiguration	 );

	fTxMACConfiguration |= kTxMACConfiguration_TxMac_Enable;
	WRITE_REGISTER( TxMACConfiguration, fTxMACConfiguration  );

	fRxMACConfiguration	= kRxMACConfiguration_Rx_Mac_Enable;// Rx MAC enable
	if ( fIsPromiscuous )
		 fRxMACConfiguration |= kRxMACConfiguration_Promiscuous;
	else fRxMACConfiguration |= kRxMACConfiguration_Strip_FCS;
	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );

#ifdef LATER 
	WRITE_REGISTER( InterruptMask, ~(kStatus_TX_ALL | kStatus_TX_INT_ME | kStatus_RX_DONE | kStatus_MIF_Interrupt) );
#else
	WRITE_REGISTER( InterruptMask, ~(kStatus_TX_ALL | kStatus_TX_INT_ME | kStatus_RX_DONE) );
#endif // LATER

	return;
}/* end startChip */


void UniNEnet::stopChip()
{
	ELG( READ_REGISTER( TxConfiguration ), READ_REGISTER( RxConfiguration ), 'HChp', "stopChip" );

	fTxConfiguration &= ~kTxConfiguration_Tx_DMA_Enable;	// clear Tx DMA enable
	WRITE_REGISTER( TxConfiguration, fTxConfiguration );

	fRxConfiguration &= ~kRxConfiguration_Rx_DMA_Enable;	// clear Rx DMA enable
	WRITE_REGISTER( RxConfiguration, fRxConfiguration	 );

	IOSleep( 1 );	// Give time for DMAs to finish what they're doing.

	fTxMACConfiguration &= ~kTxMACConfiguration_TxMac_Enable;
	WRITE_REGISTER( TxMACConfiguration, fTxMACConfiguration  );

    fRxMACConfiguration &= ~kRxMACConfiguration_Rx_Mac_Enable;    
	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration  );

	IOSleep( 4 );		/// Wait 3.2 ms (Rx) or poll for enables to clear.

	return;
}/* end stopChip */


	static MediumTable gMediumTableGEM[] =		/* PCI fiber optic card	*/
	{
		{ kIOMediumEthernetNone                                  ,   0   },
		{ kIOMediumEthernetAuto                                  ,   0   },
		{ kIOMediumEthernet1000BaseSX | kIOMediumOptionFullDuplex,	1000 },
	};


	static MediumTable gMediumTable100[] =
	{
		{ kIOMediumEthernetNone																	, 0		},
		{ kIOMediumEthernetAuto																	, 0		},

		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex								, 10	},
	//	{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex	| kIOMediumOptionLoopback	, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex								, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex	| kIOMediumOptionFlowControl, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex	| kIOMediumOptionLoopback	, 10	},

		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex								, 100	},
	//	{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex	| kIOMediumOptionLoopback	, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex								, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex	| kIOMediumOptionFlowControl, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex	| kIOMediumOptionLoopback	, 100	}
	};


	static MediumTable gMediumTableGigabit[] =
	{
		{ kIOMediumEthernetNone																	, 0		},
		{ kIOMediumEthernetAuto																	, 0		},

		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex								, 10	},
	//	{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex	| kIOMediumOptionLoopback	, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex								, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex	| kIOMediumOptionFlowControl, 10	},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex	| kIOMediumOptionLoopback	, 10	},

		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex								, 100	},
	//	{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex	| kIOMediumOptionLoopback	, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex								, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex	| kIOMediumOptionFlowControl, 100	},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex	| kIOMediumOptionLoopback	, 100	},

		{ kIOMediumEthernet1000BaseT  	| kIOMediumOptionFullDuplex								, 1000	},
		{ kIOMediumEthernet1000BaseT	| kIOMediumOptionFullDuplex	| kIOMediumOptionFlowControl, 1000	},
		{ kIOMediumEthernet1000BaseT 	| kIOMediumOptionFullDuplex	| kIOMediumOptionLoopback	, 1000	}
	};


void UniNEnet::getPhyType()
{
	UInt16		*pPhyType;
	UInt16		phyWord;


	ELG( this, phyId, 'gPhT', "getPhyType" );

	if ( !fBuiltin )
	{
		fPHYType = ' GEM';

		fpgMediumTable		= gMediumTableGEM;
		fMediumTableCount	=  sizeof( gMediumTableGEM ) / sizeof( MediumTable ) ;
		setProperty( "PHY type", "GEM integrated" );
		return;
	}

	pPhyType = (UInt16*)&phyType;
	miiReadWord( pPhyType,   MII_ID0 );
	miiReadWord( pPhyType+1, MII_ID1 );

	setProperty( "PHY ID", phyType, sizeof( phyType ) * 8 );

	ELG( phyId, phyType, '=FyT', "UniNEnet::getPhyType" );

	if ( (phyType & MII_BCM5400_MASK) == MII_BCM5400_ID )
	{
		fPHYType = 0x5400;
		ELG( this, phyId, '5400', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5400" );

		miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL );
		phyWord |= MII_BCM5400_AUXCONTROL_PWR10BASET;
		miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL );
	  
		miiReadWord( &phyWord, MII_1000BASETCONTROL );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL );

		IODelay( 100 );   

				// Clockwork was the 1st machine to have gigabit
				// which came with the Broadcom 5400 PHY. That PHY
				//	did not support 10 Mbps so Apple supplemented it
				// with a Broadcom 5201 at 0x1F.
			UInt8	temp = phyId;
			phyId = 0x1F;
		miiResetPHY();	// Reset the supplemental 5201 PHY

		miiReadWord( &phyWord, MII_BCM5201_MULTIPHY );
		phyWord |= MII_BCM5201_MULTIPHY_SERIALMODE;
		miiWriteWord( phyWord, MII_BCM5201_MULTIPHY );

			phyId = temp;

		miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL );
		phyWord &= ~MII_BCM5400_AUXCONTROL_PWR10BASET;
		miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end ELSE IF 5400 */
	else if ( (phyType & MII_BCM5400_MASK) == MII_BCM5401_ID )
	{
		fPHYType = 0x5401;
		ELG( this, phyId, '5401', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5401" );

			// "0x1" in the low nibble which is Rev. B0 Silicon
			// "0x3" in the low nibble which is Rev. B2 Silicon	
		miiReadWord( &phyWord, MII_ID1 );

			// check if this is the "B0" revision of the 5401 PHY...this will
			// help with the gigabit link establishment.
		phyWord &= 0x000F ;

		if ( (phyWord == 0x0001 ) || (phyWord == 0x0003) )
		{
			miiWriteWord( 0x0C20, 0x018 );
			miiWriteWord( 0x0012, 0x017 );
			miiWriteWord( 0x1804, 0x015 );
			miiWriteWord( 0x0013, 0x017 );
			miiWriteWord( 0x1204, 0x015 );
			miiWriteWord( 0x8006, 0x017 );
			miiWriteWord( 0x0132, 0x015 );
			miiWriteWord( 0x8006, 0x017 );
			miiWriteWord( 0x0232, 0x015 );
			miiWriteWord( 0x201F, 0x017 );
			miiWriteWord( 0x0A20, 0x015 );
		}

		miiReadWord( &phyWord, MII_1000BASETCONTROL );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL );

		IODelay( 10 );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end else IF 5401 */
	else if ( (phyType & MII_BCM5400_MASK) == MII_BCM5411_ID )	// 5411:
	{
		fPHYType = 0x5411;
		ELG( this, phyId, '5411', "UniNEnet::getPhyType - Broadcom 5411" );
		setProperty( "PHY type", "Broadcom 5411" );

		miiWriteWord( 0x8C23, 0x01C );		// setting some undocumented voltage
		miiWriteWord( 0x8CA3, 0x01C );
		miiWriteWord( 0x8C23, 0x01C );

		miiWriteWord( 0x8000, 0x000 );		// reset PHY (MII_CONTROL_RESET, MII_CONTROL)

		miiWriteWord( 0x1340, 0x000 );		// advertise gigabit, full-duplex

		miiReadWord( &phyWord, MII_1000BASETCONTROL );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL );

		IODelay( 10 );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end IF 5411 */
	else if ( (phyType & MII_BCM5400_MASK) == MII_BCM5421_ID	// 5421:
		   || (phyType & MII_BCM5400_MASK) == MII_BCM54K2_ID	// 54K2:
		   || (phyType & MII_BCM5400_MASK) == MII_BCM5462_ID )	// 5462:
	{
		const IORegistryPlane	*plane = getPlane( kIODeviceTreePlane );
		IORegistryEntry			*entry;
		OSObject				*obj;
		bool					noAutoLowPower = false;

		fPHYType = 0x5421;
		ELG( this, phyId, '5421', "UniNEnet::getPhyType - Broadcom 5421" );
		setProperty( "PHY type", "Broadcom 5421" );

		if ( phyType == MII_BCM5421_ID )			// patch only 5421 rev 0:
		{
			miiWriteWord( 0x1007,			0x18 );	// Set Class A Mode
			miiReadWord( &phyWord,			0x18 );
			miiWriteWord( phyWord | 0x0400,	0x18 );
	
			miiWriteWord( 0x0007,			0x18 );	// Set FFE Gamma Override to -0.125
			miiReadWord( &phyWord,			0x18 );
			miiWriteWord( phyWord | 0x0800,	0x18 );
			miiWriteWord( 0x000A,			0x17 );
			miiReadWord( &phyWord,			0x15 );
			miiWriteWord( phyWord | 0x0200,	0x15 );
		}

		miiReadWord( &phyWord, MII_1000BASETCONTROL );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL );

			/* P57B and P58 with BCom 5421 have a problem with Auto Power-down.	*/
			/* If put to sleep, it can't be woken with a USB keyboard.			*/
			/* To resolve this, only P57B and P58 have a registry entry			*/
			/* "no-autolowpower" within the "ethernet-phy" entry.				*/

		entry = nub->getChildEntry( plane );	// get "ethernet-phy" registry entry
		if ( entry )
		{		// try to get "no-autolowpower" property on P57B, and P58
			obj = entry->copyProperty( "no-autolowpower" );
			if ( obj )
				 noAutoLowPower = true;
		}

	//	ELG( 0, noAutoLowPower, '?alp', "UniNEnet::getPhyType" );
		if ( noAutoLowPower == false )
		{
			miiWriteWord( 0x9002, 0x1C );		// enable Energy Detect
			miiWriteWord( 0xA821, 0x1C );		// enable Auto Power-Down bit
			miiWriteWord( 0x941D, 0x1C );		// disable CLK125
		}
		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end IF 5421 */
	else if ( ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID)		// 0x01410C2x
	       || ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_1) )	// 0x01410C6x
	{
		fPHYType = 0x1011;
		ELG( this, phyId, '1011', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Marvell 88E1011" );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable );
	}/* end else IF Marvell 88e1011 */
	else if ( (phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_2 )	// 0x01410CCx 88E1111
	{
		fPHYType = 0x1011;				// for now, use same code as older 1011
		ELG( this, phyId, '1111', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Marvell 88E1111" );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable );

		if ( phyType == MII_MARVELL_ID_2 )	// patch only version 0 of 88E1111:
		{
			miiWriteWord( 0x000A, 0x01D );
			miiWriteWord( 0x0821, 0x01E );
		
			miiWriteWord( 0x0006, 0x01D );
			miiWriteWord( 0x8600, 0x01E );
		
			miiWriteWord( 0x000B, 0x01D );
			miiWriteWord( 0x0100, 0x01E );
		
			miiWriteWord( 0x0004, 0x01D );
			miiWriteWord( 0x4850, 0x01E );
		}

	}/* end else IF Marvell 88E1111 */
	else if ( (phyType & MII_BCM5201_MASK) == MII_BCM5201_ID )
	{
		fPHYType = 0x5201;
		ELG( this, phyId, '5201', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5201" );

		fpgMediumTable		= gMediumTable100;
		fMediumTableCount	=  sizeof( gMediumTable100 ) / sizeof( MediumTable ) ;
	}
	else if ( (phyType & MII_BCM5221_MASK) == MII_BCM5221_ID )
	{
		fPHYType = 0x5221;
		ELG( this, phyId, '5221', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5221" );

			// 1: enable shadow mode registers in 5221 (0x1A-0x1E)
		miiReadWord( &phyWord, MII_BCM5221_TestRegister );
		phyWord |= MII_BCM5221_ShadowRegEnableBit;
		miiWriteWord( phyWord, MII_BCM5221_TestRegister );

			// 2: enable APD (Auto PowerDetect)
		miiReadWord( &phyWord, MII_BCM5221_AuxiliaryStatus2 );
		phyWord |= MII_BCM5221_APD_EnableBit;
		miiWriteWord( phyWord, MII_BCM5221_AuxiliaryStatus2 );

			// 3: enable clocks across APD for Auto-MDIX functionality
		miiReadWord( &phyWord, MII_BCM5221_AuxiliaryMode4 );
		phyWord |= MII_BCM5221_EnableClkDuringLowPwr;
		miiWriteWord( phyWord, MII_BCM5221_AuxiliaryMode4 );

			// 4: Disable shadow mode registers in 5221 (0x1A-0x1E)
		miiReadWord( &phyWord, MII_BCM5221_TestRegister );
		phyWord &= ~MII_BCM5221_ShadowRegEnableBit;
		miiWriteWord( phyWord, MII_BCM5221_TestRegister );

		miiWriteWord( MII_BCM5201_INTERRUPT_INTREnable, MII_BCM5201_INTERRUPT );
	///	miiWriteWord( 0x4F00, MII_BCM5201_INTERRUPT );

		fpgMediumTable		= gMediumTable100;
		fMediumTableCount	=  sizeof( gMediumTable100 ) / sizeof( MediumTable ) ;
	}/* end ELSE 5221 */
	else if ( (phyType & MII_BCM5241_MASK) == MII_BCM5241_ID )
	{
		fPHYType = 0x5241;
		ELG( this, phyId, '5241', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5241" );

			// Use some symbols in common with 5221.

			// 1: enable shadow register mode
		miiReadWord( &phyWord, MII_BCM5221_TestRegister );
		phyWord |= MII_BCM5221_ShadowRegEnableBit;
		miiWriteWord( phyWord, MII_BCM5221_TestRegister );

			// 2: enable APD (Auto PowerDetect)
		miiReadWord( &phyWord, MII_BCM5221_AuxiliaryStatus2 );
		phyWord |= MII_BCM5221_APD_EnableBit;
		miiWriteWord( phyWord, MII_BCM5221_AuxiliaryStatus2 );

			// 3: clear standby power bit:
		miiReadWord( &phyWord, MII_BCM5221_AuxiliaryMode4 );
		phyWord &= ~MII_BCM5241_StandbyPowerMode;
		miiWriteWord( phyWord, MII_BCM5221_AuxiliaryMode4 );

			// 4: Disable shadow register mode
		miiReadWord( &phyWord, MII_BCM5221_TestRegister );
		phyWord &= ~MII_BCM5221_ShadowRegEnableBit;
		miiWriteWord( phyWord, MII_BCM5221_TestRegister );

///		miiWriteWord( MII_BCM5201_INTERRUPT_INTREnable, MII_BCM5201_INTERRUPT );

		fpgMediumTable		= gMediumTable100;
		fMediumTableCount	=  sizeof( gMediumTable100 ) / sizeof( MediumTable ) ;
	}/* end ELSE 5241 */
	else if ( (phyType & MII_LXT971_MASK) == MII_LXT971_ID )
	{
		fPHYType = 0x0971;
		ELG( this, phyId, 'L971', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Level One LXT971" );

		fpgMediumTable		= gMediumTable100;
		fMediumTableCount	=  sizeof( gMediumTable100 ) / sizeof( MediumTable ) ;
	}
	else
	{
		fPHYType = 0;
		ELG( this, phyType, 'phy?', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Unknown" );

		fpgMediumTable		= gMediumTable100;
		fMediumTableCount	=  sizeof( gMediumTable100 ) / sizeof( MediumTable ) ;
	}
	//	IOLog( "DEBUG:UniNEnet: phy type = %d\n", fPHYType );

    return;
}/* end getPhyType */


bool UniNEnet::initChip()
{
    UInt32          	i;
    mach_timespec_t		timeStamp;
    UInt16       		*p16;
	OSNumber			*numObj;
	UInt32				busSpeed;
	volatile UInt32		vui32;


	ELG( 0, phyId, 'ChpI', "initChip" );

		/* Initialize the chip but don't start anything until startChip:	*/

	fTxConfiguration	&= ~kTxConfiguration_Tx_DMA_Enable;
	fRxConfiguration	&= ~kRxConfiguration_Rx_DMA_Enable;
	fTxMACConfiguration	&= ~kTxMACConfiguration_TxMac_Enable;
	fRxMACConfiguration	&= ~kRxMACConfiguration_Rx_Mac_Enable;

    if ( fBuiltin == false )
    {
		WRITE_REGISTER( DatapathMode,		kDatapathMode_ExtSERDESMode );
		WRITE_REGISTER( SerialinkControl,	kSerialinkControl_DisableLoopback
										  | kSerialinkControl_EnableSyncDet );
		WRITE_REGISTER( Advertisement,		kAdvertisement_Full_Duplex
										  | kAdvertisement_PAUSE );
		WRITE_REGISTER( PCSMIIControl,		kPCSMIIControl_Auto_Negotiation_Enable
										  |	kPCSMIIControl_Restart_Auto_Negotiation );
		WRITE_REGISTER( PCSConfiguration,	kPCSConfiguration_Enable );
		fXIFConfiguration =					kXIFConfiguration_Tx_MII_OE
										  | kXIFConfiguration_GMIIMODE
										  |	kXIFConfiguration_FDPLXLED;
    }
    else
    {
		WRITE_REGISTER( DatapathMode, kDatapathMode_GMIIMode );
	}
	WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );

	WRITE_REGISTER( MACControlType,			kMACControlType_default );
	WRITE_REGISTER( MACControlConfiguration,fMACControlConfiguration );
	WRITE_REGISTER( InterruptMask,			kInterruptMask_None );

	WRITE_REGISTER( TxMACMask,				kTxMACMask_default );
	WRITE_REGISTER( RxMACMask,				kRxMACMask_default );
	WRITE_REGISTER( MACControlMask,			kMACControlMask_default );

	vui32 = READ_REGISTER( TxMACStatus );		// read-autoclear the MAC status bits
	vui32 = READ_REGISTER( RxMACStatus );
	vui32 = READ_REGISTER( MACControlStatus );

	WRITE_REGISTER( Configuration, fConfiguration );	// try the default
	vui32 = READ_REGISTER( Configuration );				// read it back
    if ( (vui32 & kConfiguration_Infinite_Burst) == 0 )	
    {													// not infinite-burst capable:
        ELG( 0, 0, 'Lims', "UniNEnet::initChip: set TX_DMA_Limit and RX_DMA_Limit." );
		fConfiguration	= (0x02 << 1) | (0x08 << 6);	// change TX_DMA_Limit, RX_DMA_Limit
		WRITE_REGISTER( Configuration, fConfiguration );
    }

	WRITE_REGISTER( InterPacketGap0,	kInterPacketGap0_default );
	WRITE_REGISTER( InterPacketGap1,	kInterPacketGap1_default );
	WRITE_REGISTER( InterPacketGap2,	kInterPacketGap2_default );
	WRITE_REGISTER( SlotTime,			kSlotTime_default );
	WRITE_REGISTER( MinFrameSize,		kMinFrameSize_default );
	WRITE_REGISTER( MaxFrameSize,		kMaxFrameSize_default );
	WRITE_REGISTER( PASize,				kPASize_default );
	WRITE_REGISTER( JamSize,			kJamSize_default );
	WRITE_REGISTER( AttemptLimit,		kAttemptLimit_default );

    p16 = (UInt16*)myAddress.bytes;
    for ( i = 0; i < sizeof( IOEthernetAddress ) / 2; i++ )
	{
		WRITE_REGISTER( MACAddress[ i ],		p16[ 2 - i ] );
		WRITE_REGISTER( MACAddress[ i + 3 ],	p16[ 2 - i ] );	// init the alternate MAC address
	}

    for ( i = 0; i < 3; i++ )
		WRITE_REGISTER( AddressFilter[ i  ],	0 );

	WRITE_REGISTER( MACAddress[ 6 ], kMACAddress_default_6 );
	WRITE_REGISTER( MACAddress[ 7 ], kMACAddress_default_7 );
	WRITE_REGISTER( MACAddress[ 8 ], kMACAddress_default_8 );

	WRITE_REGISTER( AddressFilter2_1Mask,	0 );
	WRITE_REGISTER( AddressFilter0Mask,		0 );

    for ( i = 0; i < 16; i++ )
		WRITE_REGISTER( HashTable[ i ], 0 );

	WRITE_REGISTER( NormalCollisionCounter,					0 );
	WRITE_REGISTER( FirstAttemptSuccessfulCollisionCounter,	0 );
	WRITE_REGISTER( ExcessiveCollisionCounter,				0 );
	WRITE_REGISTER( LateCollisionCounter,					0 );
	WRITE_REGISTER( DeferTimer,								0 );
	WRITE_REGISTER( PeakAttempts,							0 );
	WRITE_REGISTER( ReceiveFrameCounter,					0 );
	WRITE_REGISTER( LengthErrorCounter,						0 );
	WRITE_REGISTER( AlignmentErrorCounter,					0 );
	WRITE_REGISTER( FCSErrorCounter,						0 );
	WRITE_REGISTER( RxCodeViolationErrorCounter,			0 );

    IOGetTime( &timeStamp );
	WRITE_REGISTER( RandomNumberSeed, timeStamp.tv_nsec & 0x03FF );

	WRITE_REGISTER( TxDescriptorBaseLow, fTxDescriptorRingPhys );
	WRITE_REGISTER( TxDescriptorBaseHigh, 0 );

	WRITE_REGISTER( TxConfiguration,	fTxConfiguration );
	WRITE_REGISTER( TxMACConfiguration,	fTxMACConfiguration );
	IOSleep( 4 );		/// Wait or poll for enable to clear.
   
	WRITE_REGISTER( RxDescriptorBaseLow,	fRxDescriptorRingPhys );
	WRITE_REGISTER( RxDescriptorBaseHigh,	0 );

	WRITE_REGISTER( RxKick, fRxRingElements - 4 );
	WRITE_REGISTER( RxConfiguration, fRxConfiguration );

	WRITE_REGISTER( RxMACConfiguration,	fRxMACConfiguration );
	IOSleep( 4 ); 		// it takes time to clear the enable bit

	WRITE_REGISTER( PauseThresholds,	fPauseThresholds  );
	WRITE_REGISTER( SendPauseCommand,	fSendPauseCommand );

	fRxBlanking = kRxBlanking_default_66;	/* built-in is always 66 MHz PCI	*/
	if ( !fBuiltin )
	{		/* Must be GEM card in a slot:	*/
		fRxBlanking = kRxBlanking_default_33;			/* assume 33 MHz slot	*/
		numObj = OSDynamicCast( OSNumber, getProperty( "clock-frequency" ) );
		if ( numObj )
		{
			busSpeed = numObj->unsigned32BitValue();
			if ( busSpeed > 33333333 )
				fRxBlanking = kRxBlanking_default_66;
		}
	}
	WRITE_REGISTER( RxBlanking, fRxBlanking );

#ifdef LATER
	if ( fPHYType == 0x5221 )
	{
		phyAdrToPoll	 = phyId;
		phyRegToPoll	 = MII_BCM5201_INTERRUPT;
		phyBitsToMonitor = ~(MII_BCM5201_INTERRUPT_LINKChange
						 |   MII_BCM5201_INTERRUPT_SPDChange
						 |   MII_BCM5201_INTERRUPT_FDXChange);
		WRITE_REGISTER( MIFMask, phyBitsToMonitor );
	
		val16	= READ_REGISTER( MIFConfiguration )
				| kMIFConfiguration_Poll_Enable
				| phyAdrToPoll << kMIFConfiguration_Poll_Phy_Adr_Shift
				| phyRegToPoll << kMIFConfiguration_Poll_Reg_Adr_Shift;
		
		WRITE_REGISTER( MIFConfiguration, val16 );
	}
#endif // LATER

	if ( fBuiltin )
		WRITE_REGISTER( WOLWakeupCSR, 0 );	// Disable Wake on LAN

	ELG( READ_REGISTER( TxFIFOSize ), READ_REGISTER( RxFIFOSize ), 'FFsz', "initChip - done" );

    return true;
}/* end initChip */


void UniNEnet::setDuplexMode( bool duplexMode )		/* called only by monitorLinkStatus	*/
{
	isFullDuplex = duplexMode;

	ELG( fTxMACConfiguration, duplexMode, 'DupM', "setDuplexMode" );

	WRITE_REGISTER( TxMACConfiguration, 0 );	/* Clear kTxMACConfiguration_TxMac_Enable		*/
	IOSleep( 1 );								/* must wait up to several hundred microseconds	*/

    if ( isFullDuplex )
    {
		fTxMACConfiguration |= (kTxMACConfiguration_Ignore_Collisions | kTxMACConfiguration_Ignore_Carrier_Sense);
		fXIFConfiguration &= ~kXIFConfiguration_Disable_Echo;
    }
    else
    {
		fTxMACConfiguration &= ~(kTxMACConfiguration_Ignore_Collisions | kTxMACConfiguration_Ignore_Carrier_Sense);
		fXIFConfiguration |= kXIFConfiguration_Disable_Echo;
	}

	WRITE_REGISTER( TxMACConfiguration,	fTxMACConfiguration );
	WRITE_REGISTER( XIFConfiguration,	fXIFConfiguration );
	return;
}/* end setDuplexMode */


void UniNEnet::restartTransmitter()
{
	UInt32		gemReg;


    transmitQueue->stop();	/* keep other threads from calling outputPacket	*/

	ELG( READ_REGISTER( TxKick ), READ_REGISTER( TxCompletion ), 'TxKC', "UniNEnet::restartTransmitter" );
	ELG( READ_REGISTER( TxFIFOSize ), READ_REGISTER( TxFIFOPacketCounter ), 'TxPc', "UniNEnet::restartTransmitter" );
	ELG( READ_REGISTER( TxFIFOShadowReadPointer ), READ_REGISTER( TxFIFOReadPointer ), 'TxRp', "UniNEnet::restartTransmitter" );
	ELG( READ_REGISTER( TxMACStatus ), READ_REGISTER( MACControlStatus ), 'TM S', "UniNEnet::restartTransmitter" );
	ELG( READ_REGISTER( MIFStateMachine ), READ_REGISTER( TxConfiguration ), 'MiCf', "UniNEnet::restartTransmitter" );

	ALRT( READ_REGISTER( TxStateMachine ), READ_REGISTER( StateMachine ), '-Tx-', "UniNEnet::restartTransmitter - transmitter appeared to be hung." );

		/* Stop the Tx DMA engine:	*/

	WRITE_REGISTER( TxConfiguration, 0 );
	IOSleep( 1 );

		/* Stop the Tx MAC engine:	*/

	WRITE_REGISTER( TxMACConfiguration, 0 );	/* clear kTxMACConfiguration_TxMac_Enable	*/
	IOSleep( 4 );

		/* Reset the Tx MAC engine:	*/

	WRITE_REGISTER( TxMACSoftwareResetCommand, kTxMACSoftwareResetCommand_Reset );
	IOSleep( 1 );

	WRITE_REGISTER( SoftwareReset, kSoftwareReset_TX );
    do
    {		/// ??? put a time limit here.
		IOSleep( 1 );
		gemReg = READ_REGISTER( SoftwareReset );
    } 
	while( gemReg & kSoftwareReset_TX );

	initTxRing();					/* clear out any packets hanging about in the ring	*/

		/* Start the Tx DMA engine:	*/

	fTxConfiguration |= kTxConfiguration_Tx_DMA_Enable;
	WRITE_REGISTER( TxConfiguration, fTxConfiguration );

		/* Restore the flow control pause bits:	*/

	WRITE_REGISTER( MACControlConfiguration, fMACControlConfiguration );

		/* Start the Tx MAC engine:	*/

	WRITE_REGISTER( TxMACConfiguration, fTxMACConfiguration );

	transmitQueue->start();		/* allow threads to resume outputPacket			*/
//	transmitQueue->service();	/* Make sure the output queue is not stalled	*/
	return;
}/* end restartTransmitter */


void UniNEnet::restartReceiver()
{
	UInt16		i, u16;
	UInt32		x;


	ELG( 0, READ_REGISTER( StatusAlias ), 'StsA', "UniNEnet::restartReceiver" );
	ELG( READ_REGISTER( RxKick ), READ_REGISTER( RxCompletion ), 'RxKC', "UniNEnet::restartReceiver" );
	ELG( READ_REGISTER( RxConfiguration ), READ_REGISTER( RxFIFOReadPointer ), 'RxCR', "UniNEnet::restartReceiver" );
	ELG( READ_REGISTER( RxFIFOShadowWritePointer ), READ_REGISTER( RxFIFOWritePointer ), 'RxWp', "UniNEnet::restartReceiver" );
	ELG( 0, READ_REGISTER( RxFIFOPacketCounter ), 'RxPC', "UniNEnet::restartReceiver" );
	ALRT( READ_REGISTER( RxStateMachine ), READ_REGISTER( StateMachine ), '=SMs', "UniNEnet::restartReceiver" );

		// Perform software resets to the receiver.
		// The MAC config register should be re-programmed following
		// the reset. Everything else *should* be unaffected. Tain't so:
		// the MACControlConfiguration, 0x6038, gets its Pause enable bit reset.


	fRxConfiguration &= ~kRxConfiguration_Rx_DMA_Enable;		/* Stop the DMA	*/
	WRITE_REGISTER( RxConfiguration, fRxConfiguration );
	IOSleep( 1 );				/* let it finish any frame in progress	*/

		/* Do a global Rx reset:	*/

	WRITE_REGISTER( SoftwareReset, kSoftwareReset_RX );
	for ( i = 1000; i; --i )
	{	IODelay( 1 );
		x = READ_REGISTER( SoftwareReset );
		if ( (x & kSoftwareReset_RX) == 0 )
			break;
	}
	if ( i == 0 )
		ALRT( 0, x, '-SR-', "UniNEnet::restartReceiver - timeout on SoftwareReset" );

		/* Do a MAC Rx reset;  poll until the reset bit	*/
		/* is cleared by the hardware:					*/

	WRITE_REGISTER( RxMACSoftwareResetCommand, kRxMACSoftwareResetCommand_Reset );
    for ( i = 0; i < 5000; i++ )
    {
		if ( (READ_REGISTER( RxMACSoftwareResetCommand )
				& kRxMACSoftwareResetCommand_Reset) == 0 )
        {
            break;	// 'i' is always 0 or 1
        }
        IODelay( 1 );
    }/* end FOR */

	    // Update the MAC Config register. Watch out for the programming
	    // restrictions documented in the GEM specification!!!

	    // Disable MAC before setting any other bits in the MAC config register.

	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration & ~kRxMACConfiguration_Rx_Mac_Enable );

    for ( i = 0; i < 5000; i++ )
    {
		if ( (READ_REGISTER( RxMACConfiguration )
				& kRxMACConfiguration_Rx_Mac_Enable) == 0 )
        {
            break;	// 'i' is always 0
        }
        IODelay( 1 );
    }/* end FOR */

		/* Ensure all Rx ring elements have xfer count and my ownership:	*/

	u16 = OSSwapHostToLittleConstInt16( NETWORK_BUFSIZE | kGEMRxDescFrameSize_Own );
	for ( i = 0; i < fRxRingElements; ++i )
		fRxDescriptorRing[ i ].frameDataSize = u16;

	WRITE_REGISTER( RxBlanking,			fRxBlanking );
	WRITE_REGISTER( PauseThresholds,	fPauseThresholds );

	rxCommandHead = 0;
	WRITE_REGISTER( RxKick, fRxRingElements - 4 );

	WRITE_REGISTER( MACControlConfiguration, fMACControlConfiguration );// Pause etc bits

		// Restore (re-enable Rx MAC) MAC config register.

	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );

		/* Start the Rx DMA again:	*/

	fRxConfiguration |= kRxConfiguration_Rx_DMA_Enable;
	WRITE_REGISTER( RxConfiguration, fRxConfiguration );

///	fpELG->evLogFlag = 0;	/// stop event logging so we can see what lead up to this problem.
	return;
}/* end restartReceiver */


bool UniNEnet::transmitPacket( mbuf_t packet )
{
	TxDescriptor	*dp;				// descriptor pointer
	mbuf_t			m;
	UInt32			i, j, k;
	UInt32			segCount = 1;		// count starts on 1st element in mbuf chain
	UInt32			length;
	addr64_t		dataPhys;
	UInt32			demandMask;
	UInt16			param0, param1;
	bool			needSegs = true;	// may need MbufCursor physical segments
	UInt32			txElementsAvail;

	txElementsAvail = (txCommandHead + fTxRingElements - txCommandTail - 1) & (fTxRingElements-1);
	for ( m = packet; mbuf_next( m ); m = mbuf_next( m ) )
		segCount++;

	ELG( READ_REGISTER( TxCompletion ), txElementsAvail << 16 | segCount, '  Tx', "UniNEnet::transmitPacket" );

	if ( segCount > txElementsAvail )
	{
		ELG( txElementsAvail << 16 | segCount, packet, ' Tx-', "UniNEnet::transmitPacket - not enough elements avail." );
		if ( segCount < (fTxRingElements - 2) )
			return false;		// Tell the Family to stuff it.

		ELG( txElementsAvail << 16 | segCount, packet, 'Tx--', "UniNEnet::transmitPacket - way too many elements req'd." );
		freePacket( packet );	// Pretend we Tx'd it just to get rid of it and
		return true;			// prevent the transmitter from being permanently blocked.
								/// Should return kIOReturnOutputDropped from outputPacket.
	}

	k = j = txCommandTail;

	fTxIntCnt += segCount;

	m = packet;

	for ( i = 0; i < segCount; i++ )
	{
		k = j;		// k will be the index to the last element on loop exit

		length		= mbuf_len( m );
		dataPhys	= (addr64_t)mbuf_data_to_physical( mbuf_data( m ) );
		if ( dataPhys == 0 )
		{		/* AppleTalk printing, for example, will come through here.	*/
				/* txDebuggerPkt did until bcopy of data.					*/
			if ( needSegs )
			{	needSegs = false;
				segCount = fTxMbufCursor->getPhysicalSegments( packet, fTxSegment, MAX_SEGS_PER_TX_MBUF );
			}
			ELG( segCount, i, 'mbp-', "UniNEnet::transmitPacket - mbuf segment lacking physical address" );
			dataPhys	= (addr64_t)fTxSegment[ i ].location;
			length		= fTxSegment[ i ].length;
			ELG( length, dataPhys, 'LocL', "UniNEnet::transmitPacket - length and location." );
		}

		dp = &fTxDescriptorRing[ j ];
		OSWriteLittleInt64( dp, offsetof( TxDescriptor, bufferAddr ), dataPhys );
		OSWriteLittleInt32( dp, offsetof( TxDescriptor, flags0     ), length );
		dp->flags1 = 0;
		j = (j + 1) & (fTxRingElements - 1);
		m = mbuf_next( m );
	}/* end FOR each segment */

	fTxMbuf[ k ] = packet;		// save the packet mbuf address in the last segment's index

	fTxDescriptorRing[ k ].flags0             |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags0_EndOfFrame );
	fTxDescriptorRing[ txCommandTail ].flags0 |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags0_StartOfFrame );
	if ( fTxIntCnt >= TX_DESC_PER_INT )
	{
		fTxIntCnt = 0;
		fTxDescriptorRing[ txCommandTail ].flags1 |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags1_Int );
	}

	getChecksumDemand(	packet,
						kChecksumFamilyTCPIP,
						&demandMask,			// this chip supports only TCP checksumming
						&param0,				// start offset
						&param1 );				// stuff offset
	if ( demandMask & kChecksumTCPSum16 )
	{
		fTxDescriptorRing[ txCommandTail ].flags0
			|= OSSwapHostToLittleConstInt32(	kGEMTxDescFlags0_ChecksumEnable
											|	param0 << kGEMTxDescFlags0_ChecksumStart_Shift
											|	param1 << kGEMTxDescFlags0_ChecksumStuff_Shift );
	}

	txCommandTail = j;
	WRITE_REGISTER( TxKick, j );

	return true;          
}/* end transmitPacket */


	/*-------------------------------------------------------------------------
	 * receivePacket
	 * --------------
	 * This routine runs the receiver in polled-mode (yuk!) for the kernel debugger.
	 * Don't mess with the interrupt source here that can deadlock in the debugger
	 *
	 * The _receivePackets allocate MBufs and pass them up the stack. The kernel
	 * debugger interface passes a buffer into us. To reconcile the two interfaces,
	 * we allow the receive routine to continue to allocate its own buffers and
	 * transfer any received data to the passed-in buffer. This is handled by 
	 * receivePacket calling _packetToDebugger.
	 *-------------------------------------------------------------------------*/

void UniNEnet::receivePacket( void *pkt, UInt32 *pkt_len, UInt32 timeout )
{
	mach_timespec_t		startTime, currentTime;
	UInt32				elapsedTimeMS;

//  ELG( timeout, pkt, 'kdRx', "receivePacket - kernel debugger routine" );

	*pkt_len = 0;

	if ( fReady == false )
		return;

#if USE_ELG
	UInt32		elgFlag = fpELG->evLogFlag;	// if kernel debugging, turn off ELG
	fpELG->evLogFlag = 0;
#endif // USE_ELG

	debuggerPkt     = pkt;		// this points to the data buffer not an mbuf.
	debuggerPktSize = 0;

	IOGetTime( &startTime );
	do
	{
		receivePackets( true );

		IOGetTime( &currentTime );
		if ( currentTime.tv_nsec < startTime.tv_nsec )
		{
			currentTime.tv_nsec += 1000000000; // a billion here, a billion there
			currentTime.tv_sec  -= 1;
		}
		elapsedTimeMS  = (currentTime.tv_sec  - startTime.tv_sec)  * 1000;
		elapsedTimeMS += (currentTime.tv_nsec - startTime.tv_nsec) / 1000000;
	}
	while ( (debuggerPktSize == 0) && (elapsedTimeMS < timeout) );

    *pkt_len = debuggerPktSize;
#if USE_ELG
	fpELG->evLogFlag = elgFlag;
#endif // USE_ELG

    return;
}/* end receivePacket */


	/*-------------------------------------------------------------------------
	 * packetToDebugger
	 * -----------------
	 * This is called by _receivePackets when we are polling for kernel debugger
	 * packets. It copies the mbuf contents to the buffer passed by the debugger.
	 * It also sets the var debuggerPktSize which will break the polling loop.
	 *-------------------------------------------------------------------------*/

void UniNEnet::packetToDebugger( mbuf_t packet, u_int size )
{
//	ELG( packet, size, 'ToDb', "packetToDebugger" );

    debuggerPktSize = size;
    bcopy( (char*)mbuf_data( packet ), debuggerPkt, size );
	return;
}/* end packetToDebugger */


	/*-------------------------------------------------------------------------
	 * sendPacket
	 * -----------
	 *
	 * This routine runs the transmitter in polled-mode (yuk!) for the kernel debugger.
	 * Don't mess with the interrupt source here that can deadlock in the debugger
	 *
	 *-------------------------------------------------------------------------*/

void UniNEnet::sendPacket( void *pkt, UInt32 pkt_len )
{
	mach_timespec_t		startTime, currentTime;
	UInt32				elapsedTimeMS;


//	ELG( pkt, pkt_len, 'kdTx', "sendPacket" );

    if ( !fReady || !pkt || (pkt_len > (fMaxFrameSize - kIOEthernetCRCSize)) )
        return;

#if USE_ELG
	UInt32		elgFlag = fpELG->evLogFlag;	// if kernel debugging, turn off ELG
	fpELG->evLogFlag = 0;
#endif // USE_ELG

			/* Wait for the transmit ring to empty	*/
    IOGetTime( &startTime ); 
	do
	{   
		debugTransmitInterruptOccurred();
		IOGetTime( &currentTime );
		if ( currentTime.tv_nsec < startTime.tv_nsec )
		{
			currentTime.tv_nsec += 1000000000; // a billion here, a billion there
			currentTime.tv_sec  -= 1;
		}
		elapsedTimeMS  = (currentTime.tv_sec  - startTime.tv_sec)  * 1000;
		elapsedTimeMS += (currentTime.tv_nsec - startTime.tv_nsec) / 1000000;
	}
	while ( (txCommandHead != txCommandTail) && (elapsedTimeMS < TX_KDB_TIMEOUT) ); 
    
    if ( txCommandHead != txCommandTail )
    {
      ALRT( txCommandHead, txCommandTail, 'db1-', "UniNEnet::sendPacket - Polled transmit timeout - 1." );
      return;
    }

		/* Recycle the same buffer dedicated to KDB transmit.	*/

	bcopy( pkt, mbuf_data( txDebuggerPkt ), pkt_len );
	mbuf_setlen( txDebuggerPkt, pkt_len );
	mbuf_pkthdr_setlen( txDebuggerPkt, pkt_len );

		/* Send the debugger packet.								*/
		/* txDebuggerPkt must not be freed by the transmit routine.	*/
    transmitPacket( txDebuggerPkt );

		/* Poll waiting for the transmit ring to empty again:	*/
    do 
    {
        debugTransmitInterruptOccurred();
		IOGetTime( &currentTime );
		if ( currentTime.tv_nsec < startTime.tv_nsec )
		{
			currentTime.tv_nsec += 1000000000; // a billion here, a billion there
			currentTime.tv_sec  -= 1;
		}
		elapsedTimeMS  = (currentTime.tv_sec  - startTime.tv_sec)  * 1000;
		elapsedTimeMS += (currentTime.tv_nsec - startTime.tv_nsec) / 1000000;
    }
    while ( (txCommandHead != txCommandTail) && (elapsedTimeMS < TX_KDB_TIMEOUT) );

	if ( txCommandHead != txCommandTail )
		ALRT( txCommandHead, txCommandTail, 'db2-', "UniNEnet::sendPacket - Polled transmit timeout - 2." );

#if USE_ELG
	fpELG->evLogFlag = elgFlag;
#endif // USE_ELG

	return;
}/* end sendPacket */


bool UniNEnet::receivePackets( bool debuggerParam )
{
	mbuf_t		packet;
	UInt32		i, last, loopLimit;
	int			receivedFrameSize, maxFrameSize;
	int			cksumOffset;
	UInt16		dmaFlags;
	UInt16		checksum;
	UInt32		rxCompletion;
	UInt32		rxPktStatus = 0;
	UInt8		*pb;				/* a pointer to a byte	*/

	bool		reusePkt;
	bool		useNetif = !debuggerParam && netifEnabled;
	bool		packetsQueued = false;
	bool		replaced;
	bool		vlanPacket;


    last			= (UInt32)-1;  
    i				= rxCommandHead;
	rxCompletion	= READ_REGISTER( RxCompletion );
//	ELG( rxCompletion, i, 'Rx I', "UniNEnet::receivePackets" );

	for ( loopLimit = fRxRingElements; loopLimit; --loopLimit )
    {
		dmaFlags = OSReadLittleInt16( &fRxDescriptorRing[ i ].frameDataSize, 0 );

			/*  If the current entry has not been written, then stop at this entry	*/

		if ( dmaFlags & kGEMRxDescFrameSize_Own )
			break;

            /* Radar 2999214 - the DMA's PCI transaction to update the		*/
            /* Rx Descriptor is not atomic on the memory bus. The Own bit	*/
            /* and byte count get written first and then the buffer address.*/
            /* If the driver processes a packet based on the Own bit		*/
            /* and the packet is a replacement requiring a new buffer		*/
            /* address, the new address can under the right circumstances	*/
            /* be overwritten by the now stale address from the DMA engine.	*/
			/* The solution is to double check with the RxCompletion		*/
			/* register to make sure we own the packet.						*/

        if ( i == rxCompletion )
        {		// Refresh the completion number only when needed:
            rxCompletion = READ_REGISTER( RxCompletion );
            if ( i == rxCompletion )			// If packet still not ours,
                break;							// get it on the next interrupt.
        }

        reusePkt = false;

		checksum = OSReadLittleInt16( &fRxDescriptorRing[ i ].tcpPseudoChecksum, 0 );

        receivedFrameSize	= dmaFlags & kGEMRxDescFrameSize_Mask;
		rxPktStatus			= OSReadLittleInt32( &fRxDescriptorRing[ i ].flags, 0 );

	///	ELG( rxPktStatus, receivedFrameSize, 'Rx P', "UniNEnet::receivePackets - rx'd packet" );
		ELG( READ_REGISTER( RxCompletion ), receivedFrameSize, 'Rx P', "UniNEnet::receivePackets - rx'd packet" );

			/* Reject packets that are runts or that have other mutations.	*/

		vlanPacket		= FALSE;
		cksumOffset		= kRxHwCksumStartOffset;
		maxFrameSize	= kIOEthernetMaxPacketSize;

			/* Look at the packet type to see if a VLAN packet:	*/
		pb = (UInt8*)mbuf_data( fRxMbuf[ i ] );
		if ( (pb[ 12 ] == 0x81) && (pb[ 13 ] == 0x00)  )	// pass 12 Src/Dst addr bytes
		{
			vlanPacket = TRUE;
			maxFrameSize += 4;
			cksumOffset  -= 4;
			ELG( 0, receivedFrameSize, 'vlan', "UniNEnet::receivePackets - rx'd VLAN packet" );
		}
			/* Check for badness:	*/
		if ( receivedFrameSize > maxFrameSize
		  || receivedFrameSize < (kIOEthernetMinPacketSize - kIOEthernetCRCSize)
		  || rxPktStatus & kGEMRxDescFlags_BadCRC )
		{
            reusePkt = true;
			NETWORK_STAT_ADD( inputErrors );
			if ( receivedFrameSize < (kIOEthernetMinPacketSize - kIOEthernetCRCSize) )
				 ETHERNET_STAT_ADD( dot3RxExtraEntry.frameTooShorts );
			else ETHERNET_STAT_ADD( dot3StatsEntry.frameTooLongs );
			ELG( rxPktStatus, receivedFrameSize, '-Rx-', "receivePackets - mutant or bad CRC." );
        }
        else if ( useNetif == false )
        {
				/* Always reuse packets in debugger mode. We also refuse
				 * to pass anything up the stack unless the driver is open.
				 * The hardware is enabled before the stack has opened us
				 * to allow earlier debug interface registration.
				 * But we must not pass any packets up.
				 */
			reusePkt = true;
			if ( debuggerParam )
			{	ELG( i, receivedFrameSize, 'RxDb', "receivePackets - got a debugger packet" );
				packetToDebugger( fRxMbuf[ i ], receivedFrameSize );
			}
        }
        
 
			/* Before we pass this packet up the networking stack, Make sure we	*/
			/* can get a replacement. Otherwise, hold on to the current packet	*/
			/* and increment the input error count. Thanks Justin!				*/

        packet = 0;

        if ( reusePkt == false )
        {
            packet = replaceOrCopyPacket( &fRxMbuf[i], receivedFrameSize, &replaced );

            reusePkt = true;

			if ( packet && replaced )
			{
				genRxDescriptor( i );
				reusePkt = false;
			}

			if ( packet == 0 )
			{		// Can get here if Tx is spewing UDP packets and
					// uses up the whole pool.
				NETWORK_STAT_ADD( inputErrors );
				if ( packetsQueued )
				{	 packetsQueued = 0;
					 networkInterface->flushInputQueue();
				}
				IOSleep( 1 );					// Allow the stack to process packets.
				loopLimit = fRxRingElements;	// Do a full ring since we just slept.
			//	ELG( READ_REGISTER( RxCompletion ), i, 'Pkt-', "UniNEnet::receivePackets - no mBuf available." );
				ELG( READ_REGISTER( RxFIFOPacketCounter ), READ_REGISTER( RxMACStatus ), 'Pkt-', "UniNEnet::receivePackets - no mBuf available." );
			}
        }/* end IF reusePkt is false */

			/* Install the new MBuf for the one we're about	*/
			/* to pass to the network stack					*/

        if ( reusePkt == true )
        {
			fRxDescriptorRing[i].flags         = 0;
			fRxDescriptorRing[i].frameDataSize = OSSwapHostToLittleConstInt16( NETWORK_BUFSIZE | kGEMRxDescFrameSize_Own );
        }

		if ( (i & 3) == 3 )		// only kick modulo 4
		{
			WRITE_REGISTER( RxKick, (i - 3) );
		}

        last = i;	/* Keep track of the last receive descriptor processed	*/
		i = (i + 1) & (fRxRingElements - 1);

        if ( debuggerParam )
            break;

			/* Transfer received packet to the network stack:	*/

        if ( packet )
        {
            KERNEL_DEBUG( DBG_UniN_RXCOMPLETE | DBG_FUNC_NONE, 
                			(int)packet, (int)receivedFrameSize, 0, 0, 0 );

			if ( (receivedFrameSize > 64) && (checksum != 0) && !fIsPromiscuous )
				setChecksumResult(	packet,
									kChecksumFamilyTCPIP,
									kChecksumTCPSum16,
									0,					// validMask
									checksum,			// param0: actual cksum
									cksumOffset );		// param1: 1st byte cksum'd
			ELG( packet, receivedFrameSize << 16 | last, 'RxP+', "UniNEnet::receivePackets - packet up." );
			networkInterface->inputPacket( packet, receivedFrameSize, true );
			NETWORK_STAT_ADD( inputPackets );
			packetsQueued = true;
        }/* end IF packet to send up */
	}/* end FOR loopLimit */

    if ( last != (UInt32)-1 )
        rxCommandHead = i;

    return packetsQueued;
}/* end receivePackets */


bool UniNEnet::transmitInterruptOccurred()
{
	ELG( txCommandHead, fTxCompletion, ' Tx+', "UniNEnet::transmitInterruptOccurred" );

	fTxIntCnt = 0;		// Reset coalescing Tx Interrupt count

	if( txCommandHead == fTxCompletion )
		return false;

	while ( txCommandHead != fTxCompletion )
	{
		KERNEL_DEBUG( DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE, fTxMbuf[ txCommandHead ], 0, 0, 0, 0 );

		if ( fTxMbuf[ txCommandHead ] )
		{
		//	ELG( txCommandHead, fTxMbuf[ txCommandHead ], 'TxPF', "UniNEnet::transmitInterruptOccurred - free the packet" );
			if ( fTxMbuf[ txCommandHead ] != txDebuggerPkt )
			{
				freePacket( fTxMbuf[ txCommandHead ], kDelayFree );
			}
			else
			{
				ELG( txCommandHead, fTxMbuf[ txCommandHead ], 'TxDb', "UniNEnet::transmitInterruptOccurred - recycling txDebuggerPkt" );
				txDebuggerPktInUse = false;
			}
			fTxMbuf[ txCommandHead ] = 0;
		}

		txCommandHead = (txCommandHead + 1) & (fTxRingElements - 1);
	}/* end WHILE */

	fpNetStats->outputPackets += releaseFreePackets();
	return true;
}/* end transmitInterruptOccurred */


void UniNEnet::debugTransmitInterruptOccurred()
{
	UInt32		i;


		// Set the debugTxPoll flag to indicate the debugger was active
		// and some cleanup may be needed when the driver returns to
		// normal operation.
	debugTxPoll = true;

	i = READ_REGISTER( TxCompletion );

	while ( i != txCommandHead )
	{
			/* Free the mbuf we just transmitted.
			 *
			 * If it is the debugger packet, just remove it from the ring.
			 * and reuse the same packet for the next sendPacket() request.
			 */
			 
			/* While in debugger mode, do not touch the mbuf pool.
			 * Queue any used mbufs to a local queue. This queue
			 * will get flushed after we exit from debugger mode.
			 *
			 * During continuous debugger transmission and
			 * interrupt polling, we expect only the txDebuggerPkt
			 * to show up on the transmit mbuf ring.
			 */

		KERNEL_DEBUG(	DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE,
						(int)fTxMbuf[ txCommandHead ],
						(int)fTxMbuf[ txCommandHead ]->m_pkthdr.len, 0, 0, 0 );

		if ( fTxMbuf[ txCommandHead ] )
		{
			if ( fTxMbuf[ txCommandHead ] != txDebuggerPkt )
				debugQueue->enqueue( fTxMbuf[ txCommandHead ] );	/// ??? no use kdelay on Joe's Q
			fTxMbuf[ txCommandHead ] = 0;
		}

		txCommandHead = (txCommandHead + 1) & (fTxRingElements - 1);
	}/* end WHILE */

    return;
}/* end debugTransmitInterruptOccurred */


	/* debugTransmitCleanup - this routine has no callers ??? */

void UniNEnet::debugTransmitCleanup()
{
		// Debugger was active, clear all packets in the debugQueue, and
		// issue a start(), just in case the debugger became active while the
		// ring was full and the output queue stopped. Since the debugger
		// does not restart the output queue, to avoid calling
		// semaphore_signal() which may reenable interrupts, we need to
		// make sure the output queue is not stalled after the debugger has
		// flushed the ring.
    
    debugQueue->flush();

    transmitQueue->start();
	return;
}/* end debugTransmitCleanup */


bool UniNEnet::genRxDescriptor( UInt32 i )
{
	addr64_t		physAddr;


	physAddr = mbuf_data_to_physical( mbuf_data( fRxMbuf[i] ) );
	OSWriteLittleInt64( &fRxDescriptorRing[i].bufferAddr,  0, physAddr );

	fRxDescriptorRing[i].frameDataSize = OSSwapHostToLittleConstInt16( NETWORK_BUFSIZE | kGEMRxDescFrameSize_Own );
	fRxDescriptorRing[ i ].flags = 0;

	return true;
}/* end genRxDescriptor */


void UniNEnet::monitorLinkStatus( bool firstPoll )
{
    UInt16          phyStatus;
    UInt16          linkStatus;
    UInt16          linkMode;
    UInt16          lpAbility, expansion;
    UInt16          phyStatusChange;
    bool            fullDuplex	= false;
    bool			clockWasOff	= false;
	UInt32          linkSpeed	= 0;
    IOMediumType    mediumType	= kIOMediumEthernetNone;
    IONetworkMedium	*medium;
	bool			rc = true;


	ELG( phyId, firstPoll, ' MLS', "monitorLinkStatus" );

    if ( firstPoll )
    {
		fPHYStatus	= 0;
		fLinkStatus	= kLinkStatusUnknown;
    }

    if ( fBuiltin && fCellClockEnabled == false )
    {		/* Ethernet cell clock is disabled,				*/
			/* temporarily enable it to get the phyStatus:	*/
		enableCellClock();
		clockWasOff = true;
    }

    if ( fBuiltin )
    {
		rc = miiReadWord( &phyStatus, MII_STATUS );
    }
    else 
    {		/* GEM card:	*/
		phyStatus = READ_REGISTER( PCSMIIStatus ) & 0x0000FFFF; // read latched value
		IODelay( 3 );
		if ( firstPoll )
			IOSleep( 5000 );	/// fake wait for AutoNegotiation
		phyStatus = READ_REGISTER( PCSMIIStatus ) & 0x0000FFFF;
    }

    if ( clockWasOff )			// if it was off in the first place,
		disableCellClock();		// turn it back off

	if ( rc == false || phyStatus == 0xFFFF )	// proceed no further if PHY is not responding
		return;

		// see if Link UP, autonegotiation complete bits changed:

	if ( fLoopback )	// fake link UP if in loopback
		phyStatus |= MII_STATUS_LINK_STATUS;

	phyStatusChange	= (fPHYStatus ^ phyStatus)
					& (MII_STATUS_LINK_STATUS | MII_STATUS_NEGOTIATION_COMPLETE);

	if ( fMediumType != kIOMediumEthernetAuto )
		phyStatusChange &= ~MII_STATUS_NEGOTIATION_COMPLETE;	// no longer interesting

	if ( firstPoll || phyStatusChange )
		 ELG( fPHYStatus, phyStatusChange, 'LChg', "UniNEnet::monitorLinkStatus - initing or link status changed." );
	else return;

	if ( firstPoll && fBuiltin )
	{
			// For the initial link status poll, wait a bit, then
			// re-read the status register to clear any latched bits.
			// Why wait? Well, the debugger can kick in shortly after
			// this function returns, and we want the duplex setting
			// on the MAC to match the PHY.

	///	miiReadWord( &phyStatus, MII_STATUS );	// we read this reg earlier - no need to reread
		miiReadWord( &phyStatus, MII_STATUS );
		if ( fLoopback )	// fake link UP if in loopback
			phyStatus |= MII_STATUS_LINK_STATUS;
	}

	if ( (phyStatus & MII_STATUS_LINK_STATUS)
	  && (firstPoll || (phyStatus & MII_STATUS_NEGOTIATION_COMPLETE)
					|| (fAutoNegotiate == false)) )
	{
		if ( !fCellClockEnabled )
			wakeUp( true );			/* the link is up, make sure the cell clock is enabled	*/

		if ( phyId == 0xFF )
		{
			linkSpeed  = 1000;
			fullDuplex = true;
			mediumType = kIOMediumEthernet1000BaseSX;
		}
		else if ( (phyType & MII_LXT971_MASK) == MII_LXT971_ID )
		{
			miiReadWord( &linkStatus,  MII_LXT971_STATUS_2 );
			linkSpeed  = (linkStatus & MII_LXT971_STATUS_2_SPEED)  ?
						  100 : 10;
			fullDuplex = (linkStatus & MII_LXT971_STATUS_2_DUPLEX) ?
						  true : false;
			mediumType = (linkSpeed == 10)	? kIOMediumEthernet10BaseT
											: kIOMediumEthernet100BaseTX;
		}
		else if ( ((phyType & MII_BCM5201_MASK) == MII_BCM5201_ID)
			  ||  ((phyType & MII_BCM5221_MASK) == MII_BCM5221_ID)
			  ||  ((phyType & MII_BCM5241_MASK) == MII_BCM5241_ID) )
		{
			miiReadWord( &linkStatus,  MII_BCM5201_AUXSTATUS );
			linkSpeed  = (linkStatus & MII_BCM5201_AUXSTATUS_SPEED)  ? 100 : 10;
			fullDuplex = (linkStatus & MII_BCM5201_AUXSTATUS_DUPLEX) ? true : false;
			mediumType = (linkSpeed == 10) ? kIOMediumEthernet10BaseT : kIOMediumEthernet100BaseTX;
		}
		else if ( ((phyType & MII_BCM5400_MASK) == MII_BCM5400_ID)
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5401_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5411_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5421_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM54K2_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5462_ID) )	/// mlj temporary quick fix
		{
			miiReadWord( &linkStatus, MII_BCM5400_AUXSTATUS );

			linkMode	= (linkStatus & MII_BCM5400_AUXSTATUS_LINKMODE_MASK)
						/ MII_BCM5400_AUXSTATUS_LINKMODE_BIT;

			if ( linkMode < 6 )
				 fXIFConfiguration &= ~kXIFConfiguration_GMIIMODE;
			else fXIFConfiguration |=  kXIFConfiguration_GMIIMODE;
			WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );

			if ( linkMode == 0 )
			{
				linkSpeed = 0;
			}
			else if ( linkMode < 3 )
			{
				linkSpeed   =  10;
				fullDuplex  =  ( linkMode < 2 ) ? false : true; 
				mediumType  =  kIOMediumEthernet10BaseT;                   
			}
			else if ( linkMode < 6 )
			{
				linkSpeed   =  100;
				fullDuplex  =  ( linkMode < 5 ) ? false : true;
				mediumType  =  kIOMediumEthernet100BaseTX;  
			}
			else
			{
				linkSpeed   = 1000;
				fullDuplex  = true;
				mediumType  =  kIOMediumEthernet1000BaseT;
			}                    
		}
		else if ( ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID)		// 0x01410C2x
			   || ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_1)	// 0x01410C6x
			   || ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_2) )	// 0x01410CCx
		{
			miiReadWord( &linkStatus, MII_MARVELL_PHY_SPECIFIC_STATUS );

			if ( !(linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_RESOLVED) )
			{
				linkSpeed = 0;
				fXIFConfiguration &= ~kXIFConfiguration_GMIIMODE;
			}
			else if ( linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_1000 )
			{
				linkSpeed	= 1000;
				mediumType	= kIOMediumEthernet1000BaseT;
				fXIFConfiguration |=  kXIFConfiguration_GMIIMODE;
			}
			else if ( linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_100 )
			{
				linkSpeed	= 100;
				mediumType	= kIOMediumEthernet100BaseTX;  
				fXIFConfiguration &= ~kXIFConfiguration_GMIIMODE;
			}
			else
			{
				linkSpeed	= 10;
				mediumType	= kIOMediumEthernet10BaseT;                   
				fXIFConfiguration &= ~kXIFConfiguration_GMIIMODE;
			}

			WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );

			if ( linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_FULL_DUPLEX )
				 fullDuplex =  true;
			else fullDuplex =  false;
		}/* end ELSE IF Marvell */

		setDuplexMode( fullDuplex );   /* Set up the MAC's registers for duplex.	*/ 

			/* Enable/disable Pause flow control depending on Link Partner:	*/

		fMACControlConfiguration = 0;					/* Assume no Pause capability by LP	*/
		if ( fBuiltin )									/* Get Link Partner ability reg:	*/
			miiReadWord( &lpAbility, MII_LINKPARTNER );	/* from PHY or 						*/
		else											/* GEM fiber optic card register.	*/
			lpAbility = READ_REGISTER( PCSMIILinkPartnerAbility ) & 0x0000FFFF;
		if ( (lpAbility & MII_LPAR_PAUSE) && isFullDuplex )
		{
			fMACControlConfiguration	= kMACControlConfiguration_Send_Pause_Enable
										| kMACControlConfiguration_Receive_Pause_Enable;
			mediumType |= kIOMediumOptionFlowControl;
		}
		WRITE_REGISTER( MACControlConfiguration, fMACControlConfiguration );

		if ( fReady == true )
			startChip();

		if ( linkSpeed != 0 )
		{
			mediumType |= (fullDuplex == true)
						? kIOMediumOptionFullDuplex
						: kIOMediumOptionHalfDuplex;
		}

		medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );

		setLinkStatus( kIONetworkLinkActive | kIONetworkLinkValid,
					   medium,
					   linkSpeed * 1000000 );
		ELG( kIONetworkLinkActive | kIONetworkLinkValid, linkSpeed * 1000000, 'SLS+', "UniNEnet::monitorLinkStatus -  setLinkStatus." );

		miiReadWord( &expansion, MII_EXPANSION );	/* get LP AutoNegotiation Ability et al bits	*/
		IOLog( "UniNEnet::monitorLinkStatus - Link is up at %ld Mbps - %s Duplex (PHY regs 5,6:0x%04x,0x%04x)\n",
					linkSpeed, fullDuplex ? "Full" : "Half", lpAbility, expansion );

		fLinkStatus = kLinkStatusUp;
	}/* end IF link is up AND (1st poll or negotiation complete)	*/
	else
	{
		if ( fCellClockEnabled
		  && ((fLinkStatus == kLinkStatusUp) || (fLinkStatus == kLinkStatusUnknown)) )
		{
			stopChip();

			medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );

			setLinkStatus( kIONetworkLinkValid, medium, 0 );	// valid but not kIONetworkLinkActive
			ELG( kIONetworkLinkValid, medium, 'SLS-', "UniNEnet::monitorLinkStatus -  setLinkStatus." );

		    transmitQueue->flush();	/* Flush all packets currently in the output queue.	*/

			if ( fLinkStatus != kLinkStatusUnknown )
			   IOLog( "UniNEnet::monitorLinkStatus - Link is down.\n" );

			fTxIntCnt = 0;

			if ( txCommandHead != txCommandTail )
			{
				initTxRing();

				txCommandHead = READ_REGISTER( TxCompletion );
				txCommandTail = txCommandHead;
			}
		}
		fLinkStatus = kLinkStatusDown;
	}
	fPHYStatus = phyStatus;

	return;
}/* end monitorLinkStatus */


IOReturn UniNEnet::getHardwareAddress( IOEthernetAddress *ea )
{
    UInt32      i;
    OSData		*macEntry;
    UInt8       *macAddress;
    UInt32      len;


    macEntry = OSDynamicCast( OSData, nub->getProperty( "local-mac-address" ) );
    if ( macEntry == 0 )
        return kIOReturnError;

    macAddress  = (UInt8*)macEntry->getBytesNoCopy();
    if ( macAddress == 0 )
        return kIOReturnError;


    len = macEntry->getLength();
    if ( len != 6 )
        return kIOReturnError;
   
    for ( i = 0; i < sizeof( *ea ); i++ )
        ea->bytes[i] = macAddress[i];


    return kIOReturnSuccess;
}/* end getHardwareAddress */


IOReturn UniNEnet::setHardwareAddress( const IOEthernetAddress *ea )
{
	UInt16			*p16;
	unsigned int	i;
	bool			clkEnable = fCellClockEnabled;

	myAddress = *ea;			/* for WOL and wakeup	*/

	if ( !clkEnable )
		enableCellClock();

    p16 = (UInt16*)myAddress.bytes;
    for ( i = 0; i < sizeof( IOEthernetAddress ) / 2; i++ )
	{
		WRITE_REGISTER( MACAddress[ i ],		p16[ 2 - i ] );
		WRITE_REGISTER( MACAddress[ i + 3 ],	p16[ 2 - i ] );	// init the alternate MAC address
	}

	if ( !clkEnable )
		disableCellClock();

	ELG( p16[0], p16[1] << 16 | p16[2], 'SetA', "UniNEnet::setHardwareAddress" );

    return kIOReturnSuccess;
}/* end setHardwareAddress */

#define ENET_CRCPOLY 0x04c11db7

static UInt32 crc416( UInt32 current, UInt16 nxtval )
{
	register UInt32		counter;
	register int		highCRCBitSet, lowDataBitSet;


	nxtval = ((nxtval & 0x00FF) << 8) | (nxtval >> 8);	/* Swap bytes	*/


	for ( counter = 0; counter != 16; ++counter )		/* Compute bit-by-bit	*/
	{
		if ( (current & 0x80000000) == 0 )  			/* is high CRC bit set?	*/
			 highCRCBitSet = 0;
		else highCRCBitSet = 1;

		current = current << 1;

		if ( (nxtval & 0x0001) == 0 )
			 lowDataBitSet = 0;
		else lowDataBitSet = 1;

		nxtval = nxtval >> 1;

		if ( highCRCBitSet ^ lowDataBitSet )	/* do the XOR */
			current = current ^ ENET_CRCPOLY;
	}/* end FOR */
	return current;
}/* end crc416 */


static UInt32 mace_crc( UInt16 *address )
{   
    register UInt32		newcrc;


    newcrc = crc416( 0xFFFFFFFF, *address );  /* address bits 47 - 32 */
    newcrc = crc416( newcrc, address[1] );    /* address bits 31 - 16 */
    newcrc = crc416( newcrc, address[2] );    /* address bits 15 - 0  */

    return newcrc;
}/* end mace_crc */


	/* Add requested mcast addr to UniN's hash table filter. 	*/

void UniNEnet::addToHashTableMask( UInt8 *addr )
{   
    UInt32   i, j;
    UInt32   crcBitIndex;
    UInt16   mask;


    j = mace_crc( (UInt16*)addr ) & 0xFF;		/* Big-endian alert! */
   
    for ( crcBitIndex = i = 0; i < 8; i++ )
    {
        crcBitIndex >>= 1;
        crcBitIndex  |= (j & 0x80);
        j           <<= 1;
    }

    crcBitIndex ^= 0xFF;

    if ( hashTableUseCount[ crcBitIndex ]++ )
      return;           /* This bit is already set */
    mask = crcBitIndex % 16;
    mask = 1 << mask;
    hashTableMask[ crcBitIndex / 16 ] |= mask;
	return;
}/* end addToHashTableMask */


void UniNEnet::resetHashTableMask()
{
    bzero( hashTableUseCount, sizeof( hashTableUseCount ) );
    bzero( hashTableMask,     sizeof( hashTableMask ) );
	return;
}/* end resetHashTableMask */


	/* Sync the adapter with the software copy of	*/
	/* the multicast mask (logical address filter).	*/

void UniNEnet::updateHashTableMask()
{
    UInt32      i, x;


	ELG( this, fRxMACConfiguration, 'updH', "updateHashTableMask" );

	WRITE_REGISTER( RxMACConfiguration,
					fRxMACConfiguration & ~kRxMACConfiguration_Rx_Mac_Enable );
		/* Time limit for bits to clear is around 3.2 ms	*/
	for ( i = 0; i <= 3; ++i )
	{
		x = READ_REGISTER( RxMACConfiguration );
		if ( (x & (kRxMACConfiguration_Rx_Mac_Enable | kRxMACConfiguration_Hash_Filter_Enable) ) == 0 )
			break;
		IOSleep( 1 );
	}

    for ( i = 0; i < 16; i++ )
		WRITE_REGISTER( HashTable[ i ], hashTableMask[ 15 - i ] );

    fRxMACConfiguration |= kRxMACConfiguration_Hash_Filter_Enable;
	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );
	return;
}/* end updateHashTableMask */
