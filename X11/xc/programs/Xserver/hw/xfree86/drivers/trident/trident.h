/*
 * Copyright 1992-2000 by Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident.h,v 1.56 2002/09/16 18:06:02 eich Exp $ */
/*#define VBE_INFO*/

#ifndef _TRIDENT_H_
#define _TRIDENT_H_

#include "xf86Cursor.h"
#include "xaa.h"
#include "xf86RamDac.h"
#include "compiler.h"
#include "vgaHW.h"
#include "xf86i2c.h"
#include "xf86int10.h"
#include "shadowfb.h"
#include "shadow.h"
#include "xf86xv.h"
#include "xf86Pci.h"
#include "vbe.h"

typedef struct {
	unsigned char tridentRegs3x4[0x100];
	unsigned char tridentRegs3CE[0x100];
	unsigned char tridentRegs3C4[0x100];
	unsigned char tridentRegsDAC[0x01];
        unsigned char tridentRegsClock[0x03];
	unsigned char DacRegs[0x300];
} TRIDENTRegRec, *TRIDENTRegPtr;

#define TRIDENTPTR(p)	((TRIDENTPtr)((p)->driverPrivate))

typedef struct {
    ScrnInfoPtr		pScrn;
    pciVideoPtr		PciInfo;
    PCITAG		PciTag;
    EntityInfoPtr	pEnt;
    int			Chipset;
    int			DACtype;
    int			RamDac;
    int                 ChipRev;
    int			HwBpp;
    int			BppShift;
    CARD32		IOAddress;
    unsigned long	FbAddress;
    unsigned char *     IOBase;
    unsigned char *	FbBase;
    long		FbMapSize;
    IOADDRESS		PIOBase;
    Bool		NoAccel;
    Bool		HWCursor;
    Bool		UsePCIRetry;
    Bool		UsePCIBurst;
    Bool		NewClockCode;
    Bool		Clipping;
    Bool		DstEnable;
    Bool		ROP;
    Bool		HasSGRAM;
    Bool		MUX;
    Bool		IsCyber;
    Bool		CyberShadow;
    Bool		CyberStretch;
    Bool		NoMMIO;
    Bool                MMIOonly;
    Bool		ShadowFB;
    Bool		Linear;
    DGAModePtr		DGAModes;
    int			numDGAModes;
    Bool		DGAactive;
    int			DGAViewportStatus;
    unsigned char *	ShadowPtr;
    int			ShadowPitch;
    RefreshAreaFuncPtr  RefreshArea;
    void	        (*PointerMoved)(int index, int x, int y);
    int                 Rotate;
    float		frequency;
    unsigned char	REGPCIReg;
    unsigned char	REGNewMode1;
    CARD8		SaveClock1;
    CARD8		SaveClock2;
    CARD8		SaveClock3;
    int			MinClock;
    int			MaxClock;
    int			MUXThreshold;
    int                 currentClock;
    int			MCLK;
    int			dwords;
    int			h;
    int			x;
    int			w;
    int			y;
    int                 lcdMode;
    Bool                lcdActive;
    Bool                doInit;
#ifdef READOUT
    Bool                DontSetClock;
#endif
    TRIDENTRegRec	SavedReg;
    TRIDENTRegRec	ModeReg;
    I2CBusPtr		DDC;
    CARD16		EngineOperation;
    CARD32		PatternLocation;
    CARD32		BltScanDirection;
    CARD32		DrawFlag;
    CARD16		LinePattern;
    RamDacRecPtr	RamDacRec;
    xf86CursorInfoPtr	CursorInfoRec;
    xf86Int10InfoPtr	Int10;
    vbeInfoPtr		pVbe;
#ifdef VBE_INFO
    vbeModeInfoPtr	vbeModes;
#endif
    XAAInfoRecPtr	AccelInfoRec;
    CloseScreenProcPtr	CloseScreen;
    ScreenBlockHandlerProcPtr BlockHandler;
    int                 panelWidth;
    int                 panelHeight;
    unsigned int	(*ddc1Read)(ScrnInfoPtr);
    CARD8*		XAAScanlineColorExpandBuffers[2];
    CARD8*		XAAImageScanlineBuffer[1];
    void                (*InitializeAccelerator)(ScrnInfoPtr);
#ifdef XvExtension
    void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    XF86VideoAdaptorPtr adaptor;
    int                 videoKey;
    int			hsync;
    int			hsync_rskew;
    int			vsync;
    int			vsync_bskew;
    CARD32              videoFlags;
    int			keyOffset;
#endif
    int                 OverrideHsync;
    int                 OverrideVsync;
    int                 OverrideBskew;
    int                 OverrideRskew;
    OptionInfoPtr	Options;
    Bool		shadowNew;
    int			displaySize;
    int			dspOverride;
    Bool		GammaBrightnessOn;
    int			brightness;
    double		gamma;
    int			FPDelay;	/* just for debugging - will go away */
} TRIDENTRec, *TRIDENTPtr;

typedef struct {
    CARD8 mode;
    int display_x;
    int display_y;
    int clock;
    int shadow_0;
    int shadow_3;
    int shadow_4;
    int shadow_5;
    int shadow_6;
    int shadow_7;
    int shadow_10;
    int shadow_11;
    int shadow_16;
    int shadow_HiOrd;
} tridentLCD;

#define LCD_ACTIVE 0x01
#define CRT_ACTIVE 0x02

extern tridentLCD LCD[];

typedef struct {
    int x_res;
    int y_res;
    int mode;
} biosMode;

typedef struct {
  int x_res;
  int y_res;
  CARD8 GR5a;
  CARD8 GR5c;
} newModes;

/* Prototypes */

Bool TRIDENTClockSelect(ScrnInfoPtr pScrn, int no);
Bool TRIDENTSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
void TRIDENTAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool TRIDENTDGAInit(ScreenPtr pScreen);
Bool TRIDENTI2CInit(ScreenPtr pScreen);
void TRIDENTInitVideo(ScreenPtr pScreen);
void TRIDENTResetVideo(ScrnInfoPtr pScrn);
unsigned int Tridentddc1Read(ScrnInfoPtr pScrn);
void TVGARestore(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg);
void TVGASave(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg);
Bool TVGAInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
void TridentRestore(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg);
void TridentSave(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg);
Bool TridentInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
Bool TridentAccelInit(ScreenPtr pScreen);
Bool XPAccelInit(ScreenPtr pScreen);
Bool ImageAccelInit(ScreenPtr pScreen);
Bool BladeAccelInit(ScreenPtr pScreen);
Bool TridentHWCursorInit(ScreenPtr pScreen);
int TridentFindMode(int xres, int yres, int depth);
void TGUISetClock(ScrnInfoPtr pScrn, int clock, unsigned char *a, unsigned char *b);
void TGUISetMCLK(ScrnInfoPtr pScrn, int clock, unsigned char *a, unsigned char *b);
void tridentSetModeBIOS(ScrnInfoPtr pScrn, DisplayModePtr mode);
void TridentOutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data);
unsigned char TridentInIndReg(ScrnInfoPtr pScrn, CARD32 reg);
void TridentWriteAddress(ScrnInfoPtr pScrn, CARD32 index);
void TridentReadAddress(ScrnInfoPtr pScrn, CARD32 index);
void TridentWriteData(ScrnInfoPtr pScrn, unsigned char data);
unsigned char TridentReadData(ScrnInfoPtr pScrn);
void TridentLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO *colors, VisualPtr pVisual);
void TridentSetOverscan(ScrnInfoPtr pScrn, int overscan);
int TGUISetRead(ScreenPtr pScreen, int bank);
int TGUISetWrite(ScreenPtr pScreen, int bank);
int TGUISetReadWrite(ScreenPtr pScreen, int bank);
int TVGA8900SetRead(ScreenPtr pScreen, int bank);
int TVGA8900SetWrite(ScreenPtr pScreen, int bank);
int TVGA8900SetReadWrite(ScreenPtr pScreen, int bank);
void TridentFindClock(ScrnInfoPtr pScrn, int clock);
float CalculateMCLK(ScrnInfoPtr pScrn);
void TRIDENTRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void TRIDENTShadowUpdate (ScreenPtr pScreen, shadowBufPtr pBuf);
void TRIDENTPointerMoved(int index, int x, int y);
void TRIDENTRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void TRIDENTRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void TRIDENTRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void TRIDENTRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/*
 * Trident Chipset Definitions
 */

/* Supported chipsets */
typedef enum {
    TVGA8200LX,
    TVGA8800BR,
    TVGA8800CS,
    TVGA8900B,
    TVGA8900C,
    TVGA8900CL,
    TVGA8900D,
    TVGA9000,
    TVGA9000i,
    TVGA9100B,
    TVGA9200CXr,
    TGUI9400CXi,
    TGUI9420DGi,
    TGUI9430DGi,
    TGUI9440AGi,
    CYBER9320,
    TGUI9660,
    TGUI9680,
    PROVIDIA9682,
    CYBER9382,
    CYBER9385,
    PROVIDIA9685,
    CYBER9388,
    CYBER9397,
    CYBER9397DVD,
    CYBER9520,
    CYBER9525DVD,
    IMAGE975,
    IMAGE985,
    BLADE3D,
    CYBERBLADEI7,
    CYBERBLADEI7D,
    CYBERBLADEI1,
    CYBERBLADEI1D,
    CYBERBLADEAI1,
    CYBERBLADEAI1D,
    CYBERBLADEE4,
    BLADEXP,
    CYBERBLADEXPAI1
} TRIDENTType;

#define UseMMIO		(pTrident->NoMMIO == FALSE)

#define IsPciCard	(pTrident->pEnt->location.type == BUS_PCI)

#define IsPrimaryCard	((xf86IsPrimaryPci(pTrident->PciInfo)) || \
			 (xf86IsPrimaryIsa()))

#define HAS_DST_TRANS	((pTrident->Chipset == PROVIDIA9682) || \
			 (pTrident->Chipset == PROVIDIA9685) || \
			 (pTrident->Chipset == BLADEXP) || \
			 (pTrident->Chipset == CYBERBLADEXPAI1))

#define Is3Dchip	((pTrident->Chipset == CYBER9397) || \
			 (pTrident->Chipset == CYBER9397DVD) || \
			 (pTrident->Chipset == CYBER9520) || \
			 (pTrident->Chipset == CYBER9525DVD) || \
			 (pTrident->Chipset == CYBERBLADEE4)  || \
			 (pTrident->Chipset == IMAGE975)  || \
			 (pTrident->Chipset == IMAGE985)  || \
			 (pTrident->Chipset == CYBERBLADEI7)  || \
			 (pTrident->Chipset == CYBERBLADEI7D)  || \
			 (pTrident->Chipset == CYBERBLADEI1)  || \
			 (pTrident->Chipset == CYBERBLADEI1D)  || \
			 (pTrident->Chipset == CYBERBLADEAI1)  || \
			 (pTrident->Chipset == CYBERBLADEAI1D)  || \
			 (pTrident->Chipset == BLADE3D) || \
			 (pTrident->Chipset == CYBERBLADEXPAI1) || \
			 (pTrident->Chipset == BLADEXP))

/*
 * Trident DAC's
 */

#define TKD8001		0
#define TGUIDAC		1

/* 
 * Video Flags
 */

#define VID_ZOOM_INV 0x1
#define VID_ZOOM_MINI 0x2
#define VID_OFF_SHIFT_4 0x4
#define VID_ZOOM_NOMINI 0x8
#define VID_DOUBLE_LINEBUFFER_FOR_WIDE_SRC 0x10
#endif /* _TRIDENT_H_ */

