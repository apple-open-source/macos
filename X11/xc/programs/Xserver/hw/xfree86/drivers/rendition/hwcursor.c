/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/hwcursor.c,v 1.8 2002/04/04 14:05:45 eich Exp $ */
/*
 * includes
 */

#include "rendition.h"
#include "vtypes.h"
#include "vramdac.h"

#include "hwcursor.h"

/*
 * defines 
 */

#undef DEBUG

/* use a 64x64 cursor, 32x32 otherwise    */
/* note that V2K supports only 64x64 size */
#define BIGCURSOR 1

/*
 * local function prototypes
 */

static Bool RENDITIONUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs);
static void RENDITIONSetCursorColors(ScrnInfoPtr pScreenInfo, int bg, int fg);
static void RENDITIONSetCursorPosition(ScrnInfoPtr pScreenInfo, int x, int y);
static void RENDITIONHideCursor(ScrnInfoPtr pScreenInfo);
static void RENDITIONShowCursor(ScrnInfoPtr pScreenInfo);
static void RENDITIONLoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char* src);


/*
 * This is top-level initialization funtion
 */
void
RenditionHWCursorPreInit (ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#ifdef DEBUG
    ErrorF ("Rendition: Debug RenditionHWCursorPreInit called\n");
#endif

    pRendition->board.hwcursor_used = TRUE;
    if (pRendition->board.chip==V1000_DEVICE){
      /* V1K uses special space on BT-485 RAMDAC */
      pRendition->board.hwcursor_vmemsize = 0;
      pRendition->board.hwcursor_membase = 0 ; /* Not used on V1K */
    }
    else{
      pRendition->board.hwcursor_vmemsize = 64*64*2/8 /* 1024 bytes used */;
      pRendition->board.hwcursor_membase = (pRendition->board.fbOffset >> 10);
      /* Last but not least, update offset-adress */
      pRendition->board.fbOffset += pRendition->board.hwcursor_vmemsize;
    }
}

void
RenditionHWCursorRelease (ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#ifdef DEBUG
    ErrorF ("Rendition: Debug RenditionHWCursorRelease called\n");
#endif

    xf86DestroyCursorInfoRec(pRendition->CursorInfoRec);
    pRendition->CursorInfoRec=NULL;
}


Bool
RenditionHWCursorInit(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr  pScreenInfo = xf86Screens[scrnIndex];
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    xf86CursorInfoPtr infoPtr;

#ifdef DEBUG
    ErrorF ("Rendition: Debug RenditionHWCursorInit called\n");
#endif

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;

    pRendition->CursorInfoRec = infoPtr;

#ifdef BIGCURSOR
    infoPtr->MaxWidth=64;
    infoPtr->MaxHeight=64;
#else
    infoPtr->MaxWidth=32;
    infoPtr->MaxHeight=32;
#endif

    infoPtr->Flags = HARDWARE_CURSOR_BIT_ORDER_MSBFIRST  |
	HARDWARE_CURSOR_TRUECOLOR_AT_8BPP   | 
	HARDWARE_CURSOR_AND_SOURCE_WITH_MASK|
	HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_8;


    infoPtr->SetCursorColors      = RENDITIONSetCursorColors;
    infoPtr->SetCursorPosition    = RENDITIONSetCursorPosition;
    infoPtr->LoadCursorImage      = RENDITIONLoadCursorImage;
    infoPtr->HideCursor           = RENDITIONHideCursor;
    infoPtr->ShowCursor           = RENDITIONShowCursor;
    infoPtr->UseHWCursor          = RENDITIONUseHWCursor;

    return xf86InitCursor(pScreen, infoPtr);
}


/*
 * local functions
 */

static Bool
RENDITIONUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
#ifdef DEBUG
    ErrorF ("Rendition: Debug RENDITIONUseHWCursor called\n");
#endif

  /* have this return false for DoubleScan and Interlaced ? */
    return TRUE;
}


static void
RENDITIONShowCursor(ScrnInfoPtr pScreenInfo)
{
  /* renditionPtr pRendition = RENDITIONPTR(pScreenInfo); */

#ifdef DEBUG
    ErrorF( "RENDITION: ShowCursor called\n");
#endif

    /* enable cursor - X11 mode */
    verite_enablecursor(pScreenInfo, VERITE_3COLORS,
#ifdef BIGCURSOR
        VERITE_CURSOR64
#else 
        VERITE_CURSOR32
#endif
        );
}



static void
RENDITIONHideCursor(ScrnInfoPtr pScreenInfo)
{
#ifdef DEBUG
    ErrorF( "RENDITION: HideCursor called\n");
#endif

    /* Disable cursor */
    verite_enablecursor(pScreenInfo, VERITE_NOCURSOR, 0);
}



static void
RENDITIONSetCursorPosition(ScrnInfoPtr pScreenInfo, int x, int y)
{
#ifdef DEBUG
    ErrorF( "RENDITION: SetCursorPosition(%d, %d) called\n", x, y);
#endif

    verite_movecursor(pScreenInfo, x, y, 1 /* xorigin */, 1 /* yorigin */);
}



static void
RENDITIONSetCursorColors(ScrnInfoPtr pScreenInfo, int bg, int fg)
{
#ifdef DEBUG
    ErrorF( "RENDITION: SetCursorColors(%x, %x) called\n", fg, bg);
#endif

    verite_setcursorcolor(pScreenInfo, bg, fg);
}



static void
RENDITIONLoadCursorImage(ScrnInfoPtr pScreenInfo, unsigned char* src)
{
#ifdef DEBUG
    ErrorF( "RENDITION: loadcursor called\n");
#endif
    verite_loadcursor(pScreenInfo,
#ifdef BIGCURSOR
        VERITE_CURSOR64, 
#else
        VERITE_CURSOR32, 
#endif
        (vu8 *)src);
}


/*
 * end of file hwcursor.c
 */
