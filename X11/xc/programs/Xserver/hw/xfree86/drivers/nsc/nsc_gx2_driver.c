/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx2_driver.c,v 1.6 2003/02/12 13:08:54 alanh Exp $ */
/*
 * $Workfile: nsc_gx2_driver.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * File Contents: This is the main module configures the interfacing 
 *                with the X server. The individual modules will be 
 *                loaded based upon the options selected from the 
 *                XF86Config. This file also has modules for finding 
 *                supported modes, turning on the modes based on options.
 *
 * Project:       Geode Xfree Frame buffer device driver.
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
#define GEODE_TRACE 0
#define CFB 0

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

#define INT10_SUPPORT 1

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

#ifdef DPMSExtension
#include "globals.h"
#include "opaque.h"
#define DPMS_SERVER
#include "extensions/dpms.h"
#endif /* DPMSExtension */

/* Our private include file (this also includes the durango headers) */
#include "nsc.h"
#if !defined(STB_X)
#include "nsc_gx2_vga.c"
#endif /* STB_X */

#if GEODE_TRACE
/* ANSI C does not allow var arg macros */
#define GeodeDebug(args) DebugPort(DCount++);ErrorF args
#else
#define GeodeDebug(args)
#endif

extern SymTabRec GeodeChipsets[];
extern PciChipsets GeodePCIchipsets[];
extern OptionInfoRec GeodeOptions[];

/* Forward definitions */
static Bool GX2PreInit(ScrnInfoPtr, int);
static Bool GX2ScreenInit(int, ScreenPtr, int, char **);
static Bool GX2EnterVT(int, int);
static void GX2LeaveVT(int, int);
static void GX2FreeScreen(int, int);
void GX2AdjustFrame(int, int, int, int);
Bool GX2SwitchMode(int, DisplayModePtr, int);
static int GX2ValidMode(int, DisplayModePtr, Bool, int);
static void GX2LoadPalette(ScrnInfoPtr pScreenInfo,
			   int numColors, int *indizes,
			   LOCO * colors, VisualPtr pVisual);
static Bool GX2MapMem(ScrnInfoPtr);
static Bool GX2UnmapMem(ScrnInfoPtr);
static void gx2_set_DvLineSize(unsigned int pitch);

extern Bool GX2AccelInit(ScreenPtr pScreen);
extern Bool GX2HWCursorInit(ScreenPtr pScreen);
extern void GX2HideCursor(ScrnInfoPtr pScreenInfo);
extern void GX2ShowCursor(ScrnInfoPtr pScreenInfo);
extern void GX2PointerMoved(int index, int x, int y);
extern void GX2RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX2RefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX2RefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX2RefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX2RefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX2InitVideo(ScreenPtr pScreen);
extern Bool GX2DGAInit(ScreenPtr pScreen);
extern void GX2LoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char *src);

#if !defined(STB_X)
extern unsigned char *XpressROMPtr;
#endif /* STB_X */

/* Existing Processor Models */
#define GX1 0x1
#define GX2 0x2
#define GX2_CRT 0x6
#define GX2_TFT 0xA

/* List of symbols from other modules that this module references.The purpose
* is that to avoid unresolved symbol warnings
*/
extern const char *nscVgahwSymbols[];
extern const char *nscVbeSymbols[];
extern const char *nscInt10Symbols[];

#if CFB
extern const char *nscCfbSymbols[];
#else
extern const char *nscFbSymbols[];
#endif
extern const char *nscXaaSymbols[];
extern const char *nscRamdacSymbols[];
extern const char *nscShadowSymbols[];

void GX2SetupChipsetFPtr(ScrnInfoPtr pScrn);
GeodePtr GX2GetRec(ScrnInfoPtr pScreenInfo);
void get_flatpanel_info(const char *options, int *W, int *H,
			int *D, int *C, int *T);
void gx2_clear_screen(int width, int height);
void EnableDACPower(void);
void redcloud_gfx_2_vga_fix(void);

void
GX2SetupChipsetFPtr(ScrnInfoPtr pScrn)
{
   GeodeDebug(("GX2SetupChipsetFPtr!\n"));

   pScrn->PreInit = GX2PreInit;
   pScrn->ScreenInit = GX2ScreenInit;
   pScrn->SwitchMode = GX2SwitchMode;
   pScrn->AdjustFrame = GX2AdjustFrame;
   pScrn->EnterVT = GX2EnterVT;
   pScrn->LeaveVT = GX2LeaveVT;
   pScrn->FreeScreen = GX2FreeScreen;
   pScrn->ValidMode = GX2ValidMode;
}

/*----------------------------------------------------------------------------
 * GX2GetRec.
 *
 * Description	:This function allocate an GeodeRec and hooked into
 * pScreenInfo 	 str driverPrivate member of ScreeenInfo
 * 				 structure.
 * Parameters.
 * pScreenInfo 	:Pointer handle to the screenonfo structure.
 *
 * Returns		:allocated pScreeninfo structure.
 *
 * Comments     :none
 *
*----------------------------------------------------------------------------
*/
GeodePtr
GX2GetRec(ScrnInfoPtr pScreenInfo)
{
   if (!pScreenInfo->driverPrivate) {
      GeodePtr pGeode;

      pGeode = pScreenInfo->driverPrivate = xnfcalloc(sizeof(GeodeRec), 1);
#if INT10_SUPPORT
      pGeode->vesa = xcalloc(sizeof(VESARec), 1);
#endif
   }
   return GEODEPTR(pScreenInfo);
}

/*----------------------------------------------------------------------------
 * GX2FreeRec.
 *
 * Description	:This function deallocate an GeodeRec and freed from
 *               pScreenInfo str driverPrivate member of ScreeenInfo
 *               structure.
 * Parameters.
 * pScreenInfo	:Pointer handle to the screenonfo structure.
 *
 * Returns		:none
 *
 * Comments     :none
 *
*----------------------------------------------------------------------------
*/
static void
GX2FreeRec(ScrnInfoPtr pScreenInfo)
{
   if (pScreenInfo->driverPrivate == NULL) {
      return;
   }
   xfree(pScreenInfo->driverPrivate);
   pScreenInfo->driverPrivate = NULL;
}

/*----------------------------------------------------------------------------
 * GX2SaveScreen.
 *
 * Description	:This is todo the screen blanking
 *
 * Parameters.
 *     pScreen	:Handle to ScreenPtr structure.
 *     mode		:mode is used by vgaHWSaveScren to check blnak os on.
 * 												
 * Returns		:TRUE on success and FALSE on failure.
 *
 * Comments     :none
*----------------------------------------------------------------------------
*/
static Bool
GX2SaveScreen(ScreenPtr pScreen, int mode)
{
#if !defined(STB_X)
   ScrnInfoPtr pScreenInfo = xf86Screens[pScreen->myNum];

   GeodeDebug(("GX2SaveScreen!\n"));

   if (!pScreenInfo->vtSema)
      return vgaHWSaveScreen(pScreen, mode);

#endif /* STB_X */
   return TRUE;
}

/*----------------------------------------------------------------------------
 * get_flatpanel_info.
 *
 * Description	:This gets the values of the flatpanel attached.
 *
 * Parameters:
 *     options	: Pointer to the display options.
 *             W: Pointer to the width of the panel
 *             H: Pointer to the height of the panel
 * 	           D: Pointer to the depth of the panel.
 *             C: Pointer to the color of the panel. 
 *             T: Pointer to the type of the panel. 
 * Returns	    : none.
 *
 * Comments     :none
 *------------------------------------------------------------------------
 */
void
get_flatpanel_info(const char *options, int *W, int *H,
		   int *D, int *C, int *T)
{
   char *pnl_opt;

   pnl_opt = strtok((char *)options, ":");
   *W = strtoul(pnl_opt, NULL, 0);
   pnl_opt = strtok(NULL, ":");
   *H = strtoul(pnl_opt, NULL, 0);
   pnl_opt = strtok(NULL, ":");
   *D = strtoul(pnl_opt, NULL, 0);
   pnl_opt = strtok(NULL, ":");
   *C = strtoul(pnl_opt, NULL, 0);
   pnl_opt = strtok(NULL, ":");
   *T = strtoul(pnl_opt, NULL, 0);

   *C = (*C) ? PNL_COLOR_PANEL : PNL_MONO_PANEL;

   switch (*T) {
   case 0:
      *T = PNL_SSTN;
      break;
   case 1:
      *T = PNL_DSTN;
      break;
   case 2:
   default:
      *T = PNL_TFT;
      break;
   }

   if ((*W != 640) && (*W != 800) && (*W != 1024))
      *W = 640;

   if ((*H != 480) && (*H != 600) && (*H != 768))
      *H = 480;
}

static void
GX2ProbeDDC(ScrnInfoPtr pScrn, int index)
{
   vbeInfoPtr pVbe;

   if (xf86LoadSubModule(pScrn, "vbe")) {
      pVbe = VBEInit(NULL, index);
      ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
      vbeFree(pVbe);
   }
}

/*----------------------------------------------------------------------------
 * GX2PreInit.
 *
 * Description	:This function is called only once ate teh server startup
 *
 * Parameters.
 *  pScreenInfo :Handle to ScreenPtr structure.
 *  flags       :flags may be used to check the probeed one with config.
 * 												
 * Returns		:TRUE on success and FALSE on failure.
 *
 * Comments     :none.
 *----------------------------------------------------------------------------
 */
static Bool
GX2PreInit(ScrnInfoPtr pScreenInfo, int flags)
{
   static ClockRange GeodeClockRange =
	 { NULL, 25175, 229500, 0, FALSE, TRUE, 1, 1, 0 };
   MessageType from;
   int i = 0;
   GeodePtr pGeode;
   char *mod = NULL;

#if CFB
   char *reqSymbol = NULL;
#endif /* CFB */
#if defined(STB_X)
   GAL_ADAPTERINFO sAdapterInfo;
#endif /* STB_X */
   unsigned int PitchInc = 0, minPitch = 0, maxPitch = 0;
   unsigned int minHeight = 0, maxHeight = 0;
   unsigned int SupportFlags;
   const char *s;
   char **modes;

#if INT10_SUPPORT
   VESAPtr pVesa;
#endif

   DCount = 10;
   GeodeDebug(("GX2PreInit!\n"));
   /* Allocate driver private structure */
   if (!(pGeode = GX2GetRec(pScreenInfo)))
      return FALSE;

   /* This is the general case */
   for (i = 0; i < pScreenInfo->numEntities; i++) {
      pGeode->pEnt = xf86GetEntityInfo(pScreenInfo->entityList[i]);
      if (pGeode->pEnt->resources)
	 return FALSE;
      pGeode->Chipset = pGeode->pEnt->chipset;
      pScreenInfo->chipset = (char *)xf86TokenToString(GeodeChipsets,
						       pGeode->pEnt->chipset);
   }

   if (flags & PROBE_DETECT) {
      GX2ProbeDDC(pScreenInfo, pGeode->pEnt->index);
      return TRUE;
   }
#if INT10_SUPPORT
   if (!xf86LoadSubModule(pScreenInfo, "int10"))
      return FALSE;
   xf86LoaderReqSymLists(nscInt10Symbols, NULL);
#endif
   pGeode->FBVGAActive = 0;		/* KFB will Knock of VGA */

#if !defined(STB_X)
   /* If the vgahw module would be needed it would be loaded here */
   if (!xf86LoadSubModule(pScreenInfo, "vgahw")) {
      return FALSE;
   }
   xf86LoaderReqSymLists(nscVgahwSymbols, NULL);
#endif /* STB_X */
   GeodeDebug(("GX2PreInit(1)!\n"));
   /* Do the durango hardware detection */
#if defined(STB_X)
   if (!Gal_initialize_interface()) {
      GeodeDebug(("GALintialize fail GX2PreInit(1.00)!\n"));
      return FALSE;
   }

   if (Gal_get_adapter_info(&sAdapterInfo)) {
      pGeode->cpu_version = sAdapterInfo.dwCPUVersion;

      /* find the base chipset core. Currently there can be only one 
       * chip active at any time.
       */
      if ((pGeode->cpu_version & 0xFF) == GFX_CPU_REDCLOUD) {
	 if (sAdapterInfo.dwCPUType)
	    pGeode->DetectedChipSet = GX2_TFT;
	 else
	    pGeode->DetectedChipSet = GX2_CRT;
      }

      DEBUGMSG(1,
	       (0, X_NONE, "Detected BaseChip %d, %d\n",
		pGeode->DetectedChipSet, sAdapterInfo.dwCPUType));

      pGeode->vid_version = sAdapterInfo.dwVideoVersion;
      pGeode->FBSize = sAdapterInfo.dwFrameBufferSize;
      /* update the max clock from the one system suports  */
      GeodeClockRange.maxClock = sAdapterInfo.dwMaxSupportedPixelClock;
      pGeode->FBLinearAddr = sAdapterInfo.dwFrameBufferBase;
#if 0
      pGeode->FBBase = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						      VIDMEM_FRAMEBUFFER,
						      pGeode->FBLinearAddr,
						      pGeode->FBSize);
#endif
      if (!GX2MapMem(pScreenInfo))
	 return FALSE;
      DEBUGMSG(1, (0, X_NONE, "CPU=%x vid %x FB %x FBAdd %X\n",
		   pGeode->cpu_version, pGeode->vid_version, pGeode->FBSize,
		   pGeode->FBLinearAddr));
   } else {
      return FALSE;
   }
#else /* STB */
   pGeode->cpu_version = gfx_detect_cpu();

   /* find the base chipset core. Currently there can be only one 
    * chip active at any time.
    */
/*	pGeode->DetectedChipSet = GX1; */
   if ((pGeode->cpu_version & 0xFF) == GFX_CPU_REDCLOUD)
      pGeode->DetectedChipSet = GX2;
   GeodeDebug(("Detected BaseChip (%d)\n", pGeode->DetectedChipSet));
   {
      Q_WORD msrValue;

      /* GX2 : Can have CRT or TFT only */
      gfx_msr_read(RC_ID_DF, MBD_MSR_CONFIG, &msrValue);
      pGeode->DetectedChipSet =
	    ((msrValue.low & RCDF_CONFIG_FMT_MASK) ==
	     RCDF_CONFIG_FMT_FP) ? GX2_TFT : GX2_CRT;
      GeodeDebug(("Gx2 for %s\n",
		  ((pGeode->DetectedChipSet == GX2_TFT) ? "TFT" : "CRT")));
   }
   GeodeDebug(("GX2PreInit(1.1)!\n"));
   pGeode->vid_version = gfx_detect_video();
   GeodeDebug(("GX2PreInit(1.2)!\n"));
   pGeode->FBLinearAddr = gfx_get_frame_buffer_base();
   GeodeDebug(("GX2PreInit(1.3)!\n"));
   pGeode->FBSize = gfx_get_frame_buffer_size();
   GeodeDebug(("GX2PreInit(1.4)!\n"));
   /* update the max clock from the one system suports  */
   GeodeClockRange.maxClock = gfx_get_max_supported_pixel_clock();

   GeodeDebug(("GX2PreInit(1.5)!\n"));
   /* SET DURANGO REGISTER POINTERS
    * * The method of mapping from a physical address to a linear address
    * * is operating system independent.  Set variables to linear address.
    */
   if (pGeode->DetectedChipSet & GX2) {
      pGeode->cpu_reg_size = 0x4000;
      pGeode->gp_reg_size = 0x4000;
      pGeode->vid_reg_size = 0x4000;
   } else {
      pGeode->cpu_reg_size = 0x9000;
      pGeode->vid_reg_size = 0x1000;
   }

   if (!GX2MapMem(pScreenInfo))
      return FALSE;

   /* check if VGA is active */
   /* This routine saves the current VGA state in Durango VGA structure */
   /* check if VGA is active */
   pGeode->FBVGAActive = gu2_get_vga_active();

#endif /* STB_X */
   DEBUGMSG(1, (0, X_PROBED, "VGA = %d\n", pGeode->FBVGAActive));

   /* Fill in the monitor field */
   pScreenInfo->monitor = pScreenInfo->confScreen->monitor;
   GeodeDebug(("GX2PreInit(2)!\n"));
   SupportFlags = Support24bppFb | Support32bppFb;
   GeodeDebug(("GX2PreInit(2)!\n"));
   /* Determine depth, bpp, etc. */
   if (!xf86SetDepthBpp(pScreenInfo, 8, 8, 8, SupportFlags)) {
      return FALSE;
   } else {
      if (!((pScreenInfo->depth == 8) ||
	    (pScreenInfo->depth == 16) ||
	    (pScreenInfo->depth == 24) || (pScreenInfo->depth == 32))) {
	 /* Depth not supported */
	 DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		      "Given depth (%d bpp) is not supported by this driver\n",
		      pScreenInfo->depth));
	 return FALSE;
      }
   }

   /*This must happen after pScreenInfo->display has been set
    * * because xf86SetWeight references it.
    */
   if (pScreenInfo->depth > 8) {
      /* The defaults are OK for us */
      rgb BitsPerComponent = { 0, 0, 0 };
      rgb BitMask = { 0, 0, 0 };

      if (pScreenInfo->depth > 16) {
	 /* we are operating in 24 bpp, Readcloud */
	 BitsPerComponent.red = 8;
	 BitsPerComponent.green = 8;
	 BitsPerComponent.blue = 8;

	 BitMask.red = 0xFF0000;
	 BitMask.green = 0x00FF00;
	 BitMask.blue = 0x0000FF;
      }
      if (!xf86SetWeight(pScreenInfo, BitsPerComponent, BitMask)) {
	 return FALSE;
      } else {
	 /* XXX Check if the returned weight is supported */
      }
   }

   xf86PrintDepthBpp(pScreenInfo);

   GeodeDebug(("GX2PreInit(3)!\n"));

   if (!xf86SetDefaultVisual(pScreenInfo, -1))
      return FALSE;

   GeodeDebug(("GX2PreInit(4)!\n"));

   /* The new cmap layer needs this to be initialized */
   if (pScreenInfo->depth > 1) {
      Gamma zeros = { 0.0, 0.0, 0.0 };

      if (!xf86SetGamma(pScreenInfo, zeros)) {
	 return FALSE;
      }
   }
   GeodeDebug(("GX2PreInit(5)!\n"));

   /* We use a programmable clock */
   pScreenInfo->progClock = TRUE;

   /*Collect all of the relevant option flags
    * *(fill in pScreenInfo->options)
    */
   xf86CollectOptions(pScreenInfo, NULL);

   /*Process the options */
   xf86ProcessOptions(pScreenInfo->scrnIndex, pScreenInfo->options,
		      GeodeOptions);

#if INT10_SUPPORT
   pVesa = pGeode->vesa;
   /* Initialize Vesa record */

   if ((pVesa->pInt = xf86InitInt10(pGeode->pEnt->index)) == NULL) {
      xf86DrvMsg(0, X_ERROR, "Int10 initialization failed.\n");
      return (FALSE);
   }
#endif
   /*Set the bits per RGB for 8bpp mode */
   if (pScreenInfo->depth == 8) {
      /* Default to 8 */
      pScreenInfo->rgbBits = 8;
   }
   from = X_DEFAULT;

   /*
    * *The preferred method is to use the "hw cursor" option as a tri-state
    * *option, with the default set above.
    */
   pGeode->HWCursor = TRUE;
   if (xf86GetOptValBool(GeodeOptions, OPTION_HW_CURSOR, &pGeode->HWCursor)) {
      from = X_CONFIG;
   }
   /* For compatibility, accept this too (as an override) */
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_SW_CURSOR, FALSE)) {
      from = X_CONFIG;
      pGeode->HWCursor = FALSE;
   }
   DEBUGMSG(1, (pScreenInfo->scrnIndex, from, "Using %s cursor\n",
		pGeode->HWCursor ? "HW" : "SW"));

   pGeode->Compression = TRUE;
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_NOCOMPRESSION, FALSE)) {
      pGeode->Compression = FALSE;
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "NoCompression\n"));
   }

   pGeode->NoAccel = FALSE;
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_NOACCEL, FALSE)) {
      pGeode->NoAccel = TRUE;
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "Acceleration \
			disabled\n"));
   }

   if (!xf86GetOptValInteger(GeodeOptions, OPTION_OSM_IMG_BUFS,
			     &(pGeode->NoOfImgBuffers)))
      pGeode->NoOfImgBuffers = DEFAULT_NUM_OF_BUF;	/* default # of buffers */
   if (pGeode->NoOfImgBuffers <= 0)
      pGeode->NoOfImgBuffers = 0;
   DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		"NoOfImgBuffers = %d\n", pGeode->NoOfImgBuffers));

   pGeode->Panel = FALSE;
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_FLATPANEL, FALSE)) {
      DEBUGMSG(0, (pScreenInfo->scrnIndex, X_CONFIG, "FlatPanel Selected\n"));
      pGeode->Panel = TRUE;
   }

   /* Force the Panel on if on a GX2 TFT part, no crt support */
   if (pGeode->DetectedChipSet == GX2_TFT) {
      pGeode->Panel = TRUE;
   }

   /* If on a CRT and Panel flag set, disable Panel */
   if ((pGeode->DetectedChipSet == GX2_CRT) && (pGeode->Panel))
      pGeode->Panel = FALSE;

   DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		"Quering FP Bios %d\n", pGeode->Panel));

   /* if FP not supported in BIOS, then turn off user option */
   if (pGeode->Panel) {
      /* check if bios supports FP */
#if defined(STB_X)
      Gal_pnl_enabled_in_bios(&pGeode->Panel);
      Gal_pnl_info_from_bios(&pGeode->FPBX, &pGeode->FPBY,
			     &pGeode->FPBB, &pGeode->FPBF);
#else /* STB_X */
      pGeode->Panel = Pnl_IsPanelEnabledInBIOS();
      Pnl_GetPanelInfoFromBIOS(&pGeode->FPBX, &pGeode->FPBY,
			       &pGeode->FPBB, &pGeode->FPBF);
#endif /* STB_X */
   }

   DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		"Quering FP Bios %d %d %d %d\n",
		pGeode->FPBX, pGeode->FPBY, pGeode->FPBB, pGeode->FPBF));

   /* if panel not selected and Panel can be  supported. 
    * Power down the panel. 
    */
   if (!pGeode->Panel) {
#if defined(STB_X)
      Gal_pnl_powerdown();
#else /* STB_X */
      Pnl_PowerDown();
#endif /* STB_X */
   } else {
#if defined(STB_X)
      Gal_pnl_powerup();
#else
      Pnl_PowerUp();
#endif /* STB_X */
   }

   pGeode->ShadowFB = FALSE;
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_SHADOW_FB, FALSE)) {
      pGeode->ShadowFB = TRUE;
      pGeode->NoAccel = TRUE;
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		   "Using \"Shadow Framebuffer\" - acceleration disabled\n"));
   }

   pGeode->Rotate = 0;
   if ((s = xf86GetOptValString(GeodeOptions, OPTION_ROTATE))) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "Rotating - %s\n", s));
      if (!xf86NameCmp(s, "CW")) {
	 pGeode->ShadowFB = TRUE;
	 pGeode->NoAccel = TRUE;
	 pGeode->HWCursor = FALSE;
	 pGeode->Rotate = 1;
	 DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		      "Rotating screen clockwise - acceleration disabled\n"));
      } else {
	 if (!xf86NameCmp(s, "CCW")) {
	    pGeode->ShadowFB = TRUE;
	    pGeode->NoAccel = TRUE;
	    pGeode->HWCursor = FALSE;
	    pGeode->Rotate = -1;
	    DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
			 "Rotating screen counter clockwise - acceleration \
					disabled\n"));
	 } else {
	    DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
			 "\"%s\" is not a valid value for Option \"Rotate\"\n",
			 s));
	    DEBUGMSG(1,
		     (pScreenInfo->scrnIndex, X_INFO,
		      "Valid options are \"CW\" or \"CCW\"\n"));
	 }
      }
   }

   /* XXX Init further private data here */

   /*
    * * This shouldn't happen because such problems should be caught in
    * * GeodeProbe(), but check it just in case.
    */
   if (pScreenInfo->chipset == NULL) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "ChipID 0x%04X is not recognised\n", pGeode->Chipset));
      return FALSE;
   }
   if (pGeode->Chipset < 0) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognised\n",
		   pScreenInfo->chipset));
      return FALSE;
   }
   GeodeDebug(("GX2PreInit(6)!\n"));

   /*
    * * Init the screen with some values
    */
#if !defined(STB_X)

   DEBUGMSG(1, (pScreenInfo->scrnIndex, from,
		"Video I/O registers at 0x%08lX\n",
		(unsigned long)VGAHW_GET_IOBASE()));
#endif /* STB_X */

   if (pScreenInfo->memPhysBase == 0) {
      from = X_PROBED;
#if defined(STB_X)
      pScreenInfo->memPhysBase = sAdapterInfo.dwFrameBufferBase;
#else /* STB_X */
      pScreenInfo->memPhysBase = gfx_get_frame_buffer_base();
#endif /* STB_X */
   }
   DEBUGMSG(1, (pScreenInfo->scrnIndex, from,
		"Linear framebuffer at 0x%08lX\n",
		(unsigned long)pScreenInfo->memPhysBase));

   if (pGeode->pEnt->device->videoRam == 0) {
      from = X_PROBED;
      pScreenInfo->videoRam = pGeode->FBSize / 1024;
   } else {
      pScreenInfo->videoRam = pGeode->pEnt->device->videoRam;
      from = X_CONFIG;
   }
   DEBUGMSG(1, (pScreenInfo->scrnIndex, from,
		"VideoRam: %d kByte\n",
		(unsigned long)pScreenInfo->videoRam));

   GeodeDebug(("GX2PreInit(7)!\n"));

   /*
    * * xf86ValidateModes will check that the mode HTotal and VTotal values
    * * don't exceed the chipset's limit if pScreenInfo->maxHValue adn
    * * pScreenInfo->maxVValue are set. Since our GX2ValidMode()
    * * already takes care of this, we don't worry about setting them here.
    */
   /* Select valid modes from those available */
   /*
    * * min pitch 1024, max 2048 (Pixel count)
    * * min height 480, max 1024 (Pixel count)
    */
   minPitch = 1024;
   maxPitch = 4096;			/* Can support upto 1600x1200 32Bpp */
   minHeight = 480;
   maxHeight = 1200;			/* Can support upto 1600x1200 32Bpp */
   if (pScreenInfo->depth > 16) {
      PitchInc = 4096;
   } else if (pScreenInfo->depth == 16) {
      PitchInc = 2048;
   } else {
      PitchInc = 1024;
   }
   PitchInc <<= 3;			/* in bits */

   /* by default use what user sets in the XF86Config file */
   modes = pScreenInfo->display->modes;

   i = xf86ValidateModes(pScreenInfo,
			 pScreenInfo->monitor->Modes,
			 modes,
			 &GeodeClockRange,
			 NULL, minPitch, maxPitch,
			 PitchInc, minHeight, maxHeight,
			 pScreenInfo->display->virtualX,
			 pScreenInfo->display->virtualY,
#if defined(STB_X)
			 sAdapterInfo.dwFrameBufferSize,
#else /* STB_X */
			 gfx_get_frame_buffer_size(),
#endif /* STB_X */
			 LOOKUP_BEST_REFRESH);

   DEBUGMSG(1, (pScreenInfo->scrnIndex, from,
		"xf86ValidateModes: %d %d %d\n",
		pScreenInfo->virtualX,
		pScreenInfo->virtualY, pScreenInfo->displayWidth));
   if (i == -1) {
      GX2FreeRec(pScreenInfo);
      return FALSE;
   }
   GeodeDebug(("GX2PreInit(8)!\n"));

   /* Prune the modes marked as invalid */
   xf86PruneDriverModes(pScreenInfo);

   GeodeDebug(("GX2PreInit(9)!\n"));
   if (i == 0 || pScreenInfo->modes == NULL) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "No valid modes found\n"));
      GX2FreeRec(pScreenInfo);
      return FALSE;
   }
   GeodeDebug(("GX2PreInit(10)!\n"));

   xf86SetCrtcForModes(pScreenInfo, 0);
   GeodeDebug(("GX2PreInit(11)!\n"));

   /* Set the current mode to the first in the list */
   pScreenInfo->currentMode = pScreenInfo->modes;
   GeodeDebug(("GX2PreInit(12)!\n"));

   /* Print the list of modes being used */
   xf86PrintModes(pScreenInfo);
   GeodeDebug(("GX2PreInit(13)!\n"));

   /* Set the display resolution */
   xf86SetDpi(pScreenInfo, 0, 0);
   GeodeDebug(("GX2PreInit(14)!\n"));

   /* Load bpp-specific modules */
   mod = NULL;

#if CFB
   /* Load bpp-specific modules */
   switch (pScreenInfo->bitsPerPixel) {
   case 8:
      mod = "cfb";
      reqSymbol = "cfbScreenInit";
      break;
   case 16:
      mod = "cfb16";
      reqSymbol = "cfb16ScreenInit";
      break;
   case 24:
      mod = "cfb24";
      reqSymbol = "cfb24ScreenInit";
      break;
   case 32:
      mod = "cfb32";
      reqSymbol = "cfb32ScreenInit";
      break;
   default:
      return FALSE;
   }
   if (mod && xf86LoadSubModule(pScreenInfo, mod) == NULL) {
      GX2FreeRec(pScreenInfo);
      return FALSE;
   }

   xf86LoaderReqSymbols(reqSymbol, NULL);
#else
   if (xf86LoadSubModule(pScreenInfo, "fb") == NULL) {
      GX2FreeRec(pScreenInfo);
      return FALSE;
   }

   xf86LoaderReqSymLists(nscFbSymbols, NULL);
#endif
   GeodeDebug(("GX2PreInit(15)!\n"));
   if (pGeode->NoAccel == FALSE) {
      if (!xf86LoadSubModule(pScreenInfo, "xaa")) {
	 GX2FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscXaaSymbols, NULL);
   }
   GeodeDebug(("GX2PreInit(16)!\n"));
   if (pGeode->HWCursor == TRUE) {
      if (!xf86LoadSubModule(pScreenInfo, "ramdac")) {
	 GX2FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscRamdacSymbols, NULL);
   }
   GeodeDebug(("GX2PreInit(17)!\n"));
   /* Load shadowfb if needed */
   if (pGeode->ShadowFB) {
      if (!xf86LoadSubModule(pScreenInfo, "shadowfb")) {
	 GX2FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscShadowSymbols, NULL);
   }
   GeodeDebug(("GX2PreInit(18)!\n"));
   if (xf86RegisterResources(pGeode->pEnt->index, NULL, ResExclusive)) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "xf86RegisterResources() found resource conflicts\n"));
      GX2FreeRec(pScreenInfo);
      return FALSE;
   }
   GX2UnmapMem(pScreenInfo);
   GeodeDebug(("GX2PreInit ... done successfully!\n"));
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX2Restore.
 *
 * Description	:This function restores the mode that was saved on server
                 entry
 * Parameters.
 * pScreenInfo 	:Handle to ScreenPtr structure.
 *  Pmode       :poits to screen mode
 * 												
 * Returns		:none.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static void
GX2Restore(ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode;

   GeodeDebug(("GX2Restore!\n"));
   /* Get driver private structure */
   if (!(pGeode = GX2GetRec(pScreenInfo)))
      return;
   if (pGeode->FBVGAActive) {
      vgaHWPtr pvgaHW = VGAHWPTR(pScreenInfo);

      vgaHWProtect(pScreenInfo, TRUE);
      vgaHWRestore(pScreenInfo, &pvgaHW->SavedReg, VGA_SR_ALL);
      vgaHWProtect(pScreenInfo, FALSE);
   }
}

/*----------------------------------------------------------------------------
 * GX2CalculatePitchBytes.
 *
 * Description	:This function restores the mode that was saved on server
 *
 * Parameters.
 * pScreenInfo 	:Handle to ScreenPtr structure.
 *    Pmode     :Points to screenmode
 * 									
 * Returns		:none.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static int
GX2CalculatePitchBytes(unsigned int width, unsigned int bpp)
{
   int lineDelta = width * (bpp >> 3);

   if (width < 640) {
      /* low resolutions have both pixel and line doubling */
      DEBUGMSG(1, (0, X_PROBED, "lower resolution %d %d\n",
		   width, lineDelta));
      lineDelta <<= 1;
   }
   /* needed in Rotate mode when in accel is turned off */
   if (1) {				/*!pGeode->NoAccel */
      if (lineDelta > 4096)
	 lineDelta = 8192;
      else if (lineDelta > 2048)
	 lineDelta = 4096;
      else if (lineDelta > 1024)
	 lineDelta = 2048;
      else
	 lineDelta = 1024;
   }

   DEBUGMSG(1, (0, X_PROBED, "pitch %d %d\n", width, lineDelta));

   return lineDelta;
}

/*----------------------------------------------------------------------------
 * GX2GetRefreshRate.
 *
 * Description	:This function restores the mode that saved on server
 *
 * Parameters.
 *     Pmode    :Pointer to the screen modes
 * 												
 * Returns		:It returns the selected refresh rate.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static int
GX2GetRefreshRate(DisplayModePtr pMode)
{
#define THRESHOLD 2
   unsigned int i;
   static int validRates[] = { 50, 56, 60, 70, 72, 75, 85 };	/* Hz */
   unsigned long dotClock;
   int refreshRate;
   int selectedRate;

   dotClock = pMode->SynthClock * 1000;
   refreshRate = dotClock / pMode->CrtcHTotal / pMode->CrtcVTotal;

   if ((pMode->CrtcHTotal < 640) && (pMode->CrtcVTotal < 480))
      refreshRate >>= 2;		/* double pixel and double scan */

   DEBUGMSG(1, (0, X_PROBED, "dotclock %d %d\n", dotClock, refreshRate));

   selectedRate = validRates[0];

   for (i = 0; i < (sizeof(validRates) / sizeof(validRates[0])); i++) {
      if (validRates[i] < (refreshRate + THRESHOLD)) {
	 selectedRate = validRates[i];
      }
   }
   return selectedRate;
}

void
gx2_clear_screen(int width, int height)
{
   /* clean up the frame buffer memory */
   GFX(set_solid_pattern(0));
   GFX(set_raster_operation(0xF0));
   GFX(pattern_fill(0, 0, width, height));
}

void
gx2_set_DvLineSize(unsigned int pitch)
{
   unsigned long temp, dv_size = MDC_DV_LINE_SIZE_1024;

   if (pitch > 1024) {
      dv_size = MDC_DV_LINE_SIZE_2048;
   }
   if (pitch > 2048) {
      dv_size = MDC_DV_LINE_SIZE_4096;
   }
   if (pitch > 4096) {
      dv_size = MDC_DV_LINE_SIZE_8192;
   }

   /* WRITE DIRTY/VALID CONTROL WITH LINE LENGTH */

#if defined(STB_X)
   Gal_read_register(GAL_REG, MDC_DV_CTL, &temp, 4);
   temp = (temp & ~MDC_DV_LINE_SIZE_MASK) | dv_size;
   Gal_write_register(GAL_REG, MDC_DV_CTL, temp, 4);
#else
   temp = READ_REG32(MDC_DV_CTL);
   WRITE_REG32(MDC_DV_CTL, (temp & ~MDC_DV_LINE_SIZE_MASK) | dv_size);
#endif
}

/*----------------------------------------------------------------------------
 * GX2SetMode.
 *
 * Description	:This function sets parametrs for screen mode
 *
 * Parameters.
 * pScreenInfo 	:Pointer to the screenInfo structure.
 *	 Pmode      :Pointer to the screen modes
 * 												
 * Returns		:TRUE on success and FALSE on Failure.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/

static Bool
GX2SetMode(ScrnInfoPtr pScreenInfo, DisplayModePtr pMode)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   DCount = 50;
   /* unsigned int compOffset, compPitch, compSize; */
   GeodeDebug(("GX2SetMode!\n"));
#if !defined(STB_X)
   DEBUGMSG(1, (0, X_NONE, "Set mode %X %X %X %X %X\n",
		gfx_virt_regptr,
		gfx_virt_gpptr,
		gfx_virt_spptr, gfx_virt_vidptr, gfx_virt_fbptr));
#endif /* STB_X */

   /* Set the VT semaphore */
   pScreenInfo->vtSema = TRUE;
   DEBUGMSG(1, (0, X_NONE, "Set mode"));

   /* The timing will be adjusted later */
   GeodeDebug(("Set display mode: %dx%d-%d (%dHz) Pitch %d\n",
	       pMode->CrtcHDisplay,
	       pMode->CrtcVDisplay,
	       pScreenInfo->bitsPerPixel,
	       GX2GetRefreshRate(pMode), pGeode->Pitch));

   GeodeDebug(("Before setting the mode\n"));
   if (1) {
      {					/* TV not selected */

	 DEBUGMSG(1, (0, X_PROBED, "Setting Display for CRT or TFT\n"));

	 if (pGeode->Panel) {
	    DEBUGMSG(0, (0, X_PROBED, "Setting Display for TFT\n"));
	    DEBUGMSG(1, (0, X_PROBED, "Restore Panel %d %d %d %d %d\n",
			 pGeode->FPBX, pGeode->FPBY,
			 pMode->CrtcHDisplay,
			 pMode->CrtcVDisplay, pScreenInfo->bitsPerPixel));

	    DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "FP Bios %d\n",
			 pGeode->Panel));
	    GFX(set_fixed_timings(pGeode->FPBX, pGeode->FPBY,
				  pMode->CrtcHDisplay,
				  pMode->CrtcVDisplay,
				  pScreenInfo->bitsPerPixel));
	 } else {
	    /* display is crt */
	    DEBUGMSG(1, (0, X_PROBED, "Setting Display for CRT %dx%d-%d@%d\n",
			 pMode->CrtcHDisplay,
			 pMode->CrtcVDisplay,
			 pScreenInfo->bitsPerPixel,
			 GX2GetRefreshRate(pMode)));
	    GFX(set_display_mode(pMode->CrtcHDisplay,
				 pMode->CrtcVDisplay,
				 pScreenInfo->bitsPerPixel,
				 GX2GetRefreshRate(pMode)));

	    /* adjust the pitch */
	    GFX(set_display_pitch(pGeode->Pitch));
	 }
	 GFX(set_bpp(pScreenInfo->bitsPerPixel));
	 /* enable crt */
	 GFX(set_crt_enable(CRT_ENABLE));
      }
      GFX(set_display_offset(0L));
      GFX(wait_vertical_blank());

      DEBUGMSG(1, (0, X_PROBED, "Display mode set\n"));
      /* enable compression if option selected */
      if (pGeode->Compression) {
	 DEBUGMSG(1, (0, X_PROBED, "Compression mode set %d\n",
		      pGeode->Compression));
	 /* set the compression parameters,and it will be turned on later. */
	 gx2_set_DvLineSize(pGeode->Pitch);

#if defined(STB_X)
	 Gal_set_compression_parameters(GAL_COMPRESSION_ALL,
					pGeode->CBOffset,
					pGeode->CBPitch, pGeode->CBSize);

	 /* set the compression buffer, all parameters already set */
	 Gal_set_compression_enable(GAL_COMPRESSION_ENABLE);
#else /* STB_X */
	 gfx_set_compression_offset(pGeode->CBOffset);
	 gfx_set_compression_pitch(pGeode->CBPitch);
	 gfx_set_compression_size(pGeode->CBSize);

	 /* set the compression buffer, all parameters already set */
	 gfx_set_compression_enable(1);
#endif /* STB_X */

      }
      if (pGeode->HWCursor) {
	 /* Load blank cursor */
	 GX2LoadCursorImage(pScreenInfo, NULL);
	 GFX(set_cursor_position(pGeode->CursorStartOffset, 0, 0, 0, 0));
	 GFX(set_cursor_enable(1));
      }
   } else {
      GeodeDebug(("GX2Restore ... "));
      GX2Restore(pScreenInfo);
      GeodeDebug(("done.\n"));
   }

   GeodeDebug(("done.\n"));
   /* Reenable the hardware cursor after the mode switch */
   if (pGeode->HWCursor == TRUE) {
      GeodeDebug(("GX2ShowCursor ... "));
      GX2ShowCursor(pScreenInfo);
      GeodeDebug(("done.\n"));
   }
   /* Restore the contents in the screen info */
   GeodeDebug(("After setting the mode\n"));
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX2EnterGraphics.
 *
 * Description	:This function will intiallize the displaytiming
				 structure for nextmode and switch to VGA mode.
 *
 * Parameters.
 *    pScreen   :Screen information will be stored in this structure.
 * 	pScreenInfo :Pointer to the screenInfo structure.
 *													
 * Returns		:TRUE on success and FALSE on Failure.
 *
 * Comments     :gfx_vga_mode_switch() will start and end the
 *				switching based on the arguments 0 or 1.soft_vga
 *				is disabled in this function.
*----------------------------------------------------------------------------
*/
static Bool
GX2EnterGraphics(ScreenPtr pScreen, ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode = GX2GetRec(pScreenInfo);

#if defined(STB_X)
   Gal_get_display_timing(&pGeode->FBgfxdisplaytiming);

   /* Save Display offset */
   Gal_get_display_offset(&(pGeode->FBDisplayOffset));

   /* Save the current Compression state */
   Gal_get_compression_enable(&(pGeode->FBCompressionEnable));
   Gal_get_compression_parameters(GAL_COMPRESSION_ALL,
				  &(pGeode->FBCompressionOffset),
				  &(pGeode->FBCompressionPitch),
				  &(pGeode->FBCompressionSize));

   /* Save Cursor offset */
   {
      unsigned short x, y, xhot, yhot;

      Gal_get_cursor_position(&(pGeode->FBCursorOffset),
			      &x, &y, &xhot, &yhot);
   }
   /* Save the Panel state */
   Gal_pnl_save();
#else /* STB_X */
   /* Save CRT State */
   pGeode->FBgfxdisplaytiming.dwDotClock = gfx_get_clock_frequency();
   pGeode->FBgfxdisplaytiming.wPitch = gfx_get_display_pitch();
   pGeode->FBgfxdisplaytiming.wBpp = gfx_get_display_bpp();
   pGeode->FBgfxdisplaytiming.wHTotal = gfx_get_htotal();
   pGeode->FBgfxdisplaytiming.wHActive = gfx_get_hactive();
   pGeode->FBgfxdisplaytiming.wHSyncStart = gfx_get_hsync_start();
   pGeode->FBgfxdisplaytiming.wHSyncEnd = gfx_get_hsync_end();
   pGeode->FBgfxdisplaytiming.wHBlankStart = gfx_get_hblank_start();
   pGeode->FBgfxdisplaytiming.wHBlankEnd = gfx_get_hblank_end();
   pGeode->FBgfxdisplaytiming.wVTotal = gfx_get_vtotal();
   pGeode->FBgfxdisplaytiming.wVActive = gfx_get_vactive();
   pGeode->FBgfxdisplaytiming.wVSyncStart = gfx_get_vsync_start();
   pGeode->FBgfxdisplaytiming.wVSyncEnd = gfx_get_vsync_end();
   pGeode->FBgfxdisplaytiming.wVBlankStart = gfx_get_vblank_start();
   pGeode->FBgfxdisplaytiming.wVBlankEnd = gfx_get_vblank_end();
   pGeode->FBgfxdisplaytiming.wPolarity = gfx_get_sync_polarities();

   /* Save Display offset */
   pGeode->FBDisplayOffset = gfx_get_display_offset();

   /* Save the current Compression state */
   pGeode->FBCompressionEnable = gfx_get_compression_enable();
   pGeode->FBCompressionOffset = gfx_get_compression_offset();
   pGeode->FBCompressionPitch = gfx_get_compression_pitch();
   pGeode->FBCompressionSize = gfx_get_compression_size();

   /* Save Cursor offset */
   pGeode->FBCursorOffset = gfx_get_cursor_offset();

   /* Save the Panel state */
   Pnl_SavePanelState();

   /* only if comming from VGA */
   if (pGeode->FBVGAActive) {
      unsigned short sequencer;
      vgaHWPtr pvgaHW = VGAHWPTR(pScreenInfo);

      /* Map VGA aperture */
      if (!vgaHWMapMem(pScreenInfo))
	 return FALSE;

      /* Unlock VGA registers */
      vgaHWUnlock(pvgaHW);

      /* Save the current state and setup the current mode */
      vgaHWSave(pScreenInfo, &VGAHWPTR(pScreenInfo)->SavedReg, VGA_SR_ALL);

      /* DISABLE VGA SEQUENCER */
      /* This allows the VGA state machine to terminate. We must delay */
      /* such that there are no pending MBUS requests.  */

      gfx_outb(MDC_SEQUENCER_INDEX, MDC_SEQUENCER_CLK_MODE);
      sequencer = gfx_inb(MDC_SEQUENCER_DATA);
      sequencer |= MDC_CLK_MODE_SCREEN_OFF;
      gfx_outb(MDC_SEQUENCER_DATA, sequencer);

      gfx_delay_milliseconds(1);

      /* BLANK THE VGA DISPLAY */
      gfx_outw(MDC_SEQUENCER_INDEX, MDC_SEQUENCER_RESET);
      sequencer = gfx_inb(MDC_SEQUENCER_DATA);
      sequencer &= ~MDC_RESET_VGA_DISP_ENABLE;
      gfx_outb(MDC_SEQUENCER_DATA, sequencer);

      gfx_delay_milliseconds(1);
   }
#endif /* STB */

   if (!GX2SetMode(pScreenInfo, pScreenInfo->currentMode)) {
      return FALSE;
   }

   /* clear the frame buffer, for annoying noise during mode switch */
   gx2_clear_screen(pScreenInfo->currentMode->CrtcHDisplay,
		    pScreenInfo->currentMode->CrtcVDisplay);

   return TRUE;
}

#if !defined(STB_X)
void
EnableDACPower(void)
{
   /* enable the DAC POWER */
   gfx_write_vid32(RCDF_VID_MISC,
		   gfx_read_vid32(RCDF_VID_MISC) & RCDF_GAMMA_BYPASS_BOTH);
}

void
redcloud_gfx_2_vga_fix(void)
{
   /* enable the DAC POWER */
   EnableDACPower();
#if 0
   int i;

   /* set the character width to 9 */
   gfx_outb(0x3C4, 0x1);
   gfx_outb(0x3C5, 0x2);

   /* clear the gfx mode bit in VGA Attribute controller */
   gfx_inb(0x3DA);
   gfx_outb(0x3C0, 0x30);
   gfx_outb(0x3C0, 0xC);

   /* Re init the EGA Palaette */
   for (i = 0; i < 16; i++) {
      gfx_inb(0x3DA);
      gfx_outb(0x3C0, i);
      gfx_outb(0x3C0, i);
   }

   /* Re init the Overscan color to black */
   gfx_inb(0x3DA);
   gfx_outb(0x3C0, 0x11);
   gfx_outb(0x3C0, 0x0);

   /* Re Enable all the 4 color planes */
   gfx_inb(0x3DA);
   gfx_outb(0x3C0, 0x12);
   gfx_outb(0x3C0, 0xF);

   /* Clear Pixel Panning in VGA Attribute controller */
   gfx_inb(0x3DA);
   gfx_outb(0x3C0, 0x33);
   gfx_outb(0x3C0, 0x8);

   /* ??????????????????????? */
   gfx_outb(0x3C0, 0x20);
   gfx_outb(0x3C0, 0x20);
#endif
}
#endif /* STB_X */

/*----------------------------------------------------------------------------
 * GX2LeaveGraphics:
 *
 * Description	:This function will restore the displaymode parameters
 * 				 and switches the VGA mode
 *
 * Parameters.
 *    pScreen   :Screen information will be stored in this structure.
 * 	pScreenInfo :Pointer to the screenInfo structure.
 *	
 * 												
 * Returns		:none.
 *
 * Comments		: gfx_vga_mode_switch() will start and end the switching
 *			      based on the arguments 0 or 1.soft_vga is disabled in
 *                    this function.
*----------------------------------------------------------------------------
*/
static void
GX2LeaveGraphics(ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode = GX2GetRec(pScreenInfo);

   /* Restore VG registers */
#if defined(STB_X)
   Gal_set_display_timing(&pGeode->FBgfxdisplaytiming);

   Gal_set_display_offset(pGeode->FBDisplayOffset);

   /* Restore Cursor */
   Gal_set_cursor_position(pGeode->FBCursorOffset, 0, 0, 0, 0);

   /* Restore the previous Compression state */
   if (pGeode->FBCompressionEnable) {
      Gal_set_compression_parameters(GAL_COMPRESSION_ALL,
				     pGeode->FBCompressionOffset,
				     pGeode->FBCompressionPitch,
				     pGeode->FBCompressionSize);

      Gal_set_compression_enable(GAL_COMPRESSION_ENABLE);
   }
#else /* STB_X */
   gfx_set_display_timings(pGeode->FBgfxdisplaytiming.wBpp,
			   pGeode->FBgfxdisplaytiming.wPolarity,
			   pGeode->FBgfxdisplaytiming.wHActive,
			   pGeode->FBgfxdisplaytiming.wHBlankStart,
			   pGeode->FBgfxdisplaytiming.wHSyncStart,
			   pGeode->FBgfxdisplaytiming.wHSyncEnd,
			   pGeode->FBgfxdisplaytiming.wHBlankEnd,
			   pGeode->FBgfxdisplaytiming.wHTotal,
			   pGeode->FBgfxdisplaytiming.wVActive,
			   pGeode->FBgfxdisplaytiming.wVBlankStart,
			   pGeode->FBgfxdisplaytiming.wVSyncStart,
			   pGeode->FBgfxdisplaytiming.wVSyncEnd,
			   pGeode->FBgfxdisplaytiming.wVBlankEnd,
			   pGeode->FBgfxdisplaytiming.wVTotal,
			   pGeode->FBgfxdisplaytiming.dwDotClock);

   gfx_set_compression_enable(0);

   /* Restore the previous Compression state */
   if (pGeode->FBCompressionEnable) {
      gfx_set_compression_offset(pGeode->FBCompressionOffset);
      gfx_set_compression_pitch(pGeode->FBCompressionPitch);
      gfx_set_compression_size(pGeode->FBCompressionSize);
      gfx_set_compression_enable(1);
   }

   gfx_set_display_pitch(pGeode->FBgfxdisplaytiming.wPitch);

   gfx_set_display_offset(pGeode->FBDisplayOffset);

   /* Restore Cursor */
   gfx_set_cursor_position(pGeode->FBCursorOffset, 0, 0, 0, 0);

   GeodeDebug(("FBVGAActive %d\n", pGeode->FBVGAActive));
   if (pGeode->FBVGAActive) {
      pGeode->vesa->pInt->num = 0x10;
      pGeode->vesa->pInt->ax = 0x3;
      pGeode->vesa->pInt->bx = 0;
      xf86ExecX86int10(pGeode->vesa->pInt);
      gfx_delay_milliseconds(3);
      EnableDACPower();
   }
#endif /* STB_X */
}

/*----------------------------------------------------------------------------
 * GX2CloseScreen.
 *
 * Description	:This function will restore the original mode
 *				 and also it unmap video memory
 *
 * Parameters.
 *    ScrnIndex	:Screen index value of the screen will be closed.
 * 	pScreen    	:Pointer to the screen structure.
 *	
 * 												
 * Returns		:TRUE on success and FALSE on Failure.
 *
 * Comments		:none.
*----------------------------------------------------------------------------
*/
static Bool
GX2CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   if (pGeode->ShadowPtr)
      xfree(pGeode->ShadowPtr);

   DEBUGMSG(0, (scrnIndex, X_PROBED, "GX2CloseScreen %d\n",
		pScreenInfo->vtSema));
   if (pScreenInfo->vtSema)
      GX2LeaveGraphics(pScreenInfo);

   if (pGeode->AccelInfoRec)
      XAADestroyInfoRec(pGeode->AccelInfoRec);

   if (pGeode->AccelImageWriteBufferOffsets) {
      xfree(pGeode->AccelImageWriteBufferOffsets);
      pGeode->AccelImageWriteBufferOffsets = 0x0;
   }
   /* free the allocated off screen area */
   xf86FreeOffscreenArea(pGeode->AccelImgArea);
   xf86FreeOffscreenArea(pGeode->CompressionArea);

   pScreenInfo->vtSema = FALSE;

   GX2UnmapMem(pScreenInfo);
   if (pGeode && (pScreen->CloseScreen = pGeode->CloseScreen)) {
      pGeode->CloseScreen = NULL;
      return ((*pScreen->CloseScreen) (scrnIndex, pScreen));
   }
   return TRUE;
}

#ifdef DPMSExtension
/*----------------------------------------------------------------------------
 * GX2DPMSSet.
 *
 * Description	:This function sets geode into Power Management
 *               Signalling mode.				
 *
 * Parameters.
 * 	pScreenInfo	 :Pointer to screen info strucrure.
 * 	mode         :Specifies the power management mode.
 *	 												
 * Returns		 :none.
 *
 * Comments      :none.
*----------------------------------------------------------------------------
*/
static void
GX2DPMSSet(ScrnInfoPtr pScreenInfo, int mode, int flags)
{
   GeodePtr pGeode;

   pGeode = GEODEPTR(pScreenInfo);

   GeodeDebug(("GX2DPMSSet!\n"));

   /* Check if we are actively controlling the display */
   if (!pScreenInfo->vtSema) {
      ErrorF("GX2DPMSSet called when we not controlling the VT!\n");
      return;
   }
   switch (mode) {
   case DPMSModeOn:
      /* Screen: On; HSync: On; VSync: On */
      GFX(set_crt_enable(CRT_ENABLE));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerup();
#else /* STB_X */
      if (pGeode->Panel)
	 Pnl_PowerUp();
#endif /* STB_X */
      break;

   case DPMSModeStandby:
      /* Screen: Off; HSync: Off; VSync: On */
      GFX(set_crt_enable(CRT_STANDBY));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else /* STB_X */
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      break;

   case DPMSModeSuspend:
      /* Screen: Off; HSync: On; VSync: Off */
      GFX(set_crt_enable(CRT_SUSPEND));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else /* STB_X */
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      break;
   case DPMSModeOff:
      /* Screen: Off; HSync: Off; VSync: Off */
      GFX(set_crt_enable(CRT_DISABLE));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else /* STB_X */
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      break;
   }
}
#endif

/*----------------------------------------------------------------------------
 * GX2ScreenInit.
 *
 * Description	:This function will be called at the each ofserver
 *   			 generation.				
 *
 * Parameters.
 *   scrnIndex   :Specfies the screenindex value during generation.
 *    pScreen	 :Pointer to screen info strucrure.
 * 	argc         :parameters for command line arguments count
 *	argv         :command line arguments if any it is not used.  												
 *
 * Returns		 :none.
 *
 * Comments      :none.
*----------------------------------------------------------------------------
*/
static Bool
GX2ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[pScreen->myNum];
   GeodePtr pGeode;
   int i;
   Bool Inited = FALSE;
   unsigned char *FBStart;
   unsigned int req_offscreenmem;
   int width, height, displayWidth;
   VisualPtr visual;
   BoxRec AvailBox;
   RegionRec OffscreenRegion;

   DCount = 30;
   GeodeDebug(("GX2ScreenInit!\n"));
   /* Get driver private */
   pGeode = GX2GetRec(pScreenInfo);
   GeodeDebug(("GX2ScreenInit(0)!\n"));
   /*
    * * Allocate a vgaHWRec
    */
   GeodeDebug(("FBVGAActive %d\n", pGeode->FBVGAActive));
   if (pGeode->FBVGAActive) {
      if (!vgaHWGetHWRec(pScreenInfo))
	 return FALSE;
      if (!vgaHWMapMem(pScreenInfo))
	 return FALSE;
      vgaHWGetIOBase(VGAHWPTR(pScreenInfo));
   }

   if (!GX2MapMem(pScreenInfo))
      return FALSE;

   pGeode->Pitch = GX2CalculatePitchBytes(pScreenInfo->virtualX,
					  pScreenInfo->bitsPerPixel);

   /* SET UP GRAPHICS MEMORY AVAILABLE FOR PIXMAP CACHE */
   AvailBox.x1 = 0;
   AvailBox.y1 = pScreenInfo->virtualY;
   AvailBox.x2 = pScreenInfo->displayWidth;
   AvailBox.y2 = (pGeode->FBSize / pGeode->Pitch);

   pGeode->CursorSize = 16 * 64;	/* 64x64 */

   if (pGeode->HWCursor) {
      /* Compute cursor buffer */
      /* Default cursor offset, end of the frame buffer */
      pGeode->CursorStartOffset = pGeode->FBSize - pGeode->CursorSize;
      AvailBox.y2 -= 1;
   }

   DEBUGMSG(1, (scrnIndex, X_PROBED,
		"Memory manager initialized to (%d,%d) (%d,%d) %d %d %d\n",
		AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2,
		pGeode->Pitch, pScreenInfo->displayWidth,
		pScreenInfo->bitsPerPixel));

   /* set the offscreen offset accordingly */
   if (pGeode->Compression) {

      pGeode->CBPitch = 544;
      pGeode->CBSize = 544;

      req_offscreenmem = pScreenInfo->virtualY * pGeode->CBPitch;
      req_offscreenmem += pGeode->Pitch - 1;
      req_offscreenmem /= pGeode->Pitch;
      pGeode->CBOffset = AvailBox.y1 * pGeode->Pitch;
      AvailBox.y1 += req_offscreenmem;
   }
   DEBUGMSG(1, (scrnIndex, X_PROBED,
		"Memory manager initialized to (%d,%d) (%d,%d)\n",
		AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2));

   if (!pGeode->NoAccel) {
      if (pGeode->NoOfImgBuffers > 0) {
	 if (pGeode->NoOfImgBuffers <= (AvailBox.y2 - AvailBox.y1)) {
	    pGeode->AccelImageWriteBufferOffsets =
		  xalloc(sizeof(unsigned long) * pGeode->NoOfImgBuffers);

	    pGeode->AccelImageWriteBufferOffsets[0] =
		  ((unsigned char *)pGeode->FBBase) +
		  (AvailBox.y1 * pGeode->Pitch);

	    for (i = 1; i < pGeode->NoOfImgBuffers; i++) {
	       pGeode->AccelImageWriteBufferOffsets[i] =
		     pGeode->AccelImageWriteBufferOffsets[i - 1] +
		     pGeode->Pitch;
	    }

	    for (i = 0; i < pGeode->NoOfImgBuffers; i++) {
	       DEBUGMSG(1, (scrnIndex, X_PROBED,
			    "memory  %d %x\n", i,
			    pGeode->AccelImageWriteBufferOffsets[i]));
	    }
	    AvailBox.y1 += pGeode->NoOfImgBuffers;
	 } else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Unable to reserve scanline area\n");
	 }
      }
      DEBUGMSG(1, (scrnIndex, X_PROBED,
		   "Memory manager initialized to (%d,%d) (%d,%d)\n",
		   AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2));

      REGION_INIT(pScreen, &OffscreenRegion, &AvailBox, 2);

      if (!xf86InitFBManagerRegion(pScreen, &OffscreenRegion)) {
	 xf86DrvMsg(scrnIndex, X_ERROR,
		    "Memory manager initialization to (%d,%d) (%d,%d) failed\n",
		    AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2);
      } else {
	 xf86DrvMsg(scrnIndex, X_INFO,
		    "Memory manager initialized to (%d,%d) (%d,%d)\n",
		    AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2);
      }
      REGION_UNINIT(pScreen, &OffscreenRegion);
   }

   /* Initialise graphics mode */
   if (!GX2EnterGraphics(pScreen, pScreenInfo))
      return FALSE;

   GX2AdjustFrame(scrnIndex, pScreenInfo->frameX0, pScreenInfo->frameY0, 0);
   GeodeDebug(("GX2ScreenInit(1)!\n"));

   /* Reset visual list */
   miClearVisualTypes();
   GeodeDebug(("GX2ScreenInit(2)!\n"));

   /* Setup the visual we support */
   if (pScreenInfo->bitsPerPixel > 8) {
      DEBUGMSG(1, (scrnIndex, X_PROBED,
		   "miSetVisualTypes %d %X %X %X\n",
		   pScreenInfo->depth,
		   TrueColorMask,
		   pScreenInfo->rgbBits, pScreenInfo->defaultVisual));

      if (!miSetVisualTypes(pScreenInfo->depth,
			    TrueColorMask,
			    pScreenInfo->rgbBits,
			    pScreenInfo->defaultVisual)) {
	 return FALSE;
      }
   } else {
      if (!miSetVisualTypes(pScreenInfo->depth,
			    miGetDefaultVisualMask(pScreenInfo->depth),
			    pScreenInfo->rgbBits,
			    pScreenInfo->defaultVisual)) {
	 return FALSE;
      }
   }
   GeodeDebug(("GX2ScreenInit(3)!\n"));

   /* Set for RENDER extensions */
   miSetPixmapDepths();

   /* Call the framebuffer layer's ScreenInit function, and fill in other
    * * pScreen fields.
    */

   width = pScreenInfo->virtualX;
   height = pScreenInfo->virtualY;

   displayWidth = pScreenInfo->displayWidth;
   if (pGeode->Rotate) {
      width = pScreenInfo->virtualY;
      height = pScreenInfo->virtualX;
   }
   if (pGeode->ShadowFB) {
      pGeode->ShadowPitch = BitmapBytePad(pScreenInfo->bitsPerPixel * width);
      pGeode->ShadowPtr = xalloc(pGeode->ShadowPitch * height);
      displayWidth = pGeode->ShadowPitch / (pScreenInfo->bitsPerPixel >> 3);
      FBStart = pGeode->ShadowPtr;
   } else {
      pGeode->ShadowPtr = NULL;

      FBStart = pGeode->FBBase;
      DEBUGMSG(1, (0, X_PROBED, "FBStart %X \n", FBStart));
   }

   switch (pScreenInfo->bitsPerPixel) {
#if CFB
   case 8:
      Inited = cfbScreenInit(pScreen, FBStart, width, height,
			     pScreenInfo->xDpi, pScreenInfo->yDpi,
			     displayWidth);
      break;
   case 16:
      Inited = cfb16ScreenInit(pScreen, FBStart, width, height,
			       pScreenInfo->xDpi, pScreenInfo->yDpi,
			       displayWidth);
      break;
   case 24:
   case 32:
      Inited = cfb32ScreenInit(pScreen, FBStart, width, height,
			       pScreenInfo->xDpi, pScreenInfo->yDpi,
			       displayWidth);
      break;
#else
   case 8:
   case 16:
   case 24:
   case 32:
      Inited = fbScreenInit(pScreen, FBStart, width, height,
			    pScreenInfo->xDpi, pScreenInfo->yDpi,
			    displayWidth, pScreenInfo->bitsPerPixel);
      break;
#endif
   default:
      xf86DrvMsg(scrnIndex, X_ERROR,
		 "Internal error: invalid bpp (%d) in ScreenInit\n",
		 pScreenInfo->bitsPerPixel);
      Inited = FALSE;
      break;
   }
   if (!Inited)
      return FALSE;

   GeodeDebug(("GX2ScreenInit(4)!\n"));
   xf86SetBlackWhitePixels(pScreen);

   if (!pGeode->ShadowFB) {
      GX2DGAInit(pScreen);
   }
   GeodeDebug(("GX2ScreenInit(5)!\n"));
   if (pScreenInfo->bitsPerPixel > 8) {
      /* Fixup RGB ordering */
      visual = pScreen->visuals + pScreen->numVisuals;
      while (--visual >= pScreen->visuals) {
	 if ((visual->class | DynamicClass) == DirectColor) {
	    visual->offsetRed = pScreenInfo->offset.red;
	    visual->offsetGreen = pScreenInfo->offset.green;
	    visual->offsetBlue = pScreenInfo->offset.blue;
	    visual->redMask = pScreenInfo->mask.red;
	    visual->greenMask = pScreenInfo->mask.green;
	    visual->blueMask = pScreenInfo->mask.blue;
	 }
      }
   }
#if CFB
#else
   /* must be after RGB ordering fixed */
   fbPictureInit(pScreen, 0, 0);
#endif

   GeodeDebug(("GX2ScreenInit(6)!\n"));
   if (!pGeode->NoAccel) {
      GX2AccelInit(pScreen);
   }
   GeodeDebug(("GX2ScreenInit(7)!\n"));
   miInitializeBackingStore(pScreen);
   xf86SetBackingStore(pScreen);
   GeodeDebug(("GX2ScreenInit(8)!\n"));
   /* Initialise software cursor */
   miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
   /* Initialize HW cursor layer.
    * * Must follow software cursor initialization
    */
   if (pGeode->HWCursor) {
      if (!GX2HWCursorInit(pScreen))
	 xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		    "Hardware cursor initialization failed\n");
   }
   GeodeDebug(("GX2ScreenInit(9)!\n"));
   /* Setup default colourmap */
   if (!miCreateDefColormap(pScreen)) {
      return FALSE;
   }
   GeodeDebug(("GX2ScreenInit(10)!\n"));
   /* Initialize colormap layer.
    * * Must follow initialization of the default colormap
    */
   if (!xf86HandleColormaps(pScreen, 256, 8,
			    GX2LoadPalette, NULL,
			    CMAP_PALETTED_TRUECOLOR |
			    CMAP_RELOAD_ON_MODE_SWITCH)) {
      return FALSE;
   }

   GeodeDebug(("GX2ScreenInit(11)!\n"));

   if (pGeode->ShadowFB) {
      RefreshAreaFuncPtr refreshArea = GX2RefreshArea;

      if (pGeode->Rotate) {
	 if (!pGeode->PointerMoved) {
	    pGeode->PointerMoved = pScreenInfo->PointerMoved;
	    pScreenInfo->PointerMoved = GX2PointerMoved;
	 }
	 switch (pScreenInfo->bitsPerPixel) {
	 case 8:
	    refreshArea = GX2RefreshArea8;
	    break;
	 case 16:
	    refreshArea = GX2RefreshArea16;
	    break;
	 case 24:
	    refreshArea = GX2RefreshArea24;
	    break;
	 case 32:
	    refreshArea = GX2RefreshArea32;
	    break;
	 }
      }
      ShadowFBInit(pScreen, refreshArea);
   }
#ifdef DPMSExtension
   xf86DPMSInit(pScreen, GX2DPMSSet, 0);
#endif
   GeodeDebug(("GX2ScreenInit(12)!\n"));

   pScreenInfo->memPhysBase = (unsigned long)pGeode->FBBase;
   pScreenInfo->fbOffset = 0;

   GeodeDebug(("GX2ScreenInit(13)!\n"));
   GX2InitVideo(pScreen);		/* needed for video */
   /* Wrap the screen's CloseScreen vector and set its
    * SaveScreen vector 
    */
   pGeode->CloseScreen = pScreen->CloseScreen;
   pScreen->CloseScreen = GX2CloseScreen;

   pScreen->SaveScreen = GX2SaveScreen;
   GeodeDebug(("GX2ScreenInit(14)!\n"));

   /* Report any unused options */
   if (serverGeneration == 1) {
      xf86ShowUnusedOptions(pScreenInfo->scrnIndex, pScreenInfo->options);
   }
   GeodeDebug(("GX2ScreenInit(15)!\n"));
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX2SwitchMode.
 *
 * Description	:This function will switches the screen mode
 *   			    				
 * Parameters:
 *    scrnIndex	:Specfies the screen index value.
 *    pMode		:pointer to the mode structure.
 * 	  flags     :may be used for status check?.
 *	  												
 * Returns		:Returns TRUE on success and FALSE on failure.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
Bool
GX2SwitchMode(int scrnIndex, DisplayModePtr pMode, int flags)
{
   GeodeDebug(("GX2SwitchMode!\n"));
   return GX2SetMode(xf86Screens[scrnIndex], pMode);
}

/*----------------------------------------------------------------------------
 * GX2AdjustFrame.
 *
 * Description	:This function is used to intiallize the start
 *				 address of the memory.
 * Parameters.
 *    scrnIndex	:Specfies the screen index value.
 *     x     	:x co-ordinate value interms of pixels.
 * 	   y        :y co-ordinate value interms of pixels.
 *	  												
 * Returns		:none.
 *
 * Comments    	:none.
*----------------------------------------------------------------------------
*/
void
GX2AdjustFrame(int scrnIndex, int x, int y, int flags)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   GeodePtr pGeode = GX2GetRec(pScreenInfo);
   unsigned long offset;

   /* y offset */
   offset = (unsigned long)y *(unsigned long)pGeode->Pitch;

   /* x offset */
   offset += x * (pScreenInfo->bitsPerPixel >> 3);

   GFX(set_display_offset(offset));
}

/*----------------------------------------------------------------------------
 * GX2EnterVT.
 *
 * Description	:This is called when VT switching back to the X server
 *			
 * Parameters.
 *    scrnIndex	:Specfies the screen index value.
 *     flags   	:Not used inside the function.
 * 	 						
 * Returns		:none.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static Bool
GX2EnterVT(int scrnIndex, int flags)
{
   GeodeDebug(("GX2EnterVT!\n"));
   return GX2EnterGraphics(NULL, xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX2LeaveVT.
 *
 * Description	:This is called when VT switching  X server text mode.
 *			
 * Parameters.
 *    scrnIndex	:Specfies the screen index value.
 *     flags    :Not used inside the function.
 * 	 						
 * Returns		:none.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static void
GX2LeaveVT(int scrnIndex, int flags)
{
   GeodeDebug(("GX2LeaveVT!\n"));
   GX2LeaveGraphics(xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX2FreeScreen.
 *
 * Description	:This is called to free any persistent data structures.
 *			
 * Parameters.
 *    scrnIndex :Specfies the screen index value.
 *     flags   	:Not used inside the function.
 * 	 						
 * Returns		:none.
 *
 * Comments     :This will be called only when screen being deleted..
*----------------------------------------------------------------------------
*/
static void
GX2FreeScreen(int scrnIndex, int flags)
{
   GeodeDebug(("GX2FreeScreen!\n"));
   if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
      vgaHWFreeHWRec(xf86Screens[scrnIndex]);
   GX2FreeRec(xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX2ValidMode.
 *
 * Description	:This function checks if a mode is suitable for selected
 *                   		chipset.
 * Parameters.
 *    scrnIndex :Specfies the screen index value.
 *     pMode	:Pointer to the screen mode structure..
 * 	 verbose    :not used for implementation.						
 *     flags    :not used for implementation
 *
 * Returns		:MODE_OK if the specified mode is supported or
 *                    		MODE_NO_INTERLACE.
 * Comments     :none.
*----------------------------------------------------------------------------
*/
static int
GX2ValidMode(int scrnIndex, DisplayModePtr pMode, Bool Verbose, int flags)
{
   unsigned int total_memory_required;
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   int ret = -1;
   GeodePtr pGeode = GX2GetRec(pScreenInfo);

   DEBUGMSG(1, (0, X_NONE, "GeodeValidateMode: %dx%d %d %d\n",
		pMode->CrtcHDisplay, pMode->CrtcVDisplay,
		pScreenInfo->bitsPerPixel, GX2GetRefreshRate(pMode)));
   {
      DEBUGMSG(1, (0, X_NONE, "CRT mode\n"));
      if (pMode->Flags & V_INTERLACE)
	 return MODE_NO_INTERLACE;

#if defined(STB_X)
      Gal_is_display_mode_supported(pMode->CrtcHDisplay, pMode->CrtcVDisplay,
				    pScreenInfo->bitsPerPixel,
				    GX2GetRefreshRate(pMode), &ret);
#else /* STB_X */
      ret = gfx_is_display_mode_supported(pMode->CrtcHDisplay,
					  pMode->CrtcVDisplay,
					  pScreenInfo->bitsPerPixel,
					  GX2GetRefreshRate(pMode));
#endif /* STB_X */
   }
   if (ret < 0)
      return MODE_NOMODE;

   total_memory_required = GX2CalculatePitchBytes(pMode->CrtcHDisplay,
						  pScreenInfo->bitsPerPixel) *
	 pMode->CrtcVDisplay;

   DEBUGMSG(1, (0, X_NONE, "Total Mem %X %X\n",
		total_memory_required, pGeode->FBSize));

   if (total_memory_required > pGeode->FBSize)
      return MODE_MEM;

   return MODE_OK;
}

/*----------------------------------------------------------------------------
 * GX2LoadPalette.
 *
 * Description	:This function sets the  palette entry used for graphics data
 *
 * Parameters.
 *   pScreenInfo:Points the screeninfo structure.
 *     numColors:Specifies the no of colors it supported.
 * 	 indizes    :This is used get index value .						
 *     LOCO     :to be added.
 *     pVisual  :to be added.
 *
 * Returns		:MODE_OK if the specified mode is supported or
 *          	 MODE_NO_INTERLACE.
 * Comments     :none.
*----------------------------------------------------------------------------
*/

static void
GX2LoadPalette(ScrnInfoPtr pScreenInfo,
	       int numColors, int *indizes, LOCO * colors, VisualPtr pVisual)
{
   int i, index, color;

   for (i = 0; i < numColors; i++) {
      index = indizes[i] & 0xFF;
      color = (((unsigned long)(colors[index].red & 0xFF)) << 16) |
	    (((unsigned long)(colors[index].green & 0xFF)) << 8) |
	    ((unsigned long)(colors[index].blue & 0xFF));
      DEBUGMSG(0, (0, X_NONE, "GX2LoadPalette: %d %d %X\n",
		   numColors, index, color));

      GFX(set_display_palette_entry(index, color));
   }
}

static Bool
GX2MapMem(ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

#if defined(STB_X)
   pGeode->FBBase = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						   VIDMEM_FRAMEBUFFER,
						   pGeode->FBLinearAddr,
						   pGeode->FBSize);

#else
   gfx_virt_regptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						    VIDMEM_MMIO,
						    (unsigned int)
						    gfx_get_cpu_register_base
						    (), pGeode->cpu_reg_size);

   if (pGeode->DetectedChipSet & GX2) {
      gfx_virt_gpptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						      VIDMEM_MMIO,
						      (unsigned int)
						      gfx_get_graphics_register_base
						      (),
						      pGeode->gp_reg_size);
   } else {
      gfx_virt_spptr = gfx_virt_regptr;
   }

   gfx_virt_vidptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						    VIDMEM_MMIO,
						    (unsigned int)
						    gfx_get_vid_register_base
						    (), pGeode->vid_reg_size);

   gfx_virt_fbptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						   VIDMEM_FRAMEBUFFER,
						   pGeode->FBLinearAddr,
						   pGeode->FBSize);

   pGeode->FBBase = gfx_virt_fbptr;

   DEBUGMSG(1, (0, X_NONE, "Set mode %X %X %X %X %X\n",
		gfx_virt_regptr,
		gfx_virt_gpptr,
		gfx_virt_spptr, gfx_virt_vidptr, gfx_virt_fbptr));

   /* CHECK IF REGISTERS WERE MAPPED SUCCESSFULLY */
   if ((!gfx_virt_regptr) ||
       (!gfx_virt_gpptr) || (!gfx_virt_vidptr) || (!gfx_virt_fbptr)) {
      DEBUGMSG(1, (0, X_NONE, "Could not map hardware registers.\n"));
      return (FALSE);
   }

   /* Map the XpressROM ptr to read what platform are we on */
   XpressROMPtr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						 VIDMEM_FRAMEBUFFER, 0xF0000,
						 0x10000);

   DEBUGMSG(1, (0, X_NONE, "adapter info %x %x %x %x, %X\n",
		pGeode->cpu_version,
		pGeode->vid_version,
		pGeode->FBSize, pGeode->FBBase, XpressROMPtr));
#endif

   return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
GX2UnmapMem(ScrnInfoPtr pScreenInfo)
{
#if !defined(STB_X)
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   /* unmap all the memory map's */
   xf86UnMapVidMem(pScreenInfo->scrnIndex,
		   gfx_virt_regptr, pGeode->cpu_reg_size);
   if (pGeode->DetectedChipSet & GX2) {
      xf86UnMapVidMem(pScreenInfo->scrnIndex,
		      gfx_virt_gpptr, pGeode->gp_reg_size);
   }
   xf86UnMapVidMem(pScreenInfo->scrnIndex,
		   gfx_virt_vidptr, pGeode->vid_reg_size);
   xf86UnMapVidMem(pScreenInfo->scrnIndex, gfx_virt_fbptr, pGeode->FBSize);
   xf86UnMapVidMem(pScreenInfo->scrnIndex, XpressROMPtr, 0x10000);
#endif /* STB_X */
   return TRUE;
}

/* End of file */
