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

#ifndef _APPLEDP83816_H
#define _APPLEDP83816_H

#include <IOKit/assert.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <libkern/OSByteOrder.h>
extern "C" {
#include <sys/kpi_mbuf.h>
}

#define BIT(bit)        (1 << (bit))
#define SHIFT(bit)      (bit)
#define MASK(bit, m)    ((m) << (bit))

struct RxDesc {
    UInt32          link;
    volatile UInt32 cmdStatus;
    UInt32          bufferPtr;
    mbuf_t          packet;       /* driver use only */
};

struct TxDesc {
    UInt32          link;
    volatile UInt32 cmdStatus;
    UInt32          bufferPtr;
    mbuf_t          packet;       /* driver area below */
    TxDesc *        descLast;     /* last descriptor (MORE bit clear) */
    UInt32          descCount;    /* number of descriptors for packet */
    UInt32          nextIndex;    /* ring index of next slot */
    UInt32          padTo32Bytes;
};

/*
 * Alignment requirements in bytes
 */
#define kDescAlignment              4
#define kTxBufferAlignment          1
#define kRxBufferAlignment          4

/*
 * CmdStatus common bits
 */
#define kDescOwn                    BIT(31)
#define kDescMore                   BIT(30)
#define kDescInterrupt              BIT(29)
#define kDescIncludeCRC             BIT(28)
#define kDescPacketOK               BIT(27)
#define kDescBufferSizeMask         0x00000FFF

/*
 * TX Descriptor Status
 */
#define kDescTxAbort                BIT(26)
#define kDescTxFIFOUnderrun         BIT(25)
#define kDescCarrierSenseLost       BIT(24)
#define kDescTransmitDeferral       BIT(23)
#define kDescExcessiveDeferral      BIT(22)
#define kDescLateCollision          BIT(21)
#define kDescExcessiveCollisions    BIT(20)
#define kDescTxCollisionCountMask   MASK(16, 0xF)
#define kDescTxCollisionCountShift  SHIFT(16)

#define kDescTxAbnormalMask           \
        ( kDescTxAbort              | \
          kDescTxFIFOUnderrun       | \
          kDescCarrierSenseLost     | \
          kDescTransmitDeferral     | \
          kDescExcessiveDeferral    | \
          kDescLateCollision        | \
          kDescExcessiveCollisions  | \
          kDescTxCollisionCountMask )

#define kDescTxErrorMask              \
        ( kDescTxAbort              | \
          kDescTxFIFOUnderrun       | \
          kDescCarrierSenseLost     | \
          kDescExcessiveDeferral    | \
          kDescLateCollision        | \
          kDescExcessiveCollisions  )

/*
 * RX Descriptor Status
 */
#define kDescRxAbort                BIT(26)
#define kDescRxOverrun              BIT(25)
#define kDescDestinationMask        MASK(23, 3)
#define kDescDestinationReject      MASK(23, 0)
#define kDescDestinationUnicast     MASK(23, 1)
#define kDescDestinationMulticast   MASK(23, 2)
#define kDescDestinationBroadcast   MASK(23, 3)
#define kDescTooLong                BIT(22)
#define kDescRuntFrame              BIT(21)
#define kDescSymbolError            BIT(20)
#define kDescFCSError               BIT(19)
#define kDescAlignmentError         BIT(18)
#define kDescLoopback               BIT(17)
#define kDescRxCollision            BIT(16)

/*
 * Command Register
 */
#define REG_CR                      0x0000
#define CR_RST                      BIT(8)
#define CR_SWI                      BIT(7)
#define CR_RXR                      BIT(5)
#define CR_TXR                      BIT(4)
#define CR_RXD                      BIT(3)
#define CR_RXE                      BIT(2)
#define CR_TXD                      BIT(1)
#define CR_TXE                      BIT(0)

/*
 * Configuration Register
 */
#define REG_CFG                     0x0004
#define CFG_LNKSTS                  BIT(31)
#define CFG_SPEED100                BIT(30)
#define CFG_FULLDUPLEX              BIT(29)
#define CFG_POL                     BIT(28)
#define CFG_ANEG_DONE               BIT(27)
#define CFG_PINT_ACEN               BIT(17)
#define CFG_PAUSE_ADV               BIT(16)
#define CFG_EXT_PHY                 BIT(12)
#define CFG_PHY_RST                 BIT(10)
#define CFG_PHY_DIS                 BIT(9)
#define CFG_EUPHCOMP                BIT(8)
#define CFG_REQALG                  BIT(7)
#define CFG_SB                      BIT(6)
#define CFG_POW                     BIT(5)
#define CFG_EXD                     BIT(4)
#define CFG_PESEL                   BIT(3)
#define CFG_BROM_DIS                BIT(2)
#define CFG_BEM                     BIT(0)

/*
 * EEPROM Access Register
 */
#define REG_MEAR                    0x0008
#define MEAR_MDC                    BIT(6)
#define MEAR_MDDIR                  BIT(5)
#define MEAR_MDIO                   BIT(4)
#define MEAR_EESEL                  BIT(3)
#define MEAR_EECLK                  BIT(2)
#define MEAR_EEDO                   BIT(1)
#define MEAR_EEDI                   BIT(0)

/*
 * PCI Test Control Register
 */
#define REG_PTSCR                   0x000C
#define PTSCR_RBIST_RST             BIT(10)
#define PTSCR_RBIST_EN              BIT(7)
#define PTSCR_RBIST_DONE            BIT(6)
#define PTSCR_RBIST_RXFAIL          BIT(5)
#define PTSCR_RBIST_TXFAIL          BIT(4)
#define PTSCR_RBIST_RXFFAIL         BIT(3)
#define PTSCR_EELOAD_EN             BIT(2)
#define PTSCR_EEBIST_EN             BIT(1)
#define PTSCR_EEBIST_FAIL           BIT(0)

/*
 * Interrupt Status Register
 */
#define REG_ISR                     0x0010
#define ISR_TXRCMP                  BIT(25)
#define ISR_RXRCMP                  BIT(24)
#define ISR_DPERR                   BIT(23)
#define ISR_SSERR                   BIT(22)
#define ISR_RMABT                   BIT(21)
#define ISR_RTABT                   BIT(20)
#define ISR_RXSOVR                  BIT(16)
#define ISR_HIBERR                  BIT(15)
#define ISR_PHY                     BIT(14)
#define ISR_PME                     BIT(13)
#define ISR_SWI                     BIT(12)
#define ISR_MIB                     BIT(11)
#define ISR_TXURN                   BIT(10)
#define ISR_TXIDLE                  BIT(9)
#define ISR_TXERR                   BIT(8)
#define ISR_TXDESC                  BIT(7)
#define ISR_TXOK                    BIT(6)
#define ISR_RXORN                   BIT(5)
#define ISR_RXIDLE                  BIT(4)
#define ISR_RXEARLY                 BIT(3)
#define ISR_RXERR                   BIT(2)
#define ISR_RXDESC                  BIT(1)
#define ISR_RXOK                    BIT(0)

/*
 * Interrupt Mask Register
 */
#define REG_IMR                     0x0014
#define IMR_TXRCMP                  BIT(25)
#define IMR_RXRCMP                  BIT(24)
#define IMR_DPERR                   BIT(23)
#define IMR_SSERR                   BIT(22)
#define IMR_RMABT                   BIT(21)
#define IMR_RTABT                   BIT(20)
#define IMR_RXSOVR                  BIT(16)
#define IMR_HIBERR                  BIT(15)
#define IMR_PHY                     BIT(14)
#define IMR_PME                     BIT(13)
#define IMR_SWI                     BIT(12)
#define IMR_MIB                     BIT(11)
#define IMR_TXURN                   BIT(10)
#define IMR_TXIDLE                  BIT(9)
#define IMR_TXERR                   BIT(8)
#define IMR_TXDESC                  BIT(7)
#define IMR_TXOK                    BIT(6)
#define IMR_RXORN                   BIT(5)
#define IMR_RXIDLE                  BIT(4)
#define IMR_RXEARLY                 BIT(3)
#define IMR_RXERR                   BIT(2)
#define IMR_RXDESC                  BIT(1)
#define IMR_RXOK                    BIT(0)

/*
 * Interrupt Enable Register
 */
#define REG_IER                     0x0018
#define IER_IE                      BIT(0)

/*
 * Interrupt Holdoff Register
 */
#define REG_IHR                     0x001C
#define IHR_IHCTL                   BIT(8)
#define IHR_IH_MASK                 MASK(0, 0xFF)

/*
 * Transmit Descriptor Pointer Register
 */
#define REG_TXDP                    0x0020

/*
 * Transmit Configuration Register
 */
#define REG_TXCFG                   0x0024
#define TXCFG_CSI                   BIT(31)
#define TXCFG_HBI                   BIT(30)
#define TXCFG_MLB                   BIT(29)
#define TXCFG_ATP                   BIT(28)
#define TXCFG_IFG1                  BIT(27)
#define TXCFG_IFG0                  BIT(26)
#define TXCFG_ECRETRY               BIT(23)
#define TXCFG_MXDMA_512             MASK(20, 0)
#define TXCFG_MXDMA_4               MASK(20, 1)
#define TXCFG_MXDMA_8               MASK(20, 2)
#define TXCFG_MXDMA_16              MASK(20, 3)
#define TXCFG_MXDMA_32              MASK(20, 4)
#define TXCFG_MXDMA_64              MASK(20, 5)
#define TXCFG_MXDMA_128             MASK(20, 6)
#define TXCFG_MXDMA_256             MASK(20, 7)
#define TXCFG_FLTH_SHIFT            SHIFT(8)
#define TXCFG_FLTH_MASK             MASK(8, 0x3F)
#define TXCFG_DRTH_SHIFT            SHIFT(0)
#define TXCFG_DRTH_MASK             MASK(0, 0x3F)

/*
 * Receive Descriptor Pointer Register
 */
#define REG_RXDP                    0x0030

/*
 * Receive Configuration Register
 */
#define REG_RXCFG                   0x0034
#define RXCFG_AEP                   BIT(31)
#define RXCFG_ARP                   BIT(30)
#define RXCFG_ATX                   BIT(28)
#define RXCFG_ALP                   BIT(27)
#define RXCFG_MXDMA_512             MASK(20, 0)
#define RXCFG_MXDMA_4               MASK(20, 1)
#define RXCFG_MXDMA_8               MASK(20, 2)
#define RXCFG_MXDMA_16              MASK(20, 3)
#define RXCFG_MXDMA_32              MASK(20, 4)
#define RXCFG_MXDMA_64              MASK(20, 5)
#define RXCFG_MXDMA_128             MASK(20, 6)
#define RXCFG_MXDMA_256             MASK(20, 7)
#define RXCFG_DRTH_SHIFT            SHIFT(1)
#define RXCFG_DRTH_MASK             MASK(1, 0x1F)

/*
 * CLKRUN Control/Status Register
 */
#define REG_CCSR                    0x003C
#define CCSR_PMESTS                 BIT(15)
#define CCSR_PMEEN                  BIT(8)
#define CCSR_CLKRUN_EN              BIT(0)

/*
 * Wake on LAN Control/Status Register
 */
#define REG_WCSR                    0x0040
#define WCSR_MPR                    BIT(31)
#define WCSR_PATM3                  BIT(30)
#define WCSR_PATM2                  BIT(29)
#define WCSR_PATM1                  BIT(28)
#define WCSR_PATM0                  BIT(27)
#define WCSR_ARPR                   BIT(26)
#define WCSR_BCASTR                 BIT(25)
#define WCSR_MCASTR                 BIT(24)
#define WCSR_UCASTR                 BIT(23)
#define WCSR_PHYINT                 BIT(22)
#define WCSR_SOHACK                 BIT(20)
#define WCSR_MPSOE                  BIT(10)
#define WCSR_WKMAG                  BIT(9)
#define WCSR_WKPAT3                 BIT(8)
#define WCSR_WKPAT2                 BIT(7)
#define WCSR_WKPAT1                 BIT(6)
#define WCSR_WKPAT0                 BIT(5)
#define WCSR_WKARP                  BIT(4)
#define WCSR_WKBCP                  BIT(3)
#define WCSR_WKMCP                  BIT(2)
#define WCSR_WKUCP                  BIT(1)
#define WCSR_WKPHY                  BIT(0)

/*
 * Pause Control/Status Register
 */
#define REG_PCR                     0x0044
#define PCR_PSEN                    BIT(31)
#define PCR_PS_MCAST                BIT(30)
#define PCR_PS_DA                   BIT(29)
#define PCR_PS_ACT                  BIT(23)
#define PCR_PS_RCVD                 BIT(22)
#define PCR_PSNEG                   BIT(21)
#define PCR_MLD_EN                  BIT(16)
#define PCR_PAUSE_CNT               MASK(0, 0xFFFF)

/*
 * Receive Filter/Match Control Register
 */
#define REG_RFCR                    0x0048
#define RFCR_RFEN                   BIT(31)
#define RFCR_AAB                    BIT(30)
#define RFCR_AAM                    BIT(29)
#define RFCR_AAU                    BIT(28)
#define RFCR_APM                    BIT(27)
#define RFCR_APAT3                  BIT(26)
#define RFCR_APAT2                  BIT(25)
#define RFCR_APAT1                  BIT(24)
#define RFCR_APAT0                  BIT(23)
#define RFCR_AARP                   BIT(22)
#define RFCR_MHEN                   BIT(21)
#define RFCR_UHEN                   BIT(20)
#define RFCR_ULM                    BIT(19)
#define RFCR_RFADDR_PM_1_0          MASK(0, 0)
#define RFCR_RFADDR_PM_3_2          MASK(0, 2)
#define RFCR_RFADDR_PM_5_4          MASK(0, 4)

/*
 * Receive Filter/Match Data Register
 */
#define REG_RFDR                    0x004C
#define RFDR_BMASK                  MASK(16, 0x3)
#define RFDR_RFDATA                 MASK( 0, 0xFFFF)

/*
 * Boot ROM Address Register
 */
#define REG_BRAR                    0x0050
#define BRAR_AUTOINC                BIT(31)
#define BRAR_ADDR                   MASK(0, 0xFFFF)

/*
 * Boot ROM Data Register
 */
#define REG_BRDR                    0x0054

/*
 * Silicon Revision Register
 */
#define REG_SRR                     0x0058
#define SRR_REV_MASK                MASK(0, 0xFFFF)

/*
 * MIB Control Register
 */
#define REG_MIBC                    0x005C
#define MIBC_MIBS                   BIT(3)
#define MIBC_ACLR                   BIT(2)
#define MIBC_FRZ                    BIT(1)
#define MIBC_WRN                    BIT(0)

/*
 * MIB Data Registers
 */
#define REG_RXErrorPkts             0x0060   /* 16 bits */
#define REG_RXFCSErrors             0x0064   /*  8 bits */
#define REG_RXMissedPkts            0x0068   /*  8 bits */
#define REG_RXFAErrors              0x006C   /*  8 bits */
#define REG_RXSymbolErrors          0x0070   /*  8 bits */
#define REG_RXFrameTooLong          0x0074   /*  4 bits */
#define REG_RXSQEErrors             0x0078   /*  4 bits */

/*
 * Internal PHY Registers
 */
#define REG_BMCR                    0x0080
#define REG_BMSR                    0x0084
#define REG_PHYID1                  0x0088
#define REG_PHYID2                  0x008C
#define REG_ANAR                    0x0090
#define REG_ANLPAR                  0x0094
#define REG_ANER                    0x0098
#define REG_ANNPTR                  0x009C
#define REG_PHYSTS                  0x00C0

#define REG_MICR                    0x00C4
#define MICR_INTEN                  BIT(1)
#define MICR_TINT                   BIT(0)

#define REG_MISR                    0x00C8
#define MISR_MINT                   BIT(15)
#define MISR_MSK_LINK               BIT(14)
#define MISR_MSK_JAB                BIT(13)
#define MISR_MSK_RF                 BIT(12)
#define MISR_MSK_ANC                BIT(11)
#define MISR_MSK_FHF                BIT(10)
#define MISR_MSK_RHF                BIT(9)

#define REG_FCSCR                   0x00D0
#define REG_RECR                    0x00D4
#define REG_PCSR                    0x00D8
#define REG_PHYCR                   0x00E4
#define REG_TBTSCR                  0x00E8

#define REG_PGSEL                   0x00CC
#define REG_PMDCSR                  0x00E4
#define REG_DSPCFG                  0x00F4
#define REG_SDCFG                   0x00F8
#define REG_TSTDAT                  0x00FC

#ifdef  DEBUG
#warning ***************************************************
#warning ** DEBUG defined - turn off for deployment build **
#warning ***************************************************
#define DEBUG_LOG(args...)   kprintf(args)
#define ERROR_LOG(args...)   kprintf(args)
#else
#define DEBUG_LOG(args...)
#define ERROR_LOG(args...)   IOLog(args)
#endif

#define WriteReg( reg, data ) \
        do { OSWriteLittleInt32(fRegBase, REG_##reg, (data)); } while (0)

#define ReadReg( reg ) \
        OSReadLittleInt32(fRegBase, REG_##reg)

#define WriteRegFlush( reg, data ) \
        do { OSWriteLittleInt32(fRegBase, REG_##reg, (data)); \
             fRegFlushPostedWrite = OSReadLittleInt32(fRegBase, REG_##reg); } \
             while (0)

#define NET_STAT(var, cnt) \
        do { (fNetStats->var) += (cnt); } while(0)

#define ETH_STAT(var, cnt) \
        do { (fEtherStats->var) += (cnt); } while(0)

#define kTxMaxSegmentCount       8

#define kRxDescCount             256   /* must be power of 2 */
#define kTxDescCount             128   /* must be power of 2 */

#define kRxMaxBufferSize         1520
#define kTxMaxBufferSize         1520

#define kWatchdogTimerPeriodMS   3500
#define kTransmitQueueCapacity   1024

#define kHardwareWaitValue       10
#define kHardwareWaitTimeout     (2500/kHardwareWaitValue)

/*
 * Adapter activation levels.
 */
enum {
    kActivationLevelNone = 0,  /* adapter shut off */
    kActivationLevelKDP,       /* adapter partially up to support KDP */
    kActivationLevelBSD        /* adapter fully up to support KDP and BSD */
};

class AppleDP83816Ethernet : public IOEthernetController
{
    OSDeclareDefaultStructors( AppleDP83816Ethernet )

protected:
    IOEthernetInterface *          fNetif;
    IOPCIDevice *                  fPCINub;
    IOWorkLoop *                   fWorkLoop;
    IOInterruptEventSource *       fInterruptSource;
    IOOutputQueue *                fTransmitQueue;
    IOTimerEventSource *           fWatchdogTimer;
    IONetworkStats *               fNetStats;
    IOEthernetStats *              fEtherStats;
    IOMemoryMap *                  fRegMap;
    volatile void *                fRegBase;
    OSDictionary *                 fMediumDict;

    IOEthernetAddress              fMacAddress;
    UInt32                         fActivationLevel;
    UInt32                         fLinkDownPollHoldOff;
    UInt32                         fLinkStatus;
    bool                           fEnabledForBSD;
    bool                           fEnabledForKDP;
    bool                           fMagicPacketSupported;
    bool                           fMagicPacketEnabled;
    bool                           fFullDuplexMode;

    UInt32                         fCurrentMediumIndex;
    UInt32                         fCurrentPowerState;
    UInt32                         fAutoNegANAR;
    UInt32                         fRegRFCR;
    UInt32                         fRegSRR;
    UInt32                         fRegTXCFG;
    UInt32                         fRegRXCFG;
    UInt32                         fRegCFG;
    UInt32                         fRegFlushPostedWrite;

    IOMbufLittleMemoryCursor *     fRxMbufCursor;
    IOBufferMemoryDescriptor *     fRxDescMemory;
    IOPhysicalAddress              fRxDescPhysAddr;
    RxDesc *                       fRxDescBase;
    UInt32                         fRxHeadIndex;

    IOMbufNaturalMemoryCursor *    fTxMbufCursor;
    IOBufferMemoryDescriptor *     fTxDescMemory;
    IOPhysicalAddress              fTxDescPhysAddr;
    TxDesc *                       fTxDescBase;
    UInt32                         fTxHeadIndex;
    UInt32                         fTxTailIndex;
    UInt32                         fTxInterruptInterval;

    mbuf_t                         fKDPMbuf;
    IOPhysicalSegment              fKDPMbufSeg;
    IOKernelDebugger *             fKDPNub;
    IOPacketQueue *                fKDPQueue;

    static void timeoutHandler( OSObject * owner,
                                 IOTimerEventSource * timer );    

    static void interruptHandler( OSObject * owner,
                                  IOInterruptEventSource * source,
                                  int count );

    void     interruptOccurred( void );

    void     initPCIConfigSpace( IOPCIDevice * pci );

    bool     increaseActivationLevel( UInt32 newLevel );
    
    bool     decreaseActivationLevel( UInt32 currentLevel );

    bool     setActivationLevel( UInt32 level );

    void     disableHardwareInterrupts( void );

    void     enableHardwareInterrupts( void );

    bool     allocateSupportObjects( IOService * provider );

    bool     probeHardware( void );

    bool     swInit( void );

    void     swFree( void );

    bool     hwReset( void );

    void     hwInit( void );

    void     hwStop( void );

    void     updateMACForDuplexModeChange( void );

    void     hwSetMagicPacketEnable( bool enable );

    void     getMacAddress( void );

    bool     allocateRxMemory( void );

    void     releaseRxMemory( void );

    bool     initRxRing( void );

    bool     allocateTxMemory( void );

    bool     initTxRing( void );

    void     releaseTxMemory( void );

    void     setMacAddress( void );

    void     phyInit( void );

    IOReturn phySelectMedium( const IONetworkMedium * medium,
                              bool  waitForAutoNegotiation );

    void     serviceRxInterrupt( void );

    void     recordRxDescriptorErrors( UInt32 rxStatus );

    void     serviceTxInterrupt( void );

    void     recordTxDescriptorErrors( UInt32 txStatus );

    void     waitForFreeTransmitDescriptors( UInt32 freeCount );

    bool     phyAddMediumType( IOMediumType type,
                               UInt32       speed,
                               UInt32       refcon );

    void     phyProbeCapability( void );

    bool     phyReset( void );

    void     phyWaitForAutoNegotiation( void );

    bool     phySetMedium( UInt32 mediumIndex,
                           bool   waitForAutoNegotiation );

    bool     phySetMedium( const IONetworkMedium * medium,
                           bool  waitForAutoNegotiation );

    void     phyReportLinkStatus( void );

    void     phyPollLinkStatus( void );

public:
    virtual bool             start( IOService * provider );

    virtual void             free( void );

    virtual IOReturn         enable(  IONetworkInterface * netif );

    virtual IOReturn         disable( IONetworkInterface * netif );

    virtual IOReturn         enable(  IOKernelDebugger * netif );

    virtual IOReturn         disable( IOKernelDebugger * netif );

    virtual UInt32           outputPacket(
                                     mbuf_t m,
                                     void * param );

    virtual IOOutputQueue *  createOutputQueue( void );

    virtual const OSString * newVendorString( void ) const;

    virtual const OSString * newModelString( void ) const;

    virtual IOReturn         selectMedium(
                                     const IONetworkMedium * medium );

    virtual bool             configureInterface(
                                     IONetworkInterface * interface );

    virtual bool             createWorkLoop( void );

    virtual IOWorkLoop *     getWorkLoop( void ) const;

    virtual IOReturn         getHardwareAddress(
                                     IOEthernetAddress * addr );

    virtual IOReturn         setPromiscuousMode(
                                     bool enabled );

    virtual IOReturn         setMulticastMode(
                                     bool enabled );

    virtual IOReturn         setMulticastList(
                                     IOEthernetAddress * addrs,
                                     UInt32              count );

    virtual IOReturn         registerWithPolicyMaker(
                                     IOService *   policyMaker );

    virtual IOReturn         setPowerState(
                                     unsigned long powerStateOrdinal,
                                     IOService *   policyMaker );

    virtual IOReturn         getPacketFilters(
                                     const OSSymbol * group,
                                     UInt32 *         filters ) const;

    virtual void             getPacketBufferConstraints(
                                     IOPacketBufferConstraints *
                                     constraints ) const;

    virtual IOReturn         setWakeOnMagicPacket( bool active );

    virtual void             sendPacket(
                                     void *   pkt_data,
                                     UInt32   pkt_size );

    virtual void             receivePacket(
                                     void *   pkt_data,
                                     UInt32 * pkt_size,
                                     UInt32   timeout );

    virtual bool             setLinkStatus(
                                     UInt32                  status,
                                     const IONetworkMedium * medium = 0,
                                     UInt64                  speed  = 0,
                                     OSData *                data   = 0 );
};

#endif /* !_APPLEDP83816_H */
