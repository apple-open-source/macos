/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo.h,v 1.23 2002/10/30 12:52:21 alanh Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

#include "xaa.h"
#include "xaalocal.h"		/* XAA internals as we replace some of XAA */
#include "xf86Cursor.h"

#include "shadowfb.h"

#include "vbe.h"

/* Needed by the Shadow Framebuffer */
#include "shadow.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

#include "xf86i2c.h"

#ifdef XvExtension
# include "xf86xv.h"
# include "Xv.h"
#endif /* XvExtension */

/*
 * Driver data structures.
 */
#include "neo_reg.h"
#include "neo_macros.h"

/* Supported chipsets */
typedef enum {
    NM2070,
    NM2090,
    NM2093,
    NM2097,
    NM2160,
    NM2200,
    NM2230,
    NM2360,
    NM2380
} NEOType;

/* function prototypes */

extern Bool NEOSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
extern void NEOAdjustFrame(int scrnIndex, int x, int y, int flags);

/* in neo_2070.c */
extern Bool Neo2070AccelInit(ScreenPtr pScreen);

/* in neo_2090.c */
extern Bool Neo2090AccelInit(ScreenPtr pScreen);

/* in neo_2097.c */
extern Bool Neo2097AccelInit(ScreenPtr pScreen);

/* in neo_2200.c */
extern Bool Neo2200AccelInit(ScreenPtr pScreen);

/* in neo_cursor.c */
extern Bool NeoCursorInit(ScreenPtr pScrn);
extern void NeoShowCursor(ScrnInfoPtr pScrn);
extern void NeoHideCursor(ScrnInfoPtr pScrn);

/* in neo_bank.c */
int NEOSetReadWrite(ScreenPtr pScreen, int bank);
int NEOSetWrite(ScreenPtr pScreen, int bank);
int NEOSetRead(ScreenPtr pScreen, int bank);

/* in neo_i2c.c */
extern Bool neo_I2CInit(ScrnInfoPtr pScrn);

/* in neo_shadow.c */
void neoShadowUpdate (ScreenPtr pScreen, shadowBufPtr pBuf);
void neoPointerMoved(int index, int x, int y);
void neoRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void neoRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void neoRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void neoRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void neoRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* in neo_dga.c */
Bool NEODGAInit(ScreenPtr pScreen);

/* in neo_video.c */
extern void NEOInitVideo(ScreenPtr pScreen);
extern void NEOResetVideo(ScrnInfoPtr pScrn);

/* shadow regs */

#define NEO_EXT_CR_MAX 0x85
#define NEO_EXT_GR_MAX 0xC7
typedef struct {
    unsigned char CR[NEO_EXT_CR_MAX+1];
    unsigned char GR[NEO_EXT_GR_MAX+1];
} regSaveRec, *regSavePtr;

/* registers */
typedef struct {
    unsigned char GeneralLockReg;
    unsigned char ExtCRTDispAddr;
    unsigned char ExtCRTOffset;
    unsigned char SysIfaceCntl1;
    unsigned char SysIfaceCntl2;
    unsigned char ExtColorModeSelect;
    unsigned char SingleAddrPage;
    unsigned char DualAddrPage;
    unsigned char biosMode;
    unsigned char PanelDispCntlReg1;
    unsigned char PanelDispCntlReg2;
    unsigned char PanelDispCntlReg3;
    unsigned char PanelVertCenterReg1;
    unsigned char PanelVertCenterReg2;
    unsigned char PanelVertCenterReg3;
    unsigned char PanelVertCenterReg4;
    unsigned char PanelVertCenterReg5;
    unsigned char PanelHorizCenterReg1;
    unsigned char PanelHorizCenterReg2;
    unsigned char PanelHorizCenterReg3;
    unsigned char PanelHorizCenterReg4;
    unsigned char PanelHorizCenterReg5;
    unsigned char Sequencer1;
    Bool ProgramVCLK;
    unsigned char VCLK3NumeratorLow;
    unsigned char VCLK3NumeratorHigh;
    unsigned char VCLK3Denominator;
    unsigned char VerticalExt;
    regSavePtr reg;
} NeoRegRec, *NeoRegPtr;

typedef struct {
    /* Hardware cursor address */
    unsigned int CursorAddress;
    Bool UseHWCursor;
    Bool NoCursorMode;
    unsigned char CursTemp[1024];
    /* Boundaries of the pixmap cache */
    unsigned int cacheStart;
    unsigned int cacheEnd;
    /* Blitter */
    unsigned int tmpBltCntlFlags;
    unsigned int BltCntlFlags;
    unsigned int BltModeFlags;
    unsigned int ColorShiftAmt;
    unsigned int Pitch;
    unsigned int PixelWidth;
    unsigned int PlaneMask;
    int CPUToScreenColorExpandFill_x;
    int CPUToScreenColorExpandFill_y;
    int CPUToScreenColorExpandFill_w;
    int CPUToScreenColorExpandFill_h;
    int CPUToScreenColorExpandFill_skipleft;
} NEOACLRec, *NEOACLPtr;
#define NEOACLPTR(p)	&((NEOPtr)((p)->driverPrivate))->Accel

/* globals */
typedef struct neoRec 
{
    int NeoChipset;
    pciVideoPtr PciInfo;
    PCITAG      PciTag;
    EntityInfoPtr pEnt;
    XAAInfoRecPtr	AccelInfoRec;
    NEOACLRec Accel;
    unsigned long NeoMMIOAddr;
    unsigned long NeoLinearAddr;
    unsigned char* NeoMMIOBase;
    unsigned long NeoMMIOAddr2;
    unsigned char* NeoMMIOBase2;
    unsigned char* NeoFbBase;
    long NeoFbMapSize;
    unsigned long vgaIOBase;
    DGAModePtr		DGAModes;
    int			numDGAModes;
    Bool		DGAactive;
    int			DGAViewportStatus;
    /* ??? */
    int NeoFifoCount;
    /* cursor */
    int NeoCursorMem;
    Bool NeoHWCursorShown;
    Bool NeoHWCursorInitialized;
    xf86CursorInfoPtr CursorInfo;
    int NeoCursorOffset;
    int NeoCursorPrevX;
    int NeoCursorPrevY;
    unsigned char *NeoCursorImage;
    /* Panels size */
    int NeoPanelWidth;
    int NeoPanelHeight;
    /* options */
    OptionInfoPtr Options;
    Bool noLinear;
    Bool noAccel;
    Bool noAccelSet;
    Bool swCursor;
    Bool noMMIO;
    Bool internDisp;
    Bool externDisp;
    Bool noLcdStretch;
    Bool shadowFB;
    Bool lcdCenter;
    Bool onPciBurst;
    Bool progLcdRegs;
    Bool progLcdStretch;
    Bool progLcdStretchOpt;
    Bool overrideValidate;
    Bool strangeLockups;
    /* registers */
    NeoRegRec NeoModeReg;
    NeoRegRec NeoSavedReg;
    /* proc pointer */
    CloseScreenProcPtr CloseScreen;
    I2CBusPtr I2C;
    vbeInfoPtr          pVbe;
    unsigned char * ShadowPtr;
    int ShadowPitch;
    RefreshAreaFuncPtr refreshArea;
    void	(*PointerMoved)(int index, int x, int y);
    int rotate;
    Bool showcache;
#ifdef XvExtension
    Bool video;
    double videoHZoom;
    double videoVZoom;
    XF86VideoAdaptorPtr overlayAdaptor;
    int overlay;
    int overlay_offset;
    int videoKey;
    int interlace;
#endif /* XvExtension */
} NEORec, *NEOPtr;

typedef struct {
    int x_res;
    int y_res;
    int mode;
} biosMode;

/* The privates of the NEO driver */
#define NEOPTR(p)	((NEOPtr)((p)->driverPrivate))

/* I/O register offsets */
#define GRAX	0x3CE

/* vga IO functions */
#define VGArCR(index)		(*hwp->readCrtc)(hwp, index)
#define VGAwCR(index, val)	(*hwp->writeCrtc)(hwp, index, val)
#define VGArGR(index)		(*hwp->readGr)(hwp, index)
#define VGAwGR(index, val)	(*hwp->writeGr)(hwp, index, val)
#define VGArSR(index)		(*hwp->readSeq)(hwp, index)
#define VGAwSR(index, val)	(*hwp->writeSeq)(hwp, index, val)

/* memory mapped register access macros */
#define INREG8(addr)		MMIO_IN8(nPtr->NeoMMIOBase, addr)
#define INREG16(addr)		MMIO_IN16(nPtr->NeoMMIOBase, addr)
#define INREG(addr)		MMIO_IN32(nPtr->NeoMMIOBase, addr)
#define OUTREG8(addr, val)	MMIO_OUT8(nPtr->NeoMMIOBase, addr, val)
#define OUTREG16(addr, val)	MMIO_OUT16(nPtr->NeoMMIOBase, addr, val)
#define OUTREG(addr, val)	MMIO_OUT32(nPtr->NeoMMIOBase, addr, val)

/* This swizzle macro is to support the manipulation of cursor masks when
 * the sprite moves off the left edge of the display.  This code is
 * platform specific, and is known to work with 32bit little endian machines
 */
#define SWIZZLE32(__b) { \
  ((unsigned char *)&__b)[0] = byte_reversed[((unsigned char *)&__b)[0]]; \
  ((unsigned char *)&__b)[1] = byte_reversed[((unsigned char *)&__b)[1]]; \
  ((unsigned char *)&__b)[2] = byte_reversed[((unsigned char *)&__b)[2]]; \
  ((unsigned char *)&__b)[3] = byte_reversed[((unsigned char *)&__b)[3]]; \
}

#define PROBED_NM2070	0x01
#define PROBED_NM2090	0x42
#define PROBED_NM2093	0x43
#define PROBED_NM2097	0x83
#define PROBED_NM2160	0x44
#define PROBED_NM2200	0x45
