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

#include <libkern/c++/OSObject.h>
#include <IOKit/IOLib.h>

#define PRE_NWAY_RESET    1
#define PRE_FORCE_RESET   1

#define POST_FORCE_RESET  0
#define POST_NWAY_RESET   0

#ifdef  DEBUG
#define PHY_LOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define PHY_LOG(fmt, args...)
#endif

typedef UInt32 PHYAddr;
typedef UInt16 PHYWord;
typedef UInt16 PHYReg;
typedef UInt32 MIILink;

typedef bool (*MDIReadFunc)(void * owner, PHYAddr phyAddr,
                            PHYReg phyReg, PHYWord * phyData);

typedef bool (*MDIWriteFunc)(void * owner, PHYAddr phyAddr,
                             PHYReg phyReg, PHYWord phyData);

/*
 * PHY Address
 */
enum {
    kPHYAddrMin     = 0,
    kPHYAddrMax     = 31,
    kPHYAddrInvalid = 0xff
};

/*
 * Various timeout values in milliseconds units
 */
enum {
    kPHYResetPause       = 10,
    kPHYResetTimeout     = 100,
    kPHYDefaultLinkWait  = 5000
};

/*
 * Option flags for checkForLinkChange()
 */
enum {
    kPHYLinkChangePoll = 0x01
};

enum {
    kMIILinkNone         = 0,
    kMIILink10BASET      = 0x0001,
    kMIILink10BASET_FD   = 0x0002,
    kMIILink100BASETX    = 0x0004,
    kMIILink100BASETX_FD = 0x0008,
    kMIILink100BASET4    = 0x0010,
    kMIILinkMask         = 0x001F,
    kMIILinkAutoNeg      = 0x8000
};

/*
 * MII Register Offsets
 */
enum {
    kMIIRegisterControl        = 0,
    kMIIRegisterStatus         = 1,
    kMIIRegisterID0            = 2,
    kMIIRegisterID1            = 3,
    kMIIRegisterAdvertisement  = 4,
    kMIIRegisterLinkPartner    = 5,
    kMIIRegisterExpansion      = 6,
    kMIIRegisterNextPage       = 7,
};

/*
 * Control Register
 */
#define kMIIControlCollisionTest           0x0080
#define kMIIControlFullDuplex              0x0100
#define kMIIControlRestartAutoNegotiation  0x0200
#define kMIIControlIsolate                 0x0400
#define kMIIControlPowerDown               0x0800
#define kMIIControlEnableAutoNegotiation   0x1000
#define kMIIControlSpeedSelection          0x2000
#define kMIIControlLoopback                0x4000
#define kMIIControlReset                   0x8000

/*
 * Status Register
 */
#define kMIIStatusExtendedCapable          0x0001
#define kMIIStatusJabberDetect             0x0002
#define kMIIStatusLinkValid                0x0004
#define kMIIStatusAutoNegotiationAbility   0x0008
#define kMIIStatusRemoteFault              0x0010
#define kMIIStatusAutoNegotiationComplete  0x0020
#define kMIIStatus10BASET                  0x0800
#define kMIIStatus10BASET_FD               0x1000
#define kMIIStatus100BASETX                0x2000
#define kMIIStatus100BASETX_FD             0x4000
#define kMIIStatus100BASET4                0x8000
#define kMIIStatusLinkMask                 0xF800
#define kMIIStatusLinkShift                11

/*
 * Auto-Negotiation Advertisement Register
 */
#define kMIIAdvertisementProtocolMask      0x001F
#define kMIIAdvertisement10BASET           0x0020
#define kMIIAdvertisement10BASET_FD        0x0040
#define kMIIAdvertisement100BASETX         0x0080
#define kMIIAdvertisement100BASETX_FD      0x0100
#define kMIIAdvertisement100BASET4         0x0200
#define kMIIAdvertisementPauseCapable      0x0400
#define kMIIAdvertisementRemoteFault       0x2000
#define kMIIAdvertisementAcknowledge       0x4000
#define kMIIAdvertisementLinkMask          0x03E0
#define kMIIAdvertisementLinkShift         5

/*
 * Link Partner Ability Register
 */
#define kMIILinkPartnerProtocolMask        0x001F
#define kMIILinkPartner10BASET             0x0020
#define kMIILinkPartner10BASET_FD          0x0040
#define kMIILinkPartner100BASETX           0x0080
#define kMIILinkPartner100BASETX_FD        0x0100
#define kMIILinkPartner100BASET4           0x0200
#define kMIILinkPartnerRemoteFault         0x2000
#define kMIILinkPartnerAcknowledge         0x4000
#define kMIILinkPartnerLinkMask            0x03E0
#define kMIILinkPartnerLinkShift           5


class AppleBCM440XPHY : public OSObject
{
    OSDeclareDefaultStructors( AppleBCM440XPHY )

protected:
    PHYAddr          fPHYAddr;
    UInt32           fPHYID;
    MIILink          fPHYConfiguredLink;
    MIILink          fPHYActiveLink;
    PHYWord          fPHYStatus;
    PHYWord          fPHYLastStatus;
    void *           fPHYOwner;
    MDIReadFunc      fMDIReadFunc;
    MDIWriteFunc     fMDIWriteFunc;

    virtual bool     mdiReadWord( PHYAddr phyAddr, PHYReg phyReg, PHYWord * phyData );
    
    virtual void     mdiWriteWord( PHYAddr phyAddr, PHYReg phyReg, PHYWord phyData );

    virtual IOReturn waitForStatusBits( UInt32  timeoutMS,
                                        PHYWord statusMask,
                                        PHYWord statusValue );

    virtual void     setConfiguredLink( MIILink configuredLink );

public:
    static AppleBCM440XPHY * BCM440XPHY( void *       owner,
                                         MDIReadFunc  readFunc,
                                         MDIWriteFunc writeFunc );

    virtual bool     init( void *       owner,
                           MDIReadFunc  readFunc,
                           MDIWriteFunc writeFunc );

    virtual IOReturn probePHY( void );

    virtual IOReturn resetPHY( void );

    virtual PHYAddr  getPHYAddress( void ) const;

    virtual UInt32   getPHYIdentifier( void ) const;

    virtual MIILink  getLocalLinkSupportMask( void ) const;

    virtual MIILink  getActiveLink( void ) const;

    virtual IOReturn restartAutoNegotiation( UInt32 waitTimeoutMS );

    virtual IOReturn forceSpeedAndDuplex( MIILink miiLink,
                                          UInt32  waitTimeoutMS );

    virtual IOReturn checkForLinkChange( bool *   linkChanged = 0,
                                         IOOptionBits options = 0 );

    virtual IOReturn programLink( MIILink miiLink, UInt32 waitTimeoutMS );
};
