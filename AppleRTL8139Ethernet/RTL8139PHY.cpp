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
 * rtl8139PHY.cpp
 *
 * HISTORY
 *
 * 09-Jul-01	Owen Wei at Realtek Semiconductor Corp. created for Realtek
 *		RTL8139 family NICs.
 */

#include "RTL8139.h"

	//---------------------------------------------------------------------------
	// Function: phyAddMediumType
	//
	// Purpose:
	//   Add an IONetworkMedium to a dictionary.
	//   Also add the object to an array for quick lookup.

bool RTL8139::phyAddMediumType( IOMediumType type, UInt32 bps, MediumIndex index )
{	
	IONetworkMedium	*medium;
	bool			ret = false;

	medium = IONetworkMedium::medium( type, bps, 0, index );

	if ( medium )
    {
		ret = IONetworkMedium::addMedium( mediumDict, medium );
		if ( ret )
			mediumTable[ index ] = medium;
		ELG( index, medium, '+Med', "RTL8139::phyAddMediumType" );
		medium->release();
	}

	return ret;
}/* end phyAddMediumType */


	//---------------------------------------------------------------------------
	// Function: phyProbeMediaCapability
	//
	// Purpose:
	//   Examine the PHY capabilities and advertise all supported media.

#define kMbScale 1000000 

void RTL8139::phyProbeMediaCapability()
{
	UInt16	anar;			// AutoNegotiation Advertisement Register

	reg_bms	= csrRead16( RTL_BMS );		// get Basic Mode Status
	anar	= csrRead16( RTL_ANAR );	// get AutoNegotiation Advertisement Reg
	ELG( anar, reg_bms, 'PPMC', "RTL8139::phyProbeMediaCapability" );

	fPauseSupported = false;			// Realtek 8139 NICs disable flow control.
	if ( anar & MII_ANAR_PAUSE )		// This is a Read-Only bit set by the EEPROM
		fPauseSupported = true;			// This had better be set for AppleTV.

		/* We publish all medium combinations even though some are unsupported	*/
		/* in order that when autoselect (the default) is chosen, the right		*/
		/* combination is available to report up the stack.						*/
		/* If selectMedium is called (via ifconfig) with an unsupported			*/
		/* combination, an error is returned.									*/

    phyAddMediumType( kIOMediumEthernetAuto, 0, MEDIUM_INDEX_AUTO );

		/*** CAUTION: the order of the entries is important for some reason.	***/
		/*** If you put flow-control ahead of full-duplex, and ifconfig from	***/
		/*** 10/half to 100/full the result is 100/flow-control.				***/

/* 100 Mbps combos:	*/

    if ( reg_bms & MII_STATUS_TX_FD )		// 100/Full:
    {

        phyAddMediumType( kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex,
                          100 * kMbScale, MEDIUM_INDEX_TX_FD );

		phyAddMediumType(	kIOMediumEthernet100BaseTX
						  | kIOMediumOptionFullDuplex
						  | kIOMediumOptionFlowControl,
								100 * kMbScale, MEDIUM_INDEX_TX_FC );

        phyAddMediumType( kIOMediumEthernet100BaseTX | kIOMediumOptionLoopback,
                          100 * kMbScale, MEDIUM_INDEX_TX_LB );
    }/* end IF 100/Full */

    if ( reg_bms & MII_STATUS_TX_HD )		// 100/Half:
    {
        phyAddMediumType( kIOMediumEthernet100BaseTX |  kIOMediumOptionHalfDuplex,
                          100 * kMbScale, MEDIUM_INDEX_TX_HD );
    }/* end IF 100/Half */

/* 10 Mbps combos:	*/

    if ( reg_bms & MII_STATUS_10_FD )		// 10/Full:
    {
        phyAddMediumType( kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex,
                          10 * kMbScale, MEDIUM_INDEX_10_FD );

		phyAddMediumType(	kIOMediumEthernet10BaseT
						  | kIOMediumOptionFullDuplex
						  | kIOMediumOptionFlowControl,
								10 * kMbScale, MEDIUM_INDEX_10_FC );

        phyAddMediumType( kIOMediumEthernet10BaseT | kIOMediumOptionLoopback,
                          10 * kMbScale, MEDIUM_INDEX_10_LB );
    }/* end IF 10/Full */

	if ( reg_bms & MII_STATUS_10_HD )		// 10/Half:
    {
        phyAddMediumType( kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex,
                          10 * kMbScale, MEDIUM_INDEX_10_HD );
    }/* end IF 10/Half */

	phyAddMediumType( kIOMediumEthernetNone, 0, MEDIUM_INDEX_NONE );

	return;
}/* end phyProbeMediaCapability */

	//---------------------------------------------------------------------------

bool RTL8139::phyReset()
{
	ELG( 0, 0, 'rPHY', "RTL8139::phyReset" );
	return true;
}/* end phyReset */

	//---------------------------------------------------------------------------

bool RTL8139::phyWaitForAutoNegotiation()
{
	SInt32		timeout;
	UInt16		bms;

	ELG( 0, 0, 'WtAN', "RTL8139::phyWaitForAutoNegotiation" );

    for ( timeout = 9000; timeout > 0; timeout -= 20 )
    {
		bms = csrRead16( RTL_BMS );
        IOSleep( 20 );					// give 20 ms for link status up
        if ( bms & MII_STATUS_AUTONEG_COMPLETE )
			return true;
    }

    return false;
}/* end phyWaitForAutoNegotiation */

	//---------------------------------------------------------------------------

bool RTL8139::phySetMedium( MediumIndex mediumIndex )
{
    UInt16  bmc;		// PHY reg 0 - Basic Mode Control
    UInt16  anar;		// PHY reg 4 - AutoNegotiation Advertisement Register
	UInt32	tcr;		// Transmit Control Register

	ELG( 0, mediumIndex, 'PhSM', "RTL8139::phySetMedium" );
    if ( mediumIndex == currentMediumIndex )
        return true;  // no change

    switch ( mediumIndex )
	{
	case MEDIUM_INDEX_AUTO:
		if ( fLoopback )
		{
			tcr  = csrRead32( RTL_TCR );		// get Transmit Control Register
			tcr &= ~(R_TCR_LBKI | R_TCR_LBKE);
			csrWrite32( RTL_TCR, tcr );
			fLoopback = false;
		}
			/* advertise everything (MII_ANAR_PAUSE is Read-Only):	*/
		anar = MII_ANAR_10_HD | MII_ANAR_10_FD
			 | MII_ANAR_TX_HD | MII_ANAR_TX_FD | MII_ANAR_SELECTOR_CSMACD;
		csrWrite16( RTL_ANAR, anar );

			// Turn on and restart auto-negotiation:

		csrWrite16( RTL_BMC, MII_CONTROL_AUTONEG_ENABLE  | MII_CONTROL_RESTART_AUTONEG );

		phyWaitForAutoNegotiation();
		break;

	case MEDIUM_INDEX_10_FC:
	case MEDIUM_INDEX_10_FD:
	case MEDIUM_INDEX_10_HD:

	case MEDIUM_INDEX_TX_FC:
	case MEDIUM_INDEX_TX_FD:
	case MEDIUM_INDEX_TX_HD:

		if ( fLoopback )					// If loopback was on, turn it off:
		{
			fLoopback = false;
			tcr  = csrRead32( RTL_TCR );	// get Transmit Control Register
			tcr &= ~(R_TCR_LBKI | R_TCR_LBKE);
			csrWrite32( RTL_TCR, tcr );
		}
				// assume 10 half/full (the CSMACD is crucial):
		anar = MII_ANAR_10_HD | MII_ANAR_10_FD | MII_ANAR_SELECTOR_CSMACD;

			/* Speed:	*/

		if ( (mediumIndex == MEDIUM_INDEX_TX_HD)	// Half duplex
		  || (mediumIndex == MEDIUM_INDEX_TX_FD)	// Full duplex
		  || (mediumIndex == MEDIUM_INDEX_TX_FC) )	// Flow Control full duplex
		{		// 100 half/full (the CSMACD is crucial):
			anar = MII_ANAR_TX_HD | MII_ANAR_TX_FD | MII_ANAR_SELECTOR_CSMACD;
		}
			/* Duplex and Flow Control:	*/

		if ( (mediumIndex == MEDIUM_INDEX_10_HD) || (mediumIndex == MEDIUM_INDEX_TX_HD) )
		{
			anar &= ~(MII_ANAR_10_FD | MII_ANAR_TX_FD);	// turn off full duplex
		}
		else
		{
			anar &= ~(MII_ANAR_10_HD | MII_ANAR_TX_HD);	// turn off half duplex
			if ( mediumIndex == MEDIUM_INDEX_10_FC		// enable Flow Control?
			  || mediumIndex == MEDIUM_INDEX_TX_FC )	anar |= MII_ANAR_PAUSE;
		}
		csrWrite16( RTL_ANAR, anar );
		csrWrite16( RTL_BMC,  MII_CONTROL_AUTONEG_ENABLE | MII_CONTROL_RESTART_AUTONEG );
		phyWaitForAutoNegotiation();
		break;

	case MEDIUM_INDEX_10_LB:	// Loopback
	case MEDIUM_INDEX_TX_LB:
		tcr  = csrRead32( RTL_TCR );		// get Transmit Control Register
		tcr &= (R_TCR_LBKI | R_TCR_LBKE);	// turn off both loopback bits.

		bmc = MII_CONTROL_RESET;			// Init Basic Mode Control
		if ( mediumIndex == MEDIUM_INDEX_TX_LB )
			bmc |= MII_CONTROL_100;

		if ( fLoopbackMode == kSelectLoopbackMAC )
		{
			tcr |= R_TCR_LBKI;
		}
		else	// must be kSelectLoopbackPHY
		{
			tcr |= R_TCR_LBKE;			// set External (PHY) loopback
			bmc |= MII_CONTROL_LOOPBACK;
		}
		csrWrite32( RTL_TCR, tcr );
		csrWrite16( RTL_BMC, bmc );		// hit the PHY Control register
		IOSleep( 1 );
		fLoopback = true;
		break;

	case MEDIUM_INDEX_NONE:

		if ( fLoopback )
		{
			tcr  = csrRead32( RTL_TCR );		// get Transmit Control Register
			tcr &= ~(R_TCR_LBKI | R_TCR_LBKE);
			csrWrite32( RTL_TCR, tcr );
			fLoopback = false;
		}
		csrWrite16( RTL_BMC, MII_CONTROL_POWER_DOWN );
		break;

	default:
		return false;
	}/* end SWITCH */

	currentMediumIndex = mediumIndex;
	return true;
}/* end phySetMedium */


bool RTL8139::phySetMedium( const IONetworkMedium* medium )
{
	ELG( medium, medium->getIndex(), 'PhSM', "RTL8139::phySetMedium" );
    return phySetMedium( medium->getIndex() );
}/* end phySetMedium */


	//---------------------------------------------------------------------------
	// Function: phyReportLinkStatus
	//
	// Purpose:
	//   Called periodically to monitor for link changes. When a change
	//   is detected, determine the negotiated link and report it to the
	//   upper layers by calling IONetworkController::setLinkStatus().

void RTL8139::phyReportLinkStatus()
{
    MediumIndex	activeMediumIndex;
	UInt16		anlp;		// AutoNegotiation Link Partner Ability PHY reg
    UInt16		phyStatus;
    UInt16		linkChanged;
	UInt8		mediaStatus;
	UInt16		modeControl;

    phyStatus = csrRead16( RTL_BMS );	// Get current value of the PHY Status register

	ELG( phyStatusLast, phyStatus, 'PRLS', "RTL8139::phyReportLinkStatus" );

    linkChanged = (phyStatus ^ phyStatusLast)
				& (MII_STATUS_LINK_STATUS | MII_STATUS_AUTONEG_COMPLETE );

    if ( linkChanged || forceLinkChange )
    {
			// Determine the link status:

        if ( phyStatus & MII_STATUS_LINK_STATUS )
        {
				// Link up, determine the type of link established:

			mediaStatus = csrRead8( RTL_MEDIA_STATUS );
			modeControl = csrRead16( RTL_BMC );
			anlp		= csrRead16( RTL_ANLP );
			ELG( mediaStatus << 16 | phyStatus, modeControl << 16 | anlp, '+Up+', "RTL8139::phyReportLinkStatus - 10/half" );

			if ( !(anlp & MII_ANLP_PAUSE) )
				ALRT( mediaStatus, anlp, 'FlC-', "RTL8139::phyReportLinkStatus - ethernet flow control not enabled.");

            if ( mediaStatus & R_MEDIA_STATUS_SPEED_10 )
            {
				fSpeed100 = false;
                if ( (modeControl & R_BMC_DUPLEXMODE) == 0 )
				{
					 activeMediumIndex = MEDIUM_INDEX_10_HD;	// Half duplex
					ELG( currentMediumIndex, activeMediumIndex, '10HD', "RTL8139::phyReportLinkStatus - 10/half" );
				}
				else if ( anlp & MII_ANLP_PAUSE
							&& (currentMediumIndex == MEDIUM_INDEX_AUTO
							 || currentMediumIndex == MEDIUM_INDEX_10_FC) )
				{
                     activeMediumIndex = MEDIUM_INDEX_10_FC;	// 10/Flow Control
					ELG( currentMediumIndex, activeMediumIndex, '10FC', "RTL8139::phyReportLinkStatus - 10/Flow Control" );
				}
				else
				{
					activeMediumIndex = MEDIUM_INDEX_10_FD;	// 10/Full sans Flow Control
					ELG( currentMediumIndex, activeMediumIndex, '10FD', "RTL8139::phyReportLinkStatus - 10/Full Duplex" );
				}
            }
            else	// 100 Mbps:
            {
				fSpeed100 = true;
                if ( (modeControl & R_BMC_DUPLEXMODE) == 0 )
				{
					activeMediumIndex = MEDIUM_INDEX_TX_HD;		// Half duplex
					ELG( currentMediumIndex, activeMediumIndex, 'TXHD', "RTL8139::phyReportLinkStatus - 100/half" );
				}
				else if ( anlp & MII_ANLP_PAUSE
							&& (currentMediumIndex == MEDIUM_INDEX_AUTO
							 || currentMediumIndex == MEDIUM_INDEX_TX_FC) )
				{
                     activeMediumIndex = MEDIUM_INDEX_TX_FC;	// 100/Flow Control
					ELG( currentMediumIndex, activeMediumIndex, 'TXFC', "RTL8139::phyReportLinkStatus - 100/Flow Control" );
				}
				else
				{
					activeMediumIndex = MEDIUM_INDEX_TX_FD;	// 100/Full sans Flow Control
					ELG( currentMediumIndex, activeMediumIndex, 'TXFD', "RTL8139::phyReportLinkStatus - 100/Full Duplex" );
				}
            }

            setLinkStatus( kIONetworkLinkValid | kIONetworkLinkActive,
                           phyGetMediumWithIndex( activeMediumIndex ),
						   fSpeed100 ? 100000000 : 10000000 );
			enableHardwareInterrupts();
			fTransmitQueue->start();
		}/* end IF link now up */
        else							//  Link down:
        {
            setLinkStatus( kIONetworkLinkValid );	// not kIONetworkLinkActive.
			disableHardwareInterrupts();
			fTransmitQueue->stop();
			fTransmitQueue->flush();
        }

        phyStatusLast = phyStatus;	// Save phyStatus for the next run.
    }/* end IF link changed */

	forceLinkChange = false;		// Don't return before clearing this.
	return;
}/* end phyReportLinkStatus */

	//---------------------------------------------------------------------------

const IONetworkMedium* RTL8139::phyGetMediumWithIndex( MediumIndex index ) const
{
//	ELG( 0000, __LINE__, ' PHY', "RTL8139::phyGetMediumWithIndex" );
    if ( index < MEDIUM_INDEX_COUNT )
        return mediumTable[ index ];
	return 0;
}/* end phyGetMediumWithIndex */
