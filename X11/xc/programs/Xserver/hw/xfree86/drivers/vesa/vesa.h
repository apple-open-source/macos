/*
 * Copyright (c) 2000 by Conectiva S.A. (http://www.conectiva.com)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * CONECTIVA LINUX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Conectiva Linux shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from
 * Conectiva Linux.
 *
 * Authors: Paulo César Pereira de Andrade <pcpa@conectiva.com.br>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vesa/vesa.h,v 1.14 2003/01/16 16:09:10 eich Exp $
 */

#ifndef _VESA_H_
#define _VESA_H_

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

#include "vgaHW.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* VBE/DDC support */
#include "vbe.h"
#include "vbeModes.h"
#include "xf86DDC.h"

/* ShadowFB support */
#include "shadow.h"

/* Int 10 support */
#include "xf86int10.h"

/* bank switching */
#include "mibank.h"

/* Dga definitions */
#include "dgaproc.h"

#include "xf86Resources.h"
#include "xf86RAC.h"

#include "xf1bpp.h"
#include "xf4bpp.h"
#include "fb.h"
#include "afb.h"
#include "mfb.h"
#include "cfb24_32.h"

#define VESA_VERSION		4000
#define VESA_NAME		"VESA"
#define VESA_DRIVER_NAME	"vesa"
#define VESA_MAJOR_VERSION	1
#define VESA_MINOR_VERSION	0
#define VESA_PATCHLEVEL		0

/*XXX*/

typedef struct _VESARec
{
    vbeInfoPtr pVbe;
    EntityInfoPtr pEnt;
    CARD16 major, minor;
    VbeInfoBlock *vbeInfo;
    GDevPtr device;
    pciVideoPtr pciInfo;
    PCITAG pciTag;
    miBankInfoRec bank;
    int curBank, bankSwitchWindowB;
    CARD16 maxBytesPerScanline;
    unsigned long mapPhys, mapOff, mapSize;	/* video memory */
    void *base, *VGAbase;
    CARD8 *state, *pstate;	/* SVGA state */
    int statePage, stateSize, stateMode;
    int page;
    CARD32 *pal, *savedPal;
    CARD8 *fonts;
    xf86MonPtr monitor;
    Bool shadowFB, primary;
    CARD8 *shadowPtr;
    CARD32 windowAoffset;
    /* Don't override the default refresh rate. */
    Bool defaultRefresh;
    /* DGA info */
    DGAModePtr pDGAMode;
    int nDGAMode;
    CloseScreenProcPtr CloseScreen;
    OptionInfoPtr Options;
    IOADDRESS ioBase;
} VESARec, *VESAPtr;


#endif /* _VESA_H_ */
