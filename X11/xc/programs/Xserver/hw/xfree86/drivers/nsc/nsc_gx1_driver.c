/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx1_driver.c,v 1.11 2003/11/03 05:11:20 tsi Exp $ */
/*
 * $Workfile: nsc_gx1_driver.c $
 * $Revision: 1.1.1.2 $
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

/*
 * Lots of fixes & updates
 * Alan Hourihane <alanh@fairlite.demon.co.uk>
 */

#define DEBUG(x)
#define GEODE_TRACE 0
#define CFB 0
#define HWVGA 0

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

/* Our private include file (this also includes the durango headers) */
#include "nsc.h"

#if GEODE_TRACE
/* ANSI C does not allow var arg macros */
#define GeodeDebug(args) DebugPort(DCount++);ErrorF args
#else
#define GeodeDebug(args)
#endif

extern SymTabRec GeodeChipsets[];
extern OptionInfoRec GeodeOptions[];

typedef struct _MemOffset
{
   unsigned long xres;
   unsigned long yres;
   unsigned long bpp;
   unsigned long CBOffset;
   unsigned short CBPitch;
   unsigned short CBSize;
   unsigned long CurOffset;
   unsigned long OffScreenOffset;
}
MemOffset;

/* predefined memory address for compression and cursor offsets
 * if COMPRESSION enabled.
 */
MemOffset GeodeMemOffset[] = {
   {640, 480, 8, 640, 1024, 272, 0x78000, 0x78100},
   {640, 480, 16, 1280, 2048, 272, 0x610, 0xF0000},
   {800, 600, 8, 800, 1024, 208, 0x96000, 0x96100},
   {800, 600, 16, 1600, 2048, 272, 0x12C000, 0x12C100},
   {1024, 768, 8, 0xC0000, 272, 272, 0xF3000, 0xF3100},
   {1024, 768, 16, 0x180000, 272, 272, 0x1B3000, 0x1B3100},
   {1152, 864, 8, 1152, 2048, 272, 0x590, 0x1B0000},
   {1152, 864, 16, 2304, 4096, 272, 0xA10, 0x360000},
   {1280, 1024, 8, 1280, 2048, 272, 0x610, 0x200000},
   {1280, 1024, 16, 2560, 4096, 272, 0xB10, 0x400000},

   /* PAL TV modes */

   {704, 576, 16, 1408, 2048, 272, 0x690, 0x120000},
   {720, 576, 16, 1440, 2048, 272, 0x6B0, 0x120000},
   {768, 576, 16, 1536, 2048, 256, 0x700, 0x120000},

   /* NTSC TV modes */

   {704, 480, 16, 1408, 2048, 272, 0x690, 0xF0000},
   {720, 480, 16, 1440, 2048, 272, 0x6B0, 0xF0000}

};
static int MemIndex = 0;

static Bool GX1PreInit(ScrnInfoPtr, int);
static Bool GX1ScreenInit(int, ScreenPtr, int, char **);
static Bool GX1EnterVT(int, int);
static void GX1LeaveVT(int, int);
static void GX1FreeScreen(int, int);
void GX1AdjustFrame(int, int, int, int);
Bool GX1SwitchMode(int, DisplayModePtr, int);
static ModeStatus GX1ValidMode(int, DisplayModePtr, Bool, int);
static void GX1LoadPalette(ScrnInfoPtr pScreenInfo,
			   int numColors, int *indizes,
			   LOCO * colors, VisualPtr pVisual);
static Bool GX1MapMem(ScrnInfoPtr);
static Bool GX1UnmapMem(ScrnInfoPtr);

extern Bool GX1AccelInit(ScreenPtr pScreen);
extern Bool GX1HWCursorInit(ScreenPtr pScreen);
extern void GX1HideCursor(ScrnInfoPtr pScreenInfo);
extern void GX1ShowCursor(ScrnInfoPtr pScreenInfo);
extern void GX1PointerMoved(int index, int x, int y);
extern void GX1RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX1RefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX1RefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
extern void GX1InitVideo(ScreenPtr pScreen);
extern Bool GX1DGAInit(ScreenPtr pScreen);
extern void GX1LoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char *src);
extern unsigned int GetVideoMemSize(void);

void get_tv_overscan_geom(const char *options, int *X, int *Y, int *W,
			  int *H);
void GX1SetupChipsetFPtr(ScrnInfoPtr pScrn);
GeodePtr GX1GetRec(ScrnInfoPtr pScreenInfo);
void gx1_clear_screen(int width, int height);

#if !defined(STB_X)
extern unsigned char *XpressROMPtr;
#endif /* STB_X */

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

void
GX1SetupChipsetFPtr(ScrnInfoPtr pScrn)
{
   GeodeDebug(("GX1SetupChipsetFPtr!\n"));

   pScrn->PreInit = GX1PreInit;
   pScrn->ScreenInit = GX1ScreenInit;
   pScrn->SwitchMode = GX1SwitchMode;
   pScrn->AdjustFrame = GX1AdjustFrame;
   pScrn->EnterVT = GX1EnterVT;
   pScrn->LeaveVT = GX1LeaveVT;
   pScrn->FreeScreen = GX1FreeScreen;
   pScrn->ValidMode = GX1ValidMode;
}

/*----------------------------------------------------------------------------
 * GX1GetRec.
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
GX1GetRec(ScrnInfoPtr pScreenInfo)
{
   if (!pScreenInfo->driverPrivate)
      pScreenInfo->driverPrivate = xnfcalloc(sizeof(GeodeRec), 1);
   return GEODEPTR(pScreenInfo);
}

/*----------------------------------------------------------------------------
 * GX1FreeRec.
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
GX1FreeRec(ScrnInfoPtr pScreenInfo)
{
   if (pScreenInfo->driverPrivate == NULL) {
      return;
   }
   xfree(pScreenInfo->driverPrivate);
   pScreenInfo->driverPrivate = NULL;
}

/*----------------------------------------------------------------------------
 * GX1SaveScreen.
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
GX1SaveScreen(ScreenPtr pScreen, int mode)
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
 * get_tv_overscan_geom.
 *
 * Description	:This is todo the screen blanking
 *
 *    Parameters:
 *     options	: Pointer to the display options.
 *             X: Pointer to the offset of the screen X-co-ordinate.
 *             Y: Pointer to the offset of the screen Y-co-ordinate.
 * 	       W: Pointer to the width of the screen.
 *             H: Pointer to the height of the screen. 
 * Returns	: none.
 *
 * Comments     :none
 *------------------------------------------------------------------------
 */
void
get_tv_overscan_geom(const char *options, int *X, int *Y, int *W, int *H)
{
   char *tv_opt;

   tv_opt = strtok((char *)options, ":");
   *X = strtoul(tv_opt, NULL, 0);
   tv_opt = strtok(NULL, ":");
   *Y = strtoul(tv_opt, NULL, 0);
   tv_opt = strtok(NULL, ":");
   *W = strtoul(tv_opt, NULL, 0);
   tv_opt = strtok(NULL, ":");
   *H = strtoul(tv_opt, NULL, 0);
}

static void
GX1ProbeDDC(ScrnInfoPtr pScrn, int index)
{
   vbeInfoPtr pVbe;

   if (xf86LoadSubModule(pScrn, "vbe")) {
      pVbe = VBEInit(NULL, index);
      ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
      vbeFree(pVbe);
   }
}

/*----------------------------------------------------------------------------
 * GX1PreInit.
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
GX1PreInit(ScrnInfoPtr pScreenInfo, int flags)
{
   static ClockRange GeodeClockRange =
	 { NULL, 25175, 135000, 0, FALSE, TRUE, 1, 1, 0 };
   MessageType from;
   int i = 0;
   GeodePtr pGeode;
#if CFB
   char *mod = NULL;

   char *reqSymbol = NULL;
#endif

#if defined(STB_X)
   GAL_ADAPTERINFO sAdapterInfo;
#endif /* STB_X */
   unsigned int PitchInc = 0, minPitch = 0, maxPitch = 0;
   unsigned int minHeight = 0, maxHeight = 0;
   const char *s;
   char **modes;
   char **tvmodes_defa;

   GeodeDebug(("GX1PreInit!\n"));
   /* Allocate driver private structure */
   if (!(pGeode = GX1GetRec(pScreenInfo)))
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
      GX1ProbeDDC(pScreenInfo, pGeode->pEnt->index);
      return TRUE;
   }

   pGeode->FBVGAActive = 0;		/* KFB will Knock of VGA */

#if !defined(STB_X)
   /* If the vgahw module would be needed it would be loaded here */
   if (!xf86LoadSubModule(pScreenInfo, "vgahw")) {
      return FALSE;
   }

   xf86LoaderReqSymLists(nscVgahwSymbols, NULL);
#endif /* STB_X */
   GeodeDebug(("GX1PreInit(1)!\n"));

   /* Do the durango hardware detection */
#if defined(STB_X)
   if (!Gal_initialize_interface())
      return FALSE;
   if (Gal_get_adapter_info(&sAdapterInfo)) {
      pGeode->cpu_version = sAdapterInfo.dwCPUVersion;
      pGeode->vid_version = sAdapterInfo.dwVideoVersion;
      pGeode->FBSize = sAdapterInfo.dwFrameBufferSize;
      /* update the max clock from the one system suports  */
      GeodeClockRange.maxClock = sAdapterInfo.dwMaxSupportedPixelClock;
      pGeode->FBLinearAddr = sAdapterInfo.dwFrameBufferBase;

      if (!GX1MapMem(pScreenInfo))
	 return FALSE;

   } else {
      return FALSE;
   }
#else
   pGeode->cpu_version = gfx_detect_cpu();
   pGeode->vid_version = gfx_detect_video();
   pGeode->FBLinearAddr = gfx_get_frame_buffer_base();
   /* update the max clock from the one system suports  */
   GeodeClockRange.maxClock = gfx_get_max_supported_pixel_clock();

   if (!GX1MapMem(pScreenInfo))
      return FALSE;

   DEBUGMSG(1,
	    (0, X_INFO,
	     "Geode chip info: cpu:%x vid:%x size:%x base:%x, rom:%X\n",
	     pGeode->cpu_version, pGeode->vid_version, pGeode->FBSize,
	     pGeode->FBBase, XpressROMPtr));
#endif /* STB_X */

   /* Fill in the monitor field */
   pScreenInfo->monitor = pScreenInfo->confScreen->monitor;
   GeodeDebug(("GX1PreInit(2)!\n"));
   /* Determine depth, bpp, etc. */
   if (!xf86SetDepthBpp(pScreenInfo, 16, 0, 0, 0)) {
      return FALSE;

   } else {

      switch (pScreenInfo->depth) {
      case 8:
      case 16:
	 break;
      default:
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
      rgb zeros = { 0, 0, 0 };

      if (!xf86SetWeight(pScreenInfo, zeros, zeros)) {
	 return FALSE;
      } else {
	 /* XXX Check if the returned weight is supported */
      }
   }
   xf86PrintDepthBpp(pScreenInfo);
   GeodeDebug(("GX1PreInit(3)!\n"));
   if (!xf86SetDefaultVisual(pScreenInfo, -1))
      return FALSE;
   GeodeDebug(("GX1PreInit(4)!\n"));

   /* The new cmap layer needs this to be initialized */
   if (pScreenInfo->depth > 1) {
      Gamma zeros = { 0.0, 0.0, 0.0 };

      if (!xf86SetGamma(pScreenInfo, zeros)) {
	 return FALSE;
      }
   }
   GeodeDebug(("GX1PreInit(5)!\n"));
   /* We use a programmable clock */
   pScreenInfo->progClock = TRUE;

   /*Collect all of the relevant option flags
    * *(fill in pScreenInfo->options)
    */
   xf86CollectOptions(pScreenInfo, NULL);

   /*Process the options */
   xf86ProcessOptions(pScreenInfo->scrnIndex, pScreenInfo->options,
		      GeodeOptions);

   /*Set the bits per RGB for 8bpp mode */
   if (pScreenInfo->depth == 8) {
      /* Default to 8 */
      pScreenInfo->rgbBits = 8;
   }
   from = X_DEFAULT;
   pGeode->HWCursor = TRUE;
   /*
    * *The preferred method is to use the "hw cursor" option as a tri-state
    * *option, with the default set above.
    */
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
   DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "No of Buffers %d\n",
		pGeode->NoOfImgBuffers));

   pGeode->TVSupport = FALSE;

   pGeode->FBTVActive = 0;
   GFX(get_tv_enable(&(pGeode->FBTVActive)));
   DEBUGMSG(1, (1, X_PROBED, "FB TV %d \n", pGeode->FBTVActive));

   if ((s = xf86GetOptValString(GeodeOptions, OPTION_TV_SUPPORT))) {

      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG, "TV = %s\n", s));
      if (!xf86NameCmp(s, "PAL-768x576")) {
	 pGeode->TvParam.wStandard = TV_STANDARD_PAL;
	 pGeode->TvParam.wType = GFX_ON_TV_SQUARE_PIXELS;
	 pGeode->TvParam.wWidth = 768;
	 pGeode->TvParam.wHeight = 576;
	 pGeode->TVSupport = TRUE;
      } else if (!xf86NameCmp(s, "PAL-720x576")) {
	 pGeode->TvParam.wStandard = TV_STANDARD_PAL;
	 pGeode->TvParam.wType = GFX_ON_TV_NO_SCALING;
	 pGeode->TvParam.wWidth = 720;
	 pGeode->TvParam.wHeight = 576;
	 pGeode->TVSupport = TRUE;
      } else if (!xf86NameCmp(s, "NTSC-640x480")) {
	 pGeode->TvParam.wStandard = TV_STANDARD_NTSC;
	 pGeode->TvParam.wType = GFX_ON_TV_SQUARE_PIXELS;
	 pGeode->TvParam.wWidth = 640;
	 pGeode->TvParam.wHeight = 480;
	 pGeode->TVSupport = TRUE;
      } else if (!xf86NameCmp(s, "NTSC-720x480")) {
	 pGeode->TvParam.wStandard = TV_STANDARD_NTSC;
	 pGeode->TvParam.wType = GFX_ON_TV_NO_SCALING;
	 pGeode->TvParam.wWidth = 720;
	 pGeode->TvParam.wHeight = 480;
	 pGeode->TVSupport = TRUE;
      }

      if (pGeode->TVSupport == TRUE) {
	 pGeode->TvParam.wOutput = TV_OUTPUT_S_VIDEO;	/* default */

	 /* Now find the output */
	 if (pGeode->TVSupport) {
	    if ((s = xf86GetOptValString(GeodeOptions, OPTION_TV_OUTPUT))) {
	       if (!xf86NameCmp(s, "COMPOSITE")) {
		  pGeode->TvParam.wOutput = TV_OUTPUT_COMPOSITE;
	       } else if (!xf86NameCmp(s, "SVIDEO")) {
		  pGeode->TvParam.wOutput = TV_OUTPUT_S_VIDEO;
	       } else if (!xf86NameCmp(s, "YUV")) {
		  pGeode->TvParam.wOutput = TV_OUTPUT_YUV;
	       } else if (!xf86NameCmp(s, "SCART")) {
		  pGeode->TvParam.wOutput = TV_OUTPUT_SCART;
	       }
	       DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
			    "TVOutput = %s %d\n", s,
			    pGeode->TvParam.wOutput));
	    }
	 }
      }
/* Only SC1200 can support TV modes */
      if ((pGeode->vid_version != GFX_VID_SC1200)
	  && (pGeode->TVSupport == TRUE)) {
	 pGeode->TVSupport = FALSE;
      }

      /*TV can be turned on only in 16BPP mode */
      if ((pScreenInfo->depth == 8) && (pGeode->TVSupport == TRUE)) {
	 DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		      "Warning TV Disabled, TV Can't be supported in 8Bpp !!!\n"));
	 pGeode->TVSupport = FALSE;
      }
   }

   /* If TV Supported then check for TVO support */
   if (pGeode->TVSupport == TRUE) {
      pGeode->TVOx = 0;
      pGeode->TVOy = 0;
      pGeode->TVOw = 0;
      pGeode->TVOh = 0;
      pGeode->TV_Overscan_On = FALSE;
      if ((s = xf86GetOptValString(GeodeOptions, OPTION_TV_OVERSCAN))) {
	 get_tv_overscan_geom(s, &(pGeode->TVOx),
			      &(pGeode->TVOy), &(pGeode->TVOw),
			      &(pGeode->TVOh));

	 DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		      "TVO %d %d %d %d\n", pGeode->TVOx, pGeode->TVOy,
		      pGeode->TVOw, pGeode->TVOh));

	 if ((pGeode->TVOx >= 0 && pGeode->TVOy >= 0) &&
	     (pGeode->TVOh > 0 && pGeode->TVOw > 0)) {
	    if (((pGeode->TVOx + pGeode->TVOw) <= pGeode->TvParam.wWidth) &&
		((pGeode->TVOy + pGeode->TVOh) <= pGeode->TvParam.wHeight)) {
	       pGeode->TV_Overscan_On = TRUE;
	    }
	 }
      }
   }

   /* If TV is not selected and currently TV is enabled, disable the TV out */
   if (pGeode->TVSupport == FALSE) {
      unsigned int status = 0;

      GFX(get_tv_enable(&status));
      if (status)
	 GFX(set_tv_enable(0));
   }

   pGeode->Panel = FALSE;
   if (xf86ReturnOptValBool(GeodeOptions, OPTION_FLATPANEL, FALSE)) {
      DEBUGMSG(0, (pScreenInfo->scrnIndex, X_CONFIG, "FlatPanel Selected\n"));
      pGeode->Panel = TRUE;
   }

   DEBUGMSG(1, (pScreenInfo->scrnIndex, X_CONFIG,
		"Quering FP Bios %d\n", pGeode->Panel));

   /* if FP not supported in BIOS, then turn off user option */
   if (pGeode->Panel) {
      int ret;

      /* check if bios supports FP */
#if defined(STB_X)
      Gal_get_softvga_state(&ret);
      if (!ret) {
	 /* its time to wakeup softvga */
	 Gal_set_softvga_state(TRUE);
	 Gal_vga_mode_switch(0);
      }
      Gal_pnl_enabled_in_bios(&pGeode->Panel);

      if (pGeode->Panel) {
	 Gal_pnl_info_from_bios(&pGeode->FPBX, &pGeode->FPBY,
				&pGeode->FPBB, &pGeode->FPBF);
      }
      if (!ret) {
	 /* its time to put softvga back to sleep */
	 Gal_set_softvga_state(FALSE);
	 Gal_vga_mode_switch(1);
      }
#else
      ret = gfx_get_softvga_active();
      if (!ret) {
	 /* its time to wakeup softvga */
	 gfx_enable_softvga();
	 gfx_vga_mode_switch(0);
      }
      pGeode->Panel = Pnl_IsPanelEnabledInBIOS();
      if (pGeode->Panel) {
	 Pnl_GetPanelInfoFromBIOS(&pGeode->FPBX, &pGeode->FPBY,
				  &pGeode->FPBB, &pGeode->FPBF);
      }
      if (!ret) {
	 /* its time to put softvga back to sleep */
	 gfx_disable_softvga();
	 gfx_vga_mode_switch(1);
      }
#endif
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
#else
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

   /* Disable Rotation when TV Over Scan is enabled */
   if (pGeode->TV_Overscan_On)
      pGeode->Rotate = 0;

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
   GeodeDebug(("GX1PreInit(6)!\n"));

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

   GeodeDebug(("GX1PreInit(7)!\n"));

   /*
    * * xf86ValidateModes will check that the mode HTotal and VTotal values
    * * don't exceed the chipset's limit if pScreenInfo->maxHValue adn
    * * pScreenInfo->maxVValue are set. Since our GX1ValidMode()
    * * already takes care of this, we don't worry about setting them here.
    */
   /* Select valid modes from those available */
   /*
    * * min pitch 1024, max 2048 (Pixel count)
    * * min height 480, max 1024 (Pixel count)
    */
   minPitch = 1024;
   maxPitch = 2048;
   minHeight = 480;
   maxHeight = 1024;			/* Can support upto 1280x1024 16Bpp */
   if (pScreenInfo->depth == 16) {
      PitchInc = 2048;
   } else {
      PitchInc = 1024;
   }
   PitchInc <<= 3;			/* in bits */

   /* by default use what user sets in the XF86Config file */
   modes = pScreenInfo->display->modes;
   if (pGeode->TVSupport == TRUE) {
      char str[20];

      sprintf(str, "%dx%d-%d",
	      pGeode->TvParam.wWidth,
	      pGeode->TvParam.wHeight,
	      ((pGeode->TvParam.wStandard == TV_STANDARD_PAL) ? 50 : 60));

      tvmodes_defa = (char **)malloc(sizeof(char *) * 2);
      tvmodes_defa[0] = (char *)malloc(strlen(str));
      tvmodes_defa[1] = NULL;
      strcpy(str, tvmodes_defa[0]);

      modes = tvmodes_defa;
   }

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
#else
			 pGeode->FBSize,
#endif /* STB_X */
			 LOOKUP_BEST_REFRESH);

   DEBUGMSG(0, (pScreenInfo->scrnIndex, from,
		"xf86ValidateModes: %d %d %d\n",
		pScreenInfo->virtualX,
		pScreenInfo->virtualY, pScreenInfo->displayWidth));
   if (i == -1) {
      GX1FreeRec(pScreenInfo);
      return FALSE;
   }
   GeodeDebug(("GX1PreInit(8)!\n"));
   /* Prune the modes marked as invalid */
   xf86PruneDriverModes(pScreenInfo);
   GeodeDebug(("GX1PreInit(9)!\n"));
   if (i == 0 || pScreenInfo->modes == NULL) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "No valid modes found\n"));
      GX1FreeRec(pScreenInfo);
      return FALSE;
   }
   GeodeDebug(("GX1PreInit(10)!\n"));
   xf86SetCrtcForModes(pScreenInfo, 0);
   GeodeDebug(("GX1PreInit(11)!\n"));
   /* Set the current mode to the first in the list */
   pScreenInfo->currentMode = pScreenInfo->modes;
   GeodeDebug(("GX1PreInit(12)!\n"));
   /* Print the list of modes being used */
   xf86PrintModes(pScreenInfo);
   GeodeDebug(("GX1PreInit(13)!\n"));
   /* Set the display resolution */
   xf86SetDpi(pScreenInfo, 0, 0);
   GeodeDebug(("GX1PreInit(14)!\n"));

#if CFB
   /* Load bpp-specific modules */
   mod = NULL;

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
   default:
      return FALSE;
   }
   if (mod && xf86LoadSubModule(pScreenInfo, mod) == NULL) {
      GX1FreeRec(pScreenInfo);
      return FALSE;
   }

   xf86LoaderReqSymbols(reqSymbol, NULL);
#else
   if (xf86LoadSubModule(pScreenInfo, "fb") == NULL) {
      GX1FreeRec(pScreenInfo);
      return FALSE;
   }

   xf86LoaderReqSymLists(nscFbSymbols, NULL);
#endif
   GeodeDebug(("GX1PreInit(15)!\n"));
   if (pGeode->NoAccel == FALSE) {
      if (!xf86LoadSubModule(pScreenInfo, "xaa")) {
	 GX1FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscXaaSymbols, NULL);
   }
   GeodeDebug(("GX1PreInit(16)!\n"));
   if (pGeode->HWCursor == TRUE) {
      if (!xf86LoadSubModule(pScreenInfo, "ramdac")) {
	 GX1FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscRamdacSymbols, NULL);
   }
   GeodeDebug(("GX1PreInit(17)!\n"));
   /* Load shadowfb if needed */
   if (pGeode->ShadowFB) {
      if (!xf86LoadSubModule(pScreenInfo, "shadowfb")) {
	 GX1FreeRec(pScreenInfo);
	 return FALSE;
      }
      xf86LoaderReqSymLists(nscShadowSymbols, NULL);
   }
   GeodeDebug(("GX2PreInit(18)!\n"));
   if (xf86RegisterResources(pGeode->pEnt->index, NULL, ResExclusive)) {
      DEBUGMSG(1, (pScreenInfo->scrnIndex, X_ERROR,
		   "xf86RegisterResources() found resource conflicts\n"));
      GX1FreeRec(pScreenInfo);
      return FALSE;
   }
   GX1UnmapMem(pScreenInfo);
   GeodeDebug(("GX1PreInit(19)!\n"));
   GeodeDebug(("GX1PreInit(20)!\n"));
   GeodeDebug(("GX1PreInit ... done successfully!\n"));
   (void) from;
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX1RestoreEx.
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
GX1RestoreEx(ScrnInfoPtr pScreenInfo, DisplayModePtr pMode)
{
   GeodePtr pGeode;

   GeodeDebug(("GX1RestoreEx!\n"));
   /* Get driver private structure */
   if (!(pGeode = GX1GetRec(pScreenInfo)))
      return;
   /* Restore the extended registers */
#if defined(STB_X)
   pGeode->FBgfxVgaRegs.dwFlags = GFX_VGA_FLAG_MISC_OUTPUT |
	 GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC;
   Gal_vga_restore(&(pGeode->FBgfxVgaRegs));
#else
   gfx_vga_restore(&(pGeode->FBgfxVgaRegs),
		   GFX_VGA_FLAG_MISC_OUTPUT |
		   GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC);
#endif /* STB_X */
}

/*----------------------------------------------------------------------------
 * GX1CalculatePitchBytes.
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
GX1CalculatePitchBytes(unsigned int width, unsigned int bpp)
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
      if (lineDelta > 2048)
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
 * GX1GetRefreshRate.
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
GX1GetRefreshRate(DisplayModePtr pMode)
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
gx1_clear_screen(int width, int height)
{
   /* clean up the frame buffer memory */
   GFX(set_solid_pattern(0));
   GFX(set_raster_operation(0xF0));
   GFX(pattern_fill(0, 0, width, height));
}

/*----------------------------------------------------------------------------
 * GX1SetMode.
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
GX1SetMode(ScrnInfoPtr pScreenInfo, DisplayModePtr pMode)
{
   GeodePtr pGeode;

   /* unsigned int compOffset, compPitch, compSize; */
   GeodeDebug(("GX1SetMode!\n"));
   pGeode = GEODEPTR(pScreenInfo);
   /* Set the VT semaphore */
   pScreenInfo->vtSema = TRUE;
   DEBUGMSG(1, (0, X_NONE, "Set mode"));
   /* The timing will be adjusted later */
   GeodeDebug(("Set display mode: %dx%d-%d (%dHz)\n",
	       pMode->CrtcHDisplay,
	       pMode->CrtcVDisplay,
	       pScreenInfo->bitsPerPixel, GX1GetRefreshRate(pMode)));
   GeodeDebug(("Before setting the mode\n"));
   if ((pMode->CrtcHDisplay >= 640) && (pMode->CrtcVDisplay >= 480))

      GFX(set_display_bpp(pScreenInfo->bitsPerPixel));

   if (pGeode->TVSupport == TRUE) {
      pGeode->TvParam.bState = 1;	/* enable */
      /* wWidth and wHeight already set in init. */
#if defined(STB_X)
      Gal_tv_set_params(GAL_TVSTATE | GAL_TVOUTPUT |
			GAL_TVFORMAT | GAL_TVRESOLUTION, &(pGeode->TvParam));
#else
      /* sequence might be important */
      gfx_set_tv_display(pGeode->TvParam.wWidth, pGeode->TvParam.wHeight);
      gfx_set_tv_format((TVStandardType)pGeode->TvParam.wStandard,
			(GfxOnTVType)pGeode->TvParam.wType);
      gfx_set_tv_output(pGeode->TvParam.wOutput);
      gfx_set_tv_enable(pGeode->TvParam.bState);

#endif /* STB_X */
   } else {				/* TV not selected */

      DEBUGMSG(0, (0, X_PROBED, "Setting Display for CRT or TFT\n"));

      if (pGeode->Panel) {
	 DEBUGMSG(0, (0, X_PROBED, "Setting Display for TFT\n"));
	 DEBUGMSG(1, (0, X_PROBED, "Restore Panel %d %d %d %d %d\n",
		      pGeode->FPBX, pGeode->FPBY,
		      pMode->CrtcHDisplay,
		      pMode->CrtcVDisplay, pScreenInfo->bitsPerPixel));

	 GFX(set_fixed_timings(pGeode->FPBX, pGeode->FPBY,
			       pMode->CrtcHDisplay,
			       pMode->CrtcVDisplay,
			       pScreenInfo->bitsPerPixel));
      } else {
	 /* display is crt */
	 DEBUGMSG(0, (0, X_PROBED, "Setting Display for CRT\n"));
	 GFX(set_display_mode(pMode->CrtcHDisplay,
			      pMode->CrtcVDisplay,
			      pScreenInfo->bitsPerPixel,
			      GX1GetRefreshRate(pMode)));

	 /* adjust the pitch */
	 GFX(set_display_pitch(pGeode->Pitch));

      }
      /* enable crt */
      GFX(set_crt_enable(CRT_ENABLE));
   }

   GFX(set_display_offset(0L));
   GFX(wait_vertical_blank());

   /* enable compression if option selected */
   if (pGeode->Compression) {
      /* set the compression parameters,and it will be turned on later. */
#if defined(STB_X)
      Gal_set_compression_parameters(GAL_COMPRESSION_ALL,
				     pGeode->CBOffset,
				     pGeode->CBPitch, pGeode->CBSize);

      /* set the compression buffer, all parameters already set */
      Gal_set_compression_enable(GAL_COMPRESSION_ENABLE);
#else
      gfx_set_compression_offset(pGeode->CBOffset);
      gfx_set_compression_pitch(pGeode->CBPitch);
      gfx_set_compression_size(pGeode->CBSize);

      /* set the compression buffer, all parameters already set */
      gfx_set_compression_enable(1);
#endif /* STB_X */

   }
   if (pGeode->HWCursor) {
      /* Load blank cursor */
      GX1LoadCursorImage(pScreenInfo, NULL);
      GFX(set_cursor_position(pGeode->CursorStartOffset, 0, 0, 0, 0));
      GFX(set_cursor_enable(1));
   } else {
      GeodeDebug(("GX1RestoreEx ... "));
      GX1RestoreEx(pScreenInfo, pMode);
      GeodeDebug(("done.\n"));
   }

   GeodeDebug(("done.\n"));
   /* Reenable the hardware cursor after the mode switch */
   if (pGeode->HWCursor == TRUE) {
      GeodeDebug(("GX1ShowCursor ... "));
      GX1ShowCursor(pScreenInfo);
      GeodeDebug(("done.\n"));
   }
   /* Restore the contents in the screen info */
   GeodeDebug(("After setting the mode\n"));
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX1EnterGraphics.
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
GX1EnterGraphics(ScreenPtr pScreen, ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode;

#if !defined(STB_X)

   vgaHWPtr hwp = VGAHWPTR(pScreenInfo);

   vgaHWUnlock(hwp);
#endif

   GeodeDebug(("GX1EnterGraphics!\n"));

   DEBUGMSG(1, (0, X_NONE, "EnterGraphics\n"));
   /* Should we re-save the text mode on each VT enter? */
   pGeode = GX1GetRec(pScreenInfo);
#if 0
   print_gxm_gfx_reg(pGeode, 0x4C);
   print_gxm_vga_reg();
#endif
   /* This routine saves the current VGA state in Durango VGA structure */
#if defined(STB_X)
   Gal_get_softvga_state(&pGeode->FBVGAActive);
   pGeode->FBgfxVgaRegs.dwFlags = GFX_VGA_FLAG_MISC_OUTPUT |
	 GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC;
   Gal_vga_save(&(pGeode->FBgfxVgaRegs));
#else
   pGeode->FBVGAActive = gfx_get_softvga_active();
   gfx_vga_save(&(pGeode->FBgfxVgaRegs),
		GFX_VGA_FLAG_MISC_OUTPUT |
		GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC);
#endif /* STB_X */
   DEBUGMSG(1, (0, X_PROBED, "VSA = %d\n", pGeode->FBVGAActive));

#if !defined(STB_X)

   vgaHWSave(pScreenInfo, &VGAHWPTR(pScreenInfo)->SavedReg, VGA_SR_ALL);
#endif

#if defined(STB_X)
   Gal_get_display_timing(&pGeode->FBgfxdisplaytiming);
   Gal_tv_get_timings(0, &pGeode->FBgfxtvtiming);

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

   /* its time to put softvga to sleep */
   Gal_set_softvga_state(FALSE);
   Gal_vga_mode_switch(1);

#else
   /* Save TV State */
   gfx_get_tv_enable(&(pGeode->FBTVEnabled));
   if (pGeode->FBTVEnabled) {
      /* TV Format */
      pGeode->FBtvtiming.HorzTim = READ_VID32(SC1200_TVOUT_HORZ_TIM);
      pGeode->FBtvtiming.HorzSync = READ_VID32(SC1200_TVOUT_HORZ_SYNC);
      pGeode->FBtvtiming.VertSync = READ_VID32(SC1200_TVOUT_VERT_SYNC);
      pGeode->FBtvtiming.LineEnd = READ_VID32(SC1200_TVOUT_LINE_END);
      pGeode->FBtvtiming.VertDownscale =
	    READ_VID32(SC1200_TVOUT_VERT_DOWNSCALE);
      pGeode->FBtvtiming.HorzScaling = READ_VID32(SC1200_TVOUT_HORZ_SCALING);
      pGeode->FBtvtiming.TimCtrl1 = READ_VID32(SC1200_TVENC_TIM_CTRL_1);
      pGeode->FBtvtiming.TimCtrl2 = READ_VID32(SC1200_TVENC_TIM_CTRL_2);
      pGeode->FBtvtiming.Subfreq = READ_VID32(SC1200_TVENC_SUB_FREQ);
      pGeode->FBtvtiming.DispPos = READ_VID32(SC1200_TVENC_DISP_POS);
      pGeode->FBtvtiming.DispSize = READ_VID32(SC1200_TVENC_DISP_SIZE);
      /* TV Output */
      pGeode->FBtvtiming.TimCtrl2 = READ_VID32(SC1200_TVENC_TIM_CTRL_2);
      pGeode->FBtvtiming.Debug = READ_VID32(SC1200_TVOUT_DEBUG);
      /* TV Enable */
      pGeode->FBtvtiming.DacCtrl = READ_VID32(SC1200_TVENC_DAC_CONTROL);
   }

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

   /* its time to put softvga to sleep */
   gfx_disable_softvga();
   gfx_vga_mode_switch(1);

#endif /* STB_X */

   if (!GX1SetMode(pScreenInfo, pScreenInfo->currentMode)) {
      return FALSE;
   }
#if 1
   /* clear the frame buffer, for annoying noise during mode switch */
   gx1_clear_screen(pScreenInfo->currentMode->CrtcHDisplay,
		    pScreenInfo->currentMode->CrtcVDisplay);
#endif
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX1LeaveGraphics:
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
GX1LeaveGraphics(ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode;

   GeodeDebug(("GX1LeaveGraphics!\n"));
   pGeode = GEODEPTR(pScreenInfo);

   if (!pGeode->FBTVActive) {
      GFX(set_tv_enable(0));
   }
   /* clear the frame buffer, when leaving X */
   gx1_clear_screen(pScreenInfo->virtualX, pScreenInfo->virtualY);

#if defined(STB_X)
   Gal_set_display_timing(&pGeode->FBgfxdisplaytiming);
   Gal_tv_set_timings(0, &pGeode->FBgfxtvtiming);
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
#else
   /* Restore TV */
   if (pGeode->FBTVEnabled) {
      /* TV Format */
      WRITE_VID32(SC1200_TVOUT_HORZ_TIM, pGeode->FBtvtiming.HorzTim);
      WRITE_VID32(SC1200_TVOUT_HORZ_SYNC, pGeode->FBtvtiming.HorzSync);
      WRITE_VID32(SC1200_TVOUT_VERT_SYNC, pGeode->FBtvtiming.VertSync);
      WRITE_VID32(SC1200_TVOUT_LINE_END, pGeode->FBtvtiming.LineEnd);
      WRITE_VID32(SC1200_TVOUT_VERT_DOWNSCALE,
		  pGeode->FBtvtiming.VertDownscale);
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, pGeode->FBtvtiming.HorzScaling);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1, pGeode->FBtvtiming.TimCtrl1);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, pGeode->FBtvtiming.TimCtrl2);
      WRITE_VID32(SC1200_TVENC_SUB_FREQ, pGeode->FBtvtiming.Subfreq);
      WRITE_VID32(SC1200_TVENC_DISP_POS, pGeode->FBtvtiming.DispPos);
      WRITE_VID32(SC1200_TVENC_DISP_SIZE, pGeode->FBtvtiming.DispSize);
      /* TV Output */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, pGeode->FBtvtiming.TimCtrl2);
      /*  WRITE_VID32(SC1200_TVENC_DAC_CONTROL, tmp); */
      WRITE_VID32(SC1200_TVOUT_DEBUG, pGeode->FBtvtiming.Debug);
      /* TV Enable */
      WRITE_VID32(SC1200_TVENC_DAC_CONTROL, pGeode->FBtvtiming.DacCtrl);
   }

   /* Restore CRT */
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

   gfx_set_display_pitch(pGeode->FBgfxdisplaytiming.wPitch);

   gfx_set_display_offset(pGeode->FBDisplayOffset);

   /* Restore Cursor */
   gfx_set_cursor_position(pGeode->FBCursorOffset, 0, 0, 0, 0);

   /* Restore the previous Compression state */
   if (pGeode->FBCompressionEnable) {
      gfx_set_compression_offset(pGeode->FBCompressionOffset);
      gfx_set_compression_pitch(pGeode->FBCompressionPitch);
      gfx_set_compression_size(pGeode->FBCompressionSize);
      gfx_set_compression_enable(1);
   }
#endif /* STB_X */

   /* We need this to get back to vga mode when soft-vga
    * * kicks in.
    * * We actually need to examine the attribute Ctlr to find out
    * * what kinda crap (grafix or text) we need to enter in
    * * For now just lookout for 720x400
    */
#if 0
   if ((pGeode->FBgfxdisplaytiming.wHActive == 720) &&
       (pGeode->FBgfxdisplaytiming.wVActive == 400))
#else
   if (pGeode->FBVGAActive)
#endif
   {
      /* VSA was active before we started. Since we disabled it 
       * we got to enable it */
#if defined(STB_X)
      Gal_set_softvga_state(TRUE);
      Gal_vga_mode_switch(1);
      Gal_vga_clear_extended();
#else
      gfx_enable_softvga();
      gfx_vga_mode_switch(1);
      gfx_vga_clear_extended();
#endif /* STB_X */

#if !defined(STB_X)

      vgaHWRestore(pScreenInfo, &VGAHWPTR(pScreenInfo)->SavedReg, VGA_SR_ALL);
#endif

#if defined(STB_X)
      pGeode->FBgfxVgaRegs.dwFlags = GFX_VGA_FLAG_MISC_OUTPUT |
	    GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC;
      Gal_vga_restore(&(pGeode->FBgfxVgaRegs));
      Gal_vga_mode_switch(0);
#else
      gfx_vga_restore(&(pGeode->FBgfxVgaRegs),
		      GFX_VGA_FLAG_MISC_OUTPUT |
		      GFX_VGA_FLAG_STD_CRTC | GFX_VGA_FLAG_EXT_CRTC);
      gfx_vga_mode_switch(0);
#endif /* STB_X */
   }
#if 0
   print_gxm_gfx_reg(pGeode, 0x4C);
   print_gxm_vga_reg();
#endif
}

/*----------------------------------------------------------------------------
 * GX1CloseScreen.
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
GX1CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   DEBUGMSG(1, (scrnIndex, X_PROBED, "GX1CloseScreen\n"));
   GeodeDebug(("GX1CloseScreen!\n"));
   GX1LeaveGraphics(pScreenInfo);
   if (pGeode->AccelInfoRec)
      XAADestroyInfoRec(pGeode->AccelInfoRec);
   pScreenInfo->vtSema = FALSE;
   if (pGeode->DGAModes)
      xfree(pGeode->DGAModes);
   pGeode->DGAModes = 0;
   if (pGeode->ShadowPtr)
      xfree(pGeode->ShadowPtr);

   if (pGeode->AccelImageWriteBufferOffsets) {
      xfree(pGeode->AccelImageWriteBufferOffsets);
      pGeode->AccelImageWriteBufferOffsets = 0x0;
   }
   /* free the allocated off screen area */
   xf86FreeOffscreenArea(pGeode->AccelImgArea);
   xf86FreeOffscreenArea(pGeode->CompressionArea);

   GX1UnmapMem(pScreenInfo);

   pScreen->CloseScreen = pGeode->CloseScreen;
   return (*pScreen->CloseScreen) (scrnIndex, pScreen);

}

/*----------------------------------------------------------------------------
 * GX1DPMSet.
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
GX1DPMSSet(ScrnInfoPtr pScreenInfo, int mode, int flags)
{
   GeodePtr pGeode;

   pGeode = GEODEPTR(pScreenInfo);

   GeodeDebug(("GX1DPMSSet!\n"));

   /* Check if we are actively controlling the display */
   if (!pScreenInfo->vtSema) {
      ErrorF("GX1DPMSSet called when we not controlling the VT!\n");
      return;
   }
   switch (mode) {
   case DPMSModeOn:
      /* Screen: On; HSync: On; VSync: On */
      GFX(set_crt_enable(CRT_ENABLE));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerup();
#else
      if (pGeode->Panel)
	 Pnl_PowerUp();
#endif /* STB_X */
      if (pGeode->TVSupport)
	 GFX(set_tv_enable(1));
      break;

   case DPMSModeStandby:
      /* Screen: Off; HSync: Off; VSync: On */
      GFX(set_crt_enable(CRT_STANDBY));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      if (pGeode->TVSupport)
	 GFX(set_tv_enable(0));
      break;

   case DPMSModeSuspend:
      /* Screen: Off; HSync: On; VSync: Off */
      GFX(set_crt_enable(CRT_SUSPEND));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      if (pGeode->TVSupport)
	 GFX(set_tv_enable(0));
      break;
   case DPMSModeOff:
      /* Screen: Off; HSync: Off; VSync: Off */
      GFX(set_crt_enable(CRT_DISABLE));
#if defined(STB_X)
      if (pGeode->Panel)
	 Gal_pnl_powerdown();
#else
      if (pGeode->Panel)
	 Pnl_PowerDown();
#endif /* STB_X */
      if (pGeode->TVSupport)
	 GFX(set_tv_enable(0));
      break;
   }
}

/*----------------------------------------------------------------------------
 * GX1ScreenInit.
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
GX1ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
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

   GeodeDebug(("GX1ScreenInit!\n"));
   /* Get driver private */
   pGeode = GX1GetRec(pScreenInfo);
   GeodeDebug(("GX1ScreenInit(0)!\n"));

#if !defined(STB_X)
   /*
    * * Allocate a vgaHWRec
    */
   if (!vgaHWGetHWRec(pScreenInfo))
      return FALSE;
   VGAHWPTR(pScreenInfo)->MapSize = 0x10000;	/* Standard 64k VGA window */
   if (!vgaHWMapMem(pScreenInfo))
      return FALSE;
#endif

   if (!GX1MapMem(pScreenInfo))
      return FALSE;

#if !defined(STB_X)
   vgaHWGetIOBase(VGAHWPTR(pScreenInfo));
#endif

   pGeode->Pitch = GX1CalculatePitchBytes(pScreenInfo->virtualX,
					  pScreenInfo->bitsPerPixel);

   /* find the index to our operating mode the offsets are located */
   for (i = 0; i < (int)((sizeof(GeodeMemOffset) / sizeof(MemOffset))); i++) {
      if ((pScreenInfo->virtualX == (int)GeodeMemOffset[i].xres) &&
	  (pScreenInfo->virtualY == (int)GeodeMemOffset[i].yres) &&
	  (pScreenInfo->bitsPerPixel == (int)GeodeMemOffset[i].bpp)) {
	 MemIndex = i;
	 break;
      }
   }
   if (MemIndex == -1)			/* no match */
      return FALSE;

   /* SET UP GRAPHICS MEMORY AVAILABLE FOR PIXMAP CACHE */
   AvailBox.x1 = 0;
   AvailBox.y1 = pScreenInfo->virtualY;
   AvailBox.x2 = pScreenInfo->displayWidth;
   AvailBox.y2 = (pGeode->FBSize / pGeode->Pitch);

   pGeode->CursorSize = 8 * 32;		/* 32 DWORDS */

   if (pGeode->HWCursor) {
      /* Compute cursor buffer */
      /* Default cursor offset, end of the frame buffer */
      pGeode->CursorStartOffset = pGeode->FBSize - pGeode->CursorSize;
      AvailBox.y2 -= 1;
   }

   DEBUGMSG(1, (scrnIndex, X_PROBED,
		"Memory manager initialized to (%d,%d) (%d,%d) %d %d\n",
		AvailBox.x1, AvailBox.y1, AvailBox.x2, AvailBox.y2,
		pGeode->Pitch, pScreenInfo->displayWidth));
   /* set the offscreen offset accordingly */
   if (pGeode->Compression) {

      pGeode->CBOffset = GeodeMemOffset[MemIndex].CBOffset;
      pGeode->CBSize = GeodeMemOffset[MemIndex].CBSize - 16;
      pGeode->CBPitch = GeodeMemOffset[MemIndex].CBPitch;

      if ((pScreenInfo->virtualX == 1024) && (pScreenInfo->virtualY == 768)) {
	 req_offscreenmem = pScreenInfo->virtualY * pGeode->CBPitch;
	 req_offscreenmem += pGeode->Pitch - 1;
	 req_offscreenmem /= pGeode->Pitch;
	 pGeode->CBOffset = AvailBox.y1 * pGeode->Pitch;
	 AvailBox.y1 += req_offscreenmem;
      }
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
   if (!GX1EnterGraphics(pScreen, pScreenInfo))
      return FALSE;

   GX1AdjustFrame(scrnIndex, pScreenInfo->frameX0, pScreenInfo->frameY0, 0);
   GeodeDebug(("GX1ScreenInit(1)!\n"));

   /* Reset visual list */
   miClearVisualTypes();
   GeodeDebug(("GX1ScreenInit(2)!\n"));

   /* Setup the visual we support */
   if (pScreenInfo->bitsPerPixel > 8) {
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
   GeodeDebug(("GX1ScreenInit(3)!\n"));
   /* Set this for RENDER extension */
   miSetPixmapDepths();
   /* Call the framebuffer layer's ScreenInit function, and fill in other
    * * pScreen fields.
    */

   if (pGeode->TV_Overscan_On) {
      width = pGeode->TVOw;
      height = pGeode->TVOh;
      GeodeDebug(("width : %d , height : %d", width, height));
   } else {
      width = pScreenInfo->virtualX;
      height = pScreenInfo->virtualY;
   }

   displayWidth = pScreenInfo->displayWidth;
   if (pGeode->Rotate) {
      if (pGeode->TV_Overscan_On) {
	 width = pGeode->TVOh;
	 height = pGeode->TVOw;
      } else {
	 width = pScreenInfo->virtualY;
	 height = pScreenInfo->virtualX;
      }
   }
   if (pGeode->ShadowFB) {
      pGeode->ShadowPitch = BitmapBytePad(pScreenInfo->bitsPerPixel * width);
      pGeode->ShadowPtr = xalloc(pGeode->ShadowPitch * height);
      displayWidth = pGeode->ShadowPitch / (pScreenInfo->bitsPerPixel >> 3);
      FBStart = pGeode->ShadowPtr;
   } else {
      pGeode->ShadowPtr = NULL;

      if (pGeode->TV_Overscan_On) {
	 GeodeDebug(("pGeode->Pitch (%d)!\n", pGeode->Pitch));
	 FBStart = pGeode->FBBase + (pGeode->Pitch * pGeode->TVOy) +
	       (pGeode->TVOx << ((pScreenInfo->depth >> 3) - 1));
      } else {
	 FBStart = pGeode->FBBase;
      }
      DEBUGMSG(1, (0, X_PROBED, "FBStart %X \n", FBStart));
   }

   /* Initialise the framebuffer */
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

#else
   case 8:
   case 16:
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

   GeodeDebug(("GX1ScreenInit(4)!\n"));

   xf86SetBlackWhitePixels(pScreen);

   if (!pGeode->ShadowFB && (!pGeode->TV_Overscan_On)) {
      GX1DGAInit(pScreen);
   }
   GeodeDebug(("GX1ScreenInit(5)!\n"));
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

   GeodeDebug(("GX1ScreenInit(6)!\n"));
   if (!pGeode->NoAccel) {
      GX1AccelInit(pScreen);
   }
   GeodeDebug(("GX1ScreenInit(7)!\n"));
   miInitializeBackingStore(pScreen);
   xf86SetBackingStore(pScreen);

   GeodeDebug(("GX1ScreenInit(8)!\n"));
   /* Initialise software cursor */
   miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
   /* Initialize HW cursor layer.
    * * Must follow software cursor initialization
    */
   if (pGeode->HWCursor) {
      if (!GX1HWCursorInit(pScreen))
	 xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
		    "Hardware cursor initialization failed\n");
   }
   GeodeDebug(("GX1ScreenInit(9)!\n"));

   /* Setup default colourmap */
   if (!miCreateDefColormap(pScreen)) {
      return FALSE;
   }

   GeodeDebug(("GX1ScreenInit(10)!\n"));
   /* Initialize colormap layer.
    * * Must follow initialization of the default colormap
    */
   if (!xf86HandleColormaps(pScreen, 256, 8,
			    GX1LoadPalette, NULL,
			    CMAP_PALETTED_TRUECOLOR |
			    CMAP_RELOAD_ON_MODE_SWITCH)) {
      return FALSE;
   }
   GeodeDebug(("GX1ScreenInit(11)!\n"));

   if (pGeode->ShadowFB) {
      RefreshAreaFuncPtr refreshArea = GX1RefreshArea;

      if (pGeode->Rotate) {
	 if (!pGeode->PointerMoved) {
	    pGeode->PointerMoved = pScreenInfo->PointerMoved;
	    pScreenInfo->PointerMoved = GX1PointerMoved;
	 }
	 switch (pScreenInfo->bitsPerPixel) {
	 case 8:
	    refreshArea = GX1RefreshArea8;
	    break;
	 case 16:
	    refreshArea = GX1RefreshArea16;
	    break;
	 }
      }
      ShadowFBInit(pScreen, refreshArea);
   }
   xf86DPMSInit(pScreen, GX1DPMSSet, 0);
   GeodeDebug(("GX1ScreenInit(12)!\n"));

   if (pGeode->TV_Overscan_On) {
      GeodeDebug(("pGeode->Pitch (%d)!\n", pGeode->Pitch));

      pScreenInfo->memPhysBase = (unsigned long)(pGeode->FBBase +
						 (pGeode->Pitch *
						  pGeode->TVOy) +
						 (pGeode->
						  TVOx <<
						  ((pScreenInfo->depth >> 3) -
						   1)));
      GeodeDebug(("->memPhysBase (%X)!\n", pScreenInfo->memPhysBase));
   } else {
      pScreenInfo->memPhysBase = (unsigned long)pGeode->FBBase;
   }
   pScreenInfo->fbOffset = 0;

   GeodeDebug(("GX1ScreenInit(13)!\n"));
   GX1InitVideo(pScreen);		/* needed for video */
   /* Wrap the screen's CloseScreen vector and set its
    * SaveScreen vector 
    */
   pGeode->CloseScreen = pScreen->CloseScreen;
   pScreen->CloseScreen = GX1CloseScreen;

   pScreen->SaveScreen = GX1SaveScreen;
   GeodeDebug(("GX1ScreenInit(14)!\n"));
   /* Report any unused options */
   if (serverGeneration == 1) {
      xf86ShowUnusedOptions(pScreenInfo->scrnIndex, pScreenInfo->options);
   }
   GeodeDebug(("GX2ScreenInit(15)!\n"));
   return TRUE;
}

/*----------------------------------------------------------------------------
 * GX1SwitchMode.
 *
 * Description	:This function will switches the screen mode
 *   			    				
 * Parameters:
 *    scrnIndex	:Specfies the screen index value.
 *    pMode		:pointer to the mode structure.
 * 	flags       :may be used for status check?.
 *	  												
 * Returns		:Returns TRUE on success and FALSE on failure.
 *
 * Comments     :none.
*----------------------------------------------------------------------------
*/
Bool
GX1SwitchMode(int scrnIndex, DisplayModePtr pMode, int flags)
{
   GeodeDebug(("GX1SwitchMode!\n"));
   return GX1SetMode(xf86Screens[scrnIndex], pMode);
}

/*----------------------------------------------------------------------------
 * GX1AdjustFrame.
 *
 * Description	:This function is used to intiallize the start
 *				 address of the memory.
 * Parameters.
 *    scrnIndex	:Specfies the screen index value.
 *     x     	:x co-ordinate value interms of pixels.
 * 	 y          :y co-ordinate value interms of pixels.
 *	  												
 * Returns		:none.
 *
 * Comments    	:none.
*----------------------------------------------------------------------------
*/
void
GX1AdjustFrame(int scrnIndex, int x, int y, int flags)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   GeodePtr pGeode;
   unsigned long offset;

   pGeode = GX1GetRec(pScreenInfo);
   offset = (unsigned long)y *(unsigned long)pGeode->Pitch;

   offset += x;
   if (pScreenInfo->bitsPerPixel > 8) {
      offset += x;
   }
   GFX(set_display_offset(offset));
}

/*----------------------------------------------------------------------------
 * GX1EnterVT.
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
GX1EnterVT(int scrnIndex, int flags)
{
   GeodeDebug(("GX1EnterVT!\n"));
   return GX1EnterGraphics(NULL, xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX1LeaveVT.
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
GX1LeaveVT(int scrnIndex, int flags)
{
   GeodeDebug(("GX1LeaveVT!\n"));
   GX1LeaveGraphics(xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX1FreeScreen.
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
GX1FreeScreen(int scrnIndex, int flags)
{
   GeodeDebug(("GX1FreeScreen!\n"));
#if !defined(STB_X)

   if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
      vgaHWFreeHWRec(xf86Screens[scrnIndex]);
#endif
   GX1FreeRec(xf86Screens[scrnIndex]);
}

/*----------------------------------------------------------------------------
 * GX1ValidMode.
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
static ModeStatus
GX1ValidMode(int scrnIndex, DisplayModePtr pMode, Bool Verbose, int flags)
{
   ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
   unsigned int total_memory_required;
   int ret = -1;
   GeodePtr pGeode = GX1GetRec(pScreenInfo);

   DEBUGMSG(1, (0, X_NONE, "GeodeValidateMode: %dx%d %d %d\n",
		pMode->CrtcHDisplay, pMode->CrtcVDisplay,
		pScreenInfo->bitsPerPixel, GX1GetRefreshRate(pMode)));

   if (pGeode->TVSupport == TRUE) {
      if ((pGeode->TvParam.wWidth == pMode->CrtcHDisplay) &&
	  (pGeode->TvParam.wHeight == pMode->CrtcVDisplay)) {
	 DEBUGMSG(1, (0, X_NONE, "TV mode\n"));

#if defined(STB_X)
	 Gal_is_tv_mode_supported(0, &(pGeode->TvParam), &ret);
#else
	 ret = gfx_is_tv_display_mode_supported(pMode->CrtcHDisplay,
						pMode->CrtcVDisplay,
			(TVStandardType)pGeode->TvParam.wStandard);
#endif
      }
   } else {
      DEBUGMSG(1, (0, X_NONE, "CRT mode\n"));

      if (pMode->Flags & V_INTERLACE)
	 return MODE_NO_INTERLACE;

#if defined(STB_X)
      Gal_is_display_mode_supported(pMode->CrtcHDisplay, pMode->CrtcVDisplay,
				    pScreenInfo->bitsPerPixel,
				    GX1GetRefreshRate(pMode), &ret);
#else
      ret = gfx_is_display_mode_supported(pMode->CrtcHDisplay,
					  pMode->CrtcVDisplay,
					  pScreenInfo->bitsPerPixel,
					  GX1GetRefreshRate(pMode));
#endif /* STB_X */
   }

   if (ret < 0)
      return MODE_NOMODE;

   total_memory_required = GX1CalculatePitchBytes(pMode->CrtcHDisplay,
						  pScreenInfo->bitsPerPixel) *
	 pMode->CrtcVDisplay;

   DEBUGMSG(0, (0, X_NONE, "Total Mem %X %X",
		total_memory_required, pGeode->FBSize));

   if (total_memory_required > pGeode->FBSize)
      return MODE_MEM;

   return MODE_OK;
}

/*----------------------------------------------------------------------------
 * GX1LoadPalette.
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
GX1LoadPalette(ScrnInfoPtr pScreenInfo,
	       int numColors, int *indizes, LOCO * colors, VisualPtr pVisual)
{
   int i, index, color;

   for (i = 0; i < numColors; i++) {
      index = indizes[i] & 0xFF;
      color = (((unsigned long)(colors[index].red & 0xFF)) << 16) |
	    (((unsigned long)(colors[index].green & 0xFF)) << 8) |
	    ((unsigned long)(colors[index].blue & 0xFF));
      DEBUGMSG(0, (0, X_NONE, "GX1LoadPalette: %d %d %X\n",
		   numColors, index, color));

      GFX(set_display_palette_entry(index, color));
   }
}

static Bool
GX1MapMem(ScrnInfoPtr pScreenInfo)
{
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

#if defined(STB_X)
   pGeode->FBBase = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						   VIDMEM_FRAMEBUFFER,
						   pGeode->FBLinearAddr,
						   pGeode->FBSize);

#else
   /* SET DURANGO REGISTER POINTERS
    * * The method of mapping from a physical address to a linear address
    * * is operating system independent.  Set variables to linear address.
    */
   gfx_virt_regptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						    VIDMEM_MMIO,
						    (unsigned int)
						    gfx_get_cpu_register_base
						    (), 0x9000);
   gfx_virt_spptr = gfx_virt_regptr;
   gfx_virt_vidptr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						    VIDMEM_MMIO,
						    (unsigned int)
						    gfx_get_vid_register_base
						    (), 0x1000);
   pGeode->FBSize = GetVideoMemSize();
   gfx_virt_fbptr =
	 (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
					VIDMEM_FRAMEBUFFER,
					pGeode->FBLinearAddr, pGeode->FBSize);

   /* CHECK IF REGISTERS WERE MAPPED SUCCESSFULLY */
   if ((!gfx_virt_regptr) ||
       (!gfx_virt_spptr) || (!gfx_virt_vidptr) || (!gfx_virt_fbptr)) {
      DEBUGMSG(1, (0, X_NONE, "Could not map hardware registers.\n"));
      return (FALSE);
   }

   /* Map the XpressROM ptr to read what platform are we on */
   XpressROMPtr = (unsigned char *)xf86MapVidMem(pScreenInfo->scrnIndex,
						 VIDMEM_FRAMEBUFFER, 0xF0000,
						 0x10000);

   if (!XpressROMPtr)
      return FALSE;

   pGeode->FBBase = gfx_virt_fbptr;
#endif

   return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
GX1UnmapMem(ScrnInfoPtr pScreenInfo)
{
#if !defined(STB_X)
   GeodePtr pGeode = GEODEPTR(pScreenInfo);

   /* unmap all the memory map's */
   xf86UnMapVidMem(pScreenInfo->scrnIndex, gfx_virt_regptr, 0x9000);
   xf86UnMapVidMem(pScreenInfo->scrnIndex, gfx_virt_vidptr, 0x1000);
   xf86UnMapVidMem(pScreenInfo->scrnIndex, gfx_virt_fbptr, pGeode->FBSize);
   xf86UnMapVidMem(pScreenInfo->scrnIndex, XpressROMPtr, 0x10000);
#endif /* STB_X */

   return TRUE;
}

/* End of file */
