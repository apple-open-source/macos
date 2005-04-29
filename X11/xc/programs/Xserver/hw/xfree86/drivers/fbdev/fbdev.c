/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/fbdev/fbdev.c,v 1.45 2004/01/11 18:42:59 dawes Exp $ */

/*
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *	     Michel Dänzer, <michdaen@iiic.ethz.ch>
 */

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "shadow.h"
#include "dgaproc.h"

/* for visuals */
#include "fb.h"
#ifdef USE_AFB
#include "afb.h"
#endif

#include "xf86Resources.h"
#include "xf86RAC.h"

#include "fbdevhw.h"

#include "xf86xv.h"

#define DEBUG 0

#if DEBUG
# define TRACE_ENTER(str)       ErrorF("fbdev: " str " %d\n",pScrn->scrnIndex)
# define TRACE_EXIT(str)        ErrorF("fbdev: " str " done\n")
# define TRACE(str)             ErrorF("fbdev trace: " str "\n")
#else
# define TRACE_ENTER(str)
# define TRACE_EXIT(str)
# define TRACE(str)
#endif

/* -------------------------------------------------------------------- */
/* prototypes                                                           */

static const OptionInfoRec * FBDevAvailableOptions(int chipid, int busid);
static void	FBDevIdentify(int flags);
static Bool	FBDevProbe(DriverPtr drv, int flags);
static Bool	FBDevPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	FBDevScreenInit(int Index, ScreenPtr pScreen, int argc,
				char **argv);
static Bool	FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen);
static void *	FBDevWindowLinear(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
				  CARD32 *size, void *closure);
static void	FBDevPointerMoved(int index, int x, int y);
static Bool	FBDevDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen);


enum { FBDEV_ROTATE_NONE=0, FBDEV_ROTATE_CW=270, FBDEV_ROTATE_UD=180, FBDEV_ROTATE_CCW=90 };

/*static ShadowUpdateProc updateFuncs[] =
  { shadowUpdatePacked, shadowUpdateRotate8_270, shadowUpdateRotate8_180, shadowUpdateRotate8_90,
    shadowUpdatePacked, shadowUpdateRotate16_270, shadowUpdateRotate16_180, shadowUpdateRotate16_90,
    shadowUpdatePacked, shadowUpdateRotate32_270, shadowUpdateRotate32_180, shadowUpdateRotate32_90 }; */


/* -------------------------------------------------------------------- */

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

#define VERSION			4000
#define FBDEV_NAME		"FBDEV"
#define FBDEV_DRIVER_NAME	"fbdev"
#define FBDEV_MAJOR_VERSION	0
#define FBDEV_MINOR_VERSION	1

DriverRec FBDEV = {
	VERSION,
	FBDEV_DRIVER_NAME,
#if 0
	"driver for linux framebuffer devices",
#endif
	FBDevIdentify,
	FBDevProbe,
	FBDevAvailableOptions,
	NULL,
	0
};

/* Supported "chipsets" */
static SymTabRec FBDevChipsets[] = {
    { 0, "fbdev" },
#ifdef USE_AFB
    { 0, "afb" },
#endif
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_SHADOW_FB,
	OPTION_ROTATE,
	OPTION_FBDEV
} FBDevOpts;

static const OptionInfoRec FBDevOptions[] = {
	{ OPTION_SHADOW_FB,	"ShadowFB",	OPTV_BOOLEAN,	{0},	FALSE },
	{ OPTION_ROTATE,	"Rotate",	OPTV_STRING,	{0},	FALSE },
	{ OPTION_FBDEV,		"fbdev",	OPTV_STRING,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

static const char *afbSymbols[] = {
	"afbScreenInit",
	"afbCreateDefColormap",
	NULL
};

static const char *fbSymbols[] = {
	"fbScreenInit",
	"fbPictureInit",
	NULL
};

static const char *shadowSymbols[] = {
	"shadowAdd",
	"shadowAlloc",
	"shadowInit",
	"shadowSetup",
	"shadowUpdatePacked",
	"shadowUpdateRotatePacked",
	NULL
};

static const char *fbdevHWSymbols[] = {
	"fbdevHWInit",
	"fbdevHWProbe",
	"fbdevHWSetVideoModes",
	"fbdevHWUseBuildinMode",

	"fbdevHWGetDepth",
	"fbdevHWGetLineLength",
	"fbdevHWGetName",
	"fbdevHWGetType",
	"fbdevHWGetVidmem",
	"fbdevHWLinearOffset",
	"fbdevHWLoadPalette",
	"fbdevHWMapVidmem",
	"fbdevHWUnmapVidmem",

	/* colormap */
	"fbdevHWLoadpalette",

	/* ScrnInfo hooks */
	"fbdevHWAdjustFrame",
	"fbdevHWEnterVT",
	"fbdevHWLeaveVT",
	"fbdevHWModeInit",
	"fbdevHWRestore",
	"fbdevHWSave",
	"fbdevHWSaveScreen",
	"fbdevHWSwitchMode",
	"fbdevHWValidMode",

	"fbdevHWDPMSSet",

	NULL
};

#ifdef XFree86LOADER

MODULESETUPPROTO(FBDevSetup);

static XF86ModuleVersionInfo FBDevVersRec =
{
	"fbdev",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	FBDEV_MAJOR_VERSION, FBDEV_MINOR_VERSION, 0,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	NULL,
	{0,0,0,0}
};

XF86ModuleData fbdevModuleData = { &FBDevVersRec, FBDevSetup, NULL };

pointer
FBDevSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&FBDEV, module, 0);
		LoaderRefSymLists(afbSymbols, fbSymbols,
				  shadowSymbols, fbdevHWSymbols, NULL);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

typedef struct {
	unsigned char*			fbstart;
	unsigned char*			fbmem;
	int				fboff;
	int				lineLength;
	unsigned char*			shadowmem;
	int				rotate;
	Bool				shadowFB;
	CloseScreenProcPtr		CloseScreen;
	void				(*PointerMoved)(int index, int x, int y);
	EntityInfoPtr			pEnt;
	/* DGA info */
	DGAModePtr			pDGAMode;
	int				nDGAMode;
	OptionInfoPtr			Options;
} FBDevRec, *FBDevPtr;

#define FBDEVPTR(p) ((FBDevPtr)((p)->driverPrivate))

static Bool
FBDevGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;
	
	pScrn->driverPrivate = xnfcalloc(sizeof(FBDevRec), 1);
	return TRUE;
}

static void
FBDevFreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
FBDevAvailableOptions(int chipid, int busid)
{
	return FBDevOptions;
}

static void
FBDevIdentify(int flags)
{
	xf86PrintChipsets(FBDEV_NAME, "driver for framebuffer", FBDevChipsets);
}

static Bool
FBDevProbe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
       	GDevPtr *devSections;
	int numDevSections;
	int bus,device,func;
	char *dev;
	Bool foundScreen = FALSE;

	TRACE("probe start");

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(FBDEV_DRIVER_NAME, &devSections)) <= 0) 
	    return FALSE;
	
	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
	    return FALSE;
	    
	xf86LoaderReqSymLists(fbdevHWSymbols, NULL);
	
	for (i = 0; i < numDevSections; i++) {
	    Bool isIsa = FALSE;
	    Bool isPci = FALSE;

	    dev = xf86FindOptionValue(devSections[i]->options,"fbdev");
	    if (devSections[i]->busID) {
	        if (xf86ParsePciBusString(devSections[i]->busID,&bus,&device,
					  &func)) {
		    if (!xf86CheckPciSlot(bus,device,func))
		        continue;
		    isPci = TRUE;
		} else if (xf86ParseIsaBusString(devSections[i]->busID))
		    isIsa = TRUE;
		  
	    }
	    if (fbdevHWProbe(NULL,dev,NULL)) {
		pScrn = NULL;
		if (isPci) {
		    /* XXX what about when there's no busID set? */
		    int entity;
		    
		    entity = xf86ClaimPciSlot(bus,device,func,drv,
					      0,devSections[i],
					      TRUE);
		    pScrn = xf86ConfigPciEntity(pScrn,0,entity,
						      NULL,RES_SHARED_VGA,
						      NULL,NULL,NULL,NULL);
		    /* xf86DrvMsg() can't be called without setting these */
		    pScrn->driverName    = FBDEV_DRIVER_NAME;
		    pScrn->name          = FBDEV_NAME;
		    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			       "claimed PCI slot %d:%d:%d\n",bus,device,func);

		} else if (isIsa) {
		    int entity;
		    
		    entity = xf86ClaimIsaSlot(drv, 0,
					      devSections[i], TRUE);
		    pScrn = xf86ConfigIsaEntity(pScrn,0,entity,
						      NULL,RES_SHARED_VGA,
						      NULL,NULL,NULL,NULL);
		} else {
		   int entity;

		    entity = xf86ClaimFbSlot(drv, 0,
					      devSections[i], TRUE);
		    pScrn = xf86ConfigFbEntity(pScrn,0,entity,
					       NULL,NULL,NULL,NULL);
		   
		}
		if (pScrn) {
		    foundScreen = TRUE;
		    
		    pScrn->driverVersion = VERSION;
		    pScrn->driverName    = FBDEV_DRIVER_NAME;
		    pScrn->name          = FBDEV_NAME;
		    pScrn->Probe         = FBDevProbe;
		    pScrn->PreInit       = FBDevPreInit;
		    pScrn->ScreenInit    = FBDevScreenInit;
		    pScrn->SwitchMode    = fbdevHWSwitchMode;
		    pScrn->AdjustFrame   = fbdevHWAdjustFrame;
		    pScrn->EnterVT       = fbdevHWEnterVT;
		    pScrn->LeaveVT       = fbdevHWLeaveVT;
		    pScrn->ValidMode     = fbdevHWValidMode;
		    
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "using %s\n", dev ? dev : "default device");
		}
	    }
	}
	xfree(devSections);
	TRACE("probe done");
	return foundScreen;
}

static Bool
FBDevPreInit(ScrnInfoPtr pScrn, int flags)
{
	FBDevPtr fPtr;
	int default_depth, fbbpp;
	const char *mod = NULL, *s;
	const char **syms = NULL;
	int type;

	if (flags & PROBE_DETECT) return FALSE;

	TRACE_ENTER("PreInit");

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	FBDevGetRec(pScrn);
	fPtr = FBDEVPTR(pScrn);

	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
	/* XXX Is this right?  Can probably remove RAC_FB */
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

	if (fPtr->pEnt->location.type == BUS_PCI &&
	    xf86RegisterResources(fPtr->pEnt->index,NULL,ResExclusive)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "xf86RegisterResources() found resource conflicts\n");
		return FALSE;
	}

	/* open device */
	if (!fbdevHWInit(pScrn,NULL,xf86FindOptionValue(fPtr->pEnt->device->options,"fbdev")))
		return FALSE;
	default_depth = fbdevHWGetDepth(pScrn,&fbbpp);
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp,
			     Support24bppFb | Support32bppFb | SupportConvert32to24 | PreferConvert32to24))
		return FALSE;
	xf86PrintDepthBpp(pScrn);

	/* Get the depth24 pixmap format */
	if (pScrn->depth == 24 && pix24bpp == 0)
		pix24bpp = xf86GetBppFromDepth(pScrn, 24);

	/* color weight */
	if (pScrn->depth > 8) {
		rgb zeros = { 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zeros, zeros))
			return FALSE;
	}

	/* visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
			   " (%s) is not supported at depth %d\n",
			   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		return FALSE;
	}

	{
		Gamma zeros = {0.0, 0.0, 0.0};

		if (!xf86SetGamma(pScrn,zeros)) {
			return FALSE;
		}
	}

	pScrn->progClock = TRUE;
	pScrn->rgbBits   = 8;
	pScrn->chipset   = "fbdev";
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Hardware: %s (vidmem: %dk)\n",
		   fbdevHWGetName(pScrn),pScrn->videoRam/1024);

	/* handle options */
	xf86CollectOptions(pScrn, NULL);
	if (!(fPtr->Options = xalloc(sizeof(FBDevOptions))))
		return FALSE;
	memcpy(fPtr->Options, FBDevOptions, sizeof(FBDevOptions));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options, fPtr->Options);

	/* use shadow framebuffer by default */
	fPtr->shadowFB = xf86ReturnOptValBool(fPtr->Options, OPTION_SHADOW_FB, TRUE);

	/* rotation */
	fPtr->rotate = FBDEV_ROTATE_NONE;
	if ((s = xf86GetOptValString(fPtr->Options, OPTION_ROTATE)))
	{
	  if(!xf86NameCmp(s, "CW"))
	  {
	    fPtr->shadowFB = TRUE;
	    fPtr->rotate = FBDEV_ROTATE_CW;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Rotating screen clockwise\n");
	  }
	  else if(!xf86NameCmp(s, "CCW"))
	  {
	    fPtr->shadowFB = TRUE;
	    fPtr->rotate = FBDEV_ROTATE_CCW;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Rotating screen counter clockwise\n");
	  }
	  else if(!xf86NameCmp(s, "UD"))
	  {
	    fPtr->shadowFB = TRUE;
	    fPtr->rotate = FBDEV_ROTATE_UD;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Rotating screen upside down\n");
	  }
	  else
	  {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "\"%s\" is not a valid value for Option \"Rotate\"\n", s);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid options are \"CW\", \"CCW\" or \"UD\"\n");
	  }
	}

	/* select video modes */

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Checking Modes against framebuffer device...\n");
	fbdevHWSetVideoModes(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Checking Modes against monitor...\n");
	{
		DisplayModePtr mode, first = mode = pScrn->modes;
		
		if (mode != NULL) do {
			mode->status = xf86CheckModeForMonitor(mode, pScrn->monitor);
			mode = mode->next;
		} while (mode != NULL && mode != first);

		xf86PruneDriverModes(pScrn);
	}

	if (NULL == pScrn->modes)
		fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;

	if (fPtr->shadowFB)
		pScrn->displayWidth = pScrn->virtualX;	/* ShadowFB handles this correctly */
	else {
		int fbbpp;
		/* FIXME: this doesn't work for all cases, e.g. when each scanline
			has a padding which is independent from the depth (controlfb) */
		fbdevHWGetDepth(pScrn,&fbbpp);
		pScrn->displayWidth = fbdevHWGetLineLength(pScrn)/(fbbpp >> 3);
	}

	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load bpp-specific modules */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	case FBDEVHW_PLANES:
		mod = "afb";
		syms = afbSymbols;
		break;
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel)
		{
		case 8:
		case 16:
		case 24:
		case 32:
			mod = "fb";
			syms = fbSymbols;
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Unsupported bpp: %d", pScrn->bitsPerPixel);
			return FALSE;
		}
		break;
	case FBDEVHW_INTERLEAVED_PLANES:
               /* Not supported yet, don't know what to do with this */
               xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "Interleaved Planes are not supported yet by drivers/fbdev.");
		return FALSE;
	case FBDEVHW_TEXT:
               /* This should never happen ...
                * we should check for this much much earlier ... */
               xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "Text mode is not supprted by drivers/fbdev.\n"
               "Why do you want to run the X in TEXT mode anyway ?");
		return FALSE;
       case FBDEVHW_VGA_PLANES:
               /* Not supported yet */
               xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "EGA/VGA Planes are not supprted yet by drivers/fbdev.");
               return FALSE;
       default:
               xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "Fbdev type (%d) not supported yet.", type);
               return FALSE;
	}
	if (mod && xf86LoadSubModule(pScrn, mod) == NULL) {
		FBDevFreeRec(pScrn);
		return FALSE;
	}
	if (mod && syms) {
		xf86LoaderReqSymLists(syms, NULL);
	}

	/* Load shadow if needed */
	if (fPtr->shadowFB) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Using \"Shadow Framebuffer\"\n");
		if (!xf86LoadSubModule(pScrn, "shadow")) {
			FBDevFreeRec(pScrn);
			return FALSE;
		}
		xf86LoaderReqSymLists(shadowSymbols, NULL);
	}

	TRACE_EXIT("PreInit");
	return TRUE;
}

static Bool
FBDevScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FBDevPtr fPtr = FBDEVPTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	int ret,flags,width,height;
	int type;

	TRACE_ENTER("FBDevScreenInit");

#if DEBUG
	ErrorF("\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
	       "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
	       pScrn->bitsPerPixel,
	       pScrn->depth,
	       xf86GetVisualName(pScrn->defaultVisual),
	       pScrn->mask.red,pScrn->mask.green,pScrn->mask.blue,
	       pScrn->offset.red,pScrn->offset.green,pScrn->offset.blue);
#endif

	if (NULL == (fPtr->fbmem = fbdevHWMapVidmem(pScrn))) {
	        xf86DrvMsg(scrnIndex,X_ERROR,"Map vid mem failed\n");
		return FALSE;
	}
	fPtr->fboff = fbdevHWLinearOffset(pScrn);

	fbdevHWSave(pScrn);

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"Mode init failed\n");
		return FALSE;
	}
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex,0,0,0);

	/* mi layer */
	miClearVisualTypes();
	if (pScrn->bitsPerPixel > 8) {
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"Set visual types failed\n");
			return FALSE;
		}
	} else {
		if (!miSetVisualTypes(pScrn->depth,
				      miGetDefaultVisualMask(pScrn->depth),
				      pScrn->rgbBits, pScrn->defaultVisual)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"Set visual types failed\n");
			return FALSE;
		}
	}
	if (!miSetPixmapDepths()) {
	  xf86DrvMsg(scrnIndex,X_ERROR,"Set pixmap depths failed\n");
	  return FALSE;
	}

	if(fPtr->rotate==FBDEV_ROTATE_CW || fPtr->rotate==FBDEV_ROTATE_CCW)
	{
	  height = pScrn->virtualX;
	  width = pScrn->displayWidth = pScrn->virtualY;
	} else {
	  height = pScrn->virtualY;
	  width = pScrn->virtualX;
	}

	if(fPtr->rotate && !fPtr->PointerMoved) {
		fPtr->PointerMoved = pScrn->PointerMoved;
		pScrn->PointerMoved = FBDevPointerMoved;
	}

	/* shadowfb */
	if (fPtr->shadowFB) {
		if ((fPtr->shadowmem = shadowAlloc(width, height,
						   pScrn->bitsPerPixel)) == NULL) {
		xf86DrvMsg(scrnIndex,X_ERROR,
			   "Allocation of shadow memory failed\n");
		return FALSE;
	}

		fPtr->fbstart   = fPtr->shadowmem;
	} else {
		fPtr->shadowmem = NULL;
		fPtr->fbstart   = fPtr->fbmem + fPtr->fboff;
	}

	switch ((type = fbdevHWGetType(pScrn)))
	{
#ifdef USE_AFB
	case FBDEVHW_PLANES:
		if (fPtr->rotate)
		{
		  xf86DrvMsg(scrnIndex, X_ERROR,
			     "Internal error: Rotate not supported for afb\n");
		  ret = FALSE;
		  break;
		}
		if (fPtr->shadowFB)
		{
		  xf86DrvMsg(scrnIndex, X_ERROR,
			     "Internal error: Shadow framebuffer not supported for afb\n");
		  ret = FALSE;
		  break;
		}
		ret = afbScreenInit
			(pScreen, fPtr->fbstart, pScrn->virtualX, pScrn->virtualY,
			 pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth);
		break;
#endif
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel) {
		case 8:
		case 16:
		case 24:
		case 32:
			ret = fbScreenInit(pScreen, fPtr->fbstart, width, height,
					   pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth, pScrn->bitsPerPixel);
			init_picture = 1;
			break;
	 	default:
			xf86DrvMsg(scrnIndex, X_ERROR,
				   "Internal error: invalid bpp (%d) in FBDevScreenInit\n",
				   pScrn->bitsPerPixel);
			ret = FALSE;
			break;
		}
		break;
	case FBDEVHW_INTERLEAVED_PLANES:
		/* This should never happen ...
		* we should check for this much much earlier ... */
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: Text mode is not supprted by drivers/fbdev.\n"
		"Comment: Why do you want to run the X in TEXT mode anyway ?");
		ret = FALSE;
		break;
	case FBDEVHW_TEXT:
		/* This should never happen ...
		* we should check for this much much earlier ... */
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: Text mode is not supprted by drivers/fbdev.\n"
		"Comment: Why do you want to run the X in TEXT mode anyway ?");
		ret = FALSE;
		break;
	case FBDEVHW_VGA_PLANES:
		/* Not supported yet */
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: EGA/VGA Planes are not supprted"
		" yet by drivers/fbdev.");
		ret = FALSE;
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: fbdev type (%d) unsupported in"
		" FBDevScreenInit\n", type);
		ret = FALSE;
		break;
	}
	if (!ret)
		return FALSE;

	if (pScrn->bitsPerPixel > 8) {
		/* Fixup RGB ordering */
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed   = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue  = pScrn->offset.blue;
				visual->redMask     = pScrn->mask.red;
				visual->greenMask   = pScrn->mask.green;
				visual->blueMask    = pScrn->mask.blue;
			}
		}
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "RENDER extension initialisation failed.\n");

	if (fPtr->shadowFB && 
	    (!shadowSetup(pScreen) || !shadowAdd(pScreen, NULL,
	      fPtr->rotate ? shadowUpdateRotatePacked : shadowUpdatePacked,
	      FBDevWindowLinear, fPtr->rotate, NULL)) ) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Shadow framebuffer initialization failed.\n");
	    return FALSE;
	}

	if (!fPtr->rotate)
	  FBDevDGAInit(pScrn, pScreen);
	else {
	  xf86DrvMsg(scrnIndex, X_INFO, "Rotated display, disabling DGA\n");
	  xf86DrvMsg(scrnIndex, X_INFO, "Enabling Driver rotation, disabling RandR\n");
	  xf86DisableRandR();
	  if (pScrn->bitsPerPixel == 24)
	    xf86DrvMsg(scrnIndex, X_WARNING, "Rotation might be broken in 24 bpp\n");
	}

	xf86SetBlackWhitePixels(pScreen);
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* software cursor */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	/* colormap */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	/* XXX It would be simpler to use miCreateDefColormap() in all cases. */
#ifdef USE_AFB
	case FBDEVHW_PLANES:
		if (!afbCreateDefColormap(pScreen))
			return FALSE;
		break;
#endif
	case FBDEVHW_PACKED_PIXELS:
		if (!miCreateDefColormap(pScreen))
			return FALSE;
		break;
	case FBDEVHW_INTERLEAVED_PLANES:
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: invalid fbdev type (interleaved planes)"
		" in FBDevScreenInit\n");
		return FALSE;
	case FBDEVHW_TEXT:
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: invalid fbdev type (text)"
		" in FBDevScreenInit\n");
		return FALSE;
	case FBDEVHW_VGA_PLANES:
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: invalid fbdev type (ega/vga planes)"
		" in FBDevScreenInit\n");
		return FALSE;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
		"Internal error: invalid fbdev type (%d) in FBDevScreenInit\n",
		type);
		return FALSE;
	}
	flags = CMAP_PALETTED_TRUECOLOR;
	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPalette, NULL, flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSet, 0);

	pScreen->SaveScreen = fbdevHWSaveScreen;

	/* Wrap the current CloseScreen function */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = FBDevCloseScreen;

	{
	    XF86VideoAdaptorPtr *ptr;

	    int n = xf86XVListGenericAdaptors(pScrn,&ptr);
	    if (n) {
		xf86XVScreenInit(pScreen,ptr,n);
	    }
	}

	TRACE_EXIT("FBDevScreenInit");

	return TRUE;
}

static Bool
FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	FBDevPtr fPtr = FBDEVPTR(pScrn);
	
	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);
	if (fPtr->shadowmem)
		xfree(fPtr->shadowmem);
	if (fPtr->pDGAMode) {
	  xfree(fPtr->pDGAMode);
	  fPtr->pDGAMode = NULL;
	  fPtr->nDGAMode = 0;
	}
	pScrn->vtSema = FALSE;

	pScreen->CloseScreen = fPtr->CloseScreen;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}



/***********************************************************************
 * Shadow stuff
 ***********************************************************************/

static void *
FBDevWindowLinear(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
		 CARD32 *size, void *closure)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    FBDevPtr fPtr = FBDEVPTR(pScrn);

    if (!pScrn->vtSema)
      return NULL;

    if (fPtr->lineLength)
      *size = fPtr->lineLength;
    else
      *size = fPtr->lineLength = fbdevHWGetLineLength(pScrn);

    return ((CARD8 *)fPtr->fbmem + fPtr->fboff + row * fPtr->lineLength + offset);
}

static void
FBDevPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    int newX, newY;

    switch (fPtr->rotate)
    {
    case FBDEV_ROTATE_CW:
	/* 90 degrees CW rotation. */
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
	break;

    case FBDEV_ROTATE_CCW:
	/* 90 degrees CCW rotation. */
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
	break;

    case FBDEV_ROTATE_UD:
	/* 180 degrees UD rotation. */
	newX = pScrn->pScreen->width - x - 1;
	newY = pScrn->pScreen->height - y - 1;
	break;

    default:
	/* No rotation. */
	newX = x;
	newY = y;
	break;
    }

    /* Pass adjusted pointer coordinates to wrapped PointerMoved function. */
    (*fPtr->PointerMoved)(index, newX, newY);
}


/***********************************************************************
 * DGA stuff
 ***********************************************************************/
static Bool FBDevDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
				   unsigned char **ApertureBase,
				   int *ApertureSize, int *ApertureOffset,
				   int *flags);
static Bool FBDevDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode);
static void FBDevDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags);

static Bool
FBDevDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
		       unsigned char **ApertureBase, int *ApertureSize,
		       int *ApertureOffset, int *flags)
{
    *DeviceName = NULL;		/* No special device */
    *ApertureBase = (unsigned char *)(pScrn->memPhysBase);
    *ApertureSize = pScrn->videoRam;
    *ApertureOffset = pScrn->fbOffset;
    *flags = 0;

    return TRUE;
}

static Bool
FBDevDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode)
{
    DisplayModePtr pMode;
    int scrnIdx = pScrn->pScreen->myNum;
    int frameX0, frameY0;

    if (pDGAMode) {
	pMode = pDGAMode->mode;
	frameX0 = frameY0 = 0;
    }
    else {
	if (!(pMode = pScrn->currentMode))
	    return TRUE;

	frameX0 = pScrn->frameX0;
	frameY0 = pScrn->frameY0;
    }

    if (!(*pScrn->SwitchMode)(scrnIdx, pMode, 0))
	return FALSE;
    (*pScrn->AdjustFrame)(scrnIdx, frameX0, frameY0, 0);

    return TRUE;
}

static void
FBDevDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
    (*pScrn->AdjustFrame)(pScrn->pScreen->myNum, x, y, flags);
}

static int
FBDevDGAGetViewport(ScrnInfoPtr pScrn)
{
    return (0);
}

static DGAFunctionRec FBDevDGAFunctions =
{
    FBDevDGAOpenFramebuffer,
    NULL,       /* CloseFramebuffer */
    FBDevDGASetMode,
    FBDevDGASetViewport,
    FBDevDGAGetViewport,
    NULL,       /* Sync */
    NULL,       /* FillRect */
    NULL,       /* BlitRect */
    NULL,       /* BlitTransRect */
};

static void
FBDevDGAAddModes(ScrnInfoPtr pScrn)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    DisplayModePtr pMode = pScrn->modes;
    DGAModePtr pDGAMode;

    do {
	pDGAMode = xrealloc(fPtr->pDGAMode,
			    (fPtr->nDGAMode + 1) * sizeof(DGAModeRec));
	if (!pDGAMode)
	    break;

	fPtr->pDGAMode = pDGAMode;
	pDGAMode += fPtr->nDGAMode;
	(void)memset(pDGAMode, 0, sizeof(DGAModeRec));

	++fPtr->nDGAMode;
	pDGAMode->mode = pMode;
	pDGAMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	pDGAMode->byteOrder = pScrn->imageByteOrder;
	pDGAMode->depth = pScrn->depth;
	pDGAMode->bitsPerPixel = pScrn->bitsPerPixel;
	pDGAMode->red_mask = pScrn->mask.red;
	pDGAMode->green_mask = pScrn->mask.green;
	pDGAMode->blue_mask = pScrn->mask.blue;
	pDGAMode->visualClass = pScrn->bitsPerPixel > 8 ?
	    TrueColor : PseudoColor;
	pDGAMode->xViewportStep = 1;
	pDGAMode->yViewportStep = 1;
	pDGAMode->viewportWidth = pMode->HDisplay;
	pDGAMode->viewportHeight = pMode->VDisplay;

	if (fPtr->lineLength)
	  pDGAMode->bytesPerScanline = fPtr->lineLength;
	else
	  pDGAMode->bytesPerScanline = fPtr->lineLength = fbdevHWGetLineLength(pScrn);

	pDGAMode->imageWidth = pMode->HDisplay;
	pDGAMode->imageHeight =  pMode->VDisplay;
	pDGAMode->pixmapWidth = pDGAMode->imageWidth;
	pDGAMode->pixmapHeight = pDGAMode->imageHeight;
	pDGAMode->maxViewportX = pScrn->virtualX -
				    pDGAMode->viewportWidth;
	pDGAMode->maxViewportY = pScrn->virtualY -
				    pDGAMode->viewportHeight;

	pDGAMode->address = fPtr->fbstart;

	pMode = pMode->next;
    } while (pMode != pScrn->modes);
}

static Bool
FBDevDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);

    if (pScrn->depth < 8)
	return FALSE;

    if (!fPtr->nDGAMode)
	FBDevDGAAddModes(pScrn);

    return (DGAInit(pScreen, &FBDevDGAFunctions,
	    fPtr->pDGAMode, fPtr->nDGAMode));
}
