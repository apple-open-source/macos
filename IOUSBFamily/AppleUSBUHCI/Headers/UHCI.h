/*
 * Copyright © 2004-2012 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 
#ifndef _IOKIT_UHCI_H_
#define _IOKIT_UHCI_H_

/* UHCI hardware-specific info. */

/* Registers.   All are 16 bits unless noted. */

enum {
    kUHCI_CMD       = 0x00,
    kUHCI_STS       = 0x02,
    kUHCI_INTR      = 0x04,
    kUHCI_FRNUM     = 0x06,
    kUHCI_FRBASEADDR= 0x08,     /* 32 bits */
    kUHCI_SOFMOD    = 0x0C,
    kUHCI_PORTSC1   = 0x10,
    kUHCI_PORTSC2   = 0x12
};

/* Bits for kUHCI_CMD. */

enum {
    kUHCI_CMD_MAXP    = 0x0080,
    kUHCI_CMD_CF      = 0x0040,
    kUHCI_CMD_SWDBG   = 0x0020,
    kUHCI_CMD_FGR     = 0x0010,
    kUHCI_CMD_EGSM    = 0x0008,
    kUHCI_CMD_GRESET  = 0x0004,
    kUHCI_CMD_HCRESET = 0x0002,
    kUHCI_CMD_RS      = 0x0001
};

/* Bits for kUHCI_STS. */

enum {
    kUHCI_STS_HCH    = 0x0020,
    kUHCI_STS_HCPE   = 0x0010,
    kUHCI_STS_HSE    = 0x0008,
    kUHCI_STS_RD     = 0x0004,
    kUHCI_STS_EI     = 0x0002,
    kUHCI_STS_INT    = 0x0001,
    kUHCI_STS_MASK   = 0x003F,
    kUHCI_STS_INTR_MASK = 0x001F
};

/* Bits for kUHCI_INTR. */

enum {
    kUHCI_INTR_SPIE  = 0x0008,
    kUHCI_INTR_IOCE  = 0x0004,
    kUHCI_INTR_RIE   = 0x0002,
    kUHCI_INTR_TIE   = 0x0001
};

/* Bits for kUHCI_FRNUM. */

enum {
    kUHCI_FRNUM_MASK    = 0x07FF,    /* Bits 0:10. */
    kUHCI_FRNUM_SHIFT   = 11,
    kUHCI_FRNUM_FRAME_MASK = 0x03FF, /* Bits 0:9, which selects the frame number. */
    kUHCI_FRNUM_COUNT   = 0x0800,    /* Number counted by register. */
    kUHCI_NUM_FRAMES    = 1024       /* Number of frames used by controller. */
};

/* Bits for kUHCI_FLBASEADD. */

enum {
    kUHCI_FLBASEADD_BASE = 0xFFFFF000   /* Bits 31:12. */
};

/* Bits for kUHCI_SOF. */

enum {
    kUHCI_SOF_TIMING    = 0x7F      /* Bits 6:0. */
};

// Bits for kUHCI_PORTSCx.

enum {
    kUHCI_PORTSC_SUSPEND    = 0x1000,   // Suspend control
    kUHCI_PORTSC_OCI        = 0x0800,   // Overfurrent indicator
    kUHCI_PORTSC_OCA        = 0x0400,   // Overcurrent active
    kUHCI_PORTSC_RESET      = 0x0200,   // Reset control
    kUHCI_PORTSC_LS         = 0x0100,   // Low-speed device connected
    kUHCI_PORTSC_RD         = 0x0040,   // Resume detected
    kUHCI_PORTSC_LINE       = 0x0030,
    kUHCI_PORTSC_LINE0      = 0x0010,
    kUHCI_PORTSC_LINE1      = 0x0020,
    kUHCI_PORTSC_PEDC       = 0x0008,   // Port enable change
    kUHCI_PORTSC_PED        = 0x0004,   // Port enable
    kUHCI_PORTSC_CSC        = 0x0002,   // Connect status change
    kUHCI_PORTSC_CCS        = 0x0001,   // Connect status
    kUHCI_PORTSC_MASK       = (0xFFF5)
};


/* PCI configuration registers. */

enum {
    kUHCI_PCI_CLASSC    = 0x09,
    kUHCI_PCI_USBBASE   = 0x20,
    kUHCI_PCI_SBRN      = 0x60,
    kUHCI_PCI_LEGKEY    = 0xC0,     // 16 bits
    kUHCI_PCI_RES       = 0xC4,      // 8 bits
    
    kUHCI_LEGKEY_INTR_ENABLE = 0x2000
};


/* Frame list pointer. */

struct UHCIFrameListPointer {
    volatile UInt32  pointer;
};

enum {
    kUHCI_FRAME_FLP =   0xFFFFFFF0,
    kUHCI_FRAME_Q   =   0x00000002,
    kUHCI_FRAME_T   =   0x00000001
};

// Transfer descriptor
typedef struct UHCITransferDescriptorShared
UHCITransferDescriptorShared,
*UHCITransferDescriptorSharedPtr;

struct UHCITransferDescriptorShared {
    volatile UInt32  link;
    volatile UInt32  ctrlStatus;
    volatile UInt32  token;
    volatile UInt32  buffer;
};

enum {
    kUHCI_TD_SPD    =   0x20000000,
    kUHCI_TD_LS     =   0x04000000,
    kUHCI_TD_ISO    =   0x02000000,
    kUHCI_TD_IOC    =   0x01000000,
    kUHCI_TD_ACTIVE =   0x00800000,
    kUHCI_TD_STALLED=   0x00400000,
    kUHCI_TD_DBUF   =   0x00200000,
    kUHCI_TD_BABBLE =   0x00100000,
    kUHCI_TD_NAK    =   0x00080000,
    kUHCI_TD_CRCTO  =   0x00040000,
    kUHCI_TD_BITSTUFF=  0x00020000,
    kUHCI_TD_PID    =   0x000000FF,
    kUHCI_TD_ACTLEN =   0x000007FF,

    kUHCI_TD_D      =   0x00080000,
    kUHCI_TD_PID_IN =   0x00000069,
    kUHCI_TD_PID_OUT=   0x000000E1,
    kUHCI_TD_PID_SETUP= 0x0000002D,
    kUHCI_TD_MAXLEN_MASK=0xFFE00000,
    
    kUHCI_TD_ERROR_MASK = (kUHCI_TD_STALLED | kUHCI_TD_DBUF |
                           kUHCI_TD_BABBLE | kUHCI_TD_CRCTO |
                           kUHCI_TD_BITSTUFF)
};

#define UHCI_TD_GET_ERRCNT(n)   (((n) >> 27) & 3)
#define UHCI_TD_SET_ERRCNT(n)   (((n) & 3) << 27)
#define UHCI_TD_GET_ACTLEN(n)   (((n) + 1) & 0x7FF)
#define UHCI_TD_SET_ACTLEN(n)   (((n) - 1) & 0x7FF)
#define UHCI_TD_GET_STATUS(n)   (((n) >> 16) & 0xFF)
#define UHCI_TD_GET_PID(n)      ((n) & 0xFF)
#define UHCI_TD_GET_ADDR(n)     (((n) >> 8) & 0x7F)
#define UHCI_TD_SET_ADDR(n)     (((n) & 0x7F) << 8)
#define UHCI_TD_GET_ENDPT(n)    (((n) >> 15) & 0xF)
#define UHCI_TD_SET_ENDPT(n)    (((n) & 0xF) << 15)
#define UHCI_TD_GET_MAXLEN(n)   ((((n) >> 21) + 1) & 0x7FF)
#define UHCI_TD_SET_MAXLEN(n)   ((((n) - 1) & 0x7FF) << 21)


// Queue head
typedef struct UHCIQueueHeadShared
UHCIQueueHeadShared,
*UHCIQueueHeadSharedPtr;

struct UHCIQueueHeadShared {
    volatile UInt32  hlink;
    volatile UInt32  elink;
	UInt32 			 pad[2];				// 4358445 - need to make sure these get 16 byte aligned
};

enum {
    kUHCI_QH_QLP    =   0xFFFFFFF0,
    kUHCI_QH_Q      =   0x00000002,
    kUHCI_QH_T      =   0x00000001,
    kUHCI_TD_VF     =   0x00000004,
    kUHCI_TD_Q      =   0x00000002,
    kUHCI_TD_T      =   0x00000001
};

/* Various constants. */

enum {
    kUHCI_FRAME_COUNT   = 1024,
    kUHCI_FRAME_ALIGN   = 4096,
    kUHCI_TD_ALIGN      = 16,
    kUHCI_QH_ALIGN      = 16,
    kUHCI_NUM_PORTS     = 2,
    kUHCI_MAX_TRANSFER  = 1024,

    /* Delay after removing a QH,
     * to ensure that the hardware is
     * done with it.
     */
    kUHCI_QH_REMOVE_DELAY = 5
};

enum
{
    kUHCIPageSize			= 4096,
};
#define kUHCIPageOffsetMask	( kUHCIPageSize - 1 )		// mask off just the offset bits (low 12)
#define kUHCIPageMask 		(~(kUHCIPageOffsetMask))	// mask off just the page number (high 20)
#define kUHCIPtrMask		( 0xFFFFFFF0 )				// mask for list element pointers

#define	kUHCIStructureAllocationPhysicalMask	0x00000000FFFFF000ULL			// for use with inTaskWithPhysicalMask (below 4GB and 4K aligned)

#endif /*  _IOKIT_UHCI_H_ */


