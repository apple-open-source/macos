
/* 
   XFree86 driver for Glide(tm). (Mainly for Voodoo 1 and 2 cards)

   Since Voodoo 1 and Voodoo 2 cards are very, very NOT made for
   running a 2D windowing system, this driver is a little
   special. Basically, we have a virtual framebuffer in RAM (the
   Shadow Framebuffer) and we copy selected regions of this to the
   voodoo card at appropriate times. We get no hardware acceleration
   help (there isn't any for 2D on these cards), but since the
   framebuffer is in cached RAM, we get a useable display
   anyway. Also, we don't have any interaction with any hardware since
   Glide is the layer beneath the driver.

   Author: 
     Henrik Harmsen (hch@cd.chalmers.se or Henrik.Harmsen@erv.ericsson.se) 

   HISTORY
   1999-04-05
   - First release for 3.9Pi

   1999-04-17
   - Soft link to libglide2x.so instead of addition to ModulePath
   - Changed "EXTERN_MODULE" to EXTERN_MODULE
   - Uses the "GlideDevice" option instead of the "BusID" line to select
     which voodoo board to use.
   - Manpage updates

   1999-06-25
   - Modify glideSetup to not register the driver when libglide2x.so cannot
     be loaded, and to return appropriate error codes when it fails.
   - Prevent GLIDEFreeScreen() from crashing if called early.

   1999-08-22
   - Minor fixes.

   1999-11-22
   - Minor change in GLIDE_FIND_FUNC by Loïc Grenié, grenie@math.jussieu.fr.

   TODO
   * Support for adjusting gamma correction.
   * Support for setting gamma individually for R,G,B when Glide 3 arrives
     for Linux.  This will allow me to get rid of that sick green tint my
     voodoo2 board produces...
   * Support static loading.  
*/

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glide/glide_driver.c,v 1.28 2002/01/04 21:22:30 tsi Exp $ */

#include "xaa.h"
#include "xf86Cursor.h"
#include "colormapst.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "xf86DDC.h"
#include "globals.h"
#define DPMS_SERVER
#include "extensions/dpms.h"
#include "fb.h"
#include "xf86cmap.h"
#include "shadowfb.h"

#include <glide.h>

#define TRUE 1
#define FALSE 0
#ifdef NULL
#undef NULL
#endif
#define NULL 0
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef signed char        s8;
typedef unsigned char      u8;
typedef signed short int   s16;
typedef unsigned short int u16;
typedef signed long int    s32;
typedef unsigned long int  u32;
typedef u8                 bool;
 
/* Card-specific driver information */

#define GLIDEPTR(p) ((GLIDEPtr)((p)->driverPrivate))


typedef FxBool (*pgrSstQueryBoards_t)(GrHwConfiguration*);
typedef void (*pgrGlideInit_t)(void);
typedef void (*pgrSstSelect_t)(int which_sst);
typedef FxBool (*pgrSstWinOpen_t)(FxU32, GrScreenResolution_t, GrScreenRefresh_t, 
                                  GrColorFormat_t, GrOriginLocation_t, int, int);
typedef void (*pgrRenderBuffer_t)(GrBuffer_t);
typedef void (*pgrClipWindow_t)(FxU32, FxU32, FxU32, FxU32);
typedef void (*pgrBufferClear_t)(GrColor_t, GrAlpha_t, FxU16);
typedef FxBool (*pgrLfbLock_t)(GrLock_t, GrBuffer_t, GrLfbWriteMode_t, GrOriginLocation_t, 
                               FxBool, GrLfbInfo_t*);
typedef FxBool (*pgrLfbUnlock_t)(GrLock_t, GrBuffer_t);
typedef void (*pgrGlideShutdown_t)(void);


#if defined(GLIDE3) && defined(GLIDE3_ALPHA)
typedef FxBool (*pgrLfbWriteRegion_t)(GrBuffer_t, FxU32, FxU32, GrLfbSrcFmt_t, 
                                     FxU32, FxU32, FxBool, FxI32, void*);
#else
typedef FxBool (*pgrLfbWriteRegion_t)(GrBuffer_t, FxU32, FxU32, GrLfbSrcFmt_t, 
                                     FxU32, FxU32, FxI32, void*);
#endif


typedef struct {
  u8*                 ShadowPtr;
  u32                 ShadowPitch;
  u32                 SST_Index;
  CloseScreenProcPtr  CloseScreen;
  Bool                Blanked;
  u32                 grRefreshRate;
  u32                 grResolution;
  Bool                OnAtExit;
  Bool                GlideInitiated;
  EntityInfoPtr       pEnt;
  OptionInfoPtr       Options;
} GLIDERec, *GLIDEPtr;

static pgrSstQueryBoards_t pgrSstQueryBoards;
static pgrGlideInit_t      pgrGlideInit;
static pgrSstSelect_t      pgrSstSelect;
static pgrSstWinOpen_t     pgrSstWinOpen;
static pgrRenderBuffer_t   pgrRenderBuffer;
static pgrClipWindow_t     pgrClipWindow;
static pgrBufferClear_t    pgrBufferClear;
static pgrLfbLock_t        pgrLfbLock;
static pgrLfbUnlock_t      pgrLfbUnlock;
static pgrGlideShutdown_t  pgrGlideShutdown;
static pgrLfbWriteRegion_t pgrLfbWriteRegion;

static const OptionInfoRec * GLIDEAvailableOptions(int chipid, int busid);
static void	GLIDEIdentify(int flags);
static Bool	GLIDEProbe(DriverPtr drv, int flags);
static Bool	GLIDEPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	GLIDEScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);
static Bool	GLIDEEnterVT(int scrnIndex, int flags);
static void	GLIDELeaveVT(int scrnIndex, int flags);
static Bool	GLIDECloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	GLIDESaveScreen(ScreenPtr pScreen, int mode);
static void     GLIDEFreeScreen(int scrnIndex, int flags);
static void     GLIDERefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
static Bool     GLIDEModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void     GLIDERestore(ScrnInfoPtr pScrn, Bool Closing);
static void     GLIDERefreshAll(ScrnInfoPtr pScrn);

static void	GLIDEDisplayPowerManagementSet(ScrnInfoPtr pScrn,
                                               int PowerManagementMode,
                                               int flags);


static int LoadGlide(void);

#define VERSION 4000
#define GLIDE_NAME "GLIDE"
#define GLIDE_DRIVER_NAME "glide"
#define GLIDE_MAJOR_VERSION 1
#define GLIDE_MINOR_VERSION 0
#define GLIDE_PATCHLEVEL 0

/* 
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

DriverRec GLIDE = {
  VERSION,
  GLIDE_DRIVER_NAME,
  GLIDEIdentify,
  GLIDEProbe,
  GLIDEAvailableOptions,
  NULL,
  0
};

typedef enum {
  OPTION_ON_AT_EXIT,
  OPTION_GLIDEDEVICE
} GLIDEOpts;

static const OptionInfoRec GLIDEOptions[] = {
  { OPTION_ON_AT_EXIT, "OnAtExit",       OPTV_BOOLEAN, {0}, FALSE },
  { OPTION_GLIDEDEVICE, "GlideDevice",   OPTV_INTEGER, {0}, FALSE },
  { -1,	               NULL,             OPTV_NONE,    {0}, FALSE }
};

/* Supported chipsets */
static SymTabRec GLIDEChipsets[] = {
  { 0, "Voodoo" },
  {-1, NULL }
};


/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */

static const char *fbSymbols[] = {
  "fbScreenInit",
  "fbPictureInit",
  NULL
};

static const char *shadowSymbols[] = {
  "ShadowFBInit",
  NULL
};

#ifdef XFree86LOADER

static MODULESETUPPROTO(glideSetup);

static XF86ModuleVersionInfo glideVersRec =
{
  "glide",
  MODULEVENDORSTRING,
  MODINFOSTRING1,
  MODINFOSTRING2,
  XF86_VERSION_CURRENT,
  GLIDE_MAJOR_VERSION, GLIDE_MINOR_VERSION, GLIDE_PATCHLEVEL,
  ABI_CLASS_VIDEODRV,			/* This is a video driver */
  ABI_VIDEODRV_VERSION,
  MOD_CLASS_VIDEODRV,
  {0,0,0,0}
};

XF86ModuleData glideModuleData = { &glideVersRec, glideSetup, NULL };

static pointer
glideSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
  static Bool setupDone = FALSE;
  pointer ret;
  int errmaj2 = 0, errmin2 = 0;
    
  if (!setupDone) 
  {
    /*
     * Modules that this driver always requires may be loaded here
     * by calling LoadSubModule().
     */

    ret = LoadSubModule(module, "glide2x", NULL, NULL, EXTERN_MODULE, NULL,
			&errmaj2, &errmin2);
    if (!ret)
    {
      xf86Msg(X_ERROR, "Glide driver:\n"
"\n"
"Could not load the shared library file for Glide: \"libglide2x.so\"! \n"
"\n"
"You need to have Glide installed to run the glide driver for XFree86.\n"
"Also, you need to tell XFree86 where the libglide2x.so file is placed\n"
"by making a soft link in the /usr/X11R6/lib/modules directory that points\n"
"to the libglide2x.so file. For example (if your libglide2x.so file is in\n"
"/usr/lib):\n"
"\n"
"  # ln -s /usr/lib/libglide2x.so /usr/X11R6/lib/modules\n"
"\n"
"\n");
      if (errmaj)
        *errmaj = LDR_NOSUBENT;
      if (errmin)
        *errmin = errmaj2;
      return NULL;
    }

    if (!LoadGlide()) {
      if (errmaj)
        *errmaj = LDR_MODSPECIFIC;
      if (errmin)
        *errmin = 0;
      return NULL;
    }

    setupDone = TRUE;
    /* This module should be loaded only once */
    *errmaj = LDR_ONCEONLY;
    xf86AddDriver(&GLIDE, module, 0);

    /*
     * Tell the loader about symbols from other modules that this module
     * might refer to.
     */
    LoaderRefSymLists(fbSymbols, shadowSymbols, NULL);

    /*
     * The return value must be non-NULL on success even though there
     * is no TearDownProc.
     */
    return (pointer)1;
  } 
  else 
  {
    if (errmaj) 
      *errmaj = LDR_ONCEONLY;
    return NULL;
  }
}
#endif /* XFree86LOADER */

static Bool
GLIDEGetRec(ScrnInfoPtr pScrn)
{
  /*
   * Allocate an GLIDERec, and hook it into pScrn->driverPrivate.
   * pScrn->driverPrivate is initialised to NULL, so we can check if
   * the allocation has already been done.
     */
  if (pScrn->driverPrivate != NULL)
    return TRUE;

  pScrn->driverPrivate = xnfcalloc(sizeof(GLIDERec), 1);

  /* Initialize it */
  /* No init here yet */
  return TRUE;
}

static void
GLIDEFreeRec(ScrnInfoPtr pScrn)
{
  if (pScrn->driverPrivate == NULL)
    return;
  xfree(pScrn->driverPrivate);
  pScrn->driverPrivate = NULL;
}


static const OptionInfoRec *
GLIDEAvailableOptions(int chipid, int busid)
{
   return GLIDEOptions;
}

/* Mandatory */
static void
GLIDEIdentify(int flags)
{
  xf86PrintChipsets(GLIDE_NAME, "driver for Glide devices (Voodoo cards)", GLIDEChipsets);
}


/* Mandatory */
static Bool
GLIDEProbe(DriverPtr drv, int flags)
{
  GrHwConfiguration hw;
  int i, sst, r;
  GDevPtr *devList;
  GDevPtr dev = NULL;
  int numdevList;
  Bool foundScreen = FALSE;
  ScrnInfoPtr pScrn;
  int GlideDevice;

  if ((numdevList = xf86MatchDevice(GLIDE_DRIVER_NAME, &devList)) <= 0)
    return FALSE;

  r = pgrSstQueryBoards(&hw);
  if (!r)
  {
    xf86Msg(X_ERROR, "GLIDEProbe(): Error calling pgrSstQueryBoards!\n");
    goto cleanup;
  }
  
  
  /* hw.num_sst : number of Glide boards available */
  if (hw.num_sst > 0 && (flags & PROBE_DETECT)) {
    /* XXX Need to call xf886AddDeviceToConfigure() here */
    return TRUE;
  }

  for (sst = 0; sst < hw.num_sst; sst++)
  {
    for (i = 0; i < numdevList; i++)
    {
      dev = devList[i];
      GlideDevice = xf86SetIntOption(dev->options, "GlideDevice", 0);
      if (GlideDevice == sst)
      {
        int entity;
        /* Match */
        entity = xf86ClaimIsaSlot(drv, 0, dev, TRUE);
        pScrn = NULL;

        /* Allocate a ScrnInfoRec and claim the slot */
        if ((pScrn = xf86ConfigIsaEntity(pScrn, 0, entity, NULL, NULL,
					       NULL, NULL, NULL, NULL))) {
	    
	    /* I'm not going to "claim" the glide device since no other driver than this can drive it */
	    /* (A glide device is not a PCI device) */
	    /* XXX Need to see how this fits in with the new RAC */
	    
	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = VERSION;
	    pScrn->driverName    = GLIDE_DRIVER_NAME;
	    pScrn->name          = GLIDE_NAME;
	    pScrn->Probe	 = GLIDEProbe;
	    pScrn->PreInit	 = GLIDEPreInit;
	    pScrn->ScreenInit    = GLIDEScreenInit;
	    pScrn->EnterVT	 = GLIDEEnterVT;
	    pScrn->LeaveVT	 = GLIDELeaveVT;
	    pScrn->FreeScreen    = GLIDEFreeScreen;
	    pScrn->driverPrivate = (void*)sst;
	    /*
	     * XXX This is a hack because don't have the PCI info.  Set it as
	     * an ISA entity with no resources.
	     */
	    foundScreen = TRUE;
	}
        break;
      }
    }
  }

 cleanup:
  xfree(devList);
  return foundScreen;
}
	



/* Mandatory */
static Bool
GLIDEPreInit(ScrnInfoPtr pScrn, int flags)
{
  GLIDEPtr pGlide;
  MessageType from;
  int i;
  ClockRangePtr clockRanges;
  int sst;

  if (flags & PROBE_DETECT) return FALSE;

  /* Check the number of entities, and fail if it isn't one. */
  if (pScrn->numEntities != 1)
    return FALSE;

  sst = (int)(pScrn->driverPrivate);
  pScrn->driverPrivate = NULL;

  /* Set pScrn->monitor */
  pScrn->monitor = pScrn->confScreen->monitor;

  if (!xf86SetDepthBpp(pScrn, 16, 0, 0, Support32bppFb)) {
    return FALSE;
  }

  /* Check that the returned depth is one we support */
  switch (pScrn->depth) {
  case 16:
  case 24:
    /* OK */
    break;
  default:
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "Given depth (%d) is not supported by this driver\n",
               pScrn->depth);
    return FALSE;
  }
  xf86PrintDepthBpp(pScrn);

  /*
   * This must happen after pScrn->display has been set because
   * xf86SetWeight references it.
   */
  if (pScrn->depth > 8) {
    /* The defaults are OK for us */
    rgb zeros = {0, 0, 0};

    if (!xf86SetWeight(pScrn, zeros, zeros)) {
      return FALSE;
    } else {
      /* XXX check that weight returned is supported */
      ;
    }
  }

  /* Set the default visual. */
  if (!xf86SetDefaultVisual(pScrn, -1)) {
    return FALSE;
  }
  /* We don't support DirectColor at > 8bpp */
  if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
               " (%s) is not supported at depth %d\n",
               xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
    return FALSE;
  }

  /* Set default gamma */
  {
    Gamma zeros = {0.0, 0.0, 0.0};

    if (!xf86SetGamma(pScrn, zeros)) {
      return FALSE;
    }
  }

  /* We use a programmable clock */
  pScrn->progClock = TRUE;

  /* Allocate the GLIDERec driverPrivate */
  if (!GLIDEGetRec(pScrn)) {
    return FALSE;
  }

  pGlide = GLIDEPTR(pScrn);

  /* Get the entity */
  pGlide->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

  /* Collect all of the relevant option flags (fill in pScrn->options) */
  xf86CollectOptions(pScrn, NULL);

  /* Process the options */
  if (!(pGlide->Options = xalloc(sizeof(GLIDEOptions))))
    return FALSE;
  memcpy(pGlide->Options, GLIDEOptions, sizeof(GLIDEOptions));
  xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pGlide->Options);

  pGlide->OnAtExit = FALSE;
  from = X_DEFAULT;
  if (xf86GetOptValBool(pGlide->Options, OPTION_ON_AT_EXIT, &(pGlide->OnAtExit)))
    from = X_CONFIG;

  xf86DrvMsg(pScrn->scrnIndex, from, 
             "Voodoo card will be %s when exiting server.\n", 
             pGlide->OnAtExit ? "ON" : "OFF");

  pGlide->SST_Index = sst;

  /*
   * If the user has specified the amount of memory in the XF86Config
   * file, we respect that setting.
   */
  if (pGlide->pEnt->device->videoRam != 0) {
    pScrn->videoRam = pGlide->pEnt->device->videoRam;
    from = X_CONFIG;
  } else {
    pScrn->videoRam = 8192; /* It's just virtual framebuffer anyway so let's say we have an 8MB sized framebuffer */
    from = X_PROBED;
  }
#if 0
  xf86DrvMsg(pScrn->scrnIndex, from, "Virtual video RAM: %d kB\n",
             pScrn->videoRam);
#endif

  /* Set up clock ranges so that the xf86ValidateModes() function will not fail a mode because of the clock
     requirement (because we don't use the clock value anyway) */
  clockRanges = xnfcalloc(sizeof(ClockRange), 1);
  clockRanges->next = NULL;
  clockRanges->minClock = 10000;
  clockRanges->maxClock = 300000;
  clockRanges->clockIndex = -1;		/* programmable */
  clockRanges->interlaceAllowed = TRUE;
  clockRanges->doubleScanAllowed = TRUE;

  /* Select valid modes from those available */
  i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                        pScrn->display->modes, clockRanges,
                        NULL, 256, 2048,
                        pScrn->bitsPerPixel, 128, 2048,
                        pScrn->display->virtualX,
                        pScrn->display->virtualY,
                        pScrn->videoRam * 1024,
                        LOOKUP_BEST_REFRESH);
    
  if (i == -1) {
    GLIDEFreeRec(pScrn);
    return FALSE;
  }

  /* Prune the modes marked as invalid */
  xf86PruneDriverModes(pScrn);

  /* If no valid modes, return */
  if (i == 0 || pScrn->modes == NULL) {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
    GLIDEFreeRec(pScrn);
    return FALSE;
  }

  /* Set the current mode to the first in the list */
  pScrn->currentMode = pScrn->modes;

  /* Do some checking, we will not support a virtual framebuffer larger than
     the visible screen. */
  if (pScrn->currentMode->HDisplay != pScrn->virtualX || 
      pScrn->currentMode->VDisplay != pScrn->virtualY ||
      pScrn->displayWidth != pScrn->virtualX)
  {
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
               "Virtual size doesn't equal display size. Forcing virtual size to equal display size.\n");
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
               "(Virtual size: %dx%d, Display size: %dx%d)\n", pScrn->virtualX, pScrn->virtualY,
               pScrn->currentMode->HDisplay, pScrn->currentMode->VDisplay);
    /* I'm not entirely sure this is "legal" but I hope so. */
    pScrn->virtualX = pScrn->currentMode->HDisplay;
    pScrn->virtualY = pScrn->currentMode->VDisplay;
    pScrn->displayWidth = pScrn->virtualX;
  }

  /* TODO: Note: If I return FALSE right here, the server will not restore the console correctly,
     forcing a reboot. Must find that. (valid for 3.9Pi) */

  /* Print the list of modes being used */
  xf86PrintModes(pScrn);

  /* Set display resolution */
  xf86SetDpi(pScrn, 0, 0);
    
  /* Load fb */
  if (xf86LoadSubModule(pScrn, "fb") == NULL) {
    GLIDEFreeRec(pScrn);
    return FALSE;
  }

  xf86LoaderReqSymLists(fbSymbols, NULL);

  /* Load the shadow framebuffer */
  if (!xf86LoadSubModule(pScrn, "shadowfb")) {
    GLIDEFreeRec(pScrn);
    return FALSE;
  }
  xf86LoaderReqSymLists(shadowSymbols, NULL);

  return TRUE;
}


/* Mandatory */
/* This gets called at the start of each server generation */
static Bool
GLIDEScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
  ScrnInfoPtr pScrn;
  GLIDEPtr pGlide;
  int ret;
  VisualPtr visual;

  /* 
   * First get the ScrnInfoRec
   */
  pScrn = xf86Screens[pScreen->myNum];

  pGlide = GLIDEPTR(pScrn);

  if (!GLIDEModeInit(pScrn, pScrn->currentMode))
    return FALSE;

  /*
   * The next step is to setup the screen's visuals, and initialise the
   * framebuffer code.  In cases where the framebuffer's default
   * choices for things like visual layouts and bits per RGB are OK,
   * this may be as simple as calling the framebuffer's ScreenInit()
   * function.  If not, the visuals will need to be setup before calling
   * a fb ScreenInit() function and fixed up after.
   *
   * For most PC hardware at depths >= 8, the defaults that fb uses
   * are not appropriate.  In this driver, we fixup the visuals after.
   */

  /*
   * Reset the visual list.
   */
  miClearVisualTypes();

  /* Setup the visuals we support. Only TrueColor. */
  if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth), pScrn->rgbBits, pScrn->defaultVisual))
    return FALSE;

  miSetPixmapDepths ();

  pGlide->ShadowPitch = ((pScrn->virtualX * pScrn->bitsPerPixel >> 3) + 3) & ~3L;
  pGlide->ShadowPtr = xnfalloc(pGlide->ShadowPitch * pScrn->virtualY);

  
  /*
   * Call the framebuffer layer's ScreenInit function, and fill in other
   * pScreen fields.
   */
  ret = fbScreenInit(pScreen, pGlide->ShadowPtr,
		     pScrn->virtualX, pScrn->virtualY,
		     pScrn->xDpi, pScrn->yDpi,
		     pScrn->displayWidth,
		     pScrn->bitsPerPixel);

  if (!ret)
    return FALSE;

  /* Fixup RGB ordering */
  visual = pScreen->visuals + pScreen->numVisuals;
  while (--visual >= pScreen->visuals) {
    if ((visual->class | DynamicClass) == DirectColor) {
      visual->offsetRed = pScrn->offset.red;
      visual->offsetGreen = pScrn->offset.green;
      visual->offsetBlue = pScrn->offset.blue;
      visual->redMask = pScrn->mask.red;
      visual->greenMask = pScrn->mask.green;
        visual->blueMask = pScrn->mask.blue;
    }
  }

  /* must be after RGB ordering fixed */
  fbPictureInit (pScreen, 0, 0);

  miInitializeBackingStore(pScreen);
  xf86SetBlackWhitePixels(pScreen);
  xf86SetBackingStore(pScreen);

  /* Initialize software cursor.  
     Must precede creation of the default colormap */
  miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

  /* Initialise default colourmap */
  if (!miCreateDefColormap(pScreen))
    return FALSE;

  ShadowFBInit(pScreen, GLIDERefreshArea);

  xf86DPMSInit(pScreen, GLIDEDisplayPowerManagementSet, 0);

  pScreen->SaveScreen = GLIDESaveScreen;

  /* Wrap the current CloseScreen function */
  pGlide->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = GLIDECloseScreen;

  /* Report any unused options (only for the first generation) */
  if (serverGeneration == 1) {
    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
  }

#if 0
  LoaderCheckUnresolved(LD_RESOLV_NOW);
  return FALSE;
#endif

  /* Done */
  return TRUE;
}



/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
GLIDEEnterVT(int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  return GLIDEModeInit(pScrn, pScrn->currentMode);
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 */

/* Mandatory */
static void
GLIDELeaveVT(int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  GLIDERestore(pScrn, FALSE);
}

/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
GLIDECloseScreen(int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  GLIDEPtr pGlide = GLIDEPTR(pScrn);

  if (pScrn->vtSema)
      GLIDERestore(pScrn, TRUE);
  xfree(pGlide->ShadowPtr);

  pScrn->vtSema = FALSE;

  pScreen->CloseScreen = pGlide->CloseScreen;
  return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any persistent data structures */

/* Optional */
static void
GLIDEFreeScreen(int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  GLIDEPtr pGlide = GLIDEPTR(pScrn);
  /*
   * This only gets called when a screen is being deleted.  It does not
   * get called routinely at the end of a server generation.
   */
  if (pGlide && pGlide->ShadowPtr)
    xfree(pGlide->ShadowPtr);
  GLIDEFreeRec(xf86Screens[scrnIndex]);
}


/* Do screen blanking */
/* Mandatory */
static Bool
GLIDESaveScreen(ScreenPtr pScreen, int mode)
{
  ScrnInfoPtr pScrn;
  GLIDEPtr pGlide;
  Bool unblank;
  
  unblank = xf86IsUnblank(mode);
  pScrn = xf86Screens[pScreen->myNum];
  pGlide = GLIDEPTR(pScrn);
  pGlide->Blanked = !unblank;
  if (unblank)
    GLIDERefreshAll(pScrn);
  else
    pgrBufferClear(0, 0, GR_ZDEPTHVALUE_FARTHEST);

  return TRUE;
}

static Bool
GLIDEModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  GLIDEPtr pGlide;
  int r;
  int width, height;
  double refresh;
  Bool match = FALSE;

  pGlide = GLIDEPTR(pScrn);

  if (mode->Flags & V_INTERLACE)
  {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Interlaced modes not supported\n");
    return FALSE;
  }

  width = mode->HDisplay;
  height = mode->VDisplay;

#if 0
  ErrorF("mode->HDisplay = %d, pScrn->displayWidth = %d\n", mode->HDisplay, pScrn->displayWidth);
  ErrorF("mode->VDisplay = %d, mode->HTotal = %d, mode->VTotal = %d\n", 
         mode->VDisplay, mode->HTotal, mode->VTotal);
  ErrorF("mode->Clock = %d\n", mode->Clock);
#endif

  if (width == 640 && height == 480)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_640x480;
  }
  if (width == 800 && height == 600)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_800x600;
  }
  if (width == 960 && height == 720)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_960x720;
  }
  if (width == 1024 && height == 768)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_1024x768;
  }
  if (width == 1280 && height == 1024)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_1280x1024;
  }
  if (width == 1600 && height == 1200)
  {
    match = TRUE;
    pGlide->grResolution = GR_RESOLUTION_1600x1200;
  }
  
  if (!match)
  {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
               "Selected width = %d and height = %d is not supported by glide\n", width, height);
    return FALSE;
  }

  refresh = (mode->Clock * 1.0e3)/((double)(mode->HTotal) * 
                                   (double)(mode->VTotal));
#if 0
  ErrorF("Calculated refresh rate for mode is %.2fHz\n",refresh);
#endif

  /* The Glide header files indicate there are a rather large number of
     refresh rates available. In practice, though, only 60, 75 and 85Hz
     seem to be available. If we try using another refresh rate, glide
     will default to 60Hz. */
  pGlide->grRefreshRate = GR_REFRESH_60Hz;
  if (refresh > 74.0)  pGlide->grRefreshRate = GR_REFRESH_75Hz;
  if (refresh > 84.0)  pGlide->grRefreshRate = GR_REFRESH_85Hz;


  /* Initialize the video card */
  pgrGlideInit();
  pgrSstSelect(pGlide->SST_Index);

  r = pgrSstWinOpen(0,
                    pGlide->grResolution,
                    pGlide->grRefreshRate,
                    GR_COLORFORMAT_ARGB,
                    GR_ORIGIN_UPPER_LEFT,
                    2, 0);
  if (!r)
  {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "grSstWinOpen returned %d. "
               "You are probably trying to use a resolution that is not supported by your hardware.", r);
    return FALSE;
  }

  pgrRenderBuffer(GR_BUFFER_FRONTBUFFER);
  pgrClipWindow(0, 0, 1024, 768);
  pgrBufferClear(0, 0, GR_ZDEPTHVALUE_FARTHEST);

  if (!r)
  {
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not lock glide frame buffer\n");
    return FALSE;
  }

  pGlide->Blanked = FALSE;
  pGlide->GlideInitiated = TRUE;
  return TRUE;
}

static void     
GLIDERestore(ScrnInfoPtr pScrn, Bool Closing)
{
  GLIDEPtr pGlide;

  pGlide = GLIDEPTR(pScrn);

  if (!(pGlide->GlideInitiated))
    return;
  pGlide->GlideInitiated = FALSE;
  pGlide->Blanked = TRUE;
  pgrBufferClear(0, 0, GR_ZDEPTHVALUE_FARTHEST);
  if (!Closing || !(pGlide->OnAtExit))
    pgrGlideShutdown();
}


#define GLIDE_FIND_FUNC(x) \
  p##x = (p##x##_t)LoaderSymbol(#x); \
  if (!p##x) \
  { \
    xf86Msg(X_ERROR, "Could not find " #x "() in libglide2x.so.\n"); \
    return FALSE; \
  }

static int
LoadGlide(void)
{
  GLIDE_FIND_FUNC(grSstQueryBoards);
  GLIDE_FIND_FUNC(grGlideInit);
  GLIDE_FIND_FUNC(grSstSelect);
  GLIDE_FIND_FUNC(grSstWinOpen);
  GLIDE_FIND_FUNC(grRenderBuffer);
  GLIDE_FIND_FUNC(grClipWindow);
  GLIDE_FIND_FUNC(grBufferClear);
  GLIDE_FIND_FUNC(grLfbLock);
  GLIDE_FIND_FUNC(grLfbUnlock);
  GLIDE_FIND_FUNC(grGlideShutdown);
  GLIDE_FIND_FUNC(grLfbWriteRegion);
  return TRUE;
}

static void
GLIDERefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
  GLIDEPtr pGlide = GLIDEPTR(pScrn);
  int Bpp;
  unsigned char *src;
  s32 x1, x2;

  if (pGlide->Blanked) return;
  
  Bpp = pScrn->bitsPerPixel >> 3;
  if (pScrn->bitsPerPixel == 16)
  {
    while(num--) {
      /* We align to an even number of pixels so we won't have to copy
         half-words over the PCI bus */
      x1 = (pbox->x1) & ~1;
      x2 = (pbox->x2 + 1) & ~1;
      src = pGlide->ShadowPtr + (pbox->y1 * pGlide->ShadowPitch) + 
            (x1 * Bpp);
#if defined(GLIDE3) && defined(GLIDE3_ALPHA)
      pgrLfbWriteRegion(GR_BUFFER_FRONTBUFFER, x1, pbox->y1, 
                        GR_LFB_SRC_FMT_565, x2-x1, pbox->y2-pbox->y1, FALSE,
                        pGlide->ShadowPitch, src);
#else
      pgrLfbWriteRegion(GR_BUFFER_FRONTBUFFER, x1, pbox->y1, 
                        GR_LFB_SRC_FMT_565, x2-x1, pbox->y2-pbox->y1, 
                        pGlide->ShadowPitch, src);
#endif
      pbox++;
    }
  }
  else
  {
    while(num--) {
      x1 = pbox->x1;
      x2 = pbox->x2;
      src = pGlide->ShadowPtr + (pbox->y1 * pGlide->ShadowPitch) + 
            (pbox->x1 * Bpp);
#if defined(GLIDE3) && defined(GLIDE3_ALPHA)
      pgrLfbWriteRegion(GR_BUFFER_FRONTBUFFER, x1, pbox->y1, 
                        GR_LFB_SRC_FMT_888, x2-x1, pbox->y2-pbox->y1, FALSE,
                        pGlide->ShadowPitch, src);
#else
      pgrLfbWriteRegion(GR_BUFFER_FRONTBUFFER, x1, pbox->y1, 
                        GR_LFB_SRC_FMT_888, x2-x1, pbox->y2-pbox->y1, 
                        pGlide->ShadowPitch, src);
#endif
      pbox++;
    }
  }
} 


/*
 * GLIDEDisplayPowerManagementSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 */
static void
GLIDEDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode,
                               int flags)
{
  GLIDEPtr pGlide = GLIDEPTR(pScrn);
  static int oldmode = -1;

#if 0
  ErrorF("GLIDEDisplayPowerManagementSet: %d\n", PowerManagementMode);
#endif

  if (oldmode == DPMSModeOff && PowerManagementMode != DPMSModeOff)
  {
    GLIDEModeInit(pScrn, pScrn->currentMode);
  }

  switch (PowerManagementMode)
  {
  case DPMSModeOn:
    /* Screen: On; HSync: On, VSync: On */
    pGlide->Blanked = FALSE;
    GLIDERefreshAll(pScrn);
    break;
  case DPMSModeStandby:
  case DPMSModeSuspend:
    pGlide->Blanked = TRUE;
    pgrBufferClear(0, 0, GR_ZDEPTHVALUE_FARTHEST);
    break;
  case DPMSModeOff:
    GLIDERestore(pScrn, FALSE);
    break;
  }
  oldmode = PowerManagementMode;
}


static void
GLIDERefreshAll(ScrnInfoPtr pScrn)
{
  BoxRec box;
  box.x1 = 0;
  box.x2 = pScrn->virtualX;
  box.y1 = 0;
  box.y2 = pScrn->virtualY;
  GLIDERefreshArea(pScrn, 1, &box);
}

