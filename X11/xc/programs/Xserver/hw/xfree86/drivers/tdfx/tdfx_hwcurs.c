/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tdfx/tdfx_hwcurs.c,v 1.5 2001/10/28 03:33:50 tsi Exp $ */
/*
   Voodoo Banshee driver version 1.0.2

   Author: Daryll Strauss

   Copyright: 1998,1999
*/

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86fbman.h"

#include "tdfx.h"

static void TDFXLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void TDFXShowCursor(ScrnInfoPtr pScrn);
static void TDFXHideCursor(ScrnInfoPtr pScrn);
static void TDFXSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void TDFXSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);
static Bool TDFXUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs);

Bool
TDFXCursorInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;
  xf86CursorInfoPtr infoPtr;

  TDFXTRACECURS("TDFXCursorInit start\n");
  pScrn = xf86Screens[pScreen->myNum];
  pTDFX = TDFXPTR(pScrn);
  pTDFX->CursorInfoRec = infoPtr = xf86CreateCursorInfoRec();
  if (!infoPtr) return FALSE;

  infoPtr->MaxWidth = 64;
  infoPtr->MaxHeight = 64;

  infoPtr->Flags = 
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
#endif
    HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK|
    HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
    HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;
  infoPtr->SetCursorColors = TDFXSetCursorColors;
  infoPtr->SetCursorPosition = TDFXSetCursorPosition;
  infoPtr->LoadCursorImage = TDFXLoadCursorImage;
  infoPtr->HideCursor = TDFXHideCursor;
  infoPtr->ShowCursor = TDFXShowCursor;
  infoPtr->UseHWCursor = TDFXUseHWCursor;

  pTDFX->ModeReg.cursloc = pTDFX->cursorOffset;
  pTDFX->writeLong(pTDFX, HWCURPATADDR, pTDFX->cursorOffset);

  return xf86InitCursor(pScreen, infoPtr);
}

#if X_BYTE_ORDER == X_BIG_ENDIAN
static unsigned int TDFXSwap(TDFXPtr pTDFX, unsigned int val) {
  switch (pTDFX->cpp) {
  default:
    return val;
  case 2:
    return ((val & 0x00ff00ff) << 8) |
	    ((val & 0xff00ff00) >> 8);
  case 3:
  case 4:
    return BE_BSWAP32(val);
  }
}
#endif

static void 
TDFXLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
  TDFXPtr pTDFX;

  TDFXTRACECURS("TDFXLoadCursorImage start\n");
  pTDFX = TDFXPTR(pScrn);
#if X_BYTE_ORDER == X_BIG_ENDIAN
  {
    int i;
    unsigned int *ptr=(unsigned int *)(pTDFX->FbBase+pTDFX->cursorOffset);

    for (i=0; i<256; i++) {
      unsigned int val = ((unsigned int *)src)[i];
      *ptr++ = TDFXSwap(pTDFX, val);
    }
  }
#else
  memcpy(pTDFX->FbBase+pTDFX->cursorOffset, src, 1024);
#endif
}

static void
TDFXShowCursor(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  TDFXTRACECURS("TDFXShowCursor start\n");
  pTDFX = TDFXPTR(pScrn);
  pTDFX->ModeReg.vidcfg|=BIT(27);
  pTDFX->writeLong(pTDFX, VIDPROCCFG, pTDFX->ModeReg.vidcfg);
}

void
TDFXHideCursor(ScrnInfoPtr pScrn)
{
  TDFXPtr pTDFX;

  TDFXTRACECURS("TDFXHideCursor start\n");
  pTDFX = TDFXPTR(pScrn);
  pTDFX->ModeReg.vidcfg&=~BIT(27);
  pTDFX->writeLong(pTDFX, VIDPROCCFG, pTDFX->ModeReg.vidcfg);
}

static void
TDFXSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
  TDFXPtr pTDFX;

  /* TDFXTRACECURS("TDFXSetCursorPosition start\n"); */
  pTDFX = TDFXPTR(pScrn);
  pTDFX->writeLong(pTDFX, HWCURLOC, ((y+64)<<16)|(x+64));
}

static void
TDFXSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
  TDFXPtr pTDFX;

  TDFXTRACECURS("TDFXSetCursorColors start\n");
  pTDFX = TDFXPTR(pScrn);
  pTDFX->writeLong(pTDFX, HWCURC0, bg);
  pTDFX->writeLong(pTDFX, HWCURC1, fg);
}

static Bool
TDFXUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs) 
{
  ScrnInfoPtr pScrn;
  TDFXPtr pTDFX;

  TDFXTRACECURS("TDFXUseHWCursor start\n");
  pScrn = xf86Screens[pScreen->myNum];
  pTDFX = TDFXPTR(pScrn);
  if (pScrn->currentMode->Flags&V_DBLSCAN)
    return FALSE;
  if (!pTDFX->CursorInfoRec) return FALSE;
  return TRUE;
}
