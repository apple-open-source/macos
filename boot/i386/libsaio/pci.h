/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1994 NeXT Computer, Inc.
 * All rights reserved.
 */

#ifndef __LIBSAIO_PCI_H
#define __LIBSAIO_PCI_H

typedef struct _pci_slot_info {
    unsigned long int pid, sid;
    unsigned dev;
    unsigned func;
    unsigned bus;
} _pci_slot_info_t;

extern _pci_slot_info_t *PCISlotInfo;

/* The IOPCIConfigSpace structure can be used to decode the 256 byte
 * configuration space presented by each PCI device.  This structure
 * is based on the PCI LOCAL BUS SPECIFICATION, rev 2.1, section 6.1
 */

typedef struct _IOPCIConfigSpace {
	unsigned short	VendorID;
	unsigned short	DeviceID;
	unsigned short	Command;
	unsigned short	Status;
	unsigned long	RevisionID:8;
	unsigned long	ClassCode:24;
	unsigned char	CacheLineSize;
	unsigned char	LatencyTimer;
	unsigned char	HeaderType;
	unsigned char	BuiltInSelfTest;
	unsigned long	BaseAddress[6];
	unsigned long	CardbusCISpointer;
	unsigned short	SubVendorID;
	unsigned short	SubDeviceID;
	unsigned long	ROMBaseAddress;
	unsigned long	reserved3;
	unsigned long	reserved4;
	unsigned char	InterruptLine;
	unsigned char	InterruptPin;
	unsigned char	MinGrant;
	unsigned char	MaxLatency;
	unsigned long	VendorUnique[48];
} IOPCIConfigSpace;


/* PCI_DEFAULT_DATA is the value resulting from a read to a non-existent
 * PCI device's configuration space.
 */

#define PCI_DEFAULT_DATA	0xffffffff


/* PCI_INVALID_VENDOR_ID is a Vendor ID reserved by the PCI/SIG and is
 * guaranteed not to be assigned to any vendor.
 */

#define	PCI_INVALID_VENDOR_ID	0xffff

#endif /* !__LIBSAIO_PCI_H */
