/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_hwcurs.c,v 1.9 1999/03/14 03:22:00 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "mga_bios.h"
#include "mga.h"
#include "mga_reg.h"

Bool 
MGAHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    MGAPtr pMga = MGAPTR(pScrn);
    MGARamdacPtr MGAdac = &pMga->Dac;
    xf86CursorInfoPtr infoPtr;

    if (!MGAdac->isHwCursor) 
        return FALSE;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pMga->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = MGAdac->CursorMaxWidth;
    infoPtr->MaxHeight = MGAdac->CursorMaxHeight;
    infoPtr->Flags = MGAdac->CursorFlags;
    infoPtr->SetCursorColors = MGAdac->SetCursorColors;
    infoPtr->SetCursorPosition = MGAdac->SetCursorPosition;
    infoPtr->LoadCursorImage = MGAdac->LoadCursorImage;
    infoPtr->HideCursor = MGAdac->HideCursor;
    infoPtr->ShowCursor = MGAdac->ShowCursor;
    infoPtr->UseHWCursor = MGAdac->UseHWCursor;

    return(xf86InitCursor(pScreen, infoPtr));
}
