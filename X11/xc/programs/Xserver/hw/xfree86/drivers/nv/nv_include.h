/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_include.h,v 1.11 2001/12/07 00:09:56 mvojkovi Exp $ */

#ifndef __NV_INCLUDE_H__
#define __NV_INCLUDE_H__

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

#include "micmap.h"

#include "xf86DDC.h"

#include "vbe.h"

#include "xf86RAC.h"

#include "nv_const.h"

#include "dixstruct.h"
#include "scrnintstr.h"

#include "fb.h"

#include "xaa.h"
#include "xf86cmap.h"
#include "shadowfb.h"
#include "fbdevhw.h"

#include "xf86xv.h"
#include "Xv.h"

#include "vgaHW.h"

#include "xf86Cursor.h"
#include "xf86DDC.h"

#include "region.h"

#include "nv_local.h"
#include "nv_type.h"
#include "nv_proto.h"

#endif /* __NV_INCLUDE_H__ */
