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

#ifndef _APPLEONBOARDPCATASHARED_H
#define _APPLEONBOARDPCATASHARED_H

#define kMaxDriveCount          2
#define kMaxChannelCount        2

#define kPrimaryCommandPort     0x1f0
#define kPrimaryControlPort     0x3f4
#define kSecondaryCommandPort   0x170
#define kSecondaryControlPort   0x374

#define kPrimaryIRQ             14
#define kSecondaryIRQ           15

#define kPrimaryChannelID       0
#define kSecondaryChannelID     1

/*
 * Bus master registers are located in I/O space.
 * Register size (bits) indicated in parenthesis.
 */
#define BM_COMMAND              0x00     // (8)  Command register
#define BM_STATUS               0x02     // (8)  Status register
#define BM_PRD_PTR              0x04     // (32) Descriptor table register
#define BM_SEC_OFFSET           8        // offset to channel 1 registers
#define BM_STATUS_INT           0x04     // IDE device asserted its interrupt
#define BM_ADDR_MASK            0xfff0   // BMIBA I/O base address mask

/*
 * PCI Programming Interface register
 */
#define PCI_ATA_PRI_NATIVE_ENABLED     0x01
#define PCI_ATA_PRI_NATIVE_SUPPORTED   0x02
#define PCI_ATA_SEC_NATIVE_ENABLED     0x04
#define PCI_ATA_SEC_NATIVE_SUPPORTED   0x08
#define PCI_ATA_BUS_MASTER_SUPPORTED   0x80

#define PCI_ATA_PRI_NATIVE_MASK \
       (PCI_ATA_PRI_NATIVE_ENABLED | PCI_ATA_PRI_NATIVE_SUPPORTED)

#define PCI_ATA_SEC_NATIVE_MASK \
       (PCI_ATA_SEC_NATIVE_ENABLED | PCI_ATA_SEC_NATIVE_SUPPORTED)

/*
 * ATA Root property keys
 */
#define kATAChannelCount               "ATA Channel Count"
#define kRootHardwareVendorNameKey     "Hardware Vendor"
#define kRootHardwareDeviceNameKey     "Hardware Device"

/*
 * ATA Channel property keys
 */
#define kChannelNumberKey              "Channel Number"
#define kCommandBlockAddressKey        "Command Block Address"
#define kControlBlockAddressKey        "Control Block Address"
#define kInterruptVectorKey            "Interrupt Vector"

/*
 * ATA Driver property keys
 */
#define kSelectedPIOModeKey            "PIO Mode"
#define kSelectedDMAModeKey            "DMA Mode"
#define kSelectedUltraDMAModeKey       "Ultra DMA Mode"

#ifdef  DEBUG
#define DEBUG_LOG(fmt, args...) kprintf(fmt, ## args)
#define ERROR_LOG(fmt, args...) kprintf(fmt, ## args)
#else
#define DEBUG_LOG(fmt, args...)
#define ERROR_LOG(fmt, args...) IOLog(fmt, ## args)
#endif

#define RELEASE(x) do { if(x) { (x)->release(); (x) = 0; } } while(0)

#endif /* !_APPLEONBOARDPCATASHARED_H */
