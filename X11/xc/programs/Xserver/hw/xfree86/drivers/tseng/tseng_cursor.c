/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_cursor.c,v 1.17 2001/05/07 21:59:07 tsi Exp $ */





#include "tseng.h"

static void TsengShowCursor(ScrnInfoPtr pScrn);
static void TsengHideCursor(ScrnInfoPtr pScrn);
static void TsengSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static Bool TsengUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs);
static void TsengSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);
static void TsengLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits);

Bool 
TsengHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    PDEBUG("	TsengHWCursorInit\n");

    if (!pTseng->HWCursor)
	return FALSE;

    infoPtr = xf86CreateCursorInfoRec();
    if (!infoPtr)
	return FALSE;

    pTseng->CursorInfoRec = infoPtr;

    /* calculate memory addres from video memory offsets */
    pTseng->HWCursorBuffer =
	pTseng->FbBase + pTseng->HWCursorBufferOffset;

    /*
     * for banked memory, translate this address to fall in the
     * correct aperture. HWcursor uses aperture #0, which sits at
     * pTseng->FbBase + 0x18000.
     */
    if (!pTseng->UseLinMem) {
#ifdef TODO
	pTseng->HWCursorBuffer =
	    pTseng->something
	    - pTseng->what
	    + 0x18000;
#else
	ErrorF("banked HW cursor not implemented yet!\n");
#endif
    }

    /* set up the XAA HW cursor structure */
    infoPtr->MaxWidth = 64;
    infoPtr->MaxHeight = 64;
    infoPtr->Flags =
	HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
	HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1 |
	HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
	HARDWARE_CURSOR_INVERT_MASK;
    infoPtr->SetCursorColors = TsengSetCursorColors;
    infoPtr->SetCursorPosition = TsengSetCursorPosition;
    infoPtr->LoadCursorImage = TsengLoadCursorImage;
    infoPtr->HideCursor = TsengHideCursor;
    infoPtr->ShowCursor = TsengShowCursor;
    infoPtr->UseHWCursor = TsengUseHWCursor;

    return (xf86InitCursor(pScreen, infoPtr));
}

static Bool
TsengUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    /* have this return false for DoubleScan and Interlaced ? */
    return TRUE;
}

static void
TsengShowCursor(ScrnInfoPtr pScrn)
{
    unsigned char tmp;
    TsengPtr pTseng = TsengPTR(pScrn);

    /* Enable the hardware cursor. */
    if (Is_ET6K) {
	tmp = inb(pTseng->IOAddress + 0x46);
	outb(pTseng->IOAddress + 0x46, (tmp | 0x01));
    } else {
	outb(0x217A, 0xF7);
	tmp = inb(0x217B);
	outb(0x217B, tmp | 0x80);
    }
}

static void
TsengHideCursor(ScrnInfoPtr pScrn)
{
    unsigned char tmp;
    TsengPtr pTseng = TsengPTR(pScrn);

    /* Disable the hardware cursor. */
    if (Is_ET6K) {
	tmp = inb(pTseng->IOAddress + 0x46);
	outb(pTseng->IOAddress + 0x46, (tmp & 0xfe));;
    } else {
	outb(0x217A, 0xF7);
	tmp = inb(0x217B);
	outb(0x217B, tmp & ~0x80);
    }
}

static void
TsengSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    int xorigin, yorigin;
    TsengPtr pTseng = TsengPTR(pScrn);

    /*
     * If the cursor is partly out of screen at the left or top,
     * we need to modify the origin.
     */
    xorigin = 0;
    yorigin = 0;
    if (x < 0) {
	xorigin = -x;
	x = 0;
    }
    if (y < 0) {
	yorigin = -y;
	y = 0;
    }
#ifdef TODO
    /* Correct cursor position in DoubleScan modes */
    if (XF86SCRNINFO(pScr)->modes->Flags & V_DBLSCAN)
	y *= 2;
#endif

    if (Is_ET6K) {
	outb(pTseng->IOAddress + 0x82, xorigin);
	outb(pTseng->IOAddress + 0x83, yorigin);

	outb(pTseng->IOAddress + 0x84, (x & 0xff));	/* X bits 7-0 */
	outb(pTseng->IOAddress + 0x85, ((x >> 8) & 0x0f));	/* X bits 11-8 */

	outb(pTseng->IOAddress + 0x86, (y & 0xff));	/* Y bits 7-0 */
	outb(pTseng->IOAddress + 0x87, ((y >> 8) & 0x0f));	/* Y bits 11-8 */
    } else {
	outb(0x217A, 0xE2);
	outb(0x217B, xorigin);
	outb(0x217A, 0xE6);
	outb(0x217B, yorigin);

	outb(0x217A, 0xE0);
	outb(0x217B, (x & 0xff));      /* X bits 7-0 */
	outb(0x217A, 0xE1);
	outb(0x217B, ((x >> 8) & 0x0f));	/* X bits 10-8 */

	outb(0x217A, 0xE4);
	outb(0x217B, (y & 0xff));      /* Y bits 7-0 */
	outb(0x217A, 0xE5);
	outb(0x217B, ((y >> 8) & 0x0f));	/* Y bits 10-8 */
    }
}

/*
 * The ET6000 cursor color is only 6 bits, with 2 bits per color. This
 * is of course very inaccurate, but high-bit-depth color differences
 * are only visible on _large_ planes of equal color. i.e. small areas
 * of a certain color (like a cursor) don't need many bits per pixel at
 * all, because the difference will not be seen.
 * 
 * So it won't be as bad, but should still be documented nonetheless.
 */
static void
TsengSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    unsigned char et6k_fg, et6k_bg;

    if (Is_ET6K) {
	et6k_fg = (fg & 0x00000003)
	    | ((fg & 0x00000300) >> 6)
	    | ((fg & 0x00030000) >> 12);
	et6k_bg = (bg & 0x00000003)
	    | ((bg & 0x00000300) >> 6)
	    | ((bg & 0x00030000) >> 12);

	outb(pTseng->IOAddress + 0x67, 0x09);	/* prepare for colour data */
	outb(pTseng->IOAddress + 0x69, et6k_bg);
	outb(pTseng->IOAddress + 0x69, et6k_fg);
    } else {
	/*
	 * The ET4000 uses color 0 as sprite color "0", and color 0xFF as
	 * sprite color "1". Changing colors implies changing colors 0 and
	 * 255. This is currently not implemented.
	 *
	 * In non-8bpp modes, this would result in always black and white
	 * colors (since the colormap isn't there to translate 0 and 255 to
	 * other colors). And besides, in non-8bpp, there seem to be TWO
	 * cursor images on the screen...
	 */
	xf86Msg(X_ERROR, "Internal error: ET4000 hardware cursor color changes not implemented\n");
    }
}

void 
TsengLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
    TsengPtr pTseng = TsengPTR(pScrn);

    int iobase = VGAHW_GET_IOBASE();
    unsigned char tmp;
#ifdef DEBUG_HWC
    int i;
    int d;

    for (i = 0; i < 1024; i++) {
	d = *(bits + i);
	ErrorF("%d%d%d%d", d & 0x03, (d >> 2) & 0x03, (d >> 4) & 0x03, (d >> 6) & 0x03);
	if ((i & 15) == 15)
	    ErrorF("\n");
    }
#endif
    /*
     * Program the cursor image address in video memory. 
     * We need to set it here or we might loose it on mode/vt switches.
     */

    if (Is_ET6K) {
	/* bits 19:16 */
	outb(iobase + 0x04, 0x0E);
	tmp = inb(iobase + 0x05) & 0xF0;
	outb(iobase + 0x05, tmp | (((pTseng->HWCursorBufferOffset / 4) >> 16) & 0x0F));
	/* bits 15:8 */
	outb(iobase + 0x04, 0x0F);
	outb(iobase + 0x05, ((pTseng->HWCursorBufferOffset / 4) >> 8) & 0xFF);
	/* on the ET6000, bits (7:0) are always 0 */
    } else {
	/* bits 19:16 */
	outb(0x217A, 0xEA);
	tmp = inb(0x217B) & 0xF0;
	outb(0x217B, tmp | (((pTseng->HWCursorBufferOffset / 4) >> 16) & 0x0F));
	/* bits 15:8 */
	outb(0x217A, 0xE9);
	outb(0x217B, ((pTseng->HWCursorBufferOffset / 4) >> 8) & 0xFF);
	/* bits 7:0 */
	outb(0x217A, 0xE8);
	outb(0x217B, (pTseng->HWCursorBufferOffset / 4) & 0xFF);

	/* this needs to be set for the sprite */
	outb(0x217A, 0xEB);
	outb(0x217B, 2);
	outb(0x217A, 0xEC);
	tmp = inb(0x217B);
	outb(0x217B, tmp & 0xFE);
	outb(0x217A, 0xEF);
	tmp = inb(0x217B);
	outb(0x217B, (tmp & 0xF8) | 0x02);
	outb(0x217A, 0xEE);
	outb(0x217B, 1);
    }
    /* this assumes the apertures have been set up correctly for banked mode */
    memcpy(pTseng->HWCursorBuffer, bits, 1024);
}





