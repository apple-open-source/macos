/* (c) Itai Nahshon */
/* #define DEBUG */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cirrus/alp_xaam.c,v 1.8 2002/07/10 02:36:50 tsi Exp $ */

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

#ifdef DEBUG
#define minb(p) \
        (ErrorF("minb(%X)\n", p),\
        MMIO_IN8(pCir->chip.alp->BLTBase, (p)))
#define moutb(p,v) \
        (ErrorF("moutb(%X, %X)\n", p,v),\
	MMIO_OUT8(pCir->chip.alp->BLTBase, (p),(v)))
#define vga_minb(p) \
        (ErrorF("minb(%X)\n", p),\
        MMIO_IN8(hwp->MMIOBase, (hwp->MMIOOffset + (p))))
#define vga_moutb(p,v) \
        { ErrorF("moutb(%X, %X)\n", p,v);\
	MMIO_OUT8(hwp->MMIOBase, (hwp->MMIOOffset + (p)),(v));}
#define minl(p) \
        (ErrorF("minl(%X)\n", p),\
        MMIO_IN32(pCir->chip.alp->BLTBase, (p)))
#define moutl(p,v) \
        (ErrorF("moutl(%X, %X)\n", p,v),\
	MMIO_OUT32(pCir->chip.alp->BLTBase, (p),(v)))
#else
#define minb(p) MMIO_IN8(pCir->chip.alp->BLTBase, (p))
#define moutb(p,v) MMIO_OUT8(pCir->chip.alp->BLTBase, (p),(v))
#define vga_minb(p) MMIO_IN8(hwp->MMIIOBase, (hwp->MMIOOffset + (p)))
#define vga_moutb(p,v) MMIO_OUT8(hwp->MMIOBase, (hwp->MMIOOffset + (p)),(v))
#define minl(p) MMIO_IN32(pCir->chip.alp->BLTBase, (p))
#define moutl(p,v) MMIO_OUT32(pCir->chip.alp->BLTBase, (p),(v))
#endif

static const CARD8 translated_rop[] =
{
  /* GXclear */        0x00U,
  /* GXand   */        0x05U,
  /* GXandreverse */   0x09U,
  /* GXcopy */         0x0DU,
  /* GXandinversted */ 0x50U,
  /* GXnoop */         0x06U,
  /* GXxor */          0x59U,
  /* GXor */           0x6DU,
  /* GXnor */          0x90U,
  /* GXequiv */        0x95U,
  /* GXinvert */       0x0BU,
  /* GXorReverse */    0xADU,
  /* GXcopyInverted */ 0xD0U,
  /* GXorInverted */   0xD6U,
  /* GXnand */         0xDAU,
  /* GXset */          0x0EU
};

#define WAIT while(minl(0x40) & pCir->chip.alp->waitMsk){};
#define WAIT_1 while((minl(0x40)) & 0x1){};

static void AlpSync(ScrnInfoPtr pScrn)
{
    CirPtr pCir = CIRPTR(pScrn);
#ifdef ALP_DEBUG
	ErrorF("AlpSync mm\n");
#endif
	WAIT_1;
	return;
}

static void
AlpSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
								unsigned int planemask, int trans_color)
{
	CirPtr pCir = CIRPTR(pScrn);
	int pitch = pCir->pitch;

	WAIT;

	pCir->chip.alp->transRop = translated_rop[rop] << 16;
	
#ifdef ALP_DEBUG
	ErrorF("AlpSetupForScreenToScreenCopy xdir=%d ydir=%d rop=%x planemask=%x trans_color=%x\n",
			xdir, ydir, rop, planemask, trans_color);
#endif
	moutl(0x0C, (pitch << 16) | pitch); 

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
    
    ww = ((w * pScrn->bitsPerPixel / 8) - 1) & 0x1fff;
    hh = (h - 1) & 0x1fff;
    dest = y2 * pitch + x2 * pScrn->bitsPerPixel / 8;
    source = y1 * pitch + x1 * pScrn->bitsPerPixel / 8;
    if (dest > source) {
        decrement = 1;
	dest += hh * pitch + ww;
	source += hh * pitch + ww;
    }

    WAIT;

    /* Width / Height */
    moutl(0x08, (hh << 16) | ww);
    /* source */
    moutl(0x14, source & 0x3fffff);
    moutl(0x18, pCir->chip.alp->transRop | decrement);
    
    /* dest */
    write_mem_barrier();
    moutl(0x10, dest & 0x3fffff);
    
#ifdef ALP_DEBUG
    ErrorF("AlpSubsequentScreenToScreenCopy x1=%d y1=%d x2=%d y2=%d w=%d h=%d\n",
	   x1, y1, x2, y2, w, h);
    ErrorF("AlpSubsequentScreenToScreenCopy s=%d d=%d ww=%d hh=%d\n",
	   source, dest, ww, hh);
#endif
    if (!pCir->chip.alp->autoStart) {
        CARD32 val = minl(0x40);
	moutl(0x40,val | 0x02);
    }
    write_mem_barrier();
}


static void
AlpSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
						unsigned int planemask)
{
    CirPtr pCir = CIRPTR(pScrn);
    int pitch = pCir->pitch;

    WAIT;

#ifdef ALP_DEBUG
    ErrorF("AlpSetupForSolidFill color=%x rop=%x planemask=%x\n",
	   color, rop, planemask);
#endif

    moutl(0x04, color & 0xffffff);

    /* Set dest pitch */
    moutl(0x0C, pitch & 0x1fff);
    moutl(0x18, (((pScrn->bitsPerPixel - 8) << 1))
	  | translated_rop[rop] << 16
	  | 0x040000C0);
}

static void
AlpSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    int dest;
    int hh, ww;
    CirPtr pCir = CIRPTR(pScrn);
    int pitch = pCir->pitch;

    ww = ((w * pScrn->bitsPerPixel / 8) - 1) & 0x1fff;
    hh = (h - 1) & 0x7ff;
    dest = y * pitch + x * pScrn->bitsPerPixel / 8;

    WAIT;
    
    /* Width / Height */
    write_mem_barrier();
    moutl(0x08, (hh << 16) | ww);

#ifdef ALP_DEBUG
    ErrorF("AlpSubsequentSolidFillRect x=%d y=%d w=%d h=%d\n",
	   x, y, w, h);
#endif
    /* dest */
    moutl(0x10, (dest & 0x3fffff));
    
    if (!pCir->chip.alp->autoStart) {
        CARD32 val = minl(0x40);
	moutl(0x40, val | 0x02);
    }
    write_mem_barrier();
}

static void
AlpAccelEngineInit(ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp = VGAHWPTR(pScrn);
    CirPtr pCir = CIRPTR(pScrn);

    if (pCir->Chipset != PCI_CHIP_GD7548)  {
        vga_moutb(0x3CE, 0x0E); /* enable writes to gr33 */
        vga_moutb(0x3CF, 0x20); /* enable writes to gr33 */
    }
    if (pCir->properties & ACCEL_AUTOSTART) {
        moutl(0x40, 0x80); /* enable autostart */
	pCir->chip.alp->waitMsk = 0x10;
	pCir->chip.alp->autoStart = TRUE;
    } else {
        pCir->chip.alp->waitMsk = 0x1;
	pCir->chip.alp->autoStart = FALSE;
    }
}

Bool
AlpXAAInitMMIO(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	CirPtr pCir = CIRPTR(pScrn);
	XAAInfoRecPtr XAAPtr;
	
	pCir->InitAccel =  AlpAccelEngineInit;
#ifdef ALP_DEBUG
	ErrorF("AlpXAAInitMM\n");
#endif

	XAAPtr = XAACreateInfoRec();
	if (!XAAPtr) return FALSE;
	
	XAAPtr->Flags |= LINEAR_FRAMEBUFFER;
	XAAPtr->Sync = AlpSync;

	XAAPtr->SetupForScreenToScreenCopy = AlpSetupForScreenToScreenCopy;
	XAAPtr->SubsequentScreenToScreenCopy = AlpSubsequentScreenToScreenCopy;
	XAAPtr->ScreenToScreenCopyFlags = 
	  (NO_TRANSPARENCY | NO_PLANEMASK);

	XAAPtr->SetupForSolidFill = AlpSetupForSolidFill;
	XAAPtr->SubsequentSolidFillRect = AlpSubsequentSolidFillRect;
	XAAPtr->SubsequentSolidFillTrap = NULL;
	XAAPtr->SolidFillFlags =  NO_PLANEMASK;

	switch (pCir->Chipset) {
	  case PCI_CHIP_GD5480:
	  case PCI_CHIP_GD5446:
	      pCir->chip.alp->BLTBase = pCir->IOBase + 0x100;
	      break;
	  default:
	      pCir->chip.alp->BLTBase = pCir->IOBase;
	      break;
	}

	AlpAccelEngineInit(pScrn);
	
	pCir->AccelInfoRec = XAAPtr;

	if (!XAAInit(pScreen, XAAPtr))
	    return FALSE;

	return TRUE;
}





