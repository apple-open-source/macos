/* (c) Itai Nahshon */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cirrus/alp_xaa.c,v 1.8 2002/01/25 21:56:00 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "cir.h"
#define _ALP_PRIVATE_
#include "alp.h"

#define WAIT	outb(pCir->PIOReg, 0x31); \
		while(inb(pCir->PIOReg + 1) & pCir->chip.alp->waitMsk){};
#define WAIT_1	outb(pCir->PIOReg, 0x31); \
		while(inb(pCir->PIOReg + 1) & 0x1){};

static const CARD16 translated_rop[] =
{
  /* GXclear */        0x0032U,
  /* GXand   */        0x0532U,
  /* GXandreverse */   0x0932U,
  /* GXcopy */         0x0D32U,
  /* GXandinversted */ 0x5032U,
  /* GXnoop */         0x0632U,
  /* GXxor */          0x5932U,
  /* GXor */           0x6D32U,
  /* GXnor */          0x9032U,
  /* GXequiv */        0x9532U,
  /* GXinvert */       0x0B32U,
  /* GXorReverse */    0xAD32U,
  /* GXcopyInverted */ 0xD032U,
  /* GXorInverted */   0xD632U,
  /* GXnand */         0xDA32U,
  /* GXset */          0x0E32U
};

#if 1
#define SetupForRop(rop) outw(pCir->PIOReg, translated_rop[rop])
#else
#define SetupForRop(rop) outw(pCir->PIOReg, 0x0D32)
#endif

static void AlpSync(ScrnInfoPtr pScrn)
{
	CirPtr pCir = CIRPTR(pScrn);

#ifdef ALP_DEBUG
	ErrorF("AlpSync\n");
#endif
	WAIT_1;
	return;
}

static void
AlpSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
			      int rop, unsigned int planemask,
			      int trans_color)
{
	CirPtr pCir = CIRPTR(pScrn);
	int pitch = pCir->pitch;

#ifdef ALP_DEBUG
	ErrorF("AlpSetupForScreenToScreenCopy xdir=%d ydir=%d rop=%x planemask=%x trans_color=%x\n",
		xdir, ydir, rop, planemask, trans_color);
#endif
	WAIT;
	SetupForRop(rop);
	/* Set dest pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x24);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x25);
	/* Set source pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x26);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x27);
}

static void
AlpSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2,
								int y2, int w, int h)
{
	CirPtr pCir = CIRPTR(pScrn);
	int source, dest;
	int  hh, ww;
	int decrement = 0;
	int pitch = pCir->pitch;

	ww = (w * pScrn->bitsPerPixel / 8) - 1;
	hh = h - 1;
	dest = y2 * pitch + x2 * pScrn->bitsPerPixel / 8;
	source = y1 * pitch + x1 * pScrn->bitsPerPixel / 8;
	if (dest > source) {
		decrement = 1 << 8;
		dest += hh * pitch + ww;
		source += hh * pitch + ww;
	}

	WAIT;

	outw(pCir->PIOReg, decrement | 0x30);

	/* Width */
	outw(pCir->PIOReg, ((ww << 8) & 0xff00) | 0x20);
	outw(pCir->PIOReg, ((ww) & 0x1f00) | 0x21);
	/* Height */
	outw(pCir->PIOReg, ((hh << 8) & 0xff00) | 0x22);
	outw(pCir->PIOReg, ((hh) & 0x0700) | 0x23);


	/* source */
	outw(pCir->PIOReg, ((source << 8) & 0xff00) | 0x2C);
	outw(pCir->PIOReg, ((source) & 0xff00) | 0x2D);
	outw(pCir->PIOReg, ((source >> 8) & 0x3f00)| 0x2E);

	/* dest */
	outw(pCir->PIOReg, ((dest  << 8) & 0xff00) | 0x28);
	outw(pCir->PIOReg, ((dest) & 0xff00) | 0x29);
	outw(pCir->PIOReg, ((dest >> 8) & 0x3f00) | 0x2A);
	if (!pCir->chip.alp->autoStart)
	  outw(pCir->PIOReg, 0x0231);

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentScreenToScreenCopy x1=%d y1=%d x2=%d y2=%d w=%d h=%d\n",
			x1, y1, x2, y2, w, h);
	ErrorF("AlpSubsequentScreenToScreenCopy s=%d d=%d ww=%d hh=%d\n",
			source, dest, ww, hh);
#endif

}

static void
AlpSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
						unsigned int planemask)
{
	CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);
	int pitch = pCir->pitch;

#ifdef ALP_DEBUG
	ErrorF("AlpSetupForSolidFill color=%x rop=%x planemask=%x\n",
			color, rop, planemask);
#endif
	WAIT;

	SetupForRop(rop);

	switch (pCir -> Chipset)
	{
	case PCI_CHIP_GD7548:
	  /* The GD7548 does not (apparently) support solid filling
	     directly, it always need an actual source.
	     We therefore use it as a pattern fill with a solid
	     pattern */
	  {
	    int source = pAlp->monoPattern8x8;
	    /* source = 8x8 solid mono pattern */
	    outw(pCir->PIOReg, ((source << 8) & 0xff00) | 0x2C);
	    outw(pCir->PIOReg, ((source) & 0xff00) | 0x2D);
	    outw(pCir->PIOReg, ((source >> 8) & 0x3f00) | 0x2E);
	    /* memset() may not be the fastest */
	    memset(pCir->FbBase + pAlp->monoPattern8x8, 0xFF, 8);
	    write_mem_barrier();
	    break;
	  }
        default:
          /* GR33 = 0x04 => does not exist on GD7548 */
	  outw(pCir->PIOReg, 0x0433);
	}

        /* GR30 = color expansion, pattern copy */
	/* Choses 8bpp / 16bpp color expansion */
	outw(pCir->PIOReg, 0xC030 |((pScrn->bitsPerPixel - 8) << 9));

	outw(pCir->PIOReg, ((color << 8) & 0xff00) | 0x01);
	outw(pCir->PIOReg, ((color) & 0xff00) | 0x11);
	outw(pCir->PIOReg, ((color >> 8) & 0xff00) | 0x13);
	outw(pCir->PIOReg, 0x15);

	/* Set dest pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x24);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x25);
}

static void
AlpSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
	CirPtr pCir = CIRPTR(pScrn);
	int dest;
	int hh, ww;
	int pitch = pCir->pitch;

	ww = (w * pScrn->bitsPerPixel / 8) - 1;
	hh = h - 1;
	dest = y * pitch + x * pScrn->bitsPerPixel / 8;

	WAIT;

	/* Width */
	outw(pCir->PIOReg, ((ww << 8) & 0xff00) | 0x20);
	outw(pCir->PIOReg, ((ww) & 0x1f00) | 0x21);
	/* Height */
	outw(pCir->PIOReg, ((hh << 8) & 0xff00) | 0x22);
	outw(pCir->PIOReg, ((hh) & 0x0700) | 0x23);

	/* dest */
	outw(pCir->PIOReg, ((dest << 8) & 0xff00) | 0x28);
	outw(pCir->PIOReg, ((dest) & 0xff00) | 0x29);
	outw(pCir->PIOReg, ((dest >> 8) & 0x3f00) | 0x2A);
	if (!pCir->chip.alp->autoStart)
	  outw(pCir->PIOReg, 0x0231);

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentSolidFillRect x=%d y=%d w=%d h=%d\n",
			x, y, w, h);
#endif

}

static void
AlpSetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
			      int patx, int paty,
			      int fg, int bg,
			      int rop, unsigned int planemask)
{
	CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);
	int pitch = pCir->pitch;

#ifdef ALP_DEBUG
	ErrorF("AlpSetupFor8x8PatternFill pattern=%8x%8x"
	       "fg=%x bg=%x rop=%x planemask=%x\n",
			patx, paty, fg, bg, rop, planemask);
#endif
	WAIT;

	SetupForRop(rop);

	{
	  int source = pAlp->monoPattern8x8;
	  /* source = 8x8 solid mono pattern */
	  outw(pCir->PIOReg, ((source << 8) & 0xff00) | 0x2C);
	  outw(pCir->PIOReg, ((source) & 0xff00) | 0x2D);
	  outw(pCir->PIOReg, ((source >> 8) & 0x3f00) | 0x2E);
	}

        /* GR30 = color expansion, pattern copy */
	/* Choses 8bpp / 16bpp color expansion */
	if (bg == -1)
	{ /* transparency requested */
	  outw(pCir->PIOReg, 0xC830 |((pScrn->bitsPerPixel - 8) << 9));

	  bg = ~fg;
	  /* transparent color compare */
	  outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x34);
	  outw(pCir->PIOReg, ((bg) & 0xff00) | 0x35);

	  /* transparent color mask = 0 (all bits matters) */
	  outw(pCir->PIOReg, 0x38);
	  outw(pCir->PIOReg, 0x39);
	}
	else
	{
	  outw(pCir->PIOReg, 0xC030 |((pScrn->bitsPerPixel - 8) << 9));
	}

	outw(pCir->PIOReg, ((fg << 8) & 0xff00) | 0x01);
	outw(pCir->PIOReg, ((fg) & 0xff00) | 0x11);

	outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x00);
	outw(pCir->PIOReg, ((bg) & 0xff00) | 0x10);

	/* Set dest pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x24);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x25);
}

static void
AlpSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, int paty,
			   int x, int y, int w, int h)
{
	CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);
	int dest;
	int hh, ww;
	int pitch = pCir->pitch;

	ww = (w * pScrn->bitsPerPixel / 8) - 1;
	hh = h - 1;
	dest = y * pitch + x * pScrn->bitsPerPixel / 8;

	WAIT;
	/* memcpy() may not be the fastest */
	memcpy(pCir->FbBase + pAlp->monoPattern8x8, &patx, 4);
	memcpy(pCir->FbBase + pAlp->monoPattern8x8 + 4, &paty, 4);
	write_mem_barrier();

	/* Width */
	outw(pCir->PIOReg, ((ww << 8) & 0xff00) | 0x20);
	outw(pCir->PIOReg, ((ww) & 0x1f00) | 0x21);
	/* Height */
	outw(pCir->PIOReg, ((hh << 8) & 0xff00) | 0x22);
	outw(pCir->PIOReg, ((hh) & 0x0700) | 0x23);

	/* dest */
	outw(pCir->PIOReg, ((dest << 8) & 0xff00) | 0x28);
	outw(pCir->PIOReg, ((dest) & 0xff00) | 0x29);
	outw(pCir->PIOReg, ((dest >> 8) & 0x3f00) | 0x2A);
	if (!pCir->chip.alp->autoStart)
	  outw(pCir->PIOReg, 0x0231);

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequent8x8PatternFill x=%d y=%d w=%d h=%d\n",
			x, y, w, h);
#endif

}

#if 0
/* XF86 does not support byte-padded scanlines */

static void
AlpSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                        int fg, int bg,
                        int rop,
                        unsigned int planemask)
{
	CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);
	int pitch = pCir->pitch;

#ifdef ALP_DEBUG
	ErrorF("AlpSetupForCPUToScreenColorExpandFill "
	       "fg=%x bg=%x rop=%x planemask=%x\n",
			fg, bg, rop, planemask);
#endif
	WAIT;

	SetupForRop(rop);

        /* GR30 = color expansion, CPU->display copy */
	/* Choses 8bpp / 16bpp color expansion */
	if (bg == -1)
	{ /* transparency requested */
	  outw(pCir->PIOReg, 0x8C30 |((pScrn->bitsPerPixel - 8) << 9));

	  bg = ~fg;
	  /* transparent color compare */
	  outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x34);
	  outw(pCir->PIOReg, ((bg) & 0xff00) | 0x35);

	  /* transparent color mask = 0 (all bits matters) */
	  outw(pCir->PIOReg, 0x38);
	  outw(pCir->PIOReg, 0x39);
	}
	else
	{
	  outw(pCir->PIOReg, 0x8430 |((pScrn->bitsPerPixel - 8) << 9));
	}

	outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x00);
	outw(pCir->PIOReg, ((bg) & 0xff00) | 0x10);

	outw(pCir->PIOReg, ((fg << 8) & 0xff00) | 0x01);
	outw(pCir->PIOReg, ((fg) & 0xff00) | 0x11);

	/* Set dest pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x24);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x25);  
}

static void
AlpSubsequentCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft)
{
	CirPtr pCir = CIRPTR(pScrn);
	int dest;
	int hh, ww;
	int pitch = pCir->pitch;

	ww = (((w+7) & ~7) * pScrn->bitsPerPixel / 8) - 1;
	hh = h - 1;
	dest = y * pitch + x * pScrn->bitsPerPixel / 8;

	WAIT;

	/* Width */
	outw(pCir->PIOReg, ((ww << 8) & 0xff00) | 0x20);
	outw(pCir->PIOReg, ((ww) & 0x1f00) | 0x21);
	/* Height */
	outw(pCir->PIOReg, ((hh << 8) & 0xff00) | 0x22);
	outw(pCir->PIOReg, ((hh) & 0x0700) | 0x23);

	/* source = CPU ; description of bit 2 of GR30 in the 7548 manual
	   says that if we do color expansion we must zero the source
	   adress registers (GR2C, GR2D, GR2E) */
	outw(pCir->PIOReg, 0x2C);
	outw(pCir->PIOReg, 0x2D);
	outw(pCir->PIOReg, 0x2E);

	/* dest */
	outw(pCir->PIOReg, ((dest << 8) & 0xff00) | 0x28);
	outw(pCir->PIOReg, ((dest) & 0xff00) | 0x29);
	outw(pCir->PIOReg, ((dest >> 8) & 0x3f00) | 0x2A);
	if (!pCir->chip.alp->autoStart)
	  outw(pCir->PIOReg, 0x0231);

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentCPUToScreenColorExpandFill x=%d y=%d w=%d h=%d\n",
			x, y, w, h);
#endif
}
#endif

#if 1
static void
AlpSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                        int fg, int bg,
                        int rop,
                        unsigned int planemask)
{
	CirPtr pCir = CIRPTR(pScrn);
	int pitch = pCir->pitch;

#ifdef ALP_DEBUG
	ErrorF("AlpSetupForCPUToScreenColorExpandFill "
	       "fg=%x bg=%x rop=%x planemask=%x, bpp=%d\n",
			fg, bg, rop, planemask, pScrn->bitsPerPixel);
#endif
	WAIT;

	SetupForRop(rop);

        /* GR30 = color expansion, CPU->display copy */
	/* Choses 8bpp / 16bpp color expansion */
	if (bg == -1)
	{ /* transparency requested */
	  if (pScrn->bitsPerPixel > 8) /* 16 bpp */
	  {
	    outw(pCir->PIOReg, 0x9C30);

	    bg = ~fg;
	    /* transparent color compare */
	    outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x34);
	    outw(pCir->PIOReg, ((bg) & 0xff00) | 0x35);
	  } else /* 8 bpp */
	  {
	    outw(pCir->PIOReg, 0x8C30);

	    bg = ~fg;
	    /* transparent color compare */
	    outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x34);
	    outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x35);
	  }

	  /* transparent color mask = 0 (all bits matters) */
	  outw(pCir->PIOReg, 0x38);
	  outw(pCir->PIOReg, 0x39);
	}
	else
	{
	  outw(pCir->PIOReg, 0x8430 |((pScrn->bitsPerPixel - 8) << 9));
	}

	outw(pCir->PIOReg, ((bg << 8) & 0xff00) | 0x00);
	outw(pCir->PIOReg, ((bg) & 0xff00) | 0x10);

	outw(pCir->PIOReg, ((fg << 8) & 0xff00) | 0x01);
	outw(pCir->PIOReg, ((fg) & 0xff00) | 0x11);

	/* Set dest pitch */
	outw(pCir->PIOReg, ((pitch << 8) & 0xff00) | 0x24);
	outw(pCir->PIOReg, ((pitch) & 0x1f00) | 0x25);  
}

static void
AlpSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft)
{
	CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);

	int pitch = pCir->pitch;

	pAlp->SubsequentColorExpandScanlineByteWidth =
	  (w * pScrn->bitsPerPixel / 8) - 1;
	pAlp->SubsequentColorExpandScanlineDWordWidth =
	  (w + 31) >> 5;
	pAlp->SubsequentColorExpandScanlineDest =
	  y * pitch + x * pScrn->bitsPerPixel / 8;

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentScanlineCPUToScreenColorExpandFill x=%d y=%d w=%d h=%d skipleft=%d\n",
			x, y, w, h, skipleft);
#endif
}

static void 
AlpSubsequentColorExpandScanline(
	ScrnInfoPtr pScrn,
	int bufno)
{
        CirPtr pCir = CIRPTR(pScrn);
	AlpPtr pAlp = ALPPTR(pCir);
	int dest=pAlp->SubsequentColorExpandScanlineDest;
	int ww=pAlp->SubsequentColorExpandScanlineByteWidth;
	int width=pAlp->SubsequentColorExpandScanlineDWordWidth;
	CARD32* from;
	volatile CARD32 *to;

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentColorExpandScanline\n");
#endif

	pAlp->SubsequentColorExpandScanlineDest += pCir->pitch;

	to   = (CARD32*) pCir->FbBase;
	from = (CARD32*) (pCir->ScanlineColorExpandBuffers[bufno]);
	WAIT_1;

	/* Width */
	outw(pCir->PIOReg, ((ww << 8) & 0xff00) | 0x20);
	outw(pCir->PIOReg, ((ww) & 0x1f00) | 0x21);

	/* Height = 1 */
	outw(pCir->PIOReg, 0x22);
	outw(pCir->PIOReg, 0x23);

	/* source = CPU ; description of bit 2 of GR30 in the 7548 manual
	   says that if we do color expansion we must zero the source
	   adress registers (GR2C, GR2D, GR2E) */
	outw(pCir->PIOReg, 0x2C);
	outw(pCir->PIOReg, 0x2D);
	outw(pCir->PIOReg, 0x2E);

	/* dest */
	outw(pCir->PIOReg, ((dest << 8) & 0xff00) | 0x28);
	outw(pCir->PIOReg, ((dest) & 0xff00) | 0x29);
	write_mem_barrier();

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentColorExpandScanline (2)\n");
#endif

	outw(pCir->PIOReg, ((dest >> 8) & 0x3f00) | 0x2A);
	if (!pCir->chip.alp->autoStart)
	  outw(pCir->PIOReg, 0x0231);

	{
	  int i;
	  for (i=0; i<width; i++)
	    *to=*(from++);
	  write_mem_barrier();
	}

#ifdef ALP_DEBUG
	ErrorF("AlpSubsequentColorExpandScanline (3)\n");
#endif
}
#endif

static void
AlpAccelEngineInit(ScrnInfoPtr pScrn)
{
    CirPtr pCir = CIRPTR(pScrn);

    outw(pCir->PIOReg, 0x200E); /* enable writes to gr33 */
    /* Setup things for autostart */
    if (pCir->properties & ACCEL_AUTOSTART) {
        outw(pCir->PIOReg, 0x8031); /* enable autostart */
	pCir->chip.alp->waitMsk = 0x10;
	pCir->chip.alp->autoStart = TRUE;
    } else {
        pCir->chip.alp->waitMsk = 0x1;
	pCir->chip.alp->autoStart = FALSE;
    }
}

Bool
AlpXAAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    CirPtr pCir = CIRPTR(pScrn);
    AlpPtr pAlp = ALPPTR(pCir);
    XAAInfoRecPtr XAAPtr;

    pCir->InitAccel =  AlpAccelEngineInit;
#ifdef ALP_DEBUG
    ErrorF("AlpXAAInit\n");
#endif

    XAAPtr = XAACreateInfoRec();
    if (!XAAPtr) return FALSE;

    /* Pixmap cache */
    XAAPtr->Flags |= LINEAR_FRAMEBUFFER;
    XAAPtr->Sync = AlpSync;
    
    XAAPtr->SetupForScreenToScreenCopy = AlpSetupForScreenToScreenCopy;
    XAAPtr->SubsequentScreenToScreenCopy = AlpSubsequentScreenToScreenCopy;
    XAAPtr->ScreenToScreenCopyFlags =
        NO_TRANSPARENCY | NO_PLANEMASK;

    XAAPtr->SetupForSolidFill = AlpSetupForSolidFill;
    XAAPtr->SubsequentSolidFillRect = AlpSubsequentSolidFillRect;
    XAAPtr->SubsequentSolidFillTrap = NULL;
    XAAPtr->SolidFillFlags = NO_PLANEMASK;

    if (pCir->Chipset == PCI_CHIP_GD7548) {
        if (pAlp->monoPattern8x8) {
	    XAAPtr->SetupForMono8x8PatternFill 
	        = AlpSetupForMono8x8PatternFill;
	    XAAPtr->SubsequentMono8x8PatternFillRect 
	        = AlpSubsequentMono8x8PatternFillRect;
	    XAAPtr->SubsequentMono8x8PatternFillTrap = NULL;
	    XAAPtr->Mono8x8PatternFillFlags =
	        NO_PLANEMASK |
		HARDWARE_PATTERN_PROGRAMMED_BITS | BIT_ORDER_IN_BYTE_MSBFIRST;
	}
#if 1
	/* kludge: since XF86 does not support byte-padded
	   mono bitmaps (only dword-padded), use the
	   scanline version */
	XAAPtr->SetupForScanlineCPUToScreenColorExpandFill =
	    AlpSetupForScanlineCPUToScreenColorExpandFill;
	XAAPtr->SubsequentScanlineCPUToScreenColorExpandFill =
	    AlpSubsequentScanlineCPUToScreenColorExpandFill;
	XAAPtr->SubsequentColorExpandScanline =
	    AlpSubsequentColorExpandScanline;
	{
	  const int NumScanlineColorExpandBuffers = 2;
	  int i;
	  int buffer_size = (pCir->pScrn->virtualX + 31) & ~31;
#ifdef ALP_DEBUG
	  ErrorF("Computing buffers for %d pixel lines\n",
		 pCir->pScrn->virtualX);
#endif
	  XAAPtr->NumScanlineColorExpandBuffers =
	      NumScanlineColorExpandBuffers;
	  XAAPtr->ScanlineColorExpandBuffers =
	      pCir->ScanlineColorExpandBuffers = (unsigned char **)
	      (malloc(sizeof(unsigned char *) *
		      NumScanlineColorExpandBuffers));
	    /* TODO: are those mallocs to be freed ? */

	  for(i=0; i<NumScanlineColorExpandBuffers; i++)
	      pCir->ScanlineColorExpandBuffers[i] = (unsigned char *)
		malloc(buffer_size);
	}
	XAAPtr->ScanlineCPUToScreenColorExpandFillFlags =
	    NO_PLANEMASK | BIT_ORDER_IN_BYTE_MSBFIRST |
	    SCANLINE_PAD_DWORD | ROP_NEEDS_SOURCE;
#endif
#if 0
	/* Currently disabled: XF86 sends DWORD-padded data,
	   not byte-padded */
	XAAPtr->SetupForCPUToScreenColorExpandFill =
	  AlpSetupForCPUToScreenColorExpandFill;
	XAAPtr->SubsequentCPUToScreenColorExpandFill =
	  AlpSubsequentCPUToScreenColorExpandFill;
	XAAPtr->ColorExpandBase = pCir->FbBase + 4;
	XAAPtr->CPUToScreenColorExpandFillFlags =
	  NO_PLANEMASK | BIT_ORDER_IN_BYTE_MSBFIRST |
	  SCANLINE_PAD_DWORD | ROP_NEEDS_SOURCE |
	  CPU_TRANSFER_PAD_DWORD | CPU_TRANSFER_BASE_FIXED;
#endif
    }

    AlpAccelEngineInit(pScrn);

    pCir->AccelInfoRec = XAAPtr;

    if (!XAAInit(pScreen, XAAPtr))
        return FALSE;

    return TRUE;
}

