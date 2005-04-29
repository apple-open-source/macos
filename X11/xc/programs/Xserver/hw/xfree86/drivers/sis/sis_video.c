/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_video.c,v 1.50 2004/02/25 17:45:14 twini Exp $ */
/*
 * Xv driver for SiS 300, 315 and 330 series.
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author:    Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Formerly based on a mostly non-working code fragment for the 630 by
 * Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan which is
 * Copyright (C) 2000 Silicon Integrated Systems Corp, Inc.
 *
 * Basic structure based on the mga Xv driver by Mark Vojkovich
 * and i810 Xv driver by Jonathan Bian <jonathan.bian@intel.com>.
 *
 * All comments in this file are by Thomas Winischhofer.
 *
 * This supports the following chipsets:
 *  SiS300: No registers >0x65, two overlays (one used for CRT1, one for CRT2)
 *  SiS630/730: No registers >0x6b, two overlays (one used for CRT1, one for CRT2)
 *  SiS550: Full register range, two overlays (one used for CRT1, one for CRT2)
 *  SiS315: Full register range, one overlay (used for both CRT1 and CRT2 alt.)
 *  SiS650/740: Full register range, one overlay (used for both CRT1 and CRT2 alt.)
 *  SiSM650/651: Full register range, two overlays (one used for CRT1, one for CRT2)
 *  SiS330: Full register range, one overlay (used for both CRT1 and CRT2 alt.)
 *  SiS661/741/760: Full register range, two overlays (one used for CRT1, one for CRT2)
 *
 * Help for reading the code:
 * 315/550/650/740/M650/651/330/661/741/760 = SIS_315_VGA
 * 300/630/730                              = SIS_300_VGA
 * For chipsets with 2 overlays, hasTwoOverlays will be true
 *
 * Notes on display modes:
 *
 * -) dual head mode:
 *    DISPMODE is either SINGLE1 or SINGLE2, hence you need to check dualHeadMode flag
 *    DISPMODE is _never_ MIRROR.
 *    a) Chipsets with 2 overlays:
 *       315/330 series: Only half sized overlays available (width 960), 660: 1536
 *       Overlay 1 is used on CRT1, overlay 2 for CRT2.
 *    b) Chipsets with 1 overlay:
 *       Full size overlays available.
 *       Overlay is used for either CRT1 or CRT2
 * -) merged fb mode:
 *    a) Chipsets with 2 overlays:
 *       315/330 series: Only half sized overlays available (width 960), 660: 1536
 *       DISPMODE is always MIRROR. Overlay 1 is used for CRT1, overlay 2 for CRT2.
 *    b) Chipsets with 1 overlay:
 *       Full size overlays available.
 *       DISPMODE is either SINGLE1 or SINGLE2. Overlay is used accordingly on either
 *       CRT1 or CRT2 (automatically, where it is located)
 * -) mirror mode (without dualhead or mergedfb)
 *    a) Chipsets with 2 overlays:
 *       315/330 series: Only half sized overlays available (width 960), 660: 1536
 *       DISPMODE is MIRROR. Overlay 1 is used for CRT1, overlay 2 for CRT2.
 *    b) Chipsets with 1 overlay:
 *       Full size overlays available.
 *       DISPMODE is either SINGLE1 or SINGLE2. Overlay is used depending on
 * 	 XvOnCRT2 flag.
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "sis.h"
#include "xf86xv.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#include "sis_regs.h"

static 		XF86VideoAdaptorPtr SISSetupImageVideo(ScreenPtr);
static void 	SISStopVideo(ScrnInfoPtr, pointer, Bool);
static int 	SISSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int 	SISGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void 	SISQueryBestSize(ScrnInfoPtr, Bool, short, short, short,
			short, unsigned int *,unsigned int *, pointer);
static int 	SISPutImage( ScrnInfoPtr,
    			short, short, short, short, short, short, short, short,
    			int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int 	SISQueryImageAttributes(ScrnInfoPtr,
    			int, unsigned short *, unsigned short *, int *, int *);
static void 	SISVideoTimerCallback(ScrnInfoPtr pScrn, Time now);
static void     SISInitOffscreenImages(ScreenPtr pScrn);
extern BOOLEAN  SiSBridgeIsInSlaveMode(ScrnInfoPtr pScrn);

#define OFF_DELAY   	200  /* milliseconds */
#define FREE_DELAY  	60000

#define OFF_TIMER   	0x01
#define FREE_TIMER  	0x02
#define CLIENT_VIDEO_ON 0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#define WATCHDOG_DELAY  500000 /* Watchdog counter for Vertical Restrace waiting */

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

#define IMAGE_MIN_WIDTH         32  	/* Minimum and maximum source image sizes */
#define IMAGE_MIN_HEIGHT        24
#define IMAGE_MAX_WIDTH_300    720
#define IMAGE_MAX_HEIGHT_300   576
#define IMAGE_MAX_WIDTH_315   1920
#define IMAGE_MAX_HEIGHT_315  1080

#define OVERLAY_MIN_WIDTH       32  	/* Minimum overlay sizes */
#define OVERLAY_MIN_HEIGHT      24

#define DISPMODE_SINGLE1 0x1  		/* CRT1 only */
#define DISPMODE_SINGLE2 0x2  		/* CRT2 only */
#define DISPMODE_MIRROR  0x4  		/* CRT1 + CRT2 MIRROR (see note below) */

#define LINEBUFLIMIT1    384		/* Limits at which line buffers must be merged */
#define LINEBUFLIMIT2    720
#define LINEBUFLIMIT3    576

#ifdef SISDUALHEAD
#define HEADOFFSET (pSiS->dhmOffset)
#endif

#define GET_PORT_PRIVATE(pScrn) \
   (SISPortPrivPtr)((SISPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

/* Note on "MIRROR":
 * When using VESA on machines with an enabled video bridge, this means
 * a real mirror. CRT1 and CRT2 have the exact same resolution and
 * refresh rate. The same applies to modes which require the bridge to
 * operate in slave mode.
 * When not using VESA and the bridge is not in slave mode otherwise,
 * CRT1 and CRT2 have the same resolution but possibly a different
 * refresh rate.
 */

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{
   0,
   "XV_IMAGE",
   0, 0,		/* Will be filled in */
   {1, 1}
};

#define NUM_FORMATS 3

static XF86VideoFormatRec SISFormats[NUM_FORMATS] =
{
   { 8, PseudoColor},
   {16, TrueColor},
   {24, TrueColor}
};

static char sisxvcolorkey[] 				= "XV_COLORKEY";
static char sisxvbrightness[] 				= "XV_BRIGHTNESS";
static char sisxvcontrast[] 				= "XV_CONTRAST";
static char sisxvsaturation[] 				= "XV_SATURATION";
static char sisxvhue[] 					= "XV_HUE";
static char sisxvautopaintcolorkey[] 			= "XV_AUTOPAINT_COLORKEY";
static char sisxvsetdefaults[] 				= "XV_SET_DEFAULTS";
static char sisxvswitchcrt[] 				= "XV_SWITCHCRT";
static char sisxvtvxposition[] 				= "XV_TVXPOSITION";
static char sisxvtvyposition[] 				= "XV_TVYPOSITION";
static char sisxvgammared[] 				= "XV_GAMMA_RED";
static char sisxvgammagreen[] 				= "XV_GAMMA_GREEN";
static char sisxvgammablue[] 				= "XV_GAMMA_BLUE";
static char sisxvdisablegfx[] 				= "XV_DISABLE_GRAPHICS";
static char sisxvdisablegfxlr[] 			= "XV_DISABLE_GRAPHICS_LR";
static char sisxvdisablecolorkey[] 			= "XV_DISABLE_COLORKEY";
static char sisxvusechromakey[] 			= "XV_USE_CHROMAKEY";
static char sisxvinsidechromakey[] 			= "XV_INSIDE_CHROMAKEY";
static char sisxvyuvchromakey[] 			= "XV_YUV_CHROMAKEY";
static char sisxvchromamin[] 				= "XV_CHROMAMIN";
static char sisxvchromamax[] 				= "XV_CHROMAMAX";
static char sisxvqueryvbflags[] 			= "XV_QUERYVBFLAGS";
static char sisxvsdgetdriverversion[] 			= "XV_SD_GETDRIVERVERSION";
static char sisxvsdgethardwareinfo[]			= "XV_SD_GETHARDWAREINFO";
static char sisxvsdgetbusid[] 				= "XV_SD_GETBUSID";
static char sisxvsdqueryvbflagsversion[] 		= "XV_SD_QUERYVBFLAGSVERSION";
static char sisxvsdgetsdflags[] 			= "XV_SD_GETSDFLAGS";
static char sisxvsdunlocksisdirect[] 			= "XV_SD_UNLOCKSISDIRECT";
static char sisxvsdsetvbflags[] 			= "XV_SD_SETVBFLAGS";
static char sisxvsdquerydetecteddevices[] 		= "XV_SD_QUERYDETECTEDDEVICES";
static char sisxvsdcrt1status[] 			= "XV_SD_CRT1STATUS";
static char sisxvsdcheckmodeindexforcrt2[] 		= "XV_SD_CHECKMODEINDEXFORCRT2";
static char sisxvsdresultcheckmodeindexforcrt2[] 	= "XV_SD_RESULTCHECKMODEINDEXFORCRT2";
static char sisxvsdsisantiflicker[] 			= "XV_SD_SISANTIFLICKER";
static char sisxvsdsissaturation[] 			= "XV_SD_SISSATURATION";
static char sisxvsdsisedgeenhance[] 			= "XV_SD_SISEDGEENHANCE";
static char sisxvsdsiscolcalibf[] 			= "XV_SD_SISCOLCALIBF";
static char sisxvsdsiscolcalibc[] 			= "XV_SD_SISCOLCALIBC";
static char sisxvsdsiscfilter[] 			= "XV_SD_SISCFILTER";
static char sisxvsdsisyfilter[] 			= "XV_SD_SISYFILTER";
static char sisxvsdchcontrast[] 			= "XV_SD_CHCONTRAST";
static char sisxvsdchtextenhance[] 			= "XV_SD_CHTEXTENHANCE";
static char sisxvsdchchromaflickerfilter[] 		= "XV_SD_CHCHROMAFLICKERFILTER";
static char sisxvsdchlumaflickerfilter[] 		= "XV_SD_CHLUMAFLICKERFILTER";
static char sisxvsdchcvbscolor[] 			= "XV_SD_CHCVBSCOLOR";
static char sisxvsdchoverscan[]				= "XV_SD_CHOVERSCAN";
static char sisxvsdenablegamma[]			= "XV_SD_ENABLEGAMMA";
static char sisxvsdtvxscale[] 				= "XV_SD_TVXSCALE";
static char sisxvsdtvyscale[] 				= "XV_SD_TVYSCALE";
static char sisxvsdgetscreensize[] 			= "XV_SD_GETSCREENSIZE";
static char sisxvsdstorebrir[] 				= "XV_SD_STOREDGAMMABRIR";
static char sisxvsdstorebrig[] 				= "XV_SD_STOREDGAMMABRIG";
static char sisxvsdstorebrib[] 				= "XV_SD_STOREDGAMMABRIB";
static char sisxvsdstorepbrir[] 			= "XV_SD_STOREDGAMMAPBRIR";
static char sisxvsdstorepbrig[] 			= "XV_SD_STOREDGAMMAPBRIG";
static char sisxvsdstorepbrib[] 			= "XV_SD_STOREDGAMMAPBRIB";
static char sisxvsdstorebrir2[]				= "XV_SD_STOREDGAMMABRIR2";
static char sisxvsdstorebrig2[]				= "XV_SD_STOREDGAMMABRIG2";
static char sisxvsdstorebrib2[]				= "XV_SD_STOREDGAMMABRIB2";
static char sisxvsdstorepbrir2[] 			= "XV_SD_STOREDGAMMAPBRIR2";
static char sisxvsdstorepbrig2[] 			= "XV_SD_STOREDGAMMAPBRIG2";
static char sisxvsdstorepbrib2[] 			= "XV_SD_STOREDGAMMAPBRIB2";
static char sisxvsdhidehwcursor[] 			= "XV_SD_HIDEHWCURSOR";
static char sisxvsdpanelmode[] 				= "XV_SD_PANELMODE";
#ifdef TWDEBUG
static char sisxvsetreg[]				= "XV_SD_SETREG";
#endif

#ifndef SIS_CP
#define NUM_ATTRIBUTES_300 57
#ifdef TWDEBUG
#define NUM_ATTRIBUTES_315 64
#else
#define NUM_ATTRIBUTES_315 63
#endif
#endif

static XF86AttributeRec SISAttributes_300[NUM_ATTRIBUTES_300] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvcolorkey},
   {XvSettable | XvGettable, -128, 127,        sisxvbrightness},
   {XvSettable | XvGettable, 0, 7,             sisxvcontrast},
   {XvSettable | XvGettable, 0, 1,             sisxvautopaintcolorkey},
   {XvSettable             , 0, 0,             sisxvsetdefaults},
   {XvSettable | XvGettable, -32, 32,          sisxvtvxposition},
   {XvSettable | XvGettable, -32, 32,          sisxvtvyposition},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablegfx},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablegfxlr},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablecolorkey},
   {XvSettable | XvGettable, 0, 1,             sisxvusechromakey},
   {XvSettable | XvGettable, 0, 1,             sisxvinsidechromakey},
   {XvSettable | XvGettable, 0, 1,             sisxvyuvchromakey},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvchromamin},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvchromamax},
   {             XvGettable, 0, 0xffffffff,    sisxvqueryvbflags},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetdriverversion},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgethardwareinfo},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetbusid},
   {             XvGettable, 0, 0xffffffff,    sisxvsdqueryvbflagsversion},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetsdflags},
   {XvSettable | XvGettable, 0, 0xffffffff,    sisxvsdunlocksisdirect},
   {XvSettable             , 0, 0xffffffff,    sisxvsdsetvbflags},
   {             XvGettable, 0, 0xffffffff,    sisxvsdquerydetecteddevices},
   {XvSettable | XvGettable, 0, 1,    	       sisxvsdcrt1status},
   {XvSettable             , 0, 0xffffffff,    sisxvsdcheckmodeindexforcrt2},
   {             XvGettable, 0, 0xffffffff,    sisxvsdresultcheckmodeindexforcrt2},
   {XvSettable | XvGettable, 0, 4,             sisxvsdsisantiflicker},
   {XvSettable | XvGettable, 0, 15,            sisxvsdsissaturation},
   {XvSettable | XvGettable, 0, 15,            sisxvsdsisedgeenhance},
   {XvSettable | XvGettable, -128, 127,        sisxvsdsiscolcalibf},
   {XvSettable | XvGettable, -120, 120,        sisxvsdsiscolcalibc},
   {XvSettable | XvGettable, 0, 1,             sisxvsdsiscfilter},
   {XvSettable | XvGettable, 0, 8,             sisxvsdsisyfilter},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchcontrast},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchtextenhance},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchchromaflickerfilter},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchlumaflickerfilter},
   {XvSettable | XvGettable, 0, 1,             sisxvsdchcvbscolor},
   {XvSettable | XvGettable, 0, 3,             sisxvsdchoverscan},
   {XvSettable | XvGettable, 0, 3,             sisxvsdenablegamma},
   {XvSettable | XvGettable, -16, 16,          sisxvsdtvxscale},
   {XvSettable | XvGettable, -4, 3,            sisxvsdtvyscale},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetscreensize},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrir},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrig},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrib},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrir},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrig},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrib},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrir2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrig2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrib2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrir2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrig2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrib2},
   {XvSettable | XvGettable, 0, 15,            sisxvsdpanelmode},
#ifdef SIS_CP
   SIS_CP_VIDEO_ATTRIBUTES
#endif
};

static XF86AttributeRec SISAttributes_315[NUM_ATTRIBUTES_315] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvcolorkey},
   {XvSettable | XvGettable, -128, 127,        sisxvbrightness},
   {XvSettable | XvGettable, 0, 7,             sisxvcontrast},
   {XvSettable | XvGettable, -7, 7,            sisxvsaturation},
   {XvSettable | XvGettable, -8, 7,            sisxvhue},
   {XvSettable | XvGettable, 0, 1,             sisxvautopaintcolorkey},
   {XvSettable             , 0, 0,             sisxvsetdefaults},
   {XvSettable | XvGettable, -32, 32,          sisxvtvxposition},
   {XvSettable | XvGettable, -32, 32,          sisxvtvyposition},
   {XvSettable | XvGettable, 100, 10000,       sisxvgammared},
   {XvSettable | XvGettable, 100, 10000,       sisxvgammagreen},
   {XvSettable | XvGettable, 100, 10000,       sisxvgammablue},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablegfx},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablegfxlr},
   {XvSettable | XvGettable, 0, 1,             sisxvdisablecolorkey},
   {XvSettable | XvGettable, 0, 1,             sisxvusechromakey},
   {XvSettable | XvGettable, 0, 1,             sisxvinsidechromakey},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvchromamin},
   {XvSettable | XvGettable, 0, (1 << 24) - 1, sisxvchromamax},
   {             XvGettable, 0, 0xffffffff,    sisxvqueryvbflags},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetdriverversion},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgethardwareinfo},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetbusid},
   {             XvGettable, 0, 0xffffffff,    sisxvsdqueryvbflagsversion},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetsdflags},
   {XvSettable | XvGettable, 0, 0xffffffff,    sisxvsdunlocksisdirect},
   {XvSettable             , 0, 0xffffffff,    sisxvsdsetvbflags},
   {             XvGettable, 0, 0xffffffff,    sisxvsdquerydetecteddevices},
   {XvSettable | XvGettable, 0, 1,    	       sisxvsdcrt1status},
   {XvSettable             , 0, 0xffffffff,    sisxvsdcheckmodeindexforcrt2},
   {             XvGettable, 0, 0xffffffff,    sisxvsdresultcheckmodeindexforcrt2},
   {XvSettable | XvGettable, 0, 4,             sisxvsdsisantiflicker},
   {XvSettable | XvGettable, 0, 15,            sisxvsdsissaturation},
   {XvSettable | XvGettable, 0, 15,            sisxvsdsisedgeenhance},
   {XvSettable | XvGettable, -128, 127,        sisxvsdsiscolcalibf},
   {XvSettable | XvGettable, -120, 120,        sisxvsdsiscolcalibc},
   {XvSettable | XvGettable, 0, 1,             sisxvsdsiscfilter},
   {XvSettable | XvGettable, 0, 8,             sisxvsdsisyfilter},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchcontrast},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchtextenhance},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchchromaflickerfilter},
   {XvSettable | XvGettable, 0, 15,            sisxvsdchlumaflickerfilter},
   {XvSettable | XvGettable, 0, 1,             sisxvsdchcvbscolor},
   {XvSettable | XvGettable, 0, 3,             sisxvsdchoverscan},
   {XvSettable | XvGettable, 0, 7,             sisxvsdenablegamma},
   {XvSettable | XvGettable, -16, 16,          sisxvsdtvxscale},
   {XvSettable | XvGettable, -4, 3,            sisxvsdtvyscale},
   {             XvGettable, 0, 0xffffffff,    sisxvsdgetscreensize},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrir},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrig},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrib},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrir},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrig},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrib},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrir2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrig2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorebrib2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrir2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrig2},
   {XvSettable | XvGettable, 100, 10000,       sisxvsdstorepbrib2},
   {XvSettable | XvGettable, 0, 1,             sisxvsdhidehwcursor},
   {XvSettable | XvGettable, 0, 15,            sisxvsdpanelmode},
#ifdef TWDEBUG
   {XvSettable             , 0, 0xffffffff,    sisxvsetreg},
#endif
#ifdef SIS_CP
   SIS_CP_VIDEO_ATTRIBUTES
#endif
   {XvSettable | XvGettable, 0, 1,             sisxvswitchcrt},
};

#define NUM_IMAGES_300 6
#define NUM_IMAGES_315 7	    /* NV12 only - but does not work */
#define NUM_IMAGES_330 9  	    /* NV12 and NV21 */
#define PIXEL_FMT_YV12 FOURCC_YV12  /* 0x32315659 */
#define PIXEL_FMT_UYVY FOURCC_UYVY  /* 0x59565955 */
#define PIXEL_FMT_YUY2 FOURCC_YUY2  /* 0x32595559 */
#define PIXEL_FMT_I420 FOURCC_I420  /* 0x30323449 */
#define PIXEL_FMT_RGB5 0x35315652
#define PIXEL_FMT_RGB6 0x36315652
#define PIXEL_FMT_YVYU 0x55595659   /* 315/330 only */
#define PIXEL_FMT_NV12 0x3231564e   /* 330 only */
#define PIXEL_FMT_NV21 0x3132564e   /* 330 only */

/* TODO: */
#define PIXEL_FMT_RAW8 0x38574152

static XF86ImageRec SISImages[NUM_IMAGES_330] =
{
    XVIMAGE_YUY2, /* If order is changed, SISOffscreenImages must be adapted */
    XVIMAGE_YV12,
    XVIMAGE_UYVY,
    XVIMAGE_I420
    ,
    { /* RGB 555 */
      PIXEL_FMT_RGB5,
      XvRGB,
      LSBFirst,
      {'R','V','1','5',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    15, 0x001F, 0x03E0, 0x7C00, - incorrect! */
      15, 0x7C00, 0x03E0, 0x001F,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    { /* RGB 565 */
      PIXEL_FMT_RGB6,
      XvRGB,
      LSBFirst,
      {'R','V','1','6',
       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
      16,
      XvPacked,
      1,
/*    16, 0x001F, 0x07E0, 0xF800, - incorrect!  */
      16, 0xF800, 0x07E0, 0x001F,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      {'R', 'V', 'B',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
    },
    {  /* YVYU */
      PIXEL_FMT_YVYU, \
      XvYUV, \
      LSBFirst, \
      {'Y','V','Y','U',
	0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
      16,
      XvPacked,
      1,
      0, 0, 0, 0,
      8, 8, 8,
      1, 2, 2,
      1, 1, 1,
      {'Y','V','Y','U',
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
   },
   {   /* NV12 */
      PIXEL_FMT_NV12,
      XvYUV,
      LSBFirst,
      {'N','V','1','2',
       0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
      12,
      XvPlanar,
      2,
      0, 0, 0, 0,
      8, 8, 8,
      1, 2, 2,
      1, 2, 2,
      {'Y','U','V',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
   },
   {   /* NV21 */
      PIXEL_FMT_NV21,
      XvYUV,
      LSBFirst,
      {'N','V','2','1',
       0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71},
      12,
      XvPlanar,
      2,
      0, 0, 0, 0,
      8, 8, 8,
      1, 2, 2,
      1, 2, 2,
      {'Y','V','U',0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
      XvTopToBottom
   },
};

typedef struct {
    FBLinearPtr  linear;
    CARD32       bufAddr[2];

    unsigned char currentBuf;

    short drw_x, drw_y, drw_w, drw_h;
    short src_x, src_y, src_w, src_h;
    int id;
    short srcPitch, height;

    char          brightness;
    unsigned char contrast;
    char 	  hue;
    short         saturation;

    RegionRec    clip;
    CARD32       colorKey;
    Bool 	 autopaintColorKey;

    Bool 	 disablegfx;
    Bool	 disablegfxlr;

    Bool         usechromakey;
    Bool	 insidechromakey, yuvchromakey;
    CARD32	 chromamin, chromamax;

    CARD32       videoStatus;
    BOOLEAN	 overlayStatus;
    Time         offTime;
    Time         freeTime;

    CARD32       displayMode;
    Bool	 bridgeIsSlave;

    Bool         hasTwoOverlays;   /* Chipset has two overlays */
    Bool         dualHeadMode;     /* We're running in DHM */

    Bool  	 NoOverlay;
    Bool	 PrevOverlay;

    Bool	 AllowSwitchCRT;
    int 	 crtnum;	   /* 0=CRT1, 1=CRT2 */

    Bool         needToScale;      /* Need to scale video */

    int          shiftValue;       /* 315/330 series need word addr/pitch, 300 series double word */

    short  	 linebufMergeLimit;
    CARD8        linebufmask;

    short        oldx1, oldx2, oldy1, oldy2;
#ifdef SISMERGED
    short        oldx1_2, oldx2_2, oldy1_2, oldy2_2;
#endif
    int          mustwait;

    Bool         grabbedByV4L;	   /* V4L stuff */
    int          pitch;
    int          offset;

    int 	 modeflags;	   /* Flags field of current display mode */

    int 	 tvxpos, tvypos;
    Bool 	 updatetvxpos, updatetvypos;

} SISPortPrivRec, *SISPortPrivPtr;

typedef struct {
    int pixelFormat;

    CARD16  pitch;
    CARD16  origPitch;

    CARD8   keyOP;
    CARD16  HUSF;
    CARD16  VUSF;
    CARD8   IntBit;
    CARD8   wHPre;

    CARD16  srcW;
    CARD16  srcH;

    BoxRec  dstBox;

    CARD32  PSY;
    CARD32  PSV;
    CARD32  PSU;

    CARD16  SCREENheight;

    CARD8   lineBufSize;

    DisplayModePtr  currentmode;

#ifdef SISMERGED
    CARD16  pitch2;
    CARD16  HUSF2;
    CARD16  VUSF2;
    CARD8   IntBit2;
    CARD8   wHPre2;

    CARD16  srcW2;
    CARD16  srcH2;
    BoxRec  dstBox2;
    CARD32  PSY2;
    CARD32  PSV2;
    CARD32  PSU2;
    CARD16  SCREENheight2;
    CARD8   lineBufSize2;

    DisplayModePtr  currentmode2;

    Bool    DoFirst, DoSecond;
#endif

    CARD8   bobEnable;

    CARD8   contrastCtrl;
    CARD8   contrastFactor;

    CARD8   (*VBlankActiveFunc)(SISPtr, SISPortPrivPtr);
#if 0
    CARD32  (*GetScanLineFunc)(SISPtr pSiS);
#endif

#if 0
    /* The following are not used yet */
    CARD16  SubPictHUSF;        /* Subpicture scaling */
    CARD16  SubpictVUSF;
    CARD8   SubpictIntBit;
    CARD8   SubPictwHPre;
    CARD16  SubPictsrcW;       /* Subpicture source width */
    CARD16  SubPictsrcH;       /* Subpicture source height */
    BoxRec  SubPictdstBox;     /* SubPicture destination box */
    CARD32  SubPictAddr;       /* SubPicture address */
    CARD32  SubPictPitch;      /* SubPicture pitch */
    CARD32  SubPictOrigPitch;  /* SubPicture real pitch (needed for scaling twice) */
    CARD32  SubPictPreset;     /* Subpicture Preset */

    CARD32  MPEG_Y;	       /* MPEG Y Buffer Addr */
    CARD32  MPEG_UV;	       /* MPEG UV Buffer Addr */
#endif

} SISOverlayRec, *SISOverlayPtr;



/****************************************************************************
 * Raw register access : These routines directly interact with the sis's
 *                       control aperature.  Must not be called until after
 *                       the board's pci memory has been mapped.
 ****************************************************************************/

#if 0
static CARD32 _sisread(SISPtr pSiS, CARD32 reg)
{
    return *(pSiS->IOBase + reg);
}

static void _siswrite(SISPtr pSiS, CARD32 reg, CARD32 data)
{
    *(pSiS->IOBase + reg) = data;
}
#endif

static CARD8 getsrreg(SISPtr pSiS, CARD8 reg)
{
    CARD8 ret;
    inSISIDXREG(SISSR, reg, ret);
    return(ret);
}

static CARD8 getvideoreg(SISPtr pSiS, CARD8 reg)
{
    CARD8 ret;
    inSISIDXREG(SISVID, reg, ret);
    return(ret);
}

static __inline void setvideoreg(SISPtr pSiS, CARD8 reg, CARD8 data)
{
    outSISIDXREG(SISVID, reg, data);
}

static __inline void setvideoregmask(SISPtr pSiS, CARD8 reg, CARD8 data, CARD8 mask)
{
    CARD8   old;
    inSISIDXREG(SISVID, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISVID, reg, data);
}

static void setsrregmask(SISPtr pSiS, CARD8 reg, CARD8 data, CARD8 mask)
{
    CARD8   old;

    inSISIDXREG(SISSR, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISSR, reg, data);
}

/* VBlank */
static CARD8 vblank_active_CRT1(SISPtr pSiS, SISPortPrivPtr pPriv)
{
    return(inSISREG(SISINPSTAT) & 0x08);
}

static CARD8 vblank_active_CRT2(SISPtr pSiS, SISPortPrivPtr pPriv)
{
    CARD8 ret;

    if(pPriv->bridgeIsSlave) return(vblank_active_CRT1(pSiS, pPriv));

    if(pSiS->VGAEngine == SIS_315_VGA) {
       inSISIDXREG(SISPART1, 0x30, ret);
    } else {
       inSISIDXREG(SISPART1, 0x25, ret);
    }
    return((ret & 0x02) ^ 0x02);
}

/* Scanline - unused */
#if 0
static CARD32 get_scanline_CRT1(SISPtr pSiS)
{
    CARD32 line;

    _siswrite (pSiS, REG_PRIM_CRT_COUNTER, 0x00000001);
    line = _sisread (pSiS, REG_PRIM_CRT_COUNTER);

    return ((line >> 16) & 0x07FF);
}

static CARD32 get_scanline_CRT2(SISPtr pSiS)
{
    CARD32 line;

    line = (CARD32)(getsisreg(pSiS, SISPART1, Index_CRT2_FC_VCount1) & 0x70) * 16
                + getsisreg(pSiS, SISPART1, Index_CRT2_FC_VCount);

    return line;
}
#endif

static void
SiSComputeXvGamma(SISPtr pSiS)
{
    int num = 255, i;
    double red = 1.0 / (double)((double)pSiS->XvGammaRed / 1000);
    double green = 1.0 / (double)((double)pSiS->XvGammaGreen / 1000);
    double blue = 1.0 / (double)((double)pSiS->XvGammaBlue / 1000);

    for(i = 0; i <= num; i++) {
        pSiS->XvGammaRampRed[i] =
	    (red == 1.0) ? i : (CARD8)(pow((double)i / (double)num, red) * (double)num + 0.5);

	pSiS->XvGammaRampGreen[i] =
	    (green == 1.0) ? i : (CARD8)(pow((double)i / (double)num, green) * (double)num + 0.5);

	pSiS->XvGammaRampBlue[i] =
	    (blue == 1.0) ? i : (CARD8)(pow((double)i / (double)num, blue) * (double)num + 0.5);
    }
}

static void
SiSSetXvGamma(SISPtr pSiS)
{
    int i;
    unsigned char backup = getsrreg(pSiS, 0x1f);
    setsrregmask(pSiS, 0x1f, 0x08, 0x18);
    for(i = 0; i <= 255; i++) {
       MMIO_OUT32(pSiS->IOBase, 0x8570,
       			(i << 24)     |
			(pSiS->XvGammaRampBlue[i] << 16) |
			(pSiS->XvGammaRampGreen[i] << 8) |
			pSiS->XvGammaRampRed[i]);
    }
    setsrregmask(pSiS, 0x1f, backup, 0xff);
}

static void
SiSUpdateXvGamma(SISPtr pSiS, SISPortPrivPtr pPriv)
{
    unsigned char sr7 = getsrreg(pSiS, 0x07);

    if(!pSiS->XvGamma) return;
    if(!(pSiS->MiscFlags & MISC_CRT1OVERLAYGAMMA)) return;

#ifdef SISDUALHEAD
    if((pPriv->dualHeadMode) && (!pSiS->SecondHead)) return;
#endif

    if(!(sr7 & 0x04)) return;

    SiSComputeXvGamma(pSiS);
    SiSSetXvGamma(pSiS);
}

static void
SISResetXvGamma(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    SiSUpdateXvGamma(pSiS, pPriv);
}

void SISInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;

    newAdaptor = SISSetupImageVideo(pScreen);
    if(newAdaptor)
	SISInitOffscreenImages(pScreen);

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if(newAdaptor) {
       if(!num_adaptors) {
          num_adaptors = 1;
          adaptors = &newAdaptor;
       } else {
          /* need to free this someplace */
          newAdaptors = xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));
          if(newAdaptors) {
             memcpy(newAdaptors, adaptors, num_adaptors * sizeof(XF86VideoAdaptorPtr));
             newAdaptors[num_adaptors] = newAdaptor;
             adaptors = newAdaptors;
             num_adaptors++;
          }
       }
    }

    if(num_adaptors)
       xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(newAdaptors)
       xfree(newAdaptors);
}

static void
SISSetPortDefaults(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    SISPtr    pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;;
#endif    
    
    pPriv->colorKey    = pSiS->colorKey = 0x000101fe;
    pPriv->videoStatus = 0;
    pPriv->brightness  = pSiS->XvDefBri;
    pPriv->contrast    = pSiS->XvDefCon;
    pPriv->hue         = pSiS->XvDefHue;
    pPriv->saturation  = pSiS->XvDefSat;
    pPriv->autopaintColorKey = TRUE;
    pPriv->disablegfx  = pSiS->XvDefDisableGfx;
    pPriv->disablegfxlr= pSiS->XvDefDisableGfxLR;
    pSiS->disablecolorkeycurrent = pSiS->XvDisableColorKey;
    pPriv->usechromakey    = pSiS->XvUseChromaKey;
    pPriv->insidechromakey = pSiS->XvInsideChromaKey;
    pPriv->yuvchromakey    = pSiS->XvYUVChromaKey;
    pPriv->chromamin       = pSiS->XvChromaMin;
    pPriv->chromamax       = pSiS->XvChromaMax;
    if(pPriv->dualHeadMode) {
#ifdef SISDUALHEAD
       if(!pSiS->SecondHead) {
          pPriv->tvxpos      = pSiS->tvxpos;
          pPriv->tvypos      = pSiS->tvypos;
	  pPriv->updatetvxpos = TRUE;
          pPriv->updatetvypos = TRUE;
       }
#endif
    } else {
       pPriv->tvxpos      = pSiS->tvxpos;
       pPriv->tvypos      = pSiS->tvypos;
       pPriv->updatetvxpos = TRUE;
       pPriv->updatetvypos = TRUE;
    }
#ifdef SIS_CP
    SIS_CP_VIDEO_DEF
#endif
    if(pPriv->dualHeadMode) {
#ifdef SISDUALHEAD
       pPriv->crtnum =
	  pSiSEnt->curxvcrtnum =
	     pSiSEnt->XvOnCRT2 ? 1 : 0;
#endif
    } else
       pPriv->crtnum = pSiS->XvOnCRT2 ? 1 : 0;

    pSiS->XvGammaRed = pSiS->XvGammaRedDef;
    pSiS->XvGammaGreen = pSiS->XvGammaGreenDef;
    pSiS->XvGammaBlue = pSiS->XvGammaBlueDef;
    SiSUpdateXvGamma(pSiS, pPriv);
}

static void
SISResetVideo(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    /* Unlock registers */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
    if(getvideoreg (pSiS, Index_VI_Passwd) != 0xa1) {
        setvideoreg (pSiS, Index_VI_Passwd, 0x86);
        if(getvideoreg (pSiS, Index_VI_Passwd) != 0xa1)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Xv: Video password could not unlock registers\n");
    }

    /* Initialize first overlay (CRT1) ------------------------------- */

    /* This bit has obviously a different meaning on 315 series (linebuffer-related) */
    if(pSiS->VGAEngine == SIS_300_VGA) {
       /* Write-enable video registers */
       setvideoregmask(pSiS, Index_VI_Control_Misc2,      0x80, 0x81);
    } else {
       /* Select overlay 2, clear all linebuffer related bits */
       setvideoregmask(pSiS, Index_VI_Control_Misc2,      0x00, 0xb1);
    }

    /* Disable overlay */
    setvideoregmask(pSiS, Index_VI_Control_Misc0,         0x00, 0x02);

    /* Disable bob de-interlacer and some strange bit */
    setvideoregmask(pSiS, Index_VI_Control_Misc1,         0x00, 0x82);

    /* Select RGB chroma key format (300 series only) */
    if(pSiS->VGAEngine == SIS_300_VGA) {
       setvideoregmask(pSiS, Index_VI_Control_Misc0,      0x00, 0x40);
    }

    /* Reset scale control and contrast */
    /* (Enable DDA (interpolation)) */
    setvideoregmask(pSiS, Index_VI_Scale_Control,         0x60, 0x60);
    setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,     0x04, 0x1F);

    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Low,     0x00);
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Middle,  0x00);
    setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Low,         0x00);
    setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Middle,      0x00);
    setvideoreg(pSiS, Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
    setvideoreg(pSiS, Index_VI_Play_Threshold_Low,        0x00);
    setvideoreg(pSiS, Index_VI_Play_Threshold_High,       0x00);

    if(pSiS->Chipset == PCI_CHIP_SIS330) {
       setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, 0x00, 0x10);
    } else if(pSiS->Chipset == PCI_CHIP_SIS660) {
       setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, 0x00, 0xE0);
    }
    if(pSiS->sishw_ext.jChipType == SIS_661) {
       setvideoregmask(pSiS, Index_VI_V_Buf_Start_Over, 0x2c, 0x3c);
    }

    if((pSiS->ChipFlags & SiSCF_Is65x) || (pSiS->Chipset == PCI_CHIP_SIS660)) {
       setvideoregmask(pSiS, Index_VI_Control_Misc2,  0x00, 0x04);
    }

    /* Initialize second overlay (CRT2) - only for 300, 630/730, 550, M650/651, 661/741/660/760 */
    if(pPriv->hasTwoOverlays) {

        if(pSiS->VGAEngine == SIS_300_VGA) {
    	   /* Write-enable video registers */
    	   setvideoregmask(pSiS, Index_VI_Control_Misc2,      0x81, 0x81);
	} else {
	   /* Select overlay 2, clear all linebuffer related bits */
           setvideoregmask(pSiS, Index_VI_Control_Misc2,      0x01, 0xb1);
        }

    	/* Disable overlay */
    	setvideoregmask(pSiS, Index_VI_Control_Misc0,         0x00, 0x02);

    	/* Disable bob de-interlacer and some strange bit */
    	setvideoregmask(pSiS, Index_VI_Control_Misc1,         0x00, 0x82);

	/* Select RGB chroma key format */
	if(pSiS->VGAEngine == SIS_300_VGA) {
	   setvideoregmask(pSiS, Index_VI_Control_Misc0,      0x00, 0x40);
	}

    	/* Reset scale control and contrast */
	/* (Enable DDA (interpolation)) */
    	setvideoregmask(pSiS, Index_VI_Scale_Control,         0x60, 0x60);
    	setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,     0x04, 0x1F);

    	setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Low,     0x00);
    	setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Preset_Middle,  0x00);
    	setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Low,         0x00);
    	setvideoreg(pSiS, Index_VI_UV_Buf_Preset_Middle,      0x00);
    	setvideoreg(pSiS, Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
    	setvideoreg(pSiS, Index_VI_Play_Threshold_Low,        0x00);
    	setvideoreg(pSiS, Index_VI_Play_Threshold_High,       0x00);

	if(pSiS->Chipset == PCI_CHIP_SIS330) {
           setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, 0x00, 0x10);
        } else if(pSiS->Chipset == PCI_CHIP_SIS660) {
           setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, 0x00, 0xE0);
        }
	if(pSiS->sishw_ext.jChipType == SIS_661) {
           setvideoregmask(pSiS, Index_VI_V_Buf_Start_Over, 0x24, 0x3c);
        }

    }

    /* set default properties for overlay 1 (CRT1) -------------------------- */
    setvideoregmask(pSiS, Index_VI_Control_Misc2,         0x00, 0x01);
    setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,     0x04, 0x07);
    setvideoreg(pSiS, Index_VI_Brightness,                0x20);
    if(pSiS->VGAEngine == SIS_315_VGA) {
       setvideoreg(pSiS, Index_VI_Hue,          	  0x00);
       setvideoreg(pSiS, Index_VI_Saturation,             0x00);
    }

    /* set default properties for overlay 2(CRT2)  -------------------------- */
    if(pPriv->hasTwoOverlays) {
       setvideoregmask(pSiS, Index_VI_Control_Misc2,      0x01, 0x01);
       setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl,  0x04, 0x07);
       setvideoreg(pSiS, Index_VI_Brightness,             0x20);
       if(pSiS->VGAEngine == SIS_315_VGA) {
          setvideoreg(pSiS, Index_VI_Hue,                 0x00);
          setvideoreg(pSiS, Index_VI_Saturation,    	  0x00);
       }
    }

    /* Reset Xv gamma correction */
    if(pSiS->VGAEngine == SIS_315_VGA) {
       SiSUpdateXvGamma(pSiS, pPriv);
    }
}

/* Set display mode (single CRT1/CRT2, mirror).
 * MIRROR mode is only available on chipsets with two overlays.
 * On the other chipsets, if only CRT1 or only CRT2 are used,
 * the correct display CRT is chosen automatically. If both
 * CRT1 and CRT2 are connected, the user can choose between CRT1 and
 * CRT2 by using the option XvOnCRT2.
 */
static void
set_dispmode(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    SISPtr pSiS = SISPTR(pScrn);
    
    pPriv->dualHeadMode = pPriv->bridgeIsSlave = FALSE;

    if(SiSBridgeIsInSlaveMode(pScrn)) pPriv->bridgeIsSlave = TRUE;

    if( (pSiS->VBFlags & VB_DISPMODE_MIRROR) ||
        ((pPriv->bridgeIsSlave) && (pSiS->VBFlags & DISPTYPE_DISP2)) )  {
       if(pPriv->hasTwoOverlays)
          pPriv->displayMode = DISPMODE_MIRROR;     /* CRT1+CRT2 (2 overlays) */
       else if(pPriv->crtnum)
	  pPriv->displayMode = DISPMODE_SINGLE2;    /* CRT2 only */
       else
	  pPriv->displayMode = DISPMODE_SINGLE1;    /* CRT1 only */
    } else {
#ifdef SISDUALHEAD
       if(pSiS->DualHeadMode) {
          pPriv->dualHeadMode = TRUE;
      	  if(pSiS->SecondHead)
	     pPriv->displayMode = DISPMODE_SINGLE1; /* CRT1 only */
	  else
	     pPriv->displayMode = DISPMODE_SINGLE2; /* CRT2 only */
       } else
#endif
       if(pSiS->VBFlags & DISPTYPE_DISP1) {
      	  pPriv->displayMode = DISPMODE_SINGLE1;    /* CRT1 only */
       } else {
          pPriv->displayMode = DISPMODE_SINGLE2;    /* CRT2 only */
       }
    }
}

static void
set_disptype_regs(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
    SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
    int crtnum = 0;
    
    if(pPriv->dualHeadMode) crtnum = pSiSEnt->curxvcrtnum;
#endif 

    /*
     *     SR06[7:6]
     *	      Bit 7: Enable overlay 2 on CRT2
     *	      Bit 6: Enable overlay 1 on CRT2
     *     SR32[7:6]
     *        Bit 7: DCLK/TCLK overlay 2
     *               0=DCLK (overlay on CRT1)
     *               1=TCLK (overlay on CRT2)
     *        Bit 6: DCLK/TCLK overlay 1
     *               0=DCLK (overlay on CRT1)
     *               1=TCLK (overlay on CRT2)
     *
     * On chipsets with two overlays, we can freely select and also
     * have a mirror mode. However, we use overlay 1 for CRT1 and
     * overlay 2 for CRT2.
     * ATTENTION: CRT2 can only take up to 1 (one) overlay. Setting
     * SR06/32 to 0xc0 DOES NOT WORK. THAT'S CONFIRMED.
     * Therefore, we use overlay 1 on CRT2 if in SINGLE2 mode.
     *
     * For chipsets with only one overlay, user must choose whether
     * to display the overlay on CRT1 or CRT2 by setting XvOnCRT2
     * to TRUE (CRT2) or FALSE (CRT1). The driver does this auto-
     * matically if only CRT1 or only CRT2 is used.
     */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    switch (pPriv->displayMode)
    {
        case DISPMODE_SINGLE1:				/* CRT1-only mode: */
	  if(pPriv->hasTwoOverlays) {
	      if(pPriv->dualHeadMode) {
	         setsrregmask(pSiS, 0x06, 0x00, 0x40);  /* overlay 1 -> CRT1 */
      	         setsrregmask(pSiS, 0x32, 0x00, 0x40);
	      } else {
      	         setsrregmask(pSiS, 0x06, 0x00, 0xc0);  /* both overlays -> CRT1 */
      	         setsrregmask(pSiS, 0x32, 0x00, 0xc0);
              }
	  } else {
#ifdef SISDUALHEAD
	      if((!pPriv->dualHeadMode) || (crtnum == 0)) {
#endif
	         setsrregmask(pSiS, 0x06, 0x00, 0xc0);  /* only overlay -> CRT1 */
	         setsrregmask(pSiS, 0x32, 0x00, 0xc0);
#ifdef SISDUALHEAD
	      }
#endif
	  }
	  break;

       	case DISPMODE_SINGLE2:  			/* CRT2-only mode: */
	  if(pPriv->hasTwoOverlays) {
	      if(pPriv->dualHeadMode) {
	         setsrregmask(pSiS, 0x06, 0x80, 0x80);  /* overlay 2 -> CRT2 */
      	         setsrregmask(pSiS, 0x32, 0x80, 0x80);
	      } else {
   	         setsrregmask(pSiS, 0x06, 0x40, 0xc0);  /* overlay 1 -> CRT2 */
      	         setsrregmask(pSiS, 0x32, 0xc0, 0xc0);  /* (although both clocks for CRT2!) */
	      }
	  } else {
#ifdef SISDUALHEAD
	      if((!pPriv->dualHeadMode) || (crtnum == 1)) {
#endif
                 setsrregmask(pSiS, 0x06, 0x40, 0xc0);  /* only overlay -> CRT2 */
	         setsrregmask(pSiS, 0x32, 0x40, 0xc0);
#ifdef SISDUALHEAD
              }
#endif
	  }
	  break;

    	case DISPMODE_MIRROR:				/* CRT1+CRT2-mode: (only on chips with 2 overlays) */
	default:
          setsrregmask(pSiS, 0x06, 0x80, 0xc0);         /* overlay 1 -> CRT1, overlay 2 -> CRT2 */
      	  setsrregmask(pSiS, 0x32, 0x80, 0xc0);
	  break;
    }
}

static void
set_allowswitchcrt(SISPtr pSiS, SISPortPrivPtr pPriv)
{
    if(pSiS->hasTwoOverlays) {
       pPriv->AllowSwitchCRT = FALSE;
    } else {
       pPriv->AllowSwitchCRT = TRUE;
       if(pSiS->XvOnCRT2) {
          if(!(pSiS->VBFlags & DISPTYPE_DISP1)) {
	     pPriv->AllowSwitchCRT = FALSE;
	  }
       } else {
          if(!(pSiS->VBFlags & DISPTYPE_DISP2)) {
	     pPriv->AllowSwitchCRT = FALSE;
	  }
       }
    }
}

static void
set_maxencoding(SISPtr pSiS, SISPortPrivPtr pPriv)
{
    if(pSiS->VGAEngine == SIS_300_VGA) {
       DummyEncoding.width = IMAGE_MAX_WIDTH_300;
       DummyEncoding.height = IMAGE_MAX_HEIGHT_300;
    } else {
       DummyEncoding.width = IMAGE_MAX_WIDTH_315;
       DummyEncoding.height = IMAGE_MAX_HEIGHT_315;
       if(pPriv->hasTwoOverlays) {
          /* Only half width available if both overlays
	   * are going to be used
	   */
#ifdef SISDUALHEAD
          if(pSiS->DualHeadMode) {
	     if(pSiS->Chipset == PCI_CHIP_SIS660) {
	        DummyEncoding.width = 1536;
	     } else {
                DummyEncoding.width >>= 1;
	     }
          } else
#endif
#ifdef SISMERGED
          if(pSiS->MergedFB) {
	     if(pSiS->Chipset == PCI_CHIP_SIS660) {
	        DummyEncoding.width = 1536;
	     } else {
                DummyEncoding.width >>= 1;
	     }
          } else
#endif
          if(pPriv->displayMode == DISPMODE_MIRROR) {
	     if(pSiS->Chipset == PCI_CHIP_SIS660) {
	        DummyEncoding.width = 1536;
	     } else {
                DummyEncoding.width >>= 1;
	     }
          }
       }
    }
}

static XF86VideoAdaptorPtr
SISSetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr pSiS = SISPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    SISPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
                            sizeof(SISPortPrivRec) +
                            sizeof(DevUnion))))
    	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "SIS 300/315/330 series Video Overlay";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding;

    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = SISFormats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

    pPriv = (SISPortPrivPtr)(&adapt->pPortPrivates[1]);
    
    /* Setup chipset type helpers */
    if(pSiS->hasTwoOverlays) {
       pPriv->hasTwoOverlays = TRUE;
       pPriv->AllowSwitchCRT = FALSE;
    } else {
       pPriv->hasTwoOverlays = FALSE;
       pPriv->AllowSwitchCRT = TRUE;
       if(pSiS->XvOnCRT2) {
          if(!(pSiS->VBFlags & DISPTYPE_DISP1)) {
	     pPriv->AllowSwitchCRT = FALSE;
	  }
       } else {
          if(!(pSiS->VBFlags & DISPTYPE_DISP2)) {
	     pPriv->AllowSwitchCRT = FALSE;
	  }
       }
    }

    set_allowswitchcrt(pSiS, pPriv);

    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    if(pSiS->VGAEngine == SIS_300_VGA) {
       adapt->nImages = NUM_IMAGES_300;
       adapt->pAttributes = SISAttributes_300;
       adapt->nAttributes = NUM_ATTRIBUTES_300;
    } else {
       if(pSiS->sishw_ext.jChipType >= SIS_330) {
          adapt->nImages = NUM_IMAGES_330;
       } else {
          adapt->nImages = NUM_IMAGES_315;
       }
       adapt->pAttributes = SISAttributes_315;
       adapt->nAttributes = NUM_ATTRIBUTES_315;
       if(pPriv->hasTwoOverlays) adapt->nAttributes--;
    }
    
    adapt->pImages = SISImages;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = SISStopVideo;
    adapt->SetPortAttribute = SISSetPortAttribute;
    adapt->GetPortAttribute = SISGetPortAttribute;
    adapt->QueryBestSize = SISQueryBestSize;
    adapt->PutImage = SISPutImage;
    adapt->QueryImageAttributes = SISQueryImageAttributes;

    pPriv->videoStatus = 0;
    pPriv->currentBuf  = 0;
    pPriv->linear      = NULL;
    pPriv->grabbedByV4L= FALSE;
    pPriv->NoOverlay   = FALSE;
    pPriv->PrevOverlay = FALSE;

    /* gotta uninit this someplace */
#if defined(REGION_NULL)
    REGION_NULL(pScreen, &pPriv->clip);
#else
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0);
#endif

    pSiS->adaptor = adapt;

    pSiS->xvBrightness = MAKE_ATOM(sisxvbrightness);
    pSiS->xvContrast   = MAKE_ATOM(sisxvcontrast);
    pSiS->xvColorKey   = MAKE_ATOM(sisxvcolorkey);
    pSiS->xvSaturation = MAKE_ATOM(sisxvsaturation);
    pSiS->xvHue        = MAKE_ATOM(sisxvhue);
    pSiS->xvSwitchCRT  = MAKE_ATOM(sisxvswitchcrt);
    pSiS->xvAutopaintColorKey = MAKE_ATOM(sisxvautopaintcolorkey);
    pSiS->xvSetDefaults       = MAKE_ATOM(sisxvsetdefaults);
    pSiS->xvDisableGfx        = MAKE_ATOM(sisxvdisablegfx);
    pSiS->xvDisableGfxLR      = MAKE_ATOM(sisxvdisablegfxlr);
    pSiS->xvTVXPosition       = MAKE_ATOM(sisxvtvxposition);
    pSiS->xvTVYPosition       = MAKE_ATOM(sisxvtvyposition);
    pSiS->xvGammaRed  	      = MAKE_ATOM(sisxvgammared);
    pSiS->xvGammaGreen 	      = MAKE_ATOM(sisxvgammagreen);
    pSiS->xvGammaBlue  	      = MAKE_ATOM(sisxvgammablue);
    pSiS->xvDisableColorkey   = MAKE_ATOM(sisxvdisablecolorkey);
    pSiS->xvUseChromakey      = MAKE_ATOM(sisxvusechromakey);
    pSiS->xvInsideChromakey   = MAKE_ATOM(sisxvinsidechromakey);
    pSiS->xvYUVChromakey      = MAKE_ATOM(sisxvyuvchromakey);
    pSiS->xvChromaMin	      = MAKE_ATOM(sisxvchromamin);
    pSiS->xvChromaMax         = MAKE_ATOM(sisxvchromamax);
    pSiS->xv_QVF              = MAKE_ATOM(sisxvqueryvbflags);
    pSiS->xv_GDV	      = MAKE_ATOM(sisxvsdgetdriverversion);
    pSiS->xv_GHI	      = MAKE_ATOM(sisxvsdgethardwareinfo);
    pSiS->xv_GBI	      = MAKE_ATOM(sisxvsdgetbusid);
    pSiS->xv_QVV              = MAKE_ATOM(sisxvsdqueryvbflagsversion);
    pSiS->xv_GSF              = MAKE_ATOM(sisxvsdgetsdflags);
    pSiS->xv_USD              = MAKE_ATOM(sisxvsdunlocksisdirect);
    pSiS->xv_SVF              = MAKE_ATOM(sisxvsdsetvbflags);
    pSiS->xv_QDD	      = MAKE_ATOM(sisxvsdquerydetecteddevices);
    pSiS->xv_CT1	      = MAKE_ATOM(sisxvsdcrt1status);
    pSiS->xv_CMD	      = MAKE_ATOM(sisxvsdcheckmodeindexforcrt2);
    pSiS->xv_CMDR	      = MAKE_ATOM(sisxvsdresultcheckmodeindexforcrt2);
    pSiS->xv_TAF	      = MAKE_ATOM(sisxvsdsisantiflicker);
    pSiS->xv_TSA	      = MAKE_ATOM(sisxvsdsissaturation);
    pSiS->xv_TEE	      = MAKE_ATOM(sisxvsdsisedgeenhance);
    pSiS->xv_COC	      = MAKE_ATOM(sisxvsdsiscolcalibc);
    pSiS->xv_COF	      = MAKE_ATOM(sisxvsdsiscolcalibf);
    pSiS->xv_CFI	      = MAKE_ATOM(sisxvsdsiscfilter);
    pSiS->xv_YFI	      = MAKE_ATOM(sisxvsdsisyfilter);
    pSiS->xv_TCO	      = MAKE_ATOM(sisxvsdchcontrast);
    pSiS->xv_TTE	      = MAKE_ATOM(sisxvsdchtextenhance);
    pSiS->xv_TCF	      = MAKE_ATOM(sisxvsdchchromaflickerfilter);
    pSiS->xv_TLF	      = MAKE_ATOM(sisxvsdchlumaflickerfilter);
    pSiS->xv_TCC	      = MAKE_ATOM(sisxvsdchcvbscolor);
    pSiS->xv_OVR	      = MAKE_ATOM(sisxvsdchoverscan);
    pSiS->xv_SGA	      = MAKE_ATOM(sisxvsdenablegamma);
    pSiS->xv_TXS	      = MAKE_ATOM(sisxvsdtvxscale);
    pSiS->xv_TYS	      = MAKE_ATOM(sisxvsdtvyscale);
    pSiS->xv_GSS	      = MAKE_ATOM(sisxvsdgetscreensize);
    pSiS->xv_BRR	      = MAKE_ATOM(sisxvsdstorebrir);
    pSiS->xv_BRG	      = MAKE_ATOM(sisxvsdstorebrig);
    pSiS->xv_BRB	      = MAKE_ATOM(sisxvsdstorebrib);
    pSiS->xv_PBR	      = MAKE_ATOM(sisxvsdstorepbrir);
    pSiS->xv_PBG	      = MAKE_ATOM(sisxvsdstorepbrig);
    pSiS->xv_PBB	      = MAKE_ATOM(sisxvsdstorepbrib);
    pSiS->xv_BRR2	      = MAKE_ATOM(sisxvsdstorebrir2);
    pSiS->xv_BRG2	      = MAKE_ATOM(sisxvsdstorebrig2);
    pSiS->xv_BRB2	      = MAKE_ATOM(sisxvsdstorebrib2);
    pSiS->xv_PBR2	      = MAKE_ATOM(sisxvsdstorepbrir2);
    pSiS->xv_PBG2	      = MAKE_ATOM(sisxvsdstorepbrig2);
    pSiS->xv_PBB2	      = MAKE_ATOM(sisxvsdstorepbrib2);
    pSiS->xv_SHC	      = MAKE_ATOM(sisxvsdhidehwcursor);
    pSiS->xv_PMD	      = MAKE_ATOM(sisxvsdpanelmode);
#ifdef TWDEBUG
    pSiS->xv_STR	      = MAKE_ATOM(sisxvsetreg);
#endif
#ifdef SIS_CP
    SIS_CP_VIDEO_ATOMS
#endif

    pSiS->xv_sisdirectunlocked = 0;
    pSiS->xv_sd_result = 0;

    /* 300 series require double words for addresses and pitches,
     * 315/330 series require word.
     */
    switch (pSiS->VGAEngine) {
    case SIS_315_VGA:
    	pPriv->shiftValue = 1;
	break;
    case SIS_300_VGA:
    default:
    	pPriv->shiftValue = 2;
	break;
    }

    /* Set displayMode according to VBFlags */
    set_dispmode(pScrn, pPriv);

    /* Now for the linebuffer stuff.
     * All chipsets have a certain number of linebuffers, each of a certain
     * size. The number of buffers is per overlay.
     * Chip        number     size        	  max video size
     *  300          2         ?		     720x576
     *  630/730      2         ?		     720x576
     *  315          2         960?	    	    1920x1080
     *  650/740      2         960 ("120x128")	    1920x1080
     *  M650/651..   4         480	    	    1920x1080
     *  330          2         960	    	    1920x1080
     *  661/741/760  4	       768 		    1920x1080
     * The unit of size is unknown; I just know that a size of 480 limits
     * the video source width to 384. Beyond that, line buffers must be
     * merged (otherwise the video output is garbled).
     * To use the maximum width (eg 1920x1080 on the 315 series, including
     * the M650, 651 and later), *all* line buffers must be merged. Hence,
     * we can only use one overlay. This should be set up for modes where
     * either only CRT1 or only CRT2 is used.
     * If both overlays are going to be used (such as in modes were both
     * CRT1 and CRT2 are active), we are limited to the half of the
     * maximum width, or 1536 on 661/741/760.
     */

    pPriv->linebufMergeLimit = LINEBUFLIMIT1;
    if(pSiS->Chipset == PCI_CHIP_SIS660) {
       pPriv->linebufMergeLimit = LINEBUFLIMIT3;
    }

    set_maxencoding(pSiS, pPriv);

    if(pSiS->VGAEngine == SIS_300_VGA) {
       pPriv->linebufmask = 0x11;
    } else {
       pPriv->linebufmask = 0xb1;
       if(!(pPriv->hasTwoOverlays)) {
          /* On machines with only one overlay, the linebuffers are
           * generally larger, so our merging-limit is higher, too.
	   */
          pPriv->linebufMergeLimit = LINEBUFLIMIT2;
       }
    }
    
    /* Reset the properties to their defaults */
    SISSetPortDefaults(pScrn, pPriv);

    /* Set SR(06, 32) registers according to DISPMODE */
    set_disptype_regs(pScrn, pPriv);

    SISResetVideo(pScrn);
    pSiS->ResetXv = SISResetVideo;
    if(pSiS->VGAEngine == SIS_315_VGA) {
       pSiS->ResetXvGamma = SISResetXvGamma;
    }

    return adapt;
}

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,0,0)
static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    int *dataA, *dataB;
    int num;

    num = REGION_NUM_RECTS(A);
    if(num != REGION_NUM_RECTS(B))
    return FALSE;

    if((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) ||
       (A->extents.y2 != B->extents.y2))
    return FALSE;

    dataA = (int*)REGION_RECTS(A);
    dataB = (int*)REGION_RECTS(B);

    while(num--) {
      if((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
        return FALSE;
      dataA += 2;
      dataB += 2;
    }

    return TRUE;
}
#endif

static int
SISSetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
  		    INT32 value, pointer data)
{
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
  SISEntPtr pSiSEnt = pSiS->entityPrivate;;
#endif  

  if(attribute == pSiS->xvBrightness) {
     if((value < -128) || (value > 127))
        return BadValue;
     pPriv->brightness = value;
  } else if(attribute == pSiS->xvContrast) {
     if((value < 0) || (value > 7))
        return BadValue;
     pPriv->contrast = value;
  } else if(attribute == pSiS->xvColorKey) {
     pPriv->colorKey = pSiS->colorKey = value;
     REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
  } else if(attribute == pSiS->xvAutopaintColorKey) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->autopaintColorKey = value;
  } else if(attribute == pSiS->xvSetDefaults) {
     SISSetPortDefaults(pScrn, pPriv);
  } else if(attribute == pSiS->xvDisableGfx) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->disablegfx = value;
  } else if(attribute == pSiS->xvDisableGfxLR) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->disablegfxlr = value;
  } else if(attribute == pSiS->xvTVXPosition) {
     if((value < -32) || (value > 32))
        return BadValue;
     pPriv->tvxpos = value;
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetTVxposoffset(pScrn, pPriv->tvxpos);
        pPriv->updatetvxpos = FALSE;
     } else {
        pSiS->tvxpos = pPriv->tvxpos;
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->tvxpos = pPriv->tvxpos;
#endif
        pPriv->updatetvxpos = TRUE;
     }
  } else if(attribute == pSiS->xvTVYPosition) {
     if((value < -32) || (value > 32))
        return BadValue;
     pPriv->tvypos = value;
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetTVyposoffset(pScrn, pPriv->tvypos);
        pPriv->updatetvypos = FALSE;
     } else {
        pSiS->tvypos = pPriv->tvypos;
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->tvypos = pPriv->tvypos;
#endif
        pPriv->updatetvypos = TRUE;
     }
  } else if(attribute == pSiS->xvDisableColorkey) {
     if((value < 0) || (value > 1))
        return BadValue;
     pSiS->disablecolorkeycurrent = value;
  } else if(attribute == pSiS->xvUseChromakey) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->usechromakey = value;
  } else if(attribute == pSiS->xvInsideChromakey) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->insidechromakey = value;
  } else if(attribute == pSiS->xvYUVChromakey) {
     if((value < 0) || (value > 1))
        return BadValue;
     pPriv->yuvchromakey = value;
  } else if(attribute == pSiS->xvChromaMin) {
     pPriv->chromamin = value;
  } else if(attribute == pSiS->xvChromaMax) {
     pPriv->chromamax = value;
  } else if(attribute == pSiS->xv_USD) {
     if(pSiS->enablesisctrl) {
        if(value == SIS_DIRECTKEY) {
	   pSiS->xv_sisdirectunlocked++;
	} else if(pSiS->xv_sisdirectunlocked) {
	   pSiS->xv_sisdirectunlocked--;
	}
     } else {
     	pSiS->xv_sisdirectunlocked = 0;
     }
  } else if(attribute == pSiS->xv_SVF) {
#ifdef SISDUALHEAD
     if(!pPriv->dualHeadMode)
#endif
        if(pSiS->xv_sisdirectunlocked) {
	   SISSwitchCRT2Type(pScrn, (unsigned long)value);
	   set_dispmode(pScrn, pPriv);
	   set_allowswitchcrt(pSiS, pPriv);
	   set_maxencoding(pSiS, pPriv);
        }
  } else if(attribute == pSiS->xv_CT1) {
#ifdef SISDUALHEAD
     if(!pPriv->dualHeadMode)
#endif
        if(pSiS->xv_sisdirectunlocked) {
	   SISSwitchCRT1Status(pScrn, (unsigned long)value);
	   set_dispmode(pScrn, pPriv);
	   set_allowswitchcrt(pSiS, pPriv);
	   set_maxencoding(pSiS, pPriv);
        }
  } else if(attribute == pSiS->xv_TAF) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVantiflicker(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TSA) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVsaturation(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TEE) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVedgeenhance(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_CFI) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVcfilter(pScrn, value ? 1 : 0);
     }
  } else if(attribute == pSiS->xv_YFI) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVyfilter(pScrn, value);
     }
  } else if(attribute == pSiS->xv_COC) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVcolcalib(pScrn, (int)value, TRUE);
     }
  } else if(attribute == pSiS->xv_COF) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetSISTVcolcalib(pScrn, (int)value, FALSE);
     }
  } else if(attribute == pSiS->xv_TCO) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetCHTVcontrast(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TTE) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetCHTVtextenhance(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TCF) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetCHTVchromaflickerfilter(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TLF) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetCHTVlumaflickerfilter(pScrn, (int)value);
     }
  } else if(attribute == pSiS->xv_TCC) {
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetCHTVcvbscolor(pScrn, value ? 1 : 0);
     }
  } else if(attribute == pSiS->xv_OVR) {
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->UseCHOverScan = -1;
        pSiS->OptTVSOver = FALSE;
        if(value == 3) {
	   if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTSOVER) {
     	      pSiS->OptTVSOver = TRUE;
	   }
	   pSiS->UseCHOverScan = 1;
        } else if(value == 2) pSiS->UseCHOverScan = 1;
        else if(value == 1)   pSiS->UseCHOverScan = 0;
     }
  } else if(attribute == pSiS->xv_CMD) {
     if(pSiS->xv_sisdirectunlocked) {
        int result = 0;
        pSiS->xv_sd_result = (value & 0xffffff00);
        result = SISCheckModeIndexForCRT2Type(pScrn, (unsigned short)(value & 0xff),
	                                      (unsigned short)((value >> 8) & 0xff),
					      FALSE);
	pSiS->xv_sd_result |= (result & 0xff);
     }
  } else if(attribute == pSiS->xv_SGA) {
     if(pSiS->xv_sisdirectunlocked) {
        Bool backup = pSiS->XvGamma;
        pSiS->CRT1gamma = (value & 0x01) ? TRUE : FALSE;
	pSiS->CRT2gamma = (value & 0x02) ? TRUE : FALSE;
	pSiS->XvGamma   = (value & 0x04) ? TRUE : FALSE;
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) {
           pSiSEnt->CRT1gamma = pSiS->CRT1gamma;
	   pSiSEnt->CRT2gamma = pSiS->CRT2gamma;
        }
#endif
        if(pSiS->VGAEngine == SIS_315_VGA) {
           if(backup != pSiS->XvGamma) {
	      SiSUpdateXvGamma(pSiS, pPriv);
	   }
	}
     }
  } else if(attribute == pSiS->xv_TXS) {
     if((value < -16) || (value > 16))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetTVxscale(pScrn, value);
     }
  } else if(attribute == pSiS->xv_TYS) {
     if((value < -4) || (value > 3))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        SiS_SetTVyscale(pScrn, value);
     }
  } else if(attribute == pSiS->xv_BRR) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaBriR = value;
     }
  } else if(attribute == pSiS->xv_BRG) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaBriG = value;
     }
  } else if(attribute == pSiS->xv_BRB) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaBriB = value;
     }
  } else if(attribute == pSiS->xv_PBR) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaPBriR = value;
     }
  } else if(attribute == pSiS->xv_PBG) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaPBriG = value;
     }
  } else if(attribute == pSiS->xv_PBB) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
        pSiS->GammaPBriB = value;
     }
  } else if(attribute == pSiS->xv_BRR2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaBriR = value;
#endif
     }
  } else if(attribute == pSiS->xv_BRG2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaBriG = value;
#endif
     }
  } else if(attribute == pSiS->xv_BRB2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaBriB = value;
#endif
     }
  } else if(attribute == pSiS->xv_PBR2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaPBriR = value;
#endif
     }
  } else if(attribute == pSiS->xv_PBG2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaPBriG = value;
#endif
     }
  } else if(attribute == pSiS->xv_PBB2) {
     if((value < 100) || (value > 10000))
        return BadValue;
     if(pSiS->xv_sisdirectunlocked) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) pSiSEnt->GammaPBriB = value;
#endif
     }
  } else if(attribute == pSiS->xv_SHC) {
     if(pSiS->xv_sisdirectunlocked) {
        Bool VisibleBackup = pSiS->HWCursorIsVisible;
        pSiS->HideHWCursor = value ? TRUE : FALSE;
	if(pSiS->CursorInfoPtr) {
	   if(VisibleBackup) {
	      if(value) {
	         (pSiS->CursorInfoPtr->HideCursor)(pScrn);
	      } else {
	         (pSiS->CursorInfoPtr->ShowCursor)(pScrn);
	      }
	   }
	   pSiS->HWCursorIsVisible = VisibleBackup;
	}
     }
  } else if(attribute == pSiS->xv_PMD) {
     if(pSiS->xv_sisdirectunlocked) {
        if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTSCALE) {
	   if(value & 0x01)      pSiS->SiS_Pr->UsePanelScaler = -1;
	   else if(value & 0x02) pSiS->SiS_Pr->UsePanelScaler = 1;
	   else			 pSiS->SiS_Pr->UsePanelScaler = 0;
	   if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTCENTER) {
	      if(value & 0x04)      pSiS->SiS_Pr->CenterScreen = -1;
	      else if(value & 0x08) pSiS->SiS_Pr->CenterScreen = 1;
	      else		    pSiS->SiS_Pr->CenterScreen = 0;
	   }
        }
     }
#ifdef TWDEBUG
  } else if(attribute == pSiS->xv_STR) {
     unsigned short port;
     switch((value & 0xff000000) >> 24) {
     case 0x00: port = SISSR;    break;
     case 0x01: port = SISPART1; break;
     case 0x02: port = SISPART2; break;
     case 0x03: port = SISPART3; break;
     case 0x04: port = SISPART4; break;
     case 0x05: port = SISCR;    break;
     case 0x06: port = SISVID;   break;
     default:   return BadValue;
     }
     outSISIDXREG(port,((value & 0x00ff0000) >> 16), ((value & 0x0000ff00) >> 8));
     return Success;
#endif
#ifdef SIS_CP
  SIS_CP_VIDEO_SETATTRIBUTE
#endif
  } else if(pSiS->VGAEngine == SIS_315_VGA) {
     if(attribute == pSiS->xvSwitchCRT) {
        if(pPriv->AllowSwitchCRT) {
           if((value < 0) || (value > 1))
              return BadValue;
	   pPriv->crtnum = value;
#ifdef SISDUALHEAD
           if(pPriv->dualHeadMode) pSiSEnt->curxvcrtnum = value;
#endif
        }
     } else if(attribute == pSiS->xvHue) {
       if((value < -8) || (value > 7))
          return BadValue;
       pPriv->hue = value;
     } else if(attribute == pSiS->xvSaturation) {
       if((value < -7) || (value > 7))
          return BadValue;
       pPriv->saturation = value;
     } else if(attribute == pSiS->xvGammaRed) {
       if((value < 100) || (value > 10000))
          return BadValue;
       pSiS->XvGammaRed = value;
       SiSUpdateXvGamma(pSiS, pPriv);
     } else if(attribute == pSiS->xvGammaGreen) {
       if((value < 100) || (value > 10000))
          return BadValue;
       pSiS->XvGammaGreen = value;
       SiSUpdateXvGamma(pSiS, pPriv);
     } else if(attribute == pSiS->xvGammaBlue) {
       if((value < 100) || (value > 10000))
          return BadValue;
       pSiS->XvGammaBlue = value;
       SiSUpdateXvGamma(pSiS, pPriv);
     } else return BadMatch;
  } else return BadMatch;
  return Success;
}

static int
SISGetPortAttribute(
  ScrnInfoPtr pScrn,
  Atom attribute,
  INT32 *value, 
  pointer data
){
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
  SISEntPtr pSiSEnt = pSiS->entityPrivate;;
#endif

  if(attribute == pSiS->xvBrightness) {
     *value = pPriv->brightness;
  } else if(attribute == pSiS->xvContrast) {
     *value = pPriv->contrast;
  } else if(attribute == pSiS->xvColorKey) {
     *value = pPriv->colorKey;
  } else if(attribute == pSiS->xvAutopaintColorKey) {
     *value = (pPriv->autopaintColorKey) ? 1 : 0;
  } else if(attribute == pSiS->xvDisableGfx) {
     *value = (pPriv->disablegfx) ? 1 : 0;
  } else if(attribute == pSiS->xvDisableGfxLR) {
     *value = (pPriv->disablegfxlr) ? 1 : 0;
  } else if(attribute == pSiS->xvTVXPosition) {
     *value = SiS_GetTVxposoffset(pScrn);
  } else if(attribute == pSiS->xvTVYPosition) {
     *value = SiS_GetTVyposoffset(pScrn);
  } else if(attribute == pSiS->xvDisableColorkey) {
     *value = (pSiS->disablecolorkeycurrent) ? 1 : 0;
  } else if(attribute == pSiS->xvUseChromakey) {
     *value = (pPriv->usechromakey) ? 1 : 0;
  } else if(attribute == pSiS->xvInsideChromakey) {
     *value = (pPriv->insidechromakey) ? 1 : 0;
  } else if(attribute == pSiS->xvYUVChromakey) {
     *value = (pPriv->yuvchromakey) ? 1 : 0;
  } else if(attribute == pSiS->xvChromaMin) {
     *value = pPriv->chromamin;
  } else if(attribute == pSiS->xvChromaMax) {
     *value = pPriv->chromamax;
  } else if(attribute == pSiS->xv_QVF) {
     *value = pSiS->VBFlags;
  } else if(attribute == pSiS->xv_GDV) {
     *value = SISDRIVERIVERSION;
  } else if(attribute == pSiS->xv_GHI) {
     *value = (pSiS->ChipFlags & 0xffff) | (pSiS->sishw_ext.jChipType << 16) | (pSiS->ChipRev << 24);
  } else if(attribute == pSiS->xv_GBI) {
     *value = (pSiS->PciInfo->bus << 16) | (pSiS->PciInfo->device << 8) | pSiS->PciInfo->func;
  } else if(attribute == pSiS->xv_QVV) {
     *value = SIS_VBFlagsVersion;
  } else if(attribute == pSiS->xv_QDD) {
     *value = pSiS->detectedCRT2Devices;
  } else if(attribute == pSiS->xv_CT1) {
     *value = pSiS->CRT1isoff ? 0 : 1;
  } else if(attribute == pSiS->xv_GSF) {
     *value = pSiS->SiS_SD_Flags;
  } else if(attribute == pSiS->xv_USD) {
     *value = pSiS->xv_sisdirectunlocked;
  } else if(attribute == pSiS->xv_TAF) {
     *value = SiS_GetSISTVantiflicker(pScrn);
  } else if(attribute == pSiS->xv_TSA) {
     *value = SiS_GetSISTVsaturation(pScrn);
  } else if(attribute == pSiS->xv_TEE) {
     *value = SiS_GetSISTVedgeenhance(pScrn);
  } else if(attribute == pSiS->xv_CFI) {
     *value = SiS_GetSISTVcfilter(pScrn);
  } else if(attribute == pSiS->xv_YFI) {
     *value = SiS_GetSISTVyfilter(pScrn);
  } else if(attribute == pSiS->xv_COC) {
     *value = SiS_GetSISTVcolcalib(pScrn, TRUE);
  } else if(attribute == pSiS->xv_COF) {
     *value = SiS_GetSISTVcolcalib(pScrn, FALSE);
  } else if(attribute == pSiS->xv_TCO) {
     *value = SiS_GetCHTVcontrast(pScrn);
  } else if(attribute == pSiS->xv_TTE) {
     *value = SiS_GetCHTVtextenhance(pScrn);
  } else if(attribute == pSiS->xv_TCF) {
     *value = SiS_GetCHTVchromaflickerfilter(pScrn);
  } else if(attribute == pSiS->xv_TLF) {
     *value = SiS_GetCHTVlumaflickerfilter(pScrn);
  } else if(attribute == pSiS->xv_TCC) {
     *value = SiS_GetCHTVcvbscolor(pScrn);
  } else if(attribute == pSiS->xv_CMDR) {
     *value = pSiS->xv_sd_result;
  } else if(attribute == pSiS->xv_OVR) {
     /* Changing of CRT2 settings not supported in DHM! */
     *value = 0;
     if(pSiS->OptTVSOver == 1)         *value = 3;
     else if(pSiS->UseCHOverScan == 1) *value = 2;
     else if(pSiS->UseCHOverScan == 0) *value = 1;
  } else if(attribute == pSiS->xv_SGA) {
     *value = 0;
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) {
        if(pSiSEnt->CRT1gamma) *value |= 0x01;
	if(pSiSEnt->CRT2gamma) *value |= 0x02;
     } else {
#endif
	if(pSiS->CRT1gamma) *value |= 0x01;
	if(pSiS->CRT2gamma) *value |= 0x02;
#ifdef SISDUALHEAD
     }
     if(pSiS->XvGamma) *value |= 0x04;
#endif
  } else if(attribute == pSiS->xv_TXS) {
     *value = SiS_GetTVxscale(pScrn);
  } else if(attribute == pSiS->xv_TYS) {
     *value = SiS_GetTVyscale(pScrn);
  } else if(attribute == pSiS->xv_GSS) {
     *value = (pScrn->virtualX << 16) | pScrn->virtualY;
  } else if(attribute == pSiS->xv_BRR) {
     *value = pSiS->GammaBriR;
  } else if(attribute == pSiS->xv_BRG) {
     *value = pSiS->GammaBriG;
  } else if(attribute == pSiS->xv_BRB) {
     *value = pSiS->GammaBriB;
  } else if(attribute == pSiS->xv_PBR) {
     *value = pSiS->GammaPBriR;
  } else if(attribute == pSiS->xv_PBG) {
     *value = pSiS->GammaPBriG;
  } else if(attribute == pSiS->xv_PBB) {
     *value = pSiS->GammaPBriB;
  } else if(attribute == pSiS->xv_BRR2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaBriR;
     else
#endif
          *value = pSiS->GammaBriR;
  } else if(attribute == pSiS->xv_BRG2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaBriG;
     else
#endif
          *value = pSiS->GammaBriG;
  } else if(attribute == pSiS->xv_BRB2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaBriB;
     else
#endif
          *value = pSiS->GammaBriB;
  } else if(attribute == pSiS->xv_PBR2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaPBriR;
     else
#endif
          *value = pSiS->GammaPBriR;
  } else if(attribute == pSiS->xv_PBG2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaPBriG;
     else
#endif
          *value = pSiS->GammaPBriG;
  } else if(attribute == pSiS->xv_PBB2) {
#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) *value = pSiSEnt->GammaPBriB;
     else
#endif
          *value = pSiS->GammaPBriB;
  } else if(attribute == pSiS->xv_SHC) {
     *value = pSiS->HideHWCursor ? 1 : 0;
  } else if(attribute == pSiS->xv_PMD) {
     *value = 0;
     if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTSCALE) {
        switch(pSiS->SiS_Pr->UsePanelScaler) {
           case -1: *value |= 0x01; break;
           case 1:  *value |= 0x02; break;
        }
	if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTCENTER) {
           switch(pSiS->SiS_Pr->CenterScreen) {
              case -1: *value |= 0x04; break;
              case 1:  *value |= 0x08; break;
           }
	}
     }
#ifdef SIS_CP
  SIS_CP_VIDEO_GETATTRIBUTE
#endif
  } else if(pSiS->VGAEngine == SIS_315_VGA) {
     if(attribute == pSiS->xvSwitchCRT) {
#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode)
           *value = pSiSEnt->curxvcrtnum;
        else
#endif
           *value = pPriv->crtnum;
     } else if(attribute == pSiS->xvHue) {
        *value = pPriv->hue;
     } else if(attribute == pSiS->xvSaturation) {
        *value = pPriv->saturation;
     } else if(attribute == pSiS->xvGammaRed) {
        *value = pSiS->XvGammaRed;
     } else if(attribute == pSiS->xvGammaGreen) {
        *value = pSiS->XvGammaGreen;
     } else if(attribute == pSiS->xvGammaBlue) {
        *value = pSiS->XvGammaBlue;
     } else return BadMatch;
  } else return BadMatch;
  return Success;
}

#if 0 /* For future use */
static int
SiSHandleSiSDirectCommand(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv, sisdirectcommand *sdcbuf)
{
   SISPtr pSiS = SISPTR(pScrn);
   int i;
   unsigned long j;

   if(sdcbuf->sdc_id != SDC_ID) return BadMatch;

   j = sdcbuf->sdc_header;
   j += sdcbuf->sdc_command;
   for(i = 0; i < SDC_NUM_PARM; i++) {
      j += sdcbuf->sdc_parm[i];
   }
   if(j != sdcbuf->sdc_chksum) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "SiS Direct: Bad packet checksum\n");
    	return BadMatch;
   }
   sdcbuf->sdc_header = SDC_RESULT_OK;
   switch(sdcbuf->sdc_command) {
   case SDC_CMD_GETVERSION:
      sdcbuf->sdc_parm[0] = SDC_VERSION;
      break;
   case SDC_CMD_CHECKMODEFORCRT2:
      j = sdcbuf->sdc_parm[0];
      sdcbuf->sdc_parm[0] = SISCheckModeIndexForCRT2Type(pScrn,
      			(unsigned short)(j & 0xff),
	                (unsigned short)((j >> 8) & 0xff),
			FALSE) & 0xff;
      break;
   default:
      sdcbuf->sdc_header = SDC_RESULT_UNDEFCMD;
   }

   return Success;
}
#endif

static void
SISQueryBestSize(
  ScrnInfoPtr pScrn,
  Bool motion,
  short vid_w, short vid_h,
  short drw_w, short drw_h,
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h; 
}

static void
calc_scale_factor(SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                 SISPortPrivPtr pPriv, int index, int iscrt2)
{
  SISPtr pSiS = SISPTR(pScrn);
  CARD32 I=0,mult=0;
  int flag=0;

  int dstW = pOverlay->dstBox.x2 - pOverlay->dstBox.x1;
  int dstH = pOverlay->dstBox.y2 - pOverlay->dstBox.y1;
  int srcW = pOverlay->srcW;
  int srcH = pOverlay->srcH;
  CARD16 LCDheight = pSiS->LCDheight;
  int srcPitch = pOverlay->origPitch;
  int origdstH = dstH;
  int modeflags = pOverlay->currentmode->Flags;

  /* Stretch image due to panel link scaling */
  if(pSiS->VBFlags & (CRT2_LCD | CRT1_LCDA)) {
     if(pPriv->bridgeIsSlave) {
        if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH)) {
           if(pSiS->MiscFlags & MISC_PANELLINKSCALER) {
  	      dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
           }
	}
     } else if((iscrt2 && (pSiS->VBFlags & CRT2_LCD)) ||
               (!iscrt2 && (pSiS->VBFlags & CRT1_LCDA))) {
  	if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH | CRT1_LCDA)) {
	   if(pSiS->MiscFlags & MISC_PANELLINKSCALER) {
   	      dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
	      if(pPriv->displayMode == DISPMODE_MIRROR) flag = 1;
	   }
        }
     }
  }

  /* For double scan modes, we need to double the height
   * On 315 and 550 (?), we need to double the width as well.
   * Interlace mode vice versa.
   */
  if(modeflags & V_DBLSCAN) {
     dstH = origdstH << 1;
     flag = 0;
     if((pSiS->sishw_ext.jChipType >= SIS_315H) &&
	(pSiS->sishw_ext.jChipType <= SIS_550)) {
	dstW <<= 1;
     }
  }
  if(modeflags & V_INTERLACE) {
     dstH = origdstH >> 1;
     flag = 0;
  }

#if 0
  /* TEST @@@ */
  if(pOverlay->bobEnable & 0x08) dstH <<= 1;
#endif

  if(dstW < OVERLAY_MIN_WIDTH) dstW = OVERLAY_MIN_WIDTH;
  if(dstW == srcW) {
     pOverlay->HUSF   = 0x00;
     pOverlay->IntBit = 0x05;
     pOverlay->wHPre  = 0;
  } else if(dstW > srcW) {
     dstW += 2;
     pOverlay->HUSF   = (srcW << 16) / dstW;
     pOverlay->IntBit = 0x04;
     pOverlay->wHPre  = 0;
  } else {
     int tmpW = dstW;

     /* It seems, the hardware can't scale below factor .125 (=1/8) if the
        pitch isn't a multiple of 256.
	TODO: Test this on the 315 series!
      */
     if((srcPitch % 256) || (srcPitch < 256)) {
        if(((dstW * 1000) / srcW) < 125) dstW = tmpW = ((srcW * 125) / 1000) + 1;
     }

     I = 0;
     pOverlay->IntBit = 0x01;
     while(srcW >= tmpW) {
        tmpW <<= 1;
        I++;
     }
     pOverlay->wHPre = (CARD8)(I - 1);
     dstW <<= (I - 1);
     if((srcW % dstW))
        pOverlay->HUSF = ((srcW - dstW) << 16) / dstW;
     else
        pOverlay->HUSF = 0x00;
  }

  if(dstH < OVERLAY_MIN_HEIGHT) dstH = OVERLAY_MIN_HEIGHT;
  if(dstH == srcH) {
     pOverlay->VUSF   = 0x00;
     pOverlay->IntBit |= 0x0A;
  } else if(dstH > srcH) {
     dstH += 0x02;
     pOverlay->VUSF = (srcH << 16) / dstH;
     pOverlay->IntBit |= 0x08;
  } else {

     I = srcH / dstH;
     pOverlay->IntBit |= 0x02;

     if(I < 2) {
        pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
	/* TW: Needed for LCD-scaling modes */
	if((flag) && (mult = (srcH / origdstH)) >= 2) {
	   pOverlay->pitch /= mult;
	}
     } else {
#if 0
        if(((pOverlay->bobEnable & 0x08) == 0x00) &&
           (((srcPitch * I)>>2) > 0xFFF)){
           pOverlay->bobEnable |= 0x08;
           srcPitch >>= 1;
        }
#endif
        if(((srcPitch * I)>>2) > 0xFFF) {
           I = (0xFFF*2/srcPitch);
           pOverlay->VUSF = 0xFFFF;
        } else {
           dstH = I * dstH;
           if(srcH % dstH)
              pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
           else
              pOverlay->VUSF = 0x00;
        }
        /* set video frame buffer offset */
        pOverlay->pitch = (CARD16)(srcPitch*I);
     }
  }
}

#ifdef SISMERGED
static void
calc_scale_factor_2(SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                 SISPortPrivPtr pPriv, int index, int iscrt2)
{
  SISPtr pSiS = SISPTR(pScrn);
  CARD32 I=0,mult=0;
  int flag=0;

  int dstW = pOverlay->dstBox2.x2 - pOverlay->dstBox2.x1;
  int dstH = pOverlay->dstBox2.y2 - pOverlay->dstBox2.y1;
  int srcW = pOverlay->srcW2;
  int srcH = pOverlay->srcH2;
  CARD16 LCDheight = pSiS->LCDheight;
  int srcPitch = pOverlay->origPitch;
  int origdstH = dstH;
  int modeflags = pOverlay->currentmode2->Flags;

  /* Stretch image due to panel link scaling */
  if(pSiS->VBFlags & CRT2_LCD) {
     if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH)) {
	if(pSiS->MiscFlags & MISC_PANELLINKSCALER) {
   	   dstH = (dstH * LCDheight) / pOverlay->SCREENheight2;
	   flag = 1;
	}
     }
  }
  /* For double scan modes, we need to double the height
   * On 315 and 550 (?), we need to double the width as well.
   * Interlace mode vice versa.
   */
  if(modeflags & V_DBLSCAN) {
     dstH = origdstH << 1;
     flag = 0;
     if((pSiS->sishw_ext.jChipType >= SIS_315H) &&
	(pSiS->sishw_ext.jChipType <= SIS_550)) {
  	dstW <<= 1;
     }
  }
  if(modeflags & V_INTERLACE) {
     dstH = origdstH >> 1;
     flag = 0;
  }

#if 0
  /* TEST @@@ */
  if(pOverlay->bobEnable & 0x08) dstH <<= 1;
#endif

  if(dstW < OVERLAY_MIN_WIDTH) dstW = OVERLAY_MIN_WIDTH;
  if(dstW == srcW) {
     pOverlay->HUSF2   = 0x00;
     pOverlay->IntBit2 = 0x05;
     pOverlay->wHPre2  = 0;
  } else if(dstW > srcW) {
     dstW += 2;
     pOverlay->HUSF2   = (srcW << 16) / dstW;
     pOverlay->IntBit2 = 0x04;
     pOverlay->wHPre2  = 0;
  } else {
     int tmpW = dstW;

     /* It seems, the hardware can't scale below factor .125 (=1/8) if the
	pitch isn't a multiple of 256.
        TODO: Test this on the 315 series!
      */
     if((srcPitch % 256) || (srcPitch < 256)) {
	if(((dstW * 1000) / srcW) < 125) dstW = tmpW = ((srcW * 125) / 1000) + 1;
     }

     I = 0;
     pOverlay->IntBit2 = 0x01;
     while(srcW >= tmpW) {
        tmpW <<= 1;
        I++;
     }
     pOverlay->wHPre2 = (CARD8)(I - 1);
     dstW <<= (I - 1);
     if((srcW % dstW))
        pOverlay->HUSF2 = ((srcW - dstW) << 16) / dstW;
     else
        pOverlay->HUSF2 = 0x00;
  }

  if(dstH < OVERLAY_MIN_HEIGHT) dstH = OVERLAY_MIN_HEIGHT;
  if(dstH == srcH) {
     pOverlay->VUSF2   = 0x00;
     pOverlay->IntBit2 |= 0x0A;
  } else if(dstH > srcH) {
     dstH += 0x02;
     pOverlay->VUSF2 = (srcH << 16) / dstH;
     pOverlay->IntBit2 |= 0x08;
  } else {

     I = srcH / dstH;
     pOverlay->IntBit2 |= 0x02;

     if(I < 2) {
        pOverlay->VUSF2 = ((srcH - dstH) << 16) / dstH;
	/* Needed for LCD-scaling modes */
	if(flag && ((mult = (srcH / origdstH)) >= 2)) {
	   pOverlay->pitch2 /= mult;
	}
     } else {
#if 0
        if(((pOverlay->bobEnable & 0x08) == 0x00) &&
           (((srcPitch * I)>>2) > 0xFFF)){
           pOverlay->bobEnable |= 0x08;
           srcPitch >>= 1;
        }
#endif
        if(((srcPitch * I)>>2) > 0xFFF) {
           I = (0xFFF*2/srcPitch);
           pOverlay->VUSF2 = 0xFFFF;
        } else {
           dstH = I * dstH;
           if(srcH % dstH)
              pOverlay->VUSF2 = ((srcH - dstH) << 16) / dstH;
           else
              pOverlay->VUSF2 = 0x00;
        }
        /* set video frame buffer offset */
        pOverlay->pitch2 = (CARD16)(srcPitch*I);
     }
  }
}
#endif

static CARD8
calc_line_buf_size(CARD32 srcW, CARD8 wHPre, CARD32 pixelFormat)
{
    CARD8  preHIDF;
    CARD32 I;
    CARD32 line = srcW;

    if( (pixelFormat == PIXEL_FMT_YV12) ||
        (pixelFormat == PIXEL_FMT_I420) ||
	(pixelFormat == PIXEL_FMT_NV12) ||
	(pixelFormat == PIXEL_FMT_NV21) )
    {
        preHIDF = wHPre & 0x07;
        switch (preHIDF)
        {
            case 3 :
                if((line & 0xffffff00) == line)
                   I = (line >> 8);
                else
                   I = (line >> 8) + 1;
                return((CARD8)(I * 32 - 1));
            case 4 :
                if((line & 0xfffffe00) == line)
                   I = (line >> 9);
                else
                   I = (line >> 9) + 1;
                return((CARD8)(I * 64 - 1));
            case 5 :
                if((line & 0xfffffc00) == line)
                   I = (line >> 10);
                else
                   I = (line >> 10) + 1;
                return((CARD8)(I * 128 - 1));
            case 6 :
                return((CARD8)(255));
            default :
                if((line & 0xffffff80) == line)
                   I = (line >> 7);
                else
                   I = (line >> 7) + 1;
                return((CARD8)(I * 16 - 1));
        }
    } else { /* YUV2, UYVY */
        if((line & 0xffffff8) == line)
           I = (line >> 3);
        else
           I = (line >> 3) + 1;
        return((CARD8)(I - 1));
    }
}

static __inline void
set_line_buf_size_1(SISOverlayPtr pOverlay)
{
    pOverlay->lineBufSize = calc_line_buf_size(pOverlay->srcW,pOverlay->wHPre, pOverlay->pixelFormat);
}

#ifdef SISMERGED
static __inline void
set_line_buf_size_2(SISOverlayPtr pOverlay)
{
    pOverlay->lineBufSize2 = calc_line_buf_size(pOverlay->srcW2,pOverlay->wHPre2, pOverlay->pixelFormat);
}

static void
merge_line_buf_mfb(SISPtr pSiS, SISPortPrivPtr pPriv, Bool enable1, Bool enable2,
                   short width1, short width2, short limit)
{
  unsigned char misc1, misc2, mask = pPriv->linebufmask;

  if(pPriv->hasTwoOverlays) {     /* This means we are in MIRROR mode */

     misc2 = 0x00;
     if(enable1) misc1 = 0x04;
     else 	 misc1 = 0x00;
     setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
     setvideoregmask(pSiS, Index_VI_Control_Misc1, misc1, 0x04);

     misc2 = 0x01;
     if(enable2) misc1 = 0x04;
     else        misc1 = 0x00;
     setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
     setvideoregmask(pSiS, Index_VI_Control_Misc1, misc1, 0x04);

  } else {			/* This means we are either in SINGLE1 or SINGLE2 mode */

     misc2 = 0x00;
     if(enable1 || enable2) misc1 = 0x04;
     else                   misc1 = 0x00;

     setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
     setvideoregmask(pSiS, Index_VI_Control_Misc1, misc1, 0x04);

  }
}
#endif

/* About linebuffer merging:
 *
 * For example the 651:
 * Each overlay has 4 line buffers, 384 bytes each (<-- Is that really correct? 1920 / 384 = 5 !!!)
 * If the source width is greater than 384, line buffers must be merged.
 * Dual merge: Only O1 usable (uses overlay 2's linebuffer), maximum width 384*2
 * Individual merge: Both overlays available, maximum width 384*2
 * All merge: Only overlay 1 available, maximum width = 384*4 (<--- should be 1920, is 1536...)
 *
 *
 *        Normally:                  Dual merge:                 Individual merge
 *  Overlay 1    Overlay 2         Overlay 1 only!                Both overlays
 *  ___1___      ___5___           ___1___ ___2___ -\         O1  ___1___ ___2___
 *  ___2___      ___6___           ___3___ ___4___   \_ O 1   O1  ___3___ ___4___
 *  ___3___      ___7___	   ___5___ ___6___   /        O2  ___5___ ___6___
 *  ___4___      ___8___           ___7___ ___8___ -/         O2  ___7___ ___8___
 *
 *
 * All merge:          ___1___ ___2___ ___3___ ___4___
 * (Overlay 1 only!)   ___5___ ___6___ ___7___ ___8___
 *
 * Individual merge is supported on all chipsets.
 * Dual merge is only supported on the 300 series and M650/651 and later.
 * All merge is only supported on the M650/651 and later.
 *
 */


static void
merge_line_buf(SISPtr pSiS, SISPortPrivPtr pPriv, Bool enable, short width, short limit)
{
  unsigned char misc1, misc2, mask = pPriv->linebufmask;

  if(enable) { 		/* ----- enable linebuffer merge */

    switch(pPriv->displayMode){
    case DISPMODE_SINGLE1:
        if(pSiS->VGAEngine == SIS_300_VGA) {
           if(pPriv->dualHeadMode) {
	       misc2 = 0x00;
	       misc1 = 0x04;
	   } else {
	       misc2 = 0x10;
	       misc1 = 0x00;
	   }
        } else {
	   if(pPriv->hasTwoOverlays) {
	      if(pPriv->dualHeadMode) {
	         misc2 = 0x00;
		 misc1 = 0x04;
	      } else {
	         if(width > (limit * 2)) {
		    misc2 = 0x20;
		 } else {
	            misc2 = 0x10;
		 }
		 misc1 = 0x00;
	      }
	   } else {
	      misc2 = 0x00;
	      misc1 = 0x04;
	   }
	}
	setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
	setvideoregmask(pSiS, Index_VI_Control_Misc1, misc1, 0x04);
      	break;

    case DISPMODE_SINGLE2:
        if(pSiS->VGAEngine == SIS_300_VGA) {
	   if(pPriv->dualHeadMode) {
	      misc2 = 0x01;
	      misc1 = 0x04;
	   } else {
	      misc2 = 0x10;
	      misc1 = 0x00;
	   }
	} else {
	   if(pPriv->hasTwoOverlays) {
	      if(pPriv->dualHeadMode) {
	         misc2 = 0x01;
		 misc1 = 0x04;
	      } else {
	         if(width > (limit * 2)) {
		    misc2 = 0x20;
		 } else {
	            misc2 = 0x10;
		 }
		 misc1 = 0x00;
	      }
	   } else {
	      misc2 = 0x00;
	      misc1 = 0x04;
	   }
	}
	setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
	setvideoregmask(pSiS, Index_VI_Control_Misc1, misc1, 0x04);
     	break;

    case DISPMODE_MIRROR:   /* This can only be on chips with 2 overlays */
    default:
        setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, mask);
      	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, mask);
      	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x04, 0x04);
        break;
    }

  } else {  		/* ----- disable linebuffer merge */

    switch(pPriv->displayMode) {

    case DISPMODE_SINGLE1:
	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, mask);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
    	break;

    case DISPMODE_SINGLE2:
        if(pSiS->VGAEngine == SIS_300_VGA) {
	   if(pPriv->dualHeadMode) misc2 = 0x01;
	   else       		   misc2 = 0x00;
    	} else {
    	   if(pPriv->hasTwoOverlays) {
	      if(pPriv->dualHeadMode) misc2 = 0x01;
	      else                    misc2 = 0x00;
	   } else {
	      misc2 = 0x00;
	   }
	}
	setvideoregmask(pSiS, Index_VI_Control_Misc2, misc2, mask);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	break;

    case DISPMODE_MIRROR:   /* This can only be on chips with 2 overlays */
    default:
        setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, mask);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, mask);
    	setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x04);
        break;
    }
  }
}

static __inline void
set_format(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    CARD8 fmt;

    switch (pOverlay->pixelFormat){
    case PIXEL_FMT_YV12:
    case PIXEL_FMT_I420:
        fmt = 0x0c;
        break;
    case PIXEL_FMT_YUY2:
        fmt = 0x28;
        break;
    case PIXEL_FMT_UYVY:
        fmt = 0x08;
        break;
    case PIXEL_FMT_YVYU:
        fmt = 0x38;
	break;
    case PIXEL_FMT_NV12:
        fmt = 0x4c;
	break;
    case PIXEL_FMT_NV21:
        fmt = 0x5c;
	break;
    case PIXEL_FMT_RGB5:   /* D[5:4] : 00 RGB555, 01 RGB 565 */
        fmt = 0x00;
	break;
    case PIXEL_FMT_RGB6:
        fmt = 0x10;
	break;
    default:
        fmt = 0x00;
        break;
    }
    setvideoregmask(pSiS, Index_VI_Control_Misc0, fmt, 0xfc);
}

static __inline void
set_colorkey(SISPtr pSiS, CARD32 colorkey)
{
    CARD8 r, g, b;

    b = (CARD8)(colorkey & 0xFF);
    g = (CARD8)((colorkey>>8) & 0xFF);
    r = (CARD8)((colorkey>>16) & 0xFF);

    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Blue_Min  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Green_Min ,(CARD8)g);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Red_Min   ,(CARD8)r);

    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Blue_Max  ,(CARD8)b);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Green_Max ,(CARD8)g);
    setvideoreg(pSiS, Index_VI_Overlay_ColorKey_Red_Max   ,(CARD8)r);
}

static __inline void
set_chromakey(SISPtr pSiS, CARD32 chromamin, CARD32 chromamax)
{
    CARD8 r1, g1, b1;
    CARD8 r2, g2, b2;

    b1 = (CARD8)(chromamin & 0xFF);
    g1 = (CARD8)((chromamin>>8) & 0xFF);
    r1 = (CARD8)((chromamin>>16) & 0xFF);
    b2 = (CARD8)(chromamax & 0xFF);
    g2 = (CARD8)((chromamax>>8) & 0xFF);
    r2 = (CARD8)((chromamax>>16) & 0xFF);

    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Blue_V_Min  ,(CARD8)b1);
    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Green_U_Min ,(CARD8)g1);
    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Red_Y_Min   ,(CARD8)r1);

    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Blue_V_Max  ,(CARD8)b2);
    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Green_U_Max ,(CARD8)g2);
    setvideoreg(pSiS, Index_VI_Overlay_ChromaKey_Red_Y_Max   ,(CARD8)r2);
}

static __inline void
set_brightness(SISPtr pSiS, CARD8 brightness)
{
    setvideoreg(pSiS, Index_VI_Brightness, brightness);
}

static __inline void
set_contrast(SISPtr pSiS, CARD8 contrast)
{
    setvideoregmask(pSiS, Index_VI_Contrast_Enh_Ctrl, contrast, 0x07);
}

/* 315 series and later only */
static __inline void
set_saturation(SISPtr pSiS, short saturation)
{
    CARD8 temp = 0;

    if(saturation < 0) {
    	temp |= 0x88;
	saturation = -saturation;
    }
    temp |= (saturation & 0x07);
    temp |= ((saturation & 0x07) << 4);

    setvideoreg(pSiS, Index_VI_Saturation, temp);
}

/* 315 series and later only */
static __inline void
set_hue(SISPtr pSiS, CARD8 hue)
{
    setvideoregmask(pSiS, Index_VI_Hue, (hue & 0x08) ? (hue ^ 0x07) : hue, 0x0F);
}

static __inline void
set_disablegfx(SISPtr pSiS, Bool mybool, SISOverlayPtr pOverlay)
{
    /* This is not supported on M65x, 65x (x>0) or later */
    /* For CRT1 ONLY!!! */
    if((!(pSiS->ChipFlags & SiSCF_Is65x)) && (pSiS->Chipset != PCI_CHIP_SIS660)) {
       setvideoregmask(pSiS, Index_VI_Control_Misc2, mybool ? 0x04 : 0x00, 0x04);
       if(mybool) pOverlay->keyOP = VI_ROP_Always;
    }
}

static __inline void
set_disablegfxlr(SISPtr pSiS, Bool mybool, SISOverlayPtr pOverlay)
{
    setvideoregmask(pSiS, Index_VI_Control_Misc1, mybool ? 0x01 : 0x00, 0x01);
    if(mybool) pOverlay->keyOP = VI_ROP_Always;
}

#ifdef SIS_CP
    SIS_CP_VIDEO_SUBS
#endif

static void
set_overlay(SISPtr pSiS, SISOverlayPtr pOverlay, SISPortPrivPtr pPriv, int index, int iscrt2)
{
    ScrnInfoPtr pScrn = pSiS->pScrn;

    CARD16 pitch=0;
    CARD8  h_over=0, v_over=0;
    CARD16 top, bottom, left, right;
    CARD16 screenX, screenY;
    int    modeflags, watchdog;
    CARD8  data;
    CARD32 PSY;

#ifdef SISMERGED
    if(pSiS->MergedFB && iscrt2) {
       screenX = pOverlay->currentmode2->HDisplay;
       screenY = pOverlay->currentmode2->VDisplay;
       modeflags = pOverlay->currentmode2->Flags;
       top = pOverlay->dstBox2.y1;
       bottom = pOverlay->dstBox2.y2;
       left = pOverlay->dstBox2.x1;
       right = pOverlay->dstBox2.x2;
       pitch = pOverlay->pitch2 >> pPriv->shiftValue;
    } else {
#endif
       screenX = pOverlay->currentmode->HDisplay;
       screenY = pOverlay->currentmode->VDisplay;
       modeflags = pOverlay->currentmode->Flags;
       top = pOverlay->dstBox.y1;
       bottom = pOverlay->dstBox.y2;
       left = pOverlay->dstBox.x1;
       right = pOverlay->dstBox.x2;
       pitch = pOverlay->pitch >> pPriv->shiftValue;
#ifdef SISMERGED
    }
#endif

    if(bottom > screenY) {
        bottom = screenY;
    }
    if(right > screenX) {
        right = screenX;
    }

    /* DoubleScan modes require Y coordinates * 2 */
    if(modeflags & V_DBLSCAN) {
    	 top <<= 1;
	 bottom <<= 1;
    }
    /* Interlace modes require Y coordinates / 2 */
    if(modeflags & V_INTERLACE) {
    	 top >>= 1;
	 bottom >>= 1;
    }

    h_over = (((left>>8) & 0x0f) | ((right>>4) & 0xf0));
    v_over = (((top>>8) & 0x0f) | ((bottom>>4) & 0xf0));

    /* set line buffer size */
#ifdef SISMERGED
    if(pSiS->MergedFB && iscrt2)
       setvideoreg(pSiS, Index_VI_Line_Buffer_Size, pOverlay->lineBufSize2);
    else
#endif
       setvideoreg(pSiS, Index_VI_Line_Buffer_Size, pOverlay->lineBufSize);

    /* set color key mode */
    setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, pOverlay->keyOP, 0x0f);

    /* We don't have to wait for vertical retrace in all cases */
    if(pPriv->mustwait) {
        if((pSiS->VGAEngine == SIS_315_VGA) && (index)) {
	   /* overlay 2 needs special treatment */
	   setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
	}
	watchdog = WATCHDOG_DELAY;
    	while(pOverlay->VBlankActiveFunc(pSiS, pPriv) && --watchdog);
	watchdog = WATCHDOG_DELAY;
	while((!pOverlay->VBlankActiveFunc(pSiS, pPriv)) && --watchdog);
	if(!watchdog) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			"Xv: Waiting for vertical retrace timed-out\n");
    }

    /* Unlock address registers */
    data = getvideoreg(pSiS, Index_VI_Control_Misc1);
    setvideoreg(pSiS, Index_VI_Control_Misc1, data | 0x20);
    /* Is this required? */
    setvideoreg(pSiS, Index_VI_Control_Misc1, data | 0x20);

    /* Is this required? (seems so) */
    if((pSiS->Chipset == SIS_315_VGA) && !index)
       setvideoregmask(pSiS, Index_VI_Control_Misc3, 0x00, (1 << index));

    /* Set Y buf pitch */
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Pitch_Low, (CARD8)(pitch));
    setvideoregmask(pSiS, Index_VI_Disp_Y_UV_Buf_Pitch_Middle, (CARD8)(pitch >> 8), 0x0f);

    /* Set Y start address */
#ifdef SISMERGED
    if(pSiS->MergedFB && iscrt2) {
       PSY = pOverlay->PSY2;
    } else
#endif
       PSY = pOverlay->PSY;

    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Start_Low,    (CARD8)(PSY));
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Start_Middle, (CARD8)(PSY >> 8));
    setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Start_High,   (CARD8)(PSY >> 16));

    /* set 315 series overflow bits for Y plane */
    if(pSiS->VGAEngine == SIS_315_VGA) {
        setvideoreg(pSiS, Index_VI_Disp_Y_Buf_Pitch_High, (CARD8)(pitch >> 12));
    	setvideoreg(pSiS, Index_VI_Y_Buf_Start_Over, ((CARD8)(PSY >> 24) & 0x03));
    }

    /* Set U/V data if using planar formats */
    if( (pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
    	(pOverlay->pixelFormat == PIXEL_FMT_I420) ||
	(pOverlay->pixelFormat == PIXEL_FMT_NV12) ||
	(pOverlay->pixelFormat == PIXEL_FMT_NV21) )  {

        CARD32  PSU=0, PSV=0, uvpitch = pitch;

        PSU = pOverlay->PSU;
        PSV = pOverlay->PSV;
#ifdef SISMERGED
        if(pSiS->MergedFB && iscrt2) {
	   PSU = pOverlay->PSU2;
           PSV = pOverlay->PSV2;
	}
#endif
        if((pOverlay->pixelFormat == PIXEL_FMT_YV12) ||
    	   (pOverlay->pixelFormat == PIXEL_FMT_I420)) {
	   uvpitch >>= 1;
	}

	/* Set U/V pitch */
	setvideoreg (pSiS, Index_VI_Disp_UV_Buf_Pitch_Low, (CARD8)uvpitch);
        setvideoregmask (pSiS, Index_VI_Disp_Y_UV_Buf_Pitch_Middle, (CARD8)(uvpitch >> 4), 0xf0);

        /* set U/V start address */
        setvideoreg (pSiS, Index_VI_U_Buf_Start_Low,   (CARD8)PSU);
        setvideoreg (pSiS, Index_VI_U_Buf_Start_Middle,(CARD8)(PSU >> 8));
        setvideoreg (pSiS, Index_VI_U_Buf_Start_High,  (CARD8)(PSU >> 16));

        setvideoreg (pSiS, Index_VI_V_Buf_Start_Low,   (CARD8)PSV);
        setvideoreg (pSiS, Index_VI_V_Buf_Start_Middle,(CARD8)(PSV >> 8));
        setvideoreg (pSiS, Index_VI_V_Buf_Start_High,  (CARD8)(PSV >> 16));

	/* 315 series overflow bits */
	if(pSiS->VGAEngine == SIS_315_VGA) {
	   setvideoreg (pSiS, Index_VI_Disp_UV_Buf_Pitch_High, (CARD8)(uvpitch >> 12));
	   setvideoreg (pSiS, Index_VI_U_Buf_Start_Over, ((CARD8)(PSU >> 24) & 0x03));
	   if(pSiS->sishw_ext.jChipType == SIS_661) {
	      setvideoregmask (pSiS, Index_VI_V_Buf_Start_Over, ((CARD8)(PSV >> 24) & 0x03), 0xc3);
	   } else {
	      setvideoreg (pSiS, Index_VI_V_Buf_Start_Over, ((CARD8)(PSV >> 24) & 0x03));
	   }
	}
    }

    /* set scale factor */
#ifdef SISMERGED
    if(pSiS->MergedFB && iscrt2) {
       setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_Low, (CARD8)(pOverlay->HUSF2));
       setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_High,(CARD8)((pOverlay->HUSF2) >> 8));
       setvideoreg (pSiS, Index_VI_Ver_Up_Scale_Low,      (CARD8)(pOverlay->VUSF2));
       setvideoreg (pSiS, Index_VI_Ver_Up_Scale_High,     (CARD8)((pOverlay->VUSF2) >> 8));

       setvideoregmask (pSiS, Index_VI_Scale_Control,     (pOverlay->IntBit2 << 3)
                                                         |(pOverlay->wHPre2), 0x7f);
    } else {
#endif
       setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_Low, (CARD8)(pOverlay->HUSF));
       setvideoreg (pSiS, Index_VI_Hor_Post_Up_Scale_High,(CARD8)((pOverlay->HUSF) >> 8));
       setvideoreg (pSiS, Index_VI_Ver_Up_Scale_Low,      (CARD8)(pOverlay->VUSF));
       setvideoreg (pSiS, Index_VI_Ver_Up_Scale_High,     (CARD8)((pOverlay->VUSF)>>8));

       setvideoregmask (pSiS, Index_VI_Scale_Control,     (pOverlay->IntBit << 3)
                                                         |(pOverlay->wHPre), 0x7f);
#ifdef SISMERGED
    }
#endif

    if((pSiS->VGAEngine == SIS_315_VGA) && (index)){
       /* Trigger register copy for 315/330 series */
       /* setvideoreg(pSiS, Index_VI_Control_Misc3, (1 << index)); */
       setvideoregmask(pSiS, Index_VI_Control_Misc3, (1 << index), (1 << index)); 
    }

    /* set destination window position */
    setvideoreg(pSiS, Index_VI_Win_Hor_Disp_Start_Low, (CARD8)left);
    setvideoreg(pSiS, Index_VI_Win_Hor_Disp_End_Low,   (CARD8)right);
    setvideoreg(pSiS, Index_VI_Win_Hor_Over,           (CARD8)h_over);

    setvideoreg(pSiS, Index_VI_Win_Ver_Disp_Start_Low, (CARD8)top);
    setvideoreg(pSiS, Index_VI_Win_Ver_Disp_End_Low,   (CARD8)bottom);
    setvideoreg(pSiS, Index_VI_Win_Ver_Over,           (CARD8)v_over);

    setvideoregmask(pSiS, Index_VI_Control_Misc1, pOverlay->bobEnable, 0x1a);

    /* Lock the address registers */
    setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x20);
}

/* Overlay MUST NOT be switched off while beam is over it */
static void
close_overlay(SISPtr pSiS, SISPortPrivPtr pPriv)
{
  CARD32 watchdog;

  if(!(pPriv->overlayStatus)) return;
  pPriv->overlayStatus = FALSE;

  if(pPriv->displayMode & (DISPMODE_MIRROR | DISPMODE_SINGLE2)) {

     /* CRT2: MIRROR or SINGLE2
      * 1 overlay:  Uses overlay 0
      * 2 overlays: Uses Overlay 1 if MIRROR or DUAL HEAD
      *             Uses Overlay 0 if SINGLE2 and not DUAL HEAD
      */

     if(pPriv->hasTwoOverlays) {

        if((pPriv->dualHeadMode) || (pPriv->displayMode == DISPMODE_MIRROR)) {
     	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x01);
	} else {
	   setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x01);
	}

     } else if(pPriv->displayMode == DISPMODE_SINGLE2) {

#ifdef SISDUALHEAD
        if(pPriv->dualHeadMode) {
	   /* Check if overlay already grabbed by other head */
	   if(!(getsrreg(pSiS, 0x06) & 0x40)) return;
	}
#endif
      	setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x01);

     }

     setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT2(pSiS, pPriv) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT2(pSiS, pPriv)) && --watchdog);
     setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT2(pSiS, pPriv) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT2(pSiS, pPriv)) && --watchdog);

#ifdef SIS_CP
     SIS_CP_RESET_CP
#endif

  }

  if(pPriv->displayMode & (DISPMODE_SINGLE1 | DISPMODE_MIRROR)) {

     /* CRT1: Always uses overlay 0
      */

#ifdef SISDUALHEAD
     if(pPriv->dualHeadMode) {
        if(!pPriv->hasTwoOverlays) {
	   /* Check if overlay already grabbed by other head */
	   if(getsrreg(pSiS, 0x06) & 0x40) return;
	}
     }
#endif	
     setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x05);
     setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT1(pSiS, pPriv) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT1(pSiS, pPriv)) && --watchdog);
     setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
     watchdog = WATCHDOG_DELAY;
     while(vblank_active_CRT1(pSiS, pPriv) && --watchdog);
     watchdog = WATCHDOG_DELAY;
     while((!vblank_active_CRT1(pSiS, pPriv)) && --watchdog);

  }
}

static void
SISDisplayVideo(ScrnInfoPtr pScrn, SISPortPrivPtr pPriv)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif   
   short srcPitch = pPriv->srcPitch;
   short height = pPriv->height;
   unsigned short screenwidth;
   SISOverlayRec overlay; 
   int srcOffsetX=0, srcOffsetY=0;
   int sx=0, sy=0;
   int index = 0, iscrt2 = 0;
#ifdef SISMERGED
   unsigned char temp;
   unsigned short screen2width=0;
   int srcOffsetX2=0, srcOffsetY2=0;
   int sx2=0, sy2=0, watchdog;
#endif
   
   pPriv->NoOverlay = FALSE;
#ifdef SISDUALHEAD
   if(pPriv->dualHeadMode) {
      if(!pPriv->hasTwoOverlays) {
         if(pSiS->SecondHead) {
	    if(pSiSEnt->curxvcrtnum != 0) {
	       if(pPriv->overlayStatus) {
	          close_overlay(pSiS, pPriv);
	       }  
	       pPriv->NoOverlay = TRUE;
	       return;
	    }
         } else {
	    if(pSiSEnt->curxvcrtnum != 1) {
	       if(pPriv->overlayStatus) {
	          close_overlay(pSiS, pPriv);
	       }  
	       pPriv->NoOverlay = TRUE;
	       return;
	    }
	 }
      }
   }
#endif
   
   /* setup dispmode (MIRROR, SINGLEx) */
   set_dispmode(pScrn, pPriv);

   /* Check if overlay is supported with current mode */
#ifdef SISMERGED
   if(!pSiS->MergedFB) {
#endif
      if(pPriv->displayMode & (DISPMODE_SINGLE1 | DISPMODE_MIRROR)) {
         if(!(pSiS->MiscFlags & MISC_CRT1OVERLAY)) {
            if(pPriv->overlayStatus) {
	       close_overlay(pSiS, pPriv);
	    }
	    pPriv->NoOverlay = TRUE;
	    return;
         }
      }
#ifdef SISMERGED
   }
#endif

   memset(&overlay, 0, sizeof(overlay));

   overlay.pixelFormat = pPriv->id;
   overlay.pitch = overlay.origPitch = srcPitch;
   if(pPriv->usechromakey) {
      overlay.keyOP = (pPriv->insidechromakey) ? VI_ROP_ChromaKey : VI_ROP_NotChromaKey;
   } else {
      overlay.keyOP = VI_ROP_DestKey;
   }
   /* overlay.bobEnable = 0x02; */
   overlay.bobEnable = 0x00;    /* Disable BOB de-interlacer */

#ifdef SISMERGED
   if(pSiS->MergedFB) {
      overlay.DoFirst = TRUE;
      overlay.DoSecond = TRUE;
      overlay.pitch2 = overlay.origPitch;
      overlay.currentmode = ((SiSMergedDisplayModePtr)pSiS->CurrentLayout.mode->Private)->CRT1;
      overlay.currentmode2 = ((SiSMergedDisplayModePtr)pSiS->CurrentLayout.mode->Private)->CRT2;
      overlay.SCREENheight  = overlay.currentmode->VDisplay;
      overlay.SCREENheight2 = overlay.currentmode2->VDisplay;
      screenwidth = overlay.currentmode->HDisplay;
      screen2width = overlay.currentmode2->HDisplay;
      overlay.dstBox.x1  = pPriv->drw_x - pSiS->CRT1frameX0;
      overlay.dstBox.x2  = overlay.dstBox.x1 + pPriv->drw_w;
      overlay.dstBox.y1  = pPriv->drw_y - pSiS->CRT1frameY0;
      overlay.dstBox.y2  = overlay.dstBox.y1 + pPriv->drw_h;
      overlay.dstBox2.x1 = pPriv->drw_x - pSiS->CRT2pScrn->frameX0;
      overlay.dstBox2.x2 = overlay.dstBox2.x1 + pPriv->drw_w;
      overlay.dstBox2.y1 = pPriv->drw_y - pSiS->CRT2pScrn->frameY0;
      overlay.dstBox2.y2 = overlay.dstBox2.y1 + pPriv->drw_h;
      /* xf86DrvMsg(0, X_INFO, "DV(1): %d %d %d %d  | %d %d %d %d\n",
         overlay.dstBox.x1,overlay.dstBox.x2,overlay.dstBox.y1,overlay.dstBox.y2,
         overlay.dstBox2.x1,overlay.dstBox2.x2,overlay.dstBox2.y1,overlay.dstBox2.y2); */
   } else {
#endif
      overlay.currentmode = pSiS->CurrentLayout.mode;
      overlay.SCREENheight = overlay.currentmode->VDisplay;
      screenwidth = overlay.currentmode->HDisplay;
      overlay.dstBox.x1 = pPriv->drw_x - pScrn->frameX0;
      overlay.dstBox.x2 = pPriv->drw_x + pPriv->drw_w - pScrn->frameX0;
      overlay.dstBox.y1 = pPriv->drw_y - pScrn->frameY0;
      overlay.dstBox.y2 = pPriv->drw_y + pPriv->drw_h - pScrn->frameY0;
      /* xf86DrvMsg(0, X_INFO, "DV(1): %d %d %d %d\n",
         overlay.dstBox.x1,overlay.dstBox.x2,overlay.dstBox.y1,overlay.dstBox.y2); */
#ifdef SISMERGED
   }
#endif

   /* Note: x2/y2 is actually real coordinate + 1 */

   if((overlay.dstBox.x1 >= overlay.dstBox.x2) ||
      (overlay.dstBox.y1 >= overlay.dstBox.y2)) {
#ifdef SISMERGED
      if(pSiS->MergedFB) overlay.DoFirst = FALSE;
      else
#endif
           return;
   }

   if((overlay.dstBox.x2 <= 0) || (overlay.dstBox.y2 <= 0)) {
#ifdef SISMERGED
      if(pSiS->MergedFB) overlay.DoFirst = FALSE;
      else
#endif
           return;
   }

   if((overlay.dstBox.x1 >= screenwidth) || (overlay.dstBox.y1 >= overlay.SCREENheight)) {
#ifdef SISMERGED
      if(pSiS->MergedFB) overlay.DoFirst = FALSE;
      else
#endif
           return;
   }

#ifdef SISMERGED
   if(pSiS->MergedFB) {
      /* Check if dotclock is within limits for CRT1 */
      if(pPriv->displayMode & (DISPMODE_SINGLE1 | DISPMODE_MIRROR)) {
         if(!(pSiS->MiscFlags & MISC_CRT1OVERLAY)) {
            overlay.DoFirst = FALSE;
         }
      }
   }
#endif

   if(overlay.dstBox.x1 < 0) {
      srcOffsetX = pPriv->src_w * (-overlay.dstBox.x1) / pPriv->drw_w;
      overlay.dstBox.x1 = 0;
   }
   if(overlay.dstBox.y1 < 0) {
      srcOffsetY = pPriv->src_h * (-overlay.dstBox.y1) / pPriv->drw_h;
      overlay.dstBox.y1 = 0;
   }

#ifdef SISMERGED
   if(pSiS->MergedFB) {
      if((overlay.dstBox2.x1 >= overlay.dstBox2.x2) ||
         (overlay.dstBox2.y1 >= overlay.dstBox2.y2))
	 overlay.DoSecond = FALSE;

      if((overlay.dstBox2.x2 <= 0) || (overlay.dstBox2.y2 <= 0))
         overlay.DoSecond = FALSE;

      if((overlay.dstBox2.x1 >= screen2width) || (overlay.dstBox2.y1 >= overlay.SCREENheight2))
 	 overlay.DoSecond = FALSE;

      if(overlay.dstBox2.x1 < 0) {
         srcOffsetX2 = pPriv->src_w * (-overlay.dstBox2.x1) / pPriv->drw_w;
         overlay.dstBox2.x1 = 0;
      }
      if(overlay.dstBox2.y1 < 0) {
         srcOffsetY2 = pPriv->src_h * (-overlay.dstBox2.y1) / pPriv->drw_h;
         overlay.dstBox2.y1 = 0;
      }

      /* If neither overlay is to be displayed, disable them if they are currently enabled */
      if((!overlay.DoFirst) && (!overlay.DoSecond)) {
	 setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x05);
         setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
	 temp = getvideoreg(pSiS,Index_VI_Control_Misc0);
	 if(temp & 0x02) {
	    watchdog = WATCHDOG_DELAY;
	    if(pPriv->hasTwoOverlays) {
     	       while(vblank_active_CRT1(pSiS, pPriv) && --watchdog);
     	       watchdog = WATCHDOG_DELAY;
     	       while((!vblank_active_CRT1(pSiS, pPriv)) && --watchdog);
	    } else {
	       temp = getsrreg(pSiS, 0x06);
	       if(!(temp & 0x40)) {
     	          while(vblank_active_CRT1(pSiS, pPriv) && --watchdog);
     	          watchdog = WATCHDOG_DELAY;
     	          while((!vblank_active_CRT1(pSiS, pPriv)) && --watchdog);
	       } else {
	          while(vblank_active_CRT2(pSiS, pPriv) && --watchdog);
     	          watchdog = WATCHDOG_DELAY;
     	          while((!vblank_active_CRT2(pSiS, pPriv)) && --watchdog);
	       }
	    }
     	    setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
	 }
	 if(pPriv->hasTwoOverlays) {
            setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x01);
            setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
	    temp = getvideoreg(pSiS,Index_VI_Control_Misc0);
	    if(temp & 0x02) {
	       watchdog = WATCHDOG_DELAY;
     	       while(vblank_active_CRT2(pSiS, pPriv) && --watchdog);
     	       watchdog = WATCHDOG_DELAY;
     	       while((!vblank_active_CRT2(pSiS, pPriv)) && --watchdog);
     	       setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
	    }
         }
	 pPriv->overlayStatus = FALSE;
         return;
      }
   }
#endif

   switch(pPriv->id) {

     case PIXEL_FMT_YV12:
#ifdef SISMERGED
       if((!pSiS->MergedFB) || (overlay.DoFirst)) {
#endif
          sx = (pPriv->src_x + srcOffsetX) & ~7;
          sy = (pPriv->src_y + srcOffsetY) & ~1;
          overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy*srcPitch;
          overlay.PSV = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx + sy*srcPitch/2) >> 1);
          overlay.PSU = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx + sy*srcPitch/2) >> 1);
#ifdef SISDUALHEAD
          overlay.PSY += HEADOFFSET;
          overlay.PSV += HEADOFFSET;
          overlay.PSU += HEADOFFSET;
#endif
          overlay.PSY >>= pPriv->shiftValue;
          overlay.PSV >>= pPriv->shiftValue;
          overlay.PSU >>= pPriv->shiftValue;
#ifdef SISMERGED
       }
       if((pSiS->MergedFB) && (overlay.DoSecond)) {
          sx2 = (pPriv->src_x + srcOffsetX2) & ~7;
          sy2 = (pPriv->src_y + srcOffsetY2) & ~1;
          overlay.PSY2 = pPriv->bufAddr[pPriv->currentBuf] + sx2 + sy2*srcPitch;
          overlay.PSV2 = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx2 + sy2*srcPitch/2) >> 1);
          overlay.PSU2 = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx2 + sy2*srcPitch/2) >> 1);
          overlay.PSY2 >>= pPriv->shiftValue;
          overlay.PSV2 >>= pPriv->shiftValue;
          overlay.PSU2 >>= pPriv->shiftValue;
       }
#endif
       break;

     case PIXEL_FMT_I420:
#ifdef SISMERGED
       if((!pSiS->MergedFB) || (overlay.DoFirst)) {
#endif
          sx = (pPriv->src_x + srcOffsetX) & ~7;
          sy = (pPriv->src_y + srcOffsetY) & ~1;
          overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy*srcPitch;
          overlay.PSV = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx + sy*srcPitch/2) >> 1);
          overlay.PSU = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx + sy*srcPitch/2) >> 1);
#ifdef SISDUALHEAD
          overlay.PSY += HEADOFFSET;
          overlay.PSV += HEADOFFSET;
          overlay.PSU += HEADOFFSET;
#endif
          overlay.PSY >>= pPriv->shiftValue;
          overlay.PSV >>= pPriv->shiftValue;
          overlay.PSU >>= pPriv->shiftValue;
#ifdef SISMERGED
       }
       if((pSiS->MergedFB) && (overlay.DoSecond)) {
          sx2 = (pPriv->src_x + srcOffsetX2) & ~7;
          sy2 = (pPriv->src_y + srcOffsetY2) & ~1;
          overlay.PSY2 = pPriv->bufAddr[pPriv->currentBuf] + sx2 + sy2*srcPitch;
          overlay.PSV2 = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch*5/4 + ((sx2 + sy2*srcPitch/2) >> 1);
          overlay.PSU2 = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx2 + sy2*srcPitch/2) >> 1);
          overlay.PSY2 >>= pPriv->shiftValue;
          overlay.PSV2 >>= pPriv->shiftValue;
          overlay.PSU2 >>= pPriv->shiftValue;
       }
#endif
       break;

     case PIXEL_FMT_NV12:
     case PIXEL_FMT_NV21:
#ifdef SISMERGED
       if((!pSiS->MergedFB) || (overlay.DoFirst)) {
#endif
          sx = (pPriv->src_x + srcOffsetX) & ~7;
          sy = (pPriv->src_y + srcOffsetY) & ~1;
          overlay.PSY = pPriv->bufAddr[pPriv->currentBuf] + sx + sy*srcPitch;
          overlay.PSV =	pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx + sy*srcPitch/2) >> 1);
#ifdef SISDUALHEAD
          overlay.PSY += HEADOFFSET;
          overlay.PSV += HEADOFFSET;
#endif
          overlay.PSY >>= pPriv->shiftValue;
          overlay.PSV >>= pPriv->shiftValue;
          overlay.PSU = overlay.PSV; 
#ifdef SISMERGED
       }
       if((pSiS->MergedFB) && (overlay.DoSecond)) {
          sx2 = (pPriv->src_x + srcOffsetX2) & ~7;
          sy2 = (pPriv->src_y + srcOffsetY2) & ~1;
          overlay.PSY2 = pPriv->bufAddr[pPriv->currentBuf] + sx2 + sy2*srcPitch;
          overlay.PSV2 = pPriv->bufAddr[pPriv->currentBuf] + height*srcPitch + ((sx2 + sy2*srcPitch/2) >> 1);
          overlay.PSY2 >>= pPriv->shiftValue;
          overlay.PSV2 >>= pPriv->shiftValue;
          overlay.PSU2 = overlay.PSV2;
       }
#endif
       break;

     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_YVYU:
     case PIXEL_FMT_RGB6:
     case PIXEL_FMT_RGB5:
     default:
#ifdef SISMERGED
       if((!pSiS->MergedFB) || (overlay.DoFirst)) {
#endif
          sx = (pPriv->src_x + srcOffsetX) & ~1;
          sy = (pPriv->src_y + srcOffsetY);
          overlay.PSY = (pPriv->bufAddr[pPriv->currentBuf] + sx*2 + sy*srcPitch);
#ifdef SISDUALHEAD
          overlay.PSY += HEADOFFSET;
#endif
          overlay.PSY >>= pPriv->shiftValue;
#ifdef SISMERGED
       }
       if((pSiS->MergedFB) && (overlay.DoSecond)) {
          sx2 = (pPriv->src_x + srcOffsetX2) & ~1;
          sy2 = (pPriv->src_y + srcOffsetY2);
          overlay.PSY2 = (pPriv->bufAddr[pPriv->currentBuf] + sx2*2 + sy2*srcPitch);
          overlay.PSY2 >>= pPriv->shiftValue;
       }
#endif
       break;
   }

   /* Some clipping checks */
#ifdef SISMERGED
   if((!pSiS->MergedFB) || (overlay.DoFirst)) {
#endif
      overlay.srcW = pPriv->src_w - (sx - pPriv->src_x);
      overlay.srcH = pPriv->src_h - (sy - pPriv->src_y);
      if( (pPriv->oldx1 != overlay.dstBox.x1) ||
   	  (pPriv->oldx2 != overlay.dstBox.x2) ||
	  (pPriv->oldy1 != overlay.dstBox.y1) ||
	  (pPriv->oldy2 != overlay.dstBox.y2) ) {
	 pPriv->mustwait = 1;
	 pPriv->oldx1 = overlay.dstBox.x1; pPriv->oldx2 = overlay.dstBox.x2;
	 pPriv->oldy1 = overlay.dstBox.y1; pPriv->oldy2 = overlay.dstBox.y2;

      }
#ifdef SISMERGED
   }
   if((pSiS->MergedFB) && (overlay.DoSecond)) {
      overlay.srcW2 = pPriv->src_w - (sx2 - pPriv->src_x);
      overlay.srcH2 = pPriv->src_h - (sy2 - pPriv->src_y);
      if( (pPriv->oldx1_2 != overlay.dstBox2.x1) ||
   	  (pPriv->oldx2_2 != overlay.dstBox2.x2) ||
	  (pPriv->oldy1_2 != overlay.dstBox2.y1) ||
	  (pPriv->oldy2_2 != overlay.dstBox2.y2) ) {
	 pPriv->mustwait = 1;
	 pPriv->oldx1_2 = overlay.dstBox2.x1; pPriv->oldx2_2 = overlay.dstBox2.x2;
	 pPriv->oldy1_2 = overlay.dstBox2.y1; pPriv->oldy2_2 = overlay.dstBox2.y2;
      }
   }
#endif

#ifdef SISMERGED
   /* Disable an overlay if it is not to be displayed (but enabled currently) */
   if((pSiS->MergedFB) && (pPriv->hasTwoOverlays)) {
      if(!overlay.DoFirst) {
	 setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x00, 0x05);
         setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
	 temp = getvideoreg(pSiS,Index_VI_Control_Misc0);
	 if(temp & 0x02) {
	    watchdog = WATCHDOG_DELAY;
     	    while(vblank_active_CRT1(pSiS, pPriv) && --watchdog);
     	    watchdog = WATCHDOG_DELAY;
     	    while((!vblank_active_CRT1(pSiS, pPriv)) && --watchdog);
     	    setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
	 }
      } else if(!overlay.DoSecond) {
         setvideoregmask(pSiS, Index_VI_Control_Misc2, 0x01, 0x01);
         setvideoregmask(pSiS, Index_VI_Control_Misc1, 0x00, 0x01);
	 temp = getvideoreg(pSiS,Index_VI_Control_Misc0);
	 if(temp & 0x02) {
	    watchdog = WATCHDOG_DELAY;
     	    while(vblank_active_CRT2(pSiS, pPriv) && --watchdog);
     	    watchdog = WATCHDOG_DELAY;
     	    while((!vblank_active_CRT2(pSiS, pPriv)) && --watchdog);
     	    setvideoregmask(pSiS, Index_VI_Control_Misc0, 0x00, 0x02);
	 }
      }
   }
#endif

   /* Loop head */
   if(pPriv->displayMode & DISPMODE_SINGLE2) {
      if(pPriv->hasTwoOverlays) {    			/* We have 2 overlays: */
         if(pPriv->dualHeadMode) {
	    /* Dual head: We use overlay 2 for CRT2 */
      	    index = 1; iscrt2 = 1;
	 } else {
	    /* Single head: We use overlay 1 for CRT2 */
	    index = 0; iscrt2 = 1;
	 }
      } else {			     			/* We have 1 overlay */
         /* We use that only overlay for CRT2 */
         index = 0; iscrt2 = 1;
      }
      overlay.VBlankActiveFunc = vblank_active_CRT2;
#ifdef SISMERGED
      if(!pPriv->hasTwoOverlays) {
         if((pSiS->MergedFB) && (!overlay.DoSecond)) {
	    index = 0; iscrt2 = 0;
            overlay.VBlankActiveFunc = vblank_active_CRT1;
	    pPriv->displayMode = DISPMODE_SINGLE1;
	 }
      }
#endif
   } else {
      index = 0; iscrt2 = 0;
      overlay.VBlankActiveFunc = vblank_active_CRT1;
#ifdef SISMERGED
      if((pSiS->MergedFB) && (!overlay.DoFirst)) {
         if(pPriv->hasTwoOverlays) index = 1;
         iscrt2 = 1;
	 overlay.VBlankActiveFunc = vblank_active_CRT2;
	 if(!pPriv->hasTwoOverlays) {
	    pPriv->displayMode = DISPMODE_SINGLE2;
	 }
      }
#endif
   }

   /* set display mode SR06,32 (CRT1, CRT2 or mirror) */
   set_disptype_regs(pScrn, pPriv);

   /* set (not only calc) merge line buffer */
#ifdef SISMERGED
   if(!pSiS->MergedFB) {
#endif
      merge_line_buf(pSiS, pPriv, (overlay.srcW > pPriv->linebufMergeLimit), overlay.srcW,
      		     pPriv->linebufMergeLimit);
#ifdef SISMERGED
   } else {
      Bool temp1 = FALSE, temp2 = FALSE;
      if(overlay.DoFirst) {
         if(overlay.srcW > pPriv->linebufMergeLimit)  temp1 = TRUE;
      }
      if(overlay.DoSecond) {
         if(overlay.srcW2 > pPriv->linebufMergeLimit) temp2 = TRUE;
      }
      merge_line_buf_mfb(pSiS, pPriv, temp1, temp2, overlay.srcW, overlay.srcW2, pPriv->linebufMergeLimit);
   }
#endif

   /* calculate (not set!) line buffer length */
#ifdef SISMERGED
   if((!pSiS->MergedFB) || (overlay.DoFirst))
#endif
      set_line_buf_size_1(&overlay);
#ifdef SISMERGED
   if((pSiS->MergedFB) && (overlay.DoSecond))
      set_line_buf_size_2(&overlay);
#endif

   /* Do the following in a loop for CRT1 and CRT2 ----------------- */
MIRROR:

   /* calculate (not set!) scale factor */
#ifdef SISMERGED
   if(pSiS->MergedFB && iscrt2)
      calc_scale_factor_2(&overlay, pScrn, pPriv, index, iscrt2);
   else
#endif
      calc_scale_factor(&overlay, pScrn, pPriv, index, iscrt2);

   /* Select overlay 1 (used for CRT1/or CRT2) or overlay 2 (used for CRT2) */
   setvideoregmask(pSiS, Index_VI_Control_Misc2, index, 0x01);

   /* set format */
   set_format(pSiS, &overlay);

   /* set color key */
   set_colorkey(pSiS, pPriv->colorKey);

   if(pPriv->usechromakey) {
      /* Select chroma key format (300 series only) */
      if(pSiS->VGAEngine == SIS_300_VGA) {
	 setvideoregmask(pSiS, Index_VI_Control_Misc0,
	                 (pPriv->yuvchromakey ? 0x40 : 0x00), 0x40);
      }
      set_chromakey(pSiS, pPriv->chromamin, pPriv->chromamax);
   }

   /* set brightness, contrast, hue, saturation */
   set_brightness(pSiS, pPriv->brightness);
   set_contrast(pSiS, pPriv->contrast);
   if(pSiS->VGAEngine == SIS_315_VGA) {
      set_hue(pSiS, pPriv->hue);
      set_saturation(pSiS, pPriv->saturation);
   }

   if(pPriv->dualHeadMode) {
#ifdef SISDUALHEAD
      if(!pSiS->SecondHead) {
         if(pPriv->updatetvxpos) {
            SiS_SetTVxposoffset(pScrn, pPriv->tvxpos);
            pPriv->updatetvxpos = FALSE;
         }
         if(pPriv->updatetvypos) {
            SiS_SetTVyposoffset(pScrn, pPriv->tvypos);
            pPriv->updatetvypos = FALSE;
         }
      }
#endif
   } else {
      if(pPriv->updatetvxpos) {
         SiS_SetTVxposoffset(pScrn, pPriv->tvxpos);
         pPriv->updatetvxpos = FALSE;
      }
      if(pPriv->updatetvypos) {
         SiS_SetTVyposoffset(pScrn, pPriv->tvypos);
         pPriv->updatetvypos = FALSE;
      }
   }

   /* enable/disable graphics display around overlay
    * (Since disabled overlays don't get treated in this
    * loop, we omit respective checks here)
    */

   if(!iscrt2) set_disablegfx(pSiS, pPriv->disablegfx, &overlay);
   else if(!pPriv->hasTwoOverlays) {
     set_disablegfx(pSiS, FALSE, &overlay);
   }
   set_disablegfxlr(pSiS, pPriv->disablegfxlr, &overlay);

#ifdef SIS_CP
   SIS_CP_VIDEO_SET_CP
#endif

   /* set overlay parameters */
   set_overlay(pSiS, &overlay, pPriv, index, iscrt2);

   if((pSiS->VGAEngine == SIS_315_VGA) && !index) {
      /* Trigger register copy for 315 series */
      setvideoregmask(pSiS, Index_VI_Control_Misc3, (1 << index), (1 << index));
   }

   /* enable overlay */
   setvideoregmask (pSiS, Index_VI_Control_Misc0, 0x02, 0x02);

   /* loop foot */
   if(pPriv->displayMode & DISPMODE_MIRROR &&
      index == 0 		           &&
      pPriv->hasTwoOverlays) {
#ifdef SISMERGED
      if((!pSiS->MergedFB) || overlay.DoSecond) {
#endif
         index = 1; iscrt2 = 1;
         overlay.VBlankActiveFunc = vblank_active_CRT2;
         goto MIRROR;
#ifdef SISMERGED
     }
#endif
   }
   
   pPriv->mustwait = 0;
   pPriv->overlayStatus = TRUE;
}

static FBLinearPtr
SISAllocateOverlayMemory(
  ScrnInfoPtr pScrn,
  FBLinearPtr linear,
  int size
){
   ScreenPtr pScreen;
   FBLinearPtr new_linear;

   if(linear) {
	if(linear->size >= size)
	   return linear;

	if(xf86ResizeOffscreenLinear(linear, size))
	   return linear;

	xf86FreeOffscreenLinear(linear);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 8,
                                            NULL, NULL, NULL);

   if(!new_linear) {
        int max_size;

        xf86QueryLargestOffscreenLinear(pScreen, &max_size, 8,
				       PRIORITY_EXTREME);

        if(max_size < size) return NULL;

        xf86PurgeUnlockedOffscreenAreas(pScreen);
        new_linear = xf86AllocateOffscreenLinear(pScreen, size, 8,
                                                 NULL, NULL, NULL);
   }
   if(!new_linear)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Xv: Failed to allocate %dK of video memory\n", size/1024);
#ifdef TWDEBUG
   else
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Xv: Allocated %dK of video memory\n", size/1024);
#endif

   return new_linear;
}

static void
SISFreeOverlayMemory(ScrnInfoPtr pScrn)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    if(pPriv->linear) {
        xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
    }
}

static void
SISStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
  SISPortPrivPtr pPriv = (SISPortPrivPtr)data;
  SISPtr pSiS = SISPTR(pScrn);

  if(pPriv->grabbedByV4L) return;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

  if(shutdown) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
        close_overlay(pSiS, pPriv);
        pPriv->mustwait = 1;
     }
     SISFreeOverlayMemory(pScrn);
     pPriv->videoStatus = 0;
     pSiS->VideoTimerCallback = NULL;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
        pPriv->videoStatus = OFF_TIMER | CLIENT_VIDEO_ON;
        pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
        pSiS->VideoTimerCallback = SISVideoTimerCallback;
     }
  }
}

static int
SISPutImage(
  ScrnInfoPtr pScrn,
  short src_x, short src_y,
  short drw_x, short drw_y,
  short src_w, short src_h,
  short drw_w, short drw_h,
  int id, unsigned char* buf,
  short width, short height,
  Bool sync,
  RegionPtr clipBoxes, pointer data
){
   SISPtr pSiS = SISPTR(pScrn);
   SISPortPrivPtr pPriv = (SISPortPrivPtr)data;

   int totalSize=0;
   int depth = pSiS->CurrentLayout.bitsPerPixel >> 3;
   int myreds[] = { 0x000000ff, 0x0000f800, 0, 0x00ff0000 };

#if 0
   if(id == SDC_ID) {
      return(SiSHandleSiSDirectCommand(pScrn, pPriv, (sisdirectcommand *)buf));
   }
#endif

   if(pPriv->grabbedByV4L) return Success;

   pPriv->drw_x = drw_x;
   pPriv->drw_y = drw_y;
   pPriv->drw_w = drw_w;
   pPriv->drw_h = drw_h;
   pPriv->src_x = src_x;
   pPriv->src_y = src_y;
   pPriv->src_w = src_w;
   pPriv->src_h = src_h;
   pPriv->id = id;
   pPriv->height = height;

   /* Pixel formats:
      1. YU12:  3 planes:       H    V
               Y sample period  1    1   (8 bit per pixel)
	       V sample period  2    2	 (8 bit per pixel, subsampled)
	       U sample period  2    2   (8 bit per pixel, subsampled)

 	 Y plane is fully sampled (width*height), U and V planes
	 are sampled in 2x2 blocks, hence a group of 4 pixels requires
	 4 + 1 + 1 = 6 bytes. The data is planar, ie in single planes
	 for Y, U and V.
      2. UYVY: 3 planes:        H    V
               Y sample period  1    1   (8 bit per pixel)
	       V sample period  2    1	 (8 bit per pixel, subsampled)
	       U sample period  2    1   (8 bit per pixel, subsampled)
	 Y plane is fully sampled (width*height), U and V planes
	 are sampled in 2x1 blocks, hence a group of 4 pixels requires
	 4 + 2 + 2 = 8 bytes. The data is bit packed, there are no separate
	 Y, U or V planes.
	 Bit order:  U0 Y0 V0 Y1  U2 Y2 V2 Y3 ...
      3. I420: Like YU12, but planes U and V are in reverse order.
      4. YUY2: Like UYVY, but order is
                     Y0 U0 Y1 V0  Y2 U2 Y3 V2 ...
      5. YVYU: Like YUY2, but order is
      		     Y0 V0 Y1 U0  Y2 V2 Y3 U2 ...
   */

   switch(id){
     case PIXEL_FMT_YV12:
     case PIXEL_FMT_I420:
     case PIXEL_FMT_NV12:
     case PIXEL_FMT_NV21:
       pPriv->srcPitch = (width + 7) & ~7;
       /* Size = width * height * 3 / 2 */
       totalSize = (pPriv->srcPitch * height * 3) >> 1; /* Verified */
       break;
     case PIXEL_FMT_YUY2:
     case PIXEL_FMT_UYVY:
     case PIXEL_FMT_YVYU:
     case PIXEL_FMT_RGB6:
     case PIXEL_FMT_RGB5:
     default:
       pPriv->srcPitch = ((width << 1) + 3) & ~3;	/* Verified */
       /* Size = width * 2 * height */
       totalSize = pPriv->srcPitch * height;
   }

   /* make it a multiple of 16 to simplify to copy loop */
   totalSize += 15;
   totalSize &= ~15;

   /* allocate memory (we do doublebuffering) */
   if(!(pPriv->linear = SISAllocateOverlayMemory(pScrn, pPriv->linear,
						 totalSize<<1)))
	return BadAlloc;

   /* fixup pointers */
   pPriv->bufAddr[0] = (pPriv->linear->offset * depth);
   pPriv->bufAddr[1] = pPriv->bufAddr[0] + totalSize;

   /* copy data */
   if((pSiS->XvUseMemcpy) || (totalSize < 16)) {
      memcpy(pSiS->FbBase + pPriv->bufAddr[pPriv->currentBuf], buf, totalSize);
   } else {
      unsigned long i;
      CARD32 *src = (CARD32 *)buf;
      CARD32 *dest = (CARD32 *)(pSiS->FbBase + pPriv->bufAddr[pPriv->currentBuf]);
      for(i = 0; i < (totalSize/16); i++) {
         *dest++ = *src++;
	 *dest++ = *src++;
	 *dest++ = *src++;
	 *dest++ = *src++;
      }
   }

   SISDisplayVideo(pScrn, pPriv);

   /* update cliplist */
   if(pPriv->autopaintColorKey &&
      (pPriv->grabbedByV4L ||
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,0,0)
       (!RegionsEqual(&pPriv->clip, clipBoxes)) ||
#else
       (!REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes)) ||
#endif
       (pPriv->PrevOverlay != pPriv->NoOverlay))) {
     /* We always paint the colorkey for V4L */
     if(!pPriv->grabbedByV4L) {
     	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
     }
     /* draw these */
     pPriv->PrevOverlay = pPriv->NoOverlay;
     if((pPriv->NoOverlay) && (!pSiS->NoAccel)) {
        XAAFillMono8x8PatternRects(pScrn, myreds[depth-1], 0x000000, GXcopy, ~0,
			REGION_NUM_RECTS(clipBoxes),
			REGION_RECTS(clipBoxes),
			0x00422418, 0x18244200, 0, 0);
     } else {
        if(!pSiS->disablecolorkeycurrent) {
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
           XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
                           REGION_NUM_RECTS(clipBoxes),
                           REGION_RECTS(clipBoxes));
#else
	   xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
#endif
	}
     }

   }

   pPriv->currentBuf ^= 1;

   pPriv->videoStatus = CLIENT_VIDEO_ON;

   pSiS->VideoTimerCallback = SISVideoTimerCallback;

   return Success;
}

static int
SISQueryImageAttributes(
  ScrnInfoPtr pScrn,
  int id,
  unsigned short *w, unsigned short *h,
  int *pitches, int *offsets
){
    int    pitchY, pitchUV;
    int    size, sizeY, sizeUV;

    if(*w < IMAGE_MIN_WIDTH) *w = IMAGE_MIN_WIDTH;
    if(*h < IMAGE_MIN_HEIGHT) *h = IMAGE_MIN_HEIGHT;

    if(*w > DummyEncoding.width) *w = DummyEncoding.width;
    if(*h > DummyEncoding.height) *h = DummyEncoding.height;

    switch(id) {
    case PIXEL_FMT_YV12:
    case PIXEL_FMT_I420:
        *w = (*w + 7) & ~7;
        *h = (*h + 1) & ~1;
        pitchY = *w;
    	pitchUV = *w >> 1;
    	if(pitches) {
      	    pitches[0] = pitchY;
            pitches[1] = pitches[2] = pitchUV;
        }
    	sizeY = pitchY * (*h);
    	sizeUV = pitchUV * ((*h) >> 1);
    	if(offsets) {
          offsets[0] = 0;
          offsets[1] = sizeY;
          offsets[2] = sizeY + sizeUV;
        }
        size = sizeY + (sizeUV << 1);
    	break;
    case PIXEL_FMT_NV12:
    case PIXEL_FMT_NV21:
        *w = (*w + 7) & ~7;
        *h = (*h + 1) & ~1;
	pitchY = *w;
    	pitchUV = *w;
    	if(pitches) {
      	    pitches[0] = pitchY;
            pitches[1] = pitchUV;
        }
    	sizeY = pitchY * (*h);
    	sizeUV = pitchUV * ((*h) >> 1);
    	if(offsets) {
          offsets[0] = 0;
          offsets[1] = sizeY;
        }
        size = sizeY + (sizeUV << 1);
        break;
    case PIXEL_FMT_YUY2:
    case PIXEL_FMT_UYVY:
    case PIXEL_FMT_YVYU:
    case PIXEL_FMT_RGB6:
    case PIXEL_FMT_RGB5:
    default:
        *w = (*w + 1) & ~1;
        pitchY = *w << 1;
    	if(pitches) pitches[0] = pitchY;
    	if(offsets) offsets[0] = 0;
    	size = pitchY * (*h);
    	break;
    }

    return size;
}

static void
SISVideoTimerCallback(ScrnInfoPtr pScrn, Time now)
{
    SISPtr         pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = NULL;
    unsigned char  sridx, cridx;

    pSiS->VideoTimerCallback = NULL;

    if(!pScrn->vtSema) return;

    if(pSiS->adaptor) {
       pPriv = GET_PORT_PRIVATE(pScrn);
       if(!pPriv->videoStatus)
	  pPriv = NULL;
    }

    if(pPriv) {
       if(pPriv->videoStatus & TIMER_MASK) {
          UpdateCurrentTime();
	  if(pPriv->offTime < currentTime.milliseconds) {
             if(pPriv->videoStatus & OFF_TIMER) {
                /* Turn off the overlay */
	        sridx = inSISREG(SISSR); cridx = inSISREG(SISCR);
                close_overlay(pSiS, pPriv);
	        outSISREG(SISSR, sridx); outSISREG(SISCR, cridx);
	        pPriv->mustwait = 1;
                pPriv->videoStatus = FREE_TIMER;
                pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	        pSiS->VideoTimerCallback = SISVideoTimerCallback;
             } else if(pPriv->videoStatus & FREE_TIMER) {
                SISFreeOverlayMemory(pScrn);
	        pPriv->mustwait = 1;
                pPriv->videoStatus = 0;
             }
          } else
	     pSiS->VideoTimerCallback = SISVideoTimerCallback;
       }
    }
}

/* Offscreen surface stuff */

static int
SISAllocSurface (
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w,
    unsigned short h,
    XF86SurfacePtr surface
)
{
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    int size, depth;

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv: SISAllocSurface called\n");
#endif

    if((w < IMAGE_MIN_WIDTH) || (h < IMAGE_MIN_HEIGHT))
          return BadValue;
    if((w > DummyEncoding.width) || (h > DummyEncoding.height))
    	  return BadValue;

    if(pPriv->grabbedByV4L)
    	return BadAlloc;

    depth = pSiS->CurrentLayout.bitsPerPixel >> 3;
    w = (w + 1) & ~1;
    pPriv->pitch = ((w << 1) + 63) & ~63; /* Only packed pixel modes supported */
    size = h * pPriv->pitch; /*  / depth;   - Why? */
    pPriv->linear = SISAllocateOverlayMemory(pScrn, pPriv->linear, size);
    if(!pPriv->linear)
    	return BadAlloc;

    pPriv->offset    = pPriv->linear->offset * depth;

    surface->width   = w;
    surface->height  = h;
    surface->pScrn   = pScrn;
    surface->id      = id;
    surface->pitches = &pPriv->pitch;
    surface->offsets = &pPriv->offset;
    surface->devPrivate.ptr = (pointer)pPriv;

    close_overlay(pSiS, pPriv);
    pPriv->videoStatus = 0;
    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
    pSiS->VideoTimerCallback = NULL;
    pPriv->grabbedByV4L = TRUE;
    return Success;
}

static int
SISStopSurface (XF86SurfacePtr surface)
{
    SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);
    SISPtr pSiS = SISPTR(surface->pScrn);

    if(pPriv->grabbedByV4L && pPriv->videoStatus) {
       close_overlay(pSiS, pPriv);
       pPriv->mustwait = 1;
       pPriv->videoStatus = 0;
    }
    return Success;
}

static int
SISFreeSurface (XF86SurfacePtr surface)
{
    SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);

    if(pPriv->grabbedByV4L) {
       SISStopSurface(surface);
       SISFreeOverlayMemory(surface->pScrn);
       pPriv->grabbedByV4L = FALSE;
    }
    return Success;
}

static int
SISGetSurfaceAttribute (
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);

    return SISGetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SISSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
)
{
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);;

    return SISSetPortAttribute(pScrn, attribute, value, (pointer)pPriv);
}

static int
SISDisplaySurface (
    XF86SurfacePtr surface,
    short src_x, short src_y,
    short drw_x, short drw_y,
    short src_w, short src_h,
    short drw_w, short drw_h,
    RegionPtr clipBoxes
)
{
   ScrnInfoPtr pScrn = surface->pScrn;
   SISPtr pSiS = SISPTR(pScrn);
   SISPortPrivPtr pPriv = (SISPortPrivPtr)(surface->devPrivate.ptr);
   int myreds[] = { 0x000000ff, 0x0000f800, 0, 0x00ff0000 };

#ifdef TWDEBUG
   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv: DisplaySurface called\n");
#endif

   if(!pPriv->grabbedByV4L) return Success;

   pPriv->drw_x = drw_x;
   pPriv->drw_y = drw_y;
   pPriv->drw_w = drw_w;
   pPriv->drw_h = drw_h;
   pPriv->src_x = src_x;
   pPriv->src_y = src_y;
   pPriv->src_w = src_w;
   pPriv->src_h = src_h;
   pPriv->id = surface->id;
   pPriv->height = surface->height;
   pPriv->bufAddr[0] = surface->offsets[0];
   pPriv->currentBuf = 0;
   pPriv->srcPitch = surface->pitches[0];

   SISDisplayVideo(pScrn, pPriv);

   if(pPriv->autopaintColorKey) {
      if((pPriv->NoOverlay) && (!(pSiS->NoAccel))) {
         XAAFillMono8x8PatternRects(pScrn,
	  		myreds[(pSiS->CurrentLayout.bitsPerPixel >> 3) - 1], 
	 		0x000000, GXcopy, ~0,
			REGION_NUM_RECTS(clipBoxes),
			REGION_RECTS(clipBoxes),
			0x00422418, 0x18244200, 0, 0);
	
      } else {
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,0,0)
   	 XAAFillSolidRects(pScrn, pPriv->colorKey, GXcopy, ~0,
                        REGION_NUM_RECTS(clipBoxes),
                        REGION_RECTS(clipBoxes));
#else
         xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
#endif
      }
   }

   pPriv->videoStatus = CLIENT_VIDEO_ON;

   return Success;
}

#define NUMOFFSCRIMAGES_300 4
#define NUMOFFSCRIMAGES_315 5

static XF86OffscreenImageRec SISOffscreenImages[NUMOFFSCRIMAGES_315] =
{
 {
   &SISImages[0],  	/* YUV2 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   0, 0,  			/* Rest will be filled in */
   0,
   NULL
 },
 {
   &SISImages[2],	/* UYVY */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   0, 0,  			/* Rest will be filled in */
   0,
   NULL
 }
 ,
 {
   &SISImages[4],	/* RV15 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   0, 0,  			/* Rest will be filled in */
   0,
   NULL
 },
 {
   &SISImages[5],	/* RV16 */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   0, 0,  			/* Rest will be filled in */
   0,
   NULL
 },
 {
   &SISImages[6],	/* YVYU */
   VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT,
   SISAllocSurface,
   SISFreeSurface,
   SISDisplaySurface,
   SISStopSurface,
   SISGetSurfaceAttribute,
   SISSetSurfaceAttribute,
   0, 0,  			/* Rest will be filled in */
   0,
   NULL
 }
};

static void
SISInitOffscreenImages(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SISPtr pSiS = SISPTR(pScrn);
    SISPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    int i, num;

    if(pSiS->VGAEngine == SIS_300_VGA) 	num = NUMOFFSCRIMAGES_300;
    else 				num = NUMOFFSCRIMAGES_315;

    for(i = 0; i <= num; i++) {
       SISOffscreenImages[i].max_width  = DummyEncoding.width;
       SISOffscreenImages[i].max_height = DummyEncoding.height;
       if(pSiS->VGAEngine == SIS_300_VGA) {
	  SISOffscreenImages[i].num_attributes = NUM_ATTRIBUTES_300;
	  SISOffscreenImages[i].attributes = &SISAttributes_300[0];
       } else {
	  if(pPriv->hasTwoOverlays) {
	     SISOffscreenImages[i].num_attributes = NUM_ATTRIBUTES_315;
	  } else {
	     SISOffscreenImages[i].num_attributes = NUM_ATTRIBUTES_315 - 1;
	  }
	  SISOffscreenImages[i].attributes = &SISAttributes_315[0];
       }
    }
    xf86XVRegisterOffscreenImages(pScreen, SISOffscreenImages, num);
}

#ifdef NOT_YET_IMPLEMENTED /* ----------- TW: FOR FUTURE USE -------------------- */

/* Set alpha - does not work */
static void
set_alpha(SISPtr pSiS, CARD8 alpha)
{
    setvideoregmask(pSiS, Index_VI_Key_Overlay_OP, ((alpha & 0x0f) << 4), 0xf0);
}

/* Set SubPicture Start Address (yet unused) */
static void
set_subpict_start_offset(SISPtr pSiS, SISOverlayPtr pOverlay, int index)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictAddr >> 4; /* 630 <-> 315 shiftValue? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_Low, temp & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_Middle, (temp>>8) & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High, (temp>>16) & 0x3F);
    if(pSiS->VGAEngine == SIS_315_VGA) {
       setvideoreg(pSiS,Index_VI_SubPict_Start_Over, (temp>>22) & 0x01);
       /* Submit SubPict offset ? */
       /* data=getvideoreg(pSiS,Index_VI_Control_Misc3); */
       setvideoreg(pSiS,Index_VI_Control_Misc3, (1 << index) | 0x04);
    }
}

/* Set SubPicture Pitch (yet unused) */
static void
set_subpict_pitch(SISPtr pSiS, SISOverlayPtr pOverlay, int index)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictPitch >> 4; /* 630 <-> 315 shiftValue? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Pitch, temp & 0xFF);
    if(pSiS->VGAEngine == SIS_315_VGA) {
       setvideoreg(pSiS,Index_VI_SubPict_Buf_Pitch_High, (temp>>8) & 0xFF);
       /* Submit SubPict pitch ? */
       /* data=getvideoreg(pSiS,Index_VI_Control_Misc3); */
       setvideoreg(pSiS,Index_VI_Control_Misc3, (1 << index) | 0x04);
    }
}

/* Calculate and set SubPicture scaling (untested, unused yet) */
static void
set_subpict_scale_factor(SISOverlayPtr pOverlay, ScrnInfoPtr pScrn,
                         SISPortPrivPtr pPriv, int index, int iscrt2)
{
  SISPtr pSiS = SISPTR(pScrn);
  CARD32 I=0,mult=0;
  int flag=0;

  int dstW = pOverlay->SubPictdstBox.x2 - pOverlay->SubPictdstBox.x1;
  int dstH = pOverlay->SubPictdstBox.y2 - pOverlay->SubPictdstBox.y1;
  int srcW = pOverlay->SubPictsrcW;
  int srcH = pOverlay->SubPictsrcH;
  CARD16 LCDheight = pSiS->LCDheight;
  int srcPitch = pOverlay->SubPictOrigPitch;
  int origdstH = dstH;

  /* Stretch image due to idiotic LCD "auto"-scaling */
  /* INCOMPLETE and INCORRECT - See set_scale_factor() */
  if( (pPriv->bridgeIsSlave) && (pSiS->VBFlags & CRT2_LCD) ) {
  	dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
  } else if((index) && (pSiS->VBFlags & CRT2_LCD)) {
   	dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
	if(pPriv->displayMode == DISPMODE_MIRROR) flag = 1;
  }

  if(dstW == srcW) {
        pOverlay->SubPictHUSF   = 0x00;
        pOverlay->SubPictIntBit = 0x01;
  } else if(dstW > srcW) {
        pOverlay->SubPictHUSF   = (srcW << 16) / dstW;
        pOverlay->SubPictIntBit = 0x00;
  } else {
        int tmpW = dstW;

        I = 0x00;
        while (srcW >= tmpW) {
            tmpW <<= 1;
            I++;
        }
        pOverlay->SubPictwHPre = (CARD8)(I - 1);
        dstW <<= (I - 1);
        if((srcW % dstW))
            pOverlay->SubPictHUSF = ((srcW - dstW) << 16) / dstW;
        else
            pOverlay->SubPictHUSF = 0x00;

	pOverlay->SubPictIntBit = 0x01;
  }

  if(dstH == srcH) {
        pOverlay->SubPictVUSF   = 0x00;
        pOverlay->SubPictIntBit |= 0x02;
  } else if(dstH > srcH) {
        dstH += 0x02;
        pOverlay->SubPictVUSF = (srcH << 16) / dstH;
     /* pOverlay->SubPictIntBit |= 0x00; */
  } else {

        I = srcH / dstH;
        pOverlay->SubPictIntBit |= 0x02;

        if(I < 2) {
            pOverlay->SubPictVUSF = ((srcH - dstH) << 16) / dstH;
	    /* TW: Needed for LCD-scaling modes */
	    if((flag) && (mult = (srcH / origdstH)) >= 2)
	    		pOverlay->SubPictPitch /= mult;
        } else {
            if(((srcPitch * I)>>2) > 0xFFF) {
                I = (0xFFF*2/srcPitch);
                pOverlay->SubPictVUSF = 0xFFFF;
            } else {
                dstH = I * dstH;
                if(srcH % dstH)
                    pOverlay->SubPictVUSF = ((srcH - dstH) << 16) / dstH;
                else
                    pOverlay->SubPictVUSF = 0x00;
            }
            /* set video frame buffer offset */
            pOverlay->SubPictPitch = (CARD16)(srcPitch*I);
        }
   }
   /* set SubPicture scale factor */
   setvideoreg (pSiS, Index_VI_SubPict_Hor_Scale_Low,  (CARD8)(pOverlay->SubPictHUSF));
   setvideoreg (pSiS, Index_VI_SubPict_Hor_Scale_High, (CARD8)((pOverlay->SubPictHUSF)>>8));
   setvideoreg (pSiS, Index_VI_SubPict_Vert_Scale_Low, (CARD8)(pOverlay->SubPictVUSF));
   setvideoreg (pSiS, Index_VI_SubPict_Vert_Scale_High,(CARD8)((pOverlay->SubPictVUSF)>>8));

   setvideoregmask (pSiS, Index_VI_SubPict_Scale_Control,
   				(pOverlay->SubPictIntBit << 3) |
				(pOverlay->SubPictwHPre), 0x7f);
}

/* Set SubPicture Preset (yet unused) */
static void
set_subpict_preset(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    CARD32 temp;
    CARD8  data;

    temp = pOverlay->SubPictPreset >> 4; /* TW: 630 <-> 315 ? */

    setvideoreg(pSiS,Index_VI_SubPict_Buf_Preset_Low, temp & 0xFF);
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Preset_Middle, (temp>>8) & 0xFF);
    data = getvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High);
    if(temp > 0xFFFF)
    	data |= 0x40;
    else
    	data &= ~0x40;
    setvideoreg(pSiS,Index_VI_SubPict_Buf_Start_High, data);
}

static void
enable_subpict_overlay(SISPtr pSiS, Bool enable)
{
   setvideoregmask(pSiS, Index_VI_SubPict_Scale_Control,
   		enable ? 0x40 : 0x00,
		0x40);
}

/* Set overlay for subpicture */
static void
set_subpict_overlay(SISPtr pSiS, SISOverlayPtr pOverlay, SISPortPrivPtr pPriv, int index)
{
    ScrnInfoPtr pScrn = pSiS->pScrn;

    set_subpict_pitch(pSiS, &overlay, index);
    set_subpict_start_offset(pSiS, &overlay, index);
    set_subpict_scale_factor(&overlay, pScrn, pPriv, index);
    /* set_subpict_preset(pSiS, &overlay); */
    /* enable_subpict_overlay(pSiS, 1); */
}


/* Set MPEG Field Preset (yet unused) */
static void
set_mpegfield_preset(SISPtr pSiS, SISOverlayPtr pOverlay)
{
    setvideoreg(pSiS,Index_MPEG_Y_Buf_Preset_Low, pOverlay->MPEG_Y & 0xFF);
    setvideoreg(pSiS,Index_MPEG_Y_Buf_Preset_Middle, (pOverlay->MPEG_Y>>8) & 0xFF);

    setvideoreg(pSiS,Index_MPEG_UV_Buf_Preset_Low, pOverlay->MPEG_UV & 0xFF);
    setvideoreg(pSiS,Index_MPEG_UV_Buf_Preset_Middle, (pOverlay->MPEG_UV>>8) & 0xFF);

    setvideoreg(pSiS,Index_MPEG_Y_UV_Buf_Preset_High,
    		((pOverlay->MPEG_Y>>16) & 0x0F) | ((pOverlay->MPEG_UV>>12) & 0xF0));
}

static void
set_mpegfield_scale(SISPtr pSiS, SISOverlayPtr pOverlay)
{
	/* Empty for now */
}

#endif /* ------------------------------------------------------------------- */



