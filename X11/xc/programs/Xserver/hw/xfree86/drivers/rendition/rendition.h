/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/rendition.h,v 1.7 2001/05/16 06:48:10 keithp Exp $ */

#ifndef __RENDITION_H__
#define __RENDITION_H__

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

/* All drivers need this */
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

/* This is used for module versioning */
#include "xf86Version.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* All drivers using the vgahw module need this  */
/* All V1000 _need_ VGA register access,         */
/* so multihead operation is out of the question */
#include "vgaHW.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

/* Drivers using the mi banking wrapper need this */
#include "mibank.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

/* Needed by Resources Access Control (RAC) */
#include "xf86RAC.h"

/* Several predefined resource lists */
#include "xf86Resources.h"

/* Needed by the Shadow Framebuffer */
#include "shadowfb.h"

/* Needed for replacement LoadPalette function for Gamma Correction */
#include "xf86cmap.h"

/* Drivers using fb need this */

#include "fb.h"

/* Drivers using the XAA interface ... */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86fbman.h"

/* HW-cursor definitions */
#include "xf86Cursor.h"

/* DDC support */
#include "xf86DDC.h"

#include "commonregs.h"

/* end of __RENDITION_H__ */
#endif
