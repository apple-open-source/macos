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

#include <libkern/OSByteOrder.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "UniNEnet.h"
#include "UniNEnetMII.h"


	/* Read from MII/PHY registers.	 */

bool UniNEnet::miiReadWord( UInt16 *dataPtr, UInt16 reg )
{
	UInt32		i;
	UInt32		miiReg;


	if ( phyId == 0xFF )
	{
		ALRT( 0, phyId << 16 | reg, 'IdR-', "miiReadWord - phyId not established yet." );
		return false;
	}

	WRITE_REGISTER( MIFBitBangFrame_Output,
							    kMIFBitBangFrame_Output_ST_default
					|           kMIFBitBangFrame_Output_OP_read
					| phyId  << kMIFBitBangFrame_Output_PHYAD_shift
					| reg    << kMIFBitBangFrame_Output_REGAD_shift
					|           kMIFBitBangFrame_Output_TA_MSB );

    for ( i = 0; i < 20; i++ )
    {     
		miiReg = READ_REGISTER( MIFBitBangFrame_Output );

        if ( miiReg & kMIFBitBangFrame_Output_TA_LSB )
        {
			ELG( miiReg, phyId << 16 | reg, 'miiR', "miiReadWord" );
            *dataPtr = (UInt16) miiReg;
            return true;
        }
        IODelay( 10 );
    }

	ELG( miiReg, phyId << 16 | reg, 'miR-', "miiReadWord - failed" );
    return false;
}/* end miiReadWord */


	/* Write to MII/PHY registers.	*/

bool UniNEnet::miiWriteWord( UInt16 data, UInt16 reg )
{
    UInt32              i;
    UInt32              miiReg;


	if ( phyId == 0xFF )
	{
		ALRT( data, phyId << 16 | reg, 'IdW-', "miiWriteWord - phyId not established yet." );
		return false;
	}

	WRITE_REGISTER( MIFBitBangFrame_Output,
					            kMIFBitBangFrame_Output_ST_default
					|           kMIFBitBangFrame_Output_OP_write
					| phyId  << kMIFBitBangFrame_Output_PHYAD_shift
					| reg    << kMIFBitBangFrame_Output_REGAD_shift
					|           kMIFBitBangFrame_Output_TA_MSB
					| data );

    for ( i = 0; i < 20; i++ )
    {     
		miiReg = READ_REGISTER( MIFBitBangFrame_Output );
        
        if ( miiReg & kMIFBitBangFrame_Output_TA_LSB )
        {
			ELG( data, phyId << 16 | reg, 'miiW', "miiWriteWord" );
            return true;
        }
        IODelay( 10 );
    }

	ELG( data, phyId << 16 | reg, 'miW-', "miiWriteWord - failed" );
    return false;
}/* end miiWriteWord */


bool UniNEnet::miiResetPHY()
{
    int 		i = MII_RESET_TIMEOUT;
    UInt16 		mii_control;


	ELG( i, phyId, 'RstP', "miiResetPHY" );

    miiWriteWord( MII_CONTROL_RESET, MII_CONTROL );	// Set the reset bit
	IOSleep( MII_RESET_DELAY );

		// Wait till reset process is complete (MII_CONTROL_RESET returns to zero)

    while ( i > 0 ) 
    {
		if ( miiReadWord( &mii_control, MII_CONTROL ) == false )
			return false;

        if ( !(mii_control & MII_CONTROL_RESET) )
		{
			miiReadWord( &mii_control, MII_CONTROL );
			mii_control &= ~MII_CONTROL_ISOLATE;
			miiWriteWord( mii_control, MII_CONTROL );
			return true;
		}

		IOSleep( MII_RESET_DELAY );
		i -= MII_RESET_DELAY;
	}/* end WHILE */

	return false;
}/* end miiResetPHY */


bool UniNEnet::miiWaitForAutoNegotiation()
{
	int			i = MII_LINK_TIMEOUT;
	UInt16		mii_status;


	ELG( i, phyId, 'miWA', "miiWaitForAutoNegotiation" );

	while ( i > 0 ) 
	{
		if ( miiReadWord( &mii_status, MII_STATUS ) == false )
			return false;
	
		if ( mii_status & MII_STATUS_NEGOTIATION_COMPLETE )
			return true;
	
		IOSleep( MII_LINK_DELAY );
		i -= MII_LINK_DELAY;
	}/* end WHILE */

	return false;
}/* end miiWaitForAutoNegotiation */
