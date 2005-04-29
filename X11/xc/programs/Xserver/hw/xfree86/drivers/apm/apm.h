/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/apm/apm.h,v 1.20 2003/11/03 05:11:04 tsi Exp $ */


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

/* All drivers using the vgahw module need this */
#include "vgaHW.h"

/* Drivers using the mi banking wrapper need this */
#include "mibank.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

/* Needed for the 1 and 4 bpp framebuffers */
#include "xf1bpp.h"
#include "xf4bpp.h"

#include "fb.h"

/* Drivers using the XAA interface ... */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86Cursor.h"
#include "xf86fbman.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

/* I2C support */
#include "xf86i2c.h"

/* DDC support */
#include "xf86DDC.h"

#include "xf86xv.h"
#include "Xv.h"

#ifdef TRUE
#undef TRUE
#endif
#define TRUE	(1)

#define rdinx(port, ind)	(outb((port), (ind)), inb((port) + 1))
#define wrinx(port, ind, val)	outb((port), (ind)),  outb((port) + 1, (val))
#define modinx(port, ind, mask, bits)					\
    do {								\
	unsigned char tmp;						\
	tmp = (rdinx((port), (ind)) & ~(mask)) | ((bits) & (mask));	\
	wrinx((port), (ind), tmp);					\
    } while(0)

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned long	u32;

#define NoSEQRegs	0x20
#define NoCRTRegs	0x1F
#define NoGRCRegs	0x09
#define	NoATCRegs	0x15

enum {
    XR80, XRC0, XRD0, XRE0, XRE8, XREC, XR140, XR144, XR148, XR14C, NoEXRegs
};

typedef struct {
	unsigned char	SEQ[NoSEQRegs];
	unsigned char	CRT[NoCRTRegs];
	unsigned char	GRC[NoGRCRegs];
	unsigned char	ATC[NoATCRegs];
	unsigned int	EX[NoEXRegs];
} ApmRegStr, *ApmRegPtr;

typedef struct {
    int			displayWidth, displayHeight;
    int			bitsPerPixel, bytesPerScanline;
    int			depth, Scanlines;
    CARD32		mask32;		/* Mask to have 32bit aligned data */
    unsigned int	Setup_DEC;
    DisplayModePtr	pMode;
} ApmFBLayout;

#define APM_CACHE_NUMBER	32

typedef struct {
    pciVideoPtr		PciInfo;
    PCITAG		PciTag;
    int			scrnIndex;
    int			Chipset;
    int			ChipRev;
    CARD32		LinAddress;
    unsigned long	LinMapSize;
    CARD32		FbMapSize;
    pointer		LinMap;
    pointer		FbBase;
    char		*VGAMap;
    char		*MemMap;
    pointer		BltMap;
    Bool		UnlockCalled;
    IOADDRESS		iobase, xport, xbase;
    unsigned char	savedSR10;
    CARD8		MiscOut;
    CARD8		c9, d9, db, Rush;
    unsigned long	saveCmd;
    pointer		FontInfo;
    Bool		hwCursor;
    Bool		noLinear;
    ApmRegStr		ModeReg, SavedReg;
    CloseScreenProcPtr	CloseScreen;
    Bool		UsePCIRetry;  /* Do we use PCI-retry or busy-waiting */
    Bool		NoAccel;  /* Do we use XAA acceleration architecture */
    int			MinClock;                        /* Min ramdac clock */
    int			MaxClock;                        /* Max ramdac clock */
    ApmFBLayout		CurrentLayout, SavedLayout;
    EntityInfoPtr	pEnt;
    XAAInfoRecPtr	AccelInfoRec, DGAXAAInfo;
    xf86CursorInfoPtr	CursorInfoRec;
    int			DGAactive, numDGAModes;
    DGAModePtr		DGAModes;
    int			BaseCursorAddress,CursorAddress,DisplayedCursorAddress;
    int			OffscreenReserved;
    int			blitxdir, blitydir;
    Bool		apmTransparency, apmClip, ShadowFB, I2C;
    int			rop, Bg8x8, Fg8x8;
    I2CBusPtr		I2CPtr;
    struct ApmStippleCacheRec {
	XAACacheInfoRec		apmStippleCache;
	FBAreaPtr		area;
	unsigned int		apmStippleCached:1;
    }			apmCache[APM_CACHE_NUMBER];
    int			apmCachePtr;
    unsigned char	regcurr[0x54];
    ScreenPtr		pScreen;
    int			Generation;
    int			apmLock, pixelStride, RushY[7], CopyMode;
    int			PutImageStride;
    Bool		(*DestroyPixmap)(PixmapPtr);
    PixmapPtr		(*CreatePixmap)(ScreenPtr, int, int, int);
    void (*SetupForSolidFill)(ScrnInfoPtr pScrn, int color, int rop,
					unsigned int planemask);
    void (*SubsequentSolidFillRect)(ScrnInfoPtr pScrn, int x, int y,
				       int w, int h);
    void (*SetupForSolidFill24)(ScrnInfoPtr pScrn, int color, int rop,
					unsigned int planemask);
    void (*SubsequentSolidFillRect24)(ScrnInfoPtr pScrn, int x, int y,
				       int w, int h);
    void (*SetupForScreenToScreenCopy)(ScrnInfoPtr pScrn, int xdir, int ydir,
					  int rop, unsigned int planemask,
                                          int transparency_color);
    void (*SubsequentScreenToScreenCopy)(ScrnInfoPtr pScrn, int x1, int y1,
					    int x2, int y2, int w, int h);
    void (*SetupForScreenToScreenCopy24)(ScrnInfoPtr pScrn, int xdir, int ydir,
					  int rop, unsigned int planemask,
                                          int transparency_color);
    void (*SubsequentScreenToScreenCopy24)(ScrnInfoPtr pScrn, int x1, int y1,
					    int x2, int y2, int w, int h);
    int			MemClk;
    unsigned char	*ShadowPtr;
    int			ShadowPitch;
    memType		ScratchMem, ScratchMemSize, ScratchMemOffset;
    memType		ScratchMemPtr, ScratchMemEnd;
    int			ScratchMemWidth;
    CARD32		color;
    XF86VideoAdaptorPtr	adaptor;
    int			timerIsOn;
    Time		offTime;
    OptionInfoPtr	Options;
    char		DPMSMask[4];
} ApmRec, *ApmPtr;

#define curr		((unsigned char *)pApm->regcurr)

typedef struct {
    u16		ca;
    u8		font;
    u8		pad;
} ApmFontBuf;

typedef struct {
    u16		ca;
    u8		font;
    u8		pad;
    u16		ca2;
    u8		font2;
    u8		pad2;
} ApmTextBuf;

enum ApmChipId {
    AP6422	= 0x6422,
    AT24	= 0x6424,
    AT3D	= 0x643D
};

typedef struct {
    BoxRec			box;
    int				num;
    MoveAreaCallbackProcPtr	MoveAreaCallback;
    RemoveAreaCallbackProcPtr	RemoveAreaCallback;
    void			*devPriv;
} ApmPixmapRec, *ApmPixmapPtr;

#define APMDECL(p)	ApmPtr pApm = ((ApmPtr)(((ScrnInfoPtr)(p))->driverPrivate))
#define APMPTR(p)	((ApmPtr)(((ScrnInfoPtr)(p))->driverPrivate))

extern int	ApmHWCursorInit(ScreenPtr pScreen);
extern int	ApmDGAInit(ScreenPtr pScreen);
extern int	ApmAccelInit(ScreenPtr pScreen);
extern Bool	ApmI2CInit(ScrnInfoPtr pScrn);
extern void	XFree86RushExtensionInit(ScreenPtr pScreen);
extern void	ApmInitVideo(ScreenPtr pScreen);
extern void	ApmInitVideo_IOP(ScreenPtr pScreen);
extern void	ApmSetupXAAInfo(ApmPtr pApm, XAAInfoRecPtr pXAAinfo);
extern Bool     ApmSwitchMode(int scrnIndex, DisplayModePtr mode,
                                  int flags);
extern void     ApmAdjustFrame(int scrnIndex, int x, int y, int flags);
extern void	ApmHWCursorReserveSpace(ApmPtr pApm);
extern void	ApmAccelReserveSpace(ApmPtr pApm);

extern int	ApmPixmapIndex;
#define APM_GET_PIXMAP_PRIVATE(pix)\
	((ApmPixmapPtr)(((PixmapPtr)(pix))->devPrivates[ApmPixmapIndex].ptr))

#include "apm_regs.h"
