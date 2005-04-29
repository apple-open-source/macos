/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#include "AppleDP83816.h"

#define CLASS AppleDP83816Ethernet
#define super IOEthernetController

enum {
    MII_BMCR_RESET               = 0x8000,
    MII_BMCR_LOOPBACK            = 0x4000,
    MII_BMCR_SPEED_100           = 0x2000,
    MII_BMCR_AUTONEG_ENABLE      = 0x1000,
    MII_BMCR_POWER_DOWN          = 0x0800,
    MII_BMCR_ISOLATE             = 0x0400,
    MII_BMCR_RESTART_AUTONEG     = 0x0200,
    MII_BMCR_FULL_DUPLEX         = 0x0100,
    MII_BMCR_CDT_ENABLE          = 0x0080,
    MII_BMCR_SPEED_1000          = 0x0040
};

enum {
    MII_BMSR_T4                  = 0x8000,
    MII_BMSR_TX_FD               = 0x4000,
    MII_BMSR_TX_HD               = 0x2000,
    MII_BMSR_10_FD               = 0x1000,
    MII_BMSR_10_HD               = 0x0800,
    MII_BMSR_AUTONEG_COMPLETE    = 0x0020,
    MII_BMSR_REMOTE_FAULT_DETECT = 0x0010,
    MII_BMSR_AUTONEG_CAPABLE     = 0x0008,
    MII_BMSR_LINK_STATUS         = 0x0004,
    MII_BMSR_JABBER_DETECTED     = 0x0002,
    MII_BMSR_EXTENDED_CAPABILITY = 0x0001
};

enum {
    MII_ANAR_NEXT_PAGE           = 0x8000,
    MII_ANAR_ACKNOWLEDGE         = 0x4000,
    MII_ANAR_REMOTE_FAULT        = 0x2000,
    MII_ANAR_ASYMMETRIC_PAUSE    = 0x0800,
    MII_ANAR_PAUSE               = 0x0400,
    MII_ANAR_T4                  = 0x0200,
    MII_ANAR_TX_FD               = 0x0100,
    MII_ANAR_TX_HD               = 0x0080,
    MII_ANAR_10_FD               = 0x0040,
    MII_ANAR_10_HD               = 0x0020,
    MII_ANAR_SELECTOR_CSMACD     = 0x0001
};

enum {
    kMediumIndex10H = 0,
    kMediumIndex10F,
    kMediumIndex100H,
    kMediumIndex100F,
    kMediumIndexAuto,
    kMediumIndexCount
};

#define MBPS(x)   (1000000 * (x))

static const UInt32 MediumIndexToPHYStatus[ kMediumIndexCount ] =
{
    CFG_LNKSTS,
    CFG_LNKSTS | CFG_FULLDUPLEX,
    CFG_LNKSTS | CFG_SPEED100,
    CFG_LNKSTS | CFG_SPEED100  | CFG_FULLDUPLEX,
    0
};

static const UInt16 MediumIndexToANAR[ kMediumIndexCount ] =
{
    MII_ANAR_10_HD,
    MII_ANAR_10_FD,
    MII_ANAR_TX_HD,
    MII_ANAR_TX_FD,
    0
};

//---------------------------------------------------------------------------

void CLASS::phyInit( void )
{
    // Recommended by the DP83815 manual, do it for DP83816?

    WriteReg(PGSEL,  0x0001);
    WriteReg(PMDCSR, 0x189C);
    WriteReg(TSTDAT, 0x0000);
    WriteReg(DSPCFG, 0x5040);
    WriteReg(SDCFG,  0x008C);

    // Mask all PHY interrupt source except link change and
    // auto-negotiation complete interrupts.

    WriteReg(MISR, MISR_MSK_JAB | MISR_MSK_RF | MISR_MSK_FHF | MISR_MSK_RHF);
    WriteReg(MICR, MICR_INTEN);
}

//---------------------------------------------------------------------------

bool CLASS::phyAddMediumType( IOMediumType type,
                              UInt32       speed,
                              UInt32       refcon )
{
    IONetworkMedium * medium;
    bool              ret = false;

    medium = IONetworkMedium::medium( type, speed, 0, refcon );
    if (medium)
    {
        ret = IONetworkMedium::addMedium( fMediumDict, medium );
        medium->release();
    }

    return ret;
}

//---------------------------------------------------------------------------

void CLASS::phyProbeCapability( void )
{
    if (fMediumDict) return;

    UInt32 bmsr = ReadReg(BMSR);

    fAutoNegANAR = 0;

    fMediumDict = OSDictionary::withCapacity(5);
    if (!fMediumDict) return;

    if ( bmsr & MII_BMSR_10_HD )
    {
        phyAddMediumType(kIOMediumEthernet10BaseT |
                         kIOMediumOptionHalfDuplex,
                         MBPS(10),
                         kMediumIndex10H);

        fAutoNegANAR |= MII_ANAR_10_HD;
    }

    if ( bmsr & MII_BMSR_10_FD )
    {
        phyAddMediumType(kIOMediumEthernet10BaseT |
                         kIOMediumOptionFullDuplex,
                         MBPS(10),
                         kMediumIndex10F);

        fAutoNegANAR |= MII_ANAR_10_FD;
    }

    if ( bmsr & MII_BMSR_TX_HD )
    {
        phyAddMediumType(kIOMediumEthernet100BaseTX |
                         kIOMediumOptionHalfDuplex,
                         MBPS(100),
                         kMediumIndex100H);

        fAutoNegANAR |= MII_ANAR_TX_HD;
    }

    if ( bmsr & MII_BMSR_TX_FD )
    {
        phyAddMediumType(kIOMediumEthernet100BaseTX |
                         kIOMediumOptionFullDuplex,
                         MBPS(100),
                         kMediumIndex100F);

        fAutoNegANAR |= MII_ANAR_TX_FD;
    }

    if (fAutoNegANAR)
    {
        phyAddMediumType( kIOMediumEthernetAuto, 0, kMediumIndexAuto );
    }

    DEBUG_LOG("fPhy_ANAR = %04x\n", fAutoNegANAR);

    publishMediumDictionary(fMediumDict);
}

//---------------------------------------------------------------------------

bool CLASS::phyReset( void )
{
    bool    success = false;
    UInt32  control;

    DEBUG_LOG("%s::%s\n", getName(), __FUNCTION__);

    // Set the reset bit in the PHY Control register

    control = ReadReg( BMCR );

    WriteReg( BMCR, (control | MII_BMCR_RESET) );

    // Wait till reset is complete, MII_BMCR_RESET bit
    // will auto clear to zero.

    for ( int timeout = 100; timeout > 0; timeout -- )
    {
        control = ReadReg(BMCR);

        if ((control & MII_BMCR_RESET) == 0)
        {
            success = true;
            IOSleep(10);  // wait a bit following reset
            break;
        }

        IOSleep(1);
    }

    return success;
}

//---------------------------------------------------------------------------

void CLASS::phyWaitForAutoNegotiation( void )
{
    bool ok = false;

    DEBUG_LOG("%s::%s\n", getName(), __FUNCTION__);

    // Wait for auto-negotiation to complete

    for (int timeout = 3500; timeout > 0; timeout -= 20)
    {
        if (ReadReg(BMSR) & MII_BMSR_AUTONEG_COMPLETE)
        {
            ok = true;
            break;
        }

        IOSleep(20);
    }

    if (!ok)
    {
        ERROR_LOG("%s: %s TIMEOUT\n", getName(), __FUNCTION__);
        return;
    }

    // Wait for link good and program the MAC according to the
    // full-duplex mode indicator. If we don't wait, and early
    // KDP debugging is active, driver may not program the MAC
    // correctly to match the duplex mode.

    for (int timeout = 3000; timeout > 0; timeout -= 20)
    {
        if (ReadReg(CFG) & CFG_LNKSTS)
        {
            phyReportLinkStatus();
            break;
        }

        IOSleep(20);
    }
}

//---------------------------------------------------------------------------

bool CLASS::phySetMedium( UInt32 mediumIndex, bool waitForAutoNegotiation )
{
    UInt32 anar;

    phyReset();
    phyInit();

    switch ( mediumIndex )
    {
        case kMediumIndexAuto:
            anar = fAutoNegANAR | MII_ANAR_SELECTOR_CSMACD;
            break;

        case kMediumIndex10H:
        case kMediumIndex10F:
        case kMediumIndex100H:
        case kMediumIndex100F:
            anar = MediumIndexToANAR[mediumIndex] |
                   MII_ANAR_SELECTOR_CSMACD;
            break;

        default:
            return false;
    }

    WriteReg( ANAR, anar );
    DEBUG_LOG("ANAR = %04x\n", anar);

    // Enable and restart auto-negotiation

    WriteReg( BMCR, MII_BMCR_AUTONEG_ENABLE  |
                    MII_BMCR_RESTART_AUTONEG );

    if (waitForAutoNegotiation)
        phyWaitForAutoNegotiation();

    fCurrentMediumIndex = mediumIndex;

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::phySetMedium( const IONetworkMedium * medium,
                          bool  waitForAutoNegotiation )
{
    return phySetMedium(medium->getIndex(), waitForAutoNegotiation);
}

//---------------------------------------------------------------------------

void CLASS::phyReportLinkStatus( void )
{
    UInt32            phyStatus;
    UInt32            linkStatus = kIONetworkLinkValid;
    IONetworkMedium * medium = 0;
    int               index = fCurrentMediumIndex;

    phyStatus = ReadReg(CFG);
    DEBUG_LOG("PHYStatus = %x\n", phyStatus);        

    phyStatus &= (CFG_LNKSTS | CFG_SPEED100 | CFG_FULLDUPLEX);

    if (index == kMediumIndexAuto)
    {
        for (index = 0; index < kMediumIndexAuto; index++)
        {
            if (phyStatus == MediumIndexToPHYStatus[index])
            {
                break;
            }
        }
    }

    if (index < kMediumIndexAuto)
    {
        medium = IONetworkMedium::getMediumWithIndex(fMediumDict, index);
    }

    if (phyStatus & CFG_LNKSTS)
    {
        bool fullDuplex;

        linkStatus |= kIONetworkLinkActive;
        DEBUG_LOG("Link UP [%lu]\n", index);

        fullDuplex = ((phyStatus & CFG_FULLDUPLEX) == CFG_FULLDUPLEX);
        if (fFullDuplexMode != fullDuplex)
        {
            fFullDuplexMode = fullDuplex;
            updateMACForDuplexModeChange();
        }
    }
    else
    {
        DEBUG_LOG("Link DOWN\n");
    }

    setLinkStatus( linkStatus, medium );
}

//---------------------------------------------------------------------------

void CLASS::phyPollLinkStatus( void )
{
    if (fLinkDownPollHoldOff)
        fLinkDownPollHoldOff--;

    if ((fLinkDownPollHoldOff == 0) &&
        ((fLinkStatus & kIONetworkLinkActive) == 0) &&
        (ReadReg(CFG) & CFG_LNKSTS))
    {
        DEBUG_LOG("*** POLL LINK UP ***\n");
        phyReportLinkStatus();
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::phySelectMedium( const IONetworkMedium * medium,
                                 bool  waitForAutoNegotiation )
{
    bool success;

    if (medium == 0)
        medium = IONetworkMedium::getMediumWithIndex(
                                  fMediumDict, kMediumIndexAuto);
    if (medium == 0)
        return kIOReturnUnsupported;

    setLinkStatus(0);  // report link status unknown

    fLinkDownPollHoldOff = 3;

    success = phySetMedium( medium, waitForAutoNegotiation );
    if (success)
    {
        setCurrentMedium( medium );
    }

    return success ? kIOReturnSuccess : kIOReturnIOError;
}

//---------------------------------------------------------------------------

bool CLASS::setLinkStatus( UInt32                  status,
                           const IONetworkMedium * medium,
                           UInt64                  speed,
                           OSData *                data )
{
    fLinkStatus = status;
    return super::setLinkStatus( status, medium, speed, data );
}
