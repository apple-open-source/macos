/*
 * newport_cursor.c
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/newport/newport_cursor.c,v 1.1 2002/12/10 04:03:00 dawes Exp $ */

#include "newport.h"
#include "cursorstr.h"

#include "servermd.h"

#define MAX_CURS 32

static void NewportShowCursor(ScrnInfoPtr pScrn);
static void NewportHideCursor(ScrnInfoPtr pScrn);
static void NewportSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void NewportSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);
/* static void NewportLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits); */
static unsigned char* NewportRealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs);

Bool
NewportHWCursorInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);
	xf86CursorInfoPtr infoPtr;
	CARD16 tmp;

	infoPtr = xf86CreateCursorInfoRec();
	if(!infoPtr)
		return FALSE;

	pNewport->CursorInfoRec = infoPtr;
	infoPtr->MaxWidth = MAX_CURS;
	infoPtr->MaxHeight = MAX_CURS;
	infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

	infoPtr->SetCursorColors = NewportSetCursorColors;
	infoPtr->SetCursorPosition = NewportSetCursorPosition;
	infoPtr->LoadCursorImage = NewportLoadCursorImage;
	infoPtr->HideCursor = NewportHideCursor;
	infoPtr->ShowCursor = NewportShowCursor;
	infoPtr->RealizeCursor = NewportRealizeCursor;
	infoPtr->UseHWCursor = NULL;

	/* enable cursor funtion in shadow register */
	pNewport->vc2ctrl |= VC2_CTRL_ECURS;
	/* enable glyph cursor, maximum size is 32x32x2 */
	pNewport->vc2ctrl &= ~( VC2_CTRL_ECG64 | VC2_CTRL_ECCURS);
	/* setup hw cursors cmap base address  */
	NewportBfwait(pNewportRegs);
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | R_DCB_XMAP9_PROTOCOL |
			XM9_CRS_CURS_CMAP_MSB | NPORT_DMODE_W1 );
	tmp = pNewportRegs->set.dcbdata0.bytes.b3;
#if 0
	/* The docs say we can change base address of the cursors
	 * cmap entries, but it doesn't work. */
	tmp++;
#endif
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | W_DCB_XMAP9_PROTOCOL |
			XM9_CRS_CURS_CMAP_MSB | NPORT_DMODE_W1 );
	pNewportRegs->set.dcbdata0.bytes.b3 = tmp;
	pNewport->curs_cmap_base = (tmp << 5) & 0xffe0;

	return xf86InitCursor(pScreen, infoPtr);
}


static void NewportShowCursor(ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	pNewport->vc2ctrl |= VC2_CTRL_ECDISP;
	NewportVc2Set( pNewportRegs, VC2_IREG_CONTROL, pNewport->vc2ctrl);
}

static void NewportHideCursor(ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	pNewport->vc2ctrl &= ~VC2_CTRL_ECDISP;
	NewportVc2Set( pNewportRegs, VC2_IREG_CONTROL, pNewport->vc2ctrl);
}

static void NewportSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
	CARD16 x_off;
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);
	NewportPtr pNewport = NEWPORTPTR(pScrn);
		
	/* the docu says this should always be 31, but it isn't */
	x_off = 31;
	if ( pNewport->board_rev < 6 )
		x_off = 21;
	NewportVc2Set( pNewportRegs, VC2_IREG_CURSX, (CARD16) x + x_off);
	NewportVc2Set( pNewportRegs, VC2_IREG_CURSY, (CARD16) y + 31);
}

static void NewportSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);
	LOCO color;

	color.blue = bg & 0xff;
	color.green = (bg & 0xff00) >> 8;
	color.red = (bg & 0xff0000) >> 16;
	NewportCmapSetRGB( pNewportRegs, pNewport->curs_cmap_base+2, color);

	color.blue = fg & 0xff;
	color.green = (fg & 0xff00) >> 8;
	color.red = (fg & 0xff0000) >> 16;
	NewportCmapSetRGB( pNewportRegs, pNewport->curs_cmap_base+1, color);
}

static unsigned char* NewportRealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs)
{
	int size = (infoPtr->MaxWidth * infoPtr->MaxHeight) >> 2;
	CARD32 *mem, *SrcS, *SrcM, *DstS;
	unsigned int i;

	if (!(mem = xcalloc(1, size)))
        	return NULL;

	SrcS = (CARD32*)pCurs->bits->source;
    	SrcM = (CARD32*)pCurs->bits->mask;
    	DstS = mem;
	/* first color: maximum is 32*4 Bytes */
	for(i=0; i < pCurs->bits->height; i++) {
		*DstS = *SrcS & *SrcM;
		DstS++, SrcS++, SrcM++;
	}
	/* second color is the lower of mem: again 32*4 Bytes at most */
	DstS = mem + MAX_CURS;
	SrcS = (CARD32*)pCurs->bits->source;
    	SrcM = (CARD32*)pCurs->bits->mask;
	for(i=0; i < pCurs->bits->height; i++) {
		*DstS = (~*SrcS) & *SrcM;
		DstS++, SrcS++, SrcM++;
	}
	return (unsigned char*) mem;
}

void NewportLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
	int i;
	CARD16 tmp;
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	/* address of cursor data in vc2's ram */
        tmp = NewportVc2Get( pNewportRegs, VC2_IREG_CENTRY);
 	/* this is where we want to write to: */
        NewportVc2Set( pNewportRegs, VC2_IREG_RADDR, tmp);
        pNewportRegs->set.dcbmode = (NPORT_DMODE_AVC2 | VC2_REGADDR_RAM |
                               		NPORT_DMODE_W2 | VC2_PROTOCOL);
	/* write cursor data */
        for (i = 0; i < ((MAX_CURS * MAX_CURS) >> 3); i++) {
		NewportBfwait(pNewportRegs);
		pNewportRegs->set.dcbdata0.hwords.s1 = *(unsigned short*)bits;
		bits += sizeof(unsigned short);
        }
}

