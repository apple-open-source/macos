/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include "BCM440XPHY.h"

#define kPHYName  "bcm_phy"

#define super OSObject
OSDefineMetaClassAndStructors( AppleBCM440XPHY, OSObject )

//---------------------------------------------------------------------------

bool AppleBCM440XPHY::init( void * owner, MDIReadFunc readFunc,
                            MDIWriteFunc writeFunc )
{
    if (super::init() == false)
        return false;

    if (readFunc == 0 || writeFunc == 0)
        return false;

    fPHYOwner     = owner;
    fMDIReadFunc  = readFunc;
    fMDIWriteFunc = writeFunc;

    setConfiguredLink(kMIILinkNone);

    return true;
}

//---------------------------------------------------------------------------

AppleBCM440XPHY * AppleBCM440XPHY::BCM440XPHY( void *       owner,
                                               MDIReadFunc  readFunc,
                                               MDIWriteFunc writeFunc )
{
    AppleBCM440XPHY * phy = new AppleBCM440XPHY;

    if (phy && !phy->init(owner, readFunc, writeFunc))
    {
        phy->release();
        phy = 0;
    }

    return phy;
}

//---------------------------------------------------------------------------

bool AppleBCM440XPHY::mdiReadWord( PHYAddr phyAddr, PHYReg phyReg,
                                   PHYWord * phyData )
{
    return fMDIReadFunc(fPHYOwner, phyAddr, phyReg, phyData);
}

void AppleBCM440XPHY::mdiWriteWord( PHYAddr phyAddr, PHYReg phyReg,
                                    PHYWord phyData )
{
    fMDIWriteFunc(fPHYOwner, phyAddr, phyReg, phyData);
}

//---------------------------------------------------------------------------

IOReturn AppleBCM440XPHY::probePHY( void )
{
    PHYWord id0, id1;

    PHY_LOG("%s::%s\n", kPHYName, __FUNCTION__);

    fPHYAddr = kPHYAddrInvalid;

    for (int addr = kPHYAddrMin; addr <= kPHYAddrMax; addr++)
    {
        if (mdiReadWord(addr, kMIIRegisterStatus, &fPHYStatus) &&
            fPHYStatus != 0 &&
            mdiReadWord(addr, kMIIRegisterID0, &id0) &&
            mdiReadWord(addr, kMIIRegisterID1, &id1))
        {            
            fPHYID   = ((UInt32)(id0) << 16) | id1;
            fPHYAddr = addr;
            PHY_LOG("Found PHY @ 0x%lx status %04x ID 0x%08lx\n",
                    fPHYAddr, fPHYStatus, fPHYID);
            return kIOReturnSuccess;
        }
    }

    return kIOReturnNoDevice;
}

//---------------------------------------------------------------------------
// resetPHY
//
// Reset the PHY device. Returns true for a successful reset cycle.
// This routine can block.

IOReturn AppleBCM440XPHY::resetPHY( void )
{
    PHYWord  control;
    IOReturn ior;

    PHY_LOG("%s::%s\n", kPHYName, __FUNCTION__);

    if (mdiReadWord(fPHYAddr, kMIIRegisterControl, &control) == false)
        return kIOReturnIOError;

    // Assert reset bit in PHY Control register

    mdiWriteWord(fPHYAddr, kMIIRegisterControl, control | kMIIControlReset);

    // Wait until reset is complete, kMIIControlReset bit will auto clear

    ior = kIOReturnTimeout;
    for ( SInt32 timeout = kPHYResetTimeout;
          timeout > 0; timeout -= kPHYResetPause )
    {
        if (mdiReadWord(fPHYAddr, kMIIRegisterControl, &control) == false)
        {
            ior = kIOReturnIOError;
            break;
        }

        if ((control & kMIIControlReset) == 0)
        {
            ior = kIOReturnSuccess;
            break;
        }

        IOSleep(kPHYResetPause);
    }

    if (ior != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s error 0x%x\n", kPHYName, __FUNCTION__, ior);
    }

    return ior;
}

//---------------------------------------------------------------------------

UInt32 AppleBCM440XPHY::getPHYAddress( void ) const
{
    return fPHYAddr;
}

//---------------------------------------------------------------------------

UInt32 AppleBCM440XPHY::getPHYIdentifier( void ) const
{
    return fPHYID;
}

//---------------------------------------------------------------------------

UInt32 AppleBCM440XPHY::getLocalLinkSupportMask( void ) const
{
    return ((fPHYStatus & kMIIStatusLinkMask) >> kMIIStatusLinkShift);
}

//---------------------------------------------------------------------------

UInt32 AppleBCM440XPHY::getActiveLink( void ) const
{
    return fPHYActiveLink;
}

//---------------------------------------------------------------------------

void AppleBCM440XPHY::setConfiguredLink( MIILink configuredLink )
{
    fPHYLastStatus     = 0;
    fPHYActiveLink     = kMIILinkNone;
    fPHYConfiguredLink = configuredLink;
}

//---------------------------------------------------------------------------

IOReturn AppleBCM440XPHY::waitForStatusBits( UInt32  timeoutMS,
                                             PHYWord statusMask,
                                             PHYWord statusValue )
{
    PHYWord  status;
    IOReturn ior = kIOReturnTimeout;

    PHY_LOG("%s::%s(%ld)\n", kPHYName, __FUNCTION__, timeoutMS);

    if (timeoutMS == 0) return kIOReturnSuccess;

    for ( SInt32 t = timeoutMS; t > 0; t -= 10 )
    {
        if (mdiReadWord(fPHYAddr, kMIIRegisterStatus, &status) == false)
        {
            ior = kIOReturnIOError;
            break;
        }

        if ((status & statusMask) == statusValue)
        {
            ior = kIOReturnSuccess;
            break;
        }

        IOSleep(10);
    }

    if (ior != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s error 0x%x\n", kPHYName, __FUNCTION__, ior);
    }

    return ior;
}

//---------------------------------------------------------------------------
// restartAutoNegotiation

IOReturn AppleBCM440XPHY::restartAutoNegotiation( UInt32 waitTimeoutMS )
{
    PHYWord  word;
    UInt32   linkMask = kMIILinkMask;
    IOReturn ior;

    PHY_LOG("%s::%s(%ld)\n", kPHYName, __FUNCTION__, waitTimeoutMS);

#if PRE_NWAY_RESET
    if ((ior = resetPHY()) != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s PHY reset 1 error\n", kPHYName, __FUNCTION__);
        return ior;
    }
#endif

    // Verify that auto-negotiation is supported

    if ((fPHYStatus & kMIIStatusAutoNegotiationAbility) == 0)
    {
        PHY_LOG("%s::%s no Nway support\n", kPHYName, __FUNCTION__);
        return kIOReturnUnsupported;
    }

    // Determine which links should be advertised to remote partner

    linkMask &= getLocalLinkSupportMask();
    if (linkMask == 0)
    {
        PHY_LOG("%s::%s Link mask is zero\n", kPHYName, __FUNCTION__);
        return kIOReturnUnsupported;
    }

    setConfiguredLink(kMIILinkAutoNeg);

    // Update advertisement register

    if (mdiReadWord(fPHYAddr, kMIIRegisterAdvertisement, &word) == false)
    {
        PHY_LOG("%s::%s: ANAR read error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }

    word &= ~(kMIIAdvertisementLinkMask | kMIIAdvertisementPauseCapable); 
    word |= (linkMask << kMIIAdvertisementLinkShift) & kMIIAdvertisementLinkMask;

    mdiWriteWord(fPHYAddr, kMIIRegisterAdvertisement, word);

    // Enable and restart auto-negotiation

    if (mdiReadWord(fPHYAddr, kMIIRegisterControl, &word) == false)
    {
        PHY_LOG("%s::%s CONTROL read error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }

    word &= ~(kMIIControlPowerDown | kMIIControlIsolate | kMIIControlLoopback);
    word |= (kMIIControlEnableAutoNegotiation |
             kMIIControlRestartAutoNegotiation);
    
    mdiWriteWord(fPHYAddr, kMIIRegisterControl, word);

#if POST_NWAY_RESET
    if ((ior = resetPHY()) != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s PHY reset 2 error\n", kPHYName, __FUNCTION__);
        return ior;
    }
#endif

    return waitForStatusBits( waitTimeoutMS, 
                              kMIIStatusAutoNegotiationComplete,
                              kMIIStatusAutoNegotiationComplete );
}

//---------------------------------------------------------------------------

IOReturn AppleBCM440XPHY::forceSpeedAndDuplex( MIILink forceLink,
                                               UInt32  waitTimeoutMS )
{
    PHYWord  word;
    IOReturn ior;

    PHY_LOG("%s::%s(0x%lx, %ld)\n", kPHYName, __FUNCTION__,
            forceLink, waitTimeoutMS);

    if (forceLink & kMIILinkAutoNeg)
        return kIOReturnBadArgument;

    // Verify that the link is supported

    if ((forceLink & getLocalLinkSupportMask()) == 0)
    {
        PHY_LOG("%s::%s PHY does not support forced link %lx\n",
                kPHYName, __FUNCTION__, forceLink);
        return kIOReturnBadArgument;
    }

#if PRE_FORCE_RESET
    if ((ior = resetPHY()) != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s PHY reset 1 error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }
#endif

    // Disable auto-negotiation and force speed + duplex settings

    if (mdiReadWord(fPHYAddr, kMIIRegisterControl, &word) == false)
    {
        PHY_LOG("%s::%s CONTROL read error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }

    word &= ~( kMIIControlSpeedSelection        |
               kMIIControlFullDuplex            |
               kMIIControlIsolate               |
               kMIIControlEnableAutoNegotiation );

    if (forceLink >= kMIILink100BASETX)
        word |= kMIIControlSpeedSelection;
    if (forceLink & (kMIILink100BASETX_FD | kMIILink10BASET_FD))
        word |= kMIIControlFullDuplex;

    mdiWriteWord(fPHYAddr, kMIIRegisterControl, word);
    PHY_LOG("%s::%s CONTROL set to %04x\n", kPHYName, __FUNCTION__, word);

#if POST_FORCE_RESET
    if ((ior = resetPHY()) != kIOReturnSuccess)
    {
        PHY_LOG("%s::%s PHY reset 2 error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }
#endif

    setConfiguredLink(forceLink);

    return waitForStatusBits( waitTimeoutMS, 
                              kMIIStatusLinkValid, kMIIStatusLinkValid );
}

//---------------------------------------------------------------------------

IOReturn AppleBCM440XPHY::checkForLinkChange( bool * linkChanged,
                                              IOOptionBits options )
{    
    PHYWord  status;

    if (linkChanged)
        *linkChanged = false;

    if (mdiReadWord(fPHYAddr, kMIIRegisterStatus, &status) == false)
    {
        PHY_LOG("%s::%s STATUS read 1 error\n", kPHYName, __FUNCTION__);
        return kIOReturnIOError;
    }

    // Drivers can call this routine if a link change was detected by the
    // MAC, or to poll the PHY periodically to see if the link has changed.

    if (options & kPHYLinkChangePoll)
    {
        if (((status ^ fPHYLastStatus) & 
             (kMIIStatusAutoNegotiationComplete | kMIIStatusLinkValid)) == 0)
        {
            goto done;  // no change
        }
    }
    else
    {
        // Read status register again to clear any latched bits
        if (mdiReadWord(fPHYAddr, kMIIRegisterStatus, &status) == false)
        {
            PHY_LOG("%s::%s STATUS read 2 error\n", kPHYName, __FUNCTION__);
            return kIOReturnIOError;
        }
    }

    PHY_LOG("%s::%s link changed\n", kPHYName, __FUNCTION__);
    fPHYActiveLink = kMIILinkNone;
    if (linkChanged)
        *linkChanged = true;

    if (fPHYConfiguredLink & kMIILinkAutoNeg)
    {
        // PHY was configured with auto-negotiation enabled

        if ((status &
            (kMIIStatusLinkValid | kMIIStatusAutoNegotiationComplete)) ==
            (kMIIStatusLinkValid | kMIIStatusAutoNegotiationComplete))
        {
            PHYWord local, remote, common;

            if (mdiReadWord(fPHYAddr, kMIIRegisterAdvertisement, &local) &&
                mdiReadWord(fPHYAddr, kMIIRegisterLinkPartner,  &remote))
            {
                local  = (local & kMIIAdvertisementLinkMask) >>
                         kMIIAdvertisementLinkShift;
                remote = (remote & kMIILinkPartnerLinkMask) >>
                         kMIILinkPartnerLinkShift;

                PHY_LOG("%s: local  links: 0x%04x\n", kPHYName, local);
                PHY_LOG("%s: remote links: 0x%04x\n", kPHYName, remote);
        
                // Ranked according to 802.3u defined priority

                common = local & remote;
                if ( common & kMIILink100BASETX_FD )
                    fPHYActiveLink = kMIILink100BASETX_FD;
                else if ( common & kMIILink100BASET4 )
                    fPHYActiveLink = kMIILink100BASET4;
                else if ( common & kMIILink100BASETX )
                    fPHYActiveLink = kMIILink100BASETX;
                else if ( common & kMIILink10BASET_FD )
                    fPHYActiveLink = kMIILink10BASET_FD;
                else if ( common & kMIILink10BASET )
                    fPHYActiveLink = kMIILink10BASET;
            }
        }
    }
    else
    {
        // No Auto-negotiation
        if (status & kMIIStatusLinkValid)
            fPHYActiveLink = fPHYConfiguredLink;
    }

done:
    fPHYLastStatus = status;
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn AppleBCM440XPHY::programLink( MIILink miiLink, UInt32 waitTimeoutMS )
{
    IOReturn ior;

    if (miiLink & kMIILinkAutoNeg)
        ior = restartAutoNegotiation(waitTimeoutMS);
    else
        ior = forceSpeedAndDuplex(miiLink, waitTimeoutMS);
    
    return ior;
}
