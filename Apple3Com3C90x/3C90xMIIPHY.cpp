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

/**************************************************************************
 *
 * Low-level management interface methods, specific to the 3C90x.
 *
 **************************************************************************/

//---------------------------------------------------------------------------
// physicalMgmtWriteWord
//
// Write 'bits' number of bits to the MII management interface
// starting with the most significant bit in 'word'.

void
Apple3Com3C90x::physicalMgmtWriteWord( UInt32 word, UInt8 bits )
{
    UInt16 reg;

    for ( int shifts = 31; (shifts >= 0) && bits; shifts--, bits-- )
    {
        reg = kPhysicalMgmtDirWrite
            | ( (word & (1 << shifts)) ? kPhysicalMgmtDataMask : 0 );
        setPhysicalMgmt( reg );
        IODelay( kMIIDelay );

        reg |= kPhysicalMgmtClkMask;
        setPhysicalMgmt( reg );
        IODelay( kMIIDelay );

        reg &= ~kPhysicalMgmtClkMask;
        setPhysicalMgmt( reg );
        IODelay( kMIIDelay );
    }
}

//---------------------------------------------------------------------------
// physicalMgmtReadBit
//
// Read one bit from the MII management interface. Also used to
// tri-state the STA's MDIO pin.

UInt8 Apple3Com3C90x::physicalMgmtReadBit()
{
    UInt16 reg;

	reg = kPhysicalMgmtDirRead;
    setPhysicalMgmt( reg );
    IODelay( kMIIDelay );

    reg = kPhysicalMgmtDirRead | kPhysicalMgmtClkMask;
    setPhysicalMgmt( reg );
    IODelay( kMIIDelay );

    reg = kPhysicalMgmtDirRead;
    setPhysicalMgmt( reg );
    IODelay( kMIIDelay );

    reg = getPhysicalMgmt();
    
    return ( reg & kPhysicalMgmtDataMask ) ? 1 : 0;
}

//---------------------------------------------------------------------------
// miiReadWord

bool
Apple3Com3C90x::miiReadWord( PHYAddress  phyAddr,
                             PHYRegAddr  phyReg,
                             PHYWord *   phyData )
{
    UInt32   miiFrame;
    PHYWord  value = 0;
    bool     ret   = true;

    // Write preamble.

    physicalMgmtWriteWord( kMIIFramePreamble, kMIIFrameSize );

    // Prepare command frame.

    miiFrame = kMIIFrameStart
             | kMIIFrameOpcodeRead
             | SetBitField( MIIFrame, RegAddr, phyReg  )
             | SetBitField( MIIFrame, PhyAddr, phyAddr );

    // Write ST, OP, PHYAD, REGAD in the MII command frame.

    physicalMgmtWriteWord( miiFrame, 14 );

    // Hi-Z state
    // Make sure the PHY generated a zero bit after the 2nd Hi-Z bit.

    if ( physicalMgmtReadBit() ) ret = false;

    // read 16-bit data.

    for ( int i = 0; i < 16; i++ )
    {
        value = physicalMgmtReadBit() | (value << 1);
    }
    if ( phyData )
        *phyData = value;

    physicalMgmtReadBit();  // Hi-Z state

    return ret;
}

//---------------------------------------------------------------------------
// miiWriteWord

void
Apple3Com3C90x::miiWriteWord( PHYAddress  phyAddr,
                              PHYRegAddr  phyReg,
                              PHYWord     phyData )
{
    UInt32  miiFrame;

    // Write preamble.

    physicalMgmtWriteWord( kMIIFramePreamble, kMIIFrameSize );

    // Prepare command frame.

	miiFrame = kMIIFrameStart
             | kMIIFrameOpcodeWrite
             | kMIIFrameTAWrite
             | SetBitField( MIIFrame, RegAddr, phyReg  )
             | SetBitField( MIIFrame, PhyAddr, phyAddr )
             | SetBitField( MIIFrame, Data,    phyData );

    // Write command frame.

    physicalMgmtWriteWord( miiFrame, kMIIFrameSize );

    // Hi-Z state

    physicalMgmtReadBit();
}

/**************************************************************************
 *
 * Generic MII/PHY management interface methods. Hardware independent but
 * rely on the low-level MII methods.
 *
 **************************************************************************/

//---------------------------------------------------------------------------
// phyProbe
//
// Return true if a PHY is detected at the given PHY address.

bool Apple3Com3C90x::phyProbe( PHYAddress phy )
{
    if ( phy > kPHYAddrMax ) return false;

    return( miiReadWord( phy, kMIIRegisterStatus ) );
}

//---------------------------------------------------------------------------
// phyReset
//
// Reset the PHY device. Returns true for a successful reset cycle.

bool Apple3Com3C90x::phyReset( PHYAddress phy )
{
    bool     success = false;
    PHYWord  control;

    LOG_PHY("%s::%s\n", getName(), __FUNCTION__);

    if ( miiReadWord( phy, kMIIRegisterControl, &control ) == false )
        return false;

    // Set the reset bit in the PHY Control register

    miiWriteWord( phy, kMIIRegisterControl,
                  (control | kMIIControlResetMask) );

    // Wait till reset is complete, kMIIControlReset bit
    // will auto clear to zero.

    for ( SInt32 timeout = kPHYResetTimeout;
          timeout > 0;
          timeout -= kPHYResetDelay )
    {
        if ( miiReadWord( phy, kMIIRegisterControl, &control ) == false )
            break;

        if ( (control & kMIIControlResetMask) == 0 )
        {
            /*
             * Onboard implementation on Dell Optiplex GX1 needs this delay.
             * Without it, the first PHY register read after a reset will be
             * read as all zeroes.
             */
            IOSleep(10);
            success = true;
            break;
        }

        IOSleep( kPHYResetDelay );
    }

    return success;
}

//---------------------------------------------------------------------------
// phyWaitForValidLink
//
// Wait and return true if a valid link was detected,
// false on time out.

bool Apple3Com3C90x::phyWaitForValidLink( PHYAddress phy )
{
    PHYWord  status;

    LOG_PHY("%s::%s\n", getName(), __FUNCTION__);

    for ( SInt32 timeout = kPHYLinkTimeout;
          timeout > 0;
          timeout -= kPHYLinkDelay )
    {
        if ( miiReadWord( phy, kMIIRegisterStatus, &status ) == false )
            return false;

        if ( status & kMIIStatusLinkValidMask )
            return true;

        IOSleep( kPHYLinkDelay );
    }
    return false;
}

//---------------------------------------------------------------------------
// phyWaitForNegotiation
//
// Wait and return true if Nway auto-negotiation is complete.

bool Apple3Com3C90x::phyWaitForNegotiation( PHYAddress phy )
{
    PHYWord  status;

    LOG_PHY("%s::%s\n", getName(), __FUNCTION__);

    for ( SInt32 timeout = kPHYLinkTimeout;
          timeout > 0;
          timeout -= kPHYLinkDelay )
    {
        if ( miiReadWord( phy, kMIIRegisterStatus, &status ) == false )
            return false;

        if ( status & kMIIStatusNegotiationCompleteMask )
            return true;

        IOSleep( kPHYLinkDelay );
    }
    return false;
}

//---------------------------------------------------------------------------
// phyGetSupportedLinks
//
// Return a mask of all MII links supported by the PHY.

UInt32 Apple3Com3C90x::phyGetSupportedLinks( PHYAddress phy )
{
    PHYWord  status;
    UInt32   mask = 0;

    if ( miiReadWord( phy, kMIIRegisterStatus, &status ) == true )
    {
        mask = GetBitField( MIIStatus, Links, status );
    }
    return mask;
}

//---------------------------------------------------------------------------
// phyGetBestNegotiatedLink
//
// Return the best mode negotiated by the station and the remote link partner.

MIILink Apple3Com3C90x::phyGetBestNegotiatedLink( PHYAddress phy,
                                                  MIILink    phyLink,
                                                  MIILink    reportedLink,
                                                  PHYWord *  reportedStatus )
{
    MIILink  newLink = kMIILinkNone;
    PHYWord  status;

    // Read status register.

    miiReadWord( phy, kMIIRegisterStatus, &status );
    
    // Minimize the amount of work if status is unchanged.

    if ( reportedStatus )
    {
        if ( ( ( status ^ *reportedStatus ) & 
               ( kMIIStatusNegotiationCompleteMask |
                 kMIIStatusLinkValidMask ) ) == 0 )
        {
            // LOG_PHY("%s: status unchanged: 0x%04x\n", getName(), status);
            return reportedLink;
        }

        *reportedStatus = status;  // Update with current status.
    }

    if ( phyLink & kMIILinkNway )
    {
        // PHY was configured with auto-negotiation enabled.

        if (( status &
            ( kMIIStatusLinkValidMask | kMIIStatusNegotiationCompleteMask ))
         == ( kMIIStatusLinkValidMask | kMIIStatusNegotiationCompleteMask ))
        {
            PHYWord  local, remote;
    
            if ( miiReadWord( phy, kMIIRegisterAdvertisement, &local ) &&
                 miiReadWord( phy, kMIIRegisterLinkPartner,  &remote ) )
            {
                UInt32 commonLinks;

                LOG_PHY("%s: local  links: 0x%04x\n", getName(),
                        GetBitField( MIIAdvertisement, Links, local ));
                LOG_PHY("%s: remote links: 0x%04x\n", getName(),
                        GetBitField( MIILinkPartner, Links, remote ));
        
                commonLinks = GetBitField( MIIAdvertisement, Links, local ) &
                              GetBitField( MIILinkPartner, Links, remote );

                // Ranked according to 802.3u priority.
        
                if ( commonLinks & kMIILink100TX_FD )
                    newLink = kMIILink100TX_FD;
                else if ( commonLinks & kMIILink100T4 )
                    newLink = kMIILink100T4;
                else if ( commonLinks & kMIILink100TX )
                    newLink = kMIILink100TX;
                else if ( commonLinks & kMIILink10BT_FD )
                    newLink = kMIILink10BT_FD;
                else if ( commonLinks & kMIILink10BT )
                    newLink = kMIILink10BT;
            }
        }
    }
    else
    {
        // Auto-negotiation not enabled.

        if ( status & kMIIStatusLinkValidMask )
            newLink = phyLink;
    }

    // Preserve the kMIILinkNway flag.

    newLink |= ( phyLink & kMIILinkNway );

    return newLink;
}

//---------------------------------------------------------------------------
// phyGetID
//
// Return the 32-bit PHY ID.

UInt32 Apple3Com3C90x::phyGetIdentifier( PHYAddress phy )
{
    UInt32  id = kPHYIDInvalid;
    UInt16  id_hi;
    UInt16  id_lo;
    
    if ( miiReadWord( phy, kMIIRegisterID0, &id_hi ) &&
         miiReadWord( phy, kMIIRegisterID1, &id_lo ) )
    {    
        id = ((id_hi << 16) & 0xffff0000) | id_lo;
    }

    LOG_PHY("%s: PHY ID = %08lx\n", getName(), id);
    return id;
}

//---------------------------------------------------------------------------
// phyForceLink
//
// Disable Nway and force the PHY to the given link.
// Returns true on success, false otherwise.

bool Apple3Com3C90x::phyForceMIILink( PHYAddress phy, MIILink link )
{
    PHYWord  reg;
    UInt32   linkMask;

    LOG_PHY("%s::%s(%lx)\n", getName(), __FUNCTION__, link);

    // Refuse to handle Nway or invalid links.

    if ( link & kMIILinkNway )
        return false;

    if ( ( link & kMIILinkMask ) == kMIILinkNone )
        return false;

    // Verify that the link is supported by the PHY.

    linkMask = phyGetSupportedLinks( phy );

    if ( ( link & linkMask ) == 0 )
    {
        LOG_PHY("%s::%s PHY does not support link %lx\n",
                getName(), __FUNCTION__, link);
        return false;
    }

    // Reset the PHY.

    if ( phyReset( phy ) == false )
    {
        LOG_PHY("%s::%s PHY reset error\n", getName(), __FUNCTION__);
        return false;
    }

    // Advertise a single supported link, though Nway is going to
    // get turned off very shortly.

    if ( miiReadWord( phy, kMIIRegisterAdvertisement, &reg ) )
    {
        reg &= ~( kMIIAdvertisementProtocolMask |
                  kMIIAdvertisementLinksMask );

        reg |= SetBitField( MIIAdvertisement, Links, link );
        reg |= SetBitField( MIIAdvertisement, Protocol, 0x01 );

        miiWriteWord( phy, kMIIRegisterAdvertisement, reg );

        phyWaitForNegotiation( phy );
    }

    // Disable Nway auto-negotiation and set the desired link.

    if ( miiReadWord( phy, kMIIRegisterControl, &reg ) == false )
    {
        LOG_PHY("%s::%s CONTROL read error\n", getName(), __FUNCTION__);
        return false;
    }

    reg &= ~( kMIIControlSpeedSelectionMask  |
              kMIIControlFullDuplexMask      |
              kMIIControlIsolateMask         |
              kMIIControlAutoNegotiationMask );

    if ( link >= kMIILink100TX )
        reg |= kMIIControlSpeedSelectionMask;   // 100 Mb/s

    if ( link & ( kMIILink100TX_FD | kMIILink10BT_FD ) )
        reg |= kMIIControlFullDuplexMask;       // Full-Duplex

    // Update the PHY Control register.

    miiWriteWord( phy, kMIIRegisterControl, reg );
    LOG_PHY("%s::%s CONTROL set to %04x\n", getName(), __FUNCTION__,
            reg);

    return true;
}

//---------------------------------------------------------------------------
// phyStartNegotiation
//
// Start Nway auto-negotiation.

bool
Apple3Com3C90x::phyStartNegotiation( PHYAddress phy )
{
    PHYWord  reg;
    UInt32   linkMask;

    LOG_PHY("%s::%s\n", getName(), __FUNCTION__);

    // Reset PHY.

    if ( phyReset( phy ) == false )
    {
        LOG_PHY("%s::%s PHY reset error\n", getName(), __FUNCTION__);
        return false;
    }

    // Verify that the PHY supports auto-negotiation.

    if ( miiReadWord( phy, kMIIRegisterStatus, &reg ) == false )
    {
        LOG_PHY("%s::%s STATUS read error\n", getName(), __FUNCTION__);
        return false;
    }
    if ( ( reg & kMIIStatusNegotiationAbilityMask ) == 0 )
    {
        LOG_PHY("%s::%s no Nway support\n", getName(), __FUNCTION__);
        return false;
    }

    // Get local PHY capabilities.

    linkMask = phyGetSupportedLinks( phy );
    if ( linkMask == 0 )
    {
        LOG_PHY("%s::%s zero link support\n", getName(), __FUNCTION__);
        return false;
    }

    // Make sure the PHY is advertising all the available links.

    if ( miiReadWord( phy, kMIIRegisterAdvertisement, &reg ) == false )
    {
        LOG_PHY("%s::%s: ANAR read error\n", getName(), __FUNCTION__);
        return false;
    }

    reg &= ~( kMIIAdvertisementProtocolMask |
              kMIIAdvertisementLinksMask );
    
    reg |= SetBitField( MIIAdvertisement, Links, linkMask );
    reg |= SetBitField( MIIAdvertisement, Protocol, 0x01 );

    miiWriteWord( phy, kMIIRegisterAdvertisement, reg );

    // Set MII CONTROL to auto-negotiate and restart negotiation.

    if ( miiReadWord( phy, kMIIRegisterControl, &reg ) == false )
    {
        LOG_PHY("%s::%s CONTROL read error\n", getName(), __FUNCTION__);
        return false;
    }
    
    reg |= ( kMIIControlAutoNegotiationMask |
             kMIIControlRestartNegotiationMask );
    
    miiWriteWord( phy, kMIIRegisterControl, reg );

    return true;
}
