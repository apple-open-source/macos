/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbscrinit.c,v 1.4 1999/08/14 10:50:16 dawes Exp $ */


#include "X.h"
#include "Xmd.h"
#include "misc.h"
#include "servermd.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "resource.h"
#include "colormap.h"
#include "colormapst.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "mi.h"
#include "micmap.h"
#include "mistruct.h"
#include "dix.h"
#include "mibstore.h"

/* CAUTION:  We require that cfb24 and cfb32 were NOT 
	compiled with CFB_NEED_SCREEN_PRIVATE */

static BSFuncRec cfb24_32BSFuncRec = {
    cfb24SaveAreas,
    cfb24RestoreAreas,
    (BackingStoreSetClipmaskRgnProcPtr) 0,
    (BackingStoreGetImagePixmapProcPtr) 0,
    (BackingStoreGetSpansPixmapProcPtr) 0,
};


int cfb24_32GCIndex = 1;
int cfb24_32PixmapIndex = 1;

static unsigned long cfb24_32Generation = 0;
extern WindowPtr *WindowTable;

static Bool
cfb24_32AllocatePrivates(ScreenPtr pScreen)
{
   if(cfb24_32Generation != serverGeneration) {
	if( ((cfb24_32GCIndex = AllocateGCPrivateIndex()) < 0) |
	    ((cfb24_32PixmapIndex = AllocatePixmapPrivateIndex()) < 0))
	    return FALSE;
	cfb24_32Generation = serverGeneration;
   }
   
   
   /* All cfb will have the same GC and Window private indicies */
   if(!mfbAllocatePrivates(pScreen,&cfbWindowPrivateIndex, &cfbGCPrivateIndex))
	return FALSE;

   /* The cfb indicies are the mfb indicies. Reallocating them resizes them */ 
   if(!AllocateWindowPrivate(pScreen,cfbWindowPrivateIndex,sizeof(cfbPrivWin)))
	return FALSE;

   if(!AllocateGCPrivate(pScreen, cfbGCPrivateIndex, sizeof(cfbPrivGC)))
        return FALSE;

   if(!AllocateGCPrivate(pScreen, cfb24_32GCIndex, sizeof(cfb24_32GCRec)))
        return FALSE;

   if(!AllocatePixmapPrivate(
	pScreen, cfb24_32PixmapIndex, sizeof(cfb24_32PixmapRec)))
        return FALSE;


   return TRUE;
}

static Bool
cfb24_32SetupScreen(
    ScreenPtr pScreen,
    pointer pbits,		/* pointer to screen bitmap */
    int xsize, int ysize,	/* in pixels */
    int dpix, int dpiy,		/* dots per inch */
    int width			/* pixel width of frame buffer */
){
    if (!cfb24_32AllocatePrivates(pScreen))
	return FALSE;
    pScreen->defColormap = FakeClientID(0);
    /* let CreateDefColormap do whatever it wants for pixels */ 
    pScreen->blackPixel = pScreen->whitePixel = (Pixel) 0;
    pScreen->QueryBestSize = mfbQueryBestSize;
    /* SaveScreen */
    pScreen->GetImage = cfb24_32GetImage;
    pScreen->GetSpans = cfb24_32GetSpans;
    pScreen->CreateWindow = cfb24_32CreateWindow;
    pScreen->DestroyWindow = cfb24_32DestroyWindow;	
    pScreen->PositionWindow = cfb24_32PositionWindow;
    pScreen->ChangeWindowAttributes = cfb24_32ChangeWindowAttributes;
    pScreen->RealizeWindow = cfb24MapWindow;			/* OK */
    pScreen->UnrealizeWindow = cfb24UnmapWindow;		/* OK */
    pScreen->PaintWindowBackground = cfb24PaintWindow;		/* OK */
    pScreen->PaintWindowBorder = cfb24PaintWindow;		/* OK */
    pScreen->CopyWindow = cfb24_32CopyWindow;
    pScreen->CreatePixmap = cfb24_32CreatePixmap;
    pScreen->DestroyPixmap = cfb24_32DestroyPixmap;
    pScreen->RealizeFont = mfbRealizeFont;
    pScreen->UnrealizeFont = mfbUnrealizeFont;
    pScreen->CreateGC = cfb24_32CreateGC;
    pScreen->CreateColormap = miInitializeColormap;
    pScreen->DestroyColormap = (void (*)())NoopDDA;
    pScreen->InstallColormap = miInstallColormap;
    pScreen->UninstallColormap = miUninstallColormap;
    pScreen->ListInstalledColormaps = miListInstalledColormaps;
    pScreen->StoreColors = (void (*)())NoopDDA;
    pScreen->ResolveColor = miResolveColor;
    pScreen->BitmapToRegion = mfbPixmapToRegion;

    mfbRegisterCopyPlaneProc (pScreen, miCopyPlane);
    return TRUE;
}

typedef struct {
    pointer pbits; 
    int width;   
} miScreenInitParmsRec, *miScreenInitParmsPtr;

static Bool
cfb24_32CreateScreenResources(ScreenPtr pScreen)
{
    miScreenInitParmsPtr pScrInitParms;
    int pitch;
    Bool retval;

    /* get the pitch before mi destroys it */
    pScrInitParms = (miScreenInitParmsPtr)pScreen->devPrivate;
    pitch = BitmapBytePad(pScrInitParms->width * 24);

    if((retval = miCreateScreenResources(pScreen))) {
	/* fix the screen pixmap */
	PixmapPtr pPix = (PixmapPtr)pScreen->devPrivate;
	pPix->drawable.bitsPerPixel = 24;
	pPix->devKind = pitch;
    }

    return retval;
}


static Bool
cfb24_32FinishScreenInit(
    ScreenPtr pScreen,
    pointer pbits,		/* pointer to screen bitmap */
    int xsize, int ysize,	/* in pixels */
    int dpix, int dpiy,		/* dots per inch */
    int width			/* pixel width of frame buffer */
){
    VisualPtr	visuals;
    DepthPtr	depths;
    int		nvisuals;
    int		ndepths;
    int		rootdepth;
    VisualID	defaultVisual;

    rootdepth = 0;
    if (!miInitVisuals (&visuals, &depths, &nvisuals, &ndepths, &rootdepth,
			 &defaultVisual,((unsigned long)1<<(24-1)), 8, -1))
	return FALSE;
    if (! miScreenInit(pScreen, pbits, xsize, ysize, dpix, dpiy, width,
			rootdepth, ndepths, depths,
			defaultVisual, nvisuals, visuals))
	return FALSE;

    pScreen->BackingStoreFuncs = cfb24_32BSFuncRec;
    pScreen->CreateScreenResources = cfb24_32CreateScreenResources;
    pScreen->CloseScreen = cfb32CloseScreen;		/* OK */
    pScreen->GetScreenPixmap = cfb32GetScreenPixmap; 	/* OK */
    pScreen->SetScreenPixmap = cfb32SetScreenPixmap;	/* OK */
    return TRUE;
}

Bool
cfb24_32ScreenInit(
    ScreenPtr pScreen,
    pointer pbits,		/* pointer to screen bitmap */
    int xsize, int ysize,	/* in pixels */
    int dpix, int dpiy,		/* dots per inch */
    int width			/* pixel width of frame buffer */
){
    if (!cfb24_32SetupScreen(pScreen, pbits, xsize, ysize, dpix, dpiy, width))
	return FALSE;
    return cfb24_32FinishScreenInit(
		pScreen, pbits, xsize, ysize, dpix, dpiy, width);
}

