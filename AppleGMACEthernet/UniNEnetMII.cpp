/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-1999 by Apple Computer, Inc., All rights reserved.
 *
 * MII/PHY (National Semiconductor DP83840/DP83840A) support methods.
 * It is general enough to work with most MII/PHYs.
 *
 * HISTORY
 *
 */
#include "UniNEnet.h"
#include "UniNEnetMII.h"
#include <libkern/OSByteOrder.h>


/*
 * Read from MII/PHY registers.
 */
bool UniNEnet::miiReadWord( UInt16 *dataPtr, UInt16 reg, UInt8 phy )
{
    UInt32              i;
    UInt32              miiReg;


	WRITE_REGISTER( MIFBitBangFrame_Output,
							  kMIFBitBangFrame_Output_ST_default
					|         kMIFBitBangFrame_Output_OP_read
					| phy  << kMIFBitBangFrame_Output_PHYAD_shift
					| reg  << kMIFBitBangFrame_Output_REGAD_shift
					|         kMIFBitBangFrame_Output_TA_MSB );

    for (i=0; i < 20; i++ )
    {     
		miiReg = READ_REGISTER( MIFBitBangFrame_Output );

        if ( miiReg & kMIFBitBangFrame_Output_TA_LSB )
        {
//            IOLog("Phy = %d Reg = %d miiReg = %08x\n\r", phy, reg, miiReg );
			ELG( reg << 16 | phy, miiReg, 'miRW', "miiReadWord" );
            *dataPtr = (UInt16) miiReg;
            return true;
        }
        IODelay(10);
    }

	ELG( reg << 16 | phy, miiReg, 'miR-', "miiReadWord - failed" );
    return false;
}

/*
 * Write to MII/PHY registers.
 */
bool UniNEnet::miiWriteWord( UInt16 data, UInt16 reg, UInt8 phy )
{
    UInt32              i;
    UInt32              miiReg;


	WRITE_REGISTER( MIFBitBangFrame_Output,
					          kMIFBitBangFrame_Output_ST_default
					|         kMIFBitBangFrame_Output_OP_write
					| phy  << kMIFBitBangFrame_Output_PHYAD_shift
					| reg  << kMIFBitBangFrame_Output_REGAD_shift
					|         kMIFBitBangFrame_Output_TA_MSB
					| data );

    for ( i=0; i < 20; i++ )
    {     
		miiReg = READ_REGISTER( MIFBitBangFrame_Output );
        
        if ( miiReg & kMIFBitBangFrame_Output_TA_LSB )
        {
			ELG( reg << 16 | phy, data, 'miWW', "miiWriteWord" );
            return true;
        }
        IODelay(10);
    }

	ELG( reg << 16 | phy, data, 'miW-', "miiWriteWord - failed" );
    return false;
}


bool UniNEnet::miiResetPHY( UInt8 phy )
{
    int 		i = MII_RESET_TIMEOUT;
    UInt16 		mii_control;


	ELG( i, phy, 'RstP', "miiResetPHY" );

    // Set the reset bit
    //
    miiWriteWord( MII_CONTROL_RESET, MII_CONTROL, phy );
        
    IOSleep(MII_RESET_DELAY);

    // Wait till reset process is complete (MII_CONTROL_RESET returns to zero)
    //
    while ( i > 0 ) 
    {
        if ( miiReadWord( &mii_control, MII_CONTROL, phy) == false )
                return false;

        if (!(mii_control & MII_CONTROL_RESET))
        {
            miiReadWord( &mii_control, MII_CONTROL, phy);
            mii_control &= ~MII_CONTROL_ISOLATE;
            miiWriteWord( mii_control, MII_CONTROL, phy );
            return true;
        }

        IOSleep(MII_RESET_DELAY);
        i -= MII_RESET_DELAY;
    }
    return false;
}/* end miiResetPHY */


bool UniNEnet::miiWaitForLink( UInt8 phy )
{
    int i = MII_LINK_TIMEOUT;
    unsigned short mii_status;
        

	ELG( i, phy, 'miWL', "miiWaitForLink" );

    while (i > 0) 
    {
        if ( miiReadWord( &mii_status, MII_STATUS, phy ) == false)
                return false;
                
        if (mii_status & MII_STATUS_LINK_STATUS)
                return true;
                
        IOSleep(MII_LINK_DELAY);
        i -= MII_LINK_DELAY;
    }
    return false;
}

bool UniNEnet::miiWaitForAutoNegotiation( UInt8 phy )
{
    int i = MII_LINK_TIMEOUT;
    unsigned short mii_status;
        
	ELG( i, phy, 'miWA', "miiWaitForAutoNegotiation" );

    while (i > 0) 
    {
        if ( miiReadWord( &mii_status, MII_STATUS, phy ) == false)
                return false;
                
        if (mii_status & MII_STATUS_NEGOTIATION_COMPLETE)
                return true;
                
        IOSleep(MII_LINK_DELAY);
        i -= MII_LINK_DELAY;
    }
    return false;
}/* end miiWaitForAutoNegotiation */


	/* Find the first PHY device on the MII interface.	*/
	/* Return											*/
	/*      true             PHY found					*/
	/*      false            PHY not found				*/

bool UniNEnet::miiFindPHY( UInt8 *phy )
{
    int         i;
    UInt16      phyWord;


	ELG( MII_MAX_PHY, *phy, 'miFP', "miiFindPHY" );

	*phy = 0xFF;

    	// The first two PHY registers are required:

    for ( i = 0; i < MII_MAX_PHY; i++ ) 
    {        
        if ( miiReadWord( &phyWord,     MII_STATUS,  i ) == false )	continue;
        if ( miiReadWord( &fPHYControl, MII_CONTROL, i ) == false )	continue;

        if ( phyWord == 0xFFFF && fPHYControl == 0xFFFF )			continue;
    
		*phy = i;
		return true;
	}/* end FOR */
        
    return false;
}/* end miiFindPHY */


bool UniNEnet::miiInitializePHY( UInt8 phy )
{
	UInt16           phyWord; 


	ELG( phyBCMType, phy, 'miIP', "miiInitializePHY" );

		// Clear enable auto-negotiation bit

	miiReadWord( &phyWord, MII_CONTROL, phy );
	phyWord &= ~MII_CONTROL_AUTONEGOTIATION;
	miiWriteWord( phyWord, MII_CONTROL, phy );

		/* Advertise 10/100 Half/Full duplex capable to link partner:	*/

	miiReadWord( &phyWord, MII_ADVERTISEMENT, phy );
	phyWord	|= (MII_ANAR_100BASETX_FD
			|   MII_ANAR_100BASETX
			|   MII_ANAR_10BASET_FD
			|   MII_ANAR_10BASET );
	miiWriteWord( phyWord, MII_ADVERTISEMENT, phy );


		// Set auto-negotiation-enable bit:

	miiReadWord( &phyWord, MII_CONTROL, phy );
	phyWord |= MII_CONTROL_AUTONEGOTIATION;
	miiWriteWord( phyWord, MII_CONTROL, phy );

		// Restart auto-negotiation:

	miiReadWord( &phyWord, MII_CONTROL, phy );
	phyWord |= MII_CONTROL_RESTART_NEGOTIATION;
	miiWriteWord( phyWord, MII_CONTROL, phy );

		/* If the system is not connected to the network, then			*/
		/* auto-negotiation never completes and we hang in this loop!	*/
#if 0
	while ( 1 ) 
	{
		miiReadWord( &phyWord, MII_CONTROL, phy );
		if ( (phyWord & MII_CONTROL_RESTART_NEGOTIATION) == 0 )
			break;
	}
#endif

	return true;
}/* end miiInitializePHY */
