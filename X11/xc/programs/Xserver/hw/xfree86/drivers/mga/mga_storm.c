/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_storm.c,v 1.98 2003/01/16 16:09:10 eich Exp $ */


/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* For correct __inline__ usage */
#include "compiler.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that use XAA need this */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86fbman.h"
#include "miline.h"
#include "servermd.h"

#ifdef XF86DRI
#include "cfb.h"
#include "GL/glxtokens.h"
#endif

#include "mga_bios.h"
#include "mga.h"
#include "mga_reg.h"
#include "mga_map.h"
#include "mga_macros.h"

#ifdef XF86DRI
#include "mga_dri.h"
#endif

#define MGAMoveDWORDS(d,s,c) \
do { \
  write_mem_barrier(); \
  XAAMoveDWORDS((d),(s),(c)); \
} while (0)

static void MGANAME(SubsequentScreenToScreenCopy)(ScrnInfoPtr pScrn,
				int srcX, int srcY, int dstX, int dstY,
				int w, int h);
static void MGANAME(SubsequentScreenToScreenCopy_FastBlit)(ScrnInfoPtr pScrn,
				int srcX, int srcY, int dstX, int dstY,
				int w, int h);
static void MGANAME(SetupForScanlineCPUToScreenColorExpandFill)(
				ScrnInfoPtr pScrn, int fg,
				int bg, int rop, unsigned int planemask);
static void MGANAME(SubsequentScanlineCPUToScreenColorExpandFill)(
				ScrnInfoPtr pScrn,
				int x, int y, int w, int h, int skipleft);
static void MGANAME(SubsequentColorExpandScanline)(ScrnInfoPtr pScrn,
				int bufno);
static void MGANAME(SubsequentColorExpandScanlineIndirect)(ScrnInfoPtr pScrn,
                                int bufno);
static void MGANAME(SubsequentSolidFillRect)(ScrnInfoPtr pScrn,
					int x, int y, int w, int h);
static void MGANAME(SubsequentSolidFillTrap)(ScrnInfoPtr pScrn, int y, int h,
				int left, int dxL, int dyL, int eL,
				int right, int dxR, int dyR, int eR);
static void MGANAME(SubsequentSolidHorVertLine) (ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir);
static void MGANAME(SubsequentSolidTwoPointLine)(ScrnInfoPtr pScrn,
				int x1, int y1, int x2, int y2, int flags);
static void MGANAME(SetupForMono8x8PatternFill)(ScrnInfoPtr pScrn,
				int patx, int paty, int fg, int bg,
				int rop, unsigned int planemask);
static void MGANAME(SubsequentMono8x8PatternFillRect)(ScrnInfoPtr pScrn,
				int patx, int paty,
				int x, int y, int w, int h );
static void MGANAME(SubsequentMono8x8PatternFillRect_Additional)(
				ScrnInfoPtr pScrn, int patx, int paty,
				int x, int y, int w, int h );
static void MGANAME(SubsequentMono8x8PatternFillTrap)( ScrnInfoPtr pScrn,
				int patx, int paty, int y, int h,
				int left, int dxL, int dyL, int eL,
				int right, int dxR, int dyR, int eR);
static void MGANAME(SetupForScanlineImageWrite)(ScrnInfoPtr pScrn, int rop,
   				unsigned int planemask,
				int transparency_color, int bpp, int depth);
static void MGANAME(SubsequentScanlineImageWriteRect)(ScrnInfoPtr pScrn,
				int x, int y, int w, int h, int skipleft);
static void MGANAME(SubsequentImageWriteScanline)(ScrnInfoPtr pScrn, int num);
#if PSZ != 24
static void MGANAME(SetupForPlanarScreenToScreenColorExpandFill)(
				ScrnInfoPtr pScrn, int fg, int bg, int rop,
				unsigned int planemask);
static void MGANAME(SubsequentPlanarScreenToScreenColorExpandFill)(
				ScrnInfoPtr pScrn,
				int x, int y, int w, int h,
				int srcx, int srcy, int skipleft);
#endif
static void MGANAME(SetupForScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn,
				int fg, int bg, int rop,
				unsigned int planemask);
static void MGANAME(SubsequentScreenToScreenColorExpandFill)(ScrnInfoPtr pScrn,
				int x, int y, int w, int h,
				int srcx, int srcy, int skipleft);
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
static void MGANAME(SetupForDashedLine)(ScrnInfoPtr pScrn, int fg, int bg,
				int rop, unsigned int planemask, int length,
    				unsigned char *pattern);
static void MGANAME(SubsequentDashedTwoPointLine)(ScrnInfoPtr pScrn,
				int x1, int y1, int x2, int y2,
				int flags, int phase);
#endif
void MGANAME(RestoreAccelState)(ScrnInfoPtr pScrn);
#if PSZ == 8
void Mga16RestoreAccelState(ScrnInfoPtr pScrn);
void Mga24RestoreAccelState(ScrnInfoPtr pScrn);
void Mga32RestoreAccelState(ScrnInfoPtr pScrn);
#endif

#ifdef XF86DRI
void MGANAME(DRIInitBuffers)(WindowPtr pWin,
			     RegionPtr prgn, CARD32 index);
void MGANAME(DRIMoveBuffers)(WindowPtr pParent, DDXPointRec ptOldOrg,
			     RegionPtr prgnSrc, CARD32 index);

#endif

extern void MGASetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1,
				int x2, int y2);
extern void MGADisableClipping(ScrnInfoPtr pScrn);
extern void MGAFillSolidRectsDMA(ScrnInfoPtr pScrn, int fg, int rop,
				unsigned int planemask, int nBox, BoxPtr pBox);
extern void MGAFillSolidSpansDMA(ScrnInfoPtr pScrn, int fg, int rop,
				unsigned int planemask, int n, DDXPointPtr ppt,
 				int *pwidth, int fSorted);
extern void MGAFillMono8x8PatternRectsTwoPass(ScrnInfoPtr pScrn, int fg, int bg,
 				int rop, unsigned int planemask, int nBox,
 				BoxPtr pBox, int pattern0, int pattern1,
				int xorigin, int yorigin);
extern void MGAValidatePolyArc(GCPtr, unsigned long, DrawablePtr);
extern void MGAValidatePolyPoint(GCPtr, unsigned long, DrawablePtr);
extern void MGAFillCacheBltRects(ScrnInfoPtr, int, unsigned int, int, BoxPtr,
				int, int, XAACacheInfoPtr);

#ifdef RENDER

extern Bool
MGASetupForCPUToScreenAlphaTexture (
	ScrnInfoPtr	pScrn,
	int		op,
	CARD16		red,
	CARD16		green,
	CARD16		blue,
	CARD16		alpha,
	int		alphaType,
	CARD8		*alphaPtr,
	int		alphaPitch,
	int		width,
	int		height,
	int		flags
);

extern Bool
MGASetupForCPUToScreenAlphaTextureFaked (
	ScrnInfoPtr	pScrn,
	int		op,
	CARD16		red,
	CARD16		green,
	CARD16		blue,
	CARD16		alpha,
	int		alphaType,
	CARD8		*alphaPtr,
	int		alphaPitch,
	int		width,
	int		height,
	int		flags
);


extern Bool
MGASetupForCPUToScreenTexture (
	ScrnInfoPtr	pScrn,
	int		op,
	int		texType,
	CARD8		*texPtr,
	int		texPitch,
	int		width,
	int		height,
	int		flags
);

extern void
MGASubsequentCPUToScreenTexture (
	ScrnInfoPtr	pScrn,
	int		dstx,
	int		dsty,
	int		srcx,
	int		srcy,
	int		width,
	int		height
);

extern CARD32 MGAAlphaTextureFormats[2];
extern CARD32 MGATextureFormats[2];

#if PSZ == 8
#include "mipict.h"
#include "dixstruct.h"

CARD32 MGAAlphaTextureFormats[2] = {PICT_a8, 0};
CARD32 MGATextureFormats[2] = {PICT_a8r8g8b8, 0};

static void
RemoveLinear (FBLinearPtr linear)
{
   MGAPtr pMga = (MGAPtr)(linear->devPrivate.ptr);

   pMga->LinearScratch = NULL;  /* just lost our scratch */
}

static void
RenderCallback (ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);

    if((currentTime.milliseconds > pMga->RenderTime) && pMga->LinearScratch) {
	xf86FreeOffscreenLinear(pMga->LinearScratch);
	pMga->LinearScratch = NULL;
    }

    if(!pMga->LinearScratch)
	pMga->RenderCallback = NULL;
}

#define RENDER_DELAY	15000

static Bool
AllocateLinear (
   ScrnInfoPtr pScrn,
   int sizeNeeded
){
   MGAPtr pMga = MGAPTR(pScrn);

   pMga->RenderTime = currentTime.milliseconds + RENDER_DELAY;
   pMga->RenderCallback = RenderCallback;

   if(pMga->LinearScratch) {
	if(pMga->LinearScratch->size >= sizeNeeded)
	   return TRUE;
	else {
	   if(xf86ResizeOffscreenLinear(pMga->LinearScratch, sizeNeeded))
		return TRUE;

	   xf86FreeOffscreenLinear(pMga->LinearScratch);
	   pMga->LinearScratch = NULL;
	}
   }

   pMga->LinearScratch = xf86AllocateOffscreenLinear(
				pScrn->pScreen, sizeNeeded, 32,
				NULL, RemoveLinear, pMga);

   return (pMga->LinearScratch != NULL);
}

static int
GetPowerOfTwo(int w)
{
    int Pof2 = 0;
    int i = 12;

    while(--i) {
        if(w & (1 << i)) {
            Pof2 = i;
            if(w & ((1 << i) - 1))
                Pof2++;
            break;
        }
    }
    return Pof2;
}


static int tex_padw, tex_padh;

Bool
MGASetupForCPUToScreenAlphaTextureFaked (
   ScrnInfoPtr	pScrn,
   int		op,
   CARD16	red,
   CARD16	green,
   CARD16	blue,
   CARD16	alpha,
   int		alphaType,
   CARD8	*alphaPtr,
   int		alphaPitch,
   int		width,
   int		height,
   int		flags
){
    int log2w, log2h, i, pitch, sizeNeeded, offset;
    MGAPtr pMga = MGAPTR(pScrn);

    if(op != PictOpOver)  /* only one tested */
	return FALSE;

    if((width > 2048) || (height > 2048))
	return FALSE;

    log2w = GetPowerOfTwo(width);
    log2h = GetPowerOfTwo(height);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(pMga->Overlay8Plus24) {
        i = 0x00ffffff;
        WAITFIFO(1);
        SET_PLANEMASK(i);
    }

    pitch = (width + 15) & ~15;
    sizeNeeded = pitch * height;
    if(pScrn->bitsPerPixel == 16)
	sizeNeeded <<= 1;

    if(!AllocateLinear(pScrn, sizeNeeded))
	return FALSE;

    offset = pMga->LinearScratch->offset << 1;
    if(pScrn->bitsPerPixel == 32)
        offset <<= 1;

    if(pMga->AccelInfoRec->NeedToSync)
	MGAStormSync(pScrn);

    XAA_888_plus_PICT_a8_to_8888(
	(blue >> 8) | (green & 0xff00) | ((red & 0xff00) << 8),
	alphaPtr, alphaPitch, (CARD32*)(pMga->FbStart + offset),
        pitch, width, height);

    tex_padw = 1 << log2w;
    tex_padh = 1 << log2h;

    WAITFIFO(15);
    OUTREG(MGAREG_TMR0, (1 << 20) / tex_padw);  /* sx inc */
    OUTREG(MGAREG_TMR1, 0);  /* sy inc */
    OUTREG(MGAREG_TMR2, 0);  /* tx inc */
    OUTREG(MGAREG_TMR3, (1 << 20) / tex_padh);  /* ty inc */
    OUTREG(MGAREG_TMR4, 0x00000000);
    OUTREG(MGAREG_TMR5, 0x00000000);
    OUTREG(MGAREG_TMR8, 0x00010000);
    OUTREG(MGAREG_TEXORG, offset);
    OUTREG(MGAREG_TEXWIDTH,  log2w | (((8 - log2w) & 63) << 9) |
                                ((width - 1) << 18));
    OUTREG(MGAREG_TEXHEIGHT, log2h | (((8 - log2h) & 63) << 9) |
                                ((height - 1) << 18));
    OUTREG(MGAREG_TEXCTL, 0x1A000106 | ((pitch & 0x07FF) << 9));
    OUTREG(MGAREG_TEXCTL2, 0x00000014);
    OUTREG(MGAREG_DWGCTL, 0x000c7076);
    OUTREG(MGAREG_TEXFILTER, 0x01e00020);
    OUTREG(MGAREG_ALPHACTRL, 0x00000154);

    return TRUE;
}

Bool
MGASetupForCPUToScreenAlphaTexture (
   ScrnInfoPtr	pScrn,
   int		op,
   CARD16	red,
   CARD16	green,
   CARD16	blue,
   CARD16	alpha,
   int		alphaType,
   CARD8	*alphaPtr,
   int		alphaPitch,
   int		width,
   int		height,
   int		flags
){
    int log2w, log2h, i, pitch, sizeNeeded, offset;
    CARD8 *dst;
    MGAPtr pMga = MGAPTR(pScrn);

    if(op != PictOpOver)  /* only one tested */
	return FALSE;

    if((width > 2048) || (height > 2048))
	return FALSE;

    log2w = GetPowerOfTwo(width);
    log2h = GetPowerOfTwo(height);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(pMga->Overlay8Plus24) {
        i = 0x00ffffff;
        WAITFIFO(1);
        SET_PLANEMASK(i);
    }

    pitch = (width + 15) & ~15;
    sizeNeeded = (pitch * height) >> 1;
    if(pScrn->bitsPerPixel == 32)
	sizeNeeded >>= 1;

    if(!AllocateLinear(pScrn, sizeNeeded))
	return FALSE;

    offset = pMga->LinearScratch->offset << 1;
    if(pScrn->bitsPerPixel == 32)
        offset <<= 1;

    if(pMga->AccelInfoRec->NeedToSync)
	MGAStormSync(pScrn);

    i = height;
    dst = pMga->FbStart + offset;
    while(i--) {
	memcpy(dst, alphaPtr, width);
	dst += pitch;
	alphaPtr += alphaPitch;
    }

    tex_padw = 1 << log2w;
    tex_padh = 1 << log2h;


    WAITFIFO(12);
    OUTREG(MGAREG_DR4, red << 7);  /* red start */
    OUTREG(MGAREG_DR6, 0);
    OUTREG(MGAREG_DR7, 0);
    OUTREG(MGAREG_DR8, green << 7);  /* green start */
    OUTREG(MGAREG_DR10, 0);
    OUTREG(MGAREG_DR11, 0);
    OUTREG(MGAREG_DR12, blue << 7);  /* blue start */
    OUTREG(MGAREG_DR14, 0);
    OUTREG(MGAREG_DR15, 0);
    OUTREG(MGAREG_ALPHASTART, alpha << 7);  /* alpha start */
    OUTREG(MGAREG_ALPHAXINC, 0);
    OUTREG(MGAREG_ALPHAYINC, 0);

    WAITFIFO(15);
    OUTREG(MGAREG_TMR0, (1 << 20) / tex_padw);  /* sx inc */
    OUTREG(MGAREG_TMR1, 0);  /* sy inc */
    OUTREG(MGAREG_TMR2, 0);  /* tx inc */
    OUTREG(MGAREG_TMR3, (1 << 20) / tex_padh);  /* ty inc */
    OUTREG(MGAREG_TMR4, 0x00000000);
    OUTREG(MGAREG_TMR5, 0x00000000);
    OUTREG(MGAREG_TMR8, 0x00010000);
    OUTREG(MGAREG_TEXORG, offset);
    OUTREG(MGAREG_TEXWIDTH,  log2w | (((8 - log2w) & 63) << 9) |
                                ((width - 1) << 18));
    OUTREG(MGAREG_TEXHEIGHT, log2h | (((8 - log2h) & 63) << 9) |
                                ((height - 1) << 18));
    OUTREG(MGAREG_TEXCTL, 0x3A000107 | ((pitch & 0x07FF) << 9));
    OUTREG(MGAREG_TEXCTL2, 0x00000014);
    OUTREG(MGAREG_DWGCTL, 0x000c7076);
    OUTREG(MGAREG_TEXFILTER, 0x01e00020);
    OUTREG(MGAREG_ALPHACTRL, 0x02000151);

    return TRUE;
}


Bool
MGASetupForCPUToScreenTexture (
   ScrnInfoPtr	pScrn,
   int		op,
   int		texType,
   CARD8	*texPtr,
   int		texPitch,
   int		width,
   int		height,
   int		flags
){
    int log2w, log2h, i, pitch, sizeNeeded, offset;
    MGAPtr pMga = MGAPTR(pScrn);

    if(op != PictOpOver)  /* only one tested */
	return FALSE;

    if((width > 2048) || (height > 2048))
	return FALSE;

    log2w = GetPowerOfTwo(width);
    log2h = GetPowerOfTwo(height);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(pMga->Overlay8Plus24) {
        i = 0x00ffffff;
        WAITFIFO(1);
        SET_PLANEMASK(i);
    }

    pitch = (width + 15) & ~15;
    sizeNeeded = pitch * height;
    if(pScrn->bitsPerPixel == 16)
	sizeNeeded <<= 1;

    if(!AllocateLinear(pScrn, sizeNeeded))
	return FALSE;

    offset = pMga->LinearScratch->offset << 1;
    if(pScrn->bitsPerPixel == 32)
        offset <<= 1;

    if(pMga->AccelInfoRec->NeedToSync)
	MGAStormSync(pScrn);

    {
	CARD8 *dst = (CARD8*)(pMga->FbStart + offset);
	i = height;
	while(i--) {
            memcpy(dst, texPtr, width << 2);
	    texPtr += texPitch;
	    dst += pitch << 2;
	}
    }

    tex_padw = 1 << log2w;
    tex_padh = 1 << log2h;

    WAITFIFO(15);
    OUTREG(MGAREG_TMR0, (1 << 20) / tex_padw);  /* sx inc */
    OUTREG(MGAREG_TMR1, 0);  /* sy inc */
    OUTREG(MGAREG_TMR2, 0);  /* tx inc */
    OUTREG(MGAREG_TMR3, (1 << 20) / tex_padh);  /* ty inc */
    OUTREG(MGAREG_TMR4, 0x00000000);
    OUTREG(MGAREG_TMR5, 0x00000000);
    OUTREG(MGAREG_TMR8, 0x00010000);
    OUTREG(MGAREG_TEXORG, offset);
    OUTREG(MGAREG_TEXWIDTH,  log2w | (((8 - log2w) & 63) << 9) |
                                ((width - 1) << 18));
    OUTREG(MGAREG_TEXHEIGHT, log2h | (((8 - log2h) & 63) << 9) |
                                ((height - 1) << 18));
    OUTREG(MGAREG_TEXCTL, 0x1A000106 | ((pitch & 0x07FF) << 9));
    OUTREG(MGAREG_TEXCTL2, 0x00000014);
    OUTREG(MGAREG_DWGCTL, 0x000c7076);
    OUTREG(MGAREG_TEXFILTER, 0x01e00020);
    OUTREG(MGAREG_ALPHACTRL, 0x00000151);

    return TRUE;
}
void
MGASubsequentCPUToScreenTexture (
    ScrnInfoPtr	pScrn,
    int		dstx,
    int		dsty,
    int		srcx,
    int		srcy,
    int		width,
    int		height
){
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(4);
    OUTREG(MGAREG_TMR6, (srcx << 20) / tex_padw);
    OUTREG(MGAREG_TMR7, (srcy << 20) / tex_padh);
    OUTREG(MGAREG_FXBNDRY, ((dstx + width) << 16) | (dstx & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dsty << 16) | height);

    pMga->AccelInfoRec->NeedToSync = TRUE;
}


#endif
#endif

Bool
MGANAME(AccelInit)(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    int maxFastBlitMem, maxlines;
    Bool doRender = FALSE;
    BoxRec AvailFBArea;

    pMga->ScratchBuffer = xalloc(((pScrn->displayWidth * PSZ) + 127) >> 3);
    if(!pMga->ScratchBuffer) return FALSE;

    pMga->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if(!infoPtr) return FALSE;

    pMga->RenderTime = 0;
    pMga->LinearScratch = 0;
    
    pMga->MaxFastBlitY = 0;
    pMga->MaxBlitDWORDS = 0x40000 >> 5;

    switch (pMga->Chipset) {
    case PCI_CHIP_MGA2064:
    	pMga->AccelFlags = BLK_OPAQUE_EXPANSION | FASTBLT_BUG;
	break;
    case PCI_CHIP_MGA2164:
    case PCI_CHIP_MGA2164_AGP:
    	pMga->AccelFlags = BLK_OPAQUE_EXPANSION |
			   TRANSC_SOLID_FILL |
 			   USE_RECTS_FOR_LINES;
        break;
    case PCI_CHIP_MGAG400:
    case PCI_CHIP_MGAG550:
        if(pMga->SecondCrtc == TRUE) {
	    pMga->HasFBitBlt = FALSE;
	}
        pMga->MaxBlitDWORDS = 0x400000 >> 5;
	/* fallthrough */
    case PCI_CHIP_MGAG200:
    case PCI_CHIP_MGAG200_PCI:
	doRender = FALSE;
        pMga->AccelFlags = TRANSC_SOLID_FILL |
			   TWO_PASS_COLOR_EXPAND;

#if 1
	if((pMga->FbMapSize > 8*1024*1024) && (pScrn->depth == 8))
	   pMga->AccelFlags |= LARGE_ADDRESSES;
#endif
        break;
    case PCI_CHIP_MGA1064:
	pMga->AccelFlags = 0;
        break;
    case PCI_CHIP_MGAG100:
    case PCI_CHIP_MGAG100_PCI:
    default:
	pMga->AccelFlags = MGA_NO_PLANEMASK;
        break;
    }

    /* all should be able to use this now with the bug fixes */
    pMga->AccelFlags |= USE_LINEAR_EXPANSION;

#if PSZ == 24
    pMga->AccelFlags |= MGA_NO_PLANEMASK;
#endif

    if(pMga->HasSDRAM) {
	pMga->Atype = pMga->AtypeNoBLK = MGAAtypeNoBLK;
	pMga->AccelFlags &= ~TWO_PASS_COLOR_EXPAND;
    } else {
	pMga->Atype = MGAAtype;
	pMga->AtypeNoBLK = MGAAtypeNoBLK;
    }

    /* fill out infoPtr here */
    infoPtr->Flags = 	PIXMAP_CACHE |
			OFFSCREEN_PIXMAPS |
			LINEAR_FRAMEBUFFER |
			MICROSOFT_ZERO_LINE_BIAS;

    /* sync */
    infoPtr->Sync = MGAStormSync;

    /* screen to screen copy */
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;
    infoPtr->SetupForScreenToScreenCopy =
        	MGANAME(SetupForScreenToScreenCopy);
    infoPtr->SubsequentScreenToScreenCopy =
        	MGANAME(SubsequentScreenToScreenCopy);

    if(pMga->HasFBitBlt) {
	infoPtr->FillCacheBltRects = MGAFillCacheBltRects;
	infoPtr->FillCacheBltRectsFlags = NO_TRANSPARENCY;
    }
    /* solid fills */
    infoPtr->SetupForSolidFill = MGANAME(SetupForSolidFill);
    infoPtr->SubsequentSolidFillRect = MGANAME(SubsequentSolidFillRect);
    infoPtr->SubsequentSolidFillTrap = MGANAME(SubsequentSolidFillTrap);

    /* solid lines */
    infoPtr->SetupForSolidLine = infoPtr->SetupForSolidFill;
    infoPtr->SubsequentSolidHorVertLine =
		MGANAME(SubsequentSolidHorVertLine);
    infoPtr->SubsequentSolidTwoPointLine =
		MGANAME(SubsequentSolidTwoPointLine);

    /* clipping */
    infoPtr->SetClippingRectangle = MGASetClippingRectangle;
    infoPtr->DisableClipping = MGADisableClipping;
    infoPtr->ClippingFlags = 	HARDWARE_CLIP_SOLID_LINE  |
				HARDWARE_CLIP_DASHED_LINE |
				HARDWARE_CLIP_SOLID_FILL  |
				HARDWARE_CLIP_MONO_8x8_FILL;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    /* dashed lines */
    infoPtr->DashedLineFlags = LINE_PATTERN_MSBFIRST_LSBJUSTIFIED;
    infoPtr->SetupForDashedLine = MGANAME(SetupForDashedLine);
    infoPtr->SubsequentDashedTwoPointLine =
		MGANAME(SubsequentDashedTwoPointLine);
    infoPtr->DashPatternMaxLength = 128;
#endif

    /* 8x8 mono patterns */
    infoPtr->Mono8x8PatternFillFlags = HARDWARE_PATTERN_PROGRAMMED_BITS |
					HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
					HARDWARE_PATTERN_SCREEN_ORIGIN |
					BIT_ORDER_IN_BYTE_MSBFIRST;
    infoPtr->SetupForMono8x8PatternFill = MGANAME(SetupForMono8x8PatternFill);
    infoPtr->SubsequentMono8x8PatternFillRect =
		MGANAME(SubsequentMono8x8PatternFillRect);
    infoPtr->SubsequentMono8x8PatternFillTrap =
		MGANAME(SubsequentMono8x8PatternFillTrap);

    /* cpu to screen color expansion */
    infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
					CPU_TRANSFER_PAD_DWORD |
					SCANLINE_PAD_DWORD |
#if X_BYTE_ORDER == X_BIG_ENDIAN
					BIT_ORDER_IN_BYTE_MSBFIRST |
#else
					BIT_ORDER_IN_BYTE_LSBFIRST |
#endif
					LEFT_EDGE_CLIPPING |
					LEFT_EDGE_CLIPPING_NEGATIVE_X;

    if(pMga->ILOADBase) {
	pMga->ColorExpandBase = pMga->ILOADBase;
    } else {
	pMga->ColorExpandBase = pMga->IOBase;
    }
    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
		MGANAME(SetupForScanlineCPUToScreenColorExpandFill);
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill =
		MGANAME(SubsequentScanlineCPUToScreenColorExpandFill);
    infoPtr->SubsequentColorExpandScanline =
		MGANAME(SubsequentColorExpandScanline);
    infoPtr->NumScanlineColorExpandBuffers = 1;
    infoPtr->ScanlineColorExpandBuffers = &(pMga->ColorExpandBase);

    /* screen to screen color expansion */
    if(pMga->AccelFlags & USE_LINEAR_EXPANSION) {
	infoPtr->ScreenToScreenColorExpandFillFlags =
						BIT_ORDER_IN_BYTE_LSBFIRST;
	infoPtr->SetupForScreenToScreenColorExpandFill =
		MGANAME(SetupForScreenToScreenColorExpandFill);
	infoPtr->SubsequentScreenToScreenColorExpandFill =
		MGANAME(SubsequentScreenToScreenColorExpandFill);
    } else {
#if PSZ != 24
    /* Alternate (but slower) planar expansions */
	infoPtr->SetupForScreenToScreenColorExpandFill =
		MGANAME(SetupForPlanarScreenToScreenColorExpandFill);
	infoPtr->SubsequentScreenToScreenColorExpandFill =
		MGANAME(SubsequentPlanarScreenToScreenColorExpandFill);
	infoPtr->CacheColorExpandDensity = PSZ;
	infoPtr->CacheMonoStipple = XAACachePlanarMonoStipple;
	/* It's faster to blit the stipples if you have fastbilt */
	if(pMga->HasFBitBlt)
	    infoPtr->ScreenToScreenColorExpandFillFlags = TRANSPARENCY_ONLY;
#endif
    }

    /* image writes */
    infoPtr->ScanlineImageWriteFlags = 	CPU_TRANSFER_PAD_DWORD |
					SCANLINE_PAD_DWORD |
					LEFT_EDGE_CLIPPING |
					LEFT_EDGE_CLIPPING_NEGATIVE_X |
					NO_TRANSPARENCY |
					NO_GXCOPY;

    infoPtr->SetupForScanlineImageWrite =
		MGANAME(SetupForScanlineImageWrite);
    infoPtr->SubsequentScanlineImageWriteRect =
		MGANAME(SubsequentScanlineImageWriteRect);
    infoPtr->SubsequentImageWriteScanline =
		MGANAME(SubsequentImageWriteScanline);
    infoPtr->NumScanlineImageWriteBuffers = 1;
    infoPtr->ScanlineImageWriteBuffers = &(pMga->ScratchBuffer);


    /* midrange replacements */

    if(pMga->ILOADBase && pMga->UsePCIRetry && infoPtr->SetupForSolidFill) {
	infoPtr->FillSolidRects = MGAFillSolidRectsDMA;
	infoPtr->FillSolidSpans = MGAFillSolidSpansDMA;
    }

    if(pMga->AccelFlags & TWO_PASS_COLOR_EXPAND) {
	if(infoPtr->SetupForMono8x8PatternFill)
	    infoPtr->FillMono8x8PatternRects =
				MGAFillMono8x8PatternRectsTwoPass;
    }

    if(infoPtr->SetupForSolidFill) {
	infoPtr->ValidatePolyArc = MGAValidatePolyArc;
	infoPtr->PolyArcMask = GCFunction | GCLineWidth | GCPlaneMask |
				GCLineStyle | GCFillStyle;
	infoPtr->ValidatePolyPoint = MGAValidatePolyPoint;
	infoPtr->PolyPointMask = GCFunction | GCPlaneMask;
    }
    if(pMga->AccelFlags & MGA_NO_PLANEMASK) {
	infoPtr->ScanlineImageWriteFlags |= NO_PLANEMASK;
	infoPtr->ScreenToScreenCopyFlags |= NO_PLANEMASK;
	infoPtr->ScanlineCPUToScreenColorExpandFillFlags |= NO_PLANEMASK;
	infoPtr->SolidFillFlags |= NO_PLANEMASK;
	infoPtr->SolidLineFlags |= NO_PLANEMASK;
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
	infoPtr->DashedLineFlags |= NO_PLANEMASK;
#endif
	infoPtr->Mono8x8PatternFillFlags |= NO_PLANEMASK;
	infoPtr->ScreenToScreenColorExpandFillFlags |= NO_PLANEMASK;
	infoPtr->FillSolidRectsFlags |= NO_PLANEMASK;
	infoPtr->FillSolidSpansFlags |= NO_PLANEMASK;
	infoPtr->FillMono8x8PatternRectsFlags |= NO_PLANEMASK;
	infoPtr->FillCacheBltRectsFlags |= NO_PLANEMASK;
    }


    maxFastBlitMem = (pMga->Interleave ? 4096 : 2048) * 1024;

    if(pMga->FbMapSize > maxFastBlitMem) {
	pMga->MaxFastBlitY = maxFastBlitMem / (pScrn->displayWidth * PSZ / 8);
    }

    maxlines = (min(pMga->FbUsableSize, 16*1024*1024)) /
	               (pScrn->displayWidth * PSZ / 8);

#ifdef XF86DRI
    if ( pMga->directRenderingEnabled ) {
       MGADRIServerPrivatePtr pMGADRIServer = pMga->DRIServerInfo;
       BoxRec MemBox;
       int cpp = pScrn->bitsPerPixel / 8;
       int widthBytes = pScrn->displayWidth * cpp;
       int bufferSize = ((pScrn->virtualY * widthBytes + MGA_BUFFER_ALIGN)
			 & ~MGA_BUFFER_ALIGN);
       int scanlines;

       pMGADRIServer->frontOffset = 0;
       pMGADRIServer->frontPitch = widthBytes;

       /* Try for front, back, depth, and two framebuffers worth of
	* pixmap cache.  Should be enough for a fullscreen background
	* image plus some leftovers.
	*/
       pMGADRIServer->textureSize = pMga->FbMapSize - 5 * bufferSize;

       /* If that gives us less than half the available memory, let's
	* be greedy and grab some more.  Sorry, I care more about 3D
	* performance than playing nicely, and you'll get around a full
	* framebuffer's worth of pixmap cache anyway.
	*/
       if ( pMGADRIServer->textureSize < (int)pMga->FbMapSize / 2 ) {
	  pMGADRIServer->textureSize = pMga->FbMapSize - 4 * bufferSize;
       }

       /* Check to see if there is more room available after the maximum
	* scanline for textures.
	*/
       if ( (int)pMga->FbMapSize - maxlines * widthBytes - bufferSize * 2
	    > pMGADRIServer->textureSize ) {
	  pMGADRIServer->textureSize = (pMga->FbMapSize -
					maxlines * widthBytes -
					bufferSize * 2);
       }

       /* Set a minimum usable local texture heap size.  This will fit
	* two 256x256x32bpp textures.
	*/
       if ( pMGADRIServer->textureSize < 512 * 1024 ) {
	  pMGADRIServer->textureOffset = 0;
	  pMGADRIServer->textureSize = 0;
       }

       /* Reserve space for textures */
       pMGADRIServer->textureOffset = (pMga->FbMapSize -
				       pMGADRIServer->textureSize +
				       MGA_BUFFER_ALIGN) & ~MGA_BUFFER_ALIGN;

       /* Reserve space for the shared depth buffer */
       pMGADRIServer->depthOffset = (pMGADRIServer->textureOffset -
				     bufferSize +
				     MGA_BUFFER_ALIGN) & ~MGA_BUFFER_ALIGN;
       pMGADRIServer->depthPitch = widthBytes;

       /* Reserve space for the shared back buffer */
       pMGADRIServer->backOffset = (pMGADRIServer->depthOffset - bufferSize +
				    MGA_BUFFER_ALIGN) & ~MGA_BUFFER_ALIGN;
       pMGADRIServer->backPitch = widthBytes;

       scanlines = pMGADRIServer->backOffset / widthBytes - 1;
       if ( scanlines > maxlines ) scanlines = maxlines;

       MemBox.x1 = 0;
       MemBox.y1 = 0;
       MemBox.x2 = pScrn->displayWidth;
       MemBox.y2 = scanlines;

       if ( !xf86InitFBManager( pScreen, &MemBox ) ) {
	  xf86DrvMsg( pScrn->scrnIndex, X_ERROR,
		      "Memory manager initialization to (%d,%d) (%d,%d) failed\n",
		      MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2 );
	  return FALSE;
       } else {
	  int width, height;

	  xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		      "Memory manager initialized to (%d,%d) (%d,%d)\n",
		      MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2 );

	  if ( xf86QueryLargestOffscreenArea( pScreen, &width,
					      &height, 0, 0, 0 ) ) {
	     xf86DrvMsg( pScrn->scrnIndex, X_INFO,
			 "Largest offscreen area available: %d x %d\n",
			 width, height );
	  }
       }

       xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		   "Reserved back buffer at offset 0x%x\n",
		   pMGADRIServer->backOffset );
       xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		   "Reserved depth buffer at offset 0x%x\n",
		   pMGADRIServer->depthOffset );
       xf86DrvMsg( pScrn->scrnIndex, X_INFO,
		   "Reserved %d kb for textures at offset 0x%x\n",
		   pMGADRIServer->textureSize/1024,
		   pMGADRIServer->textureOffset );
    }
    else
#endif
    {
       AvailFBArea.x1 = 0;
       AvailFBArea.x2 = pScrn->displayWidth;
       AvailFBArea.y1 = 0;
       AvailFBArea.y2 = maxlines;

       /*
	* Need to keep a strip of memory to the right of screen to workaround
	* a display problem with the second CRTC.
	*/
       if (pMga->SecondCrtc)
	  AvailFBArea.x2 = pScrn->virtualX;

       xf86InitFBManager(pScreen, &AvailFBArea);
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using %d lines for offscreen "
		  "memory.\n",
		  maxlines - pScrn->virtualY);

    }

    {
	Bool shared_accel = FALSE;
	int i;

	for(i = 0; i < pScrn->numEntities; i++) {
	    if(xf86IsEntityShared(pScrn->entityList[i]))
		shared_accel = TRUE;
	}
	if(shared_accel == TRUE)
	    infoPtr->RestoreAccelState = MGANAME(RestoreAccelState);
    }

#ifdef RENDER
   if(doRender && ((pScrn->bitsPerPixel == 32) || (pScrn->bitsPerPixel == 16)))
   {
       if(pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550) {
           infoPtr->CPUToScreenAlphaTextureFlags = XAA_RENDER_NO_TILE;
           infoPtr->SetupForCPUToScreenAlphaTexture =
				MGASetupForCPUToScreenAlphaTexture;
       } else {
           infoPtr->CPUToScreenAlphaTextureFlags = XAA_RENDER_NO_TILE |
					       XAA_RENDER_NO_SRC_ALPHA;
           infoPtr->SetupForCPUToScreenAlphaTexture =
				MGASetupForCPUToScreenAlphaTextureFaked;
       }
       infoPtr->SubsequentCPUToScreenAlphaTexture =
				MGASubsequentCPUToScreenTexture;
       infoPtr->CPUToScreenAlphaTextureFormats = MGAAlphaTextureFormats;

       infoPtr->SetupForCPUToScreenTexture = MGASetupForCPUToScreenTexture;
       infoPtr->SubsequentCPUToScreenTexture = MGASubsequentCPUToScreenTexture;
       infoPtr->CPUToScreenTextureFlags = XAA_RENDER_NO_TILE;
       infoPtr->CPUToScreenTextureFormats = MGATextureFormats;
    }
#endif

    return(XAAInit(pScreen, infoPtr));
}

void
MGANAME(InitSolidFillRectFuncs)(MGAPtr pMga)
{
    pMga->SetupForSolidFill = MGANAME(SetupForSolidFill);
    pMga->SubsequentSolidFillRect = MGANAME(SubsequentSolidFillRect);
}

/* Support for multiscreen */
void
MGANAME(RestoreAccelState)(ScrnInfoPtr pScrn)
{
   MGAPtr pMga = MGAPTR(pScrn);
   MGAFBLayout *pLayout = &pMga->CurrentLayout;
   CARD32 tmp;

   MGAStormSync(pScrn);
   WAITFIFO(12);
   pMga->SrcOrg = 0;
   OUTREG(MGAREG_MACCESS, pMga->MAccess);
   OUTREG(MGAREG_PITCH, pLayout->displayWidth);
   OUTREG(MGAREG_YDSTORG, pMga->YDstOrg);
   tmp = pMga->PlaneMask;
   pMga->PlaneMask = ~tmp;
   SET_PLANEMASK(tmp);
   tmp = pMga->BgColor;
   pMga->BgColor = ~tmp;
   SET_BACKGROUND(tmp);
   tmp = pMga->FgColor;
   pMga->FgColor = ~tmp;
   SET_FOREGROUND(tmp);
   OUTREG(MGAREG_SRCORG, pMga->realSrcOrg);
   OUTREG(MGAREG_DSTORG, pMga->DstOrg);
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
   OUTREG(MGAREG_OPMODE, MGAOPM_DMA_BLIT );
#else
   OUTREG(MGAREG_OPMODE, MGAOPM_DMA_BLIT | 0x10000);
#endif
   OUTREG(MGAREG_CXBNDRY, 0xFFFF0000); /* (maxX << 16) | minX */
   OUTREG(MGAREG_YTOP, 0x00000000);    /* minPixelPointer */
   OUTREG(MGAREG_YBOT, 0x007FFFFF);    /* maxPixelPointer */
   pMga->AccelFlags &= ~CLIPPER_ON;
}

#if PSZ == 8

CARD32 MGAAtype[16] = {
   MGADWG_RPL  | 0x00000000, MGADWG_RSTR | 0x00080000,
   MGADWG_RSTR | 0x00040000, MGADWG_BLK  | 0x000c0000,
   MGADWG_RSTR | 0x00020000, MGADWG_RSTR | 0x000a0000,
   MGADWG_RSTR | 0x00060000, MGADWG_RSTR | 0x000e0000,
   MGADWG_RSTR | 0x00010000, MGADWG_RSTR | 0x00090000,
   MGADWG_RSTR | 0x00050000, MGADWG_RSTR | 0x000d0000,
   MGADWG_RPL  | 0x00030000, MGADWG_RSTR | 0x000b0000,
   MGADWG_RSTR | 0x00070000, MGADWG_RPL  | 0x000f0000
};


CARD32 MGAAtypeNoBLK[16] = {
   MGADWG_RPL  | 0x00000000, MGADWG_RSTR | 0x00080000,
   MGADWG_RSTR | 0x00040000, MGADWG_RPL  | 0x000c0000,
   MGADWG_RSTR | 0x00020000, MGADWG_RSTR | 0x000a0000,
   MGADWG_RSTR | 0x00060000, MGADWG_RSTR | 0x000e0000,
   MGADWG_RSTR | 0x00010000, MGADWG_RSTR | 0x00090000,
   MGADWG_RSTR | 0x00050000, MGADWG_RSTR | 0x000d0000,
   MGADWG_RPL  | 0x00030000, MGADWG_RSTR | 0x000b0000,
   MGADWG_RSTR | 0x00070000, MGADWG_RPL  | 0x000f0000
};


Bool
MGAStormAccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    switch( pScrn->bitsPerPixel )
    {
    case 8:
    	return Mga8AccelInit(pScreen);
    case 16:
    	return Mga16AccelInit(pScreen);
    case 24:
    	return Mga24AccelInit(pScreen);
    case 32:
    	return Mga32AccelInit(pScreen);
    }
    return FALSE;
}



void
MGAStormSync(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    while(MGAISBUSY());
    /* flush cache before a read (mga-1064g 5.1.6) */
    OUTREG8(MGAREG_CRTC_INDEX, 0);
    if(pMga->AccelFlags & CLIPPER_ON) {
        pMga->AccelFlags &= ~CLIPPER_ON;
        OUTREG(MGAREG_CXBNDRY, 0xFFFF0000);
    }
}

void
MGAStormEngineInit(ScrnInfoPtr pScrn)
{
    long maccess = 0;
    MGAPtr pMga = MGAPTR(pScrn);
    MGAFBLayout *pLayout = &pMga->CurrentLayout;
    CARD32 opmode;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if ((pMga->Chipset == PCI_CHIP_MGAG100)
	|| (pMga->Chipset == PCI_CHIP_MGAG100_PCI))
    	maccess = 1 << 14;

    opmode = INREG(MGAREG_OPMODE);

    switch( pLayout->bitsPerPixel )
    {
    case 8:
	pMga->RestoreAccelState = Mga8RestoreAccelState;
        break;
    case 16:
        maccess |= 1;
	if(pLayout->depth == 15)
	   maccess |= (1 << 31);
	Mga16InitSolidFillRectFuncs(pMga);
	pMga->RestoreAccelState = Mga16RestoreAccelState;
	opmode |= 0x10000;
        break;
    case 24:
        maccess |= 3;
	Mga24InitSolidFillRectFuncs(pMga);
	pMga->RestoreAccelState = Mga24RestoreAccelState;
	opmode |= 0x20000;
        break;
    case 32:
        maccess |= 2;
	Mga32InitSolidFillRectFuncs(pMga);
	pMga->RestoreAccelState = Mga32RestoreAccelState;
	opmode |= 0x20000;
        break;
    }
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    opmode &= ~0x30000;
#endif

    pMga->fifoCount = 0;

    while(MGAISBUSY());

    if(!pMga->FifoSize) {
	pMga->FifoSize = INREG8(MGAREG_FIFOSTATUS);
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "%i DWORD fifo\n",
						pMga->FifoSize);
    }

    OUTREG(MGAREG_PITCH, pLayout->displayWidth);
    OUTREG(MGAREG_YDSTORG, pMga->YDstOrg);
    OUTREG(MGAREG_MACCESS, maccess);
    pMga->MAccess = maccess;
    pMga->PlaneMask = ~0;
    /* looks like this doesn't apply to mga g100 pci */

    if ((pMga->Chipset != PCI_CHIP_MGAG100)
	&& (pMga->Chipset != PCI_CHIP_MGAG100_PCI))
        OUTREG(MGAREG_PLNWT, pMga->PlaneMask);

    pMga->FgColor = 0;
    OUTREG(MGAREG_FCOL, pMga->FgColor);
    pMga->BgColor = 0;
    OUTREG(MGAREG_BCOL, pMga->BgColor);
    OUTREG(MGAREG_OPMODE, MGAOPM_DMA_BLIT | opmode);

    /* put clipping in a known state */
    OUTREG(MGAREG_CXBNDRY, 0xFFFF0000);	/* (maxX << 16) | minX */
    OUTREG(MGAREG_YTOP, 0x00000000);	/* minPixelPointer */
    OUTREG(MGAREG_YBOT, 0x007FFFFF);	/* maxPixelPointer */
    pMga->AccelFlags &= ~CLIPPER_ON;

    switch(pMga->Chipset) {
    case PCI_CHIP_MGAG550:
    case PCI_CHIP_MGAG400:
    case PCI_CHIP_MGAG200:
    case PCI_CHIP_MGAG200_PCI:
	pMga->SrcOrg = 0;
	OUTREG(MGAREG_SRCORG, pMga->realSrcOrg);
	OUTREG(MGAREG_DSTORG, pMga->DstOrg);
	break;
    default:
	break;
    }
    xf86SetLastScrnFlag(pScrn->entityList[0], pScrn->scrnIndex);

}

void MGASetClippingRectangle(
   ScrnInfoPtr pScrn,
   int x1, int y1, int x2, int y2
){
    MGAPtr pMga = MGAPTR(pScrn);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    WAITFIFO(3);
    OUTREG(MGAREG_CXBNDRY,(x2 << 16) | x1);
    OUTREG(MGAREG_YTOP, (y1 * pScrn->displayWidth) + pMga->YDstOrg);
    OUTREG(MGAREG_YBOT, (y2 * pScrn->displayWidth) + pMga->YDstOrg);
    pMga->AccelFlags |= CLIPPER_ON;
}

void MGADisableClipping(ScrnInfoPtr pScrn)
{
    MGAPtr pMga = MGAPTR(pScrn);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    WAITFIFO(3);
    OUTREG(MGAREG_CXBNDRY, 0xFFFF0000);     /* (maxX << 16) | minX */
    OUTREG(MGAREG_YTOP, 0x00000000);        /* minPixelPointer */
    OUTREG(MGAREG_YBOT, 0x007FFFFF);        /* maxPixelPointer */
    pMga->AccelFlags &= ~CLIPPER_ON;
}

#endif


	/*********************************************\
	|            Screen-to-Screen Copy            |
	\*********************************************/

#define BLIT_LEFT	1
#define BLIT_UP		4

void
MGANAME(SetupForScreenToScreenCopy)(
    ScrnInfoPtr pScrn,
    int xdir, int ydir,
    int rop,
    unsigned int planemask,
    int trans
){
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 dwgctl = pMga->AtypeNoBLK[rop] | MGADWG_SHIFTZERO |
			MGADWG_BITBLT | MGADWG_BFCOL;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    pMga->AccelInfoRec->SubsequentScreenToScreenCopy =
		MGANAME(SubsequentScreenToScreenCopy);

    pMga->BltScanDirection = 0;
    if(ydir == -1) pMga->BltScanDirection |= BLIT_UP;
    if(xdir == -1)
	pMga->BltScanDirection |= BLIT_LEFT;
    else if(pMga->HasFBitBlt && (rop == GXcopy) && !pMga->DrawTransparent)
	pMga->AccelInfoRec->SubsequentScreenToScreenCopy =
		MGANAME(SubsequentScreenToScreenCopy_FastBlit);

    if(pMga->DrawTransparent) {
	dwgctl |= MGADWG_TRANSC;
	WAITFIFO(2);
	SET_FOREGROUND(trans);
	trans = ~0;
	SET_BACKGROUND(trans);
    }

    WAITFIFO(4);
    OUTREG(MGAREG_DWGCTL, dwgctl);
    OUTREG(MGAREG_SGN, pMga->BltScanDirection);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_AR5, ydir * pMga->CurrentLayout.displayWidth);
}


static void
MGANAME(SubsequentScreenToScreenCopy)(
    ScrnInfoPtr pScrn,
    int srcX, int srcY, int dstX, int dstY, int w, int h
){
    int start, end, SrcOrg = 0, DstOrg = 0;
    MGAPtr pMga = MGAPTR(pScrn);

    if (pMga->AccelFlags & LARGE_ADDRESSES) {
	SrcOrg = ((srcY & ~1023) * pMga->CurrentLayout.displayWidth * PSZ) >> 9;
	DstOrg = ((dstY & ~1023) * pMga->CurrentLayout.displayWidth * PSZ) >> 9;
        dstY &= 1023;
    }

    if(pMga->BltScanDirection & BLIT_UP) {
	srcY += h - 1;
	dstY += h - 1;
    }

    w--;
    start = end = XYADDRESS(srcX, srcY);

    if(pMga->BltScanDirection & BLIT_LEFT) start += w;
    else end += w;

    if (pMga->AccelFlags & LARGE_ADDRESSES) {
	WAITFIFO(7);
	if(DstOrg)
	    OUTREG(MGAREG_DSTORG, (DstOrg << 6) + pMga->DstOrg);
	if(SrcOrg != pMga->SrcOrg) {
	    pMga->SrcOrg = SrcOrg;
	    OUTREG(MGAREG_SRCORG, (SrcOrg << 6) + pMga->realSrcOrg);
 	}
	if(SrcOrg) {
	    SrcOrg = (SrcOrg << 9) / PSZ;
	    end -= SrcOrg;
	    start -= SrcOrg;
	}
	OUTREG(MGAREG_AR0, end);
	OUTREG(MGAREG_AR3, start);
	OUTREG(MGAREG_FXBNDRY, ((dstX + w) << 16) | (dstX & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dstY << 16) | h);
	if(DstOrg)
	   OUTREG(MGAREG_DSTORG, pMga->DstOrg);
    } else {
	WAITFIFO(4);
	OUTREG(MGAREG_AR0, end);
	OUTREG(MGAREG_AR3, start);
	OUTREG(MGAREG_FXBNDRY, ((dstX + w) << 16) | (dstX & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dstY << 16) | h);
    }
}


static void
MGANAME(SubsequentScreenToScreenCopy_FastBlit)(
    ScrnInfoPtr pScrn,
    int srcX, int srcY, int dstX, int dstY, int w, int h
)
{
    int start, end;
    MGAPtr pMga = MGAPTR(pScrn);

    if(pMga->BltScanDirection & BLIT_UP) {
	srcY += h - 1;
	dstY += h - 1;
    }

    w--;
    start = XYADDRESS(srcX, srcY);
    end = start + w;

    /* we assume the driver asserts screen pitches such that
	we can always use fastblit for scrolling */
    if(
#if PSZ == 32
        !((srcX ^ dstX) & 31)
#elif PSZ == 16
        !((srcX ^ dstX) & 63)
#else
        !((srcX ^ dstX) & 127)
#endif
    	) {
	if(pMga->MaxFastBlitY) {
	   if(pMga->BltScanDirection & BLIT_UP) {
		if((srcY >= pMga->MaxFastBlitY) ||
				(dstY >= pMga->MaxFastBlitY))
			goto FASTBLIT_BAILOUT;
	   } else {
		if(((srcY + h) > pMga->MaxFastBlitY) ||
				((dstY + h) > pMga->MaxFastBlitY))
			goto FASTBLIT_BAILOUT;
	   }
	}

	/* Millennium 1 fastblit bug fix */
        if(pMga->AccelFlags & FASTBLT_BUG) {
    	   int fxright = dstX + w;
#if PSZ == 8
           if((dstX & (1 << 6)) && (((fxright >> 6) - (dstX >> 6)) & 7) == 7) {
		fxright |= 1 << 6;
#elif PSZ == 16
           if((dstX & (1 << 5)) && (((fxright >> 5) - (dstX >> 5)) & 7) == 7) {
		fxright |= 1 << 5;
#elif PSZ == 24
           if(((dstX * 3) & (1 << 6)) &&
                 ((((fxright * 3 + 2) >> 6) - ((dstX * 3) >> 6)) & 7) == 7) {
		fxright = ((fxright * 3 + 2) | (1 << 6)) / 3;
#elif PSZ == 32
           if((dstX & (1 << 4)) && (((fxright >> 4) - (dstX >> 4)) & 7) == 7) {
		fxright |= 1 << 4;
#endif
		WAITFIFO(8);
		OUTREG(MGAREG_CXRIGHT, dstX + w);
		OUTREG(MGAREG_DWGCTL, 0x040A400C);
		OUTREG(MGAREG_AR0, end);
		OUTREG(MGAREG_AR3, start);
		OUTREG(MGAREG_FXBNDRY, (fxright << 16) | (dstX & 0xffff));
		OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dstY << 16) | h);
		OUTREG(MGAREG_DWGCTL, pMga->AtypeNoBLK[GXcopy] |
			MGADWG_SHIFTZERO | MGADWG_BITBLT | MGADWG_BFCOL);
		OUTREG(MGAREG_CXRIGHT, 0xFFFF);
	    	return;
	    } /* } } } (preserve pairs for pair matching) */
	}

   	WAITFIFO(6);
    	OUTREG(MGAREG_DWGCTL, 0x040A400C);
    	OUTREG(MGAREG_AR0, end);
    	OUTREG(MGAREG_AR3, start);
    	OUTREG(MGAREG_FXBNDRY, ((dstX + w) << 16) | (dstX & 0xffff));
    	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dstY << 16) | h);
    	OUTREG(MGAREG_DWGCTL, pMga->AtypeNoBLK[GXcopy] | MGADWG_SHIFTZERO |
			MGADWG_BITBLT | MGADWG_BFCOL);
	return;
    }

FASTBLIT_BAILOUT:

    WAITFIFO(4);
    OUTREG(MGAREG_AR0, end);
    OUTREG(MGAREG_AR3, start);
    OUTREG(MGAREG_FXBNDRY, ((dstX + w) << 16) | (dstX & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (dstY << 16) | h);
}


        /******************\
	|   Solid Fills    |
	\******************/

void
MGANAME(SetupForSolidFill)(
	ScrnInfoPtr pScrn,
	int color,
	int rop,
	unsigned int planemask )
{
    MGAPtr pMga = MGAPTR(pScrn);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

#if PSZ == 24
    if(!RGBEQUAL(color))
    pMga->FilledRectCMD = MGADWG_TRAP | MGADWG_SOLID | MGADWG_ARZERO |
		    MGADWG_SGNZERO | MGADWG_SHIFTZERO |
		    MGADWG_BMONOLEF | pMga->AtypeNoBLK[rop];
    else
#endif
    pMga->FilledRectCMD = MGADWG_TRAP | MGADWG_SOLID | MGADWG_ARZERO |
		    MGADWG_SGNZERO | MGADWG_SHIFTZERO |
		    MGADWG_BMONOLEF | pMga->Atype[rop];

    pMga->SolidLineCMD =  MGADWG_SOLID | MGADWG_SHIFTZERO | MGADWG_BFCOL |
		    pMga->AtypeNoBLK[rop];

    if(pMga->AccelFlags & TRANSC_SOLID_FILL)
	pMga->FilledRectCMD |= MGADWG_TRANSC;

    WAITFIFO(3);
    SET_FOREGROUND(color);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_DWGCTL, pMga->FilledRectCMD);
}

static void
MGANAME(SubsequentSolidFillRect)(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h )
{
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(2);
    OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
}

static void
MGANAME(SubsequentSolidFillTrap)(ScrnInfoPtr pScrn, int y, int h,
	int left, int dxL, int dyL, int eL,
	int right, int dxR, int dyR, int eR )
{
    MGAPtr pMga = MGAPTR(pScrn);
    int sdxl = (dxL < 0);
    int ar2 = sdxl? dxL : -dxL;
    int sdxr = (dxR < 0);
    int ar5 = sdxr? dxR : -dxR;

    WAITFIFO(11);
    OUTREG(MGAREG_DWGCTL,
		pMga->FilledRectCMD & ~(MGADWG_ARZERO | MGADWG_SGNZERO));
    OUTREG(MGAREG_AR0, dyL);
    OUTREG(MGAREG_AR1, ar2 - eL);
    OUTREG(MGAREG_AR2, ar2);
    OUTREG(MGAREG_AR4, ar5 - eR);
    OUTREG(MGAREG_AR5, ar5);
    OUTREG(MGAREG_AR6, dyR);
    OUTREG(MGAREG_SGN, (sdxl << 1) | (sdxr << 5));
    OUTREG(MGAREG_FXBNDRY, ((right + 1) << 16) | (left & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
    OUTREG(MGAREG_DWGCTL, pMga->FilledRectCMD);
}

	/***************\
	|  Solid Lines  |
	\***************/


static void
MGANAME(SubsequentSolidHorVertLine) (
    ScrnInfoPtr pScrn,
    int x, int y,
    int len, int dir
){
    MGAPtr pMga = MGAPTR(pScrn);

    if(dir == DEGREES_0) {
	WAITFIFO(2);
	OUTREG(MGAREG_FXBNDRY, ((x + len) << 16) | (x & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | 1);
    } else if(pMga->AccelFlags & USE_RECTS_FOR_LINES) {
	WAITFIFO(2);
	OUTREG(MGAREG_FXBNDRY, ((x + 1) << 16) | (x & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | len);
    } else {
	WAITFIFO(4);
	OUTREG(MGAREG_DWGCTL, pMga->SolidLineCMD | MGADWG_AUTOLINE_OPEN);
	OUTREG(MGAREG_XYSTRT, (y << 16) | (x & 0xffff));
	OUTREG(MGAREG_XYEND + MGAREG_EXEC, ((y + len) << 16) | (x & 0xffff));
	OUTREG(MGAREG_DWGCTL, pMga->FilledRectCMD);
    }
}


static void
MGANAME(SubsequentSolidTwoPointLine)(
   ScrnInfoPtr pScrn,
   int x1, int y1, int x2, int y2, int flags
){
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(4);
    OUTREG(MGAREG_DWGCTL, pMga->SolidLineCMD |
        ((flags & OMIT_LAST) ? MGADWG_AUTOLINE_OPEN : MGADWG_AUTOLINE_CLOSE));
    OUTREG(MGAREG_XYSTRT, (y1 << 16) | (x1 & 0xFFFF));
    OUTREG(MGAREG_XYEND + MGAREG_EXEC, (y2 << 16) | (x2 & 0xFFFF));
    OUTREG(MGAREG_DWGCTL, pMga->FilledRectCMD);
}



	/***************************\
	|   8x8 Mono Pattern Fills  |
	\***************************/


static void
MGANAME(SetupForMono8x8PatternFill)(
   ScrnInfoPtr pScrn,
   int patx, int paty,
   int fg, int bg,
   int rop,
   unsigned int planemask )
{
    MGAPtr pMga = MGAPTR(pScrn);
    XAAInfoRecPtr infoRec = pMga->AccelInfoRec;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    pMga->PatternRectCMD = MGADWG_TRAP | MGADWG_ARZERO | MGADWG_SGNZERO |
						MGADWG_BMONOLEF;

    infoRec->SubsequentMono8x8PatternFillRect =
		MGANAME(SubsequentMono8x8PatternFillRect);

    if(bg == -1) {
#if PSZ == 24
    	if(!RGBEQUAL(fg))
            pMga->PatternRectCMD |= MGADWG_TRANSC | pMga->AtypeNoBLK[rop];
	else
#endif
            pMga->PatternRectCMD |= MGADWG_TRANSC | pMga->Atype[rop];

	WAITFIFO(5);
    } else {
#if PSZ == 24
	if((pMga->AccelFlags & BLK_OPAQUE_EXPANSION) && RGBEQUAL(fg) && RGBEQUAL(bg))
#else
	if(pMga->AccelFlags & BLK_OPAQUE_EXPANSION)
#endif
        	pMga->PatternRectCMD |= pMga->Atype[rop];
	else
        	pMga->PatternRectCMD |= pMga->AtypeNoBLK[rop];
	WAITFIFO(6);
    	SET_BACKGROUND(bg);
    }

    SET_FOREGROUND(fg);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_DWGCTL, pMga->PatternRectCMD);
    OUTREG(MGAREG_PAT0, patx);
    OUTREG(MGAREG_PAT1, paty);
}



static void
MGANAME(SubsequentMono8x8PatternFillRect)(
    ScrnInfoPtr pScrn,
    int patx, int paty,
    int x, int y, int w, int h )
{
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(3);
    OUTREG(MGAREG_SHIFT, (paty << 4) | patx);
    OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
    pMga->AccelInfoRec->SubsequentMono8x8PatternFillRect =
		MGANAME(SubsequentMono8x8PatternFillRect_Additional);
}

static void
MGANAME(SubsequentMono8x8PatternFillRect_Additional)(
    ScrnInfoPtr pScrn,
    int patx, int paty,
    int x, int y, int w, int h )
{
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(2);
    OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
}


static void
MGANAME(SubsequentMono8x8PatternFillTrap)(
    ScrnInfoPtr pScrn,
    int patx, int paty,
    int y, int h,
    int left, int dxL, int dyL, int eL,
    int right, int dxR, int dyR, int eR
){
    MGAPtr pMga = MGAPTR(pScrn);

    int sdxl = (dxL < 0) ? (1<<1) : 0;
    int ar2 = sdxl? dxL : -dxL;
    int sdxr = (dxR < 0) ? (1<<5) : 0;
    int ar5 = sdxr? dxR : -dxR;

    WAITFIFO(12);
    OUTREG(MGAREG_SHIFT, (paty << 4) | patx);
    OUTREG(MGAREG_DWGCTL,
	pMga->PatternRectCMD & ~(MGADWG_ARZERO | MGADWG_SGNZERO));
    OUTREG(MGAREG_AR0, dyL);
    OUTREG(MGAREG_AR1, ar2 - eL);
    OUTREG(MGAREG_AR2, ar2);
    OUTREG(MGAREG_AR4, ar5 - eR);
    OUTREG(MGAREG_AR5, ar5);
    OUTREG(MGAREG_AR6, dyR);
    OUTREG(MGAREG_SGN, sdxl | sdxr);
    OUTREG(MGAREG_FXBNDRY, ((right + 1) << 16) | (left & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
    OUTREG(MGAREG_DWGCTL, pMga->PatternRectCMD);
}

	/***********************\
	|   Color Expand Rect   |
	\***********************/


static void
MGANAME(SetupForScanlineCPUToScreenColorExpandFill)(
	ScrnInfoPtr pScrn,
	int fg, int bg,
	int rop,
	unsigned int planemask )
{
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 mgaCMD = MGADWG_ILOAD | MGADWG_LINEAR | MGADWG_SGNZERO |
			MGADWG_SHIFTZERO | MGADWG_BMONOLEF;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(bg == -1) {
#if PSZ == 24
    	if(!RGBEQUAL(fg))
            mgaCMD |= MGADWG_TRANSC | pMga->AtypeNoBLK[rop];
	else
#endif
            mgaCMD |= MGADWG_TRANSC | pMga->Atype[rop];

	WAITFIFO(3);
    } else {
#if PSZ == 24
	if((pMga->AccelFlags & BLK_OPAQUE_EXPANSION) &&
		RGBEQUAL(fg) && RGBEQUAL(bg))
#else
	if(pMga->AccelFlags & BLK_OPAQUE_EXPANSION)
#endif
        	mgaCMD |= pMga->Atype[rop];
	else
        	mgaCMD |= pMga->AtypeNoBLK[rop];
	WAITFIFO(4);
    	SET_BACKGROUND(bg);
    }

    SET_FOREGROUND(fg);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_DWGCTL, mgaCMD);
}

static void
MGANAME(SubsequentScanlineCPUToScreenColorExpandFill)(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    MGAPtr pMga = MGAPTR(pScrn);

    pMga->AccelFlags |= CLIPPER_ON;
    pMga->expandDWORDs = (w + 31) >> 5;
    if((pMga->expandDWORDs * h) > pMga->MaxBlitDWORDS) {
	pMga->expandHeight = pMga->MaxBlitDWORDS / pMga->expandDWORDs;
	pMga->expandRemaining = h / pMga->expandHeight;
	if(!(h = h % pMga->expandHeight)) {
	   pMga->expandRemaining--;
	   h = pMga->expandHeight;
	}
	pMga->expandY = y + h;
    } else
	pMga->expandRemaining = 0;
    pMga->expandRows = h;

    WAITFIFO(5);
    OUTREG(MGAREG_CXBNDRY, ((x + w - 1) << 16) | ((x + skipleft) & 0xFFFF));
    w = pMga->expandDWORDs << 5;     /* source is dword padded */
    OUTREG(MGAREG_AR0, (w * h) - 1);
    OUTREG(MGAREG_AR3, 0);  /* crashes occasionally without this */
    OUTREG(MGAREG_FXBNDRY, ((x + w - 1) << 16) | (x & 0xFFFF));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);

#if defined(__alpha__)
    if(1) /* force indirect always on Alpha */
#else
    if(pMga->expandDWORDs > pMga->FifoSize)
#endif
    {
        pMga->AccelInfoRec->SubsequentColorExpandScanline =
                MGANAME(SubsequentColorExpandScanlineIndirect);
        pMga->AccelInfoRec->ScanlineColorExpandBuffers =
                (unsigned char**)(&pMga->ScratchBuffer);
    } else {
        pMga->AccelInfoRec->SubsequentColorExpandScanline =
                MGANAME(SubsequentColorExpandScanline);
        pMga->AccelInfoRec->ScanlineColorExpandBuffers =
                (unsigned char**)(&pMga->ColorExpandBase);
	WAITFIFO(pMga->expandDWORDs);
    }
}

static void
MGANAME(SubsequentColorExpandScanlineIndirect)(
	ScrnInfoPtr pScrn,
	int bufno
){
    MGAPtr pMga = MGAPTR(pScrn);
    int dwords = pMga->expandDWORDs;
    CARD32 *src = (CARD32*)(pMga->ScratchBuffer);

    while(dwords > pMga->FifoSize) {
	WAITFIFO(pMga->FifoSize);
	MGAMoveDWORDS((CARD32*)(pMga->ColorExpandBase), src, pMga->FifoSize);
	src += pMga->FifoSize;
	dwords -= pMga->FifoSize;
    }

    WAITFIFO(dwords);
    MGAMoveDWORDS((CARD32*)(pMga->ColorExpandBase), src, dwords);

    if(!(--pMga->expandRows)) {
	if(pMga->expandRemaining) {
	    WAITFIFO(3);
	    OUTREG(MGAREG_AR0,((pMga->expandDWORDs<< 5)*pMga->expandHeight)-1);
	    OUTREG(MGAREG_AR3, 0);  /* crashes occasionally without this */
	    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (pMga->expandY << 16) |
	                                      pMga->expandHeight);
	    pMga->expandY += pMga->expandHeight;
            pMga->expandRows = pMga->expandHeight;
	    pMga->expandRemaining--;
	} else {
            DISABLE_CLIP();
	}
    }
}

static void
MGANAME(SubsequentColorExpandScanline)(
        ScrnInfoPtr pScrn,
        int bufno
){
    MGAPtr pMga = MGAPTR(pScrn);

    if(--pMga->expandRows) {
	WAITFIFO(pMga->expandDWORDs);
    } else if(pMga->expandRemaining) {
	WAITFIFO(3);
	OUTREG(MGAREG_AR0,((pMga->expandDWORDs<<5)*pMga->expandHeight)-1);
	OUTREG(MGAREG_AR3, 0);  /* crashes occasionally without this */
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (pMga->expandY << 16) |
	                                      pMga->expandHeight);
	pMga->expandY += pMga->expandHeight;
        pMga->expandRows = pMga->expandHeight;
	pMga->expandRemaining--;
	WAITFIFO(pMga->expandDWORDs);
    } else {
        DISABLE_CLIP();
    }
}


	/*******************\
	|   Image Writes    |
	\*******************/


static void MGANAME(SetupForScanlineImageWrite)(
   ScrnInfoPtr pScrn,
   int rop,
   unsigned int planemask,
   int transparency_color,
   int bpp, int depth
){
    MGAPtr pMga = MGAPTR(pScrn);

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    WAITFIFO(3);
    OUTREG(MGAREG_AR5, 0);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_DWGCTL, MGADWG_ILOAD | MGADWG_BFCOL | MGADWG_SHIFTZERO |
			MGADWG_SGNZERO | pMga->AtypeNoBLK[rop]);
}


static void MGANAME(SubsequentScanlineImageWriteRect)(
   ScrnInfoPtr pScrn,
   int x, int y, int w, int h,
   int skipleft
){
    MGAPtr pMga = MGAPTR(pScrn);

    pMga->AccelFlags |= CLIPPER_ON;
    pMga->expandRows = h;
    pMga->expandDWORDs = ((w * PSZ) + 31) >> 5;

    WAITFIFO(5);
    OUTREG(MGAREG_CXBNDRY, 0xFFFF0000 | ((x + skipleft) & 0xFFFF));
    OUTREG(MGAREG_AR0, w - 1);
    OUTREG(MGAREG_AR3, 0);
    OUTREG(MGAREG_FXBNDRY, ((x + w - 1) << 16) | (x & 0xFFFF));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
}

static void MGANAME(SubsequentImageWriteScanline)(
    ScrnInfoPtr pScrn,
    int bufno
){
    MGAPtr pMga = MGAPTR(pScrn);
    int dwords = pMga->expandDWORDs;
    CARD32 *src = (CARD32*)(pMga->ScratchBuffer);

    while(dwords > pMga->FifoSize) {
	WAITFIFO(pMga->FifoSize);
        MGAMoveDWORDS((CARD32*)(pMga->ColorExpandBase), src, pMga->FifoSize);
        src += pMga->FifoSize;
        dwords -= pMga->FifoSize;
    }

    WAITFIFO(dwords);
    MGAMoveDWORDS((CARD32*)(pMga->ColorExpandBase), src, dwords);

    if(!(--pMga->expandRows)) {
	DISABLE_CLIP();
    }
}

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

	/***************************\
	|      Dashed  Lines        |
	\***************************/


void
MGANAME(SetupForDashedLine)(
    ScrnInfoPtr pScrn,
    int fg, int bg, int rop,
    unsigned int planemask,
    int length,
    unsigned char *pattern
){
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 *DashPattern = (CARD32*)pattern;
    CARD32 NiceDashPattern = DashPattern[0];
    int dwords = (length + 31) >> 5;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    pMga->DashCMD = MGADWG_BFCOL | pMga->AtypeNoBLK[rop];
    pMga->StyleLen = length - 1;

    if(bg == -1) {
        pMga->DashCMD |= MGADWG_TRANSC;
	WAITFIFO(dwords + 2);
    } else {
	WAITFIFO(dwords + 3);
	SET_BACKGROUND(bg);
    }
    SET_PLANEMASK(planemask);
    SET_FOREGROUND(fg);

    /* We see if we can draw horizontal lines as 8x8 pattern fills.
	This is worthwhile since the pattern fills can use block mode
	and the default X pattern is 8 pixels long.  The forward pattern
	is the top scanline, the backwards pattern is the next one. */
    switch(length) {
	case 2:	NiceDashPattern |= NiceDashPattern << 2;
	case 4:	NiceDashPattern |= NiceDashPattern << 4;
	case 8:	NiceDashPattern |= byte_reversed[NiceDashPattern] << 16;
		NiceDashPattern |= NiceDashPattern << 8;
		pMga->NiceDashCMD = MGADWG_TRAP | MGADWG_ARZERO |
				    MGADWG_SGNZERO | MGADWG_BMONOLEF;
     		pMga->AccelFlags |= NICE_DASH_PATTERN;
   		if(bg == -1) {
#if PSZ == 24
    		   if(!RGBEQUAL(fg))
            		pMga->NiceDashCMD |= MGADWG_TRANSC | pMga->AtypeNoBLK[rop];
		   else
#endif
           		pMga->NiceDashCMD |= MGADWG_TRANSC | pMga->Atype[rop];
    		} else {
#if PSZ == 24
		   if((pMga->AccelFlags & BLK_OPAQUE_EXPANSION) &&
					RGBEQUAL(fg) && RGBEQUAL(bg))
#else
		   if(pMga->AccelFlags & BLK_OPAQUE_EXPANSION)
#endif
        		pMga->NiceDashCMD |= pMga->Atype[rop];
		   else
        		pMga->NiceDashCMD |= pMga->AtypeNoBLK[rop];
    		}
		OUTREG(MGAREG_SRC0, NiceDashPattern);
		break;
	default: pMga->AccelFlags &= ~NICE_DASH_PATTERN;
		switch (dwords) {
		case 4:  OUTREG(MGAREG_SRC3, DashPattern[3]);
		case 3:  OUTREG(MGAREG_SRC2, DashPattern[2]);
		case 2:	 OUTREG(MGAREG_SRC1, DashPattern[1]);
		default: OUTREG(MGAREG_SRC0, DashPattern[0]);
		}
    }
}


void
MGANAME(SubsequentDashedTwoPointLine)(
    ScrnInfoPtr pScrn,
    int x1, int y1, int x2, int y2,
    int flags, int phase
){
    MGAPtr pMga = MGAPTR(pScrn);

    WAITFIFO(4);
    if((pMga->AccelFlags & NICE_DASH_PATTERN) && (y1 == y2)) {
    	OUTREG(MGAREG_DWGCTL, pMga->NiceDashCMD);
	if(x2 < x1) {
	   if(flags & OMIT_LAST) x2++;
   	   OUTREG(MGAREG_SHIFT, ((-y1 & 0x07) << 4) |
				((7 - phase - x1) & 0x07));
   	   OUTREG(MGAREG_FXBNDRY, ((x1 + 1) << 16) | (x2 & 0xffff));
    	} else {
 	   if(!flags) x2++;
   	   OUTREG(MGAREG_SHIFT, (((1 - y1) & 0x07) << 4) |
				((phase - x1) & 0x07));
     	   OUTREG(MGAREG_FXBNDRY, (x2 << 16) | (x1 & 0xffff));
	}
    	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y1 << 16) | 1);
    } else {
	OUTREG(MGAREG_SHIFT, (pMga->StyleLen << 16 ) |
				(pMga->StyleLen - phase));
	OUTREG(MGAREG_DWGCTL, pMga->DashCMD | ((flags & OMIT_LAST) ?
			MGADWG_AUTOLINE_OPEN : MGADWG_AUTOLINE_CLOSE));
	OUTREG(MGAREG_XYSTRT, (y1 << 16) | (x1 & 0xFFFF));
	OUTREG(MGAREG_XYEND + MGAREG_EXEC, (y2 << 16) | (x2 & 0xFFFF));
    }
}

#endif /* X_BYTE_ORDER == X_LITTLE_ENDIAN */

#if PSZ != 24

	/******************************************\
	|  Planar Screen to Screen Color Expansion |
	\******************************************/

static void
MGANAME(SetupForPlanarScreenToScreenColorExpandFill)(
   ScrnInfoPtr pScrn,
   int fg, int bg,
   int rop,
   unsigned int planemask
){
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 mgaCMD = pMga->AtypeNoBLK[rop] | MGADWG_BITBLT |
				MGADWG_SGNZERO | MGADWG_BPLAN;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(bg == -1) {
	mgaCMD |= MGADWG_TRANSC;
	WAITFIFO(4);
    } else {
	WAITFIFO(5);
    	SET_BACKGROUND(bg);
    }

    SET_FOREGROUND(fg);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_AR5, pScrn->displayWidth);
    OUTREG(MGAREG_DWGCTL, mgaCMD);
}

static void
MGANAME(SubsequentPlanarScreenToScreenColorExpandFill)(
   ScrnInfoPtr pScrn,
   int x, int y, int w, int h,
   int srcx, int srcy,
   int skipleft
){
    MGAPtr pMga = MGAPTR(pScrn);
    int start, end;

    w--;
    start = XYADDRESS(srcx, srcy) + skipleft;
    end = start + w;

    WAITFIFO(4);
    OUTREG(MGAREG_AR3, start);
    OUTREG(MGAREG_AR0, end);
    OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
}

#endif

	/***********************************\
	|  Screen to Screen Color Expansion |
	\***********************************/

static void
MGANAME(SetupForScreenToScreenColorExpandFill)(
   ScrnInfoPtr pScrn,
   int fg, int bg,
   int rop,
   unsigned int planemask
){
    MGAPtr pMga = MGAPTR(pScrn);
    CARD32 mgaCMD = MGADWG_BITBLT | MGADWG_SGNZERO | MGADWG_SHIFTZERO;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if(bg == -1) {
#if PSZ == 24
    	if(!RGBEQUAL(fg))
            mgaCMD |= MGADWG_TRANSC | pMga->AtypeNoBLK[rop];
	else
#endif
            mgaCMD |= MGADWG_TRANSC | pMga->Atype[rop];

	WAITFIFO(4);
    } else {
#if PSZ == 24
	if((pMga->AccelFlags & BLK_OPAQUE_EXPANSION) &&
		RGBEQUAL(fg) && RGBEQUAL(bg))
#else
	if((pMga->AccelFlags & BLK_OPAQUE_EXPANSION))
#endif
        	mgaCMD |= pMga->Atype[rop];
	else
        	mgaCMD |= pMga->AtypeNoBLK[rop];
	WAITFIFO(5);
    	SET_BACKGROUND(bg);
    }

    SET_FOREGROUND(fg);
    SET_PLANEMASK(planemask);
    OUTREG(MGAREG_AR5, pScrn->displayWidth * PSZ);
    OUTREG(MGAREG_DWGCTL, mgaCMD);

}

static void
MGANAME(SubsequentScreenToScreenColorExpandFill)(
   ScrnInfoPtr pScrn,
   int x, int y, int w, int h,
   int srcx, int srcy,
   int skipleft
){
    MGAPtr pMga = MGAPTR(pScrn);
    int pitch = pScrn->displayWidth * PSZ;
    int start, end, next, num;
    Bool resetDstOrg = FALSE;

    if (pMga->AccelFlags & LARGE_ADDRESSES) {
        int DstOrg = ((y & ~1023) * pScrn->displayWidth * PSZ) >> 9;
        int SrcOrg = ((srcy & ~1023) * pScrn->displayWidth * PSZ) >> 9;

	y &= 1023;
	srcy &= 1023;

	WAITFIFO(2);
	if(DstOrg) {
            OUTREG(MGAREG_DSTORG, (DstOrg << 6) + pMga->DstOrg);
	    resetDstOrg = TRUE;
	}
        if(SrcOrg != pMga->SrcOrg) {
            pMga->SrcOrg = SrcOrg;
            OUTREG(MGAREG_SRCORG, (SrcOrg << 6) + pMga->realSrcOrg);
        }
    }

    w--;
    start = (XYADDRESS(srcx, srcy) * PSZ) + skipleft;
    end = start + w + (pitch * (h - 1));

    /* src cannot split a 2 Meg boundary from SrcOrg */
    if(!((start ^ end) & 0xff000000)) {
	WAITFIFO(4);
	OUTREG(MGAREG_AR3, start);
	OUTREG(MGAREG_AR0, start + w);
	OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | h);
    } else {
	while(h) {
	    next = (start + 0x00ffffff) & 0xff000000;
	    if(next <= (start + w)) {
		num = next - start - 1;

		WAITFIFO(7);
		OUTREG(MGAREG_AR3, start);
		OUTREG(MGAREG_AR0, start + num);
		OUTREG(MGAREG_FXBNDRY, ((x + num) << 16) | (x & 0xffff));
		OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | 1);

		OUTREG(MGAREG_AR3, next);
		OUTREG(MGAREG_AR0, start + w );
		OUTREG(MGAREG_FXBNDRY + MGAREG_EXEC, ((x + w) << 16) |
                                                     ((x + num + 1) & 0xffff));
		start += pitch;
		h--; y++;
	    } else {
		num = ((next - start - w)/pitch) + 1;
		if(num > h) num = h;

		WAITFIFO(4);
		OUTREG(MGAREG_AR3, start);
		OUTREG(MGAREG_AR0, start + w);
		OUTREG(MGAREG_FXBNDRY, ((x + w) << 16) | (x & 0xffff));
		OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (y << 16) | num);

		start += num * pitch;
		h -= num; y += num;
	    }
	}
    }

    if(resetDstOrg) {
	WAITFIFO(1);
	OUTREG(MGAREG_DSTORG, pMga->DstOrg);
    }
}



#if PSZ == 8

void
MGAFillSolidRectsDMA(
    ScrnInfoPtr pScrn,
    int	fg, int rop,
    unsigned int planemask,
    int		nBox, 		/* number of rectangles to fill */
    BoxPtr	pBox  		/* Pointer to first rectangle to fill */
){
    MGAPtr pMga = MGAPTR(pScrn);
    XAAInfoRecPtr infoRec = pMga->AccelInfoRec;
    CARD32 *base = (CARD32*)pMga->ILOADBase;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    SET_SYNC_FLAG(infoRec);
    (*infoRec->SetupForSolidFill)(pScrn, fg, rop, planemask);

    if(nBox & 1) {
	OUTREG(MGAREG_FXBNDRY, ((pBox->x2) << 16) | (pBox->x1 & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC,
		(pBox->y1 << 16) | (pBox->y2 - pBox->y1));
	nBox--; pBox++;
    }

    if(!nBox) return;

    OUTREG(MGAREG_OPMODE, MGAOPM_DMA_GENERAL);
    while(nBox) {
	base[0] = DMAINDICES(MGAREG_FXBNDRY, MGAREG_YDSTLEN + MGAREG_EXEC,
                MGAREG_FXBNDRY, MGAREG_YDSTLEN + MGAREG_EXEC);
	base[1] = ((pBox->x2) << 16) | (pBox->x1 & 0xffff);
	base[2] = (pBox->y1 << 16) | (pBox->y2 - pBox->y1);
	pBox++;
	base[3] = ((pBox->x2) << 16) | (pBox->x1 & 0xffff);
	base[4] = (pBox->y1 << 16) | (pBox->y2 - pBox->y1);
	pBox++;
	base += 5; nBox -= 2;
    }
    OUTREG(MGAREG_OPMODE, MGAOPM_DMA_BLIT);
}

void
MGAFillSolidSpansDMA(
   ScrnInfoPtr pScrn,
   int fg, int rop,
   unsigned int planemask,
   int n,
   DDXPointPtr ppt,
   int *pwidth, int fSorted
){
    MGAPtr pMga = MGAPTR(pScrn);
    XAAInfoRecPtr infoRec = pMga->AccelInfoRec;
    CARD32 *base = (CARD32*)pMga->ILOADBase;

    CHECK_DMA_QUIESCENT(pMga, pScrn);
    SET_SYNC_FLAG(infoRec);

    if(infoRec->ClipBox) {
	OUTREG(MGAREG_CXBNDRY,
	   ((infoRec->ClipBox->x2 - 1) << 16) | infoRec->ClipBox->x1);
	OUTREG(MGAREG_YTOP,
	   (infoRec->ClipBox->y1 * pScrn->displayWidth) + pMga->YDstOrg);
	OUTREG(MGAREG_YBOT,
	   ((infoRec->ClipBox->y2 - 1) * pScrn->displayWidth) + pMga->YDstOrg);
    }

    (*infoRec->SetupForSolidFill)(pScrn, fg, rop, planemask);

    if(n & 1) {
	OUTREG(MGAREG_FXBNDRY, ((ppt->x + *pwidth) << 16) | (ppt->x & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (ppt->y << 16) | 1);
	ppt++; pwidth++; n--;
    }

    if(n) {
	if(n > 838860) n = 838860;  /* maximum number we have room for */

	OUTREG(MGAREG_OPMODE, MGAOPM_DMA_GENERAL);
	while(n) {
	    base[0] = DMAINDICES(MGAREG_FXBNDRY, MGAREG_YDSTLEN + MGAREG_EXEC,
                MGAREG_FXBNDRY, MGAREG_YDSTLEN + MGAREG_EXEC);
	    base[1] = ((ppt->x + *(pwidth++)) << 16) | (ppt->x & 0xffff);
	    base[2] = (ppt->y << 16) | 1;
	    ppt++;
	    base[3] = ((ppt->x + *(pwidth++)) << 16) | (ppt->x & 0xffff);
	    base[4] = (ppt->y << 16) | 1;
	    ppt++;
	    base += 5; n -= 2;
	}
	OUTREG(MGAREG_OPMODE, MGAOPM_DMA_BLIT);
    }

    if(infoRec->ClipBox) {
	OUTREG(MGAREG_CXBNDRY, 0xFFFF0000);     /* (maxX << 16) | minX */
	OUTREG(MGAREG_YTOP, 0x00000000);        /* minPixelPointer */
	OUTREG(MGAREG_YBOT, 0x007FFFFF);        /* maxPixelPointer */
    }
}


void
MGAFillMono8x8PatternRectsTwoPass(
    ScrnInfoPtr pScrn,
    int	fg, int bg, int rop,
    unsigned int planemask,
    int	nBoxInit,
    BoxPtr pBoxInit,
    int pattern0, int pattern1,
    int xorg, int yorg
){
    MGAPtr pMga = MGAPTR(pScrn);
    XAAInfoRecPtr infoRec = pMga->AccelInfoRec;
    int	nBox, SecondPassColor;
    BoxPtr pBox;

    CHECK_DMA_QUIESCENT(pMga, pScrn);

    if((rop == GXcopy) && (bg != -1)) {
	SecondPassColor = bg;
	bg = -1;
    } else SecondPassColor = -1;

    WAITFIFO(1);
    OUTREG(MGAREG_SHIFT, (((-yorg) & 0x07) << 4) | ((-xorg) & 0x07));

SECOND_PASS:

    nBox = nBoxInit;
    pBox = pBoxInit;

    (*infoRec->SetupForMono8x8PatternFill)(pScrn, pattern0, pattern1,
					fg, bg, rop, planemask);

    while(nBox--) {
	WAITFIFO(2);
	OUTREG(MGAREG_FXBNDRY, ((pBox->x2) << 16) | (pBox->x1 & 0xffff));
	OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC,
			(pBox->y1 << 16) | (pBox->y2 - pBox->y1));
	pBox++;
    }

    if(SecondPassColor != -1) {
	fg = SecondPassColor;
	SecondPassColor = -1;
	pattern0 = ~pattern0;
	pattern1 = ~pattern1;
	goto SECOND_PASS;
    }

    SET_SYNC_FLAG(infoRec);
}


void
MGAValidatePolyArc(
   GCPtr 	pGC,
   unsigned long changes,
   DrawablePtr pDraw
){
   ScrnInfoPtr pScrn = xf86Screens[pGC->pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   Bool fullPlanemask = TRUE;

   if((pGC->planemask & pMga->AccelInfoRec->FullPlanemask) !=
	pMga->AccelInfoRec->FullPlanemask)
   {
	if(pMga->AccelFlags & MGA_NO_PLANEMASK) return;
	fullPlanemask = FALSE;
   }

   if(!pGC->lineWidth &&
      (pGC->fillStyle == FillSolid) &&
      (pGC->lineStyle == LineSolid) &&
      ((pGC->alu != GXcopy) || !fullPlanemask))
   {
	pGC->ops->PolyArc = MGAPolyArcThinSolid;
   }
}

static void
MGAPolyPoint (
    DrawablePtr pDraw,
    GCPtr pGC,
    int mode,
    int npt,
    xPoint *ppt
){
    int numRects = REGION_NUM_RECTS(pGC->pCompositeClip);
    XAAInfoRecPtr infoRec;
    BoxPtr pbox;
    MGAPtr pMga;
    int xorg, yorg;

    if(!numRects) return;

    if(numRects != 1) {
	XAAFallbackOps.PolyPoint(pDraw, pGC, mode, npt, ppt);
	return;
    }

    infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    pMga = MGAPTR(infoRec->pScrn);
    xorg = pDraw->x;
    yorg = pDraw->y;

    pbox = REGION_RECTS(pGC->pCompositeClip);

    (*infoRec->SetClippingRectangle)(infoRec->pScrn,
                pbox->x1, pbox->y1, pbox->x2 - 1, pbox->y2 - 1);
    (*infoRec->SetupForSolidFill)(infoRec->pScrn, pGC->fgPixel, pGC->alu,
				   pGC->planemask);

    if(mode == CoordModePrevious) {
	while(npt--) {
	    xorg += ppt->x;
	    yorg += ppt->y;
	    WAITFIFO(2);
	    OUTREG(MGAREG_FXBNDRY, ((xorg + 1) << 16) | (xorg & 0xffff));
	    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, (yorg << 16) | 1);
	    ppt++;
	}
    } else {
	int x;
	while(npt--) {
	    x = ppt->x + xorg;
	    WAITFIFO(2);
	    OUTREG(MGAREG_FXBNDRY, ((x + 1) << 16) | (x & 0xffff));
	    OUTREG(MGAREG_YDSTLEN + MGAREG_EXEC, ((ppt->y + yorg) << 16) | 1);
	    ppt++;
	}
    }

    (*infoRec->DisableClipping)(infoRec->pScrn);

    SET_SYNC_FLAG(infoRec);
}


void
MGAValidatePolyPoint(
   GCPtr 	pGC,
   unsigned long changes,
   DrawablePtr pDraw
){
   ScrnInfoPtr pScrn = xf86Screens[pGC->pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   Bool fullPlanemask = TRUE;

   pGC->ops->PolyPoint = XAAFallbackOps.PolyPoint;

   if((pGC->planemask & pMga->AccelInfoRec->FullPlanemask) !=
	pMga->AccelInfoRec->FullPlanemask)
   {
	if(pMga->AccelFlags & MGA_NO_PLANEMASK) return;
	fullPlanemask = FALSE;
   }

   if((pGC->alu != GXcopy) || !fullPlanemask)
	pGC->ops->PolyPoint = MGAPolyPoint;
}


void
MGAFillCacheBltRects(
   ScrnInfoPtr pScrn,
   int rop,
   unsigned int planemask,
   int nBox,
   BoxPtr pBox,
   int xorg, int yorg,
   XAACacheInfoPtr pCache
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    int x, y, phaseY, phaseX, skipleft, height, width, w, blit_w, blit_h, start;

    CHECK_DMA_QUIESCENT(MGAPTR(pScrn), pScrn);

    (*infoRec->SetupForScreenToScreenCopy)(pScrn, 1, 1, rop, planemask,
		pCache->trans_color);

    while(nBox--) {
	y = pBox->y1;
	phaseY = (y - yorg) % pCache->orig_h;
	if(phaseY < 0) phaseY += pCache->orig_h;
	phaseX = (pBox->x1 - xorg) % pCache->orig_w;
	if(phaseX < 0) phaseX += pCache->orig_w;
	height = pBox->y2 - y;
	width = pBox->x2 - pBox->x1;
	start = phaseY ? (pCache->orig_h - phaseY) : 0;

	/* This is optimized for WRAM */

	if ((rop == GXcopy) && (height >= (pCache->orig_h + start))) {
	    w = width; skipleft = phaseX; x = pBox->x1;
	    blit_h = pCache->orig_h;

	    while(1) {
		blit_w = pCache->w - skipleft;
		if(blit_w > w) blit_w = w;
		(*infoRec->SubsequentScreenToScreenCopy)(pScrn,
			pCache->x + skipleft, pCache->y,
			x, y + start, blit_w, blit_h);
		w -= blit_w;
		if(!w) break;
		x += blit_w;
		skipleft = (skipleft + blit_w) % pCache->orig_w;
	    }
	    height -= blit_h;

	    if(start) {
		(*infoRec->SubsequentScreenToScreenCopy)(pScrn,
			pBox->x1, y + blit_h, pBox->x1, y, width, start);
		height -= start;
		y += start;
	    }
	    start = blit_h;

	    while(height) {
		if(blit_h > height) blit_h = height;
		(*infoRec->SubsequentScreenToScreenCopy)(pScrn,
			pBox->x1, y,
			pBox->x1, y + start, width, blit_h);
		height -= blit_h;
		start += blit_h;
		blit_h <<= 1;
	    }
	} else {
	    while(1) {
		w = width; skipleft = phaseX; x = pBox->x1;
		blit_h = pCache->h - phaseY;
		if(blit_h > height) blit_h = height;

		while(1) {
		    blit_w = pCache->w - skipleft;
		    if(blit_w > w) blit_w = w;
		    (*infoRec->SubsequentScreenToScreenCopy)(pScrn,
			pCache->x + skipleft, pCache->y + phaseY,
			x, y, blit_w, blit_h);
		    w -= blit_w;
		    if(!w) break;
		    x += blit_w;
		    skipleft = (skipleft + blit_w) % pCache->orig_w;
		}
		height -= blit_h;
		if(!height) break;
		y += blit_h;
		phaseY = (phaseY + blit_h) % pCache->orig_h;
	    }
	}
	pBox++;
    }

    SET_SYNC_FLAG(infoRec);
}

#endif

#ifdef XF86DRI
void
MGANAME(DRIInitBuffers)(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    BoxPtr pbox = REGION_RECTS(prgn);
    int nbox  = REGION_NUM_RECTS(prgn);

    CHECK_DMA_QUIESCENT(MGAPTR(pScrn), pScrn);

    MGANAME(SetupForSolidFill)(pScrn, 0, GXcopy, -1);
    while (nbox--) {
        MGASelectBuffer(pScrn, MGA_BACK);
        MGANAME(SubsequentSolidFillRect)(pScrn, pbox->x1, pbox->y1,
					 pbox->x2-pbox->x1, pbox->y2-pbox->y1);
        MGASelectBuffer(pScrn, MGA_DEPTH);
        MGANAME(SubsequentSolidFillRect)(pScrn, pbox->x1, pbox->y1,
					 pbox->x2-pbox->x1, pbox->y2-pbox->y1);
        pbox++;
    }
    MGASelectBuffer(pScrn, MGA_FRONT);

    pMga->AccelInfoRec->NeedToSync = TRUE;
}

/*
  This routine is a modified form of XAADoBitBlt with the calls to
  ScreenToScreenBitBlt built in. My routine has the prgnSrc as source
  instead of destination. My origin is upside down so the ydir cases
  are reversed.
*/

void
MGANAME(DRIMoveBuffers)(WindowPtr pParent, DDXPointRec ptOldOrg,
		   RegionPtr prgnSrc, CARD32 index)
{
    ScreenPtr pScreen = pParent->drawable.pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    int nbox;
    BoxPtr pbox, pboxTmp, pboxNext, pboxBase, pboxNew1, pboxNew2;
    DDXPointPtr pptTmp, pptNew1, pptNew2;
    int xdir, ydir;
    int dx, dy;
    DDXPointPtr pptSrc;
    int screenwidth = pScrn->virtualX;
    int screenheight = pScrn->virtualY;

    CHECK_DMA_QUIESCENT(MGAPTR(pScrn), pScrn);

    pbox = REGION_RECTS(prgnSrc);
    nbox = REGION_NUM_RECTS(prgnSrc);
    pboxNew1 = 0;
    pptNew1 = 0;
    pboxNew2 = 0;
    pboxNew2 = 0;
    pptSrc = &ptOldOrg;

    dx = pParent->drawable.x - ptOldOrg.x;
    dy = pParent->drawable.y - ptOldOrg.y;

    /* If the copy will overlap in Y, reverse the order */
    if (dy>0) {
        ydir = -1;

        if (nbox>1) {
	    /* Keep ordering in each band, reverse order of bands */
	    pboxNew1 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec)*nbox);
	    if (!pboxNew1) return;
	    pptNew1 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec)*nbox);
	    if (!pptNew1) {
	        DEALLOCATE_LOCAL(pboxNew1);
	        return;
	    }
	    pboxBase = pboxNext = pbox+nbox-1;
	    while (pboxBase >= pbox) {
	        while ((pboxNext >= pbox) && (pboxBase->y1 == pboxNext->y1))
		  pboxNext--;
	        pboxTmp = pboxNext+1;
	        pptTmp = pptSrc + (pboxTmp - pbox);
	        while (pboxTmp <= pboxBase) {
		    *pboxNew1++ = *pboxTmp++;
		    *pptNew1++ = *pptTmp++;
		}
	        pboxBase = pboxNext;
	    }
	    pboxNew1 -= nbox;
	    pbox = pboxNew1;
	    pptNew1 -= nbox;
	    pptSrc = pptNew1;
	}
    } else {
        /* No changes required */
        ydir = 1;
    }

    /* If the regions will overlap in X, reverse the order */
    if (dx>0) {
        xdir = -1;

        if (nbox > 1) {
	    /*reverse orderof rects in each band */
	    pboxNew2 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec)*nbox);
	    pptNew2 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec)*nbox);
	    if (!pboxNew2 || !pptNew2) {
	        if (pptNew2) DEALLOCATE_LOCAL(pptNew2);
	        if (pboxNew2) DEALLOCATE_LOCAL(pboxNew2);
	        if (pboxNew1) {
		    DEALLOCATE_LOCAL(pptNew1);
		    DEALLOCATE_LOCAL(pboxNew1);
		}
	       return;
	    }
	    pboxBase = pboxNext = pbox;
	    while (pboxBase < pbox+nbox) {
	        while ((pboxNext < pbox+nbox) &&
		       (pboxNext->y1 == pboxBase->y1))
		  pboxNext++;
	        pboxTmp = pboxNext;
	        pptTmp = pptSrc + (pboxTmp - pbox);
	        while (pboxTmp != pboxBase) {
		    *pboxNew2++ = *--pboxTmp;
		    *pptNew2++ = *--pptTmp;
		}
	        pboxBase = pboxNext;
	    }
	    pboxNew2 -= nbox;
	    pbox = pboxNew2;
	    pptNew2 -= nbox;
	    pptSrc = pptNew2;
	}
    } else {
        /* No changes are needed */
        xdir = 1;
    }

    MGANAME(SetupForScreenToScreenCopy)(pScrn, xdir, ydir, GXcopy, -1, -1);
    for ( ; nbox-- ; pbox++)
     {
	 int x1 = pbox->x1;
	 int y1 = pbox->y1;
	 int destx = x1 + dx;
	 int desty = y1 + dy;
	 int w = pbox->x2 - x1 + 1;
	 int h = pbox->y2 - y1 + 1;

	 if ( destx < 0 ) x1 -= destx, w += destx, destx = 0;
	 if ( desty < 0 ) y1 -= desty, h += desty, desty = 0;
	 if ( destx + w > screenwidth ) w = screenwidth - destx;
	 if ( desty + h > screenheight ) h = screenheight - desty;
	 if ( w <= 0 ) continue;
	 if ( h <= 0 ) continue;

	 MGASelectBuffer(pScrn, MGA_BACK);
	 MGANAME(SubsequentScreenToScreenCopy)(pScrn, x1, y1,
					       destx,desty, w, h);
	 MGASelectBuffer(pScrn, MGA_DEPTH);
	 MGANAME(SubsequentScreenToScreenCopy)(pScrn, x1,y1,
					       destx,desty, w, h);
     }
    MGASelectBuffer(pScrn, MGA_FRONT);

    if (pboxNew2) {
        DEALLOCATE_LOCAL(pptNew2);
        DEALLOCATE_LOCAL(pboxNew2);
    }
    if (pboxNew1) {
        DEALLOCATE_LOCAL(pptNew1);
        DEALLOCATE_LOCAL(pboxNew1);
    }

    pMga->AccelInfoRec->NeedToSync = TRUE;
}

#endif /* XF86DRI */
