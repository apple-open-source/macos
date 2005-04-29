/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_driver.c,v 1.5 2003/04/23 21:51:41 tsi Exp $ */
/*
 * $Workfile: nsc_driver.c $
 * $Revision: 1.1.1.2 $
 * $Author: jharper $
 *
 * File Contents: This is the main module configures the interfacing 
 *                with the X server. The individual modules will be 
 *                loaded based upon the options selected from the 
 *                XF86Config. This file also has modules for finding 
 *                supported modes, turning on the modes based on options.
 *
 * Project:       Nsc Xfree Frame buffer device driver.
 *
 */

/* 
 * NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * National Xfree frame buffer driver
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * National Xfree frame buffer driver
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * National Xfree frame buffer driver
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */

#define DEBUG(x)
#define NSC_TRACE 0
#define CFB 0
#define HWVGA 1

/* Includes that are used by all drivers */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Resources.h"

/* We may want inb() and outb() */
#include "compiler.h"

/* We may want to access the PCI config space */
#include "xf86PciInfo.h"
#include "xf86Pci.h"

/* Colormap handling stuff */
#include "xf86cmap.h"

#define RC_MAX_DEPTH 24

/* Frame buffer stuff */
#if CFB
/*
 * If using cfb, cfb.h is required.  Select the others for the bpp values
 * the driver supports.
 */
#define PSZ 8				/* needed for cfb.h */
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb24.h"
#include "cfb32.h"
#else
#include "fb.h"
#endif

#include "shadowfb.h"

/* Machine independent stuff */
#include "mipointer.h"
#include "mibank.h"
#include "micmap.h"
/* All drivers implementing backing store need this */
#include "mibstore.h"
#include "vgaHW.h"
#include "vbe.h"

/* Check for some extensions */
#ifdef XFreeXDGA
#define _XF86_DGA_SERVER_
#include "extensions/xf86dgastr.h"
#endif /* XFreeXDGA */

#include "globals.h"
#include "opaque.h"
#define DPMS_SERVER
#include "extensions/dpms.h"

#define EXTERN
/* Our private include file (this also includes the durango headers) */
#include "nsc.h"

#if NSC_TRACE
/* ANSI C does not allow var arg macros */
#define GeodeDebug(args) DebugPort(DCount++);ErrorF args
#else
#define GeodeDebug(args)
#endif

/* A few things all drivers should have */
#define NSC_NAME            "NSC"
#define NSC_DRIVER_NAME     "nsc"

/* This should match the durango code version.
 * The patchlevel may be used to indicate changes in geode.c 
 */
#define NSC_VERSION_NAME    "2.7.6"
#define NSC_VERSION_MAJOR   2
#define NSC_VERSION_MINOR   7
#define NSC_PATCHLEVEL      6

#define NSC_VERSION_CURRENT ((NSC_VERSION_MAJOR << 24) | \
		(NSC_VERSION_MINOR << 16) | NSC_PATCHLEVEL)

/* Forward definitions */
static const OptionInfoRec *NscAvailableOptions(int chipid, int busid);
static void NscIdentify(int);
static Bool NscProbe(DriverPtr, int);
static int CPUDetected;

extern void GX1SetupChipsetFPtr(ScrnInfoPtr pScrn);
extern void GX2SetupChipsetFPtr(ScrnInfoPtr pScrn);

#if !defined(STB_X)
extern unsigned char *XpressROMPtr;
#endif /* STB_X */

/* driver record contains the functions needed by the server after loading
 * the driver module.
 */
DriverRec NSC = {
   NSC_VERSION_CURRENT,
   NSC_DRIVER_NAME,
   NscIdentify,
   NscProbe,
   NscAvailableOptions,
   NULL,
   0
};

/* Existing Processor Models */
#define GX1 0x1
#define GX2 0x2
#define GX2_CRT 0x6
#define GX2_TFT 0xA

#define PCI_VENDOR_ID_CYRIX  0x1078
#define PCI_VENDOR_ID_NS     0x100B

#define PCI_CHIP_5530     0x0104
#define PCI_CHIP_SC1200   0x0504
#define PCI_CHIP_SC1400   0x0104
#define PCI_CHIP_REDCLOUD 0x0030

/* National Chip Models */
typedef struct _DEVICE_MODEL
{
   int DeviceId;
   int Model;
}
DeviceModel;

DeviceModel ChipModel[] = {
   {PCI_CHIP_5530, GX1},
   {PCI_CHIP_SC1200, GX1},
   {PCI_CHIP_SC1400, GX1},
   {PCI_CHIP_REDCLOUD, GX2},
   {-1, 0}
};

/* Supported chipsets */
SymTabRec GeodeChipsets[] = {
   {PCI_CHIP_5530, "5530"},
   {PCI_CHIP_SC1200, "SC1200"},
   {PCI_CHIP_SC1400, "SC1400"},
   {PCI_CHIP_REDCLOUD, "REDCLOUD"},
   {-1, NULL}
};

PciChipsets GeodePCIchipsets[] = {
   {PCI_CHIP_5530, PCI_CHIP_5530, RES_SHARED_VGA},
   {PCI_CHIP_SC1200, PCI_CHIP_SC1200, RES_SHARED_VGA},
   {PCI_CHIP_SC1400, PCI_CHIP_SC1400, RES_SHARED_VGA},
   {PCI_CHIP_REDCLOUD, PCI_CHIP_REDCLOUD, RES_SHARED_VGA},
   {-1, -1, RES_UNDEFINED},
};

OptionInfoRec GeodeOptions[] = {
   {OPTION_SW_CURSOR, "SWcursor", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_HW_CURSOR, "HWcursor", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_NOCOMPRESSION, "NoCompression", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_TV_SUPPORT, "TV", OPTV_ANYSTR, {0}, FALSE},
   {OPTION_TV_OUTPUT, "TV_Output", OPTV_ANYSTR, {0}, FALSE},
   {OPTION_TV_OVERSCAN, "TVOverscan", OPTV_ANYSTR, {0}, FALSE},
   {OPTION_SHADOW_FB, "ShadowFB", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_ROTATE, "Rotate", OPTV_ANYSTR, {0}, FALSE},
   {OPTION_FLATPANEL, "FlatPanel", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_COLOR_KEY, "ColorKey", OPTV_INTEGER, {0}, FALSE},
   {OPTION_OSM_IMG_BUFS, "OSMImageBuffers", OPTV_INTEGER, {0}, FALSE},
   {-1, NULL, OPTV_NONE, {0}, FALSE}
};

/* List of symbols from other modules that this module references.The purpose
* is that to avoid unresolved symbol warnings
*/
const char *nscVgahwSymbols[] = {
   "vgaHWGetHWRec",
   "vgaHWUnlock",
   "vgaHWInit",
   "vgaHWSave",
   "vgaHWRestore",
   "vgaHWProtect",
   "vgaHWGetIOBase",
   "vgaHWMapMem",
   "vgaHWLock",
   "vgaHWFreeHWRec",
   "vgaHWSaveScreen",
   NULL
};

const char *nscVbeSymbols[] = {
   "VBEInit",
   "vbeDoEDID",
   "vbeFree",
   NULL
};

const char *nscInt10Symbols[] = {
   "xf86ExecX86int10",
   "xf86InitInt10",
   "xf86Int10AllocPages",
   "xf86Int10Addr",
   NULL
};

#if CFB
const char *nscCfbSymbols[] = {
   "cfbScreenInit",
   "cfb16ScreenInit",
   "cfb24ScreenInit",
   "cfb32ScreenInit",
   NULL
};
#else
const char *nscFbSymbols[] = {
   "fbScreenInit",
   "fbPictureInit",
   NULL
};
#endif

const char *nscXaaSymbols[] = {
   "XAADestroyInfoRec",
   "XAACreateInfoRec",
   "XAAInit",
   NULL
};

const char *nscRamdacSymbols[] = {
   "xf86InitCursor",
   "xf86CreateCursorInfoRec",
   "xf86DestroyCursorInfoRec",
   NULL
};

const char *nscShadowSymbols[] = {
   "ShadowFBInit",
   NULL
};

#ifdef XFree86LOADER

/* Module loader interface */

static MODULESETUPPROTO(NscSetup);

static XF86ModuleVersionInfo NscVersionRec = {
   "nsc",
   MODULEVENDORSTRING,
   MODINFOSTRING1,
   MODINFOSTRING2,
   XF86_VERSION_CURRENT,
   NSC_VERSION_MAJOR, NSC_VERSION_MINOR, NSC_PATCHLEVEL,
   ABI_CLASS_VIDEODRV,			/* This is a video driver */
   ABI_VIDEODRV_VERSION,
   MOD_CLASS_VIDEODRV,
   {0, 0, 0, 0}
};

/*
 * This data is accessed by the loader.  The name must be the module name
 * followed by "ModuleInit".
 */
XF86ModuleData nscModuleData = { &NscVersionRec, NscSetup, NULL };

/*-------------------------------------------------------------------------
 * NscSetup.
 *
 * Description	:This function sets up the driver in X list and load the
 *               module symbols through xf86loader routines..
 *
 * Parameters.
 *    Module	:Pointer to the geode  module
 *    options	:Driver module options.
 *    ErrorMajor:Major no
 *    ErrorMinor:Minor no.
 *
 * Returns		:NULL on success
 *
 * Comments     :Module setup is done by this function
 *
 *-------------------------------------------------------------------------
*/
static pointer
NscSetup(pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
   static Bool Initialised = FALSE;

   if (!Initialised) {
      Initialised = TRUE;
      xf86AddDriver(&NSC, Module, 0);
      /* Tell the loader about symbols from other modules that this
       * module might refer to.
       */
      LoaderRefSymLists(nscVgahwSymbols, nscVbeSymbols,
#if CFB
			nscCfbSymbols,
#else
			nscFbSymbols,
#endif
			nscXaaSymbols,
			nscInt10Symbols, nscRamdacSymbols, nscShadowSymbols,
			NULL);
      return (pointer) TRUE;
   }
   /*The return value must be non-NULL on success */
   if (ErrorMajor)
      *ErrorMajor = LDR_ONCEONLY;
   return NULL;
}
#endif /*End of XFree86Loader */

/*-------------------------------------------------------------------------
 * NscIdentify.
 *
 * Description  :	This function identify an Nscfamily version.
 *
 *
 * Parameters.
 *    flags		:	flags may be used in PreInit*
 *
 * Returns		: 	none
 *
 * Comments     : 	none
 *
*------------------------------------------------------------------------
*/
static void
NscIdentify(int flags)
{
   xf86PrintChipsets(NSC_NAME,
		     "Nsc family driver (version " NSC_VERSION_NAME ") "
		     "for chipsets", GeodeChipsets);
}

/*----------------------------------------------------------------------------
 * NscAvailableOptions.
 *
 * Description	:This function returns the geodeoptions set geodeoption
 *
 * Parameters.
 *    chipid	:This will identify the chipset.
 *    busid     :This will identify the PCI busid
 *
 * Returns		:ptr to GeodeOptions.
 *
 * Comments     :none
 *
*----------------------------------------------------------------------------
*/
static const OptionInfoRec *
NscAvailableOptions(int chipid, int busid)
{
   return GeodeOptions;
}

/*----------------------------------------------------------------------------
 * NscProbe.
 *
 * Description	:This is to find that hardware is claimed by another
 *		 driver if not claim the slot & allocate ScreenInfoRec.
 *
 * Parameters.
 *     drv	:a pointer to the geode driver
 *     flags    :flags may passed to check the config and probe detect
 * 												
 * Returns	:TRUE on success and FALSE on failure.
 *
 * Comments     :This should ne minimal probe and it should under no
 *               circumstances change the state of the hardware.Don't do
 *               any intiallizations other than the required
 *               ScreenInforec.
*----------------------------------------------------------------------------
*/

static Bool
NscProbe(DriverPtr drv, int flags)
{
   Bool foundScreen = FALSE;
   int numDevSections, numUsed;
   GDevPtr *devSections = NULL;
   int *usedChips = NULL;
   int i;

   GeodeDebug(("NscProbe: Probing for supported devices!\n"));
   /*
    * * Find the config file Device sections that match this
    * * driver, and return if there are none.
    */
   if ((numDevSections = xf86MatchDevice(NSC_NAME, &devSections)) <= 0) {
      GeodeDebug(("NscProbe: failed 1!\n"));
      return FALSE;
   }
   GeodeDebug(("NscProbe: Before MatchPciInstances!\n"));
   /* PCI BUS */
   if (xf86GetPciVideoInfo()) {
      numUsed = xf86MatchPciInstances(NSC_NAME, PCI_VENDOR_ID_NS,
				      GeodeChipsets, GeodePCIchipsets,
				      devSections, numDevSections,
				      drv, &usedChips);
      if (numUsed <= 0) {
	 /* Check for old CYRIX vendor ID (5530) */
	 numUsed = xf86MatchPciInstances(NSC_NAME,
					 PCI_VENDOR_ID_CYRIX,
					 GeodeChipsets, GeodePCIchipsets,
					 devSections, numDevSections,
					 drv, &usedChips);
      }

      GeodeDebug(("NscProbe: MatchPCI (%d)!\n", numUsed));

      if (numUsed > 0) {
	 if (flags & PROBE_DETECT)
	    foundScreen = TRUE;
	 else {
	    /* Durango only supports one instance, */
	    /* so take the first one */
	    for (i = 0; i < numUsed; i++) {
	       /* Allocate a ScrnInfoRec  */
	       ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0);

	       EntityInfoPtr pEnt = xf86GetEntityInfo(usedChips[i]);
	       PciChipsets *p_id;

	       for (p_id = GeodePCIchipsets; p_id->numChipset != -1; p_id++) {
		  if (pEnt->chipset == p_id->numChipset) {
		     CPUDetected = GX1;
		     if (pEnt->chipset == PCI_CHIP_REDCLOUD)
			CPUDetected = GX2;
		     break;
		  }
	       }
	       xfree(pEnt);
	       GeodeDebug(("NscProbe: CPUDetected %d!\n", CPUDetected));

	       pScrn->driverName = NSC_DRIVER_NAME;
	       pScrn->name = NSC_NAME;
	       pScrn->Probe = NscProbe;

	       if (CPUDetected == GX1) {
		  GX1SetupChipsetFPtr(pScrn);
	       } else {			/* GX2 */
		  GX2SetupChipsetFPtr(pScrn);
	       }

	       foundScreen = TRUE;
	       xf86ConfigActivePciEntity(pScrn,
					 usedChips[i],
					 GeodePCIchipsets,
					 NULL, NULL, NULL, NULL, NULL);
	    }
	 }
      }
   }

   if (usedChips)
      xfree(usedChips);
   if (devSections)
      xfree(devSections);
   GeodeDebug(("NscProbe: result (%d)!\n", foundScreen));
   return foundScreen;
}
