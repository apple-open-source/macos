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
#include "UniNEnet.h"
#include "UniNEnetMII.h"
#include <libkern/OSByteOrder.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOKitKeys.h>			// kIODeviceTreePlane

	extern void			*kernel_pmap;


/*
 * Private functions
 */
bool UniNEnet::allocateMemory()
{
	UInt32		rxRingSize, txRingSize;
//	IOReturn	rc;						// return code

 
		/* Allocate memory for DMA ring elements:	*/

    txRingSize = fTxRingElements * sizeof( TxDescriptor );
	fTxDescriptorRing = (TxDescriptor*)IOMallocContiguous(	txRingSize,
															PAGE_SIZE, 0 );
	if ( !fTxDescriptorRing )
	{
		ALRT( 0, txRingSize, '-Tx-', "UniNEnet::allocateMemory - failed to alloc Tx Ring" );
		return false;
	}
#ifdef UNNECESSARY
	rc = IOSetProcessorCacheMode(	kernel_task,
									(IOVirtualAddress)fTxDescriptorRing,
									txRingSize,
									kIOMapInhibitCache );
#endif // UNNECESSARY
	ELG( txRingSize, fTxDescriptorRing, '=TxR', "UniNEnet::allocateMemory - Tx Ring alloc'd" );


    rxRingSize = fRxRingElements * sizeof( RxDescriptor );
	fRxDescriptorRing = (RxDescriptor*)IOMallocContiguous(	rxRingSize,
															PAGE_SIZE, 0 );
	if ( !fRxDescriptorRing )
	{
		ALRT( 0, rxRingSize, '-Rx-', "UniNEnet::allocateMemory - failed to alloc Rx Ring" );
		return false;
	}
#ifdef UNNECESSARY
	rc = IOSetProcessorCacheMode(	kernel_task,
									(IOVirtualAddress)fRxDescriptorRing,
									rxRingSize,
									kIOMapInhibitCache );
#endif // UNNECESSARY
	ELG( rxRingSize, fRxDescriptorRing, '=RxR', "UniNEnet::allocateMemory - Rx Ring alloc'd" );

		/* set up the Tx and Rx mBuf pointer arrays:	*/

	fTxMbuf = (mbuf**)IOMalloc( sizeof( mbuf* ) * fTxRingElements );
	if ( !fTxMbuf )
	{
		ALRT( 0, 0, 'mpT-', "UniNEnet::allocateMemory - alloc Tx mbuf pointers failed" );
		return false;
	}
	bzero( fTxMbuf, sizeof( mbuf* ) * fTxRingElements );	// clear out all the fTxMbuf pointers

	fRxMbuf = (mbuf**)IOMalloc( sizeof( mbuf* ) * fRxRingElements );
	if ( !fRxMbuf )
	{
		ALRT( 0, 0, 'mpR-', "UniNEnet::allocateMemory - alloc Rx mbuf pointers failed" );
		return false;
	}
	bzero( fRxMbuf, sizeof( mbuf* ) * fRxRingElements );	// clear out all the fRxMbuf pointers

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


	ELG( this, fTxDescriptorRing, 'ITxR', "initTxRing" );

		/* Clear the transmit DMA command memory	*/

    bzero( (void*)fTxDescriptorRing, sizeof( TxDescriptor ) * fTxRingElements );
    txCommandHead = 0;
    txCommandTail = 0;
    
    fTxDescriptorRingPhys = pmap_extract( kernel_pmap, (vm_address_t)fTxDescriptorRing );

    if ( fTxDescriptorRingPhys == 0 )
    {
        IOLog( "UniNEnet::initTxRing - Bad dma command buf - %08x\n\r", (int)fTxDescriptorRing );
    }
 
	for ( i = 0; i < fTxRingElements; i++ )
	{
		if ( fTxMbuf[ i ] )
		{
			ELG( i, fTxMbuf[ i ], 'txpf', "UniNEnet::initTxRing - free the packet" );
			freePacket( fTxMbuf[ i ] );
			fTxMbuf[ i ] = 0;
        }
	}

    fTxElementsAvail = fTxRingElements - 1; 

    txIntCnt  = 0;
    txWDCount = 0;

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


	ELG( fRxMbuf, fRxDescriptorRing, 'IRxR', "initRxRing" );

		/* Clear the Rx DMA commands:	*/

	bzero( (void*)fRxDescriptorRing, sizeof( RxDescriptor ) * fRxRingElements );

    fRxDescriptorRingPhys = pmap_extract( kernel_pmap, (vm_address_t)fRxDescriptorRing );
    if ( fRxDescriptorRingPhys == 0 )
    {
        IOLog( "UniNEnet::initRxRing - Bad dma command buf - %08x\n\r",	(int)fRxDescriptorRing );
        return false;
    }

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
    rxCommandTail = i - 4;

    return true;
}/* end initRxRing */


void UniNEnet::flushRings( bool flushTx, bool flushRx )
{
	UInt32		i;


	ELG( fTxRingElements, fRxRingElements, 'FluR', "flushRings" );


	if ( flushTx )			// Free all mbufs from the transmit ring:
	{
		for ( i = 0; i < fTxRingElements; i++ )
		{
			if ( fTxMbuf[ i ] )
			{
				ELG( i, fTxMbuf[ i ], 'flTx', "UniNEnet::flushRings" );
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
				ELG( i, fRxMbuf[ i ], 'flRx', "UniNEnet::flushRings" );
				freePacket( fRxMbuf[ i ] );
				fRxMbuf[i] = 0;
			}
		}
	}
	return;
}/* end flushRings */


void UniNEnet::startChip()
{
    UInt32  gemReg;
  

	ELG( this, 0, 'SChp', "startChip" );

	gemReg	= READ_REGISTER( TxConfiguration );				// Tx DMA enable
	gemReg |= kTxConfiguration_Tx_DMA_Enable;
	WRITE_REGISTER( TxConfiguration, gemReg );

	gemReg	= READ_REGISTER( RxConfiguration );				// Rx DMA enable
	gemReg |= kRxConfiguration_Rx_DMA_Enable;
	WRITE_REGISTER( RxConfiguration, gemReg	 );

	gemReg	= READ_REGISTER( TxMACConfiguration );			// Tx MAC enable
	gemReg |= kTxMACConfiguration_TxMac_Enable;
	WRITE_REGISTER( TxMACConfiguration, gemReg  );

	fRxMACConfiguration	= kRxMACConfiguration_Rx_Mac_Enable;// Rx MAC enable
	if ( fIsPromiscuous )
		 fRxMACConfiguration |= kRxMACConfiguration_Promiscuous;
	else fRxMACConfiguration |= kRxMACConfiguration_Strip_FCS;
	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );

	gemReg = ~(kStatus_TX_INT_ME | kStatus_RX_DONE | kStatus_MIF_Interrupt);
	WRITE_REGISTER( InterruptMask, gemReg );

	return;
}/* end startChip */


void UniNEnet::stopChip()
{
    UInt32	gemReg;


	ELG( READ_REGISTER( TxConfiguration ), READ_REGISTER( RxConfiguration ), 'HChp', "stopChip" );

	gemReg	= READ_REGISTER( TxConfiguration );				// clear Tx DMA enable
	gemReg &= ~kTxConfiguration_Tx_DMA_Enable;
	WRITE_REGISTER( TxConfiguration, gemReg );

	gemReg	= READ_REGISTER( RxConfiguration );				// clear Rx DMA enable
	gemReg &= ~kRxConfiguration_Rx_DMA_Enable;
	WRITE_REGISTER( RxConfiguration, gemReg	 );

	IOSleep( 1 );	// Give time for DMAs to finish what they're doing.

	gemReg	= READ_REGISTER( TxMACConfiguration );			// clr Tx MAC enable
	gemReg &= ~kTxMACConfiguration_TxMac_Enable;
	WRITE_REGISTER( TxMACConfiguration, gemReg  );

    fRxMACConfiguration  = READ_REGISTER( RxMACConfiguration );	// clr Rx MAC enable
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
		{ kIOMediumEthernetNone                                  									,	0		},
		{ kIOMediumEthernetAuto                                  									,	0		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex									,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex		| kIOMediumOptionLoopback	,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex									,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback	,	10		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex									,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex		| kIOMediumOptionLoopback	,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex									,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback	,	100		}
	};


	static MediumTable gMediumTableGigabit[] =
	{
		{ kIOMediumEthernetNone                                  																	,	0		},
		{ kIOMediumEthernetAuto                                  																	,	0		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex																	,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionHalfDuplex		| kIOMediumOptionLoopback									,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex																	,	10		},
		{ kIOMediumEthernet10BaseT		| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback									,	10		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex																	,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionHalfDuplex		| kIOMediumOptionLoopback									,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex																	,	100		},
		{ kIOMediumEthernet100BaseTX	| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback									,	100		},
		{ kIOMediumEthernet1000BaseTX 	| kIOMediumOptionFullDuplex																	,	1000	},
		{ kIOMediumEthernet1000BaseTX 	| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback									,	1000	},
		{ kIOMediumEthernet1000BaseTX	| kIOMediumOptionFullDuplex									| kIOMediumOptionFlowControl	,	1000	},
		{ kIOMediumEthernet1000BaseTX	| kIOMediumOptionFullDuplex		| kIOMediumOptionLoopback	| kIOMediumOptionFlowControl	,	1000	}
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

	miiResetPHY( phyId );

	pPhyType = (UInt16*)&phyType;
	miiReadWord( pPhyType,   MII_ID0, phyId );
	miiReadWord( pPhyType+1, MII_ID1, phyId );

	setProperty( "PHY ID", phyType, sizeof( phyType ) * 8 );

	ELG( phyId, phyType, '=FyT', "UniNEnet::getPhyType" );

	if ( (phyType & MII_BCM5400_MASK) == MII_BCM5400_ID )
	{
		fPHYType = 0x5400;
		ELG( this, phyId, '5400', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5400" );

		miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL, phyId );
		phyWord |= MII_BCM5400_AUXCONTROL_PWR10BASET;
		miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL, phyId );
	  
		miiReadWord( &phyWord, MII_1000BASETCONTROL, phyId );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL, phyId );

		IODelay(100);   
					
		miiResetPHY( 0x1F );	/// ??? 0x1F --> phyId?

		miiReadWord( &phyWord, MII_BCM5201_MULTIPHY, 0x1F );
		phyWord |= MII_BCM5201_MULTIPHY_SERIALMODE;
		miiWriteWord( phyWord, MII_BCM5201_MULTIPHY, 0x1F );

		miiReadWord( &phyWord, MII_BCM5400_AUXCONTROL, phyId );
		phyWord &= ~MII_BCM5400_AUXCONTROL_PWR10BASET;
		miiWriteWord( phyWord, MII_BCM5400_AUXCONTROL, phyId );

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
		miiReadWord( &phyWord, MII_ID1, phyId );

			// check if this is the "B0" revision of the 5401 PHY...this will
			// help with the gigabit link establishment.
		phyWord &= 0x000F ;

		if ( (phyWord == 0x0001 ) || (phyWord == 0x0003) )
		{
			miiWriteWord( 0x0C20, 0x018, phyId );
			miiWriteWord( 0x0012, 0x017, phyId );
			miiWriteWord( 0x1804, 0x015, phyId );
			miiWriteWord( 0x0013, 0x017, phyId );
			miiWriteWord( 0x1204, 0x015, phyId );
			miiWriteWord( 0x8006, 0x017, phyId );
			miiWriteWord( 0x0132, 0x015, phyId );
			miiWriteWord( 0x8006, 0x017, phyId );
			miiWriteWord( 0x0232, 0x015, phyId );
			miiWriteWord( 0x201F, 0x017, phyId );
			miiWriteWord( 0x0A20, 0x015, phyId );
		}

		miiReadWord( &phyWord, MII_1000BASETCONTROL, phyId );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL, phyId );

		IODelay( 10 );

		miiResetPHY( 0x1F );	/// ??? 0x1F --> phyId?

		miiReadWord( &phyWord, MII_BCM5201_MULTIPHY, 0x1F );
		phyWord |= MII_BCM5201_MULTIPHY_SERIALMODE;
		miiWriteWord( phyWord, MII_BCM5201_MULTIPHY, 0x1F );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end else IF 5401 */
	else if ( (phyType & MII_BCM5400_MASK) == MII_BCM5411_ID )	// 5411:
	{
		fPHYType = 0x5411;
		ELG( this, phyId, '5411', "UniNEnet::getPhyType - Broadcom 5411" );
		setProperty( "PHY type", "Broadcom 5411" );

		miiWriteWord( 0x8C23, 0x01C, phyId );	// setting some undocumented voltage
		miiWriteWord( 0x8CA3, 0x01C, phyId );
		miiWriteWord( 0x8C23, 0x01C, phyId );

		miiWriteWord( 0x8000, 0x000, phyId );	// reset PHY (MII_CONTROL_RESET, MII_CONTROL)

		miiWriteWord( 0x1340, 0x000, phyId );	// advertise gigabit, full-duplex

		miiReadWord( &phyWord, MII_1000BASETCONTROL, phyId );
		phyWord |= MII_1000BASETCONTROL_FULLDUPLEXCAP;
		miiWriteWord( phyWord, MII_1000BASETCONTROL, phyId );

		IODelay( 10 );

		miiResetPHY( 0x1F );	/// ??? 0x1F --> phyId?

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end IF 5411 */
	else if ( ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID)		// 0x01410C2x
	       || ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_1) )	// 0x01410C6x
	{
		fPHYType = 0x1011;
		ELG( this, phyId, '1011', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Marvell" );

		fpgMediumTable		= gMediumTableGigabit;
		fMediumTableCount	=  sizeof( gMediumTableGigabit ) / sizeof( MediumTable ) ;
	}/* end else IF Marvell */
	else if ( (phyType & MII_BCM5201_MASK) == MII_BCM5201_ID )
	{
		fPHYType = 0x5201;
		ELG( this, phyId, '5201', "UniNEnet::getPhyType" );
		setProperty( "PHY type", "Broadcom 5201" );

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


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::initChip()
{
    UInt32          	i, temp;
    mach_timespec_t		timeStamp;
    UInt32          	rxFifoSize;
    UInt32          	rxOff, rxOn;
    UInt32          	ui32;
    UInt16       		*p16;
	UInt16				phyAdrToPoll, phyRegToPoll, phyBitsToMonitor;
	UInt16				val16;


	ELG( 0, phyId, 'ChpI', "initChip" );

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
										  | kXIFConfiguration_FDPLXLED;
    }
    else
    {
		WRITE_REGISTER( DatapathMode, kDatapathMode_GMIIMode );
		fXIFConfiguration = kXIFConfiguration_Tx_MII_OE;
   }
	WRITE_REGISTER( XIFConfiguration, fXIFConfiguration );

	WRITE_REGISTER( SendPauseCommand,		kSendPauseCommand_default );
	WRITE_REGISTER( MACControlConfiguration, 0 );
	WRITE_REGISTER( InterruptMask,			kInterruptMask_None );
	WRITE_REGISTER( TxMACMask,				kTxMACMask_default );
	WRITE_REGISTER( RxMACMask,				kRxMACMask_default );
	WRITE_REGISTER( MACControlMask,			kMACControlMask_default );

	fConfiguration	= kConfiguration_TX_DMA_Limit		// default Configuration value
					| kConfiguration_RX_DMA_Limit
					| kConfiguration_Infinite_Burst;
	WRITE_REGISTER( Configuration, fConfiguration );	// try the default

	ui32 = READ_REGISTER( Configuration );				// read it back
    if ( (ui32 & kConfiguration_Infinite_Burst) == 0 )	
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
	WRITE_REGISTER( MACControlType,		kMACControlType_default );

    p16 = (UInt16*)myAddress.bytes;
    for ( i = 0; i < sizeof( IOEthernetAddress ) / 2; i++ )
		WRITE_REGISTER( MACAddress[ i ], p16[ 2 - i ] );

    for ( i = 0; i < 3; i++ )
    {
		WRITE_REGISTER( MACAddress[ i + 3 ],	0 );
		WRITE_REGISTER( AddressFilter[ i  ],	0 );
    }

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

	temp	= kTxConfiguration_TxFIFO_Threshold
			| fTxRingLengthFactor << kTxConfiguration_Tx_Desc_Ring_Size_Shift;
	WRITE_REGISTER( TxConfiguration, temp );

	WRITE_REGISTER( TxMACConfiguration, 0 );
	IOSleep( 4 );		/// Wait or poll for enable to clear.

    setDuplexMode( (phyId == 0xff) ? true : false );
   
	WRITE_REGISTER( RxDescriptorBaseLow,	fRxDescriptorRingPhys );
	WRITE_REGISTER( RxDescriptorBaseHigh,	0 );

	WRITE_REGISTER( RxKick, fRxRingElements - 4 );

	temp	= kRxConfiguration_RX_DMA_Threshold
	//		| kRxConfiguration_Batch_Disable	may cause 4x primary interrupts
			| fRxRingLengthFactor << kRxConfiguration_Rx_Desc_Ring_Size_Shift
			| kRxConfiguration_Checksum_Start_Offset;
	WRITE_REGISTER( RxConfiguration, temp );

	fRxMACConfiguration = 0;
	WRITE_REGISTER( RxMACConfiguration,	fRxMACConfiguration );
	IOSleep( 4 ); 		// it takes time to clear the enable bit

		/* Set flow control pause thresholds:								*/
		/* Pause off when within 2 max packets of FIFO full.				*/
		/* Pause on  when FIFO drops to almost 1 max packet of FIFO empty.	*/

	rxFifoSize	= READ_REGISTER( RxFIFOSize );	// 64-byte (kPauseThresholds_Factor) chunks

    rxOff  = rxFifoSize - (kMaxFrameSize_default * 2 / kPauseThresholds_Factor);
    rxOn   = kMaxFrameSize_default / kPauseThresholds_Factor + 1;

	WRITE_REGISTER( PauseThresholds,
					  (rxOff << kPauseThresholds_OFF_Threshold_Shift)
					| (rxOn	 << kPauseThresholds_ON_Threshold_Shift) );

	temp = READ_REGISTER( BIFConfiguration );
	if ( temp & kBIFConfiguration_M66EN )
		 temp = kRxBlanking_default_66;
	else temp = kRxBlanking_default_33;
	WRITE_REGISTER( RxBlanking, temp );

	if ( fPHYType == 0x5221 )
	{
		phyAdrToPoll	 = kPHYAddr0;				/// ??? kPHYAddr0 ---> phyId
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

	if ( fBuiltin )
		WRITE_REGISTER( WOLWakeupCSR, 0 );	// Disable Wake on LAN

	ELG( READ_REGISTER( TxFIFOSize ), READ_REGISTER( RxFIFOSize ), 'FFsz', "initChip - done" );

    return true;
}/* end initChip */


void UniNEnet::setDuplexMode( bool duplexMode )
{
	UInt32		txMacConfig;


	isFullDuplex	= duplexMode;
	txMacConfig		= READ_REGISTER( TxMACConfiguration );

	ELG( txMacConfig, duplexMode, 'DupM', "setDuplexMode" );

	WRITE_REGISTER( TxMACConfiguration, txMacConfig & ~kTxMACConfiguration_TxMac_Enable );
    while ( READ_REGISTER( TxMACConfiguration ) & kTxMACConfiguration_TxMac_Enable )
      ;	/// Set time limit of a couple of milliseconds.


    if ( isFullDuplex )
    {
		txMacConfig |= (kTxMACConfiguration_Ignore_Collisions | kTxMACConfiguration_Ignore_Carrier_Sense);
		fXIFConfiguration &= ~kXIFConfiguration_Disable_Echo;
    }
    else
    {
		txMacConfig &= ~(kTxMACConfiguration_Ignore_Collisions | kTxMACConfiguration_Ignore_Carrier_Sense);
		fXIFConfiguration |= kXIFConfiguration_Disable_Echo;
#ifdef HALF_DUPLEX_TESTING
txMacConfig |= kTxMACConfiguration_Never_Give_Up;
txMacConfig |= kTxMACConfiguration_Never_Give_Up_Limit;
///txMacConfig |= kTxMACConfiguration_Slow_Down;
///txMacConfig |= kTxMACConfiguration_Enable_IPG0;
///WRITE_REGISTER( InterPacketGap0, 0x10 );
WRITE_REGISTER( InterPacketGap1, 0x05 );
WRITE_REGISTER( InterPacketGap2, 0x20 );
#endif // HALF_DUPLEX_TESTING
	}

	WRITE_REGISTER( TxMACConfiguration,	txMacConfig );
	WRITE_REGISTER( XIFConfiguration,	fXIFConfiguration );
	return;
}/* end setDuplexMode */


void UniNEnet::restartReceiver()
{
	UInt16		i;
	UInt16		u16;


	ELG( 0, READ_REGISTER( StatusAlias ), 'Alas', "restartReceiver" );
	ELG( READ_REGISTER( TxKick ), READ_REGISTER( TxCompletion ), 'TxKC', "restartReceiver" );
	ELG( 0, READ_REGISTER( TxConfiguration ), 'TxCf', "restartReceiver" );
	ELG( READ_REGISTER( TxFIFOShadowReadPointer ), READ_REGISTER( TxFIFOReadPointer ), 'TxRp', "restartReceiver" );
	ELG( READ_REGISTER( TxFIFOSize ), READ_REGISTER( TxFIFOPacketCounter ), 'TxPc', "restartReceiver" );

	ELG( READ_REGISTER( RxKick ), READ_REGISTER( RxCompletion ), 'RxKC', "restartReceiver" );
	ELG( READ_REGISTER( RxStateMachine ), READ_REGISTER( RxConfiguration ), 'RxCf', "restartReceiver" );
	ELG( READ_REGISTER( RxFIFOShadowWritePointer ), READ_REGISTER( RxFIFOWritePointer ), 'RxWp', "restartReceiver" );
	ELG( READ_REGISTER( RxFIFOPacketCounter ), READ_REGISTER( RxFIFOPacketCounter ), 'RxPC', "restartReceiver" );
///	ELG( READ_REGISTER( xxxx ), READ_REGISTER( xxxx ), 'xxxx', "restartReceiver" );

		// Perform a software reset to the logic in the RX MAC.
		// The MAC config register should be re-programmed following
		// the reset. Everything else *should* be unaffected. Tain't so:
		// the MACControlConfiguration, 0x6038, gets its Pause enable bit reset.

	WRITE_REGISTER( RxMACSoftwareResetCommand, kRxMACSoftwareResetCommand_Reset );

    // Poll until the reset bit is cleared by the hardware.

    for ( i = 0; i < 5000; i++ )
    {
		if ( ( READ_REGISTER( RxMACSoftwareResetCommand )
				& kRxMACSoftwareResetCommand_Reset ) == 0 )
        {
            break;	// 'i' is always 0 or 1
        }
        IODelay( 1 );
    }

	    // Update the MAC Config register. Watch out for the programming
	    // restrictions documented in the GEM specification!!!

	    // Disable MAC before setting any other bits in the MAC config register.

	WRITE_REGISTER( RxMACConfiguration, 0 );

    for ( i = 0; i < 5000; i++ )
    {
		if ( ( READ_REGISTER( RxMACConfiguration )
				& kRxMACConfiguration_Rx_Mac_Enable ) == 0 )
        {
            break;	// 'i' is always 0
        }
        IODelay( 1 );
    }

		/* Ensure all elements in the Rx ring have my ownership:	*/

	u16 = OSSwapConstInt16( kGEMRxDescFrameSize_Own );
	for ( i = 0; i < fRxRingElements; ++i )
		fRxDescriptorRing[ i ].frameDataSize |= u16;

	WRITE_REGISTER( MACControlConfiguration, fMACControlConfiguration );// Pause etc bits

		// Restore (re-enable Rx MAC) MAC config register.

	WRITE_REGISTER( RxMACConfiguration, fRxMACConfiguration );
	return;
}/* end restartReceiver */


bool UniNEnet::transmitPacket( struct mbuf *packet )
{
	TxDescriptor	*dp;			// descriptor pointer
	UInt32			i,j,k;
	mbuf			*m;
	UInt32			dataPhys;
	UInt32			demandMask;
	UInt16			param0, param1;


    for ( m = packet, i=1; m->m_next; m=m->m_next, i++ )
      ;

	ELG( fTxElementsAvail << 16 | i, packet, '  Tx', "UniNEnet::transmitPacket" );

	if ( i > fTxElementsAvail )
	{
		ELG( fTxElementsAvail << 16 | i, packet, ' Tx-', "UniNEnet::transmitPacket - not enough elements avail." );
		return false;
	}

	j = txCommandTail;
    
	OSAddAtomic( -i, (SInt32*)&fTxElementsAvail );

    m = packet;

    do
    {        
		k = j;		// k will be the index to the last element on loop exit
        
        dataPhys = (UInt32)mcl_to_paddr( mtod( m, char* ) );
		if ( dataPhys == 0 )
			 dataPhys = pmap_extract( kernel_pmap, mtod( m, vm_offset_t ) );

		dp = &fTxDescriptorRing[ j ];
		OSWriteLittleInt32( &dp->bufferAddrLo,	0, dataPhys );
		OSWriteLittleInt32( &dp->flags0,		0, m->m_len );
		dp->flags1 = 0;
		txIntCnt++;
		j = (j + 1) & (fTxRingElements - 1);
    } while ( (m = m->m_next) != 0 );

	fTxMbuf[ k ] = packet;		// save the packet mbuf address in the last segment's index

	fTxDescriptorRing[ k ].flags0             |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags0_EndOfFrame );
	fTxDescriptorRing[ txCommandTail ].flags0 |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags0_StartOfFrame );
    if ( txIntCnt >= TX_DESC_PER_INT )
    {
		fTxDescriptorRing[ txCommandTail ].flags1 |= OSSwapHostToLittleConstInt32( kGEMTxDescFlags1_Int );
        txIntCnt = txIntCnt % TX_DESC_PER_INT;
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

	OSSynchronizeIO();				// make sure ring updated before kicked.
	WRITE_REGISTER( TxKick, j );
///	j = READ_REGISTER( TxKick );	/// read it back to force it out.
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


///	ELG( 0, fReady, 'kdRx', "receivePacket - kernel debugger routine" );

    *pkt_len = 0;

    if ( fReady == false )
        return;

#if USE_ELG
	UInt32		elgFlag = fpELG->evLogFlag;	// if kernel debugging, turn off ELG
	fpELG->evLogFlag = 0;
#endif // USE_ELG

    debuggerPkt     = pkt;
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

void UniNEnet::packetToDebugger( struct mbuf *packet, u_int size )
{
	ELG( packet, size, 'ToDb', "packetToDebugger" );

    debuggerPktSize = size;
    bcopy( mtod(packet, char*), debuggerPkt, size );
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


///	ELG( pkt, pkt_len, 'Send', "sendPacket" );

    if ( !fReady || !pkt || (pkt_len > (kIOEthernetMaxPacketSize - kIOEthernetCRCSize)) )
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
      IOLog( "UniNEnet::sendPacket - Polled transmit timeout - 1\n\r" );
      return;
    }

		/* Allocate a MBuf and copy the debugger transmit data into it.
		 *
		 * jliu - no allocation, just recycle the same buffer dedicated to
		 * KDB transmit.
		 */
    txDebuggerPkt->m_next		= 0;
    txDebuggerPkt->m_data		= (caddr_t)pkt;
    txDebuggerPkt->m_pkthdr.len	= txDebuggerPkt->m_len = pkt_len;

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
    {
        IOLog( "UniNEnet::sendPacket - Polled transmit timeout - 2\n\r" );
    }
#if USE_ELG
	fpELG->evLogFlag = elgFlag;
#endif // USE_ELG

	return;
}/* end sendPacket */


	/*-------------------------------------------------------------------------
	 * sendDummyPacket
	 * ----------------
	 * The UniN receiver seems to be locked until we send our first packet.
	 *
	 *-------------------------------------------------------------------------*/

void UniNEnet::sendDummyPacket()
{
    union
    {
        UInt8                 bytes[ 64 ];
        IOEthernetAddress     enet_addr[ 2 ];
    } dummyPacket;

	ELG( &dummyPacket, sizeof( dummyPacket ), 'SenD', "sendDummyPacket" );

    bzero( &dummyPacket, sizeof( dummyPacket ) );


    dummyPacket.enet_addr[0] = myAddress;   
    dummyPacket.enet_addr[1] = myAddress;

    sendPacket( (void*)dummyPacket.bytes, (unsigned int)sizeof( dummyPacket ) );
	return;
}/* end sendDummyPacket */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

bool UniNEnet::receiveInterruptOccurred()
{
    return receivePackets( false );
}/* end receiveInterruptOccurred */


	/*-------------------------------------------------------------------------
	 *
	 *
	 *
	 *-------------------------------------------------------------------------*/

bool UniNEnet::receivePackets( bool debuggerParam )
{
	mbuf		*packet;
	UInt32		i, last;
	int			receivedFrameSize = 0;
	UInt16		dmaFlags;
	UInt16		checksum;
	UInt32		rxPktStatus = 0;
	bool		passPacketUp;
	bool		reusePkt;
	bool		status;
	bool		useNetif = !debuggerParam && netifEnabled;
	bool		packetsQueued = false;
	bool		replaced;

   
    last      = (UInt32)-1;  
    i         = rxCommandHead;

//	ELG( rxCommandTail, rxCommandHead, 'Rx I', "receivePackets" );

///	for ( UInt32 loopLimit = fRxRingElements; loopLimit; --loopLimit )
    while ( 1 )
    {
        passPacketUp = false;
        reusePkt     = false;

		dmaFlags = OSReadLittleInt16( &fRxDescriptorRing[ i ].frameDataSize,     0 );
		checksum = OSReadLittleInt16( &fRxDescriptorRing[ i ].tcpPseudoChecksum, 0 );

			/*  If the current entry has not been written, then stop at this entry	*/

		if ( dmaFlags & kGEMRxDescFrameSize_Own )
			break;

        receivedFrameSize	= dmaFlags & kGEMRxDescFrameSize_Mask;
		rxPktStatus			= OSReadLittleInt32( &fRxDescriptorRing[ i ].flags, 0 );
	///	ELG( rxPktStatus, receivedFrameSize, 'Rx P', "UniNEnet::receivePackets - rx'd packet" );
		ELG( READ_REGISTER( RxCompletion ), receivedFrameSize, 'Rx P', "UniNEnet::receivePackets - rx'd packet" );

			/* Reject packets that are runts or that have other mutations.	*/

		if ( receivedFrameSize < (kIOEthernetMinPacketSize - kIOEthernetCRCSize)
		 ||  receivedFrameSize > (kIOEthernetMaxPacketSize)
		 ||  rxPktStatus & kGEMRxDescFlags_BadCRC )
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
                packetToDebugger( fRxMbuf[ i ], receivedFrameSize );
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
                status = genRxDescriptor( i );
                if ( status )
                {
                    reusePkt = false;
                }
                else
                {
					ELG( packet, fRxMbuf[i], 'upd-', "UniNEnet::receivePackets" );
						// Assume descriptor has not been corrupted.
					freePacket( fRxMbuf[i] );	// release new packet.
                    fRxMbuf[i] = packet;		// get the old packet back.
					packet = 0;					// pass up nothing.
                }
            }

			if ( packet == 0 )
			{		// Can get here if Tx is spewing UDP packets and
					// uses up the whole pool.
				NETWORK_STAT_ADD( inputErrors );
				ELG( 0, 0, 'Pkt-', "UniNEnet::receivePackets - no mBuf available." );
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
			OSSynchronizeIO();
			WRITE_REGISTER( RxKick, (i - 3) );
		}
        last = i;	/* Keep track of the last receive descriptor processed	*/
		i = (i + 1) & (fRxRingElements - 1);

        if ( debuggerParam )
            break;

			/* Transfer received packet to the network stack:	*/

        if ( packet )
        {
            KERNEL_DEBUG(	DBG_UniN_RXCOMPLETE | DBG_FUNC_NONE, 
                			(int)packet, (int)receivedFrameSize, 0, 0, 0 );

			if ( (receivedFrameSize > 64) && (checksum != 0) && !fIsPromiscuous )
				setChecksumResult(	packet,
									kChecksumFamilyTCPIP,
									kChecksumTCPSum16,
									0,							// validMask
									checksum,					// param0: actual cksum
									kRxHwCksumStartOffset );	// param1: 1st byte cksum'd 

			ELG( packet, receivedFrameSize << 16 | last, 'RxP+', "UniNEnet::receivePackets - packet up." );
			networkInterface->inputPacket( packet, receivedFrameSize, true );
			NETWORK_STAT_ADD( inputPackets );
			packetsQueued = true;
        }
	}/* end WHILE */

    if ( last != (UInt32)-1 )
    {
        rxCommandTail = last;
        rxCommandHead = i;
    }

    return packetsQueued;
}/* end receivePackets */


bool UniNEnet::transmitInterruptOccurred()
{
	UInt32		i, elemCnt = 0;
	bool		serviced	= false;


	i = READ_REGISTER( TxCompletion );
	ELG( txCommandHead, i, ' Tx+', "transmitInterruptOccurred" );

	while ( i != txCommandHead )	// i and txCommandHead race each other
	{
		do		/* This DO loop reduces READ_REGISTERs accessing the PCI bus.	*/
		{		/* Free the MBufs we just transmitted:	*/

			KERNEL_DEBUG(	DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE,
							fTxMbuf[ txCommandHead ], 0, 0, 0, 0 );
			elemCnt++;

			if ( fTxMbuf[ txCommandHead ] )
			{
			//	ELG( txCommandHead, fTxMbuf[ txCommandHead ], 'TxPF', "UniNEnet::receivePackets - free the packet" );
				freePacket( fTxMbuf[ txCommandHead ], kDelayFree );
				fTxMbuf[ txCommandHead ] = 0;
			}

			txCommandHead = (txCommandHead + 1) & (fTxRingElements - 1);

		} while ( i != txCommandHead );		// loop til txCommandHead catches i

		serviced = true;
		i = READ_REGISTER( TxCompletion );	// see if i advanced during last batch
	}/* end WHILE */

	OSAddAtomic( elemCnt, (SInt32*)&fTxElementsAvail );
	fpNetStats->outputPackets += releaseFreePackets();	

	return serviced;
}/* end transmitInterruptOccurred */


/*-------------------------------------------------------------------------
 *
 *
 *
 *-------------------------------------------------------------------------*/

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

		OSIncrementAtomic( (SInt32*)&fTxElementsAvail );

		KERNEL_DEBUG(	DBG_UniN_TXCOMPLETE | DBG_FUNC_NONE,
						(int)fTxMbuf[ txCommandHead ],
						(int)fTxMbuf[ txCommandHead ]->m_pkthdr.len, 0, 0, 0 );

		if ( fTxMbuf[ txCommandHead ] )
		{
			if ( fTxMbuf[ txCommandHead ] != txDebuggerPkt )
				debugQueue->enqueue( fTxMbuf[ txCommandHead ] );
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
	IOPhysicalSegment	segVector[ 2 ];
	UInt32				segments;


    segments = mbufCursor->getPhysicalSegmentsWithCoalesce( fRxMbuf[i], segVector );

	if ( segments != 1 )
	{
		ALRT( fRxMbuf[i], segments, 'seg-', "UniNEnet::genRxDescriptor - segments != 1" );
		return false;
	}

	OSWriteLittleInt32( &fRxDescriptorRing[i].bufferAddrLo,  0, segVector[0].location );
	OSWriteLittleInt16( &fRxDescriptorRing[i].frameDataSize, 0, segVector[0].length | kGEMRxDescFrameSize_Own );
	fRxDescriptorRing[ i ].flags = 0;

	return true;
}/* end genRxDescriptor */


void UniNEnet::monitorLinkStatus( bool firstPoll )
{
    UInt16          phyStatus;
    UInt16          linkStatus;
    UInt16          linkMode;
    UInt16          lpAbility;
    UInt16          phyStatusChange;
    bool            fullDuplex	= false;
    bool			clockWasOff	= false;
	UInt32          linkSpeed	= 0;
    IOMediumType    mediumType	= kIOMediumEthernetNone;
    IONetworkMedium	*medium;


	ELG( phyId, firstPoll, ' MLS', "monitorLinkStatus" );

    if ( firstPoll )
    {
		fPHYStatus	= 0;
		fLinkStatus	= kLinkStatusUnknown;
    }

    if ( fBuiltin && fCellClockEnabled == false )
    {
			/* Ethernet cell clock is disabled,				*/
			/* temporarily enable it to get the phyStatus:	*/
		ELG( 0, 0, '+Clk', "UniNEnet::monitorLinkStatus - turning on cell clock!!!" );
	//	OSSynchronizeIO();
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)true, (void*)nub, 0, 0 );
		OSSynchronizeIO();
		IODelay( 3 );		// Allow the cell some clock cycles before using it.
		fCellClockEnabled	= true;
		clockWasOff			= true;
    }

    if ( phyId == 0xFF )
    {		// must be GEM card. Fix this to use fBuiltin instead of phyId == 0xFF.
		phyStatus = READ_REGISTER( PCSMIIStatus ) & 0x0000FFFF; // read latched value
		IODelay( 3 );
		if ( firstPoll )
			IOSleep( 5000 );	/// fake wait for AutoNegotiation
		phyStatus = READ_REGISTER( PCSMIIStatus )				& 0x0000FFFF;
		lpAbility = READ_REGISTER( PCSMIILinkPartnerAbility )	& 0x0000FFFF;
    }
    else 
    {
        if ( miiReadWord( &phyStatus, MII_STATUS, phyId ) != true )
            return;
		miiReadWord( &lpAbility, MII_LINKPARTNER, phyId );
    }
//	ELG( lpAbility, phyStatus, ' mls', "monitorLinkStatus - LinkPartnerAbility and Status" );

    if ( clockWasOff )
    {		// if it was off in the first place, turn it back off
		ELG( 0, 0, '-Clk', "UniNEnet::monitorLinkStatus - turning off cell clock!!!" );
		OSSynchronizeIO();
		callPlatformFunction( "EnableUniNEthernetClock", true, (void*)false, (void*)nub, 0, 0 );
		OSSynchronizeIO();
		fCellClockEnabled = false;
    }
		// see if Link UP, autonegotiation complete bits changed:

	phyStatusChange	= (fPHYStatus ^ phyStatus)
					& (MII_STATUS_LINK_STATUS | MII_STATUS_NEGOTIATION_COMPLETE);

	if ( fMediumType == 0 || fMediumType != kIOMediumEthernetAuto )
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

		miiWaitForAutoNegotiation( phyId );
		miiReadWord( &phyStatus, MII_STATUS, phyId );
		miiReadWord( &phyStatus, MII_STATUS, phyId );
	}

		// skip this if the clock is disabled
	if ( fCellClockEnabled )
	{
		fMACControlConfiguration = 0;	// Assume no Pause capability by LP

		if ( (lpAbility & MII_LPAR_PAUSE) && isFullDuplex )
		{
			fMACControlConfiguration	= kMACControlConfiguration_Send_Pause_Enable
										| kMACControlConfiguration_Receive_Pause_Enable;
		}
		WRITE_REGISTER( MACControlConfiguration, fMACControlConfiguration );
	}

	if ( (phyStatus & MII_STATUS_LINK_STATUS)
	  && (firstPoll || (phyStatus & MII_STATUS_NEGOTIATION_COMPLETE)) )
	{
		wakeUp( true );				/* the link is up, enable the clock	*/

		if ( phyId == 0xFF )
		{
			linkSpeed  = 1000;
			fullDuplex = true;
			mediumType = kIOMediumEthernet1000BaseSX;
		}
		else if ( (phyType & MII_LXT971_MASK) == MII_LXT971_ID )
		{
			miiReadWord( &linkStatus,  MII_LXT971_STATUS_2, phyId );
			linkSpeed  = (linkStatus & MII_LXT971_STATUS_2_SPEED)  ?
						  100 : 10;
			fullDuplex = (linkStatus & MII_LXT971_STATUS_2_DUPLEX) ?
						  true : false;
			mediumType = (linkSpeed == 10)	? kIOMediumEthernet10BaseT
											: kIOMediumEthernet100BaseTX;
		}
		else if ( (phyType & MII_BCM5221_MASK) == MII_BCM5221_ID )
		{
			miiReadWord( &linkStatus,  MII_BCM5201_AUXSTATUS, phyId );
			linkSpeed  = (linkStatus & MII_BCM5201_AUXSTATUS_SPEED)  ?
						  100 : 10;
			fullDuplex = (linkStatus & MII_BCM5201_AUXSTATUS_DUPLEX) ?
						  true : false;
			mediumType = (linkSpeed == 10)	? kIOMediumEthernet10BaseT
											: kIOMediumEthernet100BaseTX;
		}
		else if ( (phyType & MII_BCM5201_MASK) == MII_BCM5201_ID )
		{
			miiReadWord( &linkStatus,  MII_BCM5201_AUXSTATUS, phyId );
			linkSpeed	= (linkStatus & MII_BCM5201_AUXSTATUS_SPEED)
						? 100 : 10;
			fullDuplex	= (linkStatus & MII_BCM5201_AUXSTATUS_DUPLEX)
						? true : false;
			mediumType = (linkSpeed == 10)	? kIOMediumEthernet10BaseT
											: kIOMediumEthernet100BaseTX;
		}
		else if ( ((phyType & MII_BCM5400_MASK) == MII_BCM5400_ID)
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5401_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5411_ID)		/// mlj temporary quick fix
			  ||  ((phyType & MII_BCM5400_MASK) == MII_BCM5421_ID) )	/// mlj temporary quick fix
		{
			miiReadWord( &linkStatus, MII_BCM5400_AUXSTATUS, phyId );

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
				mediumType  =  kIOMediumEthernet1000BaseTX;
			}                    
		}
		else if ( ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID)		// 0x01410C2x
			   || ((phyType & MII_MARVELL_MASK) == MII_MARVELL_ID_1) )	// 0x01410C6x
		{
			miiReadWord( &linkStatus, MII_MARVELL_PHY_SPECIFIC_STATUS, phyId );

			if ( !(linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_RESOLVED) )
			{
				linkSpeed = 0;
				fXIFConfiguration &= ~kXIFConfiguration_GMIIMODE;
			}
			else if ( linkStatus & MII_MARVELL_PHY_SPECIFIC_STATUS_1000 )
			{
				linkSpeed	= 1000;
				mediumType	= kIOMediumEthernet1000BaseTX;
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

	///	if ( fullDuplex != isFullDuplex )
			setDuplexMode( fullDuplex );    

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

		IOLog( "UniNEnet::monitorLinkStatus - Link is up at %ld Mbps - %s Duplex\n\r",
					linkSpeed, fullDuplex ? "Full" : "Half" );

		fLinkStatus = kLinkStatusUp;
	}
	else
	{
		if ( fCellClockEnabled
		  && ((fLinkStatus == kLinkStatusUp) || (fLinkStatus == kLinkStatusUnknown)) )
		{
			stopChip();

			medium = IONetworkMedium::getMediumWithType( fMediumDict, mediumType );

			setLinkStatus( kIONetworkLinkValid, medium, 0 );
   
			if ( fLinkStatus != kLinkStatusUnknown )
			   IOLog( "Ethernet(UniN): Link is down.\n\r" );

			txIntCnt = 0;

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
	UInt16		a0, a1, a2;


	a0 = ea->bytes[ 0 ] << 8 | ea->bytes[ 1 ];
	a1 = ea->bytes[ 2 ] << 8 | ea->bytes[ 3 ];
	a2 = ea->bytes[ 4 ] << 8 | ea->bytes[ 5 ];

	ELG( a0, a1 << 16 | a2, 'SetA', "UniNEnet::setHardwareAddress" );

	WRITE_REGISTER( MACAddress[ 0 ], a2 );
	WRITE_REGISTER( MACAddress[ 1 ], a1 );
	WRITE_REGISTER( MACAddress[ 2 ], a0 );

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


	fRxMACConfiguration = READ_REGISTER( RxMACConfiguration );
	ELG( this, fRxMACConfiguration, 'updH', "updateHashTableMask" );

	WRITE_REGISTER( RxMACConfiguration,
					fRxMACConfiguration & ~(kRxMACConfiguration_Rx_Mac_Enable
										  | kRxMACConfiguration_Hash_Filter_Enable) );
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
