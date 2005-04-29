/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __APPLE_3COM_3C90X_DEFINES_H
#define __APPLE_3COM_3C90X_DEFINES_H

#include <IOKit/IOTypes.h>

/*
 * Macros
 */
#define DefineWindowRegisterAccessors( b, w, o, n )  \
inline void set ## n( UInt ## b value )              \
{                                                    \
    setRegisterWindow( w );                          \
    writeRegister ## b( o, value );                  \
}                                                    \
inline UInt ## b get ## n()                          \
{                                                    \
    setRegisterWindow( w );                          \
    return readRegister ## b( o );                   \
}

#define DefineGlobalRegisterAccessors( b, o, n )     \
inline void set ## n( UInt ## b value )              \
{                                                    \
    writeRegister ## b( o, value );                  \
}                                                    \
inline UInt ## b get ## n()                          \
{                                                    \
    return readRegister ## b( o );                   \
}

#define Bit( p ) (1 << p)

#define BitField( p, w, r, b )    \
        k ## r ## b ## Shift = p, \
        k ## r ## b ## Mask  = (((1 << (w)) - 1) << p),

#define GetBitField( r, b, v ) \
        (((v) & k ## r ## b ## Mask) >> k ## r ## b ## Shift)

#define SetBitField( r, b, v ) \
        (((v) << k ## r ## b ## Shift) & k ## r ## b ## Mask)

#define ReadBitField( r, b )   \
        ((( get##r() & k ## r ## b ## Mask) >> k ## r ## b ## Shift))

/*
 * Ring size constants.
 */
#define kTxRingSize  64
#define kRxRingSize  32

enum {
    kInvalidRegisterWindow = 0xff
};

/*
 * Size of the aperture for I/O space mapped registers.
 */
enum {
    kApertureSizeBoomerang = 0x40,
    kApertureSizeCyclone   = 0x80
};

/*
 * Size of the EEPROM in 16-bit words.
 */
enum {
    kEEPROMSizeBoomerang = 0x18,
    kEEPROMSizeCyclone   = 0x21,
    kEEPROMSizeTornado   = 0x100
};

/*
 * Packet descriptors must be aligned on 8-byte boundaries.
 * The Cyclone has a Type 1 transmit descriptor format which
 * requires 16-byte alignment. The Type 1 format is not used.
 */
enum {
    kDescriptorAlignment = 8
};

/*
 * 3C90xB ASIC revisions.
 */
enum {
    kASIC_40_0502_00x = 0,
    kASIC_40_0483_00x = 1,
    kASIC_40_0476_001 = 3,
    kASIC_Unknown
};

/*
 * EtherLinkXL Adapter type.
 */
enum AdapterType {
    kAdapterType3C90x = 0,  // Boomerang
    kAdapterType3C90xB,     // Cyclone/Hurricane
    kAdapterType3C90xC,     // Tornado
    kAdapterTypeCount
};

/*
 * Media ports.
 */
enum MediaPort {
    kMediaPort10BaseT    = 0,
    kMediaPortAUI        = 1,
    kMediaPortAuto       = 2,
    kMediaPortDefault    = 2,
    kMediaPort10Base2    = 3,
    kMediaPort100BaseTX  = 4,
    kMediaPort100BaseFX  = 5,
    kMediaPortMII        = 6,
    kMediaPort100BaseT4  = 7,    // not in spec.
    kMediaPortAutoNeg    = 8,
    kMediaPortCount
};

/*
 * MII/PHY link masks.
 */

typedef UInt32 MIILink;

enum {
    kMIILinkNone     = 0,
    kMIILink10BT     = Bit(0),
    kMIILink10BT_FD  = Bit(1),
    kMIILink100TX    = Bit(2),
    kMIILink100TX_FD = Bit(3),
    kMIILink100T4    = Bit(4),
    kMIILinkNway     = Bit(31)
};

#define kMIILinkMask ( kMIILink10BT     | \
                       kMIILink10BT_FD  | \
                       kMIILink100TX    | \
                       kMIILink100TX_FD | \
                       kMIILink100T4 )

enum LinkSpeed {
    kLinkSpeed10,
    kLinkSpeed100
};

enum DuplexMode {
    kHalfDuplex,
    kFullDuplex
};

enum LinkState {
    kLinkFail,
    kLinkPass,
    kLinkChanged
};

enum {
    kRxPacketBufferSize = ( 1514 + 4 + 4 )
};

enum {
    kPeriodicTimerMSInterval = 3000
};

/*
 * PHY data types.
 */
typedef UInt8  PHYAddress;
typedef UInt8  PHYRegAddr;
typedef UInt16 PHYWord;

/*
 * Media program codes.
 */
enum {
    kMediaCodeDef     = 0x0000,
    kMediaCodeSQE     = 0x0008,
    kMediaCode10TP    = 0x00C0,
    kMediaCodeLink    = 0x0080,
    kMediaCodeMask    = 0x00CC
};

/*
 * MediaPortInfo structure.
 */
typedef struct {
    char *   name;         // string description
    UInt16   speed;        // port speed in Mb/s
    UInt16   mediaCode;    // written to MediaStatus register
    bool     selectable;   // true if selectable by user
    UInt32   ioType;       // IOKit media type
} MediaPortInfo;

/*
 * Media state structure.
 */
typedef struct {
    MediaPort   mediaPort;         // media port to use
    MIILink     phyLink;           // selected MII link
    MIILink     phyLinkActive;     // active MII link.
    PHYAddress  phyAddress;        // PHY address (0..31)
    PHYWord     phyStatus;         // saved PHY STATUS register value
    UInt32      phyID;             // 32-bit PHY ID
    
    
    
//    MIILink     phyNoNwayLink;     // Non Nway MII link.
    bool        phyConfigured;     // PHY configuration done
    DuplexMode  duplexMode;        // full or half duplex
    LinkSpeed   linkSpeed;         // 10 or 100 Mb/s
//    bool        xcvrSelected;      // transceiver port selected
    UInt32      linkStatus;        // Mask of IOKit link status bits
    UInt32      miiLinkMask;       // Mask of all available MII links
    UInt32      mediaPortMask;     // Mask of all available media ports
    UInt32      mediaPortFDMask;   // Mask of FD capability per port
    bool        syncStatus;
} MediaState;

/*
 * AdapterInfo structure.
 */
typedef struct {
    UInt16       deviceID;   // 16-bit PCI device ID
    AdapterType  type;       // type of controller onboard
    char *       name;       // string description
} AdapterInfo;

extern const AdapterInfo   adapterInfoTable[];
extern const UInt32        adapterInfoTableCount;
extern const MediaPortInfo mediaPortTable[];

/*
 * Controller command codes.
 * Upper 5 bits of Command register.
 */
enum {
    #define CommandCode(x) ((x)<<11)

    GlobalReset        = CommandCode(0),
    SelectWindow       = CommandCode(1),
    EnableDCConv       = CommandCode(2),
    RxDisable          = CommandCode(3),
    RxEnable           = CommandCode(4),
    RxReset            = CommandCode(5),
    UpStall            = CommandCode(6),
    UpUnStall          = CommandCode(6) + 1,
    DnStall            = CommandCode(6) + 2,
    DnUnStall          = CommandCode(6) + 3,
    TxDone             = CommandCode(7),
    RxDiscard          = CommandCode(8),
    TxEnable           = CommandCode(9),
    TxDisable          = CommandCode(10),
    TxReset            = CommandCode(11),
    ReqIntr            = CommandCode(12),
    AckIntr            = CommandCode(13),
#define kAckInterruptLatch    0x01
#define kAckTxAvailable       0x08
#define kAckRxEarly           0x20
#define kAckIntRequested      0x40
#define kAckLinkEvent         0x100 // read AutoNegExp to ack LinkEvents
#define kAckDnComplete        0x200
#define kAckUpComplete        0x400
    SetIntrEnable      = CommandCode(14),
    SetIndEnable       = CommandCode(15),
    SetRxFilter        = CommandCode(16),
#define kFilterIndividual     0x01
#define kFilterMulticast      0x02
#define kFilterBroadcast      0x04
#define kFilterPromiscuous    0x08
#define kFilterMulticastHash  0x10
    SetRxEarlyThresh   = CommandCode(17),
    SetTxAvailThresh   = CommandCode(18),
    SetTxStartThresh   = CommandCode(19),
    StartDMAUp         = CommandCode(20),
    StartDMADown       = CommandCode(20) + 1,
    StatsEnable        = CommandCode(21),
    StatsDisable       = CommandCode(22),
    DisableDCConv      = CommandCode(23),
    SetTxReclaimThresh = CommandCode(24),
    SetHashFilterBit   = CommandCode(25),
#define kHashFilterBitOn      0x0400
#define kHashFilterBitOff     0x0000
};

/*
 * EEPROM offsets
 */
enum {
    eepromAddr0     = 0x00,
    eepromAddr1     = 0x01,
    eepromAddr2     = 0x02,
    eepromID        = 0x03,
    eepromDate      = 0x04,
    eepromDiv       = 0x05,
    eepromCode      = 0x06,
    eepromManID     = 0x07,
    eepromPCIParm   = 0x08,
    eepromRomInfo   = 0x09,
    eepromOAddr0    = 0x0a,
    eepromOAddr1    = 0x0b,
    eepromOAddr2    = 0x0c,
    eepromSoftInfo  = 0x0d,
    eepromComp      = 0x0e,
    eepromSoftInfo2 = 0x0f,
    eepromCapable   = 0x10,
    eepromRsvd1     = 0x11,
    eepromConfig0   = 0x12,
    eepromConfig1   = 0x13,
    eepromRsvd2     = 0x14,
    eepromSoftInfo3 = 0x15,
    eepromRsvd3     = 0x16,

    /* 3C90xB (Cylone/Hurricane) only fields */

    eepromSubVenID  = 0x17, // checksum for Boomerang
    eepromSubID     = 0x18,
    eepromMediaOpt  = 0x19,
    eepromRsvd4     = 0x1a,
    eepromRsvd5     = 0x1b,
    eepromRsvd6     = 0x1c,
    eepromRsvd7     = 0x1d,
    eepromRsvd8     = 0x1e,
    eepromRsvd9     = 0x1f,
    eepromChecksum  = 0x20,
    
    eepromChecksum2 = 0xff, //  4K EEPROM
    eepromChecksum3 = 0x3ff // 16K EEPROM
};

/*
 * Command/Status register
 */
enum {
    BitField(  0, 1, CommandStatus, InterruptLatch      )
    BitField(  1, 1, CommandStatus, HostError           )
    BitField(  2, 1, CommandStatus, TxComplete          )
    BitField(  3, 1, CommandStatus, TxAvailable         )
    BitField(  4, 1, CommandStatus, RxComplete          )
    BitField(  5, 1, CommandStatus, RxEarly             )
    BitField(  6, 1, CommandStatus, IntRequested        )
    BitField(  7, 1, CommandStatus, UpdateStats         )
    BitField(  8, 1, CommandStatus, LinkEvent           )
    BitField(  9, 1, CommandStatus, DnComplete          )
    BitField( 10, 1, CommandStatus, UpComplete          )
    BitField( 11, 1, CommandStatus, BusMasterInProgress )
    BitField( 12, 1, CommandStatus, CmdInProgress       )
    BitField( 13, 3, CommandStatus, CurrentWindow       )
};

/*
 * EEPROM Command Register
 */
enum {
    BitField(  0, 6, EEPROMCommand, Address     )
    BitField(  6, 2, EEPROMCommand, Opcode      )
    BitField(  8, 4, EEPROMCommand, AddressHigh )  // 3C90xC
    BitField( 15, 1, EEPROMCommand, Busy        )

    kEEPROMOpcodeEnable = SetBitField( EEPROMCommand, Opcode, 0 ),
    kEEPROMOpcodeWrite  = SetBitField( EEPROMCommand, Opcode, 1 ),
    kEEPROMOpcodeRead   = SetBitField( EEPROMCommand, Opcode, 2 ),
    kEEPROMOpcodeErase  = SetBitField( EEPROMCommand, Opcode, 3 ),
};

/*
 * Power Management Event
 */
enum {
    BitField(  1, 1, PowerMgmtEvent, MagicPktEnable )
};

/*
 * InternalConfig Register
 */
enum {
    BitField(  0, 3, InternalConfig, RAMSize        ) // 3C90x
    BitField(  3, 1, InternalConfig, RAMWidth       ) // 3C90x
    BitField(  6, 2, InternalConfig, ROMSize        )
    BitField(  8, 1, InternalConfig, DisableBadSSDDetect )
    BitField(  9, 1, InternalConfig, RAMLocation    ) // 3C90x
    BitField( 14, 1, InternalConfig, EnableTxLarge  ) // 3C90xB
    BitField( 15, 1, InternalConfig, EnableRxLarge  ) // 3C90xB
    BitField( 16, 2, InternalConfig, RamPartition   ) // 3C90x
    BitField( 20, 4, InternalConfig, XcvrSelect     )
    BitField( 24, 1, InternalConfig, AutoSelect     )
    BitField( 25, 1, InternalConfig, DisableBIOSROM ) // 3C90xB
};

/*
 * MediaOptions Register
 *
 * R/W for 3C90x (used to be called ResetOptions in 3C90x)
 * RO  for 3C90xB
 */
enum {
    BitField( 0, 1, MediaOptions, BaseT4   )
    BitField( 1, 1, MediaOptions, BaseTX   )
    BitField( 2, 1, MediaOptions, BaseFX   )
    BitField( 3, 1, MediaOptions, Base10BT )
    BitField( 4, 1, MediaOptions, BaseCoax )
    BitField( 5, 1, MediaOptions, BaseAUI  )
    BitField( 6, 1, MediaOptions, BaseMII  )
    BitField( 8, 1, MediaOptions, Base10FL )
};

/*
 * ResetOptions Register
 *
 * 3C90xB only register. This is NOT the same ResetOptions register
 * documented in the Boomerang (3C90x) doc.
 */
enum {
    BitField(  0, 3, ResetOptions, FeatureSet     )
    BitField(  3, 1, ResetOptions, D3ResetDisable )
    BitField(  4, 1, ResetOptions, DisableAdvFD   )
    BitField(  5, 1, ResetOptions, DisableAdv100  )
    BitField(  6, 1, ResetOptions, DisableAutoNeg )
    BitField(  7, 1, ResetOptions, DebugMode      )
    BitField(  8, 1, ResetOptions, FastAutoNeg    )
    BitField(  9, 1, ResetOptions, FastEE         )
    BitField( 10, 1, ResetOptions, ForcedConfig   )
    BitField( 11, 1, ResetOptions, TestPDTPDR     )
    BitField( 12, 1, ResetOptions, Test100TX      )
    BitField( 13, 1, ResetOptions, Test100RX      )
};

/*
 * NetworkDiagnostic Register
 */
enum {
    BitField(  0, 1, NetworkDiagnostic, TestLowVoltageDetector )
    BitField(  1, 5, NetworkDiagnostic, AsicRevision      )
    BitField(  6, 1, NetworkDiagnostic, UpperByteEnable   )
    BitField(  7, 1, NetworkDiagnostic, StatisticsEnabled )
    BitField(  8, 1, NetworkDiagnostic, TxFatalError      )
    BitField(  9, 1, NetworkDiagnostic, Transmitting      )
    BitField( 10, 1, NetworkDiagnostic, RxEnabled         )
    BitField( 11, 1, NetworkDiagnostic, TxEnabled         )
    BitField( 12, 1, NetworkDiagnostic, FIFOLoopback      )
    BitField( 13, 1, NetworkDiagnostic, MACLoopback       )
    BitField( 14, 1, NetworkDiagnostic, EndecLoopback     )
    BitField( 15, 1, NetworkDiagnostic, ExternalLoopback  )
};

/*
 * MacControl Register
 */
enum {
    BitField(  0, 1, MacControl, DeferExtendEnable    )
    BitField(  1, 4, MacControl, DeferTimerSelect     )
    BitField(  5, 1, MacControl, FullDuplexEnable     )
    BitField(  6, 1, MacControl, AllowLargePackets    )
    BitField(  7, 1, MacControl, ExtendAfterCollision ) // 3C90xB
    BitField(  8, 1, MacControl, FlowControlEnable    ) // 3C90xB
    BitField(  9, 1, MacControl, VLTEnable            ) // 3C90xB
};

/*
 * Media Type and Status Register
 */
enum {
    BitField(  1, 1, MediaStatus, DataRate100        )
    BitField(  2, 1, MediaStatus, CRCStripDisable    ) // 3C90xB
    BitField(  3, 1, MediaStatus, EnableSQEStats     )
    BitField(  4, 1, MediaStatus, CollisionDetect    )
    BitField(  5, 1, MediaStatus, CarrierSense       )
    BitField(  6, 1, MediaStatus, JabberGuardEnabled )
    BitField(  7, 1, MediaStatus, LinkBeatEnabled    )
    BitField(  9, 1, MediaStatus, JabberDetect       )
    BitField( 10, 1, MediaStatus, PolarityReversed   )
    BitField( 11, 1, MediaStatus, LinkDetect         )
    BitField( 12, 1, MediaStatus, TxInProg           )
    BitField( 14, 1, MediaStatus, DCConverterEnabled )
    BitField( 15, 1, MediaStatus, AUIDisabled        )
};

/*
 * PhysicalMgmt Register
 */
enum {
    BitField( 0, 1, PhysicalMgmt, Clk  )
    BitField( 1, 1, PhysicalMgmt, Data )
    BitField( 2, 1, PhysicalMgmt, Dir  )

    kPhysicalMgmtDirWrite = SetBitField( PhysicalMgmt, Dir, 1 ),
    kPhysicalMgmtDirRead  = SetBitField( PhysicalMgmt, Dir, 0 )
};

/*
 * TxStatus Register
 */
enum {
    BitField( 1, 1, TxStatus, ReclaimError       )
    BitField( 2, 1, TxStatus, Overflow           )
    BitField( 3, 1, TxStatus, MaxCollisions      )
    BitField( 4, 1, TxStatus, Underrun           )
    BitField( 5, 1, TxStatus, Jabber             )
    BitField( 6, 1, TxStatus, InterruptRequested )
    BitField( 7, 1, TxStatus, Complete           )
};

/*
 * TxFreeThresh Register
 */
enum {
    kTxFreeTreshSizePacket = 6
};

/*
 * DMACtrl Register (was PktStatus on earlier NICs)
 */
enum {
    BitField(  1, 1, DMACtrl, DnCmplReq         ) 
    BitField(  2, 1, DMACtrl, DnStalled         )
    BitField(  3, 1, DMACtrl, UpComplete        )
    BitField(  4, 1, DMACtrl, DnComplete        )
    BitField(  5, 1, DMACtrl, UpRxEarlyEnable   )
    BitField(  6, 1, DMACtrl, ArmCountdown      )
    BitField(  7, 1, DMACtrl, DnInProg          )
    BitField(  8, 1, DMACtrl, CounterSpeed      )
    BitField(  9, 1, DMACtrl, CountdownMode     )
    BitField( 16, 1, DMACtrl, UpAltSeqDisable   ) // 3C90xB
    BitField( 20, 1, DMACtrl, DefeatMWI         ) // 3C90xB
    BitField( 21, 1, DMACtrl, DefeatMRL         ) // 3C90xB
    BitField( 22, 1, DMACtrl, UpOverDiscDisable ) // 3C90xB
    BitField( 30, 1, DMACtrl, TargetAbort       )
    BitField( 31, 1, DMACtrl, MasterAbort       )
};

/*
 * DnBurstThresh Register (3C90xB only)
 */
enum {
    kDnBurstThreshMax = 0x1f
};

/*
 * DnPriorityThresh Register (3C90xB only)
 */
enum {
    kDnPriorityThreshMax = 0x3f
};

/*
 * TxReclaimThresh Register (3C90xB only)
 */
enum {
    kTxReclaimThreshMin = 0x04,
    kTxReclaimThreshMax = 0xff
};

/*
 * TxStartThresh Register
 */
enum {
    kTxStartThreshMin = 64,
    kTxStartThreshMax = 0x1fff
};

/*
 * UpBurstThresh Register (3C90xB only)
 */
enum {
    kUpBurstThreshMax = 0x1f
};

/*
 * UpPriorityThresh Register (3C90xB only)
 */
enum {
    kUpPriorityThreshMax = 0x1f
};

/*
 * Station address and masks
 */
enum {
    kStationAddressWindow     = 2,
    kStationAddressOffset     = 0,
    kStationAddressMaskWindow = 2,
    kStationAddressMaskOffset = 6 
};

/*****************************************************/
/*************  Transmit data structures  ************/
/*****************************************************/

/*
 * Reserve 5 fragments for transmit mbuf chains.
 *
 * For a long chain of mbuf which exceeds the number of fragment slots
 * in the descriptor, the driver will coalesce the mbufs into a single
 * cluster mbuf.
 */
enum {
    kTxFragmentCount = 5
};

/*
 * Descriptor Header (Frame Start Header) 
 */
enum {
    BitField(  0,  13, TxDescHeader, Length           )
    BitField(  13,  1, TxDescHeader, CRCAppendDisable )
    BitField(  15,  1, TxDescHeader, TxIndicate       )
    BitField(  16,  1, TxDescHeader, DnComplete       )
    BitField(  25,  1, TxDescHeader, AddIPChecksum    )
    BitField(  26,  1, TxDescHeader, AddTCPChecksum   )
    BitField(  27,  1, TxDescHeader, AddUDPChecksum   )
    BitField(  28,  1, TxDescHeader, RoundUpDefeat    )
    BitField(  29,  1, TxDescHeader, DPDEmpty         )
    BitField(  31,  1, TxDescHeader, DnIndicate       )

    // New in 3C90xB. The DPD Length field is ignored.
    BitField(  0,   2, TxDescHeader, RoundUpBoundary  )
    BitField(  2,   8, TxDescHeader, PacketID         )
};

/*
 * DownLoad/Transmit fragment command.
 */
enum {
    BitField(  0,  13, TxFragment, Length ) 
    BitField(  31,  1, TxFragment, Last   )
};

/*
 * Each transmit fragment consists of:
 * address: fragment physical address
 * command: fragment command and length in bytes
 */
typedef struct {
    UInt32  address;
    UInt32  command;
} BufferFragment;

/*
 * DownLoad / Transmit Packet Descriptor.
 * Must be aligned on 8-byte address boundary.
 */
typedef struct _TxDescriptor
{
    IOPhysicalAddress  nextPtr;
    volatile UInt32    header;
    BufferFragment     fragments[ kTxFragmentCount ];

    /* Driver Private Area (ignored by the hardware) */

    _TxDescriptor *    drvNext;
    _TxDescriptor *    drvPrevious;
    IOPhysicalAddress  drvPhysAddr;
    mbuf_t             drvMbuf;
} TxDescriptor;

/*****************************************************/
/*************  Receive data structures  *************/
/*****************************************************/

/*
 * Receive descriptors always points to a single cluster mbuf.
 * Assuming that a cluster is less than a page size, this means that
 * only two fragments are need in case it a page boundary is crossed.
 */
enum {
    kRxFragmentCount = 2
};

/*
 * Descriptor Status (UpLoad Packet Status)
 * Updated by Boomerang to report packet reception status.
 */
enum {
    BitField(  0,  13, RxDescStatus, Length           )
    BitField(  14,  1, RxDescStatus, UpError          )
    BitField(  15,  1, RxDescStatus, UpComplete       )
    BitField(  16,  1, RxDescStatus, UpOverrun        )
    BitField(  17,  1, RxDescStatus, RuntFrame        )
    BitField(  18,  1, RxDescStatus, AlignmentError   )
    BitField(  19,  1, RxDescStatus, CRCError         )
    BitField(  20,  1, RxDescStatus, OversizedFrame   )
    BitField(  23,  1, RxDescStatus, DribbleBits      )
    BitField(  24,  1, RxDescStatus, UpOverflow       )

    // New in 3C90xB
    BitField(  25,  1, RxDescStatus, IPChecksumError  )
    BitField(  26,  1, RxDescStatus, TCPChecksumError )
    BitField(  27,  1, RxDescStatus, UDPChecksumError )
    BitField(  29,  1, RxDescStatus, IPChecksumChecked )
    BitField(  30,  1, RxDescStatus, TCPChecksumChecked )
    BitField(  31,  1, RxDescStatus, UDPChecksumChecked )

};

/*
 * UpLoad/Receive fragment command.
 */
enum {
    BitField(  0,  13, RxFragment, Length ) 
    BitField(  31,  1, RxFragment, Last   )
};

/*
 * UpLoad / Receive Packet Descriptor.
 * Must be aligned on 8-byte address boundary.
 */
typedef struct _RxDescriptor
{
    IOPhysicalAddress  nextPtr;
    volatile UInt32    status;
    BufferFragment     fragments[ kRxFragmentCount ];

    /* Driver Private Area (ignored by the hardware) */

    _RxDescriptor *    drvNext;
    mbuf_t             drvMbuf;
} RxDescriptor;

/*************************************************************************/
/************************  MII/PHY definitions  **************************/
/*************************************************************************/

/*
 * MII command frame (32-bits) as documented in IEEE 802.3u spec.
 */
enum {
    BitField(  0, 16,  MIIFrame, Data    ) 
    BitField( 16,  2,  MIIFrame, TA      )
    BitField( 18,  5,  MIIFrame, RegAddr )
    BitField( 23,  5,  MIIFrame, PhyAddr )
    BitField( 28,  2,  MIIFrame, Opcode  )
    BitField( 30,  2,  MIIFrame, ST      )

    kMIIFrameTAWrite     = SetBitField( MIIFrame, TA,     2 ),
    kMIIFrameOpcodeWrite = SetBitField( MIIFrame, Opcode, 1 ),
    kMIIFrameOpcodeRead  = SetBitField( MIIFrame, Opcode, 2 ),
    kMIIFrameStart       = SetBitField( MIIFrame, ST,     1 )
};

#define kMIIDelay          1
#define kMIIFramePreamble  0xFFFFFFFF
#define kMIIFrameSize      32

enum {
    kPHYAddrMin        = 0,
    kPHYAddrCyclone    = 24,
    kPHYAddrMax        = 31,
    kPHYAddressInvalid = 0xff
};

enum {
    kPHYIDInvalid = 0x00000000
};

/*
 * MII register address
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
 * MII control register.
 */
enum {
    BitField(  7, 1, MIIControl, CollisionTest      )
    BitField(  8, 1, MIIControl, FullDuplex         )
    BitField(  9, 1, MIIControl, RestartNegotiation )
    BitField( 10, 1, MIIControl, Isolate            )
    BitField( 11, 1, MIIControl, PowerDown          )
    BitField( 12, 1, MIIControl, AutoNegotiation    )
    BitField( 13, 1, MIIControl, SpeedSelection     )
    BitField( 14, 1, MIIControl, Loopback           )
    BitField( 15, 1, MIIControl, Reset              )
};

/*
 * MII status register.
 */
enum {
    BitField(  0, 1, MIIStatus, ExtendedCapable     )
    BitField(  1, 1, MIIStatus, JabberDetect        )
    BitField(  2, 1, MIIStatus, LinkValid           )
    BitField(  3, 1, MIIStatus, NegotiationAbility  )
    BitField(  4, 1, MIIStatus, RemoteFault         )
    BitField(  5, 1, MIIStatus, NegotiationComplete )
    BitField( 11, 5, MIIStatus, Links               )
    BitField( 11, 1, MIIStatus, 10BASET             )
    BitField( 12, 1, MIIStatus, 10BASET_FD          )
    BitField( 13, 1, MIIStatus, 100BASETX           )
    BitField( 14, 1, MIIStatus, 100BASETX_FD        )
    BitField( 15, 1, MIIStatus, 100BASET4           )
};

/*
 * MII auto-negotiation advertisement register.
 */
enum {
    BitField(  0, 5, MIIAdvertisement, Protocol     )
    BitField(  5, 5, MIIAdvertisement, Links        )
    BitField(  5, 1, MIIAdvertisement, 10BASET      )
    BitField(  6, 1, MIIAdvertisement, 10BASET_FD   )
    BitField(  7, 1, MIIAdvertisement, 100BASETX    )
    BitField(  8, 1, MIIAdvertisement, 100BASETX_FD )
    BitField(  9, 1, MIIAdvertisement, 100BASET4    )
    BitField( 10, 1, MIIAdvertisement, PauseCapable )
    BitField( 13, 1, MIIAdvertisement, RemoteFault  )
    BitField( 14, 1, MIIAdvertisement, Acknowledge  )    
};

/*
 * MII link partner ability register.
 */
enum {
    BitField(  0, 5, MIILinkPartner, Protocol     )
    BitField(  5, 5, MIILinkPartner, Links        )
    BitField(  5, 1, MIILinkPartner, 10BASET      )
    BitField(  6, 1, MIILinkPartner, 10BASET_FD   )
    BitField(  7, 1, MIILinkPartner, 100BASETX    )
    BitField(  8, 1, MIILinkPartner, 100BASETX_FD )
    BitField(  9, 1, MIILinkPartner, 100BASET4    )
    BitField( 13, 1, MIILinkPartner, RemoteFault  )
    BitField( 14, 1, MIILinkPartner, Acknowledge  )  
};

/*
 * PHY timeout parameters in milliseconds.
 */
enum {
    kPHYResetDelay   = 10,
    kPHYResetTimeout = 100,

    kPHYLinkDelay    = 20,
    kPHYLinkTimeout  = 5000,
};

#endif /* !__APPLE_3COM_3C90X_DEFINES_H */
