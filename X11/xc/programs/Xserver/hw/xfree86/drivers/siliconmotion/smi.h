/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi.h-arc   1.51   29 Nov 2000 17:45:16   Frido  $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of the XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from the XFree86 Project and Silicon Motion.
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi.h,v 1.12 2003/01/12 03:55:49 tsi Exp $ */

#ifndef _SMI_H
#define _SMI_H

#define USE_FB

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86Cursor.h"
#include "vgaHW.h"

#include "compiler.h"

#include "mipointer.h"
#include "micmap.h"

#ifdef USE_FB
#include "fb.h"
#else

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb16.h"
#include "cfb24.h"
#endif

#include "xaa.h"
#include "xf86cmap.h"
#include "xf86i2c.h"

#include "xf86int10.h"
#include "vbe.h"

#ifdef XvExtension
#	include "xf86xv.h"
#	include "Xv.h"
#endif

/******************************************************************************/
/*			D E F I N I T I O N S				      */
/******************************************************************************/

#ifndef SMI_DEBUG
#	define SMI_DEBUG	0
#endif

#define SMI_USE_IMAGE_WRITES	0
#define SMI_USE_VIDEO		1
#define SMI_USE_CAPTURE		1

/******************************************************************************/
/*			S T R U C T U R E S				      */
/******************************************************************************/

/* Driver data structure; this should contain all needed info for a mode */
typedef struct
{
	Bool    modeInit;
	CARD16	mode;
	CARD8	SR17, SR18, SR21, SR31, SR32, SR6A, SR6B, SR81, SRA0;
	CARD8	CR33, CR33_2, CR3A;
	CARD8	CR40[14], CR40_2[14];
	CARD8	CR90[16], CR9F_2;
	CARD8	CRA0[14];
	CARD8	smiDACMask, smiDacRegs[256][3];
    /* CZ 2.11.2001: for gamma correction */
    CARD8   CCR66;
    /* end CZ */
	CARD8	smiFont[8192];
	CARD32	DPR10, DPR1C, DPR20, DPR24, DPR28, DPR2C, DPR30, DPR3C, DPR40,
			DPR44;
	CARD32	VPR00, VPR0C, VPR10;
	CARD32	CPR00;

} SMIRegRec, *SMIRegPtr;

/* Global PDEV structure. */
typedef struct
{
	/* accel additions */
	CARD32			AccelCmd;	/* Value for DPR0C */
	CARD32			Stride;		/* Stride of frame buffer */
	CARD32			ScissorsLeft;	/* Left/top of current
						   scissors */
	CARD32			ScissorsRight;	/* Right/bottom of current
						   scissors */
	Bool			ClipTurnedOn;	/* Clipping was turned on by
						   the previous command */
	CARD8			SR18Value;	/* PDR#521: original SR18
						   value */
	CARD8			SR21Value;	/* PDR#521: original SR21
						   value */
	SMIRegRec		SavedReg;	/* console saved mode
						   registers */
	SMIRegRec		ModeReg;	/* XServer video state mode
						   registers */
	xf86CursorInfoPtr	CursorInfoRec;	/* HW Cursor info */

	Bool			ModeStructInit;	/* Flag indicating ModeReg has
						   been duped from console
						   state */
	int			vgaCRIndex, vgaCRReg;
	int			width, height;	/* Width and height of the
						   screen */
	int			Bpp;		/* Bytes per pixel */

	/* XAA */
	int			videoRAMBytes;	/* In units as noted, set in
						   PreInit  */
	int			videoRAMKBytes;	/* In units as noted, set in
						   PreInit */
	unsigned char *		MapBase;	/* Base of mapped memory */
	int			MapSize;	/* Size of mapped memory */
	CARD8 *			DPRBase;	/* Base of DPR registers */
	CARD8 *			VPRBase;	/* Base of VPR registers */
	CARD8 *			CPRBase;	/* Base of CPR registers */
	CARD8 *			DataPortBase;	/* Base of data port */
	int			DataPortSize;	/* Size of data port */
	CARD8 *			IOBase;		/* Base of MMIO VGA ports */
	IOADDRESS		PIOBase;	/* Base of I/O ports */
	unsigned char *		FBBase;		/* Base of FB */
	CARD32			FBOffset;	/* Current visual FB starting
						   location */
	CARD32			FBCursorOffset;	/* Cursor storage location */
	CARD32			FBReserved;	/* Reserved memory in frame
						   buffer */
	
	Bool			PrimaryVidMapped;	/* Flag indicating if
							   vgaHWMapMem was used
							   successfully for
							   this screen */
	int			dacSpeedBpp;	/* Clock value */
	int			minClock;	/* Mimimum clock */
	int			maxClock;	/* Maximum clock */
	int			MCLK;		/* Memory Clock  */
	int			GEResetCnt;	/* Limit the number of errors
						   printed using a counter */

	Bool			pci_burst;	/* Enable PCI burst mode for
						   reads? */
	Bool			NoPCIRetry;	/* Disable PCI retries */
	Bool			fifo_conservative;	/* Adjust fifo for
							   acceleration? */
	Bool			fifo_moderate;	/* Adjust fifo for
						   acceleration? */
	Bool			fifo_aggressive;	/* Adjust fifo for
							   acceleration? */
	Bool			NoAccel;	/* Disable Acceleration */
	Bool			hwcursor;	/* hardware cursor enabled */
	Bool			ShowCache;	/* Debugging option */
	Bool			useBIOS;	/* Use BIOS for mode sets */
	Bool			zoomOnLCD;	/* Zoom on LCD */
	
	CloseScreenProcPtr	CloseScreen;	/* Pointer used to save wrapped
						   CloseScreen function */
	XAAInfoRecPtr		AccelInfoRec;	/* XAA info Rec */
	pciVideoPtr		PciInfo;	/* PCI info vars */
	PCITAG			PciTag;
	int			Chipset;	/* Chip info, set using PCI
						   above */
	int			ChipRev;

	/* DGA */
	DGAModePtr		DGAModes;	/* Pointer to DGA modes */
	int			numDGAModes;	/* Number of DGA modes */
	Bool			DGAactive;	/* Flag if DGA is active */
	int			DGAViewportStatus;

	/* DPMS */
	int			CurrentDPMS;	/* Current DPMS state */
	unsigned char		DPMS_SR20;	/* Saved DPMS SR20 register */
	unsigned char		DPMS_SR21;	/* Saved DPMS SR21 register */
	unsigned char		DPMS_SR31;	/* Saved DPMS SR31 register */
	unsigned char		DPMS_SR34;	/* Saved DPMS SR34 register */

	/* Panel information */
	Bool			lcd;		/* LCD active, 1=DSTN, 2=TFT */
	int			lcdWidth;	/* LCD width */
	int			lcdHeight;	/* LCD height */

	I2CBusPtr		I2C;		/* Pointer into I2C module */
	xf86Int10InfoPtr	pInt10;		/* Pointer to INT10 module */

	/* Shadow frame buffer (rotation) */
	Bool			shadowFB;	/* Flag if shadow buffer is
						   used */
	int			rotate;		/* Rotation flags */
	int			ShadowPitch;	/* Pitch of shadow buffer */
	int			ShadowWidthBytes;	/* Width of shadow
							   buffer in bytes */
	int			ShadowWidth;	/* Width of shadow buffer in
						   pixels */
	int			ShadowHeight;	/* Height of shadow buffer in
						   pixels */
	CARD32			saveBufferSize;	/* #670 - FB save buffer size */
	void *			pSaveBuffer;	/* #670 - FB save buffer */
	CARD32			savedFBOffset;	/* #670 - Saved FBOffset value */
	CARD32			savedFBReserved;	/* #670 - Saved
							   FBReserved value */
	CARD8 *			paletteBuffer;	/* #920 - Palette save buffer */

	/* Polylines - #671 */
	ValidateGCProcPtr	ValidatePolylines;	/* Org.
							   ValidatePolylines
							   function */
	Bool			polyLines;	/* Our polylines patch is
						   active */

	void (*PointerMoved)(int index, int x, int y);

#ifdef XvExtension
	int			videoKey;	/* Video chroma key */
	Bool			ByteSwap;	/* Byte swap for ZV port */
    Bool        interlaced;   /* True: Interlaced Video */
	/* XvExtension */
	XF86VideoAdaptorPtr	ptrAdaptor;	/* Pointer to VideoAdapter
						   structure */
	void (*BlockHandler)(int i, pointer blockData, pointer pTimeout,
						 pointer pReadMask);
    GCPtr videoGC;
#endif
	OptionInfoPtr		Options;
        CARD8 DACmask;
} SMIRec, *SMIPtr;

#define SMIPTR(p) ((SMIPtr)((p)->driverPrivate))

/******************************************************************************/
/*			M A C R O S					      */
/******************************************************************************/

#if SMI_DEBUG
#	define VERBLEV 1
#	define ENTER_PROC(PROCNAME)	xf86ErrorFVerb(VERBLEV, "ENTER\t" PROCNAME \
									"(%d)\n", __LINE__); xf86Break1()
#	define DEBUG_PROC(PROCNAME)	xf86ErrorFVerb(VERBLEV, "DEBUG\t" PROCNAME \
									"(%d)\n", __LINE__); xf86Break2()
#	define LEAVE_PROC(PROCNAME)	xf86ErrorFVerb(VERBLEV, "LEAVE\t" PROCNAME \
									"(%d)\n", __LINE__); xf86Break1()
#	define DEBUG(arg)				xf86ErrorFVerb arg
#else
#	define VERBLEV	4
#	define ENTER_PROC(PROCNAME)
#	define DEBUG_PROC(PROCNAME)
#	define LEAVE_PROC(PROCNAME)
#	define DEBUG(arg)
#endif

/* Some Silicon Motion structs & registers */
#include "regsmi.h"

#if !defined (MetroLink) && !defined (VertDebug)
#define VerticalRetraceWait()						\
do									\
{									\
    if (VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x17) & 0x80)		\
    {									\
	while ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x00);	\
	while ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x08);	\
	while ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x00);	\
    }									\
} while (0)
#else
#define SPIN_LIMIT 1000000
#define VerticalRetraceWait()						\
do									\
{									\
    if (VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x17) & 0x80)		\
    {									\
	volatile unsigned long _spin_me;				\
	for (_spin_me = SPIN_LIMIT;					\
	     ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x00) && 	\
	     _spin_me;							\
	     _spin_me--);						\
	if (!_spin_me)							\
	    ErrorF("smi: warning: VerticalRetraceWait timed out.\n");	\
	for (_spin_me = SPIN_LIMIT;					\
	     ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x08) && 	\
	     _spin_me;							\
	     _spin_me--);						\
	if (!_spin_me)							\
	    ErrorF("smi: warning: VerticalRetraceWait timed out.\n");	\
	for (_spin_me = SPIN_LIMIT;					\
	     ((VGAIN8(pSmi, vgaIOBase + 0x0A) & 0x08) == 0x00) && 	\
	     _spin_me;							\
	     _spin_me--);						\
	if (!_spin_me)							\
	    ErrorF("smi: warning: VerticalRetraceWait timed out.\n");	\
	}								\
} while (0)
#endif

/******************************************************************************/
/*			F U N C T I O N   P R O T O T Y P E S		      */
/******************************************************************************/

/* smi_dac.c */
void SMI_CommonCalcClock(int scrnIndex, long freq, int min_m, int min_n1, 
			 int max_n1, int min_n2, int max_n2, long freq_min, 
			 long freq_max, unsigned char * mdiv, 
			 unsigned char * ndiv);

/* smi_i2c */
Bool SMI_I2CInit(ScrnInfoPtr pScrn);

/* smi_accel.c */
Bool SMI_AccelInit(ScreenPtr pScrn);
void SMI_AccelSync(ScrnInfoPtr pScrn);
void SMI_GEReset(ScrnInfoPtr pScrn, int from_timeout, int line, char *file);
void SMI_EngineReset(ScrnInfoPtr);

/* smi_hwcurs.c */
Bool SMI_HWCursorInit(ScreenPtr pScrn);

/* smi_driver.c */
void SMI_AdjustFrame(int scrnIndex, int x, int y, int flags);
Bool SMI_SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);

/* smi_dga.c */
Bool SMI_DGAInit(ScreenPtr pScrn);

/* smi_shadow.c */
void SMI_PointerMoved(int index, int x, int y);
void SMI_RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* smi_video.c */
void SMI_InitVideo(ScreenPtr pScreen);

#endif  /*_SMI_H*/
