/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/vm_device_version.h,v 1.1 2001/04/05 19:29:44 dawes Exp $ */
/* **********************************************************
 * Copyright (C) 1998-2001 VMware, Inc.
 * All Rights Reserved
 * Id: vm_device_version.h,v 1.2 2001/01/26 21:53:27 yoel Exp $
 * **********************************************************/


#ifndef VM_DEVICE_VERSION_H
#define VM_DEVICE_VERSION_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#define PCI_VENDOR_ID_VMWARE        0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2  0x0405
#define PCI_DEVICE_ID_VMWARE_SVGA   0x0710
#define PCI_DEVICE_ID_VMWARE_NET    0x0720
#define PCI_DEVICE_ID_VMWARE_SCSI   0x0730
#define PCI_DEVICE_ID_VMWARE_IDE    0x1729

/*  From linux/pci.h.  We emulate an AMD ethernet controller */
#define PCI_VENDOR_ID_AMD               0x1022
#define PCI_DEVICE_ID_AMD_VLANCE        0x2000
#define PCI_VENDOR_ID_BUSLOGIC			0x104B
#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC	0x0140
#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER	0x1040

/* Intel Values for the chipset */
#define PCI_VENDOR_ID_INTEL             0x8086
#define PCI_DEVICE_ID_INTEL_82439TX     0x7100
#define PCI_DEVICE_ID_INTEL_82371AB_0   0x7110
#define PCI_DEVICE_ID_INTEL_82371AB_3   0x7113
#define PCI_DEVICE_ID_INTEL_82371AB     0x7111
#define PCI_DEVICE_ID_INTEL_82443BX     0x7192


/************* Strings for IDE Identity Fields **************************/
#define VIDE_ID_SERIAL_STR	"00000000000000000001"	/* Must be 20 Bytes */
#define VIDE_ID_FIRMWARE_STR	"00000001"		/* Must be 8 Bytes */

/* No longer than 40 Bytes and must be an even length. */
#define VIDE_ATA_MODEL_STR PRODUCT_GENERIC_NAME " Virtual IDE Hard Drive  "
#define VIDE_ATAPI_MODEL_STR PRODUCT_GENERIC_NAME " Virtual IDE CDROM Drive "

#define ATAPI_VENDOR_ID	"NECVMWar"		/* Must be 8 Bytes */
#define ATAPI_PRODUCT_ID PRODUCT_GENERIC_NAME " IDE CDROM"	/* Must be 16 Bytes */
#define ATAPI_REV_LEVEL	"1.00"			/* Must be 4 Bytes */


/************* Strings for SCSI Identity Fields **************************/
#define SCSI_DISK_MODEL_STR PRODUCT_GENERIC_NAME " Virtual SCSI Hard Drive"
#define SCSI_CDROM_MODEL_STR PRODUCT_GENERIC_NAME " Virtual SCSI CDROM Drive"

/************* Strings for the VESA BIOS Identity Fields *****************/
#define VBE_OEM_STRING COMPANY_NAME " SVGA"
#define VBE_VENDOR_NAME COMPANY_NAME
#define VBE_PRODUCT_NAME PRODUCT_GENERIC_NAME


#endif /* VM_DEVICE_VERSION_H */
