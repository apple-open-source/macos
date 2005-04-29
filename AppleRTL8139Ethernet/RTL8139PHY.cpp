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

bool RTL8139::phyAddMediumType( IOMediumType type,
                                UInt32       bps,
                                MediumIndex  index )
{	
	IONetworkMedium	* medium;
	bool              ret = false;

	medium = IONetworkMedium::medium( type, bps, 0, index );
	if ( medium )
    {
		ret = IONetworkMedium::addMedium( mediumDict, medium );
		if (ret) mediumTable[index] = medium;
		medium->release();
	}

	return ret;
}

//---------------------------------------------------------------------------
// Function: phyProbeMediaCapability
//
// Purpose:
//   Examine the PHY capabilities and advertise all supported media.

#define kMbScale 1000000 

void RTL8139::phyProbeMediaCapability( void )
{
	reg_bms = csrRead16( RTL_BMS );

    phyAddMediumType( kIOMediumEthernetAuto,
                      0, MEDIUM_INDEX_AUTO );

	if ( reg_bms & MII_STATUS_10_HD )
    {
        phyAddMediumType( kIOMediumEthernet10BaseT |
                          kIOMediumOptionHalfDuplex,
                          10 * kMbScale, MEDIUM_INDEX_10_HD );
    }

    if ( reg_bms & MII_STATUS_10_FD )
    {
        phyAddMediumType( kIOMediumEthernet10BaseT |
                          kIOMediumOptionFullDuplex,
                          10 * kMbScale, MEDIUM_INDEX_10_FD );
    }

    if ( reg_bms & MII_STATUS_TX_HD )
    {
        phyAddMediumType( kIOMediumEthernet100BaseTX |
                          kIOMediumOptionHalfDuplex,
                          100 * kMbScale, MEDIUM_INDEX_TX_HD);
    }

    if ( reg_bms & MII_STATUS_TX_FD )
    {
        phyAddMediumType( kIOMediumEthernet100BaseTX |
                          kIOMediumOptionFullDuplex,
                          100 * kMbScale, MEDIUM_INDEX_TX_FD );
    }
}

//---------------------------------------------------------------------------

bool RTL8139::phyReset( void )
{
	return true;
}

//---------------------------------------------------------------------------

bool RTL8139::phyWaitForAutoNegotiation( void )
{
    for ( SInt32 timeout = 5000; timeout > 0; timeout -= 20 )
    {
        if ( csrRead16( RTL_BMS ) & MII_STATUS_AUTONEG_COMPLETE )
        {
            return true;
        }

        IOSleep( 20 );
    }

    return false;
}

//---------------------------------------------------------------------------

bool RTL8139::phySetMedium( MediumIndex mediumIndex )
{
    UInt16  supportMask;
    UInt16  control;
    bool    success = false;

    if ( mediumIndex == currentMediumIndex )
    {
        return true;  // no change
    }

    switch ( mediumIndex )
    {
        case MEDIUM_INDEX_AUTO:

            // FIXME: set advertisement register.

            // Turn on and restart auto-negotiation.

            csrWrite16( RTL_BMC, MII_CONTROL_AUTONEG_ENABLE  |
                                 MII_CONTROL_RESTART_AUTONEG );

            phyWaitForAutoNegotiation();

            success = true;
            break;
        
        case MEDIUM_INDEX_10_HD:
        case MEDIUM_INDEX_10_FD:
        case MEDIUM_INDEX_TX_HD:
        case MEDIUM_INDEX_TX_FD:
        case MEDIUM_INDEX_T4:
            
            // Check if the selection is supported.
            
            supportMask = (reg_bms >> 11) & 0x1f;
            
            if ( (supportMask & (1 << mediumIndex)) == 0 )
            {
                break; // not supported
            }

            control = 0; // new PHY control register value

			if (( mediumIndex == MEDIUM_INDEX_TX_HD ) ||
                ( mediumIndex == MEDIUM_INDEX_TX_FD ) ||
                ( mediumIndex == MEDIUM_INDEX_T4    ))
            {
				control |= MII_CONTROL_100;
            }

			if (( mediumIndex == MEDIUM_INDEX_10_FD ) ||
                ( mediumIndex == MEDIUM_INDEX_TX_FD ))
            {
				control |= MII_CONTROL_FULL_DUPLEX;
            }

			csrWrite16( RTL_BMC, control );
            success = true;
            break;

        case MEDIUM_INDEX_NONE:
            phyReset();
            success = true;
            break;

        default:
            break;
    }

    if ( success ) currentMediumIndex = mediumIndex;

	return success;
}

//---------------------------------------------------------------------------

bool RTL8139::phySetMedium( const IONetworkMedium * medium )
{
    return phySetMedium( medium->getIndex() );
}

//---------------------------------------------------------------------------
// Function: phyReportLinkStatus
//
// Purpose:
//   Called periodically to monitor for link changes. When a change
//   is detected, determine the negotiated link and report it to the
//   upper layers by calling IONetworkController::setLinkStatus().

void RTL8139::phyReportLinkStatus( bool forceStatusReport )
{
    UInt16       phyStatus;
    UInt16       linkChanged;
    MediumIndex  activeMediumIndex;

    // Read the current value of the PHY status register.

    phyStatus = csrRead16( RTL_BMS );

    linkChanged = ( phyStatus ^ phyStatusLast ) &
                  ( MII_STATUS_LINK_STATUS |
                    MII_STATUS_AUTONEG_COMPLETE );

    if ( linkChanged || forceStatusReport )
    {
        // Determine the link status.

        if ( phyStatus & MII_STATUS_LINK_STATUS )
        {
            // Link up, determine the type of link established.

            UInt8  mediaStatus = csrRead8( RTL_MEDIA_STATUS );
            UInt16 modeControl = csrRead16( RTL_BMC );

            if ( mediaStatus & R_MEDIA_STATUS_SPEED_10 )
            {
                if ( modeControl & R_BMC_DUPLEXMODE )
                    activeMediumIndex = MEDIUM_INDEX_10_FD;    
                else
                    activeMediumIndex = MEDIUM_INDEX_10_HD;
            }
            else
            {
                if ( modeControl & R_BMC_DUPLEXMODE )
                    activeMediumIndex = MEDIUM_INDEX_TX_FD;  
                else
                    activeMediumIndex = MEDIUM_INDEX_TX_HD;
            }

            setLinkStatus( kIONetworkLinkValid | kIONetworkLinkActive,
                           phyGetMediumWithIndex( activeMediumIndex ) );
        }
        else
        {
            // Link down.
            setLinkStatus( kIONetworkLinkValid );
        }

        // Save phyStatus for the next run.

        phyStatusLast = phyStatus;
    }
}

//---------------------------------------------------------------------------

const IONetworkMedium * RTL8139::phyGetMediumWithIndex( MediumIndex index ) const
{
    if ( index < MEDIUM_INDEX_COUNT )
        return mediumTable[index];
    else
        return 0;
}
