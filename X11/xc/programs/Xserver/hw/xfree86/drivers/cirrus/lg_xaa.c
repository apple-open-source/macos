/*
 * XAA acceleration for CL-GD546x -- The Laugna family
 *
 * lg_xaa.c
 *
 * (c) 1998 Corin Anderson.
 *          corina@the4cs.com
 *          Tukwila, WA
 *
 * Much of this code is inspired by the XAA acceleration from XFree86
 * 3.3.3, laguna_acl.c
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cirrus/lg_xaa.c,v 1.5 2001/02/15 17:39:28 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "cir.h"
#define _LG_PRIVATE_
#include "lg.h"
#include "lg_xaa.h"

/* Laguna raster operations, source is OP1 and destination is OP0. */
/* The order in this array is important! */
static int lgRop[16] = {
				/* Lg Op     X name          */

	0x00,		/*     0     GXclear         */
	0x88,		/*   S.D     GXand           */
	0x44,		/*  S.~D     GXandReverse    */
	0xCC,		/*     S     GXcopy          */
	0x22,		/*  ~S.D     GXandInverted   */
	0xAA,		/*     D     GXnoop          */
	0x66,		/*  S~=D     GXxor           */
	0xEE,		/*   S+D     GXor            */
	0x77,		/* ~S.~D     GXnor           */
	0x99,		/*   S=D     GXequiv         */
	0x55,		/*    ~D     GXinvert        */
	0xDD,		/*  S+~D     GXorReverse     */
	0x33,		/*    ~S     GXcopyInverted  */
	0xBB,		/*  ~S+D     GXorInverted    */
	0x11,		/* ~S+~D     GXnand          */
	0xFF		/*     1     GXset           */
};

#if 0
/* Laguna raster operations, source is OP2 and destination is OP0. */
static int lgPatRop[16] = {
				/* Lg Op     X name          */

	0x00,		/*     0     GXclear         */
	0xA0,		/*   S.D     GXand           */
	0x50,		/*  S.~D     GXandReverse    */
	0xF0,		/*     S     GXcopy          */
	0x0A,		/*  ~S.D     GXandInverted   */
	0xAA,		/*     D     GXnoop          */
	0x5A,		/*  S~=D     GXxor           */
	0xFA,		/*   S+D     GXor            */
	0x05,		/* ~S.~D     GXnor           */
	0xA5,		/*   S=D     GXequiv         */
	0x55,		/*    ~D     GXinvert        */
	0xF5,		/*  S+~D     GXorReverse     */
	0x0F,		/*    ~S     GXcopyInverted  */
	0xAF,		/*  ~S+D     GXorInverted    */
	0x5F,		/* ~S+~D     GXnand          */
	0xFF		/*     1     GXset           */
};
#endif


static void LgSetBitmask(CirPtr pCir, const CARD32 m);
static void LgWaitQAvail(CirPtr pCir, int n);
static CARD32 LgExpandColor(CARD32 color, int bpp);
static void LgSync(ScrnInfoPtr pScrn);
static void LgSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
								unsigned int planemask);

static void LgSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
										int w, int h);
static void LgSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
											int rop, unsigned int planemask,
											int transparency_color);
static void LgSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
											int x2, int y2, int w, int h);


/**************************************************** LgXAAInit *****/

Bool
LgXAAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    CirPtr pCir = CIRPTR(pScrn);
    XAAInfoRecPtr XAAPtr;

	XAAPtr = XAACreateInfoRec();
	if (!XAAPtr)
		return FALSE;

	/*
	 * Solid color fills.
	 */
	XAAPtr->SetupForSolidFill = LgSetupForSolidFill;
	XAAPtr->SubsequentSolidFillRect = LgSubsequentSolidFillRect;
	XAAPtr->SubsequentSolidFillTrap = NULL;
	XAAPtr->SolidFillFlags = 0;

	/*
	 * Screen-to-screen copies.
	 */
	XAAPtr->SetupForScreenToScreenCopy = LgSetupForScreenToScreenCopy;
	XAAPtr->SubsequentScreenToScreenCopy = LgSubsequentScreenToScreenCopy;
	/* Maybe ONLY_LEFT_TO_RIGHT_BITBLT or ONLY_TWO_BITBLT_DIRECTIONS? */
	XAAPtr->ScreenToScreenCopyFlags = ONLY_LEFT_TO_RIGHT_BITBLT;

	/*
	 * Miscellany.
	 */
	XAAPtr->Sync = LgSync;

	pCir->AccelInfoRec = XAAPtr;

	if (!XAAInit(pScreen, XAAPtr))
		return FALSE;

	return TRUE;
}

/******************************************** Lg XAA helper functions ***/

/*
 * The bitmask is usually all 1's, so it's silly to spend a DWORD write
 * to program the register with the same value each time.  Bitmask is
 * about the only register whose value is worth shadowing, so we special-
 * case it.
 */
static void
LgSetBitmask(CirPtr pCir, const CARD32 m)
{
	const LgPtr pLg = LGPTR(pCir);

	if (m != pLg->oldBitmask) {
		LgSETBITMASK(m);
		pLg->oldBitmask = m;
	}
}

/*
 * Return from the function only when there's room somewhere for the
 * upcoming register writes.  That means that either PCI retry is enabled
 * (i.e., we let the PCI bus buffer the register writes), or we wait for
 * room in the Laguna's command queue explicitly.
 */
static void
LgWaitQAvail(CirPtr pCir, int n)
{
	if (!0/*lgUsePCIRetry*/) {
		CARD8 qfree;

		/* Wait until n entries are open in the command queue */
		do
			qfree = *(volatile CARD8 *)(pCir->IOBase + QFREE);
		while (qfree < n);
	}
}


/* We might want to make this a macro at some point. */
static CARD32
LgExpandColor(CARD32 color, int bpp)
{
	if (8 == bpp)
		color = ((color&0xFF) << 8) | (color&0xFF);

	if (8 == bpp || 16 == bpp)
		color = ((color&0xFFFF) << 16) | (color&0xFFFF);

	return color;
}


/*************************************************** Lg XAA functions ***/


static void
LgSync(ScrnInfoPtr pScrn)
{
	const CirPtr pCir = CIRPTR(pScrn);
#if 0
	LgPtr pLg = LGPTR(pScrn);
#endif

	while (!LgREADY())
		;
}

static void
LgSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
			unsigned int planemask)
{

	const CirPtr pCir = CIRPTR(pScrn);

	color = LgExpandColor(color, pScrn->bitsPerPixel);

	LgWaitQAvail(pCir, 4);

	LgSETBACKGROUND(color);
	LgSETROP(lgRop[rop]);
	LgSETMODE(SCR2SCR | COLORFILL);
	LgSetBitmask(pCir, planemask);
}

static void
LgSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
	const CirPtr pCir = CIRPTR(pScrn);

	/* Wait for room in the command queue. */
	LgWaitQAvail(pCir, 2);

	LgSETDSTXY(x, y);
	LgSETEXTENTS(w, h);
}

static void
LgSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
				int rop, unsigned int planemask, int transparency_color)
{
	int bltmode = 0;
	const CirPtr pCir = CIRPTR(pScrn);
	const LgPtr pLg = LGPTR(pCir);

	pLg->blitTransparent = (transparency_color != -1);
	pLg->blitYDir = ydir;

	LgWaitQAvail(pCir, 4);

	/* We set the rop up here because the LgSETROP macro conveniently
	   (really -- it is convenient!) clears the transparency bits
	   in DRAWDEF.  We'll set those bits appropriatly later. */
	LgSETROP(lgRop[rop]);

	if (ydir < 0)
		bltmode |= BLITUP;
	if (pLg->blitTransparent) {
		/* Gotta extend the transparency_color to the full 32-bit
		   size of the register. */
		transparency_color = LgExpandColor(transparency_color,
										   pScrn->bitsPerPixel);

		bltmode |= COLORTRANS;
		LgSETBACKGROUND(transparency_color);
		LgSETTRANSPARENCY(TRANSEQ);
	} else {
		LgSETTRANSPARENCY(TRANSNONE);
	}

	LgSETMODE(SCR2SCR | COLORSRC | bltmode);
	LgSetBitmask(pCir, planemask);
}

static void
LgSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
								int x2, int y2, int w, int h)
{
	const CirPtr pCir = CIRPTR(pScrn);
	const LgPtr pLg = LGPTR(pCir);

	/*
	 * We have set the flag indicating that xdir must be one,
	 * so we can assume that here.
	 */
	if (pLg->blitYDir == -1) {
		y1 += h - 1;
		y2 += h - 1;
	}

	if (pLg->blitTransparent) {
		/* We're doing a transparent blit.  We'll need to point
		   OP2 to the color compare mask. */
		LgWaitQAvail(pCir, 4);
		LgSETTRANSMASK(x1, y1);
	} else {
		LgWaitQAvail(pCir, 3);
	}
	LgSETSRCXY(x1, y1);
	LgSETDSTXY(x2, y2);
	LgSETEXTENTS(w, h);
}

