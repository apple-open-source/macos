/* Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved. 
 *
 * PCI Configuration space structure and associated defines.
 *
 * HISTORY
 *
 * 13 May 1994	Dean Reece at NeXT
 *	Created.
 *
 */

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
