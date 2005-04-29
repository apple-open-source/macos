/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_driver.c,v 1.185 2004/02/27 17:29:24 twini Exp $ */
/*
 * SiS driver main code
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Thomas Winischhofer <thomas@winischhofer.net>
 *	- driver entirely rewritten since 2001, only basic structure taken from
 *	  old code (except sis_dri.c, sis_shadow.c, sis_accel.c and parts of
 *	  sis_dga.c; these were mostly taken over; sis_dri.c was changed for
 *	  new versions of the DRI layer)
 *
 * This notice covers the entire driver code unless otherwise indicated.
 *
 * Formerly based on code which is
 * 	     Copyright (C) 1998, 1999 by Alan Hourihane, Wigan, England.
 * Written by:
 *           Alan Hourihane <alanh@fairlite.demon.co.uk>,
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>,
 *           David Thomas <davtom@dream.org.uk>.
 */

#include "fb.h"
#include "mibank.h"
#include "micmap.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "dixstruct.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86cmap.h"
#include "vgaHW.h"
#include "xf86RAC.h"
#include "shadowfb.h"
#include "vbe.h"

#include "sis_shadow.h"

#include "mipointer.h"
#include "mibstore.h"
 
#include "sis.h"
#include "sis_regs.h"
#include "sis_vb.h"
#include "sis_dac.h"

#include "sis_driver.h"

#define _XF86DGA_SERVER_
#include "extensions/xf86dgastr.h"

#include "globals.h"

#define DPMS_SERVER
#include "extensions/dpms.h"

#if (XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,99,0,0)) || (defined(XvExtension))
#include "xf86xv.h"
#include "Xv.h"
#endif

#ifdef XF86DRI
#include "dri.h"
#endif

/* Globals (yes, these ARE really required to be global) */

#ifdef SISDUALHEAD
static int      	SISEntityIndex = -1;
#endif

#ifdef SISMERGED
#ifdef SISXINERAMA
static Bool 		SiSnoPanoramiXExtension = TRUE;
int 			SiSXineramaPixWidth = 0;
int 			SiSXineramaPixHeight = 0;
int 			SiSXineramaNumScreens = 0;
SiSXineramaData		*SiSXineramadataPtr = NULL;
static int 		SiSXineramaGeneration;

int SiSProcXineramaQueryVersion(ClientPtr client);
int SiSProcXineramaGetState(ClientPtr client);
int SiSProcXineramaGetScreenCount(ClientPtr client);
int SiSProcXineramaGetScreenSize(ClientPtr client);
int SiSProcXineramaIsActive(ClientPtr client);
int SiSProcXineramaQueryScreens(ClientPtr client);
int SiSSProcXineramaDispatch(ClientPtr client);
#endif
#endif

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

DriverRec SIS = {
    SIS_CURRENT_VERSION,
    SIS_DRIVER_NAME,
    SISIdentify,
    SISProbe,
    SISAvailableOptions,
    NULL,
    0
};

static SymTabRec SISChipsets[] = {
    { PCI_CHIP_SIS5597,     "SIS5597/5598" },
    { PCI_CHIP_SIS530,      "SIS530/620" },
    { PCI_CHIP_SIS6326,     "SIS6326/AGP/DVD" },
    { PCI_CHIP_SIS300,      "SIS300/305" },
    { PCI_CHIP_SIS630,      "SIS630/730" },
    { PCI_CHIP_SIS540,      "SIS540" },
    { PCI_CHIP_SIS315,      "SIS315" },
    { PCI_CHIP_SIS315H,     "SIS315H" },
    { PCI_CHIP_SIS315PRO,   "SIS315PRO" },
    { PCI_CHIP_SIS550,	    "SIS550" },
    { PCI_CHIP_SIS650,      "SIS650/M650/651/740" },
    { PCI_CHIP_SIS330,      "SIS330(Xabre)" },
    { PCI_CHIP_SIS660,      "SIS660/661FX/M661FX/M661MX/741/741GX/M741/760/M760" },
    { -1,                   NULL }
};

static PciChipsets SISPciChipsets[] = {
    { PCI_CHIP_SIS5597,     PCI_CHIP_SIS5597,   RES_SHARED_VGA },
    { PCI_CHIP_SIS530,      PCI_CHIP_SIS530,    RES_SHARED_VGA },
    { PCI_CHIP_SIS6326,     PCI_CHIP_SIS6326,   RES_SHARED_VGA },
    { PCI_CHIP_SIS300,      PCI_CHIP_SIS300,    RES_SHARED_VGA },
    { PCI_CHIP_SIS630,      PCI_CHIP_SIS630,    RES_SHARED_VGA },
    { PCI_CHIP_SIS540,      PCI_CHIP_SIS540,    RES_SHARED_VGA },
    { PCI_CHIP_SIS550,      PCI_CHIP_SIS550,    RES_SHARED_VGA },
    { PCI_CHIP_SIS315,      PCI_CHIP_SIS315,    RES_SHARED_VGA },
    { PCI_CHIP_SIS315H,     PCI_CHIP_SIS315H,   RES_SHARED_VGA },
    { PCI_CHIP_SIS315PRO,   PCI_CHIP_SIS315PRO, RES_SHARED_VGA },
    { PCI_CHIP_SIS650,      PCI_CHIP_SIS650,    RES_SHARED_VGA },
    { PCI_CHIP_SIS330,      PCI_CHIP_SIS330,    RES_SHARED_VGA },
    { PCI_CHIP_SIS660,      PCI_CHIP_SIS660,    RES_SHARED_VGA },
    { -1,                   -1,                 RES_UNDEFINED }
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,0,0)
    "XAAFillSolidRects",
#endif
    "XAAFillMono8x8PatternRects",
    "XAAHelpPatternROP",
    "XAAInit",
    NULL
};

static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWLock",
    "vgaHWMapMem",
    "vgaHWUnmapMem",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    "vgaHWUnlock",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    "xf86InterpretEDID",
    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *vbeSymbols[] = {
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    "VBEInit",
#else
    "VBEExtendedInit",
#endif
    "vbeDoEDID",
    "vbeFree",
    "VBEGetVBEInfo",
    "VBEFreeVBEInfo",
    "VBEGetModeInfo",
    "VBEFreeModeInfo",
    "VBESaveRestore",
    "VBESetVBEMode",
    "VBEGetVBEMode",
    "VBESetDisplayStart",
    "VBESetGetLogicalScanlineLength",
    NULL
};

#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmAddMap",
    "drmAgpAcquire",
    "drmAgpAlloc",
    "drmAgpBase",
    "drmAgpBind",
    "drmAgpEnable",
    "drmAgpFree",
    "drmAgpGetMode",
    "drmAgpRelease",
    "drmCtlInstHandler",
    "drmGetInterruptFromBusID",
    "drmSiSAgpInit",
    NULL
};

static const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "GlxSetVisualConfigs",
#ifdef SISNEWDRI2
    "DRICreatePCIBusID"
#endif        
    NULL
};
#endif

#ifdef XFree86LOADER

static MODULESETUPPROTO(sisSetup);

static XF86ModuleVersionInfo sisVersRec =
{
    SIS_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    SIS_MAJOR_VERSION, SIS_MINOR_VERSION, SIS_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,         /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

XF86ModuleData sisModuleData = { &sisVersRec, sisSetup, NULL };

pointer
sisSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if(!setupDone) {
       setupDone = TRUE;
       xf86AddDriver(&SIS, module, 0);
       LoaderRefSymLists(vgahwSymbols, fbSymbols, xaaSymbols,
			 shadowSymbols, ramdacSymbols,
			 vbeSymbols, int10Symbols,
#ifdef XF86DRI
			 drmSymbols, driSymbols,
#endif
			 NULL);
       return (pointer)TRUE;
    }

    if(errmaj) *errmaj = LDR_ONCEONLY;
    return NULL;
}

#endif /* XFree86LOADER */

static Bool
SISGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an SISRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if(pScrn->driverPrivate != NULL) return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(SISRec), 1);
    
    /* Initialise it to 0 */
    memset(pScrn->driverPrivate, 0, sizeof(SISRec));

    return TRUE;
}

static void
SISFreeRec(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif

    /* Just to make sure... */
    if(!pSiS) return;

#ifdef SISDUALHEAD
    pSiSEnt = pSiS->entityPrivate;
#endif

    if(pSiS->pstate) xfree(pSiS->pstate);
    pSiS->pstate = NULL;
    if(pSiS->fonts) xfree(pSiS->fonts);
    pSiS->fonts = NULL;

#ifdef SISDUALHEAD
    if(pSiSEnt) {
       if(!pSiS->SecondHead) {
          /* Free memory only if we are first head; in case of an error
	   * during init of the second head, the server will continue -
	   * and we need the BIOS image and SiS_Private for the first
	   * head.
	   */
	  if(pSiSEnt->BIOS) xfree(pSiSEnt->BIOS);
          pSiSEnt->BIOS = pSiS->BIOS = NULL;
	  if(pSiSEnt->SiS_Pr) xfree(pSiSEnt->SiS_Pr);
          pSiSEnt->SiS_Pr = pSiS->SiS_Pr = NULL;
	  if(pSiSEnt->RenderAccelArray) xfree(pSiSEnt->RenderAccelArray);
	  pSiSEnt->RenderAccelArray = pSiS->RenderAccelArray = NULL;
       } else {
      	  pSiS->BIOS = NULL;
	  pSiS->SiS_Pr = NULL;
	  pSiS->RenderAccelArray = NULL;
       }
    } else {
#endif
       if(pSiS->BIOS) xfree(pSiS->BIOS);
       pSiS->BIOS = NULL;
       if(pSiS->SiS_Pr) xfree(pSiS->SiS_Pr);
       pSiS->SiS_Pr = NULL;
       if(pSiS->RenderAccelArray) xfree(pSiS->RenderAccelArray);
       pSiS->RenderAccelArray = NULL;
#ifdef SISDUALHEAD
    }
#endif
#ifdef SISMERGED
    if(pSiS->CRT2HSync) xfree(pSiS->CRT2HSync);
    pSiS->CRT2HSync = NULL;
    if(pSiS->CRT2VRefresh) xfree(pSiS->CRT2VRefresh);
    pSiS->CRT2VRefresh = NULL;
    if(pSiS->MetaModes) xfree(pSiS->MetaModes);
    pSiS->MetaModes = NULL;
    if(pSiS->CRT2pScrn) {
       if(pSiS->CRT2pScrn->modes) {
          while(pSiS->CRT2pScrn->modes)
             xf86DeleteMode(&pSiS->CRT2pScrn->modes, pSiS->CRT2pScrn->modes);
       }
       if(pSiS->CRT2pScrn->monitor) {
          if(pSiS->CRT2pScrn->monitor->Modes) {
	     while(pSiS->CRT2pScrn->monitor->Modes)
	        xf86DeleteMode(&pSiS->CRT2pScrn->monitor->Modes, pSiS->CRT2pScrn->monitor->Modes);
	  }
	  if(pSiS->CRT2pScrn->monitor->DDC) xfree(pSiS->CRT2pScrn->monitor->DDC);
          xfree(pSiS->CRT2pScrn->monitor);
       }
       xfree(pSiS->CRT2pScrn);
       pSiS->CRT2pScrn = NULL;
    }
    if(pSiS->CRT1Modes) {
       if(pSiS->CRT1Modes != pScrn->modes) {
          if(pScrn->modes) {
             pScrn->currentMode = pScrn->modes;
             do {
                DisplayModePtr p = pScrn->currentMode->next;
                if(pScrn->currentMode->Private)
                   xfree(pScrn->currentMode->Private);
                xfree(pScrn->currentMode);
                pScrn->currentMode = p;
             } while(pScrn->currentMode != pScrn->modes);
          }
          pScrn->currentMode = pSiS->CRT1CurrentMode;
          pScrn->modes = pSiS->CRT1Modes;
          pSiS->CRT1CurrentMode = NULL;
          pSiS->CRT1Modes = NULL;
       }
    }
#endif
    if(pSiS->pVbe) vbeFree(pSiS->pVbe);
    pSiS->pVbe = NULL;
    if(pScrn->driverPrivate == NULL)
        return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void
SISDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    SISPtr pSiS = SISPTR(pScrn);
    BOOLEAN docrt1 = TRUE, docrt2 = TRUE;
    unsigned char sr1=0, cr17=0, cr63=0, sr11=0, pmreg=0, sr7=0;
    unsigned char p1_13=0, p2_0=0, oldpmreg=0;
    BOOLEAN backlight = TRUE;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
          "SISDisplayPowerManagementSet(%d)\n",PowerManagementMode);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) docrt2 = FALSE;
       else                 docrt1 = FALSE;
    }
#endif

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if(docrt2) {
       if(pSiS->VBFlags & CRT2_LCD) {
          if(((pSiS->VGAEngine == SIS_300_VGA) &&
             (pSiS->VBFlags & (VB_301|VB_30xBDH|VB_LVDS))) ||
            ((pSiS->VGAEngine == SIS_315_VGA) &&
             ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
#ifdef SISDUALHEAD
             if(pSiS->DualHeadMode) {
	        if(!pSiS->BlankCRT2) {
		   inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		   if(pSiS->sishw_ext.jChipType >= SIS_661) pSiS->LCDon &= 0x0f;
		}
	     } else
#endif
	     if(!pSiS->Blank) {
	        inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		if(pSiS->sishw_ext.jChipType >= SIS_661) pSiS->LCDon &= 0x0f;
	     }
          }
       }
    }

    switch (PowerManagementMode) {

       case DPMSModeOn:      /* HSync: On, VSync: On */
            if(docrt1)  pSiS->Blank = FALSE;
#ifdef SISDUALHEAD
	    else	pSiS->BlankCRT2 = FALSE;
#endif
            sr1   = 0x00;
            cr17  = 0x80;
	    pmreg = 0x00;
	    cr63  = 0x00;
	    sr7   = 0x10;
	    sr11  = (pSiS->LCDon & 0x0C);
	    p2_0  = 0x20;
	    p1_13 = 0x00;
	    backlight = TRUE;
            break;

       case DPMSModeSuspend: /* HSync: On, VSync: Off */
            if(docrt1)  pSiS->Blank = TRUE;
#ifdef SISDUALHEAD
	    else        pSiS->BlankCRT2 = TRUE;
#endif
            sr1   = 0x20;
	    cr17  = 0x80;
	    pmreg = 0x80;
	    cr63  = 0x40;
	    sr7   = 0x00;
	    sr11  = 0x08;
	    p2_0  = 0x40;
	    p1_13 = 0x80;
	    backlight = FALSE;
            break;

       case DPMSModeStandby: /* HSync: Off, VSync: On */
            if(docrt1)  pSiS->Blank = TRUE;
#ifdef SISDUALHEAD
	    else        pSiS->BlankCRT2 = TRUE;
#endif
            sr1   = 0x20;
	    cr17  = 0x80;
	    pmreg = 0x40;
	    cr63  = 0x40;
	    sr7   = 0x00;
	    sr11  = 0x08;
	    p2_0  = 0x80;
	    p1_13 = 0x40;
	    backlight = FALSE;
            break;

       case DPMSModeOff:     /* HSync: Off, VSync: Off */
            if(docrt1)  pSiS->Blank = TRUE;
#ifdef SISDUALHEAD
	    else        pSiS->BlankCRT2 = TRUE;
#endif
            sr1   = 0x20;
	    cr17  = 0x00;
	    pmreg = 0xc0;
	    cr63  = 0x40;
	    sr7   = 0x00;
	    sr11  = 0x08;
	    p2_0  = 0xc0;
	    p1_13 = 0xc0;
	    backlight = FALSE;
	    break;

       default:
	    return;
    }

    if(docrt2) {
       if(pSiS->VGAEngine == SIS_315_VGA) {
          if(pSiS->VBFlags & CRT2_LCD) {
	     if(pSiS->VBFlags & VB_CHRONTEL) {
	        if(backlight) {
	           SiS_Chrontel701xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	        } else {
	           SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
                }
	     }
          }
       }
    }

    if(pSiS->VBFlags & (VB_301LV|VB_302LV|VB_302ELV)) {
       if((docrt2 && (pSiS->VBFlags & CRT2_LCD)) || (docrt1 && (pSiS->VBFlags & CRT1_LCDA))) {
          if(backlight) {
	     SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	  } else {
             SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
	  }
       }
    }

    if(docrt1) {
       setSISIDXREG(SISSR, 0x01, ~0x20, sr1);    /* Set/Clear "Display On" bit */
       switch(pSiS->VGAEngine) {
       case SIS_OLD_VGA:
       case SIS_530_VGA:
            inSISIDXREG(SISSR, 0x11, oldpmreg);
            setSISIDXREG(SISCR, 0x17, 0x7f, cr17);
	    setSISIDXREG(SISSR, 0x11, 0x3f, pmreg);
	    break;
       case SIS_315_VGA:
            if((!pSiS->CRT1off) && ((!(pSiS->VBFlags & CRT1_LCDA)) || (pSiS->VBFlags & VB_301C))) {
               setSISIDXREG(SISCR, pSiS->myCR63, 0xbf, cr63);
	       setSISIDXREG(SISSR, 0x07, 0xef, sr7);
	    }
	    /* fall through */
       default:
            if((!(pSiS->VBFlags & CRT1_LCDA)) || (pSiS->VBFlags & VB_301C)) {
               inSISIDXREG(SISSR, 0x1f, oldpmreg);
               if(!pSiS->CRT1off) {
	          setSISIDXREG(SISSR, 0x1f, 0x3f, pmreg);
	       }
	    }
	    /* TODO: Check if Chrontel TV is active and in slave mode,
	     * don't go into power-saving mode this in this case!
	     */
       }
       oldpmreg &= 0xc0;
    }

    if(docrt2) {
       if(pSiS->VBFlags & CRT2_LCD) {
          if(((pSiS->VGAEngine == SIS_300_VGA) &&
              (pSiS->VBFlags & (VB_301|VB_30xBDH|VB_LVDS))) ||
             ((pSiS->VGAEngine == SIS_315_VGA) &&
              ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
	     if(pSiS->sishw_ext.jChipType >= SIS_661) {
	        setSISIDXREG(SISSR, 0x11, ~0xfc, sr11);
	     } else {
                setSISIDXREG(SISSR, 0x11, ~0x0c, sr11);
	     }
          }
          if(pSiS->VGAEngine == SIS_300_VGA) {
             if((pSiS->VBFlags & (VB_301B|VB_301C|VB_302B)) &&
                (!(pSiS->VBFlags & VB_30xBDH))) {
	        setSISIDXREG(SISPART1, 0x13, 0x3f, p1_13);
	     }
          } else if(pSiS->VGAEngine == SIS_315_VGA) {
             if((pSiS->VBFlags & (VB_301B|VB_301C|VB_302B)) &&
                (!(pSiS->VBFlags & VB_30xBDH))) {
	        setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
	     }
          }
       } else if(pSiS->VBFlags & CRT2_VGA) {
          if(pSiS->VBFlags & (VB_301B|VB_301C|VB_302B)) {
	     setSISIDXREG(SISPART2, 0x00, 0x1f, p2_0);
          }
       }
    }

    if((docrt1) && (pmreg != oldpmreg) && ((!(pSiS->VBFlags & CRT1_LCDA)) || (pSiS->VBFlags & VB_301C))) {
       outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
       usleep(10000);
       outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */
    }

}

/* Mandatory */
static void
SISIdentify(int flags)
{
    xf86PrintChipsets(SIS_NAME, "driver for SiS chipsets", SISChipsets);
}

#if 0
/* This won't work as long as noone added the symbols to the symlist */
static void
SISCalculateGammaRamp(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   int i, j, nramp;
   unsigned short *ramp[3];
   float gamma_max[3], gamma_prescale[3], framp;

   gamma_max[0] = (float)pSiS->GammaBriR / 1000;
   gamma_max[1] = (float)pSiS->GammaBriG / 1000;
   gamma_max[2] = (float)pSiS->GammaBriB / 1000;
   gamma_prescale[0] = (float)pSiS->GammaPBriR / 1000;
   gamma_prescale[1] = (float)pSiS->GammaPBriG / 1000;
   gamma_prescale[2] = (float)pSiS->GammaPBriB / 1000;

   if(!(nramp = xf86GetGammaRampSize(pScrn->pScreen))) return;

   for(i=0; i<3; i++) {
      ramp[i] = (unsigned short *)xalloc(nramp * sizeof(unsigned short));
      if(!ramp[i]) {
         if(ramp[0]) { xfree(ramp[0]); ramp[0] = NULL; }
	 if(ramp[1]) { xfree(ramp[1]); ramp[1] = NULL; }
         return;
      }
   }

   for(i = 0; i < 3; i++) {
      int fullscale = 65535 * gamma_max[i];
      float dramp = 1. / (nramp - 1);
      float invgamma=0.0, v;

      switch(i) {
      case 0: invgamma = 1. / pScrn->gamma.red; break;
      case 1: invgamma = 1. / pScrn->gamma.green; break;
      case 2: invgamma = 1. / pScrn->gamma.blue; break;
      }

      for(j = 0; j < nramp; j++) {
         framp = pow(gamma_prescale[i] * j * dramp, invgamma);

         v = (fullscale < 0) ? (65535 + fullscale * framp) :
	 		       fullscale * framp;
	 if(v < 0) v = 0;
	 else if(v > 65535) v = 65535;
	 ramp[i][j] = (unsigned short)v;
      }
   }

   xf86ChangeGammaRamp(pScrn->pScreen, nramp, ramp[0], ramp[1], ramp[2]);

   xfree(ramp[0]);
   xfree(ramp[1]);
   xfree(ramp[2]);
   ramp[0] = ramp[1] = ramp[2] = NULL;
}
#endif

static void
SISErrorLog(ScrnInfoPtr pScrn, const char *format, ...)
{
    va_list ap;
    static const char *str = "**************************************************\n";

    va_start(ap, format);
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, str);
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
    	"                      ERROR:\n");
    xf86VDrvMsgVerb(pScrn->scrnIndex, X_ERROR, 1, format, ap);
    va_end(ap);
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
    	"                  END OF MESSAGE\n");
    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, str);
}

/* Mandatory */
static Bool
SISProbe(DriverPtr drv, int flags)
{
    int     i;
    GDevPtr *devSections;
    int     *usedChips;
    int     numDevSections;
    int     numUsed;
    Bool    foundScreen = FALSE;

    /*
     * The aim here is to find all cards that this driver can handle,
     * and for the ones not already claimed by another driver, claim the
     * slot, and allocate a ScrnInfoRec.
     *
     * This should be a minimal probe, and it should under no circumstances
     * change the state of the hardware.  Because a device is found, don't
     * assume that it will be used.  Don't do any initialisations other than
     * the required ScrnInfoRec initialisations.  Don't allocate any new
     * data structures.
     *
     */

    /*
     * Next we check, if there has been a chipset override in the config file.
     * For this we must find out if there is an active device section which
     * is relevant, i.e., which has no driver specified or has THIS driver
     * specified.
     */

    if((numDevSections = xf86MatchDevice(SIS_DRIVER_NAME, &devSections)) <= 0) {
       /*
        * There's no matching device section in the config file, so quit
        * now.
        */
       return FALSE;
    }

    /*
     * We need to probe the hardware first.  We then need to see how this
     * fits in with what is given in the config file, and allow the config
     * file info to override any contradictions.
     */

    /*
     * All of the cards this driver supports are PCI, so the "probing" just
     * amounts to checking the PCI data that the server has already collected.
     */
    if(xf86GetPciVideoInfo() == NULL) {
       /*
        * We won't let anything in the config file override finding no
        * PCI video cards at all.  This seems reasonable now, but we'll see.
        */
       return FALSE;
    }

    numUsed = xf86MatchPciInstances(SIS_NAME, PCI_VENDOR_SIS,
               		SISChipsets, SISPciChipsets, devSections,
               		numDevSections, drv, &usedChips);

    /* Free it since we don't need that list after this */
    xfree(devSections);
    if(numUsed <= 0) return FALSE;

    if(flags & PROBE_DETECT) {
        foundScreen = TRUE;
    } else for(i = 0; i < numUsed; i++) {
        ScrnInfoPtr pScrn;
#ifdef SISDUALHEAD
	EntityInfoPtr pEnt;
#endif

        /* Allocate a ScrnInfoRec and claim the slot */
        pScrn = NULL;

        if((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
                                         SISPciChipsets, NULL, NULL,
                                         NULL, NULL, NULL))) {
            /* Fill in what we can of the ScrnInfoRec */
            pScrn->driverVersion    = SIS_CURRENT_VERSION;
            pScrn->driverName       = SIS_DRIVER_NAME;
            pScrn->name             = SIS_NAME;
            pScrn->Probe            = SISProbe;
            pScrn->PreInit          = SISPreInit;
            pScrn->ScreenInit       = SISScreenInit;
            pScrn->SwitchMode       = SISSwitchMode;
            pScrn->AdjustFrame      = SISAdjustFrame;
            pScrn->EnterVT          = SISEnterVT;
            pScrn->LeaveVT          = SISLeaveVT;
            pScrn->FreeScreen       = SISFreeScreen;
            pScrn->ValidMode        = SISValidMode;
            foundScreen = TRUE;
        }
#ifdef SISDUALHEAD
	pEnt = xf86GetEntityInfo(usedChips[i]);

	if(pEnt->chipset == PCI_CHIP_SIS630 || pEnt->chipset == PCI_CHIP_SIS540 ||
	   pEnt->chipset == PCI_CHIP_SIS650 || pEnt->chipset == PCI_CHIP_SIS550 ||
	   pEnt->chipset == PCI_CHIP_SIS315 || pEnt->chipset == PCI_CHIP_SIS315H ||
	   pEnt->chipset == PCI_CHIP_SIS315PRO || pEnt->chipset == PCI_CHIP_SIS330 ||
	   pEnt->chipset == PCI_CHIP_SIS300 || pEnt->chipset == PCI_CHIP_SIS660) {

	    SISEntPtr pSiSEnt = NULL;
	    DevUnion  *pPriv;

	    xf86SetEntitySharable(usedChips[i]);
	    if(SISEntityIndex < 0) {
	       SISEntityIndex = xf86AllocateEntityPrivateIndex();
	    }
	    pPriv = xf86GetEntityPrivate(pScrn->entityList[0], SISEntityIndex);
	    if(!pPriv->ptr) {
	       pPriv->ptr = xnfcalloc(sizeof(SISEntRec), 1);
	       pSiSEnt = pPriv->ptr;
	       pSiSEnt->lastInstance = -1;
	       pSiSEnt->DisableDual = FALSE;
	       pSiSEnt->ErrorAfterFirst = FALSE;
	       pSiSEnt->MapCountIOBase = pSiSEnt->MapCountFbBase = 0;
	       pSiSEnt->FbBase = pSiSEnt->IOBase = NULL;
  	       pSiSEnt->forceUnmapIOBase = FALSE;
	       pSiSEnt->forceUnmapFbBase = FALSE;
	       pSiSEnt->HWCursorCBufNum = pSiSEnt->HWCursorMBufNum = 0;
#ifdef __alpha__
	       pSiSEnt->MapCountIOBaseDense = 0;
	       pSiSEnt->IOBaseDense = NULL;
	       pSiSEnt->forceUnmapIOBaseDense = FALSE;
#endif
	    } else {
	       pSiSEnt = pPriv->ptr;
	    }
	    pSiSEnt->lastInstance++;
	    xf86SetEntityInstanceForScreen(pScrn, pScrn->entityList[0],
	                                   pSiSEnt->lastInstance);
	}
#endif
    }
    xfree(usedChips);
    return foundScreen;
}


/* If monitor section has no HSync/VRefresh data,
 * derive it from DDC data. Done by common layer
 * since 4.3.99.14.
 */
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,14,0)
static void
SiSSetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag)
{
   MonPtr      mon = pScrn->monitor;
   xf86MonPtr  ddc = mon->DDC;
   int         i,j;
   float       myhhigh, myhlow;
   int         myvhigh, myvlow;
   unsigned char temp;
   const myhddctiming myhtiming[11] = {
       { 1, 0x20, 31.6 }, /* rounded up by .1 */
       { 1, 0x02, 35.3 },
       { 1, 0x04, 37.6 },
       { 1, 0x08, 38.0 },
       { 1, 0x01, 38.0 },
       { 2, 0x40, 47.0 },
       { 2, 0x80, 48.2 },
       { 2, 0x08, 48.5 },
       { 2, 0x04, 56.6 },
       { 2, 0x02, 60.1 },
       { 2, 0x01, 80.1 }
   };
   const myvddctiming myvtiming[10] = {
       { 1, 0x02, 56 },
       { 1, 0x01, 60 },
       { 2, 0x08, 60 },
       { 2, 0x04, 70 },
       { 1, 0x08, 72 },
       { 2, 0x80, 72 },
       { 1, 0x04, 75 },
       { 2, 0x40, 75 },
       { 2, 0x02, 75 },
       { 2, 0x01, 75 }
   };
   /* "Future modes"; we only check the really high ones */
   const myddcstdmodes mystdmodes[8] = {
       { 1280, 1024, 85, 91.1  },
       { 1600, 1200, 60, 75.0  },
       { 1600, 1200, 65, 81.3  },
       { 1600, 1200, 70, 87.5  },
       { 1600, 1200, 75, 93.8  },
       { 1600, 1200, 85, 106.3 },
       { 1920, 1440, 60, 90.0  },
       { 1920, 1440, 75, 112.5 }
   };

   if(flag) { /* HSync */
      for(i = 0; i < 4; i++) {
    	 if(ddc->det_mon[i].type == DS_RANGES) {
            mon->nHsync = 1;
            mon->hsync[0].lo = ddc->det_mon[i].section.ranges.min_h;
            mon->hsync[0].hi = ddc->det_mon[i].section.ranges.max_h;
            return;
         }
      }
      /* If no sync ranges detected in detailed timing table, we
       * derive them from supported VESA modes. */
      myhlow = myhhigh = 0.0;
      for(i=0; i<11; i++) {
         if(myhtiming[i].whichone == 1) temp = ddc->timings1.t1;
	 else                           temp = ddc->timings1.t2;
	 if(temp & myhtiming[i].mask) {
	    if((i==0) || (myhlow > myhtiming[i].rate))
	            myhlow = myhtiming[i].rate;
	 }
	 if(myhtiming[10-i].whichone == 1) temp = ddc->timings1.t1;
	 else                              temp = ddc->timings1.t2;
	 if(temp & myhtiming[10-i].mask) {
	    if((i==0) || (myhhigh < myhtiming[10-i].rate))
	            myhhigh = myhtiming[10-i].rate;
	 }
      }
      for(i=0;i<STD_TIMINGS;i++) {
	 if(ddc->timings2[i].hsize > 256) {
            for(j=0; j<8; j++) {
	       if((ddc->timings2[i].hsize == mystdmodes[j].hsize) &&
	          (ddc->timings2[i].vsize == mystdmodes[j].vsize) &&
		  (ddc->timings2[i].refresh == mystdmodes[j].refresh)) {
		  if(mystdmodes[j].hsync > myhhigh)
		     myhhigh = mystdmodes[j].hsync;
	       }
	    }
	 }
      }
      if((myhhigh) && (myhlow)) {
         mon->nHsync = 1;
	 mon->hsync[0].lo = myhlow - 0.1;
	 mon->hsync[0].hi = myhhigh;
      }


   } else {  /* Vrefresh */

      for(i = 0; i < 4; i++) {
         if(ddc->det_mon[i].type == DS_RANGES) {
            mon->nVrefresh = 1;
            mon->vrefresh[0].lo = ddc->det_mon[i].section.ranges.min_v;
            mon->vrefresh[0].hi = ddc->det_mon[i].section.ranges.max_v;
            return;
         }
      }

      myvlow = myvhigh = 0;
      for(i=0; i<10; i++) {
         if(myvtiming[i].whichone == 1) temp = ddc->timings1.t1;
	 else                           temp = ddc->timings1.t2;
	 if(temp & myvtiming[i].mask) {
	    if((i==0) || (myvlow > myvtiming[i].rate))
	           myvlow = myvtiming[i].rate;
	 }
	 if(myvtiming[9-i].whichone == 1) temp = ddc->timings1.t1;
	 else                             temp = ddc->timings1.t2;
	 if(temp & myvtiming[9-i].mask) {
	    if((i==0) || (myvhigh < myvtiming[9-i].rate))
	           myvhigh = myvtiming[9-i].rate;
	 }
      }
      for(i=0;i<STD_TIMINGS;i++) {
	 if(ddc->timings2[i].hsize > 256) {
            for(j=0; j<8; j++) {
	       if((ddc->timings2[i].hsize == mystdmodes[j].hsize) &&
	          (ddc->timings2[i].vsize == mystdmodes[j].vsize) &&
		  (ddc->timings2[i].refresh == mystdmodes[j].refresh)) {
		  if(mystdmodes[j].refresh > myvhigh)
		     myvhigh = mystdmodes[j].refresh;
	       }
	    }
	 }
      }
      if((myvhigh) && (myvlow)) {
         mon->nVrefresh = 1;
	 mon->vrefresh[0].lo = myvlow;
	 mon->vrefresh[0].hi = myvhigh;
      }

    }
}
#endif

/* Some helper functions for MergedFB mode */

#ifdef SISMERGED

/* Helper function for CRT2 monitor vrefresh/hsync options
 * (Code base from mga driver)
 */
static int
SiSStrToRanges(range *r, char *s, int max)
{
   float num = 0.0;
   int rangenum = 0;
   Bool gotdash = FALSE;
   Bool nextdash = FALSE;
   char* strnum = NULL;
   do {
      switch(*s) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '.':
         if(strnum == NULL) {
            strnum = s;
            gotdash = nextdash;
            nextdash = FALSE;
         }
         break;
      case '-':
      case ' ':
      case 0:
         if(strnum == NULL) break;
         sscanf(strnum, "%f", &num);
	 strnum = NULL;
         if(gotdash) {
            r[rangenum - 1].hi = num;
         } else {
            r[rangenum].lo = num;
            r[rangenum].hi = num;
            rangenum++;
         }
         if(*s == '-') nextdash = (rangenum != 0);
	 else if(rangenum >= max) return rangenum;
         break;
      default:
         return 0;
      }

   } while(*(s++) != 0);

   return rangenum;
}

/* Copy and link two modes form mergedfb mode
 * (Code base taken from mga driver)
 * Copys mode i, links the result to dest, and returns it.
 * Links i and j in Private record.
 * If dest is NULL, return value is copy of i linked to itself.
 * For mergedfb auto-config, we only check the dimension
 * against virtualX/Y, if they were user-provided.
 */
static DisplayModePtr
SiSCopyModeNLink(ScrnInfoPtr pScrn, DisplayModePtr dest,
                 DisplayModePtr i, DisplayModePtr j,
		 SiSScrn2Rel srel)
{
    SISPtr pSiS = SISPTR(pScrn);
    DisplayModePtr mode;
    int dx = 0,dy = 0;

    if(!((mode = xalloc(sizeof(DisplayModeRec))))) return dest;
    memcpy(mode, i, sizeof(DisplayModeRec));
    if(!((mode->Private = xalloc(sizeof(SiSMergedDisplayModeRec))))) {
       xfree(mode);
       return dest;
    }
    ((SiSMergedDisplayModePtr)mode->Private)->CRT1 = i;
    ((SiSMergedDisplayModePtr)mode->Private)->CRT2 = j;
    ((SiSMergedDisplayModePtr)mode->Private)->CRT2Position = srel;
    mode->PrivSize = 0;

    switch(srel) {
    case sisLeftOf:
    case sisRightOf:
       if(!(pScrn->display->virtualX)) {
          dx = i->HDisplay + j->HDisplay;
       } else {
          dx = min(pScrn->virtualX, i->HDisplay + j->HDisplay);
       }
       dx -= mode->HDisplay;
       if(!(pScrn->display->virtualY)) {
          dy = max(i->VDisplay, j->VDisplay);
       } else {
          dy = min(pScrn->virtualY, max(i->VDisplay, j->VDisplay));
       }
       dy -= mode->VDisplay;
       break;
    case sisAbove:
    case sisBelow:
       if(!(pScrn->display->virtualY)) {
          dy = i->VDisplay + j->VDisplay;
       } else {
          dy = min(pScrn->virtualY, i->VDisplay + j->VDisplay);
       }
       dy -= mode->VDisplay;
       if(!(pScrn->display->virtualX)) {
          dx = max(i->HDisplay, j->HDisplay);
       } else {
          dx = min(pScrn->virtualX, max(i->HDisplay, j->HDisplay));
       }
       dx -= mode->HDisplay;
       break;
    case sisClone:
       if(!(pScrn->display->virtualX)) {
          dx = max(i->HDisplay, j->HDisplay);
       } else {
          dx = min(pScrn->virtualX, max(i->HDisplay, j->HDisplay));
       }
       dx -= mode->HDisplay;
       if(!(pScrn->display->virtualY)) {
          dy = max(i->VDisplay, j->VDisplay);
       } else {
	  dy = min(pScrn->virtualY, max(i->VDisplay, j->VDisplay));
       }
       dy -= mode->VDisplay;
       break;
    }
    mode->HDisplay += dx;
    mode->HSyncStart += dx;
    mode->HSyncEnd += dx;
    mode->HTotal += dx;
    mode->VDisplay += dy;
    mode->VSyncStart += dy;
    mode->VSyncEnd += dy;
    mode->VTotal += dy;
    mode->Clock = 0;

    if( ((mode->HDisplay * ((pScrn->bitsPerPixel + 7) / 8) * mode->VDisplay) > pSiS->maxxfbmem) ||
        (mode->HDisplay > 4088) ||
	(mode->VDisplay > 4096) ) {

       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
       		"Skipped %dx%d, not enough video RAM or beyond hardware specs\n",
		mode->HDisplay, mode->VDisplay);
       xfree(mode->Private);
       xfree(mode);

       return dest;
    }

#ifdef SISXINERAMA
    if(srel != sisClone) {
       pSiS->AtLeastOneNonClone = TRUE;
    }
#endif

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	"Merged %dx%d and %dx%d to %dx%d%s\n",
	i->HDisplay, i->VDisplay, j->HDisplay, j->VDisplay,
	mode->HDisplay, mode->VDisplay, (srel == sisClone) ? " (Clone)" : "");

    mode->next = mode;
    mode->prev = mode;

    if(dest) {
       mode->next = dest->next; 	/* Insert node after "dest" */
       dest->next->prev = mode;
       mode->prev = dest;
       dest->next = mode;
    }

    return mode;
}

/* Helper function to find a mode from a given name
 * (Code base taken from mga driver)
 */
static DisplayModePtr
SiSGetModeFromName(char* str, DisplayModePtr i)
{
    DisplayModePtr c = i;
    if(!i) return NULL;
    do {
       if(strcmp(str, c->name) == 0) return c;
       c = c->next;
    } while(c != i);
    return NULL;
}

static DisplayModePtr
SiSFindWidestTallestMode(DisplayModePtr i, Bool tallest)
{
    DisplayModePtr c = i, d = NULL;
    int max = 0;
    if(!i) return NULL;
    do {
       if(tallest) {
          if(c->VDisplay > max) {
             max = c->VDisplay;
	     d = c;
          }
       } else {
          if(c->HDisplay > max) {
             max = c->HDisplay;
	     d = c;
          }
       }
       c = c->next;
    } while(c != i);
    return d;
}

static DisplayModePtr
SiSGenerateModeListFromLargestModes(ScrnInfoPtr pScrn,
		    DisplayModePtr i, DisplayModePtr j,
		    SiSScrn2Rel srel)
{
#ifdef SISXINERAMA
    SISPtr pSiS = SISPTR(pScrn);
#endif
    DisplayModePtr mode1 = NULL;
    DisplayModePtr mode2 = NULL;
    DisplayModePtr result = NULL;

#ifdef SISXINERAMA
    pSiS->AtLeastOneNonClone = FALSE;
#endif

    switch(srel) {
    case sisLeftOf:
    case sisRightOf:
       mode1 = SiSFindWidestTallestMode(i, FALSE);
       mode2 = SiSFindWidestTallestMode(j, FALSE);
       break;
    case sisAbove:
    case sisBelow:
       mode1 = SiSFindWidestTallestMode(i, TRUE);
       mode2 = SiSFindWidestTallestMode(j, TRUE);
       break;
    case sisClone:
       mode1 = i;
       mode2 = j;
    }

    if(mode1 && mode2) {
       return(SiSCopyModeNLink(pScrn, result, mode1, mode2, srel));
    } else {
       return NULL;
    }
}

/* Generate the merged-fb mode modelist from metamodes
 * (Code base taken from mga driver)
 */
static DisplayModePtr
SiSGenerateModeListFromMetaModes(ScrnInfoPtr pScrn, char* str,
		    DisplayModePtr i, DisplayModePtr j,
		    SiSScrn2Rel srel)
{
#ifdef SISXINERAMA
    SISPtr pSiS = SISPTR(pScrn);
#endif
    char* strmode = str;
    char modename[256];
    Bool gotdash = FALSE;
    SiSScrn2Rel sr;
    DisplayModePtr mode1 = NULL;
    DisplayModePtr mode2 = NULL;
    DisplayModePtr result = NULL;

#ifdef SISXINERAMA
    pSiS->AtLeastOneNonClone = FALSE;
#endif

    do {
        switch(*str) {
        case 0:
        case '-':
        case ' ':
           if((strmode != str)) {

              strncpy(modename, strmode, str - strmode);
              modename[str - strmode] = 0;

              if(gotdash) {
                 if(mode1 == NULL) return NULL;
                 mode2 = SiSGetModeFromName(modename, j);
                 if(!mode2) {
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Mode \"%s\" is not a supported mode for CRT2\n", modename);
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Skipping metamode \"%s-%s\".\n", mode1->name, modename);
                    mode1 = NULL;
                 }
              } else {
                 mode1 = SiSGetModeFromName(modename, i);
                 if(!mode1) {
                    char* tmps = str;
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Mode \"%s\" is not a supported mode for CRT1\n", modename);
                    gotdash = FALSE;
                    while(*tmps == ' ') tmps++;
                    if(*tmps == '-') { 							/* skip the next mode */
                       tmps++;
                       while((*tmps == ' ') && (*tmps != 0)) tmps++; 			/* skip spaces */
                       while((*tmps != ' ') && (*tmps != '-') && (*tmps != 0)) tmps++; 	/* skip modename */
                       strncpy(modename,strmode,tmps - strmode);
                       modename[tmps - strmode] = 0;
                       str = tmps-1;
                    }
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Skipping metamode \"%s\".\n", modename);
                    mode1 = NULL;
                 }
              }
              gotdash = FALSE;
           }
           strmode = str + 1;
           gotdash |= (*str == '-');

           if(*str != 0) break;
	   /* Fall through otherwise */

        default:
           if(!gotdash && mode1) {
              sr = srel;
              if(!mode2) {
                 mode2 = SiSGetModeFromName(mode1->name, j);
                 sr = sisClone;
              }
              if(!mode2) {
                 xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                     "Mode: \"%s\" is not a supported mode for CRT2\n", mode1->name);
                 xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                     "Skipping metamode \"%s\".\n", modename);
                 mode1 = NULL;
              } else {
                 result = SiSCopyModeNLink(pScrn, result, mode1, mode2, sr);
                 mode1 = NULL;
                 mode2 = NULL;
              }
           }
           break;

        }

    } while(*(str++) != 0);

    return result;
}

static DisplayModePtr
SiSGenerateModeList(ScrnInfoPtr pScrn, char* str,
		    DisplayModePtr i, DisplayModePtr j,
		    SiSScrn2Rel srel)
{
   if(str != NULL) {
      return(SiSGenerateModeListFromMetaModes(pScrn, str, i, j, srel));
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
      	"No MetaModes given, linking %s modes by default\n",
	(srel == sisClone) ? "first" :
	   (((srel == sisLeftOf) || (srel == sisRightOf)) ? "widest" :  "tallest"));
      return(SiSGenerateModeListFromLargestModes(pScrn, i, j, srel));
   }
}

static void
SiSRecalcDefaultVirtualSize(ScrnInfoPtr pScrn)
{
    DisplayModePtr mode, bmode;
    int max;
    static const char *str = "MergedFB: Virtual %s %d\n";

    if(!(pScrn->display->virtualX)) {
       mode = bmode = pScrn->modes;
       max = 0;
       do {
          if(mode->HDisplay > max) max = mode->HDisplay;
          mode = mode->next;
       } while(mode != bmode);
       pScrn->virtualX = max;
       pScrn->displayWidth = max;
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, str, "width", max);
    }
    if(!(pScrn->display->virtualY)) {
       mode = bmode = pScrn->modes;
       max = 0;
       do {
          if(mode->VDisplay > max) max = mode->VDisplay;
          mode = mode->next;
       } while(mode != bmode);
       pScrn->virtualY = max;
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, str, "height", max);
    }
}

static void
SiSMergedFBSetDpi(ScrnInfoPtr pScrn1, ScrnInfoPtr pScrn2, SiSScrn2Rel srel)
{
   SISPtr pSiS = SISPTR(pScrn1);
   MessageType from = X_DEFAULT;
   xf86MonPtr DDC1 = (xf86MonPtr)(pScrn1->monitor->DDC);
   xf86MonPtr DDC2 = (xf86MonPtr)(pScrn2->monitor->DDC);
   int ddcWidthmm = 0, ddcHeightmm = 0;
   const char *dsstr = "MergedFB: Display dimensions: (%d, %d) mm\n";

   /* This sets the DPI for MergedFB mode. The problem is that
    * this can never be exact, because the output devices may
    * have different dimensions. This function tries to compromise
    * through a few assumptions, and it just calculates an average DPI
    * value for both monitors.
    */

   /* Given DisplaySize should regard BOTH monitors */
   pScrn1->widthmm = pScrn1->monitor->widthmm;
   pScrn1->heightmm = pScrn1->monitor->heightmm;

   /* Get DDC display size; if only either CRT1 or CRT2 provided these,
    * assume equal dimensions for both, otherwise add dimensions
    */
   if( (DDC1 && (DDC1->features.hsize > 0 && DDC1->features.vsize > 0)) &&
       (DDC2 && (DDC2->features.hsize > 0 && DDC2->features.vsize > 0)) ) {
      ddcWidthmm = max(DDC1->features.hsize, DDC2->features.hsize) * 10;
      ddcHeightmm = max(DDC1->features.vsize, DDC2->features.vsize) * 10;
      switch(srel) {
      case sisLeftOf:
      case sisRightOf:
         ddcWidthmm = (DDC1->features.hsize + DDC2->features.hsize) * 10;
	 break;
      case sisAbove:
      case sisBelow:
         ddcHeightmm = (DDC1->features.vsize + DDC2->features.vsize) * 10;
      default:
	 break;
      }
   } else if(DDC1 && (DDC1->features.hsize > 0 && DDC1->features.vsize > 0)) {
      ddcWidthmm = DDC1->features.hsize * 10;
      ddcHeightmm = DDC1->features.vsize * 10;
      switch(srel) {
      case sisLeftOf:
      case sisRightOf:
         ddcWidthmm *= 2;
	 break;
      case sisAbove:
      case sisBelow:
         ddcHeightmm *= 2;
      default:
	 break;
      }
   } else if(DDC2 && (DDC2->features.hsize > 0 && DDC2->features.vsize > 0) ) {
      ddcWidthmm = DDC2->features.hsize * 10;
      ddcHeightmm = DDC2->features.vsize * 10;
      switch(srel) {
      case sisLeftOf:
      case sisRightOf:
         ddcWidthmm *= 2;
	 break;
      case sisAbove:
      case sisBelow:
         ddcHeightmm *= 2;
      default:
	 break;
      }
   }

   if(monitorResolution > 0) {

      /* Set command line given values (overrules given options) */
      pScrn1->xDpi = monitorResolution;
      pScrn1->yDpi = monitorResolution;
      from = X_CMDLINE;

   } else if(pSiS->MergedFBXDPI) {

      /* Set option-wise given values (overrule DisplaySize) */
      pScrn1->xDpi = pSiS->MergedFBXDPI;
      pScrn1->yDpi = pSiS->MergedFBYDPI;
      from = X_CONFIG;

   } else if(pScrn1->widthmm > 0 || pScrn1->heightmm > 0) {

      /* Set values calculated from given DisplaySize */
      from = X_CONFIG;
      if(pScrn1->widthmm > 0) {
	 pScrn1->xDpi = (int)((double)pScrn1->virtualX * 25.4 / pScrn1->widthmm);
      }
      if(pScrn1->heightmm > 0) {
	 pScrn1->yDpi = (int)((double)pScrn1->virtualY * 25.4 / pScrn1->heightmm);
      }
      xf86DrvMsg(pScrn1->scrnIndex, from, dsstr, pScrn1->widthmm, pScrn1->heightmm);

    } else if(ddcWidthmm && ddcHeightmm) {

      /* Set values from DDC-provided display size */
      from = X_PROBED;
      xf86DrvMsg(pScrn1->scrnIndex, from, dsstr, ddcWidthmm, ddcHeightmm );
      pScrn1->widthmm = ddcWidthmm;
      pScrn1->heightmm = ddcHeightmm;
      if(pScrn1->widthmm > 0) {
	 pScrn1->xDpi = (int)((double)pScrn1->virtualX * 25.4 / pScrn1->widthmm);
      }
      if(pScrn1->heightmm > 0) {
	 pScrn1->yDpi = (int)((double)pScrn1->virtualY * 25.4 / pScrn1->heightmm);
      }

    } else {

      pScrn1->xDpi = pScrn1->yDpi = DEFAULT_DPI;

    }

    /* Sanity check */
    if(pScrn1->xDpi > 0 && pScrn1->yDpi <= 0)
       pScrn1->yDpi = pScrn1->xDpi;
    if(pScrn1->yDpi > 0 && pScrn1->xDpi <= 0)
       pScrn1->xDpi = pScrn1->yDpi;

    pScrn2->xDpi = pScrn1->xDpi;
    pScrn2->yDpi = pScrn1->yDpi;

    xf86DrvMsg(pScrn1->scrnIndex, from, "MergedFB: DPI set to (%d, %d)\n",
	       pScrn1->xDpi, pScrn1->yDpi);
}

/* Pseudo-Xinerama extension for MergedFB mode */
#ifdef SISXINERAMA

static void
SiSUpdateXineramaScreenInfo(ScrnInfoPtr pScrn1)
{
    SISPtr pSiS = SISPTR(pScrn1);
    int crt1scrnnum = 0, crt2scrnnum = 1;
    int x1=0, x2=0, y1=0, y2=0, h1=0, h2=0, w1=0, w2=0;
    DisplayModePtr currentMode, firstMode;
    Bool infochanged = FALSE;

    if(!pSiS->MergedFB) return;

    if(SiSnoPanoramiXExtension) return;

    if(!SiSXineramadataPtr) return;

    if(pSiS->CRT2IsScrn0) {
       crt1scrnnum = 1;
       crt2scrnnum = 0;
    }

    /* Attention: Usage of RandR may lead into virtual X and Y values
     * actually smaller than our MetaModes! To avoid this, we calculate
     * the maxCRT fields here (and not somewhere else, like in CopyNLink)
     *
     * *** For now: RandR will be disabled if SiS pseudo-Xinerama is on
     */

    if((pSiS->SiSXineramaVX != pScrn1->virtualX) || (pSiS->SiSXineramaVY != pScrn1->virtualY)) {

       if(!(pScrn1->modes)) return;

       pSiS->maxCRT1_X1 = pSiS->maxCRT1_X2 = 0;
       pSiS->maxCRT1_Y1 = pSiS->maxCRT1_Y2 = 0;
       pSiS->maxCRT2_X1 = pSiS->maxCRT2_X2 = 0;
       pSiS->maxCRT2_Y1 = pSiS->maxCRT2_Y2 = 0;
       pSiS->maxClone_X1 = pSiS->maxClone_X2 = 0;
       pSiS->maxClone_Y1 = pSiS->maxClone_Y2 = 0;

       currentMode = firstMode = pScrn1->modes;

       do {

          DisplayModePtr p = currentMode->next;
          DisplayModePtr i = ((SiSMergedDisplayModePtr)currentMode->Private)->CRT1;
          DisplayModePtr j = ((SiSMergedDisplayModePtr)currentMode->Private)->CRT2;
          SiSScrn2Rel srel = ((SiSMergedDisplayModePtr)currentMode->Private)->CRT2Position;

          if((i->HDisplay <= pScrn1->virtualX) && (j->HDisplay <= pScrn1->virtualX) &&
             (i->VDisplay <= pScrn1->virtualY) && (j->VDisplay <= pScrn1->virtualY)) {

             if(srel != sisClone) {
                if(pSiS->maxCRT1_X1 <= i->HDisplay) {
                   pSiS->maxCRT1_X1 = i->HDisplay;      /* Largest CRT1 mode */
                   if(pSiS->maxCRT1_X2 < j->HDisplay) {
                      pSiS->maxCRT1_X2 = j->HDisplay;   /* Largest X of CRT2 mode displayed with largest CRT1 mode */
                   }
                }
                if(pSiS->maxCRT2_X2 <= j->HDisplay) {
                   pSiS->maxCRT2_X2 = j->HDisplay;      /* Largest CRT2 mode */
                   if(pSiS->maxCRT2_X1 < i->HDisplay) {
                      pSiS->maxCRT2_X1 = i->HDisplay;   /* Largest X of CRT1 mode displayed with largest CRT2 mode */
                   }
                }
                if(pSiS->maxCRT1_Y1 <= i->VDisplay) {
                   pSiS->maxCRT1_Y1 = i->VDisplay;
                   if(pSiS->maxCRT1_Y2 < j->VDisplay) {
                      pSiS->maxCRT1_Y2 = j->VDisplay;
                   }
                }
                if(pSiS->maxCRT2_Y2 <= j->VDisplay) {
                   pSiS->maxCRT2_Y2 = j->VDisplay;
                   if(pSiS->maxCRT2_Y1 < i->VDisplay) {
                      pSiS->maxCRT2_Y1 = i->VDisplay;
                   }
                }
             } else {
                if(pSiS->maxClone_X1 < i->HDisplay) {
                   pSiS->maxClone_X1 = i->HDisplay;
                }
                if(pSiS->maxClone_X2 < j->HDisplay) {
                   pSiS->maxClone_X2 = j->HDisplay;
                }
                if(pSiS->maxClone_Y1 < i->VDisplay) {
                   pSiS->maxClone_Y1 = i->VDisplay;
                }
                if(pSiS->maxClone_Y2 < j->VDisplay) {
                   pSiS->maxClone_Y2 = j->VDisplay;
                }
             }
          }
          currentMode = p;

       } while((currentMode) && (currentMode != firstMode));

       pSiS->SiSXineramaVX = pScrn1->virtualX;
       pSiS->SiSXineramaVY = pScrn1->virtualY;
       infochanged = TRUE;

    }

    switch(pSiS->CRT2Position) {
    case sisLeftOf:
       x1 = min(pSiS->maxCRT1_X2, pScrn1->virtualX - pSiS->maxCRT1_X1);
       if(x1 < 0) x1 = 0;
       y1 = 0;
       w1 = pScrn1->virtualX - x1;
       h1 = pScrn1->virtualY;
       x2 = 0;
       y2 = 0;
       w2 = max(pSiS->maxCRT2_X2, pScrn1->virtualX - pSiS->maxCRT2_X1);
       if(w2 > pScrn1->virtualX) w2 = pScrn1->virtualX;
       h2 = pScrn1->virtualY;
       break;
    case sisRightOf:
       x1 = 0;
       y1 = 0;
       w1 = max(pSiS->maxCRT1_X1, pScrn1->virtualX - pSiS->maxCRT1_X2);
       if(w1 > pScrn1->virtualX) w1 = pScrn1->virtualX;
       h1 = pScrn1->virtualY;
       x2 = min(pSiS->maxCRT2_X1, pScrn1->virtualX - pSiS->maxCRT2_X2);
       if(x2 < 0) x2 = 0;
       y2 = 0;
       w2 = pScrn1->virtualX - x2;
       h2 = pScrn1->virtualY;
       break;
    case sisAbove:
       x1 = 0;
       y1 = min(pSiS->maxCRT1_Y2, pScrn1->virtualY - pSiS->maxCRT1_Y1);
       if(y1 < 0) y1 = 0;
       w1 = pScrn1->virtualX;
       h1 = pScrn1->virtualY - y1;
       x2 = 0;
       y2 = 0;
       w2 = pScrn1->virtualX;
       h2 = max(pSiS->maxCRT2_Y2, pScrn1->virtualY - pSiS->maxCRT2_Y1);
       if(h2 > pScrn1->virtualY) h2 = pScrn1->virtualY;
       break;
    case sisBelow:
       x1 = 0;
       y1 = 0;
       w1 = pScrn1->virtualX;
       h1 = max(pSiS->maxCRT1_Y1, pScrn1->virtualY - pSiS->maxCRT1_Y2);
       if(h1 > pScrn1->virtualY) h1 = pScrn1->virtualY;
       x2 = 0;
       y2 = min(pSiS->maxCRT2_Y1, pScrn1->virtualY - pSiS->maxCRT2_Y2);
       if(y2 < 0) y2 = 0;
       w2 = pScrn1->virtualX;
       h2 = pScrn1->virtualY - y2;
    default:
       break;
    }

    SiSXineramadataPtr[crt1scrnnum].x = x1;
    SiSXineramadataPtr[crt1scrnnum].y = y1;
    SiSXineramadataPtr[crt1scrnnum].width = w1;
    SiSXineramadataPtr[crt1scrnnum].height = h1;
    SiSXineramadataPtr[crt2scrnnum].x = x2;
    SiSXineramadataPtr[crt2scrnnum].y = y2;
    SiSXineramadataPtr[crt2scrnnum].width = w2;
    SiSXineramadataPtr[crt2scrnnum].height = h2;

    if(infochanged) {
       xf86DrvMsg(pScrn1->scrnIndex, X_INFO,
          "Pseudo-Xinerama: CRT1 (Screen %d) (%d,%d)-(%d,%d)\n",
          crt1scrnnum, x1, y1, w1+x1-1, h1+y1-1);
       xf86DrvMsg(pScrn1->scrnIndex, X_INFO,
          "Pseudo-Xinerama: CRT2 (Screen %d) (%d,%d)-(%d,%d)\n",
          crt2scrnnum, x2, y2, w2+x2-1, h2+y2-1);
    }
}

/* Proc */

int
SiSProcXineramaQueryVersion(ClientPtr client)
{
    xPanoramiXQueryVersionReply	  rep;
    register int		  n;

    REQUEST_SIZE_MATCH(xPanoramiXQueryVersionReq);
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion = SIS_XINERAMA_MAJOR_VERSION;
    rep.minorVersion = SIS_XINERAMA_MINOR_VERSION;
    if(client->swapped) {
        swaps(&rep.sequenceNumber, n);
        swapl(&rep.length, n);
        swaps(&rep.majorVersion, n);
        swaps(&rep.minorVersion, n);
    }
    WriteToClient(client, sizeof(xPanoramiXQueryVersionReply), (char *)&rep);
    return (client->noClientException);
}

int
SiSProcXineramaGetState(ClientPtr client)
{
    REQUEST(xPanoramiXGetStateReq);
    WindowPtr			pWin;
    xPanoramiXGetStateReply	rep;
    register int		n;

    REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
    pWin = LookupWindow(stuff->window, client);
    if(!pWin) return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = !SiSnoPanoramiXExtension;
    if(client->swapped) {
       swaps (&rep.sequenceNumber, n);
       swapl (&rep.length, n);
       swaps (&rep.state, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetStateReply), (char *)&rep);
    return client->noClientException;
}

int
SiSProcXineramaGetScreenCount(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenCountReq);
    WindowPtr				pWin;
    xPanoramiXGetScreenCountReply	rep;
    register int			n;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
    pWin = LookupWindow(stuff->window, client);
    if(!pWin) return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.ScreenCount = SiSXineramaNumScreens;
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.ScreenCount, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenCountReply), (char *)&rep);
    return client->noClientException;
}

int
SiSProcXineramaGetScreenSize(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenSizeReq);
    WindowPtr				pWin;
    xPanoramiXGetScreenSizeReply	rep;
    register int			n;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
    pWin = LookupWindow (stuff->window, client);
    if(!pWin)  return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.width  = SiSXineramadataPtr[stuff->screen].width;
    rep.height = SiSXineramadataPtr[stuff->screen].height;
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.width, n);
       swaps(&rep.height, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenSizeReply), (char *)&rep);
    return client->noClientException;
}

int
SiSProcXineramaIsActive(ClientPtr client)
{
    xXineramaIsActiveReply	rep;

    REQUEST_SIZE_MATCH(xXineramaIsActiveReq);

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = !SiSnoPanoramiXExtension;
    if(client->swapped) {
	register int n;
	swaps(&rep.sequenceNumber, n);
	swapl(&rep.length, n);
	swapl(&rep.state, n);
    }
    WriteToClient(client, sizeof(xXineramaIsActiveReply), (char *) &rep);
    return client->noClientException;
}

int
SiSProcXineramaQueryScreens(ClientPtr client)
{
    xXineramaQueryScreensReply	rep;

    REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);

    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.number = (SiSnoPanoramiXExtension) ? 0 : SiSXineramaNumScreens;
    rep.length = rep.number * sz_XineramaScreenInfo >> 2;
    if(client->swapped) {
       register int n;
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swapl(&rep.number, n);
    }
    WriteToClient(client, sizeof(xXineramaQueryScreensReply), (char *)&rep);

    if(!SiSnoPanoramiXExtension) {
       xXineramaScreenInfo scratch;
       int i;

       for(i = 0; i < SiSXineramaNumScreens; i++) {
	  scratch.x_org  = SiSXineramadataPtr[i].x;
	  scratch.y_org  = SiSXineramadataPtr[i].y;
	  scratch.width  = SiSXineramadataPtr[i].width;
	  scratch.height = SiSXineramadataPtr[i].height;
	  if(client->swapped) {
	     register int n;
	     swaps(&scratch.x_org, n);
	     swaps(&scratch.y_org, n);
	     swaps(&scratch.width, n);
    	     swaps(&scratch.height, n);
	  }
	  WriteToClient(client, sz_XineramaScreenInfo, (char *)&scratch);
       }
    }

    return client->noClientException;
}

static int
SiSProcXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data) {
	case X_PanoramiXQueryVersion:
	     return SiSProcXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return SiSProcXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return SiSProcXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return SiSProcXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return SiSProcXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return SiSProcXineramaQueryScreens(client);
    }
    return BadRequest;
}

/* SProc */

static int
SiSSProcXineramaQueryVersion (ClientPtr client)
{
    REQUEST(xPanoramiXQueryVersionReq);
    register int n;
    swaps(&stuff->length,n);
    REQUEST_SIZE_MATCH (xPanoramiXQueryVersionReq);
    return SiSProcXineramaQueryVersion(client);
}

static int
SiSSProcXineramaGetState(ClientPtr client)
{
    REQUEST(xPanoramiXGetStateReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
    return SiSProcXineramaGetState(client);
}

static int
SiSSProcXineramaGetScreenCount(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenCountReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
    return SiSProcXineramaGetScreenCount(client);
}

static int
SiSSProcXineramaGetScreenSize(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenSizeReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
    return SiSProcXineramaGetScreenSize(client);
}

static int
SiSSProcXineramaIsActive(ClientPtr client)
{
    REQUEST(xXineramaIsActiveReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xXineramaIsActiveReq);
    return SiSProcXineramaIsActive(client);
}

static int
SiSSProcXineramaQueryScreens(ClientPtr client)
{
    REQUEST(xXineramaQueryScreensReq);
    register int n;
    swaps (&stuff->length, n);
    REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);
    return SiSProcXineramaQueryScreens(client);
}

int
SiSSProcXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data) {
	case X_PanoramiXQueryVersion:
	     return SiSSProcXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return SiSSProcXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return SiSSProcXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return SiSSProcXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return SiSSProcXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return SiSSProcXineramaQueryScreens(client);
    }
    return BadRequest;
}

static void
SiSXineramaResetProc(ExtensionEntry* extEntry)
{
    /* Called by CloseDownExtensions() */
    if(SiSXineramadataPtr) {
       Xfree(SiSXineramadataPtr);
       SiSXineramadataPtr = NULL;
    }
}

static void
SiSXineramaExtensionInit(ScrnInfoPtr pScrn)
{
    SISPtr    	pSiS = SISPTR(pScrn);
    Bool	success = FALSE;

    if(!(SiSXineramadataPtr)) {

       if(!pSiS->MergedFB) {
          SiSnoPanoramiXExtension = TRUE;
          return;
       }

#ifdef PANORAMIX
       if(!noPanoramiXExtension) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Xinerama active, not initializing SiS Pseudo-Xinerama\n");
          SiSnoPanoramiXExtension = TRUE;
          return;
       }
#endif

       if(SiSnoPanoramiXExtension) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	      "SiS Pseudo-Xinerama disabled\n");
          return;
       }

       if(pSiS->CRT2Position == sisClone) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Running MergedFB in Clone mode, SiS Pseudo-Xinerama disabled\n");
          SiSnoPanoramiXExtension = TRUE;
          return;
       }

       if(!(pSiS->AtLeastOneNonClone)) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Only Clone modes defined, SiS Pseudo-Xinerama disabled\n");
          SiSnoPanoramiXExtension = TRUE;
          return;
       }

       SiSXineramaNumScreens = 2;

       while(SiSXineramaGeneration != serverGeneration) {

	  pSiS->XineramaExtEntry = AddExtension(PANORAMIX_PROTOCOL_NAME, 0,0,
					SiSProcXineramaDispatch,
					SiSSProcXineramaDispatch,
					SiSXineramaResetProc,
					StandardMinorOpcode);

	  if(!pSiS->XineramaExtEntry) break;

	  if(!(SiSXineramadataPtr = (SiSXineramaData *)
	        xcalloc(SiSXineramaNumScreens, sizeof(SiSXineramaData)))) break;

	  SiSXineramaGeneration = serverGeneration;
	  success = TRUE;
       }

       if(!success) {
          SISErrorLog(pScrn, "Failed to initialize SiS Pseudo-Xinerama extension\n");
          SiSnoPanoramiXExtension = TRUE;
          return;
       }

       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	  "SiS Pseudo-Xinerama extension initialized\n");

       pSiS->SiSXineramaVX = 0;
       pSiS->SiSXineramaVY = 0;

    }

    SiSUpdateXineramaScreenInfo(pScrn);

}
#endif  /* End of PseudoXinerama */

static void
SiSFreeCRT2Structs(SISPtr pSiS)
{
    if(pSiS->CRT2pScrn) {
       if(pSiS->CRT2pScrn->modes) {
          while(pSiS->CRT2pScrn->modes)
  	     xf86DeleteMode(&pSiS->CRT2pScrn->modes, pSiS->CRT2pScrn->modes);
       }
       if(pSiS->CRT2pScrn->monitor) {
          if(pSiS->CRT2pScrn->monitor->Modes) {
             while(pSiS->CRT2pScrn->monitor->Modes)
  	        xf86DeleteMode(&pSiS->CRT2pScrn->monitor->Modes, pSiS->CRT2pScrn->monitor->Modes);
	  }
	  if(pSiS->CRT2pScrn->monitor->DDC) xfree(pSiS->CRT2pScrn->monitor->DDC);
	  xfree(pSiS->CRT2pScrn->monitor);
       }
       xfree(pSiS->CRT2pScrn);
       pSiS->CRT2pScrn = NULL;
   }
}

#endif	/* End of MergedFB helpers */

static xf86MonPtr
SiSInternalDDC(ScrnInfoPtr pScrn, int crtno)
{
   SISPtr        pSiS = SISPTR(pScrn);
   USHORT        temp = 0xffff, temp1, i, realcrtno = crtno;
   unsigned char buffer[256];
   xf86MonPtr    pMonitor = NULL;

   /* If CRT1 is off, skip DDC */
   if((pSiS->CRT1off) && (!crtno)) return NULL;

   if(crtno) {
      if(pSiS->VBFlags & CRT2_LCD)      realcrtno = 1;
      else if(pSiS->VBFlags & CRT2_VGA) realcrtno = 2;
      else return NULL;
   } else {
      /* If CRT1 is LCDA, skip DDC (except 301C: DDC allowed, but uses CRT2 port!) */
      if(pSiS->VBFlags & CRT1_LCDA) {
         if(pSiS->VBFlags & VB_301C)    realcrtno = 1;
         else return NULL;
      }
   }

   i = 3; /* Number of retrys */
   do {
      temp1 = SiS_HandleDDC(pSiS->SiS_Pr, pSiS->VBFlags, pSiS->VGAEngine, realcrtno, 0, &buffer[0]);
      if((temp1) && (temp1 != 0xffff)) temp = temp1;
   } while((temp == 0xffff) && i--);
   if(temp != 0xffff) {
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT%d DDC supported\n", crtno + 1);
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT%d DDC level: %s%s%s%s\n",
	     crtno + 1,
	     (temp & 0x1a) ? "" : "[none of the supported]",
	     (temp & 0x02) ? "2 " : "",
	     (temp & 0x08) ? "D&P" : "",
             (temp & 0x10) ? "FPDI-2" : "");
      if(temp & 0x02) {
	 i = 5;  /* Number of retrys */
	 do {
	    temp = SiS_HandleDDC(pSiS->SiS_Pr, pSiS->VBFlags, pSiS->VGAEngine, realcrtno, 1, &buffer[0]);
	 } while((temp) && i--);
         if(!temp) {
	    if((pMonitor = xf86InterpretEDID(pScrn->scrnIndex, &buffer[0]))) {
	       return(pMonitor);
	    } else {
	       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	           "CRT%d DDC EDID corrupt\n", crtno + 1);
	       return(NULL);
	    }
	 } else {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    	"CRT%d DDC reading failed\n", crtno + 1);
	    return(NULL);
	 }
      } else if(!crtno) {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "DDC for VESA D&P and FPDI-2 not supported for CRT1.\n");
         return(NULL);
      } else if(temp & 0x18) {
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "DDC for VESA D&P and FPDI-2 not supported for CRT2 yet.\n");
         return(NULL);
      } 
      return(NULL);
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                "CRT%d DDC probing failed\n", crtno + 1);
      return(NULL);
   }
}

static xf86MonPtr
SiSDoPrivateDDC(ScrnInfoPtr pScrn, int *crtnum)
{
    SISPtr pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
          *crtnum = 1;
	  return(SiSInternalDDC(pScrn, 0));
       } else {
          *crtnum = 2;
	  return(SiSInternalDDC(pScrn, 1));
       }
    } else
#endif
    if(pSiS->CRT1off) {
       *crtnum = 2;
       return(SiSInternalDDC(pScrn, 1));
    } else {
       *crtnum = 1;
       return(SiSInternalDDC(pScrn, 0));
    }
}

static BOOLEAN
SiSMakeOwnModeList(ScrnInfoPtr pScrn, BOOLEAN acceptcustommodes, BOOLEAN includelcdmodes,
                   BOOLEAN isfordvi, BOOLEAN *havecustommodes)
{
    DisplayModePtr tempmode, delmode, mymodes;

    if((mymodes = SiSBuildBuiltInModeList(pScrn, includelcdmodes, isfordvi))) {
       if(!acceptcustommodes) {
	  while(pScrn->monitor->Modes)
             xf86DeleteMode(&pScrn->monitor->Modes, pScrn->monitor->Modes);
	  pScrn->monitor->Modes = mymodes;
       } else {
	  delmode = pScrn->monitor->Modes;
	  while(delmode) {
	     if(delmode->type & M_T_DEFAULT) {
	        tempmode = delmode->next;
	        xf86DeleteMode(&pScrn->monitor->Modes, delmode);
	        delmode = tempmode;
	     } else {
	        delmode = delmode->next;
	     }
	  }
	  tempmode = pScrn->monitor->Modes;
	  if(tempmode) *havecustommodes = TRUE;
	  pScrn->monitor->Modes = mymodes;
	  while(mymodes) {
	     if(!mymodes->next) break;
	     else mymodes = mymodes->next;
	  }
	  mymodes->next = tempmode;
	  if(tempmode) {
	     tempmode->prev = mymodes;
	  }
       }
       return TRUE;
    } else
       return FALSE;
}

/* Mandatory */
static Bool
SISPreInit(ScrnInfoPtr pScrn, int flags)
{
    SISPtr pSiS;
    MessageType from;
    unsigned char usScratchCR17, CR5F;
    unsigned char usScratchCR32, usScratchCR63;
    unsigned char usScratchSR1F;
    unsigned long int i;
    int temp;
    ClockRangePtr clockRanges;
    int pix24flags;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif
#if defined(SISMERGED) || defined(SISDUALHEAD)
    DisplayModePtr first, p, n;
#endif
    unsigned char srlockReg,crlockReg;
    unsigned char tempreg;
    xf86MonPtr pMonitor = NULL;
    Bool didddc2;

    vbeInfoPtr pVbe;
    VbeInfoBlock *vbe;

    static const char *ddcsstr = "CRT%d DDC monitor info: ************************************\n";
    static const char *ddcestr = "End of CRT%d DDC monitor info ******************************\n";
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,14,0)
    static const char *subshstr = "Substituting missing CRT%d monitor HSync data by DDC data\n";
    static const char *subsvstr = "Substituting missing CRT%d monitor VRefresh data by DDC data\n";
#endif
#ifdef SISMERGED
    static const char *mergednocrt1 = "CRT1 not detected or forced off. %s.\n";
    static const char *mergednocrt2 = "No CRT2 output selected or no bridge detected. %s.\n";
    static const char *mergeddisstr = "MergedFB mode disabled";
    static const char *modesforstr = "Modes for CRT%d: *********************************************\n";
    static const char *crtsetupstr = "------------------------ CRT%d setup -------------------------\n";
#endif
#if defined(SISDUALHEAD) || defined(SISMERGED)
    static const char *notsuitablestr = "Not using mode \"%s\" (not suitable for %s mode)\n";
#endif

    if(flags & PROBE_DETECT) {
       if(xf86LoadSubModule(pScrn, "vbe")) {
          int index = xf86GetEntityInfo(pScrn->entityList[0])->index;

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	  if((pVbe = VBEInit(NULL,index))) {
#else
          if((pVbe = VBEExtendedInit(NULL,index,0))) {
#endif
             ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
             vbeFree(pVbe);
          }
       }
       return TRUE;
    }

    /*
     * Note: This function is only called once at server startup, and
     * not at the start of each server generation.  This means that
     * only things that are persistent across server generations can
     * be initialised here.  xf86Screens[] is the array of all screens,
     * (pScrn is a pointer to one of these).  Privates allocated using
     * xf86AllocateScrnInfoPrivateIndex() are too, and should be used
     * for data that must persist across server generations.
     *
     * Per-generation data should be allocated with
     * AllocateScreenPrivateIndex() from the ScreenInit() function.
     */

    /* Check the number of entities, and fail if it isn't one. */
    if(pScrn->numEntities != 1) {
       SISErrorLog(pScrn, "Number of entities is not 1\n");
       return FALSE;
    }

    /* The vgahw module should be loaded here when needed */
    if(!xf86LoadSubModule(pScrn, "vgahw")) {
       SISErrorLog(pScrn, "Could not load vgahw module\n");
       return FALSE;
    }

    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /* Due to the liberal license terms this is needed for
     * keeping the copyright notice readable and intact in
     * binary distributions. Removing this is a copyright
     * infringement. Please read the license terms above.
     */

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
        "SiS driver (%d/%02d/%02d-%d)\n",
	SISDRIVERVERSIONYEAR + 2000, SISDRIVERVERSIONMONTH,
	SISDRIVERVERSIONDAY, SISDRIVERREVISION);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	"Copyright (C) 2001-2004 Thomas Winischhofer <thomas@winischhofer.net> and others\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
        "Compiled for XFree86 %d.%d.%d.%d\n",
	XF86_VERSION_MAJOR, XF86_VERSION_MINOR,
	XF86_VERSION_PATCH, XF86_VERSION_SNAP);
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
    if(xf86GetVersion() != XF86_VERSION_CURRENT) {
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
         "This version of the driver is not compiled for this version of XFree86!\n");
    }
#endif
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
        "See http://www.winischhofer.net/linuxsisvga.shtml "
	"for documentation and updates\n");

    /* Allocate a vgaHWRec */
    if(!vgaHWGetHWRec(pScrn)) {
       SISErrorLog(pScrn, "Could not allocate VGA private\n");
       return FALSE;
    }

    /* Allocate the SISRec driverPrivate */
    if(!SISGetRec(pScrn)) {
       SISErrorLog(pScrn, "Could not allocate memory for pSiS private\n");
       return FALSE;
    }
    pSiS = SISPTR(pScrn);
    pSiS->pScrn = pScrn;

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    pSiS->IODBase = 0;
#else
    pSiS->IODBase = pScrn->domainIOBase;  
#endif

    /* Get the entity, and make sure it is PCI. */
    pSiS->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if(pSiS->pEnt->location.type != BUS_PCI)  {
       SISErrorLog(pScrn, "Entity's bus type is not PCI\n");
       SISFreeRec(pScrn);
       return FALSE;
    }

#ifdef SISDUALHEAD
    /* Allocate an entity private if necessary */
    if(xf86IsEntityShared(pScrn->entityList[0])) {
       pSiSEnt = xf86GetEntityPrivate(pScrn->entityList[0],
					SISEntityIndex)->ptr;
       pSiS->entityPrivate = pSiSEnt;

       /* If something went wrong, quit here */
       if((pSiSEnt->DisableDual) || (pSiSEnt->ErrorAfterFirst)) {
	  SISErrorLog(pScrn, "First head encountered fatal error, can't continue\n");
	  SISFreeRec(pScrn);
	  return FALSE;
       }
    }
#endif

    /* Find the PCI info for this screen */
    pSiS->PciInfo = xf86GetPciInfoForEntity(pSiS->pEnt->index);
    pSiS->PciTag = pSiS->sishw_ext.PciTag = pciTag(pSiS->PciInfo->bus,
                           pSiS->PciInfo->device, pSiS->PciInfo->func);

    pSiS->Primary = xf86IsPrimaryPci(pSiS->PciInfo);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
    	"This adapter is %s display adapter\n",
	(pSiS->Primary ? "primary" : "secondary"));

    if(pSiS->Primary) {
       VGAHWPTR(pScrn)->MapSize = 0x10000;     /* Standard 64k VGA window */
       if(!vgaHWMapMem(pScrn)) {
          SISErrorLog(pScrn, "Could not map VGA memory\n");
          SISFreeRec(pScrn);
          return FALSE;
       }
    }
    vgaHWGetIOBase(VGAHWPTR(pScrn));

    /* We "patch" the PIOOffset inside vgaHW in order to force
     * the vgaHW module to use our relocated i/o ports.
     */
    VGAHWPTR(pScrn)->PIOOffset = pSiS->IODBase + (pSiS->PciInfo->ioBase[2] & 0xFFFC) - 0x380;

    pSiS->pInt = NULL;
    if(!pSiS->Primary) {
#if !defined(__alpha__)
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       		"Initializing display adapter through int10\n");
#endif
       if(xf86LoadSubModule(pScrn, "int10")) {
          xf86LoaderReqSymLists(int10Symbols, NULL);
#if !defined(__alpha__)
          pSiS->pInt = xf86InitInt10(pSiS->pEnt->index);
#endif
       } else {
          SISErrorLog(pScrn, "Could not load int10 module\n");
       }
    }

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    {
        resRange vgamem[] = {   {ResShrMemBlock,0xA0000,0xAFFFF},
                                {ResShrMemBlock,0xB0000,0xB7FFF},
                                {ResShrMemBlock,0xB8000,0xBFFFF},
                            _END };
        xf86SetOperatingState(vgamem, pSiS->pEnt->index, ResUnusedOpr);
    }
#else
    xf86SetOperatingState(resVgaMem, pSiS->pEnt->index, ResUnusedOpr);
#endif

    /* Operations for which memory access is required */
    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
    /* Operations for which I/O access is required */
    pScrn->racIoFlags = RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

    /* The ramdac module should be loaded here when needed */
    if(!xf86LoadSubModule(pScrn, "ramdac")) {
       SISErrorLog(pScrn, "Could not load ramdac module\n");
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }

    xf86LoaderReqSymLists(ramdacSymbols, NULL);

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /*
     * Set the Chipset and ChipRev, allowing config file entries to
     * override. DANGEROUS!
     */
    if(pSiS->pEnt->device->chipset && *pSiS->pEnt->device->chipset)  {
       pScrn->chipset = pSiS->pEnt->device->chipset;
       pSiS->Chipset = xf86StringToToken(SISChipsets, pScrn->chipset);
       from = X_CONFIG;
    } else if(pSiS->pEnt->device->chipID >= 0) {
       pSiS->Chipset = pSiS->pEnt->device->chipID;
       pScrn->chipset = (char *)xf86TokenToString(SISChipsets, pSiS->Chipset);

       from = X_CONFIG;
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
                                pSiS->Chipset);
    } else {
       from = X_PROBED;
       pSiS->Chipset = pSiS->PciInfo->chipType;
       pScrn->chipset = (char *)xf86TokenToString(SISChipsets, pSiS->Chipset);
    }
    if(pSiS->pEnt->device->chipRev >= 0) {
       pSiS->ChipRev = pSiS->pEnt->device->chipRev;
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
                        pSiS->ChipRev);
    } else {
       pSiS->ChipRev = pSiS->PciInfo->chipRev;
    }
    pSiS->sishw_ext.jChipRevision = pSiS->ChipRev;

    /* Determine SiS6326 chiprevision. This is not yet used for
     * anything, but it will as soon as I found out on which revisions
     * the hardware video overlay really works.
     * According to SiS the only differences are:
     * Chip name     Chip type      TV-Out       MPEG II decoder
     * 6326 AGP      Rev. G0/H0     no           no
     * 6326 DVD      Rev. D2        yes          yes
     * 6326          Rev. Cx        yes          yes
     */
    pSiS->SiS6326Flags = 0;
    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Chipset is SiS6326 %s (revision 0x%02x)\n",
		(pSiS->ChipRev == 0xaf) ? "(Ax)" :
		   ((pSiS->ChipRev == 0x0a) ? "AGP (G0)" :
		      ((pSiS->ChipRev == 0x0b) ? "AGP (H0)" :
		          (((pSiS->ChipRev & 0xf0) == 0xd0) ? "DVD (Dx/H0)" :
   			      (((pSiS->ChipRev & 0xf0) == 0x90) ? "(9x)" :
			          (((pSiS->ChipRev & 0xf0) == 0xc0) ? "(Cx)" :
				       "(unknown)"))))),
		pSiS->ChipRev);
       if((pSiS->ChipRev != 0x0a) && (pSiS->ChipRev != 0x0b)) {
		pSiS->SiS6326Flags |= SIS6326_HASTV;
       }
    }


    /*
     * This shouldn't happen because such problems should be caught in
     * SISProbe(), but check it just in case.
     */
    if(pScrn->chipset == NULL) {
       SISErrorLog(pScrn, "ChipID 0x%04X is not recognised\n", pSiS->Chipset);
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }
    if(pSiS->Chipset < 0) {
       SISErrorLog(pScrn, "Chipset \"%s\" is not recognised\n", pScrn->chipset);
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }

    /* Determine chipset and VGA engine type */
    pSiS->ChipFlags = 0;
    pSiS->SiS_SD_Flags = 0;
    pSiS->HWCursorMBufNum = pSiS->HWCursorCBufNum = 0;

    switch(pSiS->Chipset) {
	case PCI_CHIP_SIS300:
		pSiS->sishw_ext.jChipType = SIS_300;
		pSiS->VGAEngine = SIS_300_VGA;
		pSiS->SiS_SD_Flags |= SiS_SD_IS300SERIES;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS630: /* 630 + 730 */
		pSiS->sishw_ext.jChipType = SIS_630;
		if(pciReadLong(0x00000000, 0x00) == 0x07301039) {
		   pSiS->sishw_ext.jChipType = SIS_730;
		}
		pSiS->VGAEngine = SIS_300_VGA;
		pSiS->SiS_SD_Flags |= SiS_SD_IS300SERIES;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS540:
		pSiS->sishw_ext.jChipType = SIS_540;
		pSiS->VGAEngine = SIS_300_VGA;
		pSiS->SiS_SD_Flags |= SiS_SD_IS300SERIES;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS315H:
		pSiS->sishw_ext.jChipType = SIS_315H;
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->ChipFlags |= SiSCF_315Core;
		pSiS->SiS_SD_Flags |= SiS_SD_IS315SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS315:
		/* Override for simplicity */
	        pSiS->Chipset = PCI_CHIP_SIS315H;
		pSiS->sishw_ext.jChipType = SIS_315;
		pSiS->ChipFlags |= SiSCF_315Core;
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->SiS_SD_Flags |= SiS_SD_IS315SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS315PRO:
		/* Override for simplicity */
		pSiS->Chipset = PCI_CHIP_SIS315H;
		pSiS->sishw_ext.jChipType = SIS_315PRO;
		pSiS->ChipFlags |= SiSCF_315Core;
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->SiS_SD_Flags |= SiS_SD_IS315SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS550:
		pSiS->sishw_ext.jChipType = SIS_550;
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->ChipFlags |= SiSCF_Integrated;
		pSiS->SiS_SD_Flags |= SiS_SD_IS315SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS650: /* 650 + 740 */
		pSiS->sishw_ext.jChipType = SIS_650;
		if(pciReadLong(0x00000000, 0x00) == 0x07401039) {
		   pSiS->sishw_ext.jChipType = SIS_740;
		}
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->ChipFlags |= (SiSCF_Integrated | SiSCF_Real256ECore);
		pSiS->SiS_SD_Flags |= SiS_SD_IS315SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 128;
		break;
	case PCI_CHIP_SIS330:
		pSiS->sishw_ext.jChipType = SIS_330;
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->ChipFlags |= SiSCF_XabreCore;
		pSiS->SiS_SD_Flags |= SiS_SD_IS330SERIES;
		pSiS->myCR63 = 0x63;
		pSiS->mmioSize = 256;
		break;
	case PCI_CHIP_SIS660: /* 660, 661, 741, 760 */
	        {
		unsigned long hpciid = pciReadLong(0x00000000, 0x00);
		switch(hpciid) {
		case 0x06601039:
		   pSiS->sishw_ext.jChipType = SIS_660;
		   pSiS->ChipFlags |= SiSCF_Ultra256Core;
		   pSiS->mmioSize = 256;
		   break;
		case 0x07601039:
		   pSiS->sishw_ext.jChipType = SIS_760;
		   pSiS->ChipFlags |= SiSCF_Ultra256Core;
		   pSiS->mmioSize = 256;
		   break;
		case 0x07411039:
		   pSiS->sishw_ext.jChipType = SIS_741;
		   pSiS->ChipFlags |= SiSCF_Real256ECore;
		   pSiS->mmioSize = 128;
		   break;
		case 0x06611039:
		default:
		   pSiS->sishw_ext.jChipType = SIS_661;
		   pSiS->ChipFlags |= SiSCF_Real256ECore;
		   pSiS->mmioSize = 128;
		}
		/* Detection could also be done by CR5C & 0xf8:
		   0x10 = 661 (CR5F & 0xc0: 0x00 both A0 and A1)
		   0x80 = 760 (CR5F & 0xc0: 0x00 A0, 0x40 A1)
		   0x90 = 741 (CR5F & 0xc0: 0x00 A0,A1 0x40 A2)
		   other: 660 (CR5F & 0xc0: 0x00 A0 0x40 A1) (DOA?)
		 */
		pSiS->VGAEngine = SIS_315_VGA;
		pSiS->ChipFlags |= SiSCF_Integrated;
		pSiS->SiS_SD_Flags |= SiS_SD_IS330SERIES;
		pSiS->myCR63 = 0x53; /* Yes, 0x53 */
		}
		break;
	case PCI_CHIP_SIS530:
		pSiS->sishw_ext.jChipType = SIS_530;
		pSiS->VGAEngine = SIS_530_VGA;
		pSiS->mmioSize = 64;
		break;
	default:
		pSiS->sishw_ext.jChipType = SIS_OLD;
		pSiS->VGAEngine = SIS_OLD_VGA;
		pSiS->mmioSize = 64;
		break;
    }

    /* Now check if sisfb is loaded. Since sisfb only supports
     * the 300 and 315 series, we only do this for these chips.
     * We use this for checking where sisfb starts its memory
     * heap in order to automatically detect the correct MaxXFBMem
     * setting (which normally is given by the option of the same name).
     * Under kernel 2.4.y, that only works if sisfb is completely 
     * running, ie with a video mode because the fbdev will not be
     * installed otherwise. Under 2.5 and later, sisfb will install
     * the framebuffer device in any way and running it with mode=none
     * is no longer supported (or necessary).
     */

    pSiS->donttrustpdc = FALSE;
    pSiS->sisfbpdc = 0xff;
    pSiS->sisfbpdca = 0xff;
    pSiS->sisfblcda = 0xff;
    pSiS->sisfbscalelcd = -1;
    pSiS->sisfbspecialtiming = CUT_NONE;
    pSiS->sisfb_haveemi = FALSE;
    pSiS->OldMode = 0;
    pSiS->sisfbfound = FALSE;

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

       int fd, i;
       sisfb_info mysisfbinfo;
       char name[10];
       CARD32 sisfbversion;

       {
          i=0;
          do {
             sprintf(name, "/dev/fb%1d", i);
             if((fd = open(name, 'r')) != -1) {

	        if(!ioctl(fd, SISFB_GET_INFO, &mysisfbinfo)) {

	           if(mysisfbinfo.sisfb_id == SISFB_ID) {

	              sisfbversion = (mysisfbinfo.sisfb_version << 16) |
		                     (mysisfbinfo.sisfb_revision << 8) |
			  	     (mysisfbinfo.sisfb_patchlevel);

	              if(sisfbversion >= 0x010508) {
		        /* Added PCI bus/slot/func into in sisfb Version 1.5.08.
		           Check this to make sure we run on the same card as sisfb
		         */
		        if((mysisfbinfo.sisfb_pcibus == pSiS->PciInfo->bus) &&
		           (mysisfbinfo.sisfb_pcislot == pSiS->PciInfo->device) &&
		           (mysisfbinfo.sisfb_pcifunc == pSiS->PciInfo->func) ) {
	         	    pSiS->sisfbfound = TRUE;
		        }
		      } else pSiS->sisfbfound = TRUE;

		      if(pSiS->sisfbfound) {
		         xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      	             "%s: SiS kernel fb driver (sisfb) %d.%d.%d detected (PCI: %02d:%02d.%d)\n",
		             	&name[5],
		             	mysisfbinfo.sisfb_version,
		     		mysisfbinfo.sisfb_revision,
		     		mysisfbinfo.sisfb_patchlevel,
		     		pSiS->PciInfo->bus,
		     		pSiS->PciInfo->device,
		     		pSiS->PciInfo->func);
		         /* Added version/rev/pl in sisfb 1.4.0 */
		         if(mysisfbinfo.sisfb_version == 0) {
		            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		                "Old version of sisfb found. Please update\n");
		         }
		         pSiS->sisfbMem = mysisfbinfo.heapstart;
		         /* Basically, we can't trust the pdc register if sisfb is loaded */
		         pSiS->donttrustpdc = TRUE;
		         xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		         	"sisfb: memory heap starts at %dKB\n", (int)pSiS->sisfbMem);
		         xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		      		"sisfb: using video mode 0x%02x\n", mysisfbinfo.fbvidmode);
		   	 pSiS->OldMode = mysisfbinfo.fbvidmode;
		         if(sisfbversion >= 0x010506) {
		            xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		        	"sisfb: %sreserved hardware cursor, using %s command queue\n",
			        (mysisfbinfo.sisfb_caps & 0x80) ? "" : "not ",
				(mysisfbinfo.sisfb_caps & 0x40) ? "SiS300 Turbo" :
			   	   (mysisfbinfo.sisfb_caps & 0x20) ? "SiS315/330 AGP" :
			              (mysisfbinfo.sisfb_caps & 0x10) ? "SiS315/330 VRAM" :
			                 (mysisfbinfo.sisfb_caps & 0x08) ? "SiS315/330 MMIO" :
				            "no");
		         }
		         if(sisfbversion >= 0x01050A) {
		            /* We can trust the pdc value if sisfb is of recent version */
		            if(pSiS->VGAEngine == SIS_300_VGA) pSiS->donttrustpdc = FALSE;
		            if(sisfbversion >= 0x01050B) {
			       if(pSiS->VGAEngine == SIS_300_VGA) {
		                  /* As of 1.5.11, sisfb saved the register for us (300 series) */
		      	          pSiS->sisfbpdc = mysisfbinfo.sisfb_lcdpdc;
				  if(!pSiS->sisfbpdc) pSiS->sisfbpdc = 0xff;
			       }
		            }
		            if(sisfbversion >= 0x01050E) {
		               if(pSiS->VGAEngine == SIS_315_VGA) {
		                  pSiS->sisfblcda = mysisfbinfo.sisfb_lcda;
			       }
			       if(sisfbversion >= 0x01060D) {
			          pSiS->sisfbscalelcd = mysisfbinfo.sisfb_scalelcd;
				  pSiS->sisfbspecialtiming = mysisfbinfo.sisfb_specialtiming;
			       }
			       if(sisfbversion >= 0x010610) {
			          if(pSiS->VGAEngine == SIS_315_VGA) {
				     pSiS->donttrustpdc = FALSE;
				     pSiS->sisfbpdc = mysisfbinfo.sisfb_lcdpdc;
				     if(sisfbversion >= 0x010618) {
				        pSiS->sisfb_haveemi = mysisfbinfo.sisfb_haveemi ? TRUE : FALSE;
					pSiS->sisfb_haveemilcd = TRUE;  /* will match most cases */
					pSiS->sisfb_emi30 = mysisfbinfo.sisfb_emi30;
					pSiS->sisfb_emi31 = mysisfbinfo.sisfb_emi31;
					pSiS->sisfb_emi32 = mysisfbinfo.sisfb_emi32;
					pSiS->sisfb_emi33 = mysisfbinfo.sisfb_emi33;
				     }
				     if(sisfbversion >= 0x010619) {
				        pSiS->sisfb_haveemilcd = mysisfbinfo.sisfb_haveemilcd ? TRUE : FALSE;
				     }
				     if(sisfbversion >= 0x01061f) {
					pSiS->sisfbpdca = mysisfbinfo.sisfb_lcdpdca;
				     } else {
				        if(pSiS->sisfbpdc) {
				           pSiS->sisfbpdca = (pSiS->sisfbpdc & 0xf0) >> 3;
					   pSiS->sisfbpdc  = (pSiS->sisfbpdc & 0x0f) << 1;
					} else {
					   pSiS->sisfbpdca = pSiS->sisfbpdc = 0xff;
					}
				     }
				  }
			       }
		            }
		         }
		      }
	           }
	        }
	        close (fd);
             }
	     i++;
          } while((i <= 7) && (!pSiS->sisfbfound));
	  if(!pSiS->sisfbfound) xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "sisfb not found\n");
       }
    }

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * Additionally, determine the size of the HWCursor memory area.
     */
    switch(pSiS->VGAEngine) {
      case SIS_300_VGA:
        pSiS->CursorSize = 4096;
    	pix24flags = Support32bppFb;
	break;
      case SIS_315_VGA:
        pSiS->CursorSize = 16384;
    	pix24flags = Support32bppFb;
	break;
      case SIS_530_VGA:
        pSiS->CursorSize = 2048;
    	pix24flags = Support32bppFb |
	             Support24bppFb;
        break;
      default:
        pSiS->CursorSize = 2048;
        pix24flags = Support24bppFb;
	break;
    }

#ifdef SISDUALHEAD
    /* In case of Dual Head, we need to determine if we are the "master" head or
     * the "slave" head. In order to do that, we set PrimInit to DONE in the
     * shared entity at the end of the first initialization. The second
     * initialization then knows that some things have already been done. THIS
     * ALWAYS ASSUMES THAT THE FIRST DEVICE INITIALIZED IS THE MASTER!
     */

    if(xf86IsEntityShared(pScrn->entityList[0])) {
       if(pSiSEnt->lastInstance > 0) {
     	  if(!xf86IsPrimInitDone(pScrn->entityList[0])) {
	     /* First Head (always CRT2) */
	     pSiS->SecondHead = FALSE;
	     pSiSEnt->pScrn_1 = pScrn;
	     pSiSEnt->CRT1ModeNo = pSiSEnt->CRT2ModeNo = -1;
	     pSiSEnt->CRT2ModeSet = FALSE;
	     pSiS->DualHeadMode = TRUE;
	     pSiSEnt->DisableDual = FALSE;
	     pSiSEnt->BIOS = NULL;
	     pSiSEnt->ROM661New = FALSE;
	     pSiSEnt->SiS_Pr = NULL;
	     pSiSEnt->RenderAccelArray = NULL;
	  } else {
	     /* Second Head (always CRT1) */
	     pSiS->SecondHead = TRUE;
	     pSiSEnt->pScrn_2 = pScrn;
	     pSiS->DualHeadMode = TRUE;
	  }
       } else {
          /* Only one screen in config file - disable dual head mode */
          pSiS->SecondHead = FALSE;
	  pSiS->DualHeadMode = FALSE;
	  pSiSEnt->DisableDual = TRUE;
       }
    } else {
       /* Entity is not shared - disable dual head mode */
       pSiS->SecondHead = FALSE;
       pSiS->DualHeadMode = FALSE;
    }
#endif

    pSiS->ForceCursorOff = FALSE;

    /* Allocate SiS_Private (for mode switching code) and initialize it */
    pSiS->SiS_Pr = NULL;
#ifdef SISDUALHEAD
    if(pSiSEnt) {
       if(pSiSEnt->SiS_Pr) pSiS->SiS_Pr = pSiSEnt->SiS_Pr;
    }
#endif
    if(!pSiS->SiS_Pr) {
       if(!(pSiS->SiS_Pr = xnfcalloc(sizeof(SiS_Private), 1))) {
          SISErrorLog(pScrn, "Could not allocate memory for SiS_Pr private\n");
#ifdef SISDUALHEAD
	  if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	  if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	  SISFreeRec(pScrn);
          return FALSE;
       }
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->SiS_Pr = pSiS->SiS_Pr;
#endif
       memset(pSiS->SiS_Pr, 0, sizeof(SiS_Private));
       pSiS->SiS_Pr->SiS_Backup70xx = 0xff;
       pSiS->SiS_Pr->SiS_CHOverScan = -1;
       pSiS->SiS_Pr->SiS_ChSW = FALSE;
       pSiS->SiS_Pr->SiS_CustomT = CUT_NONE;
       pSiS->SiS_Pr->PanelSelfDetected = FALSE;
       pSiS->SiS_Pr->UsePanelScaler = -1;
       pSiS->SiS_Pr->CenterScreen = -1;
       pSiS->SiS_Pr->CRT1UsesCustomMode = FALSE;
       pSiS->SiS_Pr->PDC = pSiS->SiS_Pr->PDCA = -1;
       pSiS->SiS_Pr->LVDSHL = -1;
       pSiS->SiS_Pr->HaveEMI = FALSE;
       pSiS->SiS_Pr->HaveEMILCD = FALSE;
       pSiS->SiS_Pr->OverruleEMI = FALSE;
       pSiS->SiS_Pr->SiS_SensibleSR11 = FALSE;
       if(pSiS->sishw_ext.jChipType >= SIS_661) {
          pSiS->SiS_Pr->SiS_SensibleSR11 = TRUE;
       }
       pSiS->SiS_Pr->SiS_MyCR63 = pSiS->myCR63;
    }

    /* Get our relocated IO registers */
    pSiS->RelIO = (SISIOADDRESS)((pSiS->PciInfo->ioBase[2] & 0xFFFC) + pSiS->IODBase);
    pSiS->sishw_ext.ulIOAddress = (SISIOADDRESS)(pSiS->RelIO + 0x30);
    xf86DrvMsg(pScrn->scrnIndex, from, "Relocated IO registers at 0x%lX\n",
           (unsigned long)pSiS->RelIO);

    /* Initialize SiS Port Reg definitions for externally used
     * init.c/init301.c functions.
     */
    SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO + 0x30);

    /* The following identifies the old chipsets. This is only
     * partly used since the really old chips are not supported,
     * but I keep it here for future use.
     * 205, 215 and 225 are to be treated the same way, 201 and 202
     * are different.
     */
    if(pSiS->VGAEngine == SIS_OLD_VGA || pSiS->VGAEngine == SIS_530_VGA) {
       switch(pSiS->Chipset) {
       case PCI_CHIP_SG86C201:
       	  pSiS->oldChipset = OC_SIS86201; break;
       case PCI_CHIP_SG86C202:
       	  pSiS->oldChipset = OC_SIS86202; break;
       case PCI_CHIP_SG86C205:
          {
	  unsigned char temp;
	  sisSaveUnlockExtRegisterLock(pSiS, &srlockReg, &crlockReg);
	  inSISIDXREG(SISSR, 0x10, temp);
	  if(temp & 0x80) pSiS->oldChipset = OC_SIS6205B;
	  else pSiS->oldChipset = (pSiS->ChipRev == 0x11) ?
	  		OC_SIS6205C : OC_SIS6205A;
          break;
	  }
       case PCI_CHIP_SIS82C204:
       	  pSiS->oldChipset = OC_SIS82204; break;
       case 0x6225:
          pSiS->oldChipset = OC_SIS6225; break;
       case PCI_CHIP_SIS5597:
          pSiS->oldChipset = OC_SIS5597; break;
       case PCI_CHIP_SIS6326:
          pSiS->oldChipset = OC_SIS6326; break;
       case PCI_CHIP_SIS530:
          if(pciReadLong(0x00000000, 0x00) == 0x06201039) {
	     pSiS->oldChipset = OC_SIS620;
	  } else {
             if((pSiS->ChipRev & 0x0f) < 0x0a)
	  	   pSiS->oldChipset = OC_SIS530A;
	     else  pSiS->oldChipset = OC_SIS530B;
	  }
	  break;
       default:
          pSiS->oldChipset = OC_UNKNOWN;
       }
    }

    if(!xf86SetDepthBpp(pScrn, 0, 0, 0, pix24flags)) {
       SISErrorLog(pScrn, "xf86SetDepthBpp() error\n");
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }

    /* Check that the returned depth is one we support */
    temp = 0;
    switch(pScrn->depth) {
       case 8:
       case 16:
       case 24:
          break;
       case 15:
	  if((pSiS->VGAEngine == SIS_300_VGA) ||
	     (pSiS->VGAEngine == SIS_315_VGA)) {
	     temp = 1;
	  }
          break;
       default:
	  temp = 1;
    }

    if(temp) {
       SISErrorLog(pScrn,
               "Given color depth (%d) is not supported by this driver/chipset\n",
               pScrn->depth);
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    if( (((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) &&
         (pScrn->bitsPerPixel == 24)) ||
	((pSiS->VGAEngine == SIS_OLD_VGA) && (pScrn->bitsPerPixel == 32)) ) {
       SISErrorLog(pScrn,
            "Framebuffer bpp %d not supported for this chipset\n", pScrn->bitsPerPixel);
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }

    /* Get the depth24 pixmap format */
    if(pScrn->depth == 24 && pix24bpp == 0) {
       pix24bpp = xf86GetBppFromDepth(pScrn, 24);
    }

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if(pScrn->depth > 8) {
        /* The defaults are OK for us */
        rgb zeros = {0, 0, 0};

        if(!xf86SetWeight(pScrn, zeros, zeros)) {
	    SISErrorLog(pScrn, "xf86SetWeight() error\n");
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    SISFreeRec(pScrn);
            return FALSE;
        } else {
           Bool ret = FALSE;
           switch(pScrn->depth) {
	   case 15:
	      if((pScrn->weight.red != 5) ||
	         (pScrn->weight.green != 5) ||
		 (pScrn->weight.blue != 5)) ret = TRUE;
	      break;
	   case 16:
	      if((pScrn->weight.red != 5) ||
	         (pScrn->weight.green != 6) ||
		 (pScrn->weight.blue != 5)) ret = TRUE;
	      break;
	   case 24:
	      if((pScrn->weight.red != 8) ||
	         (pScrn->weight.green != 8) ||
		 (pScrn->weight.blue != 8)) ret = TRUE;
	      break;
           }
	   if(ret) {
	      SISErrorLog(pScrn,
	      	"RGB weight %d%d%d at depth %d not supported by hardware\n",
		(int)pScrn->weight.red, (int)pScrn->weight.green,
		(int)pScrn->weight.blue, pScrn->depth);
#ifdef SISDUALHEAD
	      if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	      if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	      SISFreeRec(pScrn);
              return FALSE;
	   }
        }
    }

    /* Set the current layout parameters */
    pSiS->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pSiS->CurrentLayout.depth        = pScrn->depth;
    /* (Inside this function, we can use pScrn's contents anyway) */

    if(!xf86SetDefaultVisual(pScrn, -1)) {
        SISErrorLog(pScrn, "xf86SetDefaultVisual() error\n");
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
        SISFreeRec(pScrn);
        return FALSE;
    } else {
        /* We don't support DirectColor at > 8bpp */
        if(pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
            SISErrorLog(pScrn,
	       	"Given default visual (%s) is not supported at depth %d\n",
                xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    SISFreeRec(pScrn);
            return FALSE;
        }
    }

#ifdef SISDUALHEAD
    /* Due to palette & timing problems we don't support 8bpp in DHM */
    if((pSiS->DualHeadMode) && (pScrn->bitsPerPixel == 8)) {
       SISErrorLog(pScrn, "Color depth 8 not supported in Dual Head mode.\n");
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       SISFreeRec(pScrn);
       return FALSE;
    }
#endif

    /*
     * The cmap layer needs this to be initialised.
     */
    {
        Gamma zeros = {0.0, 0.0, 0.0};

        if(!xf86SetGamma(pScrn, zeros)) {
	    SISErrorLog(pScrn, "xf86SetGamma() error\n");
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    SISFreeRec(pScrn);
            return FALSE;
        }
    }

    /* We use a programamble clock */
    pScrn->progClock = TRUE;

    /* Set the bits per RGB for 8bpp mode */
    if(pScrn->depth == 8) {
       pScrn->rgbBits = 6;
    }

    from = X_DEFAULT;

    /* Unlock registers */
    sisSaveUnlockExtRegisterLock(pSiS, &srlockReg, &crlockReg);

    /* Read BIOS for 300 and 315/330 series customization */
    pSiS->sishw_ext.pjVirtualRomBase = NULL;
    pSiS->BIOS = NULL;
    pSiS->sishw_ext.UseROM = FALSE;
    pSiS->ROM661New = FALSE;

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
#ifdef SISDUALHEAD
       if(pSiSEnt) {
          if(pSiSEnt->BIOS) {
	     pSiS->BIOS = pSiSEnt->BIOS;
	     pSiS->sishw_ext.pjVirtualRomBase = pSiS->BIOS;
	     pSiS->ROM661New = pSiSEnt->ROM661New;
          }
       }
#endif
       if(!pSiS->BIOS) {
          if(!(pSiS->BIOS = xcalloc(1, BIOS_SIZE))) {
             xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Could not allocate memory for video BIOS image\n");
          } else {
	     unsigned long  segstart;
             unsigned short romptr, pciid;
	     BOOLEAN found;

	     found = FALSE;
             for(segstart=BIOS_BASE; segstart<0x000f0000; segstart+=0x00001000) {

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
                if(xf86ReadBIOS(segstart, 0, pSiS->BIOS, BIOS_SIZE) != BIOS_SIZE) continue;
#else
                if(xf86ReadDomainMemory(pSiS->PciTag, segstart, BIOS_SIZE, pSiS->BIOS) != BIOS_SIZE) continue;
#endif

		if((pSiS->BIOS[0] != 0x55) || (pSiS->BIOS[1] != 0xaa)) continue;

		romptr = pSiS->BIOS[0x18] | (pSiS->BIOS[0x19] << 8);
		if(romptr > (BIOS_SIZE - 8)) continue;
		if((pSiS->BIOS[romptr]   != 'P') || (pSiS->BIOS[romptr+1] != 'C') ||
		   (pSiS->BIOS[romptr+2] != 'I') || (pSiS->BIOS[romptr+3] != 'R')) continue;

		pciid = pSiS->BIOS[romptr+4] | (pSiS->BIOS[romptr+5] << 8);
		if(pciid != 0x1039) continue;

		pciid = pSiS->BIOS[romptr+6] | (pSiS->BIOS[romptr+7] << 8);
		if(pciid != pSiS->Chipset) continue;

		found = TRUE;
		break;
             }

	     if(!found) {
	     	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		         "Could not find/read video BIOS\n");
 	   	xfree(pSiS->BIOS);
	        pSiS->BIOS = NULL;
             } else {
                pSiS->sishw_ext.pjVirtualRomBase = pSiS->BIOS;
		pSiS->ROM661New = SiSDetermineROMLayout661(pSiS->SiS_Pr,&pSiS->sishw_ext);
		romptr = pSiS->BIOS[0x16] | (pSiS->BIOS[0x17] << 8);
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Video BIOS version \"%7s\" found at 0x%lx (%s data layout)\n",
			&pSiS->BIOS[romptr], segstart, pSiS->ROM661New ? "new" : "old");
#ifdef SISDUALHEAD
                if(pSiSEnt) {
		   pSiSEnt->BIOS = pSiS->BIOS;
		   pSiSEnt->ROM661New = pSiS->ROM661New;
		}
#endif
             }
          }
       }
       if(pSiS->BIOS) pSiS->sishw_ext.UseROM = TRUE;
       else           pSiS->sishw_ext.UseROM = FALSE;
    }

    /* Evaluate options */
    SiSOptions(pScrn);

#ifdef SISMERGED
    /* Due to palette & timing problems we don't support 8bpp in MFBM */
    if((pSiS->MergedFB) && (pScrn->bitsPerPixel == 8)) {
       SISErrorLog(pScrn, "MergedFB: Color depth 8 not supported, %s\n", mergeddisstr);
       pSiS->MergedFB = pSiS->MergedFBAuto = FALSE;
    }
#endif

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        if(!pSiS->SecondHead) {
	     /* Copy some option settings to entity private */
             pSiSEnt->HWCursor = pSiS->HWCursor;
	     pSiSEnt->NoAccel = pSiS->NoAccel;
	     pSiSEnt->restorebyset = pSiS->restorebyset;
	     pSiSEnt->OptROMUsage = pSiS->OptROMUsage;
	     pSiSEnt->OptUseOEM = pSiS->OptUseOEM;
	     pSiSEnt->TurboQueue = pSiS->TurboQueue;
	     pSiSEnt->forceCRT1 = pSiS->forceCRT1;
	     pSiSEnt->ForceCRT1Type = pSiS->ForceCRT1Type;
	     pSiSEnt->ForceCRT2Type = pSiS->ForceCRT2Type;
	     pSiSEnt->ForceTVType = pSiS->ForceTVType;
	     pSiSEnt->ForceYPbPrType = pSiS->ForceYPbPrType;
	     pSiSEnt->ForceYPbPrAR = pSiS->ForceYPbPrAR;
	     pSiSEnt->UsePanelScaler = pSiS->UsePanelScaler;
	     pSiSEnt->CenterLCD = pSiS->CenterLCD;
	     pSiSEnt->DSTN = pSiS->DSTN;
	     pSiSEnt->OptTVStand = pSiS->OptTVStand;
	     pSiSEnt->NonDefaultPAL = pSiS->NonDefaultPAL;
	     pSiSEnt->NonDefaultNTSC = pSiS->NonDefaultNTSC;
	     pSiSEnt->chtvtype = pSiS->chtvtype;
	     pSiSEnt->OptTVOver = pSiS->OptTVOver;
	     pSiSEnt->OptTVSOver = pSiS->OptTVSOver;
	     pSiSEnt->chtvlumabandwidthcvbs = pSiS->chtvlumabandwidthcvbs;
	     pSiSEnt->chtvlumabandwidthsvideo = pSiS->chtvlumabandwidthsvideo;
	     pSiSEnt->chtvlumaflickerfilter = pSiS->chtvlumaflickerfilter;
	     pSiSEnt->chtvchromabandwidth = pSiS->chtvchromabandwidth;
	     pSiSEnt->chtvchromaflickerfilter = pSiS->chtvchromaflickerfilter;
	     pSiSEnt->chtvtextenhance = pSiS->chtvtextenhance;
	     pSiSEnt->chtvcontrast = pSiS->chtvcontrast;
	     pSiSEnt->chtvcvbscolor = pSiS->chtvcvbscolor;
	     pSiSEnt->sistvedgeenhance = pSiS->sistvedgeenhance;
	     pSiSEnt->sistvantiflicker = pSiS->sistvantiflicker;
	     pSiSEnt->sistvsaturation = pSiS->sistvsaturation;
	     pSiSEnt->sistvcfilter = pSiS->sistvcfilter;
	     pSiSEnt->sistvyfilter = pSiS->sistvyfilter;
	     pSiSEnt->sistvcolcalibc = pSiS->sistvcolcalibc;
	     pSiSEnt->sistvcolcalibf = pSiS->sistvcolcalibf;
	     pSiSEnt->tvxpos = pSiS->tvxpos;
	     pSiSEnt->tvypos = pSiS->tvypos;
	     pSiSEnt->tvxscale = pSiS->tvxscale;
	     pSiSEnt->tvyscale = pSiS->tvyscale;
	     pSiSEnt->CRT1gamma = pSiS->CRT1gamma;
	     pSiSEnt->CRT1gammaGiven = pSiS->CRT1gammaGiven;
	     pSiSEnt->XvGammaRed = pSiS->XvGammaRed;
	     pSiSEnt->XvGammaGreen = pSiS->XvGammaGreen;
	     pSiSEnt->XvGammaBlue = pSiS->XvGammaBlue;
	     pSiSEnt->XvGamma = pSiS->XvGamma;
	     pSiSEnt->XvGammaGiven = pSiS->XvGammaGiven;
	     pSiSEnt->CRT2gamma = pSiS->CRT2gamma;
	     pSiSEnt->XvOnCRT2 = pSiS->XvOnCRT2;
	     pSiSEnt->AllowHotkey = pSiS->AllowHotkey;
	     pSiSEnt->enablesisctrl = pSiS->enablesisctrl;
	     pSiSEnt->SenseYPbPr = pSiS->SenseYPbPr;
#ifdef SIS_CP
	     SIS_CP_DRIVER_COPYOPTIONSENT
#endif
	} else {
	     /* We always use same cursor type on both screens */
	     if(pSiS->HWCursor != pSiSEnt->HWCursor) {
	          pSiS->HWCursor = pSiSEnt->HWCursor;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent HWCursor setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		  	"Master head ruled: HWCursor shall be %s\n",
			pSiS->HWCursor ? "enabled" : "disabled");
	     }

	     /* We need identical NoAccel setting */
	     if(pSiS->NoAccel != pSiSEnt->NoAccel) {
	          pSiS->NoAccel = pSiSEnt->NoAccel;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent NoAccel setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: Acceleration shall be %s\n",
			pSiS->NoAccel ? "disabled" : "enabled");
	     }
	     pSiS->TurboQueue = pSiSEnt->TurboQueue;
	     pSiS->restorebyset = pSiSEnt->restorebyset;
	     pSiS->AllowHotkey = pSiS->AllowHotkey;
	     pSiS->OptROMUsage = pSiSEnt->OptROMUsage;
	     pSiS->OptUseOEM = pSiSEnt->OptUseOEM;
	     pSiS->forceCRT1 = pSiSEnt->forceCRT1;
	     pSiS->nocrt2ddcdetection = FALSE;
	     pSiS->forcecrt2redetection = FALSE;
	     pSiS->ForceCRT1Type = pSiSEnt->ForceCRT1Type;
	     pSiS->ForceCRT2Type = pSiSEnt->ForceCRT2Type;
	     pSiS->UsePanelScaler = pSiSEnt->UsePanelScaler;
	     pSiS->CenterLCD = pSiSEnt->CenterLCD;
	     pSiS->DSTN = pSiSEnt->DSTN;
	     pSiS->OptTVStand = pSiSEnt->OptTVStand;
	     pSiS->NonDefaultPAL = pSiSEnt->NonDefaultPAL;
	     pSiS->NonDefaultNTSC = pSiSEnt->NonDefaultNTSC;
	     pSiS->chtvtype = pSiSEnt->chtvtype;
	     pSiS->ForceTVType = pSiSEnt->ForceTVType;
	     pSiS->ForceYPbPrType = pSiSEnt->ForceYPbPrType;
	     pSiS->ForceYPbPrAR = pSiSEnt->ForceYPbPrAR;
	     pSiS->OptTVOver = pSiSEnt->OptTVOver;
	     pSiS->OptTVSOver = pSiSEnt->OptTVSOver;
	     pSiS->chtvlumabandwidthcvbs = pSiSEnt->chtvlumabandwidthcvbs;
	     pSiS->chtvlumabandwidthsvideo = pSiSEnt->chtvlumabandwidthsvideo;
	     pSiS->chtvlumaflickerfilter = pSiSEnt->chtvlumaflickerfilter;
	     pSiS->chtvchromabandwidth = pSiSEnt->chtvchromabandwidth;
	     pSiS->chtvchromaflickerfilter = pSiSEnt->chtvchromaflickerfilter;
	     pSiS->chtvcvbscolor = pSiSEnt->chtvcvbscolor;
	     pSiS->chtvtextenhance = pSiSEnt->chtvtextenhance;
	     pSiS->chtvcontrast = pSiSEnt->chtvcontrast;
	     pSiS->sistvedgeenhance = pSiSEnt->sistvedgeenhance;
	     pSiS->sistvantiflicker = pSiSEnt->sistvantiflicker;
	     pSiS->sistvsaturation = pSiSEnt->sistvsaturation;
	     pSiS->sistvcfilter = pSiSEnt->sistvcfilter;
	     pSiS->sistvyfilter = pSiSEnt->sistvyfilter;
	     pSiS->sistvcolcalibc = pSiSEnt->sistvcolcalibc;
	     pSiS->sistvcolcalibf = pSiSEnt->sistvcolcalibf;
	     pSiS->tvxpos = pSiSEnt->tvxpos;
	     pSiS->tvypos = pSiSEnt->tvypos;
	     pSiS->tvxscale = pSiSEnt->tvxscale;
	     pSiS->tvyscale = pSiSEnt->tvyscale;
	     pSiS->SenseYPbPr = pSiSEnt->SenseYPbPr;
	     if(!pSiS->CRT1gammaGiven) {
	        if(pSiSEnt->CRT1gammaGiven)
	           pSiS->CRT1gamma = pSiSEnt->CRT1gamma;
	     }
	     pSiS->CRT2gamma = pSiSEnt->CRT2gamma;
	     if(!pSiS->XvGammaGiven) {
	        if(pSiSEnt->XvGammaGiven) {
		   pSiS->XvGamma = pSiSEnt->XvGamma;
		   pSiS->XvGammaRed = pSiS->XvGammaRedDef = pSiSEnt->XvGammaRed;
		   pSiS->XvGammaGreen = pSiS->XvGammaGreenDef = pSiSEnt->XvGammaGreen;
		   pSiS->XvGammaBlue = pSiS->XvGammaBlueDef = pSiSEnt->XvGammaBlue;
		}
	     }
	     pSiS->XvOnCRT2 = pSiSEnt->XvOnCRT2;
	     pSiS->enablesisctrl = pSiSEnt->enablesisctrl;
	     /* Copy gamma brightness to Ent for Xinerama */
	     pSiSEnt->GammaBriR = pSiS->GammaBriR;
	     pSiSEnt->GammaBriG = pSiS->GammaBriG;
	     pSiSEnt->GammaBriB = pSiS->GammaBriB;
	     pSiSEnt->GammaPBriR = pSiS->GammaPBriR;
	     pSiSEnt->GammaPBriG = pSiS->GammaPBriG;
	     pSiSEnt->GammaPBriB = pSiS->GammaPBriB;
#ifdef SIS_CP
	     SIS_CP_DRIVER_COPYOPTIONS
#endif
	}
    }
#endif

    /* Handle UseROMData, NoOEM and UsePanelScaler options */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       from = X_PROBED;
       if(pSiS->OptROMUsage == 0)  {
       	  pSiS->sishw_ext.UseROM = FALSE;
	  from = X_CONFIG;
	  xf86DrvMsg(pScrn->scrnIndex, from, "Video ROM data usage is disabled\n");
       }

       if(!pSiS->OptUseOEM)
          xf86DrvMsg(pScrn->scrnIndex, from, "Internal OEM LCD/TV/VGA2 data usage is disabled\n");
	  
       pSiS->SiS_Pr->UsePanelScaler = pSiS->UsePanelScaler;
       pSiS->SiS_Pr->CenterScreen = pSiS->CenterLCD;
    }

    /* Do basic configuration */
    SiSSetup(pScrn);

    from = X_PROBED;
    if(pSiS->pEnt->device->MemBase != 0) {
       /*
        * XXX Should check that the config file value matches one of the
        * PCI base address values.
        */
       pSiS->FbAddress = pSiS->pEnt->device->MemBase;
       from = X_CONFIG;
    } else {
       pSiS->FbAddress = pSiS->PciInfo->memBase[0] & 0xFFFFFFF0;
    }

    pSiS->realFbAddress = pSiS->FbAddress;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode)
       xf86DrvMsg(pScrn->scrnIndex, from, "Global linear framebuffer at 0x%lX\n",
           (unsigned long)pSiS->FbAddress);
    else 	   
#endif
       xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
           (unsigned long)pSiS->FbAddress);

    if(pSiS->pEnt->device->IOBase != 0) {
        /*
         * XXX Should check that the config file value matches one of the
         * PCI base address values.
         */
       pSiS->IOAddress = pSiS->pEnt->device->IOBase;
       from = X_CONFIG;
    } else {
       pSiS->IOAddress = pSiS->PciInfo->memBase[1] & 0xFFFFFFF0;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX (size %ldK)\n",
           (unsigned long)pSiS->IOAddress, pSiS->mmioSize);
    pSiS->sishw_ext.bIntegratedMMEnabled = TRUE;

    /* Register the PCI-assigned resources. */
    if(xf86RegisterResources(pSiS->pEnt->index, NULL, ResExclusive)) {
       SISErrorLog(pScrn, "xf86RegisterResources() found resource conflicts\n");
#ifdef SISDUALHEAD
       if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
       if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
       sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
       SISFreeRec(pScrn);
       return FALSE;
    }

    from = X_PROBED;
    if(pSiS->pEnt->device->videoRam != 0) {
       if(pSiS->Chipset == PCI_CHIP_SIS6326) {
          pScrn->videoRam = pSiS->pEnt->device->videoRam;
          from = X_CONFIG;
       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	  	"Option \"VideoRAM\" ignored\n");
       }
    }

    pSiS->RealVideoRam = pScrn->videoRam;

    if((pSiS->Chipset == PCI_CHIP_SIS6326)
			&& (pScrn->videoRam > 4096)
			&& (from != X_CONFIG)) {
        pScrn->videoRam = 4096;
        xf86DrvMsg(pScrn->scrnIndex, from,
	       "SiS6326: Detected %d KB VideoRAM, limiting to %d KB\n",
               pSiS->RealVideoRam, pScrn->videoRam);
    } else {
        xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d KB\n",
               pScrn->videoRam);
    }

    if((pSiS->Chipset == PCI_CHIP_SIS6326) &&
       (pScrn->videoRam > 4096)) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	"SiS6326 engines do not support more than 4096KB RAM, therefore\n");
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
        "TurboQueue, HWCursor, 2D acceleration and XVideo are disabled.\n");
       pSiS->TurboQueue = FALSE;
       pSiS->HWCursor   = FALSE;
       pSiS->NoXvideo   = TRUE;
       pSiS->NoAccel    = TRUE;
    }

    pSiS->FbMapSize = pSiS->availMem = pScrn->videoRam * 1024;
    pSiS->sishw_ext.ulVideoMemorySize = pScrn->videoRam * 1024;
    pSiS->sishw_ext.bSkipDramSizing = TRUE;

    /* Calculate real availMem according to Accel/TurboQueue and
     * HWCursur setting. Also, initialize some variables used
     * in other modules.
     */

    pSiS->cursorOffset = 0;
    pSiS->CurARGBDest = NULL;
    pSiS->CurMonoSrc = NULL;
    pSiS->CurFGCol = pSiS->CurBGCol = 0;

    switch(pSiS->VGAEngine) {

      case SIS_300_VGA:
      	pSiS->TurboQueueLen = 512;
       	if(pSiS->TurboQueue) {
	   pSiS->availMem -= (pSiS->TurboQueueLen*1024);
	   pSiS->cursorOffset = 512;
        }
	if(pSiS->HWCursor) {
	   pSiS->availMem -= pSiS->CursorSize;
	   if(pSiS->OptUseColorCursor) pSiS->availMem -= pSiS->CursorSize;
	}
	pSiS->CmdQueLenMask = 0xFFFF;
	pSiS->CmdQueLenFix  = 0;
	pSiS->cursorBufferNum = 0;
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->cursorBufferNum = 0;
#endif
	break;

      case SIS_315_VGA:
#ifdef SISVRAMQ
	pSiS->cmdQueueSizeMask = pSiS->cmdQueueSize - 1;	/* VRAM Command Queue is variable (in therory) */
	pSiS->cmdQueueOffset = (pScrn->videoRam * 1024) - pSiS->cmdQueueSize;
	pSiS->cmdQueueLen = 0;
        pSiS->cmdQueueLenMin = 0x200;
        pSiS->cmdQueueLenMax = pSiS->cmdQueueSize - pSiS->cmdQueueLenMin;
	pSiS->cmdQueueSize_div2 = pSiS->cmdQueueSize / 2;
	pSiS->cmdQueueSize_div4 = pSiS->cmdQueueSize / 4;
	pSiS->cmdQueueSize_4_3 = (pSiS->cmdQueueSize / 4) * 3;
	pSiS->availMem -= pSiS->cmdQueueSize;
        pSiS->cursorOffset = (pSiS->cmdQueueSize / 1024);
#else
       	if(pSiS->TurboQueue) {
	   pSiS->availMem -= (512*1024);  			/* MMIO Command Queue is 512k (variable in theory) */
	   pSiS->cursorOffset = 512;
	}
#endif
	if(pSiS->HWCursor) {
           pSiS->availMem -= (pSiS->CursorSize * 2);
	   if(pSiS->OptUseColorCursor) pSiS->availMem -= (pSiS->CursorSize * 2);
	}
	pSiS->cursorBufferNum = 0;
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->cursorBufferNum = 0;
#endif	
	break;

      default:
        /* cursorOffset not used in cursor functions for 530 and
	 * older chips, because the cursor is *above* the TQ.
	 * On 5597 and older revisions of the 6326, the TQ is
	 * max 32K, on newer 6326 revisions and the 530 either 30
	 * (or 32?) or 62K (or 64?). However, to make sure, we
	 * use only 30K (or 32?), but reduce the available memory
	 * by 64, and locate the TQ at the beginning of this last
	 * 64K block. (We do this that way even when using the
	 * HWCursor, because the cursor only takes 2K and the
	 * queue does not seem to last that far anyway.)
	 * The TQ must be located at 32KB boundaries.
	 */
	if(pSiS->RealVideoRam < 3072) {
	   if(pSiS->TurboQueue) {
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Not enough video RAM for TurboQueue. TurboQueue disabled\n");
	      pSiS->TurboQueue = FALSE;
	   }
	}
	pSiS->CmdQueMaxLen = 32;
     	if(pSiS->TurboQueue) {
	              	      pSiS->availMem -= (64*1024);
			      pSiS->CmdQueMaxLen = 900;   /* To make sure; should be 992 */
	} else if(pSiS->HWCursor) {
	                      pSiS->availMem -= pSiS->CursorSize;
	}
	if(pSiS->Chipset == PCI_CHIP_SIS530) {
		/* Check if Flat Panel is enabled */
		inSISIDXREG(SISSR, 0x0e, tempreg);
		if(!tempreg & 0x04) pSiS->availMem -= pSiS->CursorSize;

		/* Set up mask for MMIO register */
		pSiS->CmdQueLenMask = (pSiS->TurboQueue) ? 0x1FFF : 0x00FF;
	} else {
	        /* TQ is never used on 6326/5597, because the accelerator
		 * always Syncs. So this is just cosmentic work. (And I
		 * am not even sure that 0x7fff is correct. MMIO 0x83a8
		 * holds 0xec0 if (30k) TQ is enabled, 0x20 if TQ disabled.
		 * The datasheet has no real explanation on the queue length
		 * if the TQ is enabled. Not syncing and waiting for a
		 * suitable queue length instead does not work.
		 */
	        pSiS->CmdQueLenMask = (pSiS->TurboQueue) ? 0x7FFF : 0x003F;
	}

	/* This is to be subtracted from MMIO queue length register contents
	 * for getting the real Queue length.
	 */
	pSiS->CmdQueLenFix  = (pSiS->TurboQueue) ? 32 : 0;
    }

#ifdef SISDUALHEAD
    /* In dual head mode, we share availMem equally - so align it
     * to 8KB; this way, the address of the FB of the second
     * head is aligned to 4KB for mapping.
     */
   if(pSiS->DualHeadMode)
      pSiS->availMem &= 0xFFFFE000;
#endif

    /* Check MaxXFBMem setting */
#ifdef SISDUALHEAD
    /* Since DRI is not supported in dual head mode, we
       don't need the MaxXFBMem setting. */
    if(pSiS->DualHeadMode) {
       if(pSiS->maxxfbmem) {
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"MaxXFBMem not used in Dual Head mode. Using all VideoRAM.\n");
       }
       pSiS->maxxfbmem = pSiS->availMem;
    } else
#endif
       if(pSiS->maxxfbmem) {
    	  if(pSiS->maxxfbmem > pSiS->availMem) {
	     if(pSiS->sisfbMem) {
	        pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
             	   	"Invalid MaxXFBMem setting. Using sisfb heap start information\n");
	     } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                	"Invalid MaxXFBMem setting. Using all VideoRAM for framebuffer\n");
	        pSiS->maxxfbmem = pSiS->availMem;
	     }
	  } else if(pSiS->sisfbMem) {
	     if(pSiS->maxxfbmem > pSiS->sisfbMem * 1024) {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       		"MaxXFBMem beyond sisfb heap start. Using sisfb heap start\n");
                pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
	     }
	  }
    } else if(pSiS->sisfbMem) {
       pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
    }
    else pSiS->maxxfbmem = pSiS->availMem;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using %ldK of framebuffer memory\n",
    				pSiS->maxxfbmem / 1024);

    /* Find out about sub-classes of some chipsets and check
     * if the chipset supports two video overlays
     */
    if(pSiS->VGAEngine == SIS_300_VGA    ||
       pSiS->VGAEngine == SIS_315_VGA    ||
       pSiS->Chipset == PCI_CHIP_SIS530  ||
       pSiS->Chipset == PCI_CHIP_SIS6326 ||
       pSiS->Chipset == PCI_CHIP_SIS5597)  {
       pSiS->hasTwoOverlays = FALSE;
       switch(pSiS->Chipset) {
         case PCI_CHIP_SIS300:
         case PCI_CHIP_SIS630:
         case PCI_CHIP_SIS550:
	   pSiS->hasTwoOverlays = TRUE;
	   pSiS->SiS_SD_Flags |= SiS_SD_SUPPORT2OVL;
	   break;
	 case PCI_CHIP_SIS315PRO:
	   pSiS->ChipFlags |= SiSCF_LARGEOVERLAY;
	   break;
         case PCI_CHIP_SIS330:
	   pSiS->ChipFlags |= SiSCF_LARGEOVERLAY;
	   pSiS->ChipFlags |= SiSCF_CRT2HWCKaputt;
	   break;
	 case PCI_CHIP_SIS660:
	   {
#if 0
	     static const char *id661str[] = {
	   	"661 ?", "661 ?", "661 ?", "661 ?",
		"661 ?", "661 ?", "661 ?", "661 ?",
		"661 ?", "661 ?", "661 ?", "661 ?",
		"661 ?", "661 ?", "661 ?", "661 ?"
	     };
#endif	     
	     pSiS->ChipFlags |= SiSCF_LARGEOVERLAY;
	     pSiS->hasTwoOverlays = TRUE;
	     pSiS->SiS_SD_Flags |= SiS_SD_SUPPORT2OVL;
#if 0
	     if(pSiS->sishw_ext.jChipType == SIS_661) {
		inSISIDXREG(SISCR, 0x5f, CR5F);
	        CR5F &= 0xf0;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	      "SiS661FX revision ID %x (%s)\n", CR5F, id661str[CR5F >> 4]);
             }
#endif
	     break;
	   }
         case PCI_CHIP_SIS650:
	   {
	     unsigned char tempreg1, tempreg2;
	     static const char *id650str[] = {
	   	"650",       "650",       "650",       "650",
		"650 A0 AA", "650 A2 CA", "650",       "650",
		"M650 A0",   "M650 A1 AA","651 A0 AA", "651 A1 AA",
		"M650",      "65?",       "651",       "65?"
	     };
	     pSiS->ChipFlags |= SiSCF_LARGEOVERLAY;
	     if(pSiS->sishw_ext.jChipType == SIS_650) {
		inSISIDXREG(SISCR, 0x5f, CR5F);
	        CR5F &= 0xf0;
	        andSISIDXREG(SISCR, 0x5c, 0x07);
		inSISIDXREG(SISCR, 0x5c, tempreg1);
		tempreg1 &= 0xf8;
		orSISIDXREG(SISCR, 0x5c, 0xf8);
		inSISIDXREG(SISCR, 0x5c, tempreg2);
		tempreg2 &= 0xf8;
		if((!tempreg1) || (tempreg2)) {
	           xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	      "SiS650 revision ID %x (%s)\n", CR5F, id650str[CR5F >> 4]);
	           if(CR5F & 0x80) {
	              pSiS->hasTwoOverlays = TRUE;  /* M650 or 651 */
		      pSiS->SiS_SD_Flags |= SiS_SD_SUPPORT2OVL;
	           }
		   switch(CR5F) {
		      case 0xa0:
		      case 0xb0:
		      case 0xe0:
		         pSiS->ChipFlags |= SiSCF_Is651;
		         break;
		      case 0x80:
		      case 0x90:
		      case 0xc0:
		         pSiS->ChipFlags |= SiSCF_IsM650;
		         break;
		   }
		} else {
		   pSiS->hasTwoOverlays = TRUE;  
		   pSiS->SiS_SD_Flags |= SiS_SD_SUPPORT2OVL;
		   switch(CR5F) {
		      case 0x90:
		         inSISIDXREG(SISCR, 0x5c, tempreg1);
			 tempreg1 &= 0xf8;
			 switch(tempreg1) {
			    case 0x00:
			       pSiS->ChipFlags |= SiSCF_IsM652;
			       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	                   "SiSM652 revision ID %x\n", CR5F);
			       break;
			    case 0x40:
			       pSiS->ChipFlags |= SiSCF_IsM653;
			       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	                   "SiSM653 revision ID %x\n", CR5F);
			       break;
			    default:
			       pSiS->ChipFlags |= SiSCF_IsM650;
			       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	                   "SiSM650 revision ID %x\n", CR5F);
			       break;
			 }
			 break;
		      case 0xb0:
		         pSiS->ChipFlags |= SiSCF_Is652;
			 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	             "SiS652 revision ID %x\n", CR5F);
			 break;
		      default:
		         pSiS->ChipFlags |= SiSCF_IsM650;
			 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	             "SiSM650 revision ID %x\n", CR5F);
			 break;
		   }
		}
	     }
             break;
	   }
       }
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Hardware supports %s video overlay%s\n",
		pSiS->hasTwoOverlays ? "two" : "one",
		pSiS->hasTwoOverlays ? "s" : "");
    }

    /* Backup VB connection and CRT1 on/off register */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       inSISIDXREG(SISSR, 0x1f, pSiS->oldSR1F);
       inSISIDXREG(SISCR, 0x17, pSiS->oldCR17);
       inSISIDXREG(SISCR, 0x32, pSiS->oldCR32);
       inSISIDXREG(SISCR, 0x36, pSiS->oldCR36);
       inSISIDXREG(SISCR, 0x37, pSiS->oldCR37);
       if(pSiS->VGAEngine == SIS_315_VGA) {
          inSISIDXREG(SISCR, pSiS->myCR63, pSiS->oldCR63);
       }

       pSiS->postVBCR32 = pSiS->oldCR32;
    }

    /* There are some machines out there which require a special
     * setup of the GPIO registers in order to make the Chrontel
     * work. Try to find out if we're running on such a machine.
     * Furthermore, there is some highly customized hardware,
     * which requires some non-standard LVDS timing. Since the
     * vendors don't seem to care about PCI subsystem ID's we
     * need to find out using the BIOS version and date strings.
     */
    pSiS->SiS_Pr->SiS_ChSW = FALSE;
    if(pSiS->Chipset == PCI_CHIP_SIS630) {
       int i = 0;
       do {
	  if(mychswtable[i].subsysVendor == pSiS->PciInfo->subsysVendor &&
	     mychswtable[i].subsysCard == pSiS->PciInfo->subsysCard) {
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	         "PCI subsystem ID found in list for Chrontel/GPIO setup\n");
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		 "Vendor/Card: %s %s (ID %04x)\n",
		  mychswtable[i].vendorName,
		  mychswtable[i].cardName,
		  pSiS->PciInfo->subsysCard);
	     pSiS->SiS_Pr->SiS_ChSW = TRUE;
	     break;
          }
          i++;
       } while(mychswtable[i].subsysVendor != 0);
    }

    if(pSiS->SiS_Pr->SiS_CustomT == CUT_NONE) {
       int i = 0, j;
       unsigned short bversptr = 0;
       BOOLEAN footprint;
       unsigned long chksum = 0;

       if(pSiS->sishw_ext.UseROM) {
          bversptr = pSiS->BIOS[0x16] | (pSiS->BIOS[0x17] << 8);
          for(i=0; i<32768; i++) chksum += pSiS->BIOS[i];
       }

       i = 0;
       do {
	  if( (mycustomttable[i].chipID == pSiS->sishw_ext.jChipType)                 &&
	      ((!strlen(mycustomttable[i].biosversion)) ||
	       (pSiS->sishw_ext.UseROM &&
	       (!strncmp(mycustomttable[i].biosversion, (char *)&pSiS->BIOS[bversptr],
	                strlen(mycustomttable[i].biosversion)))))                     &&
	      ((!strlen(mycustomttable[i].biosdate)) ||
	       (pSiS->sishw_ext.UseROM &&
	       (!strncmp(mycustomttable[i].biosdate, (char *)&pSiS->BIOS[0x2c],
	                strlen(mycustomttable[i].biosdate)))))			      &&
	      ((!mycustomttable[i].bioschksum) ||
	       (pSiS->sishw_ext.UseROM &&
	       (mycustomttable[i].bioschksum == chksum)))			      &&
	      (mycustomttable[i].pcisubsysvendor == pSiS->PciInfo->subsysVendor)      &&
	      (mycustomttable[i].pcisubsyscard == pSiS->PciInfo->subsysCard) ) {
	     footprint = TRUE;
	     for(j=0; j<5; j++) {
	        if(mycustomttable[i].biosFootprintAddr[j]) {
		   if(pSiS->sishw_ext.UseROM) {
	              if(pSiS->BIOS[mycustomttable[i].biosFootprintAddr[j]] !=
		      				mycustomttable[i].biosFootprintData[j])
		         footprint = FALSE;
		   } else footprint = FALSE;
	        }
	     }
	     if(footprint) {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Identified %s %s, special timing applies\n",
		   mycustomttable[i].vendorName, mycustomttable[i].cardName);
	        pSiS->SiS_Pr->SiS_CustomT = mycustomttable[i].SpecialID;
	        break;
	     }
          }
          i++;
       } while(mycustomttable[i].chipID);
    }

    /* Handle ForceCRT1 option */
    if(pSiS->forceCRT1 != -1) {
       if(pSiS->forceCRT1) pSiS->CRT1off = 0;
       else                pSiS->CRT1off = 1;
    } else                 pSiS->CRT1off = -1;

    /* Detect video bridge and sense TV/VGA2 */
    SISVGAPreInit(pScrn);

    /* Detect CRT1 (via DDC1 and DDC2, hence via VGA port; regardless of LCDA) */
    SISCRT1PreInit(pScrn);

    /* Detect LCD (connected via CRT2, regardless of LCDA) and LCD resolution */
    SISLCDPreInit(pScrn);

    /* LCDA only supported under these conditions: */
    if(pSiS->ForceCRT1Type == CRT1_LCDA) {
       if( ((pSiS->sishw_ext.jChipType != SIS_650) &&
            (pSiS->sishw_ext.jChipType < SIS_661))     ||
	   (!(pSiS->VBFlags & (VB_301C | VB_302B | VB_301LV | VB_302LV))) ) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	  	"Chipset/Video bridge does not support LCD-via-CRT1\n");
	  pSiS->ForceCRT1Type = CRT1_VGA;
       } else if(!(pSiS->VBFlags & CRT2_LCD)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	  	"No digitally connected LCD panel found, LCD-via-CRT1 disabled\n");
	  pSiS->ForceCRT1Type = CRT1_VGA;
       }
    }

    /* Setup SD flags */
    pSiS->SiS_SD_Flags |= SiS_SD_ADDLSUPFLAG;

    if(pSiS->VBFlags & (VB_SISTVBRIDGE | VB_CHRONTEL)) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTTV;
    }

#ifdef ENABLE_YPBPR
    if((pSiS->VGAEngine == SIS_315_VGA) &&
       (pSiS->VBFlags & (VB_301C|VB_301LV|VB_302LV|VB_302ELV))) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTYPBPR;
       if((pSiS->Chipset == PCI_CHIP_SIS660) || (pSiS->VBFlags & VB_301C)) {
          pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTYPBPRAR;
       }
    }
    if(pSiS->VBFlags & (VB_301|VB_301B|VB_302B)) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTHIVISION;
    }
#endif

    if(pSiS->VBFlags & CRT2_LCD) {
       if((pSiS->VGAEngine != SIS_300_VGA) || (!(pSiS->VBFlags & VB_TRUMPION))) {
          pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTSCALE;
          if(pSiS->VBFlags & (VB_301|VB_301B|VB_302B|VB_301C)) {
             pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTCENTER;
          }
       }
    }

#ifdef TWDEBUG	/* @@@ TEST @@@ */
    pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTYPBPRAR;
    xf86DrvMsg(0, X_INFO, "TEST: Support Aspect Ratio\n");
#endif

    /* Detect CRT2-TV and PAL/NTSC mode */
    SISTVPreInit(pScrn);

    /* Detect CRT2-VGA */
    SISCRT2PreInit(pScrn);

    /* Backup detected CRT2 devices */
    pSiS->detectedCRT2Devices = pSiS->VBFlags & (CRT2_LCD|CRT2_TV|CRT2_VGA|TV_AVIDEO|TV_SVIDEO|TV_SCART|TV_HIVISION|TV_YPBPR);

    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR)) {
       if((pSiS->ForceTVType != -1) && (pSiS->ForceTVType & TV_YPBPR)) {
          pSiS->ForceTVType = -1;
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "YPbPr TV output not supported\n");
       }
    }

    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTHIVISION)) {
       if((pSiS->ForceTVType != -1) && (pSiS->ForceTVType & TV_HIVISION)) {
          pSiS->ForceTVType = -1;
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "HiVision TV output not supported\n");
       }
    }

    if((pSiS->VBFlags & VB_SISTVBRIDGE) ||
       ((pSiS->VBFlags & VB_CHRONTEL) && (pSiS->ChrontelType == CHRONTEL_701x))) {
       pSiS->SiS_SD_Flags |= (SiS_SD_SUPPORTPALMN | SiS_SD_SUPPORTNTSCJ);
    }
    if((pSiS->VBFlags & VB_SISTVBRIDGE) ||
       ((pSiS->VBFlags & VB_CHRONTEL) && (pSiS->ChrontelType == CHRONTEL_700x))) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTTVPOS;
    }
    if(pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B)) {
       pSiS->SiS_SD_Flags |= (SiS_SD_SUPPORTSCART | SiS_SD_SUPPORTVGA2);
    }
    if(pSiS->VBFlags & VB_CHRONTEL) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTOVERSCAN;
       if(pSiS->ChrontelType == CHRONTEL_700x) {
          pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTSOVER;
       }
    }

    if( ((pSiS->sishw_ext.jChipType == SIS_650) ||
         (pSiS->sishw_ext.jChipType >= SIS_661))                    &&
        (pSiS->VBFlags & (VB_301C | VB_302B | VB_301LV | VB_302LV)) &&
        (pSiS->VBFlags & CRT2_LCD) 			            &&
	(pSiS->VESA != 1) ) {
       pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTLCDA;
    } else {
       /* Paranoia */
       pSiS->ForceCRT1Type = CRT1_VGA;
    }

    pSiS->VBFlags |= pSiS->ForceCRT1Type;

#ifdef TWDEBUG
    xf86DrvMsg(0, X_INFO, "SDFlags %lx\n", pSiS->SiS_SD_Flags);
#endif

    /* Eventually overrule detected CRT2 type
     * If no type forced, use the detected devices in the order TV->LCD->VGA2
     * Since the Chrontel 7005 sometimes delivers wrong detection results,
     * we use a different order on such machines (LCD->TV)
     */
    if(pSiS->ForceCRT2Type == CRT2_DEFAULT) {
       if((pSiS->VBFlags & CRT2_TV) && (!((pSiS->VBFlags & VB_CHRONTEL) && (pSiS->VGAEngine == SIS_300_VGA))))
          pSiS->ForceCRT2Type = CRT2_TV;
       else if((pSiS->VBFlags & CRT2_LCD) && (pSiS->ForceCRT1Type == CRT1_VGA))
          pSiS->ForceCRT2Type = CRT2_LCD;
       else if(pSiS->VBFlags & CRT2_TV)
	  pSiS->ForceCRT2Type = CRT2_TV;
       else if(pSiS->VBFlags & CRT2_VGA)
          pSiS->ForceCRT2Type = CRT2_VGA;
    }

    switch(pSiS->ForceCRT2Type) {
       case CRT2_TV:
          pSiS->VBFlags &= ~(CRT2_LCD | CRT2_VGA);
          if(pSiS->VBFlags & (VB_SISTVBRIDGE | VB_CHRONTEL))
             pSiS->VBFlags |= CRT2_TV;
          else {
             pSiS->VBFlags &= ~(CRT2_TV);
	     xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Hardware does not support TV output\n");
          }
          break;
       case CRT2_LCD:
          pSiS->VBFlags &= ~(CRT2_TV | CRT2_VGA);
          if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (pSiS->VBLCDFlags))
             pSiS->VBFlags |= CRT2_LCD;
          else {
             pSiS->VBFlags &= ~(CRT2_LCD);
	     xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Can't force CRT2 to LCD, no LCD detected\n");
	  }
          break;
       case CRT2_VGA:
          pSiS->VBFlags &= ~(CRT2_TV | CRT2_LCD);
          if(pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B))
	     pSiS->VBFlags |= CRT2_VGA;
	  else {
	     pSiS->VBFlags &= ~(CRT2_VGA);
	     xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	         "Hardware does not support secondary VGA\n");
	  }
          break;
       default:
          pSiS->VBFlags &= ~(CRT2_TV | CRT2_LCD | CRT2_VGA);
    }

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (pSiS->SecondHead)) {
#endif
       xf86DrvMsg(pScrn->scrnIndex, pSiS->CRT1gammaGiven ? X_CONFIG : X_INFO,
       	     "CRT1 gamma correction is %s\n",
             pSiS->CRT1gamma ? "enabled" : "disabled");

       if((pSiS->VGAEngine == SIS_315_VGA) && (!(pSiS->NoXvideo))) {
          xf86DrvMsg(pScrn->scrnIndex, pSiS->XvGammaGiven ? X_CONFIG : X_INFO,
       		"Separate Xv gamma correction for CRT1 is %s\n",
		pSiS->XvGamma ? "enabled" : "disabled");
	  if(pSiS->XvGamma) {
	     xf86DrvMsg(pScrn->scrnIndex, pSiS->XvGammaGiven ? X_CONFIG : X_INFO,
	        "Xv gamma correction: %.3f %.3f %.3f\n",
		(float)((float)pSiS->XvGammaRed / 1000),
		(float)((float)pSiS->XvGammaGreen / 1000),
		(float)((float)pSiS->XvGammaBlue / 1000));
	     if(!pSiS->CRT1gamma) {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		      "Separate Xv gamma corr. only effective if CRT1 gamma corr. is enabled\n");
	     }
	  }
       }
#ifdef SISDUALHEAD
    }
#endif

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if( (pSiS->VBFlags & VB_SISBRIDGE) &&
           (!((pSiS->VBFlags & VB_30xBDH) && (pSiS->VBFlags & CRT2_LCD))) ) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CRT2 gamma correction is %s\n",
		pSiS->CRT2gamma ? "enabled" : "disabled");
       }
    }

    /* Eventually overrule TV Type (SVIDEO, COMPOSITE, SCART, HIVISION, YPBPR) */
    if(pSiS->VBFlags & VB_SISTVBRIDGE) {
       if(pSiS->ForceTVType != -1) {
    	  pSiS->VBFlags &= ~(TV_INTERFACE);
	  if(!(pSiS->VBFlags & VB_CHRONTEL)) {
	     pSiS->VBFlags &= ~(TV_CHSCART | TV_CHYPBPR525I);
	  }
	  pSiS->VBFlags |= pSiS->ForceTVType;
	  if(pSiS->VBFlags & TV_YPBPR) {
	     pSiS->VBFlags &= ~(TV_STANDARD);
	     pSiS->VBFlags &= ~(TV_YPBPRAR);
	     pSiS->VBFlags |= pSiS->ForceYPbPrType;
	     pSiS->VBFlags |= pSiS->ForceYPbPrAR;
	  }
       }
    }

    /* Handle ForceCRT1 option (part 2) */
    pSiS->CRT1changed = FALSE;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       usScratchCR17 = pSiS->oldCR17;
       usScratchCR63 = pSiS->oldCR63;
       usScratchSR1F = pSiS->oldSR1F;
       usScratchCR32 = pSiS->postVBCR32;
       if(pSiS->VESA != 1) {
          /* Copy forceCRT1 option to CRT1off if option is given */
#ifdef SISDUALHEAD
          /* In DHM, handle this option only for master head, not the slave */
          if( (pSiS->forceCRT1 != -1) &&
	       (!(pSiS->DualHeadMode && pSiS->SecondHead)) ) {
#else
          if(pSiS->forceCRT1 != -1) {
#endif
             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "CRT1 detection overruled by ForceCRT1 option\n");
    	     if(pSiS->forceCRT1) {
		 pSiS->CRT1off = 0;
		 if(pSiS->VGAEngine == SIS_300_VGA) {
		    if(!(usScratchCR17 & 0x80)) pSiS->CRT1changed = TRUE;
		 } else {
		    if(usScratchCR63 & 0x40) pSiS->CRT1changed = TRUE;
		 }
		 usScratchCR17 |= 0x80;
		 usScratchCR32 |= 0x20;
		 usScratchCR63 &= ~0x40;
		 usScratchSR1F &= ~0xc0;
	     } else {
	         if( ! ( (pScrn->bitsPerPixel == 8) &&
		         ( (pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) ||
		           ((pSiS->VBFlags & VB_30xBDH) && (pSiS->VBFlags & CRT2_LCD)) ) ) ) {
		    pSiS->CRT1off = 1;
		    if(pSiS->VGAEngine == SIS_300_VGA) {
		       if(usScratchCR17 & 0x80) pSiS->CRT1changed = TRUE;
		    } else {
		       if(!(usScratchCR63 & 0x40)) pSiS->CRT1changed = TRUE;
		    }
		    usScratchCR32 &= ~0x20;
		    /* We must not actually switch off CRT1 before we changed the mode! */
		 }
	     }
	     /* Here we can write to CR17 even on 315 series as we only ENABLE
	      * the bit here
	      */
	     outSISIDXREG(SISCR, 0x17, usScratchCR17);
	     if(pSiS->VGAEngine == SIS_315_VGA) {
	        outSISIDXREG(SISCR, pSiS->myCR63, usScratchCR63);
	     }
	     outSISIDXREG(SISCR, 0x32, usScratchCR32);
	     if(pSiS->CRT1changed) {
                outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
	        usleep(10000);
                outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   		"CRT1 status changed by ForceCRT1 option\n");
	     }
	     outSISIDXREG(SISSR, 0x1f, usScratchSR1F);
          }
       }
       /* Store the new VB connection register contents for later mode changes */
       pSiS->newCR32 = usScratchCR32;
    }

    /* Check if CRT1 used (or needed; this eg. if no CRT2 detected) */
    if(pSiS->VBFlags & VB_VIDEOBRIDGE) {

        /* No CRT2 output? Then we NEED CRT1!
	 * We also need CRT1 if depth = 8 and bridge=LVDS|301B-DH
	 */
        if( (!(pSiS->VBFlags & (CRT2_VGA | CRT2_LCD | CRT2_TV))) ||
	    ( (pScrn->bitsPerPixel == 8) &&
	      ( (pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) ||
	        ((pSiS->VBFlags & VB_30xBDH) && (pSiS->VBFlags & CRT2_LCD)) ) ) ) {
	    pSiS->CRT1off = 0;
	}
	/* No CRT2 output? Then we can't use Xv on CRT2 */
	if(!(pSiS->VBFlags & (CRT2_VGA | CRT2_LCD | CRT2_TV)))
	    pSiS->XvOnCRT2 = FALSE;

    } else { /* no video bridge? */

        /* Then we NEED CRT1... */
        pSiS->CRT1off = 0;
	/* ... and can't use CRT2 for Xv output */
	pSiS->XvOnCRT2 = FALSE;
    }

    /* LCDA? Then we don't switch off CRT1 */
    if(pSiS->VBFlags & CRT1_LCDA) pSiS->CRT1off = 0;

    /* Handle TVStandard option */
    if((pSiS->NonDefaultPAL != -1) || (pSiS->NonDefaultNTSC != -1)) {
       if( (!(pSiS->VBFlags & VB_SISTVBRIDGE)) &&
	   (!((pSiS->VBFlags & VB_CHRONTEL)) && (pSiS->ChrontelType == CHRONTEL_701x)) ) {
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	"PALM, PALN and NTSCJ not supported on this hardware\n");
 	  pSiS->NonDefaultPAL = pSiS->NonDefaultNTSC = -1;
	  pSiS->VBFlags &= ~(TV_PALN | TV_PALM | TV_NTSCJ);
	  pSiS->SiS_SD_Flags &= ~(SiS_SD_SUPPORTPALMN | SiS_SD_SUPPORTNTSCJ);
       }
    }
    if(pSiS->OptTVStand != -1) {
       if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	  if( (!((pSiS->VBFlags & VB_CHRONTEL) && (pSiS->VBFlags & (TV_CHSCART | TV_CHYPBPR525I)))) &&
	      (!(pSiS->VBFlags & (TV_HIVISION | TV_YPBPR))) ) {
    	     pSiS->VBFlags &= ~(TV_PAL | TV_NTSC | TV_PALN | TV_PALM | TV_NTSCJ);
    	     if(pSiS->OptTVStand) {
	        pSiS->VBFlags |= TV_PAL;
	        if(pSiS->NonDefaultPAL == 1)  pSiS->VBFlags |= TV_PALM;
	        else if(!pSiS->NonDefaultPAL) pSiS->VBFlags |= TV_PALN;
	     } else {
	        pSiS->VBFlags |= TV_NTSC;
		if(pSiS->NonDefaultNTSC == 1) pSiS->VBFlags |= TV_NTSCJ;
	     }
	  } else {
	     pSiS->OptTVStand = pSiS->NonDefaultPAL = -1;
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	    	 "Option TVStandard ignored for YPbPr, HiVision and Chrontel-SCART\n");
	  }
       } else if(pSiS->Chipset == PCI_CHIP_SIS6326) {
	  pSiS->SiS6326Flags &= ~SIS6326_TVPAL;
	  if(pSiS->OptTVStand) pSiS->SiS6326Flags |= SIS6326_TVPAL;
       }
    }

    /* SCART only supported for PAL */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if((pSiS->VBFlags & VB_SISBRIDGE) && (pSiS->VBFlags & TV_SCART)) {
	  pSiS->VBFlags &= ~(TV_NTSC | TV_PALN | TV_PALM | TV_NTSCJ);
	  pSiS->VBFlags |= TV_PAL;
	  pSiS->OptTVStand = 1;
	  pSiS->NonDefaultPAL = pSiS->NonDefaultNTSC = -1;
       }
    }

#ifdef SIS_CP
    SIS_CP_DRIVER_RECONFIGOPT
#endif

    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
       if(pSiS->sis6326tvplug != -1) {
          pSiS->SiS6326Flags &= ~(SIS6326_TVSVIDEO | SIS6326_TVCVBS);
	  pSiS->SiS6326Flags |= SIS6326_TVDETECTED;
	  if(pSiS->sis6326tvplug == 1) 	pSiS->SiS6326Flags |= SIS6326_TVCVBS;
	  else 				pSiS->SiS6326Flags |= SIS6326_TVSVIDEO;
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	  	"SiS6326 TV plug type detection overruled by %s\n",
		(pSiS->SiS6326Flags & SIS6326_TVCVBS) ? "COMPOSITE" : "SVIDEO");
       }
    }

    /* Do some checks */
    if(pSiS->OptTVOver != -1) {
       if(pSiS->VBFlags & VB_CHRONTEL) {
	  pSiS->UseCHOverScan = pSiS->OptTVOver;
       } else {
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	    	"CHTVOverscan only supported on CHRONTEL 70xx\n");
          pSiS->UseCHOverScan = -1;
       }
    } else pSiS->UseCHOverScan = -1;

    if(pSiS->sistvedgeenhance != -1) {
       if(!(pSiS->VBFlags & VB_301)) {
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "SISTVEdgeEnhance only supported on SiS301\n");
	  pSiS->sistvedgeenhance = -1;
       }
    }
    if(pSiS->sistvsaturation != -1) {
       if(pSiS->VBFlags & VB_301) {
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "SISTVSaturation not supported on SiS301\n");
	  pSiS->sistvsaturation = -1;
       }
    }

    /* Do some MergedFB mode initialisation */
#ifdef SISMERGED
    if(pSiS->MergedFB) {
       pSiS->CRT2pScrn = xalloc(sizeof(ScrnInfoRec));
       if(!pSiS->CRT2pScrn) {
          SISErrorLog(pScrn, "Failed to allocate memory for 2nd pScrn, %s\n", mergeddisstr);
	  pSiS->MergedFB = FALSE;
       } else {
          memcpy(pSiS->CRT2pScrn, pScrn, sizeof(ScrnInfoRec));
       }
    }
#endif


    /* Determine CRT1<>CRT2 mode
     *     Note: When using VESA or if the bridge is in slavemode, display
     *           is ALWAYS in MIRROR_MODE!
     *           This requires extra checks in functions using this flag!
     *           (see sis_video.c for example)
     */
    if(pSiS->VBFlags & DISPTYPE_DISP2) {
        if(pSiS->CRT1off) {	/* CRT2 only ------------------------------- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
	     	SISErrorLog(pScrn,
		    "CRT1 not detected or forced off. Dual Head mode can't initialize.\n");
	     	if(pSiSEnt) pSiSEnt->DisableDual = TRUE;
	        if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
		if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
		pSiS->pInt = NULL;
		sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
		SISFreeRec(pScrn);
		return FALSE;
	     }
#endif
#ifdef SISMERGED
	     if(pSiS->MergedFB) {
	        if(pSiS->MergedFBAuto) {
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO, mergednocrt1, mergeddisstr);
		} else {
	     	   SISErrorLog(pScrn, mergednocrt1, mergeddisstr);
		}
		if(pSiS->CRT2pScrn) xfree(pSiS->CRT2pScrn);
		pSiS->CRT2pScrn = NULL;
		pSiS->MergedFB = FALSE;
	     }
#endif
	     pSiS->VBFlags |= VB_DISPMODE_SINGLE;
	     /* No CRT1? Then we use the video overlay on CRT2 */
	     pSiS->XvOnCRT2 = TRUE;
	} else			/* CRT1 and CRT2 - mirror or dual head ----- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
		pSiS->VBFlags |= (VB_DISPMODE_DUAL | DISPTYPE_CRT1);
	        if(pSiS->VESA != -1) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"VESA option not used in Dual Head mode. VESA disabled.\n");
		}
		if(pSiSEnt) pSiSEnt->DisableDual = FALSE;
		pSiS->VESA = 0;
	     } else
#endif
#ifdef SISMERGED
	            if(pSiS->MergedFB) {
		 pSiS->VBFlags |= (VB_DISPMODE_MIRROR | DISPTYPE_CRT1);
		 if(pSiS->VESA != -1) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"VESA option not used in MergedFB mode. VESA disabled.\n");
		 }
		 pSiS->VESA = 0;
	     } else
#endif
		 pSiS->VBFlags |= (VB_DISPMODE_MIRROR | DISPTYPE_CRT1);
    } else {			/* CRT1 only ------------------------------- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
	     	SISErrorLog(pScrn,
		   "No CRT2 output selected or no bridge detected. "
		   "Dual Head mode can't initialize.\n");
	        if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
		if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
		pSiS->pInt = NULL;
		sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
		SISFreeRec(pScrn);
		return FALSE;
	     }
#endif
#ifdef SISMERGED
	     if(pSiS->MergedFB) {
	        if(pSiS->MergedFBAuto) {
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO, mergednocrt2, mergeddisstr);
		} else {
	     	   SISErrorLog(pScrn, mergednocrt2, mergeddisstr);
		}
		if(pSiS->CRT2pScrn) xfree(pSiS->CRT2pScrn);
		pSiS->CRT2pScrn = NULL;
		pSiS->MergedFB = FALSE;
	     }
#endif
             pSiS->VBFlags |= (VB_DISPMODE_SINGLE | DISPTYPE_CRT1);
    }

    if((pSiS->VGAEngine == SIS_315_VGA) || (pSiS->VGAEngine == SIS_300_VGA)) {
       if((!pSiS->NoXvideo) && (!pSiS->hasTwoOverlays)) {
	  xf86DrvMsg(pScrn->scrnIndex, from,
	      "Using Xv overlay by default on CRT%d\n",
	      pSiS->XvOnCRT2 ? 2 : 1);
       }
    }

    /* Init Ptrs for Save/Restore functions and calc MaxClock */
    SISDACPreInit(pScrn);

    /* ********** end of VBFlags setup ********** */

    /* VBFlags are initialized now. Back them up for SlaveMode modes. */
    pSiS->VBFlags_backup = pSiS->VBFlags;

    /* Backup CR32,36,37 (in order to write them back after a VT switch) */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       inSISIDXREG(SISCR,0x32,pSiS->myCR32);
       inSISIDXREG(SISCR,0x36,pSiS->myCR36);
       inSISIDXREG(SISCR,0x37,pSiS->myCR37);
    }

    /* Find out about paneldelaycompensation and evaluate option */
#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead)) {
#endif
       if(pSiS->VGAEngine == SIS_300_VGA) {

          if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH)) {
	  
	     /* Save the current PDC if the panel is used at the moment.
	      * This seems by far the safest way to find out about it.
	      * If the system is using an old version of sisfb, we can't
	      * trust the pdc register value. If sisfb saved the pdc for
	      * us, use it.
	      */
	     if(pSiS->sisfbpdc != 0xff) {
	        pSiS->SiS_Pr->PDC = pSiS->sisfbpdc;
	     } else {
	        if(!(pSiS->donttrustpdc)) {
	           unsigned char tmp;
	           inSISIDXREG(SISCR, 0x30, tmp);
	           if(tmp & 0x20) {
	              inSISIDXREG(SISPART1, 0x13, pSiS->SiS_Pr->PDC);
                   } else {
	             xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      	          "Unable to detect LCD PanelDelayCompensation, LCD is not active\n");
	           }
	        } else {
	           xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      	        "Unable to detect LCD PanelDelayCompensation, please update sisfb\n");
	        }
	     }
	     if(pSiS->SiS_Pr->PDC != -1) {
	        pSiS->SiS_Pr->PDC &= 0x3c;
	        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	       	     "Detected LCD PanelDelayCompensation 0x%02x\n",
		     pSiS->SiS_Pr->PDC);
	     }

	     /* If we haven't been able to find out, use our other methods */
	     if(pSiS->SiS_Pr->PDC == -1) {
                int i=0;
                do {
	           if(mypdctable[i].subsysVendor == pSiS->PciInfo->subsysVendor &&
	              mypdctable[i].subsysCard == pSiS->PciInfo->subsysCard) {
	                 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	            "PCI card/vendor identified for non-default PanelDelayCompensation\n");
		         xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		             "Vendor: %s, card: %s (ID %04x), PanelDelayCompensation: 0x%02x\n",
		             mypdctable[i].vendorName, mypdctable[i].cardName,
		             pSiS->PciInfo->subsysCard, mypdctable[i].pdc);
                         if(pSiS->PDC == -1) {
		            pSiS->PDC = mypdctable[i].pdc;
		         } else {
		            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       	        "PanelDelayCompensation overruled by option\n");
		         }
	                 break;
                   }
	           i++;
                } while(mypdctable[i].subsysVendor != 0);
             }

	     if(pSiS->PDC != -1) {
	        if(pSiS->BIOS) {
	           if(pSiS->VBFlags & VB_LVDS) {
	              if(pSiS->BIOS[0x220] & 0x80) {
                         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		             "BIOS contains custom LCD Panel Delay Compensation 0x%02x\n",
		             pSiS->BIOS[0x220] & 0x3c);
	                 pSiS->BIOS[0x220] &= 0x7f;
		      }
	           }
	           if(pSiS->VBFlags & (VB_301B|VB_302B)) {
	              if(pSiS->BIOS[0x220] & 0x80) {
                         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		             "BIOS contains custom LCD Panel Delay Compensation 0x%02x\n",
		               (  (pSiS->VBLCDFlags & VB_LCD_1280x1024) ?
			                 pSiS->BIOS[0x223] : pSiS->BIOS[0x224]  ) & 0x3c);
	                 pSiS->BIOS[0x220] &= 0x7f;
		      }
		   }
	        }
	        pSiS->SiS_Pr->PDC = (pSiS->PDC & 0x3c);
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	              "Using LCD Panel Delay Compensation 0x%02x\n", pSiS->SiS_Pr->PDC);
	     }
	  }

       }  /* SIS_300_VGA */

       if(pSiS->VGAEngine == SIS_315_VGA) {

          unsigned char tmp, tmp2;
	  inSISIDXREG(SISCR, 0x30, tmp);

	  /* Save the current PDC if the panel is used at the moment. */
	  if(pSiS->VBFlags & (VB_301LV | VB_302LV | VB_302ELV)) {

	     if(pSiS->sisfbpdc != 0xff) {
	        pSiS->SiS_Pr->PDC = pSiS->sisfbpdc;
	     }
	     if(pSiS->sisfbpdca != 0xff) {
	        pSiS->SiS_Pr->PDCA = pSiS->sisfbpdca;
	     }

	     if(!pSiS->donttrustpdc) {
	        if((pSiS->sisfbpdc == 0xff) && (pSiS->sisfbpdca == 0xff)) {
		   CARD16 tempa, tempb;
		   inSISIDXREG(SISPART1,0x2d,tmp2);
		   tempa = (tmp2 & 0xf0) >> 3;
		   tempb = (tmp2 & 0x0f) << 1;
		   inSISIDXREG(SISPART1,0x20,tmp2);
		   tempa |= ((tmp2 & 0x40) >> 6);
		   inSISIDXREG(SISPART1,0x35,tmp2);
		   tempb |= ((tmp2 & 0x80) >> 7);
		   inSISIDXREG(SISPART1,0x13,tmp2);
		   if(!pSiS->ROM661New) {
		      if((tmp2 & 0x04) || (tmp & 0x20)) {
		         pSiS->SiS_Pr->PDCA = tempa;
		         pSiS->SiS_Pr->PDC  = tempb;
		      } else {
		         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      	             "Unable to detect PanelDelayCompensation, LCD is not active\n");
		      }
		   } else {
		      if(tmp2 & 0x04) {
		         pSiS->SiS_Pr->PDCA = tempa;
		      } else if(tmp & 0x20) {
		         pSiS->SiS_Pr->PDC  = tempb;
		      } else {
		         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      	             "Unable to detect PanelDelayCompensation, LCD is not active\n");
		      }
		   }
		}
	     } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      	    "Unable to detect PanelDelayCompensation, please update sisfb\n");
	     }
	     if(pSiS->SiS_Pr->PDC != -1) {
	        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      	     "Detected LCD PanelDelayCompensation 0x%02x (for LCD=CRT2)\n",
		     pSiS->SiS_Pr->PDC);
	     }
	     if(pSiS->SiS_Pr->PDCA != -1) {
	        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      	     "Detected LCD PanelDelayCompensation1 0x%02x (for LCD=CRT1)\n",
		     pSiS->SiS_Pr->PDCA);
	     }
	  }

	  /* Let user override (for all bridges) */
	  if(pSiS->VBFlags & (VB_301B | VB_301C | VB_301LV | VB_302LV | VB_302ELV)) {
	     if(pSiS->PDC != -1) {
	        pSiS->SiS_Pr->PDC = pSiS->PDC & 0x1f;
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	      	     "Using LCD PanelDelayCompensation 0x%02x (for LCD=CRT2)\n",
		     pSiS->SiS_Pr->PDC);
	     }
	     if(pSiS->PDCA != -1) {
	        pSiS->SiS_Pr->PDCA = pSiS->PDCA & 0x1f;
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	      	     "Using LCD PanelDelayCompensation1 0x%02x (for LCD=CRT1)\n",
		     pSiS->SiS_Pr->PDCA);
	     }
          }

 	  /* Read the current EMI (if not overruled) */
	  if(pSiS->VBFlags & (VB_302LV | VB_302ELV)) {
	     MessageType from = X_PROBED;
	     if(pSiS->EMI != -1) {
	        pSiS->SiS_Pr->EMI_30 = (pSiS->EMI >> 24) & 0x60;
	        pSiS->SiS_Pr->EMI_31 = (pSiS->EMI >> 16) & 0xff;
	        pSiS->SiS_Pr->EMI_32 = (pSiS->EMI >> 8)  & 0xff;
	        pSiS->SiS_Pr->EMI_33 = pSiS->EMI & 0xff;
		pSiS->SiS_Pr->HaveEMI = pSiS->SiS_Pr->HaveEMILCD = TRUE;
		pSiS->SiS_Pr->OverruleEMI = TRUE;
		from = X_CONFIG;
	     } else if((pSiS->sisfbfound) && (pSiS->sisfb_haveemi)) {
	        pSiS->SiS_Pr->EMI_30 = pSiS->sisfb_emi30;
	        pSiS->SiS_Pr->EMI_31 = pSiS->sisfb_emi31;
	        pSiS->SiS_Pr->EMI_32 = pSiS->sisfb_emi32;
	        pSiS->SiS_Pr->EMI_33 = pSiS->sisfb_emi33;
		pSiS->SiS_Pr->HaveEMI = TRUE;
		if(pSiS->sisfb_haveemilcd) pSiS->SiS_Pr->HaveEMILCD = TRUE;
		pSiS->SiS_Pr->OverruleEMI = FALSE;
	     } else {
	        inSISIDXREG(SISPART4, 0x30, pSiS->SiS_Pr->EMI_30);
		inSISIDXREG(SISPART4, 0x31, pSiS->SiS_Pr->EMI_31);
		inSISIDXREG(SISPART4, 0x32, pSiS->SiS_Pr->EMI_32);
		inSISIDXREG(SISPART4, 0x33, pSiS->SiS_Pr->EMI_33);
		pSiS->SiS_Pr->HaveEMI = TRUE;
		if(tmp & 0x20) pSiS->SiS_Pr->HaveEMILCD = TRUE;
		pSiS->SiS_Pr->OverruleEMI = FALSE;
	     }
	     xf86DrvMsg(pScrn->scrnIndex, from,
	     	   "302LV/302ELV: Using EMI 0x%02x%02x%02x%02x%s\n",
		   pSiS->SiS_Pr->EMI_30,pSiS->SiS_Pr->EMI_31,
		   pSiS->SiS_Pr->EMI_32,pSiS->SiS_Pr->EMI_33,
		   pSiS->SiS_Pr->HaveEMILCD ? " (LCD)" : "");
	  }

       } /* SIS_315_VGA */
#ifdef SISDUALHEAD
    }
#endif

#ifdef SISDUALHEAD
    /* In dual head mode, both heads (currently) share the maxxfbmem equally.
     * If memory sharing is done differently, the following has to be changed;
     * the other modules (eg. accel and Xv) use dhmOffset for hardware
     * pointer settings relative to VideoRAM start and won't need to be changed.
     */
    if(pSiS->DualHeadMode) {
        if(pSiS->SecondHead == FALSE) {
	    /* ===== First head (always CRT2) ===== */
	    /* We use only half of the memory available */
	    pSiS->maxxfbmem /= 2;
	    /* Initialize dhmOffset */
	    pSiS->dhmOffset = 0;
	    /* Copy framebuffer addresses & sizes to entity */
	    pSiSEnt->masterFbAddress = pSiS->FbAddress;
	    pSiSEnt->masterFbSize    = pSiS->maxxfbmem;
	    pSiSEnt->slaveFbAddress  = pSiS->FbAddress + pSiS->maxxfbmem;
	    pSiSEnt->slaveFbSize     = pSiS->maxxfbmem;
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	    		"%ldKB video RAM at 0x%lx available for master head (CRT2)\n",
	    		pSiS->maxxfbmem/1024, pSiS->FbAddress);
	} else {
	    /* ===== Second head (always CRT1) ===== */
	    /* We use only half of the memory available */
	    pSiS->maxxfbmem /= 2;
	    /* Adapt FBAddress */
	    pSiS->FbAddress += pSiS->maxxfbmem;
	    /* Initialize dhmOffset */
	    pSiS->dhmOffset = pSiS->availMem - pSiS->maxxfbmem;
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	    		"%ldKB video RAM at 0x%lx available for slave head (CRT1)\n",
	    		pSiS->maxxfbmem/1024,  pSiS->FbAddress);
	}
    } else
        pSiS->dhmOffset = 0;
#endif

    /* Note: Do not use availMem for anything from now. Use
     * maxxfbmem instead. (availMem does not take dual head
     * mode into account.)
     */

    pSiS->DRIheapstart = pSiS->maxxfbmem;
    pSiS->DRIheapend = pSiS->availMem;
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       pSiS->DRIheapstart = pSiS->DRIheapend = 0;
    } else
#endif
    if(pSiS->DRIheapstart == pSiS->DRIheapend) {
#if 0  /* For future use */
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	  "No memory for DRI heap. Please set the option \"MaxXFBMem\" to\n"
	  "\tlimit the memory XFree should use and leave the rest to DRI\n");
#endif
       pSiS->DRIheapstart = pSiS->DRIheapend = 0;
    }

    /* Now for something completely different: DDC.
     * For 300 and 315/330 series, we provide our
     * own functions (in order to probe CRT2 as well)
     * If these fail, use the VBE.
     * All other chipsets will use VBE. No need to re-invent
     * the wheel there.
     */

    pSiS->pVbe = NULL;
    didddc2 = FALSE;

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if(xf86LoadSubModule(pScrn, "ddc")) {
          int crtnum = 0;
          xf86LoaderReqSymLists(ddcSymbols, NULL);
	  if((pMonitor = SiSDoPrivateDDC(pScrn, &crtnum))) {
	     didddc2 = TRUE;
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED, ddcsstr, crtnum);
	     xf86PrintEDID(pMonitor);
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED, ddcestr, crtnum);
	     xf86SetDDCproperties(pScrn, pMonitor);
	     pScrn->monitor->DDC = pMonitor;
          }
       }
    }

#ifdef SISDUALHEAD
    /* In dual head mode, probe DDC using VBE only for CRT1 (second head) */
    if((pSiS->DualHeadMode) && (!didddc2) && (!pSiS->SecondHead))
         didddc2 = TRUE;
#endif

    if(!didddc2) {
       /* If CRT1 is off or LCDA, skip DDC via VBE */
       if((pSiS->CRT1off) || (pSiS->VBFlags & CRT1_LCDA))
          didddc2 = TRUE;
    }

    /* Now (re-)load and initialize the DDC module */
    if(!didddc2) {

       if(xf86LoadSubModule(pScrn, "ddc")) {

          xf86LoaderReqSymLists(ddcSymbols, NULL);

          /* Now load and initialize VBE module. */
          if(xf86LoadSubModule(pScrn, "vbe")) {
	      xf86LoaderReqSymLists(vbeSymbols, NULL);
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	      pSiS->pVbe = VBEInit(pSiS->pInt,pSiS->pEnt->index);
#else
              pSiS->pVbe = VBEExtendedInit(pSiS->pInt,pSiS->pEnt->index,
	                SET_BIOS_SCRATCH | RESTORE_BIOS_SCRATCH);
#endif
              if(!pSiS->pVbe) {
	         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	            "Could not initialize VBE module for DDC\n");
              }
          } else {
              xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	          "Could not load VBE module for DDC\n");
          }

  	  if(pSiS->pVbe) {
	      if((pMonitor = vbeDoEDID(pSiS->pVbe,NULL))) {
	         xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	      "VBE CRT1 DDC monitor info:\n");
                 xf86SetDDCproperties(pScrn, xf86PrintEDID(pMonitor));
		 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	      "End of VBE CRT1 DDC monitor info:\n");
		 pScrn->monitor->DDC = pMonitor;
              }
          } else {
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Could not retrieve DDC data\n");
	  }
       }
    }

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       pSiS->CRT2pScrn->monitor = xalloc(sizeof(MonRec));
       if(pSiS->CRT2pScrn->monitor) {
          DisplayModePtr tempm = NULL, currentm = NULL, newm = NULL;
          memcpy(pSiS->CRT2pScrn->monitor, pScrn->monitor, sizeof(MonRec));
          pSiS->CRT2pScrn->monitor->DDC = NULL;
	  pSiS->CRT2pScrn->monitor->Modes = NULL;
	  tempm = pScrn->monitor->Modes;
	  while(tempm) {
	     if(!(newm = xalloc(sizeof(DisplayModeRec)))) break;
	     memcpy(newm, tempm, sizeof(DisplayModeRec));
	     if(!(newm->name = xalloc(strlen(tempm->name) + 1))) {
	        xfree(newm);
		break;
	     }
	     strcpy(newm->name, tempm->name);
	     if(!pSiS->CRT2pScrn->monitor->Modes) pSiS->CRT2pScrn->monitor->Modes = newm;
	     if(currentm) {
	        currentm->next = newm;
		newm->prev = currentm;
	     }
	     currentm = newm;
	     tempm = tempm->next;
	  }
          if(pSiS->CRT2HSync) {
             pSiS->CRT2pScrn->monitor->nHsync =
	    	SiSStrToRanges(pSiS->CRT2pScrn->monitor->hsync, pSiS->CRT2HSync, MAX_HSYNC);
          }
          if(pSiS->CRT2VRefresh) {
             pSiS->CRT2pScrn->monitor->nVrefresh =
	    	SiSStrToRanges(pSiS->CRT2pScrn->monitor->vrefresh, pSiS->CRT2VRefresh, MAX_VREFRESH);
          }
	  if((pMonitor = SiSInternalDDC(pSiS->CRT2pScrn, 1))) {
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED, ddcsstr, 2);
	     xf86PrintEDID(pMonitor);
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED, ddcestr, 2);
	     xf86SetDDCproperties(pSiS->CRT2pScrn, pMonitor);
	     pSiS->CRT2pScrn->monitor->DDC = pMonitor;
	     /* use DDC data if no ranges in config file */
	     if(!pSiS->CRT2HSync) {
	        pSiS->CRT2pScrn->monitor->nHsync = 0;
	     }
	     if(!pSiS->CRT2VRefresh) {
	        pSiS->CRT2pScrn->monitor->nVrefresh = 0;
	     }
          } else {
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	     	"Failed to read DDC data for CRT2\n");
	  }
       } else {
          SISErrorLog(pScrn, "Failed to allocate memory for CRT2 monitor, %s.\n",
	  		mergeddisstr);
	  if(pSiS->CRT2pScrn) xfree(pSiS->CRT2pScrn);
    	  pSiS->CRT2pScrn = NULL;
	  pSiS->MergedFB = FALSE;
       }
    }
#endif

    /* If there is no HSync or VRefresh data for the monitor,
     * derive it from DDC data. Done by common layer since
     * 4.3.99.14.
     */
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,14,0)
    if(pScrn->monitor->DDC) {
       if(pScrn->monitor->nHsync <= 0) {
         xf86DrvMsg(pScrn->scrnIndex, X_INFO, subshstr,
#ifdef SISDUALHEAD
			pSiS->DualHeadMode ? (pSiS->SecondHead ? 1 : 2) :
#endif
		 		pSiS->CRT1off ? 2 : 1);
         SiSSetSyncRangeFromEdid(pScrn, 1);
       }
       if(pScrn->monitor->nVrefresh <= 0) {
         xf86DrvMsg(pScrn->scrnIndex, X_INFO, subsvstr,
#ifdef SISDUALHEAD
			pSiS->DualHeadMode ? (pSiS->SecondHead ? 1 : 2) :
#endif
		  		pSiS->CRT1off ? 2 : 1);
         SiSSetSyncRangeFromEdid(pScrn, 0);
       }
    }
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,14,0)
       if(pSiS->CRT2pScrn->monitor->DDC) {
          if(pSiS->CRT2pScrn->monitor->nHsync <= 0) {
             xf86DrvMsg(pScrn->scrnIndex, X_INFO, subshstr, 2);
             SiSSetSyncRangeFromEdid(pSiS->CRT2pScrn, 1);
          }
          if(pSiS->CRT2pScrn->monitor->nVrefresh <= 0) {
             xf86DrvMsg(pScrn->scrnIndex, X_INFO, subsvstr, 2);
             SiSSetSyncRangeFromEdid(pSiS->CRT2pScrn, 0);
          }
       }
#endif

       xf86DrvMsg(pScrn->scrnIndex, X_INFO, crtsetupstr, 1);
    }
#endif
    /* end of DDC */

    /* From here, we mainly deal with clocks and modes */

    /* Set the min pixel clock */
    pSiS->MinClock = 5000;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       pSiS->MinClock = 12000;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n",
                pSiS->MinClock / 1000);

    from = X_PROBED;
    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if(pSiS->pEnt->device->dacSpeeds[0]) {
        int speed = 0;
        switch(pScrn->bitsPerPixel) {
        case 8:
           speed = pSiS->pEnt->device->dacSpeeds[DAC_BPP8];
           break;
        case 16:
           speed = pSiS->pEnt->device->dacSpeeds[DAC_BPP16];
           break;
        case 24:
           speed = pSiS->pEnt->device->dacSpeeds[DAC_BPP24];
           break;
        case 32:
           speed = pSiS->pEnt->device->dacSpeeds[DAC_BPP32];
           break;
        }
        if(speed == 0)
            pSiS->MaxClock = pSiS->pEnt->device->dacSpeeds[0];
        else
            pSiS->MaxClock = speed;
        from = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n",
                pSiS->MaxClock / 1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pSiS->MinClock;
    clockRanges->maxClock = pSiS->MaxClock;
    clockRanges->clockIndex = -1;               /* programmable */
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = TRUE;

    /*
     * Since we have lots of built-in modes for 300/315/330 series
     * with vb support, we replace the given default mode list with our
     * own. In case the video bridge is to be used, we only allow other
     * modes if
     *   -) vbtype is 301, 301B, 301C or 302B, and
     *   -) crt2 device is not TV, and
     *   -) crt1 is not LCDA
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if(!(pSiS->noInternalModes)) {
          BOOLEAN acceptcustommodes = TRUE;
	  BOOLEAN includelcdmodes   = TRUE;
	  BOOLEAN isfordvi          = FALSE;
          if(pSiS->UseVESA) {
	     acceptcustommodes = FALSE;
	     includelcdmodes   = FALSE;
	  }
#ifdef SISDUALHEAD
          if(pSiS->DualHeadMode) {
	     if(!pSiS->SecondHead) {
	        if((pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B)) && (!(pSiS->VBFlags & VB_30xBDH))) {
		   if(!(pSiS->VBFlags & (CRT2_LCD|CRT2_VGA))) includelcdmodes   = FALSE;
		   if(pSiS->VBFlags & CRT2_LCD)               isfordvi          = TRUE;
		   if(pSiS->VBFlags & CRT2_TV)                acceptcustommodes = FALSE;
		} else {
		   acceptcustommodes = FALSE;
		   includelcdmodes   = FALSE;
		}
		clockRanges->interlaceAllowed = FALSE;
	     } else {
	        includelcdmodes = FALSE;
		if(pSiS->VBFlags & CRT1_LCDA) {
		   acceptcustommodes = FALSE;
		   /* Ignore interlace, mode switching code will handle this */
		}
	     }
	  } else
#endif
#ifdef SISMERGED
          if(pSiS->MergedFB) {
	     includelcdmodes = FALSE;
	     if(pSiS->VBFlags & CRT1_LCDA) {
		acceptcustommodes = FALSE;
		/* Ignore interlace, mode switching code will handle this */
	     }
          } else
#endif
          if((pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B))  && (!(pSiS->VBFlags & VB_30xBDH))) {
	     if(!(pSiS->VBFlags & (CRT2_LCD|CRT2_VGA))) includelcdmodes   = FALSE;
	     if(pSiS->VBFlags & CRT2_LCD)               isfordvi          = TRUE;
	     if(pSiS->VBFlags & (CRT2_TV|CRT1_LCDA))    acceptcustommodes = FALSE;
	  } else if(pSiS->VBFlags & (CRT2_ENABLE | CRT1_LCDA)) {
	     acceptcustommodes = FALSE;
	     includelcdmodes   = FALSE;
	  } else {
	     includelcdmodes   = FALSE;
	  }
	  /* Ignore interlace, mode switching code will handle this */

	  pSiS->HaveCustomModes = FALSE;
          if(SiSMakeOwnModeList(pScrn, acceptcustommodes, includelcdmodes, isfordvi, &pSiS->HaveCustomModes)) {
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	         "Replaced %s mode list with built-in modes\n",
	     pSiS->HaveCustomModes ? "default" : "entire");
#ifdef TWDEBUG
             pScrn->modes = pScrn->monitor->Modes;
	     xf86PrintModes(pScrn);
	     pScrn->modes = NULL;
#endif
          } else {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   	"Building list of built-in modes failed, using XFree86 defaults\n");
	  }
       } else {
          pSiS->HaveCustomModes = TRUE;
       }
    }

    /*
     * Add our built-in modes for TV on the 6326
     */
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
       if(pSiS->SiS6326Flags & SIS6326_TVDETECTED) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	 	"Adding %s TV modes for 6326 to mode list:\n",
		(pSiS->SiS6326Flags & SIS6326_TVPAL) ? "PAL" : "NTSC");
          if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
             SiS6326PAL800x600Mode.next = pScrn->monitor->Modes;
             pScrn->monitor->Modes = &SiS6326PAL640x480Mode;
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	     	"\t\"PAL800x600\" \"PAL800x600U\" \"PAL720x540\" \"PAL640x480\"\n");
	  } else {
	     SiS6326NTSC640x480Mode.next = pScrn->monitor->Modes;
             pScrn->monitor->Modes = &SiS6326NTSC640x400Mode;
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	     	"\t\"NTSC640x480\" \"NTSC640x480U\" \"NTSC640x400\"\n");
	  }
       }
    }

    /*
     * Add our built-in hi-res modes on the 6326
     */
    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
       if(pScrn->bitsPerPixel == 8) {
          SiS6326SIS1600x1200_60Mode.next = pScrn->monitor->Modes;
          pScrn->monitor->Modes = &SiS6326SIS1600x1200_60Mode;
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	  	"Adding mode \"SIS1600x1200-60\" (depth 8 only)\n");
       }
       if(pScrn->bitsPerPixel <= 16) {
          SiS6326SIS1280x1024_75Mode.next = pScrn->monitor->Modes;
          pScrn->monitor->Modes = &SiS6326SIS1280x1024_75Mode;
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	  	"Adding mode \"SIS1280x1024-75\" (depth 8, 15 and 16 only)\n");
       }
    }

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,	
    	  "\"Unknown reason\" in the following list means that the mode\n");
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,	
    	  "is not supported on the chipset/bridge/current output device.\n");
    }
	
    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our SISValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    /* Select valid modes from those available */
    /*
     * Assuming min pitch 256, min height 128
     */
    {
       int minpitch, maxpitch, minheight, maxheight;
       minpitch = 256;
       minheight = 128;
       switch(pSiS->VGAEngine) {
       case SIS_OLD_VGA:
       case SIS_530_VGA:
          maxpitch = 2040;
          maxheight = 2048;
          break;
       case SIS_300_VGA:
       case SIS_315_VGA:
          maxpitch = 4088;
          maxheight = 4096;
          break;
       default:
          maxpitch = 2048;
          maxheight = 2048;
          break;
       }
#ifdef SISMERGED
       pSiS->CheckForCRT2 = FALSE;
#endif
       i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                      pScrn->display->modes, clockRanges, NULL,
                      minpitch, maxpitch,
                      pScrn->bitsPerPixel * 8,
		      minheight, maxheight,
                      pScrn->display->virtualX,
                      pScrn->display->virtualY,
                      pSiS->maxxfbmem,
                      LOOKUP_BEST_REFRESH);
    }

    if(i == -1) {
        SISErrorLog(pScrn, "xf86ValidateModes() error\n");
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }

    /* Check the virtual screen against the available memory */
    {
       unsigned long memreq = (pScrn->virtualX * ((pScrn->bitsPerPixel + 7) / 8)) * pScrn->virtualY;

       if(memreq > pSiS->maxxfbmem) {
          SISErrorLog(pScrn,
       		"Virtual screen too big for memory; %ldK needed, %ldK available\n",
		memreq/1024, pSiS->maxxfbmem/1024);
#ifdef SISDUALHEAD
          if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
          if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
          pSiS->pInt = NULL;
          sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
          SISFreeRec(pScrn);
          return FALSE;
       }
    }

    /* Dual Head:
     * -) Go through mode list and mark all those modes as bad,
     *    which are unsuitable for dual head mode.
     * -) Find the highest used pixelclock on the master head.
     */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {

       if(!pSiS->SecondHead) {

          pSiSEnt->maxUsedClock = 0;

          if((p = first = pScrn->modes)) {
             do {
	        n = p->next;

	        /* Modes that require the bridge to operate in SlaveMode
                 * are not suitable for Dual Head mode.
                 */
	        if( (pSiS->VGAEngine == SIS_300_VGA) &&
		    ( (strcmp(p->name, "320x200") == 0) ||
		      (strcmp(p->name, "320x240") == 0) ||
		      (strcmp(p->name, "400x300") == 0) ||
		      (strcmp(p->name, "512x384") == 0) ||
		      (strcmp(p->name, "640x400") == 0) ) )  {
	    	   p->status = MODE_BAD;
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO, notsuitablestr, p->name, "dual head");
		}

		/* Search for the highest clock on first head in order to calculate
	         * max clock for second head (CRT1)
	         */
		if((p->status == MODE_OK) && (p->Clock > pSiSEnt->maxUsedClock)) {
		   pSiSEnt->maxUsedClock = p->Clock;
		}

	        p = n;

             } while (p != NULL && p != first);
	  }
       }
    }
#endif

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if(i == 0 || pScrn->modes == NULL) {
        SISErrorLog(pScrn, "No valid modes found\n");
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }

    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Copy to CurrentLayout */
    pSiS->CurrentLayout.mode = pScrn->currentMode;
    pSiS->CurrentLayout.displayWidth = pScrn->displayWidth;

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, modesforstr, 1);
    }
#endif

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       BOOLEAN acceptcustommodes = TRUE;
       BOOLEAN includelcdmodes   = TRUE;
       BOOLEAN isfordvi          = FALSE;

       xf86DrvMsg(pScrn->scrnIndex, X_INFO, crtsetupstr, 2);

       clockRanges->next = NULL;
       clockRanges->minClock = pSiS->MinClock;
       clockRanges->maxClock = SiSMemBandWidth(pSiS->CRT2pScrn, TRUE);
       clockRanges->clockIndex = -1;
       clockRanges->interlaceAllowed = FALSE;
       clockRanges->doubleScanAllowed = FALSE;
       if(pSiS->VGAEngine == SIS_315_VGA) {
          clockRanges->doubleScanAllowed = TRUE;
       }

       xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock for CRT2 is %d MHz\n",
                clockRanges->minClock / 1000);
       xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Max pixel clock for CRT2 is %d MHz\n",
                clockRanges->maxClock / 1000);

       if((pSiS->VBFlags & (VB_301|VB_301B|VB_301C|VB_302B)) && (!(pSiS->VBFlags & VB_30xBDH))) {
          if(!(pSiS->VBFlags & (CRT2_LCD|CRT2_VGA))) includelcdmodes   = FALSE;
	  if(pSiS->VBFlags & CRT2_LCD)               isfordvi          = TRUE;
	  if(pSiS->VBFlags & CRT2_TV)                acceptcustommodes = FALSE;
       } else {
          includelcdmodes   = FALSE;
	  acceptcustommodes = FALSE;
       }

       pSiS->HaveCustomModes2 = FALSE;
       if(!SiSMakeOwnModeList(pSiS->CRT2pScrn, acceptcustommodes, includelcdmodes, isfordvi, &pSiS->HaveCustomModes2)) {

	  SISErrorLog(pScrn, "Building list of built-in modes for CRT2 failed, %s\n",
	  			mergeddisstr);
	  SiSFreeCRT2Structs(pSiS);
	  pSiS->MergedFB = FALSE;

       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	         "Replaced %s mode list for CRT2 with built-in modes\n",
	         pSiS->HaveCustomModes2 ? "default" : "entire");
       }

    }

    if(pSiS->MergedFB) {

       pSiS->CheckForCRT2 = TRUE;
       i = xf86ValidateModes(pSiS->CRT2pScrn, pSiS->CRT2pScrn->monitor->Modes,
                      pSiS->CRT2pScrn->display->modes, clockRanges,
                      NULL, 256, 4088,
                      pSiS->CRT2pScrn->bitsPerPixel * 8, 128, 4096,
                      pScrn->display->virtualX ? pScrn->virtualX : 0,
                      pScrn->display->virtualY ? pScrn->virtualY : 0,
                      pSiS->maxxfbmem,
                      LOOKUP_BEST_REFRESH);
       pSiS->CheckForCRT2 = FALSE;

       if(i == -1) {
          SISErrorLog(pScrn, "xf86ValidateModes() error, %s.\n", mergeddisstr);
	  SiSFreeCRT2Structs(pSiS);
          pSiS->MergedFB = FALSE;
       }

    }

    if(pSiS->MergedFB) {

       if((p = first = pSiS->CRT2pScrn->modes)) {
          do {
	     n = p->next;
	     if( (pSiS->VGAEngine == SIS_300_VGA) &&
		 ( (strcmp(p->name, "320x200") == 0) ||
		   (strcmp(p->name, "320x240") == 0) ||
		   (strcmp(p->name, "400x300") == 0) ||
		   (strcmp(p->name, "512x384") == 0) ||
		   (strcmp(p->name, "640x400") == 0) ) )  {
	    	p->status = MODE_BAD;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, notsuitablestr, p->name, "MergedFB");
	     }
	     p = n;
	  } while (p != NULL && p != first);
       }

       xf86PruneDriverModes(pSiS->CRT2pScrn);

       if(i == 0 || pSiS->CRT2pScrn->modes == NULL) {
          SISErrorLog(pScrn, "No valid modes found for CRT2; %s\n", mergeddisstr);
	  SiSFreeCRT2Structs(pSiS);
	  pSiS->MergedFB = FALSE;
       }

    }

    if(pSiS->MergedFB) {

       xf86SetCrtcForModes(pSiS->CRT2pScrn, INTERLACE_HALVE_V);

       xf86DrvMsg(pScrn->scrnIndex, X_INFO, modesforstr, 2);

       xf86PrintModes(pSiS->CRT2pScrn);

       pSiS->CRT1Modes = pScrn->modes;
       pSiS->CRT1CurrentMode = pScrn->currentMode;

       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Generating MergedFB mode list\n");

       pScrn->modes = SiSGenerateModeList(pScrn, pSiS->MetaModes,
	            	                  pSiS->CRT1Modes, pSiS->CRT2pScrn->modes,
					  pSiS->CRT2Position);

       if(!pScrn->modes) {

	  SISErrorLog(pScrn, "Failed to parse MetaModes or no modes found. %s.\n",
	  		mergeddisstr);
	  SiSFreeCRT2Structs(pSiS);
	  pScrn->modes = pSiS->CRT1Modes;
	  pSiS->CRT1Modes = NULL;
	  pSiS->MergedFB = FALSE;

       }

    }

    if(pSiS->MergedFB) {

       /* If no virtual dimension was given by the user,
        * calculate a sane one now. Adapts pScrn->virtualX,
	* pScrn->virtualY and pScrn->displayWidth.
	*/
       SiSRecalcDefaultVirtualSize(pScrn);

       pScrn->modes = pScrn->modes->next;  /* We get the last from GenerateModeList(), skip to first */
       pScrn->currentMode = pScrn->modes;

       /* Update CurrentLayout */
       pSiS->CurrentLayout.mode = pScrn->currentMode;
       pSiS->CurrentLayout.displayWidth = pScrn->displayWidth;

    }
#endif

    /* Set display resolution */
#ifdef SISMERGED
    if(pSiS->MergedFB) {
       SiSMergedFBSetDpi(pScrn, pSiS->CRT2pScrn, pSiS->CRT2Position);
    } else
#endif
       xf86SetDpi(pScrn, 0, 0);

    /* Load fb module */
    switch(pScrn->bitsPerPixel) {
      case 8:
      case 16:
      case 24:
      case 32:
	if(!xf86LoadSubModule(pScrn, "fb")) {
           SISErrorLog(pScrn, "Failed to load fb module");
#ifdef SISDUALHEAD
	   if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	   if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	   sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
           SISFreeRec(pScrn);
           return FALSE;
        }
	break;
      default:
        SISErrorLog(pScrn, "Unsupported framebuffer bpp (%d)\n", pScrn->bitsPerPixel);
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }
    xf86LoaderReqSymLists(fbSymbols, NULL);

    /* Load XAA if needed */
    if(!pSiS->NoAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Accel enabled\n");
        if(!xf86LoadSubModule(pScrn, "xaa")) {
	    SISErrorLog(pScrn, "Could not load xaa module\n");
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
            SISFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(xaaSymbols, NULL);
    }

    /* Load shadowfb if needed */
    if(pSiS->ShadowFB) {
        if(!xf86LoadSubModule(pScrn, "shadowfb")) {
	    SISErrorLog(pScrn, "Could not load shadowfb module\n");
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
	    SISFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    /* Load the dri module if requested. */
#ifdef XF86DRI
    if(pSiS->loadDRI) {
       if(xf86LoadSubModule(pScrn, "dri")) {
          xf86LoaderReqSymLists(driSymbols, drmSymbols, NULL);
       } else {
#ifdef SISDUALHEAD
          if(!pSiS->DualHeadMode)
#endif
             xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	  	 "Remove >Load \"dri\"< from the Module section of your XF86Config file\n");
       }
    }
#endif    

    /* Now load and initialize VBE module for VESA and mode restoring. */
    pSiS->UseVESA = 0;
    if(pSiS->VESA == 1) {
       if(!pSiS->pVbe) {
          if(xf86LoadSubModule(pScrn, "vbe")) {
	     xf86LoaderReqSymLists(vbeSymbols, NULL);
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	     pSiS->pVbe = VBEInit(pSiS->pInt,pSiS->pEnt->index);
#else
             pSiS->pVbe = VBEExtendedInit(pSiS->pInt,pSiS->pEnt->index,
	    			SET_BIOS_SCRATCH | RESTORE_BIOS_SCRATCH);
#endif
          }
       }
       if(pSiS->pVbe) {
          vbe = VBEGetVBEInfo(pSiS->pVbe);
          pSiS->vesamajor = (unsigned)(vbe->VESAVersion >> 8);
          pSiS->vesaminor = vbe->VESAVersion & 0xff;
          pSiS->vbeInfo = vbe;
          if(pSiS->VESA == 1) {
             SiSBuildVesaModeList(pScrn, pSiS->pVbe, vbe);
             VBEFreeVBEInfo(vbe);
             pSiS->UseVESA = 1;
          }
       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    	"Could not load and initialize VBE module.%s\n",
		(pSiS->VESA == 1) ? " VESA disabled." : "");
       }
    }    
  
    if(pSiS->pVbe) {
       vbeFree(pSiS->pVbe);
       pSiS->pVbe = NULL;
    }

#ifdef SISDUALHEAD
    xf86SetPrimInitDone(pScrn->entityList[0]);
#endif

    sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);

    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
    pSiS->pInt = NULL;

    if(pSiS->VGAEngine == SIS_315_VGA) pSiS->SiS_SD_Flags |= SiS_SD_SUPPORTXVGAMMA1;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
    	pSiS->SiS_SD_Flags |= SiS_SD_ISDUALHEAD;
	if(pSiS->SecondHead)      pSiS->SiS_SD_Flags |= SiS_SD_ISDHSECONDHEAD;
	else			  pSiS->SiS_SD_Flags &= ~(SiS_SD_SUPPORTXVGAMMA1);
#ifdef PANORAMIX
	if(!noPanoramiXExtension) {
	   pSiS->SiS_SD_Flags |= SiS_SD_ISDHXINERAMA;
	   pSiS->SiS_SD_Flags &= ~(SiS_SD_SUPPORTXVGAMMA1);
	}
#endif
    }
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB)      pSiS->SiS_SD_Flags |= SiS_SD_ISMERGEDFB;
#endif

    if(pSiS->enablesisctrl) pSiS->SiS_SD_Flags |= SiS_SD_ENABLED;

    return TRUE;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
SISMapMem(ScrnInfoPtr pScrn)
{
    SISPtr pSiS;
    int mmioFlags;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif
    pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    pSiSEnt = pSiS->entityPrivate;
#endif

    /*
     * Map IO registers to virtual address space
     */
#if !defined(__alpha__)
    mmioFlags = VIDMEM_MMIO;
#else
    /*
     * For Alpha, we need to map SPARSE memory, since we need
     * byte/short access.
     */
    mmioFlags = VIDMEM_MMIO | VIDMEM_SPARSE;
#endif

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        pSiSEnt->MapCountIOBase++;
        if(!(pSiSEnt->IOBase)) {
	     /* Only map if not mapped previously */
    	     pSiSEnt->IOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                         pSiS->PciTag, pSiS->IOAddress, (pSiS->mmioSize * 1024));
        }
        pSiS->IOBase = pSiSEnt->IOBase;
    } else
#endif
    	pSiS->IOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                        pSiS->PciTag, pSiS->IOAddress, (pSiS->mmioSize * 1024));

    if(pSiS->IOBase == NULL) {
    	SISErrorLog(pScrn, "Could not map MMIO area\n");
        return FALSE;
    }

#ifdef __alpha__
    /*
     * for Alpha, we need to map DENSE memory as well, for
     * setting CPUToScreenColorExpandBase.
     */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        pSiSEnt->MapCountIOBaseDense++;
        if(!(pSiSEnt->IOBaseDense)) {
	     /* Only map if not mapped previously */
	     pSiSEnt->IOBaseDense = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
                    pSiS->PciTag, pSiS->IOAddress, (pSiS->mmioSize * 1024));
	}
	pSiS->IOBaseDense = pSiSEnt->IOBaseDense;
    } else
#endif
    	pSiS->IOBaseDense = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
                    pSiS->PciTag, pSiS->IOAddress, (pSiS->mmioSize * 1024));

    if(pSiS->IOBaseDense == NULL) {
       SISErrorLog(pScrn, "Could not map MMIO dense area\n");
       return FALSE;
    }

#endif /* __alpha__ */

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        pSiSEnt->MapCountFbBase++;
        if(!(pSiSEnt->FbBase)) {
	     /* Only map if not mapped previously */
    	     pSiSEnt->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                         pSiS->PciTag, (unsigned long)pSiS->realFbAddress,
                         pSiS->FbMapSize);
	     pSiS->sishw_ext.pjVideoMemoryAddress = (UCHAR *)pSiSEnt->FbBase;
        }
        pSiS->FbBase = pSiSEnt->FbBase;
     	/* Adapt FbBase (for DHM; dhmOffset is 0 otherwise) */
	pSiS->FbBase += pSiS->dhmOffset;
    } else {
#endif
    	pSiS->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                         pSiS->PciTag, (unsigned long)pSiS->FbAddress,
                         pSiS->FbMapSize);
	pSiS->sishw_ext.pjVideoMemoryAddress = (UCHAR *)pSiS->FbBase;
#ifdef SISDUALHEAD
    }
#endif

    if(pSiS->FbBase == NULL) {
       SISErrorLog(pScrn, "Could not map framebuffer area\n");
       return FALSE;
    }

    return TRUE;
}


/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
SISUnmapMem(ScrnInfoPtr pScrn)
{
    SISPtr pSiS;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif

    pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    pSiSEnt = pSiS->entityPrivate;
#endif

/* In dual head mode, we must not unmap if the other head still
 * assumes memory as mapped
 */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        if(pSiSEnt->MapCountIOBase) {
	    pSiSEnt->MapCountIOBase--;
	    if((pSiSEnt->MapCountIOBase == 0) || (pSiSEnt->forceUnmapIOBase)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->IOBase, (pSiS->mmioSize * 1024));
	    	pSiSEnt->IOBase = NULL;
		pSiSEnt->MapCountIOBase = 0;
		pSiSEnt->forceUnmapIOBase = FALSE;
	    }
	    pSiS->IOBase = NULL;
	}
#ifdef __alpha__
	if(pSiSEnt->MapCountIOBaseDense) {
	    pSiSEnt->MapCountIOBaseDense--;
	    if((pSiSEnt->MapCountIOBaseDense == 0) || (pSiSEnt->forceUnmapIOBaseDense)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->IOBaseDense, (pSiS->mmioSize * 1024));
	    	pSiSEnt->IOBaseDense = NULL;
		pSiSEnt->MapCountIOBaseDense = 0;
		pSiSEnt->forceUnmapIOBaseDense = FALSE;
	    }
	    pSiS->IOBaseDense = NULL;
	}
#endif /* __alpha__ */
	if(pSiSEnt->MapCountFbBase) {
	    pSiSEnt->MapCountFbBase--;
	    if((pSiSEnt->MapCountFbBase == 0) || (pSiSEnt->forceUnmapFbBase)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->FbBase, pSiS->FbMapSize);
	    	pSiSEnt->FbBase = NULL;
		pSiSEnt->MapCountFbBase = 0;
		pSiSEnt->forceUnmapFbBase = FALSE;

	    }
	    pSiS->FbBase = NULL;
	}
    } else {
#endif
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiS->IOBase, (pSiS->mmioSize * 1024));
    	pSiS->IOBase = NULL;
#ifdef __alpha__
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiS->IOBaseDense, (pSiS->mmioSize * 1024));
    	pSiS->IOBaseDense = NULL;
#endif
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiS->FbBase, pSiS->FbMapSize);
    	pSiS->FbBase = NULL;
#ifdef SISDUALHEAD
    }
#endif
    return TRUE;
}

/*
 * This function saves the video state.
 */
static void
SISSave(ScrnInfoPtr pScrn)
{
    SISPtr pSiS;
    vgaRegPtr vgaReg;
    SISRegPtr sisReg;

    pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    /* We always save master & slave */
    if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif

    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    sisReg = &pSiS->SavedReg;

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (SiSBridgeIsInSlaveMode(pScrn))) {
          vgaHWSave(pScrn, vgaReg, VGA_SR_CMAP | VGA_SR_MODE);
	  SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO+0x30);
	  SiSSetLVDSetc(pSiS->SiS_Pr, &pSiS->sishw_ext, 0);
	  SiS_GetVBType(pSiS->SiS_Pr, &pSiS->sishw_ext);
	  SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext);
	  vgaHWSave(pScrn, vgaReg, VGA_SR_FONTS);
	  SiS_EnableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext);
       } else {
          vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);
       }
    } else {
       vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);
    }

    sisSaveUnlockExtRegisterLock(pSiS,&sisReg->sisRegs3C4[0x05],&sisReg->sisRegs3D4[0x80]);

    (*pSiS->SiSSave)(pScrn, sisReg);

    if(pSiS->UseVESA) SISVESASaveRestore(pScrn, MODE_SAVE);

    /* "Save" these again as they may have been changed prior to SISSave() call */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       sisReg->sisRegs3C4[0x1f] = pSiS->oldSR1F;
       sisReg->sisRegs3D4[0x17] = pSiS->oldCR17;
       if(vgaReg->numCRTC >= 0x17) vgaReg->CRTC[0x17] = pSiS->oldCR17;
       sisReg->sisRegs3D4[0x32] = pSiS->oldCR32;
       sisReg->sisRegs3D4[0x36] = pSiS->oldCR36;
       sisReg->sisRegs3D4[0x37] = pSiS->oldCR37;
       if(pSiS->VGAEngine == SIS_315_VGA) {
	  sisReg->sisRegs3D4[pSiS->myCR63] = pSiS->oldCR63;
       }
    }
}

static void
SiS_WriteAttr(SISPtr pSiS, int index, int value)
{
    (void) inb(pSiS->IODBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);
    index |= 0x20;
    outb(pSiS->IODBase + VGA_ATTR_INDEX, index);
    outb(pSiS->IODBase + VGA_ATTR_DATA_W, value);
}

static int
SiS_ReadAttr(SISPtr pSiS, int index)
{
    (void) inb(pSiS->IODBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);
    index |= 0x20;
    outb(pSiS->IODBase + VGA_ATTR_INDEX, index);
    return(inb(pSiS->IODBase + VGA_ATTR_DATA_R));
}

#define SIS_FONTS_SIZE (8 * 8192)

static void
SiS_SaveFonts(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char miscOut, attr10, gr4, gr5, gr6, seq2, seq4, scrn;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    CARD8 *vgaIOBase = (CARD8 *)VGAHWPTR(pScrn)->IOBase;
#else
    pointer vgaIOBase = VGAHWPTR(pScrn)->Base;
#endif

    if(pSiS->fonts) return;

    /* If in graphics mode, don't save anything */
    attr10 = SiS_ReadAttr(pSiS, 0x10);
    if(attr10 & 0x01) return;

    if(!(pSiS->fonts = xalloc(SIS_FONTS_SIZE * 2))) {
       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
       		"Could not save console fonts, mem allocation failed\n");
       return;
    }

    /* save the registers that are needed here */
    miscOut = inSISREG(SISMISCR);
    inSISIDXREG(SISGR, 0x04, gr4);
    inSISIDXREG(SISGR, 0x05, gr5);
    inSISIDXREG(SISGR, 0x06, gr6);
    inSISIDXREG(SISSR, 0x02, seq2);
    inSISIDXREG(SISSR, 0x04, seq4);

    /* Force into color mode */
    outSISREG(SISMISCW, miscOut | 0x01);

    inSISIDXREG(SISSR, 0x01, scrn);
    outSISIDXREG(SISSR, 0x00, 0x01);
    outSISIDXREG(SISSR, 0x01, scrn | 0x20);
    outSISIDXREG(SISSR, 0x00, 0x03);

    SiS_WriteAttr(pSiS, 0x10, 0x01);  /* graphics mode */

    /*font1 */
    outSISIDXREG(SISSR, 0x02, 0x04);  /* write to plane 2 */
    outSISIDXREG(SISSR, 0x04, 0x06);  /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x02);  /* read plane 2 */
    outSISIDXREG(SISGR, 0x05, 0x00);  /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05);  /* set graphics */
    slowbcopy_frombus(vgaIOBase, pSiS->fonts, SIS_FONTS_SIZE);

    /* font2 */
    outSISIDXREG(SISSR, 0x02, 0x08);  /* write to plane 3 */
    outSISIDXREG(SISSR, 0x04, 0x06);  /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x03);  /* read plane 3 */
    outSISIDXREG(SISGR, 0x05, 0x00);  /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05);  /* set graphics */
    slowbcopy_frombus(vgaIOBase, pSiS->fonts + SIS_FONTS_SIZE, SIS_FONTS_SIZE);

    inSISIDXREG(SISSR, 0x01, scrn);
    outSISIDXREG(SISSR, 0x00, 0x01);
    outSISIDXREG(SISSR, 0x01, scrn & ~0x20);
    outSISIDXREG(SISSR, 0x00, 0x03);

    /* Restore clobbered registers */
    SiS_WriteAttr(pSiS, 0x10, attr10);
    outSISIDXREG(SISSR, 0x02, seq2);
    outSISIDXREG(SISSR, 0x04, seq4);
    outSISIDXREG(SISGR, 0x04, gr4);
    outSISIDXREG(SISGR, 0x05, gr5);
    outSISIDXREG(SISGR, 0x06, gr6);
    outSISREG(SISMISCW, miscOut);
}

static void
SiS_RestoreFonts(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char miscOut, attr10, gr1, gr3, gr4, gr5, gr6, gr8, seq2, seq4, scrn;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    CARD8 *vgaIOBase = (CARD8 *)VGAHWPTR(pScrn)->IOBase;
#else
    pointer vgaIOBase = VGAHWPTR(pScrn)->Base;
#endif

    if(!pSiS->fonts) return;

    /* save the registers that are needed here */
    miscOut = inSISREG(SISMISCR);
    attr10 = SiS_ReadAttr(pSiS, 0x10);
    inSISIDXREG(SISGR, 0x01, gr1);
    inSISIDXREG(SISGR, 0x03, gr3);
    inSISIDXREG(SISGR, 0x04, gr4);
    inSISIDXREG(SISGR, 0x05, gr5);
    inSISIDXREG(SISGR, 0x06, gr6);
    inSISIDXREG(SISGR, 0x08, gr8);
    inSISIDXREG(SISSR, 0x02, seq2);
    inSISIDXREG(SISSR, 0x04, seq4);

    /* Force into color mode */
    outSISREG(SISMISCW, miscOut | 0x01);
    inSISIDXREG(SISSR, 0x01, scrn);
    outSISIDXREG(SISSR, 0x00, 0x01);
    outSISIDXREG(SISSR, 0x01, scrn | 0x20);
    outSISIDXREG(SISSR, 0x00, 0x03);

    SiS_WriteAttr(pSiS, 0x10, 0x01);	  /* graphics mode */
    if(pScrn->depth == 4) {
       outSISIDXREG(SISGR, 0x03, 0x00);  /* don't rotate, write unmodified */
       outSISIDXREG(SISGR, 0x08, 0xFF);  /* write all bits in a byte */
       outSISIDXREG(SISGR, 0x01, 0x00);  /* all planes come from CPU */
    }

    outSISIDXREG(SISSR, 0x02, 0x04); /* write to plane 2 */
    outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x02); /* read plane 2 */
    outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
    slowbcopy_tobus(pSiS->fonts, vgaIOBase, SIS_FONTS_SIZE);

    outSISIDXREG(SISSR, 0x02, 0x08); /* write to plane 3 */
    outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x03); /* read plane 3 */
    outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
    slowbcopy_tobus(pSiS->fonts + SIS_FONTS_SIZE, vgaIOBase, SIS_FONTS_SIZE);

    inSISIDXREG(SISSR, 0x01, scrn);
    outSISIDXREG(SISSR, 0x00, 0x01);
    outSISIDXREG(SISSR, 0x01, scrn & ~0x20);
    outSISIDXREG(SISSR, 0x00, 0x03);

    /* restore the registers that were changed */
    outSISREG(SISMISCW, miscOut);
    SiS_WriteAttr(pSiS, 0x10, attr10);
    outSISIDXREG(SISGR, 0x01, gr1);
    outSISIDXREG(SISGR, 0x03, gr3);
    outSISIDXREG(SISGR, 0x04, gr4);
    outSISIDXREG(SISGR, 0x05, gr5);
    outSISIDXREG(SISGR, 0x06, gr6);
    outSISIDXREG(SISGR, 0x08, gr8);
    outSISIDXREG(SISSR, 0x02, seq2);
    outSISIDXREG(SISSR, 0x04, seq4);
}

#undef SIS_FONTS_SIZE

/* VESASaveRestore taken from vesa driver */
static void
SISVESASaveRestore(ScrnInfoPtr pScrn, vbeSaveRestoreFunction function)
{
    SISPtr pSiS = SISPTR(pScrn);

    /* Query amount of memory to save state */
    if((function == MODE_QUERY) ||
       (function == MODE_SAVE && pSiS->state == NULL)) {

       /* Make sure we save at least this information in case of failure */
       (void)VBEGetVBEMode(pSiS->pVbe, &pSiS->stateMode);
       SiS_SaveFonts(pScrn);

       if(pSiS->vesamajor > 1) {
	  if(!VBESaveRestore(pSiS->pVbe, function, (pointer)&pSiS->state,
				&pSiS->stateSize, &pSiS->statePage)) {
	     return;
	  }
       }
    }

    /* Save/Restore Super VGA state */
    if(function != MODE_QUERY) {

       if(pSiS->vesamajor > 1) {
	  if(function == MODE_RESTORE) {
	     memcpy(pSiS->state, pSiS->pstate, pSiS->stateSize);
	  }

	  if(VBESaveRestore(pSiS->pVbe,function,(pointer)&pSiS->state,
			    &pSiS->stateSize,&pSiS->statePage) &&
	     (function == MODE_SAVE)) {
	     /* don't rely on the memory not being touched */
	     if(!pSiS->pstate) {
		pSiS->pstate = xalloc(pSiS->stateSize);
	     }
	     memcpy(pSiS->pstate, pSiS->state, pSiS->stateSize);
	  }
       }

       if(function == MODE_RESTORE) {
	  VBESetVBEMode(pSiS->pVbe, pSiS->stateMode, NULL);
	  SiS_RestoreFonts(pScrn);
       }

    }
}

/*
 * Initialise a new mode.  This is currently done using the
 * "initialise struct, restore/write struct to HW" model for
 * the old chipsets (5597/530/6326). For newer chipsets,
 * we use our own mode switching code (or VESA).
 */

static Bool
SISModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    SISPtr pSiS = SISPTR(pScrn);
    SISRegPtr sisReg;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif

    andSISIDXREG(SISCR,0x11,0x7f);   	/* Unlock CRTC registers */

    SISModifyModeInfo(mode);		/* Quick check of the mode parameters */

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
       SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO+0x30);
    }

    if(pSiS->UseVESA) {  /* With VESA: */

#ifdef SISDUALHEAD
       /* No dual head mode when using VESA */
       if(pSiS->SecondHead) return TRUE;
#endif

       pScrn->vtSema = TRUE;

       /*
	* This order is required:
	* The video bridge needs to be adjusted before the
	* BIOS is run as the BIOS sets up CRT2 according to
	* these register settings.
	* After the BIOS is run, the bridges and turboqueue
	* registers need to be readjusted as the BIOS may
	* very probably have messed them up.
	*/
       if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
	  SiSPreSetMode(pScrn, mode, SIS_MODE_SIMU);
       }
       if(!SiSSetVESAMode(pScrn, mode)) {
	  SISErrorLog(pScrn, "SiSSetVESAMode() failed\n");
	  return FALSE;
       }
       sisSaveUnlockExtRegisterLock(pSiS,NULL,NULL);
       if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
	  SiSPreSetMode(pScrn, mode, SIS_MODE_SIMU);
	  SiSPostSetMode(pScrn, &pSiS->ModeReg);
       }
#ifdef TWDEBUG
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "REAL REGISTER CONTENTS AFTER SETMODE:\n");
#endif
       if(!(*pSiS->ModeInit)(pScrn, mode)) {
	  SISErrorLog(pScrn, "ModeInit() failed\n");
	  return FALSE;
       }

       vgaHWProtect(pScrn, TRUE);
       (*pSiS->SiSRestore)(pScrn, &pSiS->ModeReg);
       vgaHWProtect(pScrn, FALSE);

    } else { /* Without VESA: */

#ifdef SISDUALHEAD
       if(pSiS->DualHeadMode) {

          if(!(*pSiS->ModeInit)(pScrn, mode)) {
	     SISErrorLog(pScrn, "ModeInit() failed\n");
	     return FALSE;
	  }

	  pScrn->vtSema = TRUE;

	  pSiSEnt = pSiS->entityPrivate;

	  if(!(pSiS->SecondHead)) {
	     /* Head 1 (master) is always CRT2 */
	     SiSPreSetMode(pScrn, mode, SIS_MODE_CRT2);
	     if(!SiSBIOSSetModeCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, mode, pSiS->IsCustom)) {
		SISErrorLog(pScrn, "SiSBIOSSetModeCRT2() failed\n");
		return FALSE;
	     }
	     SiSPostSetMode(pScrn, &pSiS->ModeReg);
	     SISAdjustFrame(pSiSEnt->pScrn_2->scrnIndex,
		            pSiSEnt->pScrn_2->frameX0,
		            pSiSEnt->pScrn_2->frameY0, 0);
	  } else {
	     /* Head 2 (slave) is always CRT1 */
	     SiSPreSetMode(pScrn, mode, SIS_MODE_CRT1);
	     if(!SiSBIOSSetModeCRT1(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, mode, pSiS->IsCustom)) {
		SISErrorLog(pScrn, "SiSBIOSSetModeCRT1() failed\n");
		return FALSE;
	     }
	     SiSPostSetMode(pScrn, &pSiS->ModeReg);
	     SISAdjustFrame(pSiSEnt->pScrn_1->scrnIndex,
		            pSiSEnt->pScrn_1->frameX0,
		            pSiSEnt->pScrn_1->frameY0, 0);
	  }

       } else {
#endif

	  if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

             if(!(*pSiS->ModeInit)(pScrn, mode)) {
		SISErrorLog(pScrn, "ModeInit() failed\n");
	        return FALSE;
	     }

	     pScrn->vtSema = TRUE;

#ifdef SISMERGED
	     if(pSiS->MergedFB) {

	        xf86DrvMsg(0, X_INFO, "Setting MergedFB mode %dx%d\n",
			   	mode->HDisplay, mode->VDisplay);

		SiSPreSetMode(pScrn, mode, SIS_MODE_CRT1);

		if(!SiSBIOSSetModeCRT1(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn,
		                       ((SiSMergedDisplayModePtr)mode->Private)->CRT1,
				       pSiS->IsCustom)) {
 		   SISErrorLog(pScrn, "SiSBIOSSetModeCRT1() failed\n");
	   	   return FALSE;
		}

		SiSPreSetMode(pScrn, mode, SIS_MODE_CRT2);

		if(!SiSBIOSSetModeCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn,
		                       ((SiSMergedDisplayModePtr)mode->Private)->CRT2,
				       pSiS->IsCustomCRT2)) {
	 	   SISErrorLog(pScrn, "SiSBIOSSetModeCRT2() failed\n");
		   return FALSE;
	        }

	     } else {
#endif

		if(pSiS->VBFlags & CRT1_LCDA) {
	           SiSPreSetMode(pScrn, mode, SIS_MODE_CRT1);
	           if(!SiSBIOSSetModeCRT1(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn,
		                   mode, pSiS->IsCustom)) {
		      SISErrorLog(pScrn, "SiSBIOSSetModeCRT1() failed\n");
		      return FALSE;
		   }
		   SiSPreSetMode(pScrn, mode, SIS_MODE_CRT2);
	           if(!SiSBIOSSetModeCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn,
		                   mode, pSiS->IsCustom)) {
		      SISErrorLog(pScrn, "SiSBIOSSetModeCRT2() failed\n");
		      return FALSE;
		   }
		} else {
		   SiSPreSetMode(pScrn, mode, SIS_MODE_SIMU);
	           if(!SiSBIOSSetMode(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn,
		                   mode, pSiS->IsCustom)) {
		      SISErrorLog(pScrn, "SiSBIOSSetModeCRT() failed\n");
		      return FALSE;
		   }
		}

#ifdef SISMERGED
	     }
#endif
	     SiSPostSetMode(pScrn, &pSiS->ModeReg);

#ifdef TWDEBUG
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VBFlags %lx\n", pSiS->VBFlags);
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"REAL REGISTER CONTENTS AFTER SETMODE:\n");
             (*pSiS->ModeInit)(pScrn, mode);
#endif

	  } else {

	     /* For other chipsets, use the old method */

	     /* Initialise the ModeReg values */
    	     if(!vgaHWInit(pScrn, mode)) {
	        SISErrorLog(pScrn, "vgaHWInit() failed\n");
	        return FALSE;
	     }

	     /* Reset our PIOOffset as vgaHWInit might have reset it */
      	     VGAHWPTR(pScrn)->PIOOffset = pSiS->IODBase + (pSiS->PciInfo->ioBase[2] & 0xFFFC) - 0x380;

	     /* Prepare the register contents */
	     if(!(*pSiS->ModeInit)(pScrn, mode)) {
	        SISErrorLog(pScrn, "ModeInit() failed\n");
	        return FALSE;
	     }

	     pScrn->vtSema = TRUE;

	     /* Program the registers */
	     vgaHWProtect(pScrn, TRUE);
	     vgaReg = &hwp->ModeReg;
	     sisReg = &pSiS->ModeReg;

	     vgaReg->Attribute[0x10] = 0x01;
    	     if(pScrn->bitsPerPixel > 8) {
	    	vgaReg->Graphics[0x05] = 0x00;
	     }

    	     vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);

	     (*pSiS->SiSRestore)(pScrn, sisReg);

	     if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
	        SiS6326PostSetMode(pScrn, &pSiS->ModeReg);
	     }

#ifdef TWDEBUG
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"REAL REGISTER CONTENTS AFTER SETMODE:\n");
             (*pSiS->ModeInit)(pScrn, mode);
#endif

  	     vgaHWProtect(pScrn, FALSE);
	  }
#ifdef SISDUALHEAD
       }
#endif
    }

    /* Update Currentlayout */
    pSiS->CurrentLayout.mode = mode;

    return TRUE;
}

static Bool
SiSSetVESAMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    SISPtr pSiS;
    int mode;

    pSiS = SISPTR(pScrn);

    if(!(mode = SiSCalcVESAModeIndex(pScrn, pMode))) return FALSE;

    mode |= (1 << 15);	/* Don't clear framebuffer */
    mode |= (1 << 14); 	/* Use linear adressing */

    if(VBESetVBEMode(pSiS->pVbe, mode, NULL) == FALSE) {
       SISErrorLog(pScrn, "Setting VESA mode 0x%x failed\n",
	             	mode & 0x0fff);
       return (FALSE);
    }

    if(pMode->HDisplay != pScrn->virtualX) {
       VBESetLogicalScanline(pSiS->pVbe, pScrn->virtualX);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
    	"Setting VESA mode 0x%x succeeded\n",
	mode & 0x0fff);

    return (TRUE);
}

static void
SISSpecialRestore(ScrnInfoPtr pScrn)
{
    SISPtr    pSiS = SISPTR(pScrn);
    SISRegPtr sisReg = &pSiS->SavedReg;
    unsigned char temp;
    int i;

    /* 1.11.04 and later for 651 and 301B(DH) do strange register
     * fiddling after the usual mode change. This happens
     * depending on the result of a call of int 2f (with
     * ax=0x1680) and if modeno <= 0x13. I have no idea if
     * that is specific for the 651 or that very machine.
     * So this perhaps requires some more checks in the beginning
     * (although it should not do any harm on other chipsets/bridges
     * etc.) However, even if I call the VBE to restore mode 0x03,
     * these registers don't get restored correctly, possibly
     * because that int-2f-call for some reason results non-zero. So
     * what I do here is to restore these few registers
     * manually.
     */

    if(!(pSiS->ChipFlags & SiSCF_Is65x)) return;
    inSISIDXREG(SISCR, 0x34, temp);
    temp &= 0x7f;
    if(temp > 0x13) return;

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL,NULL);
#endif

    SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext);

    outSISIDXREG(SISCAP, 0x3f, sisReg->sisCapt[0x3f]);
    outSISIDXREG(SISCAP, 0x00, sisReg->sisCapt[0x00]);
    for(i = 0; i < 0x4f; i++) {
       outSISIDXREG(SISCAP, i, sisReg->sisCapt[i]);
    }
    outSISIDXREG(SISVID, 0x32, (sisReg->sisVid[0x32] & ~0x05));
    outSISIDXREG(SISVID, 0x30, sisReg->sisVid[0x30]);
    outSISIDXREG(SISVID, 0x32, ((sisReg->sisVid[0x32] & ~0x04) | 0x01));
    outSISIDXREG(SISVID, 0x30, sisReg->sisVid[0x30]);

    if(!(pSiS->ChipFlags & SiSCF_Is651)) return;
    if(!(pSiS->VBFlags & VB_SISBRIDGE)) return;

    inSISIDXREG(SISCR, 0x30, temp);
    if(temp & 0x40) {
       unsigned char myregs[] = {
       			0x2f, 0x08, 0x09, 0x03, 0x0a, 0x0c,
			0x0b, 0x0d, 0x0e, 0x12, 0x0f, 0x10,
			0x11, 0x04, 0x05, 0x06, 0x07, 0x00,
			0x2e
       };
       for(i = 0; i <= 18; i++) {
          outSISIDXREG(SISPART1, myregs[i], sisReg->VBPart1[myregs[i]]);
       }
    } else if((temp & 0x20) || (temp & 0x9c)) {
       unsigned char myregs[] = {
       			0x04, 0x05, 0x06, 0x07, 0x00, 0x2e
       };
       for(i = 0; i <= 5; i++) {
          outSISIDXREG(SISPART1, myregs[i], sisReg->VBPart1[myregs[i]]);
       }
    }
}

/*
 * Restore the initial mode. To be used internally only!
 */
static void
SISRestore(ScrnInfoPtr pScrn)
{
    SISPtr    pSiS = SISPTR(pScrn);
    SISRegPtr sisReg = &pSiS->SavedReg;
    vgaHWPtr  hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &hwp->SavedReg;
    Bool      doit = FALSE, doitlater = FALSE;
    Bool      vesasuccess = FALSE;
    
    /* WARNING: Don't ever touch this. It now seems to work on
     * all chipset/bridge combinations - but finding out the
     * correct combination was pure hell.
     */

    /* Wait for the accelerators */
    if(pSiS->AccelInfoPtr) {
       (*pSiS->AccelInfoPtr->Sync)(pScrn);
    }

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {

#ifdef SISDUALHEAD
       /* We always restore master AND slave */
       if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif

       /* We must not disable the sequencer if the bridge is in SlaveMode! */
       if(!(SiSBridgeIsInSlaveMode(pScrn))) {
          vgaHWProtect(pScrn, TRUE);
       }

#ifdef UNLOCK_ALWAYS
       sisSaveUnlockExtRegisterLock(pSiS, NULL,NULL);
#endif

       /* First, restore CRT1 on/off and VB connection registers */
       outSISIDXREG(SISCR, 0x32, pSiS->oldCR32);
       if(!(pSiS->oldCR17 & 0x80)) {			/* CRT1 was off */
          if(!(SiSBridgeIsInSlaveMode(pScrn))) {        /* Bridge is NOT in SlaveMode now -> do it */
	     doit = TRUE;
	  } else {
	     doitlater = TRUE;
	  }
       } else {						/* CRT1 was on -> do it now */
          doit = TRUE;
       }
       
       if(doit) {
          outSISIDXREG(SISCR, 0x17, pSiS->oldCR17);
       }
       if(pSiS->VGAEngine == SIS_315_VGA) {
          outSISIDXREG(SISCR, pSiS->myCR63, pSiS->oldCR63);
       }

       outSISIDXREG(SISSR, 0x1f, pSiS->oldSR1F);

       /* For 30xB/LV, restoring the registers does not
        * work. We "manually" set the old mode, instead.
	* The same applies for SiS730 machines with LVDS.
	* Finally, this behavior can be forced by setting
	* the option RestoreBySetMode.
        */
        if( ( (pSiS->restorebyset) ||
	      (pSiS->VBFlags & (VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV|VB_302ELV)) ||
	      ((pSiS->sishw_ext.jChipType == SIS_730) && (pSiS->VBFlags & VB_LVDS)) ) &&
	    (pSiS->OldMode) ) {

	   Bool changedmode = FALSE;
	   
           xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	         "Restoring by setting old mode 0x%02x\n", pSiS->OldMode);
		 
           if(((pSiS->OldMode <= 0x13) || (!pSiS->sisfbfound)) && (pSiS->pVbe)) {
	      int vmode = SiSTranslateToVESA(pScrn, pSiS->OldMode);
	      if(vmode > 0) {
	         if(vmode > 0x13) vmode |= ((1 << 15) | (1 << 14));
                 if(VBESetVBEMode(pSiS->pVbe, vmode, NULL) == TRUE) {
	            SISSpecialRestore(pScrn);
		    SiS_GetSetModeID(pScrn,pSiS->OldMode);
	      	    vesasuccess = TRUE;
	         } else {
	            xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		 	"VBE failed to restore mode 0x%x\n", pSiS->OldMode);
	         }
	      } else {
	         xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		 	"Can't identify VESA mode number for mode 0x%x\n", pSiS->OldMode);
	      }
           }

	   if(vesasuccess == FALSE) {

	      int backupscaler = pSiS->SiS_Pr->UsePanelScaler;
	      int backupcenter = pSiS->SiS_Pr->CenterScreen;
	      unsigned long backupspecialtiming = pSiS->SiS_Pr->SiS_CustomT;

 	      if((pSiS->VBFlags & (VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV|VB_302ELV))) {
	        /* !!! REQUIRED for 630+301B-DH, otherwise the text modes
	         *     will not be restored correctly !!!
	         * !!! Do this ONLY for LCD; VGA2 will not be restored
	         *     correctly otherwise.
	         */
	         unsigned char temp;
	         inSISIDXREG(SISCR, 0x30, temp);
	         if(temp & 0x20) {
	            if(pSiS->OldMode == 0x03) {
	      	       pSiS->OldMode = 0x13;
		       changedmode = TRUE;
	            }
	         }
	      }

	      pSiS->SiS_Pr->UseCustomMode = FALSE;
	      pSiS->SiS_Pr->CRT1UsesCustomMode = FALSE;
	      pSiS->SiS_Pr->UsePanelScaler = pSiS->sisfbscalelcd;
	      pSiS->SiS_Pr->CenterScreen = 0;
	      pSiS->SiS_Pr->SiS_CustomT = pSiS->sisfbspecialtiming;
	      SiSSetMode(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, pSiS->OldMode, FALSE);
	      if(changedmode) {
	   	 pSiS->OldMode = 0x03;
		 outSISIDXREG(SISCR,0x34,0x03);
	      }
	      SISSpecialRestore(pScrn);
	      SiS_GetSetModeID(pScrn,pSiS->OldMode);
	      pSiS->SiS_Pr->UsePanelScaler = backupscaler;
	      pSiS->SiS_Pr->CenterScreen = backupcenter;
	      pSiS->SiS_Pr->SiS_CustomT = backupspecialtiming;

	   }

	   /* Restore CRT1 status */
	   if(pSiS->VGAEngine == SIS_315_VGA) {
              outSISIDXREG(SISCR, pSiS->myCR63, pSiS->oldCR63);
           }
           outSISIDXREG(SISSR, 0x1f, pSiS->oldSR1F);

#ifdef SISVRAMQ
	   /* Restore queue mode registers on 315/330 series */
	   /* (This became necessary due to the switch to VRAM queue) */
	   if(pSiS->VGAEngine == SIS_315_VGA) {
	      unsigned char tempCR55=0;
	      if(pSiS->sishw_ext.jChipType <= SIS_330) {
	         inSISIDXREG(SISCR,0x55,tempCR55);
	         andSISIDXREG(SISCR,0x55,0x33);
	      }
	      outSISIDXREG(SISSR,0x26,0x01);
	      MMIO_OUT32(pSiS->IOBase, 0x85c4, 0);
	      outSISIDXREG(SISSR,0x27,sisReg->sisRegs3C4[0x27]);
	      outSISIDXREG(SISSR,0x26,sisReg->sisRegs3C4[0x26]);
	      MMIO_OUT32(pSiS->IOBase, 0x85C0, sisReg->sisMMIO85C0);
	      if(pSiS->sishw_ext.jChipType <= SIS_330) {
	         outSISIDXREG(SISCR,0x55,tempCR55);
	      }
	   }
#endif

        } else {

	   if(pSiS->VBFlags & VB_VIDEOBRIDGE) {
	      /* If a video bridge is present, we need to restore
	       * non-extended (=standard VGA) SR and CR registers
	       * before restoring the extended ones and the bridge
	       * registers itself.
	       */
	      if(!(SiSBridgeIsInSlaveMode(pScrn))) {
                 vgaHWProtect(pScrn, TRUE);
		 
		 if(pSiS->Primary) {
	            vgaHWRestore(pScrn, vgaReg, VGA_SR_MODE);
	         }
              } 
	   }
	   
           (*pSiS->SiSRestore)(pScrn, sisReg);

        }

	if(doitlater) {
            outSISIDXREG(SISCR, 0x17, pSiS->oldCR17);
	}

	if(pSiS->Primary) {
	   if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (SiSBridgeIsInSlaveMode(pScrn))) {
	      /* IMPORTANT: The 30xLV does not handle well being disabled if in
	       * LCDA mode! In LCDA mode, the bridge is NOT in slave mode,
	       * so this is the only safe way: Disable the bridge ONLY if
	       * in Slave Mode, and don't bother if not.
	       */
              SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO+0x30);
	      SiSSetLVDSetc(pSiS->SiS_Pr, &pSiS->sishw_ext, 0);
	      SiS_GetVBType(pSiS->SiS_Pr, &pSiS->sishw_ext);
	      SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext);

	      vgaHWProtect(pScrn, TRUE);

	      /* We now restore ALL to overcome the vga=extended problem */
	      vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);

	      vgaHWProtect(pScrn, FALSE);

	      SiS_EnableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext);
	      andSISIDXREG(SISSR, 0x01, ~0x20);  /* Display on */
	   } else {
	      vgaHWProtect(pScrn, TRUE);

	      /* We now restore ALL to overcome the vga=extended problem */
	      vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);

	      vgaHWProtect(pScrn, FALSE);
	   }
	}

#ifdef TWDEBUG
	{
	  SISRegPtr pReg = &pSiS->ModeReg;
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"REAL REGISTER CONTENTS AFTER RESTORE BY SETMODE:\n");
	  (*pSiS->SiSSave)(pScrn, pReg);
	}
#endif	
	
	sisRestoreExtRegisterLock(pSiS,sisReg->sisRegs3C4[0x05],sisReg->sisRegs3D4[0x80]);
    
    } else {	/* All other chipsets */

        vgaHWProtect(pScrn, TRUE);
	
#ifdef UNLOCK_ALWAYS
        sisSaveUnlockExtRegisterLock(pSiS, NULL,NULL);
#endif

        (*pSiS->SiSRestore)(pScrn, sisReg);

        vgaHWProtect(pScrn, TRUE);
	if(pSiS->Primary) {
           vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);
	}

	/* Restore TV. This is rather complicated, but if we don't do it,
	 * TV output will flicker terribly
	 */
        if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
	   if(sisReg->sis6326tv[0] & 0x04) {
	      unsigned char tmp;
	      int val;

              orSISIDXREG(SISSR, 0x01, 0x20);
              tmp = SiS6326GetTVReg(pScrn,0x00);
              tmp &= ~0x04;
              while(!(inSISREG(SISINPSTAT) & 0x08));  /* Wait while NOT vb */
              SiS6326SetTVReg(pScrn,0x00,tmp);
              for(val=0; val < 2; val++) {
                 while(!(inSISREG(SISINPSTAT) & 0x08));  /* Wait while NOT vb */
                 while(inSISREG(SISINPSTAT) & 0x08);     /* wait while vb     */
              }
              SiS6326SetTVReg(pScrn, 0x00, sisReg->sis6326tv[0]);
              tmp = inSISREG(SISINPSTAT);
              outSISREG(SISAR, 0x20);
              tmp = inSISREG(SISINPSTAT);
              while(inSISREG(SISINPSTAT) & 0x01);
              while(!(inSISREG(SISINPSTAT) & 0x01));
              andSISIDXREG(SISSR, 0x01, ~0x20);
              for(val=0; val < 10; val++) {
                 while(!(inSISREG(SISINPSTAT) & 0x08));  /* Wait while NOT vb */
                 while(inSISREG(SISINPSTAT) & 0x08);     /* wait while vb     */
              }
              andSISIDXREG(SISSR, 0x01, ~0x20);
	   }
        }

        sisRestoreExtRegisterLock(pSiS,sisReg->sisRegs3C4[5],sisReg->sisRegs3D4[0x80]);

        vgaHWProtect(pScrn, FALSE);
    }
}

static void
SISVESARestore(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);

   if(pSiS->UseVESA) {
      SISVESASaveRestore(pScrn, MODE_RESTORE);
#ifdef SISVRAMQ
      /* Restore queue mode registers on 315/330 series */
      /* (This became necessary due to the switch to VRAM queue) */
      if(pSiS->VGAEngine == SIS_315_VGA) {
         SISRegPtr sisReg = &pSiS->SavedReg;
	 unsigned char tempCR55=0;
	 if(pSiS->sishw_ext.jChipType <= SIS_330) {
	    inSISIDXREG(SISCR,0x55,tempCR55);
	    andSISIDXREG(SISCR,0x55,0x33);
	 }
	 outSISIDXREG(SISSR,0x26,0x01);
	 MMIO_OUT32(pSiS->IOBase, 0x85c4, 0);
	 outSISIDXREG(SISSR,0x27,sisReg->sisRegs3C4[0x27]);
	 outSISIDXREG(SISSR,0x26,sisReg->sisRegs3C4[0x26]);
	 MMIO_OUT32(pSiS->IOBase, 0x85C0, sisReg->sisMMIO85C0);
	 if(pSiS->sishw_ext.jChipType <= SIS_330) {
	    outSISIDXREG(SISCR,0x55,tempCR55);
	 }
      }
#endif
   }
}

/* Restore bridge config registers - to be called BEFORE VESARestore */
static void
SISBridgeRestore(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    /* We only restore for master head */
    if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
	SiSRestoreBridge(pScrn, &pSiS->SavedReg);
    }
}

/* Our generic BlockHandler for Xv */
static void
SISBlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
    ScreenPtr pScreen = screenInfo.screens[i];
    ScrnInfoPtr pScrn   = xf86Screens[i];
    SISPtr pSiS = SISPTR(pScrn);

    pScreen->BlockHandler = pSiS->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = SISBlockHandler;

    if(pSiS->VideoTimerCallback) {
       (*pSiS->VideoTimerCallback)(pScrn, currentTime.milliseconds);
    }

    if(pSiS->RenderCallback) {
       (*pSiS->RenderCallback)(pScrn);
    }
}

/* Mandatory
 * This gets called at the start of each server generation
 *
 * We use pScrn and not CurrentLayout here, because the
 * properties we use have not changed (displayWidth,
 * depth, bitsPerPixel)
 */
static Bool
SISScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    SISPtr pSiS;
    int ret;
    VisualPtr visual;
    unsigned long OnScreenSize;
    int height, width, displayWidth;
    unsigned char *FBStart;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif

    pScrn = xf86Screens[pScreen->myNum];

    hwp = VGAHWPTR(pScrn);

    pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead)) {
#endif    
       if(xf86LoadSubModule(pScrn, "vbe")) {
	  xf86LoaderReqSymLists(vbeSymbols, NULL);
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
          pSiS->pVbe = VBEInit(NULL, pSiS->pEnt->index);
#else
          pSiS->pVbe = VBEExtendedInit(NULL, pSiS->pEnt->index,
	                   SET_BIOS_SCRATCH | RESTORE_BIOS_SCRATCH);
#endif
       } else {
          SISErrorLog(pScrn, "Failed to load VBE submodule\n");
       }
#ifdef SISDUALHEAD
    }
#endif

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       pSiSEnt = pSiS->entityPrivate;
       pSiSEnt->refCount++;
    }
#endif

    /* Map the VGA memory and get the VGA IO base */
    if(pSiS->Primary) {
       hwp->MapSize = 0x10000;  /* Standard 64k VGA window */
       if(!vgaHWMapMem(pScrn)) {
          SISErrorLog(pScrn, "Could not map VGA memory window\n");
          return FALSE;
       }
    }
    vgaHWGetIOBase(hwp);
    
    /* Patch the PIOOffset inside vgaHW to use
     * our relocated IO ports.
     */
    VGAHWPTR(pScrn)->PIOOffset = pSiS->IODBase + (pSiS->PciInfo->ioBase[2] & 0xFFFC) - 0x380;

    /* Map the SIS memory and MMIO areas */
    if(!SISMapMem(pScrn)) {
       SISErrorLog(pScrn, "SiSMapMem() failed\n");
       return FALSE;
    }

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Enable TurboQueue so that SISSave() saves it in enabled
     * state. If we don't do this, X will hang after a restart!
     * (Happens for some unknown reason only when using VESA
     * for mode switching; assumingly a BIOS issue.)
     * This is done on 300 and 315 series only.
     */
    if(pSiS->UseVESA) {
#ifdef SISVRAMQ
       if(pSiS->VGAEngine != SIS_315_VGA)
#endif
          SiSEnableTurboQueue(pScrn);

    }

    /* Save the current state */
    SISSave(pScrn);

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {

       if(!pSiS->OldMode) {

          /* Try to find out current (=old) mode number
	   * (Do this only if not sisfb has told us its mode yet)
	   */

	  /* Read 0:449 which the BIOS sets to the current mode number
	   * Unfortunately, this not reliable since the int10 emulation
	   * does not change this. So if we call the VBE later, this
	   * byte won't be touched (which is why we set this manually
	   * then).
	   */
          unsigned char myoldmode = SiS_GetSetModeID(pScrn,0xFF);
	  unsigned char cr30, cr31;

          /* Read CR34 which the BIOS sets to the current mode number for CRT2
	   * This is - of course - not reliable if the machine has no video
	   * bridge...
	   */
          inSISIDXREG(SISCR, 0x34, pSiS->OldMode);
	  inSISIDXREG(SISCR, 0x30, cr30);
	  inSISIDXREG(SISCR, 0x31, cr31);

	  /* What if CR34 is different from the BIOS byte? */
	  if(pSiS->OldMode != myoldmode) {
	     /* If no bridge output is active, trust the BIOS byte */
	     if(!cr31 && !cr30) pSiS->OldMode = myoldmode;
	     /* ..else trust CR34 */
	  }

	  /* Newer 650 BIOSes set CR34 to 0xff if the mode has been
	   * "patched", for instance for 80x50 text mode. (That mode
	   * has no number of its own, it's 0x03 like 80x25). In this
	   * case, we trust the BIOS byte (provided that any of these
	   * two is valid).
	   */
	  if(pSiS->OldMode > 0x7f) {
	     pSiS->OldMode = myoldmode;
	  }
       }
#ifdef SISDUALHEAD
       if(pSiS->DualHeadMode) {
          if(!pSiS->SecondHead) pSiSEnt->OldMode = pSiS->OldMode;
          else                  pSiS->OldMode = pSiSEnt->OldMode;
       }
#endif
    }

    /* Initialise the first mode */
    if(!SISModeInit(pScrn, pScrn->currentMode)) {
       SISErrorLog(pScrn, "SiSModeInit() failed\n");
       return FALSE;
    }

    /* Darken the screen for aesthetic reasons */
    /* Not using Dual Head variant on purpose; we darken
     * the screen for both displays, and un-darken
     * it when the second head is finished
     */
    SISSaveScreen(pScreen, SCREEN_SAVER_ON);

    /* Set the viewport */
    SISAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     * For most PC hardware at depths >= 8, the defaults that cfb uses
     * are not appropriate.  In this driver, we fixup the visuals after.
     */

    /*
     * Reset visual list.
     */
    miClearVisualTypes();

    /* Setup the visuals we support. */

    /*
     * For bpp > 8, the default visuals are not acceptable because we only
     * support TrueColor and not DirectColor.
     */
    if(!miSetVisualTypes(pScrn->depth,
    			 (pScrn->bitsPerPixel > 8) ?
			 	TrueColorMask : miGetDefaultVisualMask(pScrn->depth),
			 pScrn->rgbBits, pScrn->defaultVisual)) {
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       SISErrorLog(pScrn, "miSetVisualTypes() failed (bpp %d)\n",
	  		pScrn->bitsPerPixel);
       return FALSE;
    }

    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;

    if(pSiS->Rotate) {
       height = pScrn->virtualX;
       width = pScrn->virtualY;
    }

    if(pSiS->ShadowFB) {
       pSiS->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
       pSiS->ShadowPtr = xalloc(pSiS->ShadowPitch * height);
       displayWidth = pSiS->ShadowPitch / (pScrn->bitsPerPixel >> 3);
       FBStart = pSiS->ShadowPtr;
    } else {
       pSiS->ShadowPtr = NULL;
       FBStart = pSiS->FbBase;
    }

    if(!miSetPixmapDepths()) {
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       SISErrorLog(pScrn, "miSetPixmapDepths() failed\n");
       return FALSE;
    }

    /* Point cmdQueuePtr to pSiSEnt for shared usage
     * (same technique is then eventually used in DRIScreeninit)
     * For 315/330 series, this is done in EnableTurboQueue
     * which has already been called during ModeInit().
     */
#ifdef SISDUALHEAD
    if(pSiS->SecondHead)
       pSiS->cmdQueueLenPtr = &(SISPTR(pSiSEnt->pScrn_1)->cmdQueueLen);
    else
#endif
       pSiS->cmdQueueLenPtr = &(pSiS->cmdQueueLen);

    pSiS->cmdQueueLen = 0; /* Force an EngineIdle() at start */

#ifdef XF86DRI
    if(pSiS->loadDRI) {
#ifdef SISDUALHEAD
       /* No DRI in dual head mode */
       if(pSiS->DualHeadMode) {
          pSiS->directRenderingEnabled = FALSE;
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"DRI not supported in Dual Head mode\n");
       } else
#endif
          /* Force the initialization of the context */
              if(pSiS->VGAEngine != SIS_315_VGA) {
          pSiS->directRenderingEnabled = SISDRIScreenInit(pScreen);
       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_NOT_IMPLEMENTED,
	        "DRI not supported on this chipset\n");
          pSiS->directRenderingEnabled = FALSE;
       }
    }
#endif

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */
    switch(pScrn->bitsPerPixel) {
      case 24:
        if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	   ret = FALSE;
	   break;
        }
      case 8:
      case 16:
      case 32:
        ret = fbScreenInit(pScreen, FBStart, width,
                        height, pScrn->xDpi, pScrn->yDpi,
                        displayWidth, pScrn->bitsPerPixel);
        break;
      default:
        ret = FALSE;
        break;
    }
    if(!ret) {
       SISErrorLog(pScrn, "Unsupported bpp (%d) or fbScreenInit() failed\n",
               pScrn->bitsPerPixel);
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       return FALSE;
    }

    if(pScrn->bitsPerPixel > 8) {
       /* Fixup RGB ordering */
       visual = pScreen->visuals + pScreen->numVisuals;
       while (--visual >= pScreen->visuals) {
          if((visual->class | DynamicClass) == DirectColor) {
             visual->offsetRed = pScrn->offset.red;
             visual->offsetGreen = pScrn->offset.green;
             visual->offsetBlue = pScrn->offset.blue;
             visual->redMask = pScrn->mask.red;
             visual->greenMask = pScrn->mask.green;
             visual->blueMask = pScrn->mask.blue;
          }
       }
    }

    /* Initialize RENDER ext; must be after RGB ordering fixed */
    fbPictureInit(pScreen, 0, 0);

    /* hardware cursor needs to wrap this layer    <-- TW: what does that mean? */
    if(!pSiS->ShadowFB) SISDGAInit(pScreen);

    xf86SetBlackWhitePixels(pScreen);

    if(!pSiS->NoAccel) {
       switch(pSiS->VGAEngine) {
	  case SIS_530_VGA:
	  case SIS_300_VGA:
            SiS300AccelInit(pScreen);
	    break;
	  case SIS_315_VGA:
	    SiS315AccelInit(pScreen);
	    break;
          default:
            SiSAccelInit(pScreen);
       }
    }
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Initialise cursor functions */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    if(pSiS->HWCursor) {
       SiSHWCursorInit(pScreen);
    }

    /* Initialise default colourmap */
    if(!miCreateDefColormap(pScreen)) {
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       SISErrorLog(pScrn, "miCreateDefColormap() failed\n");
       return FALSE;
    }

    if(!xf86HandleColormaps(pScreen, 256, (pScrn->depth == 8) ? 8 : pScrn->rgbBits,
                    SISLoadPalette, NULL,
                    CMAP_PALETTED_TRUECOLOR | CMAP_RELOAD_ON_MODE_SWITCH)) {
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       SISErrorLog(pScrn, "xf86HandleColormaps() failed\n");
       return FALSE;
    }

#if 0
    if((pSiS->GammaBriR != 1000) || (pSiS->GammaBriG != 1000) ||
       (pSiS->GammaBriB != 1000) || (pSiS->GammaPBriR != 1000) ||
       (pSiS->GammaPBriG != 1000) || (pSiS->GammaPBriB != 1000)) {
       SISCalculateGammaRamp(pScrn);
    }
#endif

    if(pSiS->ShadowFB) {
       RefreshAreaFuncPtr refreshArea = SISRefreshArea;

       if(pSiS->Rotate) {
          if(!pSiS->PointerMoved) {
             pSiS->PointerMoved = pScrn->PointerMoved;
             pScrn->PointerMoved = SISPointerMoved;
          }

          switch(pScrn->bitsPerPixel) {
             case 8:  refreshArea = SISRefreshArea8;  break;
             case 16: refreshArea = SISRefreshArea16; break;
             case 24: refreshArea = SISRefreshArea24; break;
             case 32: refreshArea = SISRefreshArea32; break;
          }
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,0,0,0)
	  xf86DisableRandR();
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	  	"Driver rotation enabled, RandR disabled\n");
#endif
       }

       ShadowFBInit(pScreen, refreshArea);
    }

    xf86DPMSInit(pScreen, (DPMSSetProcPtr)SISDisplayPowerManagementSet, 0);

    /* Init memPhysBase and fbOffset in pScrn */
    pScrn->memPhysBase = pSiS->FbAddress;
    pScrn->fbOffset = 0;
    
    pSiS->ResetXv = pSiS->ResetXvGamma = NULL;

#if (XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,99,0,0)) || (defined(XvExtension))
    if(!pSiS->NoXvideo) {
        if( (pSiS->VGAEngine == SIS_300_VGA) ||
	    (pSiS->VGAEngine == SIS_315_VGA) ) {
#ifdef SISDUALHEAD
              if(pSiS->DualHeadMode) {
		 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		     "Using SiS300/315 series HW Xv on CRT%d\n",
		     (pSiS->SecondHead ? 1 : 2));
		 if(!pSiS->hasTwoOverlays) {
		    if( (pSiS->XvOnCRT2 && pSiS->SecondHead) ||
		        (!pSiS->XvOnCRT2 && !pSiS->SecondHead) ) {
		       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		           "However, overlay will by default only be visible on CRT%d\n",
		           pSiS->XvOnCRT2 ? 2 : 1);
		    }
		 }
                 SISInitVideo(pScreen);
 	      } else {
#endif
	        if(pSiS->hasTwoOverlays)
                   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Using SiS300/315/330 series HW Xv\n" );
                else
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Using SiS300/315/330 series HW Xv by default on CRT%d\n",
		       (pSiS->XvOnCRT2 ? 2 : 1));
	        SISInitVideo(pScreen);
#ifdef SISDUALHEAD
              }
#endif
        } else if( pSiS->Chipset == PCI_CHIP_SIS6326 ||
	           pSiS->Chipset == PCI_CHIP_SIS530  ||
		   pSiS->Chipset == PCI_CHIP_SIS5597 ) {

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Using SiS5597/5598/6326/530/620 HW Xv\n" );
		SIS6326InitVideo(pScreen);

	} else { /* generic Xv */

            XF86VideoAdaptorPtr *ptr;
            int n;

            n = xf86XVListGenericAdaptors(pScrn, &ptr);
            if(n) {
                xf86XVScreenInit(pScreen, ptr, n);
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using generic Xv\n" );
            }

        }
    }
#endif

#ifdef XF86DRI
    if(pSiS->loadDRI) {
       if(pSiS->directRenderingEnabled) {
          /* Now that mi, drm and others have done their thing,
           * complete the DRI setup.
           */
          pSiS->directRenderingEnabled = SISDRIFinishScreenInit(pScreen);
       }
       if(pSiS->directRenderingEnabled) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering enabled\n");
          /* TODO */
          /* SISSetLFBConfig(pSiS); */
       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering disabled\n");
       }
    }
#endif

    /* Wrap some funcs and setup remaining SD flags */

    pSiS->SiS_SD_Flags &= ~(SiS_SD_PSEUDOXINERAMA);
#ifdef SISMERGED
    if(pSiS->MergedFB) {
       pSiS->PointerMoved = pScrn->PointerMoved;
       pScrn->PointerMoved = SISMergePointerMoved;
       pSiS->Rotate = FALSE;
       pSiS->ShadowFB = FALSE;
#ifdef SISXINERAMA
       if(pSiS->UseSiSXinerama) {
          SiSnoPanoramiXExtension = FALSE;
          SiSXineramaExtensionInit(pScrn);
	  if(!SiSnoPanoramiXExtension) {
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,0,0,0)
             xf86DisableRandR();
             xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	  	 "SiS Pseudo-Xinerama enabled, RandR disabled\n");
#endif
	     pSiS->SiS_SD_Flags |= SiS_SD_PSEUDOXINERAMA;
	  }
       }
#endif
    }
#endif

    pSiS->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = SISCloseScreen;
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode)
       pScreen->SaveScreen = SISSaveScreenDH;
    else
#endif
       pScreen->SaveScreen = SISSaveScreen;

    /* Install BlockHandler */
    pSiS->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = SISBlockHandler;

    /* Report any unused options (only for the first generation) */
    if(serverGeneration == 1) {
       xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* Clear frame buffer */
    /* For CRT2, we don't do that at this point in dual head
     * mode since the mode isn't switched at this time (it will
     * be reset when setting the CRT1 mode). Hence, we just
     * save the necessary data and clear the screen when
     * going through this for CRT1.
     */
     
    OnScreenSize = pScrn->displayWidth * pScrn->currentMode->VDisplay
                               * (pScrn->bitsPerPixel >> 3);

    /* Turn on the screen now */
    /* We do this in dual head mode after second head is finished */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(pSiS->SecondHead) {
          bzero(pSiS->FbBase, OnScreenSize);
	  bzero(pSiSEnt->FbBase1, pSiSEnt->OnScreenSize1);
    	  SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       } else {
          pSiSEnt->FbBase1 = pSiS->FbBase;
	  pSiSEnt->OnScreenSize1 = OnScreenSize;
       }
    } else {
#endif
       SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
       bzero(pSiS->FbBase, OnScreenSize);
#ifdef SISDUALHEAD
    }
#endif

    pSiS->SiS_SD_Flags &= ~SiS_SD_ISDEPTH8;
    if(pSiS->CurrentLayout.bitsPerPixel == 8) {
    	pSiS->SiS_SD_Flags |= SiS_SD_ISDEPTH8;
	pSiS->SiS_SD_Flags &= ~SiS_SD_SUPPORTXVGAMMA1;
    }

    return TRUE;
}

/* Usually mandatory */
Bool
SISSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SISPtr pSiS = SISPTR(pScrn);

    if(!pSiS->NoAccel) {
       if(pSiS->AccelInfoPtr) {
          (*pSiS->AccelInfoPtr->Sync)(pScrn);
       }
    }

    if(!(SISModeInit(xf86Screens[scrnIndex], mode))) return FALSE;

    /* Since RandR (indirectly) uses SwitchMode(), we need to
     * update our Xinerama info here, too, in case of resizing
     */
#ifdef SISMERGED
#ifdef SISXINERAMA
    if(pSiS->MergedFB) {
       SiSUpdateXineramaScreenInfo(pScrn);
    }
#endif
#endif
    return TRUE;
}

Bool
SISSwitchCRT2Type(ScrnInfoPtr pScrn, unsigned long newvbflags)
{
    SISPtr pSiS = SISPTR(pScrn);
    BOOLEAN hcm;
    DisplayModePtr mode = pScrn->currentMode;

    /* Do NOT use this to switch from CRT2_LCD to CRT1_LCDA */

    /* Only on 300 and 315/330 series */
    if(pSiS->VGAEngine != SIS_300_VGA &&
       pSiS->VGAEngine != SIS_315_VGA) return FALSE;

    /* Only if there is a video bridge */
    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) return FALSE;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) return FALSE;
#endif

#define SiS_NewVBMask (CRT2_ENABLE|CRT1_LCDA|TV_PAL|TV_NTSC|TV_PALM|TV_PALN|TV_NTSCJ| \
		       TV_AVIDEO|TV_SVIDEO|TV_SCART|TV_HIVISION|TV_YPBPR|TV_YPBPRALL|\
		       TV_YPBPRAR)

    newvbflags &= SiS_NewVBMask;
    newvbflags |= pSiS->VBFlags & ~SiS_NewVBMask;

    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTLCDA)) {
       newvbflags &= ~CRT1_LCDA;
    }
    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTHIVISION)) {
       newvbflags &= ~TV_HIVISION;
    }
    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR)) {
       newvbflags &= ~TV_YPBPR;
    }

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if(!(newvbflags & CRT2_ENABLE)) {
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"CRT2 can't be switched off in MergedFB mode\n");
	  return FALSE;
       }
       hcm = pSiS->HaveCustomModes2;
       if(mode->Private) {
	  mode = ((SiSMergedDisplayModePtr)mode->Private)->CRT2;
       }
    } else
#endif
       hcm = pSiS->HaveCustomModes;

    if((!(newvbflags & CRT2_ENABLE)) && (!newvbflags & DISPTYPE_CRT1)) {
       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
          "CRT2 can't be switched off while CRT1 is off\n");
       return FALSE;
    }

    /* CRT2_LCD overrules LCDA */
    if(newvbflags & CRT2_LCD) {
       newvbflags &= ~CRT1_LCDA;
    }

    /* Check if the current mode is suitable for desired output device (if any) */
    if(newvbflags & CRT2_ENABLE) {
       if(!SiS_CheckCalcModeIndex(pScrn, mode, newvbflags, hcm)) {
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	     "Current mode not suitable for desired CRT2 output device\n");
          return FALSE;
       }
    }

    /* Remember: Dualhead not supported */
    newvbflags &= ~(SINGLE_MODE | MIRROR_MODE);
    if((newvbflags & DISPTYPE_CRT1) && (newvbflags & CRT2_ENABLE)) {
       newvbflags |= MIRROR_MODE;
    } else {
       newvbflags |= SINGLE_MODE;
    }

    /* Sync the accelerators */
    if(!pSiS->NoAccel) {
       if(pSiS->AccelInfoPtr) {
          (*pSiS->AccelInfoPtr->Sync)(pScrn);
       }
    }

    pSiS->VBFlags = pSiS->VBFlags_backup = newvbflags;

    if(!(pScrn->SwitchMode(pScrn->scrnIndex, pScrn->currentMode, 0))) return FALSE;
    SISAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}

int
SISCheckModeIndexForCRT2Type(ScrnInfoPtr pScrn, unsigned short cond, unsigned short index, Bool quiet)
{
    SISPtr pSiS = SISPTR(pScrn);
    BOOLEAN hcm = pSiS->HaveCustomModes;
    DisplayModePtr mode = pScrn->modes, mastermode;
    int i, result = 0;
    unsigned long vbflags = pSiS->VBFlags;

    /* Not only CRT2, but also LCDA */

    /* returns 0 if mode ok,
     *         0x01 if mode not ok for CRT2 device,
     *         0x02 if mode too large for current root window
     *         or combinations thereof
     */

    /* No special treatment for NTSC-J here; conditions equal NTSC */
    if(cond) {
       vbflags &= ~(CRT2_ENABLE | CRT1_LCDA | TV_STANDARD | TV_INTERFACE);
       if((cond & SiS_CF2_TYPEMASK) == SiS_CF2_LCD) {
       	  vbflags |= CRT2_LCD;
       } else if((cond & SiS_CF2_TYPEMASK) == SiS_CF2_TV) {
          vbflags |= (CRT2_TV | TV_SVIDEO);
	  if(cond & SiS_CF2_TVPAL)  	  vbflags |= TV_PAL;
	  else if(cond & SiS_CF2_TVPALM)  vbflags |= (TV_PAL | TV_PALM);
	  else if(cond & SiS_CF2_TVPALN)  vbflags |= (TV_PAL | TV_PALN);
	  else if(cond & SiS_CF2_TVNTSC)  vbflags |= TV_NTSC;
       } else if((cond & SiS_CF2_TYPEMASK) == SiS_CF2_TVSPECIAL) {
          vbflags |= CRT2_TV;
	  if((cond & SiS_CF2_TVSPECMASK) == SiS_CF2_TVHIVISION)
	  	vbflags |= TV_HIVISION;
	  else if((cond & SiS_CF2_TVSPECMASK) == SiS_CF2_TVYPBPR525I)
	  	vbflags |= (TV_YPBPR | TV_YPBPR525I);
	  else if((cond & SiS_CF2_TVSPECMASK) == SiS_CF2_TVYPBPR525P)
	  	vbflags |= (TV_YPBPR | TV_YPBPR525P);
	  else if((cond & SiS_CF2_TVSPECMASK) == SiS_CF2_TVYPBPR750P)
	  	vbflags |= (TV_YPBPR | TV_YPBPR750P);
	  else if((cond & SiS_CF2_TVSPECMASK) == SiS_CF2_TVYPBPR1080I)
	  	vbflags |= (TV_YPBPR | TV_YPBPR1080I);
       } else if((cond & SiS_CF2_TYPEMASK) == SiS_CF2_VGA2) {
          vbflags |= CRT2_VGA;
       } else if((cond & SiS_CF2_TYPEMASK) == SiS_CF2_CRT1LCDA) {
          vbflags |= CRT1_LCDA;
       }
    }

    /* Find mode of given index */
    if(index) {
       for(i = 0; i < index; i++) {
          if(!mode) return 0x03;
          mode = mode->next;
       }
    }

    mastermode = mode;

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead)) {
#endif

       if(vbflags & CRT2_ENABLE) {

#ifdef SISMERGED
          if(pSiS->MergedFB) {
             hcm = pSiS->HaveCustomModes2;
             if(mode->Private) {
	        mode = ((SiSMergedDisplayModePtr)mode->Private)->CRT2;
             }
          }
#endif

          /* For RandR */
          if((mode->HDisplay > pScrn->virtualX) || (mode->VDisplay > pScrn->virtualY)) {
             if(!quiet) {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Desired mode too large for current screen size\n");
             }
             result |= 0x02;
          }

          /* Check if the desired mode is suitable for current CRT2 output device */
          if(!SiS_CheckCalcModeIndex(pScrn, mode, vbflags, hcm)) {
             if((!cond) && (!quiet)) {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Desired mode not suitable for current CRT2 output device\n");
             }
             result |= 0x01;
          }

       }

#ifdef SISDUALHEAD
    }
#endif

    mode = mastermode;

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (pSiS->SecondHead)) {
#endif

       if(vbflags & CRT1_LCDA) {

#ifdef SISMERGED
          if(pSiS->MergedFB) {
             hcm = pSiS->HaveCustomModes;
             if(mode->Private) {
	        mode = ((SiSMergedDisplayModePtr)mode->Private)->CRT1;
	     }
          }
#endif

 	  /* For RandR */
          if((mode->HDisplay > pScrn->virtualX) || (mode->VDisplay > pScrn->virtualY)) {
             if(!quiet) {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Desired mode too large for current screen size\n");
             }
             result |= 0x02;
          }

          /* Check if the desired mode is suitable for current CRT1 output device */
          if(!SiS_CalcModeIndex(pScrn, mode, vbflags, hcm)) {
             if((!cond) && (!quiet)) {
                 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	  	      "Desired mode not suitable for current CRT1 output device\n");
             }
             result |= 0x01;
          }

       }

#ifdef SISDUALHEAD
    }
#endif

    return result;
}

Bool
SISSwitchCRT1Status(ScrnInfoPtr pScrn, int onoff)
{
    SISPtr pSiS = SISPTR(pScrn);
    DisplayModePtr mode = pScrn->currentMode;
    unsigned long vbflags = pSiS->VBFlags;
    int crt1off;

    /* onoff: 0=OFF, 1=ON(VGA), 2=ON(LCDA) */
    /* Switching to LCDA will disable CRT2 if previously LCD */

    /* Do NOT use this to switch from CRT1_LCDA to CRT2_LCD */

    /* Only on 300 and 315/330 series */
    if(pSiS->VGAEngine != SIS_300_VGA &&
       pSiS->VGAEngine != SIS_315_VGA) return FALSE;

    /* Off only if at least one CRT2 device is active */
    if((!onoff) && (!(vbflags & CRT2_ENABLE))) return FALSE;

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) return FALSE;
#endif

    /* Can't switch to LCDA of not supported (duh!) */
    if(!(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTLCDA)) {
       if(onoff == 2) {
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"LCD-via-CRT1 not supported on this hardware\n");
          return FALSE;
       }
    }

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       if(!onoff) {
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"CRT1 can't be switched off in MergedFB mode\n");
          return FALSE;
       } else if(onoff == 2) {
          if(vbflags & CRT2_LCD) {
	     xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"CRT2 type can't be LCD while CRT1 is LCD-via-CRT1\n");
             return FALSE;
	  }
       }
       if(mode->Private) {
	  mode = ((SiSMergedDisplayModePtr)mode->Private)->CRT1;
       }
    }
#endif

    vbflags &= ~(DISPTYPE_CRT1 | SINGLE_MODE | MIRROR_MODE | CRT1_LCDA);
    crt1off = 1;
    if(onoff > 0) {
       vbflags |= DISPTYPE_CRT1;
       crt1off = 0;
       if(onoff == 2) {
       	  vbflags |= CRT1_LCDA;
	  vbflags &= ~CRT2_LCD;
       }
       /* Remember: Dualhead not supported */
       if(vbflags & CRT2_ENABLE) vbflags |= MIRROR_MODE;
       else vbflags |= SINGLE_MODE;
    } else {
       vbflags |= SINGLE_MODE;
    }

    if(vbflags & CRT1_LCDA) {
       if(!SiS_CalcModeIndex(pScrn, mode, vbflags, pSiS->HaveCustomModes)) {
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Current mode not suitable for LCD-via-CRT1\n");
          return FALSE;
       }
    }

    pSiS->CRT1off = crt1off;
    pSiS->VBFlags = pSiS->VBFlags_backup = vbflags;

    /* Sync the accelerators */
    if(!pSiS->NoAccel) {
       if(pSiS->AccelInfoPtr) {
          (*pSiS->AccelInfoPtr->Sync)(pScrn);
       }
    }

    if(!(pScrn->SwitchMode(pScrn->scrnIndex, pScrn->currentMode, 0))) return FALSE;
    SISAdjustFrame(pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    return TRUE;
}

static void
SISSetStartAddressCRT1(SISPtr pSiS, unsigned long base)
{
    unsigned char cr11backup;

    inSISIDXREG(SISCR,  0x11, cr11backup);  /* Unlock CRTC registers */
    andSISIDXREG(SISCR, 0x11, 0x7F);
    outSISIDXREG(SISCR, 0x0D, base & 0xFF);
    outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
    outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
    if(pSiS->VGAEngine == SIS_315_VGA) {
       setSISIDXREG(SISSR, 0x37, 0xFE, (base >> 24) & 0x01);
    }
    /* Eventually lock CRTC registers */
    setSISIDXREG(SISCR, 0x11, 0x7F,(cr11backup & 0x80));
}

static void
SISSetStartAddressCRT2(SISPtr pSiS, unsigned long base)
{
    SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext);
    outSISIDXREG(SISPART1, 0x06, GETVAR8(base));
    outSISIDXREG(SISPART1, 0x05, GETBITS(base, 15:8));
    outSISIDXREG(SISPART1, 0x04, GETBITS(base, 23:16));
    if(pSiS->VGAEngine == SIS_315_VGA) {
       setSISIDXREG(SISPART1, 0x02, 0x7F, ((base >> 24) & 0x01) << 7);
    }
    SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext);
}

#ifdef SISMERGED
static Bool
InRegion(int x, int y, region r)
{
    return (r.x0 <= x) && (x <= r.x1) && (r.y0 <= y) && (y <= r.y1);
}

static void
SISAdjustFrameHW_CRT1(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned long base;

    base = y * pSiS->CurrentLayout.displayWidth + x;
    switch(pSiS->CurrentLayout.bitsPerPixel) {
       case 16:  base >>= 1; 	break;
       case 32:  		break;
       default:  base >>= 2;
    }
    SISSetStartAddressCRT1(pSiS, base);
}

static void
SISAdjustFrameHW_CRT2(ScrnInfoPtr pScrn, int x, int y)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned long base;

    base = y * pSiS->CurrentLayout.displayWidth + x;
    switch(pSiS->CurrentLayout.bitsPerPixel) {
       case 16:  base >>= 1; 	break;
       case 32:  		break;
       default:  base >>= 2;
    }
    SISSetStartAddressCRT2(pSiS, base);
}

static void
SISMergePointerMoved(int scrnIndex, int x, int y)
{
  ScrnInfoPtr   pScrn1 = xf86Screens[scrnIndex];
  SISPtr        pSiS = SISPTR(pScrn1);
  ScrnInfoPtr   pScrn2 = pSiS->CRT2pScrn;
  region 	out, in1, in2, f2, f1;
  int 		deltax, deltay;

  f1.x0 = pSiS->CRT1frameX0;
  f1.x1 = pSiS->CRT1frameX1;
  f1.y0 = pSiS->CRT1frameY0;
  f1.y1 = pSiS->CRT1frameY1;
  f2.x0 = pScrn2->frameX0;
  f2.x1 = pScrn2->frameX1;
  f2.y0 = pScrn2->frameY0;
  f2.y1 = pScrn2->frameY1;

  /* Define the outer region. Crossing this causes all frames to move */
  out.x0 = pScrn1->frameX0;
  out.x1 = pScrn1->frameX1;
  out.y0 = pScrn1->frameY0;
  out.y1 = pScrn1->frameY1;

  /*
   * Define the inner sliding window. Being outsize both frames but
   * inside the outer clipping window will slide corresponding frame
   */
  in1 = out;
  in2 = out;
  switch(((SiSMergedDisplayModePtr)pSiS->CurrentLayout.mode->Private)->CRT2Position) {
     case sisLeftOf:
        in1.x0 = f1.x0;
        in2.x1 = f2.x1;
        break;
     case sisRightOf:
        in1.x1 = f1.x1;
        in2.x0 = f2.x0;
        break;
     case sisBelow:
        in1.y1 = f1.y1;
        in2.y0 = f2.y0;
        break;
     case sisAbove:
        in1.y0 = f1.y0;
        in2.y1 = f2.y1;
        break;
     case sisClone:
        break;
  }

  deltay = 0;
  deltax = 0;

  if(InRegion(x, y, out)) {	/* inside outer region */

     if(InRegion(x, y, in1) && !InRegion(x, y, f1)) {
        REBOUND(f1.x0, f1.x1, x);
        REBOUND(f1.y0, f1.y1, y);
        deltax = 1;
     }
     if(InRegion(x, y, in2) && !InRegion(x, y, f2)) {
        REBOUND(f2.x0, f2.x1, x);
        REBOUND(f2.y0, f2.y1, y);
        deltax = 1;
     }

  } else {			/* outside outer region */

     if(out.x0 > x) {
        deltax = x - out.x0;
     }
     if(out.x1 < x) {
        deltax = x - out.x1;
     }
     if(deltax) {
        pScrn1->frameX0 += deltax;
        pScrn1->frameX1 += deltax;
	f1.x0 += deltax;
        f1.x1 += deltax;
        f2.x0 += deltax;
        f2.x1 += deltax;
     }

     if(out.y0 > y) {
        deltay = y - out.y0;
     }
     if(out.y1 < y) {
        deltay = y - out.y1;
     }
     if(deltay) {
        pScrn1->frameY0 += deltay;
        pScrn1->frameY1 += deltay;
	f1.y0 += deltay;
        f1.y1 += deltay;
        f2.y0 += deltay;
        f2.y1 += deltay;
     }

     switch(((SiSMergedDisplayModePtr)pSiS->CurrentLayout.mode->Private)->CRT2Position) {
        case sisLeftOf:
	   if(x >= f1.x0) { REBOUND(f1.y0, f1.y1, y); }
	   if(x <= f2.x1) { REBOUND(f2.y0, f2.y1, y); }
           break;
        case sisRightOf:
	   if(x <= f1.x1) { REBOUND(f1.y0, f1.y1, y); }
	   if(x >= f2.x0) { REBOUND(f2.y0, f2.y1, y); }
           break;
        case sisBelow:
	   if(y <= f1.y1) { REBOUND(f1.x0, f1.x1, x); }
	   if(y >= f2.y0) { REBOUND(f2.x0, f2.x1, x); }
           break;
        case sisAbove:
	   if(y >= f1.y0) { REBOUND(f1.x0, f1.x1, x); }
	   if(y <= f2.y1) { REBOUND(f2.x0, f2.x1, x); }
           break;
        case sisClone:
           break;
     }

  }

  if(deltax || deltay) {
     pSiS->CRT1frameX0 = f1.x0;
     pSiS->CRT1frameY0 = f1.y0;
     pScrn2->frameX0 = f2.x0;
     pScrn2->frameY0 = f2.y0;

     pSiS->CRT1frameX1 = pSiS->CRT1frameX0 + CDMPTR->CRT1->HDisplay - 1;
     pSiS->CRT1frameY1 = pSiS->CRT1frameY0 + CDMPTR->CRT1->VDisplay - 1;
     pScrn2->frameX1   = pScrn2->frameX0   + CDMPTR->CRT2->HDisplay - 1;
     pScrn2->frameY1   = pScrn2->frameY0   + CDMPTR->CRT2->VDisplay - 1;
     pScrn1->frameX1   = pScrn1->frameX0   + pSiS->CurrentLayout.mode->HDisplay  - 1;
     pScrn1->frameY1   = pScrn1->frameY0   + pSiS->CurrentLayout.mode->VDisplay  - 1;

     SISAdjustFrameHW_CRT1(pScrn1, pSiS->CRT1frameX0, pSiS->CRT1frameY0);
     SISAdjustFrameHW_CRT2(pScrn1, pScrn2->frameX0, pScrn2->frameY0);
  }
}

static void
SISAdjustFrameMerged(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn1 = xf86Screens[scrnIndex];
    SISPtr pSiS = SISPTR(pScrn1);
    ScrnInfoPtr pScrn2 = pSiS->CRT2pScrn;
    int VTotal = pSiS->CurrentLayout.mode->VDisplay;
    int HTotal = pSiS->CurrentLayout.mode->HDisplay;
    int VMax = VTotal;
    int HMax = HTotal;

    BOUND(x, 0, pScrn1->virtualX - HTotal);
    BOUND(y, 0, pScrn1->virtualY - VTotal);

    switch(SDMPTR(pScrn1)->CRT2Position) {
        case sisLeftOf:
            pScrn2->frameX0 = x;
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            pSiS->CRT1frameX0 = x + CDMPTR->CRT2->HDisplay;
            BOUND(pSiS->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            break;
        case sisRightOf:
            pSiS->CRT1frameX0 = x;
            BOUND(pSiS->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            pScrn2->frameX0 = x + CDMPTR->CRT1->HDisplay;
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            break;
        case sisAbove:
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            pScrn2->frameY0 = y;
            BOUND(pSiS->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            pSiS->CRT1frameY0 = y + CDMPTR->CRT2->VDisplay;
            break;
        case sisBelow:
            BOUND(pSiS->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            pSiS->CRT1frameY0 = y;
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            pScrn2->frameY0 = y + CDMPTR->CRT1->VDisplay;
            break;
        case sisClone:
            BOUND(pSiS->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            BOUND(pSiS->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            break;
    }

    BOUND(pSiS->CRT1frameX0, 0, pScrn1->virtualX - CDMPTR->CRT1->HDisplay);
    BOUND(pSiS->CRT1frameY0, 0, pScrn1->virtualY - CDMPTR->CRT1->VDisplay);
    BOUND(pScrn2->frameX0,   0, pScrn1->virtualX - CDMPTR->CRT2->HDisplay);
    BOUND(pScrn2->frameY0,   0, pScrn1->virtualY - CDMPTR->CRT2->VDisplay);
    
    pScrn1->frameX0 = x;
    pScrn1->frameY0 = y;

    pSiS->CRT1frameX1 = pSiS->CRT1frameX0 + CDMPTR->CRT1->HDisplay - 1;
    pSiS->CRT1frameY1 = pSiS->CRT1frameY0 + CDMPTR->CRT1->VDisplay - 1;
    pScrn2->frameX1   = pScrn2->frameX0   + CDMPTR->CRT2->HDisplay - 1;
    pScrn2->frameY1   = pScrn2->frameY0   + CDMPTR->CRT2->VDisplay - 1;
    pScrn1->frameX1   = pScrn1->frameX0   + pSiS->CurrentLayout.mode->HDisplay  - 1;
    pScrn1->frameY1   = pScrn1->frameY0   + pSiS->CurrentLayout.mode->VDisplay  - 1;

    SISAdjustFrameHW_CRT1(pScrn1, pSiS->CRT1frameX0, pSiS->CRT1frameY0);
    SISAdjustFrameHW_CRT2(pScrn1, pScrn2->frameX0, pScrn2->frameY0);
}
#endif

/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 *
 * Usually mandatory
 */
void
SISAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr   pScrn = xf86Screens[scrnIndex];
    SISPtr        pSiS = SISPTR(pScrn);
    unsigned long base;
    unsigned char temp, cr11backup;

#ifdef SISMERGED
    if(pSiS->MergedFB) {
    	SISAdjustFrameMerged(scrnIndex, x, y, flags);
	return;
    }
#endif

    if(pSiS->UseVESA) {
	VBESetDisplayStart(pSiS->pVbe, x, y, TRUE);
	return;
    }

    if(pScrn->bitsPerPixel < 8) {
       base = (y * pSiS->CurrentLayout.displayWidth + x + 3) >> 3;
    } else {
       base  = y * pSiS->CurrentLayout.displayWidth + x;

       /* calculate base bpp dep. */
       switch(pSiS->CurrentLayout.bitsPerPixel) {
          case 16:
     	     base >>= 1;
             break;
          case 24:
             base = ((base * 3)) >> 2;
             base -= base % 6;
             break;
          case 32:
             break;
          default:      /* 8bpp */
             base >>= 2;
             break;
       }
    }

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(!pSiS->SecondHead) {
	  /* Head 1 (master) is always CRT2 */
          SISSetStartAddressCRT2(pSiS, base);
       } else {
          /* TW: Head 2 (slave) is always CRT1 */
	  base += (pSiS->dhmOffset/4);
	  SISSetStartAddressCRT1(pSiS, base);
       }
    } else {
#endif
       switch(pSiS->VGAEngine) {
          case SIS_300_VGA:
	  case SIS_315_VGA:
	     SISSetStartAddressCRT1(pSiS, base);
             if(pSiS->VBFlags & CRT2_ENABLE) {
		SISSetStartAddressCRT2(pSiS, base);
	     }
             break;
          default:
	     /* Unlock CRTC registers */
             inSISIDXREG(SISCR,  0x11, cr11backup);
             andSISIDXREG(SISCR, 0x11, 0x7F);
	     outSISIDXREG(SISCR, 0x0D, base & 0xFF);
	     outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
	     inSISIDXREG(SISSR,  0x27, temp);
	     temp &= 0xF0;
	     temp |= (base & 0x0F0000) >> 16;
	     outSISIDXREG(SISSR, 0x27, temp);
	     /* Eventually lock CRTC registers */
	     setSISIDXREG(SISCR, 0x11, 0x7F, (cr11backup & 0x80));
       }
#ifdef SISDUALHEAD
    }
#endif

}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 * Mandatory!
 */
static Bool
SISEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SISPtr pSiS = SISPTR(pScrn);

    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       outSISIDXREG(SISCR,0x32,pSiS->myCR32);
       outSISIDXREG(SISCR,0x36,pSiS->myCR36);
       outSISIDXREG(SISCR,0x37,pSiS->myCR37);
    }

    if(!SISModeInit(pScrn, pScrn->currentMode)) {
       SISErrorLog(pScrn, "SiSEnterVT: SISModeInit() failed\n");
       return FALSE;
    }

    SISAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

#ifdef XF86DRI
    if(pSiS->directRenderingEnabled) {
       DRIUnlock(screenInfo.screens[scrnIndex]);
    }
#endif

#ifdef SISDUALHEAD
    if((!pSiS->DualHeadMode) || (!pSiS->SecondHead))
#endif
       if(pSiS->ResetXv) {
          (pSiS->ResetXv)(pScrn);
       }

    return TRUE;
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 * Mandatory!
 */
static void
SISLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SISPtr pSiS = SISPTR(pScrn);
#ifdef XF86DRI
    ScreenPtr pScreen;

    if(pSiS->directRenderingEnabled) {
       pScreen = screenInfo.screens[scrnIndex];
       DRILock(pScreen, 0);
    }
#endif

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif
    
    if(pSiS->CursorInfoPtr) {
#ifdef SISDUALHEAD    
       if(pSiS->DualHeadMode) {
          if(!pSiS->SecondHead) {
	     pSiS->ForceCursorOff = TRUE;
	     pSiS->CursorInfoPtr->HideCursor(pScrn);
	     SISWaitVBRetrace(pScrn);
	     pSiS->ForceCursorOff = FALSE;
	  }
       } else {
#endif
          pSiS->CursorInfoPtr->HideCursor(pScrn);
          SISWaitVBRetrace(pScrn);
#ifdef SISDUALHEAD	  
       }	
#endif       
    }

    SISBridgeRestore(pScrn);

    if(pSiS->UseVESA) {

       /* This is a q&d work-around for a BIOS bug. In case we disabled CRT2,
    	* VBESaveRestore() does not restore CRT1. So we set any mode now,
	* because VBESetVBEMode correctly restores CRT1. Afterwards, we
	* can call VBESaveRestore to restore original mode.
	*/
       if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (!(pSiS->VBFlags & DISPTYPE_DISP2)))
	  VBESetVBEMode(pSiS->pVbe, (pSiS->SISVESAModeList->n) | 0xc000, NULL);

       SISVESARestore(pScrn);

    } else {

       SISRestore(pScrn);

    }

    /* We use (otherwise unused) bit 7 to indicate that we are running
     * to keep sisfb to change the displaymode (this would result in
     * lethal display corruption upon quitting X or changing to a VT
     * until a reboot)
     */
    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
       orSISIDXREG(SISCR,0x34,0x80);
    }

    vgaHWLock(hwp);
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 * Mandatory!
 */
static Bool
SISCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef XF86DRI
    if(pSiS->directRenderingEnabled) {
       SISDRICloseScreen(pScreen);
       pSiS->directRenderingEnabled = FALSE;
    }
#endif

    if(pScrn->vtSema) {

        if(pSiS->CursorInfoPtr) {
#ifdef SISDUALHEAD
           if(pSiS->DualHeadMode) {
              if(!pSiS->SecondHead) {
	         pSiS->ForceCursorOff = TRUE;
	         pSiS->CursorInfoPtr->HideCursor(pScrn);
	         SISWaitVBRetrace(pScrn);
	         pSiS->ForceCursorOff = FALSE;
	      }
           } else {
#endif
             pSiS->CursorInfoPtr->HideCursor(pScrn);
             SISWaitVBRetrace(pScrn);
#ifdef SISDUALHEAD
           }
#endif
	}

        SISBridgeRestore(pScrn);

	if(pSiS->UseVESA) {

	  /* This is a q&d work-around for a BIOS bug. In case we disabled CRT2,
    	   * VBESaveRestore() does not restore CRT1. So we set any mode now,
	   * because VBESetVBEMode correctly restores CRT1. Afterwards, we
	   * can call VBESaveRestore to restore original mode.
	   */
           if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (!(pSiS->VBFlags & DISPTYPE_DISP2)))
	      VBESetVBEMode(pSiS->pVbe, (pSiS->SISVESAModeList->n) | 0xc000, NULL);

	   SISVESARestore(pScrn);

	} else {

	   SISRestore(pScrn);

	}

        vgaHWLock(hwp);

    }

    /* We should restore the mode number in case vtsema = false as well,
     * but since we haven't register access then we can't do it. I think
     * I need to rework the save/restore stuff, like saving the video
     * status when returning to the X server and by that save me the
     * trouble if sisfb was started from a textmode VT while X was on.
     */
    
    SISUnmapMem(pScrn);
    vgaHWUnmapMem(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       pSiSEnt = pSiS->entityPrivate;
       pSiSEnt->refCount--;
    }
#endif

    if(pSiS->pInt) {
       xf86FreeInt10(pSiS->pInt);
       pSiS->pInt = NULL;
    }

    if(pSiS->AccelLinearScratch) {
       xf86FreeOffscreenLinear(pSiS->AccelLinearScratch);
       pSiS->AccelLinearScratch = NULL;
    }

    if(pSiS->AccelInfoPtr) {
       XAADestroyInfoRec(pSiS->AccelInfoPtr);
       pSiS->AccelInfoPtr = NULL;
    }

    if(pSiS->CursorInfoPtr) {
       xf86DestroyCursorInfoRec(pSiS->CursorInfoPtr);
       pSiS->CursorInfoPtr = NULL;
    }

    if(pSiS->ShadowPtr) {
       xfree(pSiS->ShadowPtr);
       pSiS->ShadowPtr = NULL;
    }

    if(pSiS->DGAModes) {
       xfree(pSiS->DGAModes);
       pSiS->DGAModes = NULL;
    }

    if(pSiS->adaptor) {
       xfree(pSiS->adaptor);
       pSiS->adaptor = NULL;
       pSiS->ResetXv = pSiS->ResetXvGamma = NULL;
    }

    pScrn->vtSema = FALSE;

    /* Restore Blockhandler */
    pScreen->BlockHandler = pSiS->BlockHandler;

    pScreen->CloseScreen = pSiS->CloseScreen;

    return(*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any per-generation data structures */

/* Optional */
static void
SISFreeScreen(int scrnIndex, int flags)
{
    if(xf86LoaderCheckSymbol("vgaHWFreeHWRec")) {
       vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    }

    SISFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

static ModeStatus
SISValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->UseVESA) {
       if(SiSCalcVESAModeIndex(pScrn, mode))
	  return(MODE_OK);
       else
	  return(MODE_BAD);
    }

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SISDUALHEAD
       if(pSiS->DualHeadMode) {
          if(pSiS->SecondHead) {
	     if(SiS_CalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	        return(MODE_BAD);
	  } else {
	     if(SiS_CheckCalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	        return(MODE_BAD);
	  }
       } else
#endif
#ifdef SISMERGED
       if(pSiS->MergedFB) {
	  if(!mode->Private) {
	     if(!pSiS->CheckForCRT2) {
	        if(SiS_CalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	           return(MODE_BAD);
	     } else {
	        if(SiS_CheckCalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes2) < 0x14)
	           return(MODE_BAD);
	     }
	  } else {
	     if(SiS_CalcModeIndex(pScrn, ((SiSMergedDisplayModePtr)mode->Private)->CRT1,
		                  pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	        return(MODE_BAD);

	     if(SiS_CheckCalcModeIndex(pScrn, ((SiSMergedDisplayModePtr)mode->Private)->CRT2,
		                  pSiS->VBFlags, pSiS->HaveCustomModes2) < 0x14)
	        return(MODE_BAD);
 	  }
       } else
#endif
              {

	  if(pSiS->VBFlags & CRT1_LCDA) {
	     if(SiS_CalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	        return(MODE_BAD);
	  }
	  if(SiS_CheckCalcModeIndex(pScrn, mode, pSiS->VBFlags, pSiS->HaveCustomModes) < 0x14)
	     return(MODE_BAD);
       }
    }

    return(MODE_OK);
}

/* Do screen blanking
 *
 * Mandatory
 */
static Bool
SISSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    if((pScrn != NULL) && pScrn->vtSema) {

    	SISPtr pSiS = SISPTR(pScrn);

#ifdef UNLOCK_ALWAYS
        sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

        if(pSiS->VBFlags & (CRT2_LCD | CRT1_LCDA)) {

	   if(pSiS->VGAEngine == SIS_300_VGA) {

	      if(pSiS->VBFlags & (VB_301LV|VB_302LV|VB_302ELV)) {
	         if(!xf86IsUnblank(mode)) {
	            pSiS->Blank = TRUE;
	  	    SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
	         } else {
	            pSiS->Blank = FALSE;
	            SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	         }
	      } else if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH)) {
	         if(!pSiS->Blank) {
	            inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	         }
	         if(!xf86IsUnblank(mode)) {
    		    pSiS->Blank = TRUE;
		    outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
	         } else {
    		    pSiS->Blank = FALSE;
		    outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	         }
	      }

	   } else if(pSiS->VGAEngine == SIS_315_VGA) {

	      if(!pSiS->Blank) {
		 inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		 if(pSiS->sishw_ext.jChipType >= SIS_661) pSiS->LCDon &= 0x0f;
	      }

	      if(pSiS->VBFlags & VB_CHRONTEL) {
	         if(!xf86IsUnblank(mode)) {
		    pSiS->Blank = TRUE;
		    SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
	         } else {
	            pSiS->Blank = FALSE;
	            SiS_Chrontel701xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	         }
	      } else if(pSiS->VBFlags & VB_LVDS) {
	         if(!xf86IsUnblank(mode)) {
	            pSiS->Blank = TRUE;
	 	    outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
	         } else {
	            pSiS->Blank = FALSE;
	  	    outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	         }
	      } else if(pSiS->VBFlags & (VB_301LV|VB_302LV|VB_302ELV)) {
	         if(!xf86IsUnblank(mode)) {
	            pSiS->Blank = TRUE;
	  	    SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
	         } else {
	            pSiS->Blank = FALSE;
	            SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	         }
	      }

	   }

	}

    }

    return vgaHWSaveScreen(pScreen, mode);
}

#ifdef SISDUALHEAD
/* SaveScreen for dual head mode */
static Bool
SISSaveScreenDH(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Bool checkit = FALSE;

    if((pScrn != NULL) && pScrn->vtSema) {

       SISPtr pSiS = SISPTR(pScrn);

       if((pSiS->SecondHead) && ((!(pSiS->VBFlags & CRT1_LCDA)) || (pSiS->VBFlags & VB_301C))) {

	  /* Slave head is always CRT1 */
	  if(pSiS->VBFlags & CRT1_LCDA) pSiS->Blank = xf86IsUnblank(mode) ? FALSE : TRUE;

	  return vgaHWSaveScreen(pScreen, mode);

       } else {

	  /* Master head is always CRT2 */
	  /* But we land here if CRT1 is LCDA, too */

	  /* We can only blank LCD, not other CRT2 devices */
	  if(!(pSiS->VBFlags & (CRT2_LCD|CRT1_LCDA))) return TRUE;

	  /* enable access to extended sequencer registers */
#ifdef UNLOCK_ALWAYS
          sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

 	  if(pSiS->VGAEngine == SIS_300_VGA) {

	     if(pSiS->VBFlags & (VB_301LV|VB_302LV|VB_302ELV)) {
	        checkit = TRUE;
	        if(!xf86IsUnblank(mode)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		} else {
		   SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		}
	     } else if(pSiS->VBFlags & (VB_LVDS|VB_30xBDH)) {
	        if(!pSiS->BlankCRT2) {
		   inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		}
		checkit = TRUE;
		if(!xf86IsUnblank(mode)) {
		   outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
		} else {
		   outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		}
	     }

          } else if(pSiS->VGAEngine == SIS_315_VGA) {

 	     if(!pSiS->BlankCRT2) {
	 	inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		if(pSiS->sishw_ext.jChipType >= SIS_661) pSiS->LCDon &= 0x0f;
	     }

	     if(pSiS->VBFlags & VB_CHRONTEL) {
	        checkit = TRUE;
		if(!xf86IsUnblank(mode)) {
		   SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		} else {
		   SiS_Chrontel701xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		}
	     } else if(pSiS->VBFlags & VB_LVDS) {
	        checkit = TRUE;
		if(!xf86IsUnblank(mode)) {
		   outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
		} else {
		   outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		}
	     } else if(pSiS->VBFlags & (VB_301LV|VB_302LV|VB_302ELV)) {
	        checkit = TRUE;
		if(!xf86IsUnblank(mode)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		} else {
		   SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		}
	     }

	  }

	  if(checkit) {
	     if(!pSiS->SecondHead) pSiS->BlankCRT2 = xf86IsUnblank(mode) ? FALSE : TRUE;
	     else if(pSiS->VBFlags & CRT1_LCDA) pSiS->Blank = xf86IsUnblank(mode) ? FALSE : TRUE;
	  }

       }
    }
    return TRUE;
}
#endif

#ifdef DEBUG
static void
SiSDumpModeInfo(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Clock : %x\n", mode->Clock);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Display : %x\n", mode->CrtcHDisplay);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Blank Start : %x\n", mode->CrtcHBlankStart);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Sync Start : %x\n", mode->CrtcHSyncStart);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Sync End : %x\n", mode->CrtcHSyncEnd);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Blank End : %x\n", mode->CrtcHBlankEnd);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Total : %x\n", mode->CrtcHTotal);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz Skew : %x\n", mode->CrtcHSkew);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Hz HAdjusted : %x\n", mode->CrtcHAdjusted);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Display : %x\n", mode->CrtcVDisplay);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Blank Start : %x\n", mode->CrtcVBlankStart);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Sync Start : %x\n", mode->CrtcVSyncStart);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Sync End : %x\n", mode->CrtcVSyncEnd);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Blank End : %x\n", mode->CrtcVBlankEnd);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt Total : %x\n", mode->CrtcVTotal);
    xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Vt VAdjusted : %x\n", mode->CrtcVAdjusted);
}
#endif

static void
SISModifyModeInfo(DisplayModePtr mode)
{
    if(mode->CrtcHBlankStart == mode->CrtcHDisplay)
        mode->CrtcHBlankStart++;
    if(mode->CrtcHBlankEnd == mode->CrtcHTotal)
        mode->CrtcHBlankEnd--;
    if(mode->CrtcVBlankStart == mode->CrtcVDisplay)
        mode->CrtcVBlankStart++;
    if(mode->CrtcVBlankEnd == mode->CrtcVTotal)
        mode->CrtcVBlankEnd--;
}

/* Enable the Turboqueue/Commandqueue (For 300 and 315/330 series only) */
void
SiSEnableTurboQueue(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned short SR26, SR27;
    unsigned long  temp;

    switch (pSiS->VGAEngine) {
	case SIS_300_VGA:
	   if((!pSiS->NoAccel) && (pSiS->TurboQueue)) {
	        /* TQ size is always 512k */
           	temp = (pScrn->videoRam/64) - 8;
           	SR26 = temp & 0xFF;
           	inSISIDXREG(SISSR, 0x27, SR27);
		SR27 &= 0xFC;
		SR27 |= (0xF0 | ((temp >> 8) & 3));
           	outSISIDXREG(SISSR, 0x26, SR26);
           	outSISIDXREG(SISSR, 0x27, SR27);
	   }
	   break;

	case SIS_315_VGA:
	   if(!pSiS->NoAccel) {
	      /* On 315/330 series, there are three queue modes available
	       * which are chosen by setting bits 7:5 in SR26:
	       * 1. MMIO queue mode (bit 5, 0x20). The hardware will keep
	       *    track of the queue, the FIFO, command parsing and so
	       *    on. This is the one comparable to the 300 series.
	       * 2. VRAM queue mode (bit 6, 0x40). In this case, one will
	       *    have to do queue management himself. 
	       * 3. AGP queue mode (bit 7, 0x80). Works as 2., but keeps the
	       *    queue in AGP memory space.
	       * We go VRAM or MMIO here.
	       * SR26 bit 4 is called "Bypass H/W queue".
	       * SR26 bit 1 is called "Enable Command Queue Auto Correction"
	       * SR26 bit 0 resets the queue
	       * Size of queue memory is encoded in bits 3:2 like this:
	       *    00  (0x00)  512K
	       *    01  (0x04)  1M
	       *    10  (0x08)  2M
	       *    11  (0x0C)  4M
	       * The queue location is to be written to 0x85C0.
	       */
#ifdef SISVRAMQ
	      /* We use VRAM Cmd Queue, not MMIO or AGP */
	      unsigned char tempCR55 = 0;

#ifdef SISDUALHEAD
	      if(pSiS->DualHeadMode) {
	         SISEntPtr pSiSEnt = pSiS->entityPrivate;
	         pSiS->cmdQ_SharedWritePort = &(pSiSEnt->cmdQ_SharedWritePort_2D);
	      } else
#endif
	         pSiS->cmdQ_SharedWritePort = &(pSiS->cmdQ_SharedWritePort_2D);

	      /* Set Command Queue Threshold to max value 11111b (?) */
	      outSISIDXREG(SISSR, 0x27, 0x1F);
	      /* No idea what this does */
	      if(pSiS->sishw_ext.jChipType <= SIS_330) {
	         inSISIDXREG(SISCR, 0x55, tempCR55) ;
    	         andSISIDXREG(SISCR, 0x55, 0x33) ;
	      }
	      /* Syncronous reset for Command Queue */
	      outSISIDXREG(SISSR, 0x26, 0x01);
	      MMIO_OUT32(pSiS->IOBase, 0x85c4, 0);
	      /* Enable VRAM Command Queue mode */
	      switch(pSiS->cmdQueueSize) {
    		case 1*1024*1024: SR26 = (0x40 | 0x04 | 0x01); break;
    		case 2*1024*1024: SR26 = (0x40 | 0x08 | 0x01); break;
    		case 4*1024*1024: SR26 = (0x40 | 0x0C | 0x01); break;
		default:
		                  pSiS->cmdQueueSize = 512 * 1024;
		case    512*1024: SR26 = (0x40 | 0x00 | 0x01);
	      }
    	      outSISIDXREG(SISSR, 0x26, SR26);
	      SR26 &= 0xfe;
	      outSISIDXREG(SISSR, 0x26, SR26);
	      pSiS->cmdQ_SharedWritePort_2D = (unsigned long)(MMIO_IN32(pSiS->IOBase, 0x85c8));
              *(pSiS->cmdQ_SharedWritePort) = pSiS->cmdQ_SharedWritePort_2D;
              MMIO_OUT32(pSiS->IOBase, 0x85c4, pSiS->cmdQ_SharedWritePort_2D);
	      MMIO_OUT32(pSiS->IOBase, 0x85C0, pSiS->cmdQueueOffset);
	      temp = (unsigned long)pSiS->FbBase;
#ifdef SISDUALHEAD
	      if(pSiS->DualHeadMode) {
	         SISEntPtr pSiSEnt = pSiS->entityPrivate;
	         temp = (unsigned long)pSiSEnt->FbBase;
	      }
#endif
              temp += pSiS->cmdQueueOffset;
              pSiS->cmdQueueBase = (unsigned long *)temp;
	      if(pSiS->sishw_ext.jChipType <= SIS_330) {
    	         outSISIDXREG(SISCR, 0x55, tempCR55);
	      }
#else
	      /* For MMIO */
	      /* Set Command Queue Threshold to max value 11111b */
	      outSISIDXREG(SISSR, 0x27, 0x1F);
	      /* Syncronous reset for Command Queue */
	      outSISIDXREG(SISSR, 0x26, 0x01);
	      /* Do some magic (cp readport to writeport) */
	      temp = MMIO_IN32(pSiS->IOBase, 0x85C8);
	      MMIO_OUT32(pSiS->IOBase, 0x85C4, temp);
	      /* Enable MMIO Command Queue mode (0x20),
	       * Enable_command_queue_auto_correction (0x02)
	       *        (no idea, but sounds good, so use it)
	       * 512k (0x00) (does this apply to MMIO mode?) */
    	      outSISIDXREG(SISSR, 0x26, 0x22);
	      /* Calc Command Queue position (Q is always 512k)*/
	      temp = (pScrn->videoRam - 512) * 1024;
	      /* Set Q position */
	      MMIO_OUT32(pSiS->IOBase, 0x85C0, temp);
#endif
	   }
	   break;
	default:
	   break;
    }
}

/* Things to do before a ModeSwitch. We set up the
 * video bridge configuration and the TurboQueue.
 */
void SiSPreSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode, int viewmode)
{
    SISPtr         pSiS = SISPTR(pScrn);
    unsigned char  CR30, CR31, CR32, CR33;
    unsigned char  CR39 = 0, CR3B = 0;
    unsigned char  CR17, CR38 = 0;
    unsigned char  CR35 = 0, CR79 = 0;
    unsigned long  vbflag;
    int            temp = 0, i;
    int 	   crt1rateindex = 0;
    DisplayModePtr mymode;
#ifdef SISMERGED
    DisplayModePtr mymode2 = NULL;
#endif

#ifdef SISMERGED
    if(pSiS->MergedFB) {
       mymode = ((SiSMergedDisplayModePtr)mode->Private)->CRT1;
       mymode2 = ((SiSMergedDisplayModePtr)mode->Private)->CRT2;
    } else
#endif
    mymode = mode;

    vbflag = pSiS->VBFlags;
    pSiS->IsCustom = FALSE;
#ifdef SISMERGED
    pSiS->IsCustomCRT2 = FALSE;

    if(pSiS->MergedFB) {
       /* CRT2 */
       if(vbflag & CRT2_LCD) {
          if(pSiS->SiS_Pr->CP_HaveCustomData) {
	     for(i=0; i<7; i++) {
	        if(pSiS->SiS_Pr->CP_DataValid[i]) {
	           if((mymode2->HDisplay == pSiS->SiS_Pr->CP_HDisplay[i]) &&
	              (mymode2->VDisplay == pSiS->SiS_Pr->CP_VDisplay[i])) {
	              if(mymode2->type & M_T_BUILTIN) {
	                 pSiS->IsCustomCRT2 = TRUE;
		      }
	           }
		}
	     }
	  }
       }
       if(vbflag & (CRT2_VGA|CRT2_LCD)) {
          if(pSiS->AddedPlasmaModes) {
	     if(mymode2->type & M_T_BUILTIN) {
	        pSiS->IsCustomCRT2 = TRUE;
	     }
	  }
	  if(pSiS->HaveCustomModes2) {
             if(!(mymode2->type & M_T_DEFAULT)) {
	        pSiS->IsCustomCRT2 = TRUE;
             }
          }
       }
       /* CRT1 */
       if(pSiS->HaveCustomModes) {
          if(!(mymode->type & M_T_DEFAULT)) {
	     pSiS->IsCustom = TRUE;
          }
       }
    } else
#endif
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(!pSiS->SecondHead) {
          /* CRT2 */
          if(vbflag & CRT2_LCD) {
	     if(pSiS->SiS_Pr->CP_HaveCustomData) {
	        for(i=0; i<7; i++) {
                   if(pSiS->SiS_Pr->CP_DataValid[i]) {
	              if((mymode->HDisplay == pSiS->SiS_Pr->CP_HDisplay[i]) &&
	                 (mymode->VDisplay == pSiS->SiS_Pr->CP_VDisplay[i])) {
	                 if(mymode->type & M_T_BUILTIN) {
	                    pSiS->IsCustom = TRUE;
		         }
		      }
		   }
	        }
	     }
          }
	  if(vbflag & (CRT2_VGA|CRT2_LCD)) {
             if(pSiS->AddedPlasmaModes) {
	        if(mymode->type & M_T_BUILTIN) {
	           pSiS->IsCustom = TRUE;
	        }
	     }
	     if(pSiS->HaveCustomModes) {
                if(!(mymode->type & M_T_DEFAULT)) {
	           pSiS->IsCustom = TRUE;
                }
             }
          }
       } else {
          /* CRT1 */
          if(pSiS->HaveCustomModes) {
             if(!(mymode->type & M_T_DEFAULT)) {
	        pSiS->IsCustom = TRUE;
             }
          }
       }
    } else
#endif
    {
       if(vbflag & CRT2_LCD) {
          if(pSiS->SiS_Pr->CP_HaveCustomData) {
	     for(i=0; i<7; i++) {
	        if(pSiS->SiS_Pr->CP_DataValid[i]) {
                   if((mymode->HDisplay == pSiS->SiS_Pr->CP_HDisplay[i]) &&
	              (mymode->VDisplay == pSiS->SiS_Pr->CP_VDisplay[i])) {
	              if(mymode->type & M_T_BUILTIN) {
	                 pSiS->IsCustom = TRUE;
	              }
		   }
	        }
	     }
          }
       }
       if(vbflag & (CRT2_LCD|CRT2_VGA)) {
          if(pSiS->AddedPlasmaModes) {
             if(mymode->type & M_T_BUILTIN) {
	        pSiS->IsCustom = TRUE;
             }
          }
       }
       if((pSiS->HaveCustomModes) && (!(vbflag & CRT2_TV))) {
          if(!(mymode->type & M_T_DEFAULT)) {
	     pSiS->IsCustom = TRUE;
          }
       }
    }

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);    /* Unlock Registers */
#endif

    inSISIDXREG(SISCR, 0x30, CR30);
    inSISIDXREG(SISCR, 0x31, CR31);
    CR32 = pSiS->newCR32;
    inSISIDXREG(SISCR, 0x33, CR33);

    if(pSiS->Chipset == PCI_CHIP_SIS660) {

       inSISIDXREG(SISCR, 0x35, CR35);
       inSISIDXREG(SISCR, 0x38, CR38);
       inSISIDXREG(SISCR, 0x39, CR39);

       xf86DrvMsgVerb(pScrn->scrnIndex, X_PROBED, 4,
	   "Before: CR30=0x%02x,CR31=0x%02x,CR32=0x%02x,CR33=0x%02x,CR35=0x%02x,CR38=0x%02x\n",
              CR30, CR31, CR32, CR33, CR35, CR38);
       CR38 &= ~0x07;

    } else {

       if(pSiS->Chipset != PCI_CHIP_SIS300) {
          switch(pSiS->VGAEngine) {
             case SIS_300_VGA: temp = 0x35; break;
             case SIS_315_VGA: temp = 0x38; break;
          }
          if(temp) inSISIDXREG(SISCR, temp, CR38);
       }
       if(pSiS->VGAEngine == SIS_315_VGA) {
          inSISIDXREG(SISCR, 0x79, CR79);
          CR38 &= ~0x3b;   			/* Clear LCDA/DualEdge and YPbPr bits */
       }
       inSISIDXREG(SISCR, 0x3b, CR3B);
       xf86DrvMsgVerb(pScrn->scrnIndex, X_PROBED, 4,
	   "Before: CR30=0x%02x, CR31=0x%02x, CR32=0x%02x, CR33=0x%02x, CR%02x=0x%02x\n",
              CR30, CR31, CR32, CR33, temp, CR38);
    }

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4, "VBFlags=0x%lx\n", pSiS->VBFlags);

    CR30 = 0x00;
    CR31 &= ~0x60;  /* Clear VB_Drivermode & VB_OutputDisable */
    CR31 |= 0x04;   /* Set VB_NotSimuMode (not for 30xB/1400x1050?) */
    CR35 = 0x00;

    if(pSiS->Chipset != PCI_CHIP_SIS660) {
       if(!pSiS->AllowHotkey) {
          CR31 |= 0x80;   /* Disable hotkey-switch */
       }
       CR79 &= ~0x10;     /* Enable Backlight control on 315 series */
    }

    SiS_SetEnableDstn(pSiS->SiS_Pr, FALSE);
    SiS_SetEnableFstn(pSiS->SiS_Pr, FALSE);

    if((vbflag & CRT1_LCDA) && (viewmode == SIS_MODE_CRT1)) {

       CR38 |= 0x02;

    } else {

       switch(vbflag & (CRT2_TV|CRT2_LCD|CRT2_VGA)) {

       case CRT2_TV:

          CR38 &= ~0xC0; 	/* Clear Pal M/N bits */

          if((vbflag & VB_CHRONTEL) && (vbflag & TV_CHSCART)) {			/* Chrontel */
	     CR30 |= 0x10;
	     CR38 |= 0x04;
	     CR38 &= ~0x08;
	     CR31 |= 0x01;
	  } else if((vbflag & VB_CHRONTEL) && (vbflag & TV_CHYPBPR525I)) {	/* Chrontel */
	     CR38 |= 0x08;
	     CR38 &= ~0x04;
	     CR31 &= ~0x01;
          } else if(vbflag & TV_HIVISION) {	/* SiS bridge */
	     if(pSiS->Chipset == PCI_CHIP_SIS660) {
	        CR38 |= 0x04;
	        CR35 |= 0x60;
	     } else {
	        CR30 |= 0x80;
		if(pSiS->VGAEngine == SIS_315_VGA) {
		   if(vbflag & (VB_301LV | VB_302LV | VB_301C)) {
		      CR38 |= (0x08 | 0x30);
		   }
		}
	     }
	     CR31 |= 0x01;
	     CR35 |= 0x01;
	  } else if(vbflag & TV_YPBPR) {					/* SiS bridge */
	     if(pSiS->Chipset == PCI_CHIP_SIS660) {
	        CR38 |= 0x04;
	        if(vbflag & TV_YPBPR525P)       CR35 |= 0x20;
		else if(vbflag & TV_YPBPR750P)  CR35 |= 0x40;
		else if(vbflag & TV_YPBPR1080I) CR35 |= 0x60;
		CR31 &= ~0x01;
		CR35 &= ~0x01;
		CR39 &= ~0x03;
		if((vbflag & TV_YPBPRAR) == TV_YPBPR43LB)     CR39 |= 0x00;
		else if((vbflag & TV_YPBPRAR) == TV_YPBPR43)  CR39 |= 0x01;
		else if((vbflag & TV_YPBPRAR) == TV_YPBPR169) CR39 |= 0x02;
		else					      CR39 |= 0x03;
	     } else if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPR) {
	        CR30 |= 0x80;
		CR38 |= 0x08;
	        if(vbflag & TV_YPBPR525P)       CR38 |= 0x10;
		else if(vbflag & TV_YPBPR750P)  CR38 |= 0x20;
		else if(vbflag & TV_YPBPR1080I) CR38 |= 0x30;
		CR31 &= ~0x01;
		if(pSiS->SiS_SD_Flags & SiS_SD_SUPPORTYPBPRAR) {
		   CR3B &= ~0x03;
		   if((vbflag & TV_YPBPRAR) == TV_YPBPR43LB)     CR3B |= 0x00;
		   else if((vbflag & TV_YPBPRAR) == TV_YPBPR43)  CR3B |= 0x03;
		   else if((vbflag & TV_YPBPRAR) == TV_YPBPR169) CR3B |= 0x01;
		   else					         CR3B |= 0x03;
		}
	     }
          } else {								/* All */
	     if(vbflag & TV_SCART)  CR30 |= 0x10;
	     if(vbflag & TV_SVIDEO) CR30 |= 0x08;
	     if(vbflag & TV_AVIDEO) CR30 |= 0x04;
	     if(!(CR30 & 0x1C))	    CR30 |= 0x08;    /* default: SVIDEO */

	     if(vbflag & TV_PAL) {
		CR31 |= 0x01;
		CR35 |= 0x01;
		if( (vbflag & VB_SISBRIDGE) ||
		    ((vbflag & VB_CHRONTEL) && (pSiS->ChrontelType == CHRONTEL_701x)) )  {
		   if(vbflag & TV_PALM) {
		      CR38 |= 0x40;
		      CR35 |= 0x04;
		   } else if(vbflag & TV_PALN) {
		      CR38 |= 0x80;
		      CR35 |= 0x08;
	  	   }
	        }
	     } else {
		CR31 &= ~0x01;
		CR35 &= ~0x01;
		if(vbflag & TV_NTSCJ) {
		   CR38 |= 0x40;  /* TW, not BIOS */
		   CR35 |= 0x02;
	 	}
	     }
	     if(vbflag & TV_SCART) {
	        CR31 |= 0x01;
		CR35 |= 0x01;
	     }
	  }

	  CR31 &= ~0x04;   /* Clear NotSimuMode */
	  pSiS->SiS_Pr->SiS_CHOverScan = pSiS->UseCHOverScan;
	  if((pSiS->OptTVSOver == 1) && (pSiS->ChrontelType == CHRONTEL_700x)) {
	     pSiS->SiS_Pr->SiS_CHSOverScan = TRUE;
	  } else {
	     pSiS->SiS_Pr->SiS_CHSOverScan = FALSE;
	  }
#ifdef SIS_CP
	  SIS_CP_DRIVER_CONFIG
#endif
          break;

       case CRT2_LCD:
          CR30 |= 0x20;
	  SiS_SetEnableDstn(pSiS->SiS_Pr, pSiS->DSTN);
	  SiS_SetEnableFstn(pSiS->SiS_Pr, pSiS->FSTN);
          break;

       case CRT2_VGA:
          CR30 |= 0x40;
          break;

       default:
          CR30 |= 0x00;
          CR31 |= 0x20;    /* VB_OUTPUT_DISABLE */
	  if(pSiS->UseVESA) {
	     crt1rateindex = SISSearchCRT1Rate(pScrn, mymode);
	  }
       }

    }

    if(vbflag & CRT1_LCDA) {
       switch(viewmode) {
       case SIS_MODE_CRT1:
          CR38 |= 0x01;
          break;
       case SIS_MODE_CRT2:
          if(vbflag & (CRT2_TV|CRT2_VGA)) {
             CR30 |= 0x02;
	     CR38 |= 0x01;
	  } else {
	     CR38 |= 0x03;
	  }
          break;
       case SIS_MODE_SIMU:
       default:
          if(vbflag & (CRT2_TV|CRT2_LCD|CRT2_VGA)) {
             CR30 |= 0x01;
	  }
          break;
       }
    } else {
       if(vbflag & (CRT2_TV|CRT2_LCD|CRT2_VGA)) {
          CR30 |= 0x01;
       }
    }

    /* for VESA: no DRIVERMODE, otherwise
     * -) CRT2 will not be initialized correctly when using mode
     *    where LCD has to scale, and
     * -) CRT1 will have too low rate
     */
     if(pSiS->UseVESA) {
        CR31 &= ~0x40;   /* Clear Drivermode */
	CR31 |= 0x06;    /* Set SlaveMode, Enable SimuMode in Slavemode */
#ifdef TWDEBUG
        CR31 |= 0x40;    /* DEBUG (for non-slave mode VESA) */
	crt1rateindex = SISSearchCRT1Rate(pScrn, mymode);
#endif
     } else {
        CR31 |=  0x40;  /* Set Drivermode */
	CR31 &=  ~0x06; /* Disable SlaveMode, disable SimuMode in SlaveMode */
	if(!pSiS->IsCustom) {
           crt1rateindex = SISSearchCRT1Rate(pScrn, mymode);
	} else {
	   crt1rateindex = CR33;
	}
     }

#ifdef SISDUALHEAD
     if(pSiS->DualHeadMode) {
        if(pSiS->SecondHead) {
	    /* CRT1 */
	    CR33 &= 0xf0;
	    if(!(vbflag & CRT1_LCDA)) {
	       CR33 |= (crt1rateindex & 0x0f);
	    }
	} else {
	    /* CRT2 */
	    CR33 &= 0x0f;
	    if(vbflag & CRT2_VGA) {
	       CR33 |= ((crt1rateindex << 4) & 0xf0);
	    }
	}
     } else
#endif
#ifdef SISMERGED
     if(pSiS->MergedFB) {
        CR33 = 0;
	if(!(vbflag & CRT1_LCDA)) {
	   CR33 |= (crt1rateindex & 0x0f);
	}
        if(vbflag & CRT2_VGA) {
	   if(!pSiS->IsCustomCRT2) {
	      CR33 |= (SISSearchCRT1Rate(pScrn, mymode2) << 4);
	   }
	}
     } else
#endif
     {
        CR33 = 0;
	if(!(vbflag & CRT1_LCDA)) {
	   CR33 |= (crt1rateindex & 0x0f);
	}
        if(vbflag & CRT2_VGA) {
           CR33 |= ((crt1rateindex & 0x0f) << 4);
	}
	if((!(pSiS->UseVESA)) && (vbflag & CRT2_ENABLE)) {
	   if(pSiS->CRT1off) CR33 &= 0xf0;
	}
     }

     if(pSiS->Chipset == PCI_CHIP_SIS660) {

        CR31 &= 0xfe;   /* Clear PAL flag (now in CR35) */
	CR38 &= 0x07;   /* Use only LCDA and HiVision/YPbPr bits */
	outSISIDXREG(SISCR, 0x30, CR30);
	outSISIDXREG(SISCR, 0x31, CR31);
	outSISIDXREG(SISCR, 0x33, CR33);
	outSISIDXREG(SISCR, 0x35, CR35);
	setSISIDXREG(SISCR, 0x38, 0xf8, CR38);
	outSISIDXREG(SISCR, 0x39, CR39);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		"After:  CR30=0x%02x,CR31=0x%02x,CR33=0x%02x,CR35=0x%02x,CR38=%02x\n",
		    CR30, CR31, CR33, CR35, CR38);

     } else {

        outSISIDXREG(SISCR, 0x30, CR30);
        outSISIDXREG(SISCR, 0x31, CR31);
        outSISIDXREG(SISCR, 0x33, CR33);
        if(temp) {
           outSISIDXREG(SISCR, temp, CR38);
        }
	if(pSiS->VGAEngine == SIS_315_VGA) {
	   outSISIDXREG(SISCR, 0x3b, CR3B);
	   outSISIDXREG(SISCR, 0x79, CR79);
	}
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		"After:  CR30=0x%02x,CR31=0x%02x,CR33=0x%02x,CR%02x=%02x\n",
		    CR30, CR31, CR33, temp, CR38);
     }

     pSiS->SiS_Pr->SiS_UseOEM = pSiS->OptUseOEM;

     /* Enable TurboQueue */
#ifdef SISVRAMQ
     if(pSiS->VGAEngine != SIS_315_VGA)
#endif     
        SiSEnableTurboQueue(pScrn);

     if((!pSiS->UseVESA) && (pSiS->VBFlags & CRT2_ENABLE)) {
        /* Switch on CRT1 for modes that require the bridge in SlaveMode */
	andSISIDXREG(SISSR,0x1f,0x3f);
	inSISIDXREG(SISCR, 0x17, CR17);
	if(!(CR17 & 0x80)) {
           orSISIDXREG(SISCR, 0x17, 0x80);
	   outSISIDXREG(SISSR, 0x00, 0x01);
	   usleep(10000);
           outSISIDXREG(SISSR, 0x00, 0x03);
	}
     }
}

/* Functions for adjusting various TV settings */

/* These are used by the PostSetMode() functions as well as
 * the display properties tool SiSCtrl.
 *
 * There is each a Set and a Get routine. The Set functions
 * take a value of the same range as the corresponding option.
 * The Get routines return a value of the same range (although
 * not necessarily the same value as previously set because
 * of the lower resolution of the respective setting compared
 * to the valid range).
 * The Get routines return -2 on error (eg. hardware does not
 * support this setting).
 * Note: The x and y positioning routines accept a position
 * RELATIVE to the default position. All other routines 
 * take ABSOLUTE values.
 *
 * The Set functions will store the property regardless if TV is
 * currently used or not and if the hardware supports the property
 * or not. The Get routines will return this stored
 * value if TV is not currently used (because the register does
 * not contain the correct value then) or if the hardware supports
 * the respective property. This should make it easier for the 
 * display property tool because it does not have to know the
 * hardware features.
 *
 * All the routines are dual head aware. It does not matter
 * if the function is called from the CRT1 or CRT2 session.
 * The values will be in pSiSEnt anyway, and read from there
 * if we're running dual head.
 */

void SiS_SetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
   
   pSiS->chtvlumabandwidthcvbs = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvlumabandwidthcvbs = val;
#endif

   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   
   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 8;
           if((val == 0) || (val == 1)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x03),0xFE);
           }
	   break;
       case CHRONTEL_701x:
           val /= 4;
	   if((val >= 0) && (val <= 3)) {
	       SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x02),0xFC);
	   }
           break;
   }   
}

int SiS_GetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvlumabandwidthcvbs;
      else 
#endif
           return (int)pSiS->chtvlumabandwidthcvbs;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x03) & 0x01) * 8);
      case CHRONTEL_701x:
	   return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x02) & 0x03) * 4);
      default:
           return -2;   
      }
   }
}

void SiS_SetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvlumabandwidthsvideo = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvlumabandwidthsvideo = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
      
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 6;
           if((val >= 0) && (val <= 2)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 9) | 0x03),0xF9);
           }
	   break;
       case CHRONTEL_701x:
           val /= 4;
	   if((val >= 0) && (val <= 3)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 10) | 0x02),0xF3);
	   }
           break;
   }	   
}

int SiS_GetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvlumabandwidthsvideo;
      else 
#endif
           return (int)pSiS->chtvlumabandwidthsvideo;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x03) & 0x06) >> 1) * 6);
      case CHRONTEL_701x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x02) & 0x0c) >> 2) * 4);
      default:
           return -2;   
      }
   }      
}

void SiS_SetCHTVlumaflickerfilter(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvlumaflickerfilter = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvlumaflickerfilter = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 6;
           if((val >= 0) && (val <= 2)) {
	      unsigned short reg = 0;
	      reg = SiS_GetCH70xx(pSiS->SiS_Pr, 0x01);
	      reg = (reg & 0xf0) | ((reg & 0x0c) >> 2) | (val << 2);
              SiS_SetCH70xx(pSiS->SiS_Pr, ((reg << 8) | 0x01));
           }
	   break;
       case CHRONTEL_701x:
           val /= 4;
	   if((val >= 0) && (val <= 3)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 10) | 0x01),0xF3);
	   }
           break;
   } 
}

int SiS_GetCHTVlumaflickerfilter(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
  
   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
          return (int)pSiSEnt->chtvlumaflickerfilter;
      else
#endif      
          return (int)pSiS->chtvlumaflickerfilter;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x03) * 6);
      case CHRONTEL_701x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x0c) >> 2) * 4);
      default:
           return -2;   
      }
   }     
}

void SiS_SetCHTVchromabandwidth(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvchromabandwidth = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvchromabandwidth = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 4;
           if((val >= 0) && (val <= 3)) {
              SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 12) | 0x03),0xCF);
           }
	   break;
       case CHRONTEL_701x:
           val /= 8;
	   if((val >= 0) && (val <= 1)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 12) | 0x02),0xEF);
	   }
           break;
   }	   
}

int SiS_GetCHTVchromabandwidth(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvchromabandwidth;
      else
#endif   
           return (int)pSiS->chtvchromabandwidth;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x03) & 0x30) >> 4) * 4);
      case CHRONTEL_701x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x02) & 0x10) >> 4) * 8);
      default:
           return -2;   
      }
   }    
}

void SiS_SetCHTVchromaflickerfilter(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvchromaflickerfilter = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvchromaflickerfilter = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 6;
           if((val >= 0) && (val <= 2)) {
	      unsigned short reg = 0;
	      reg = SiS_GetCH70xx(pSiS->SiS_Pr, 0x01);
	      reg = (reg & 0xc0) | ((reg & 0x0c) >> 2) | ((reg & 0x03) << 2) | (val << 4);
              SiS_SetCH70xx(pSiS->SiS_Pr, ((reg << 8) | 0x01));
           }
	   break;
       case CHRONTEL_701x:
           val /= 4;
	   if((val >= 0) && (val <= 3)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 12) | 0x01),0xCF);
	   }
           break;
   }	   
}

int SiS_GetCHTVchromaflickerfilter(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvchromaflickerfilter;
      else
#endif
           return (int)pSiS->chtvchromaflickerfilter;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x30) >> 4) * 6);
      case CHRONTEL_701x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x30) >> 4) * 4);
      default:
           return -2;   
      }
   }    
}

void SiS_SetCHTVcvbscolor(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvcvbscolor = val ? 1 : 0;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvcvbscolor = pSiS->chtvcvbscolor;
#endif

   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           if(!val)  SiS_SetCH70xxANDOR(pSiS->SiS_Pr, 0x4003,0x00);
           else      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, 0x0003,~0x40);
	   break;
       case CHRONTEL_701x:
           if(!val)  SiS_SetCH70xxANDOR(pSiS->SiS_Pr, 0x0002,~0x20);
	   else      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, 0x2002,0x00);
           break;
   }
}

int SiS_GetCHTVcvbscolor(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD
      if(pSiSEnt && pSiS->DualHeadMode)
           return (int)pSiSEnt->chtvcvbscolor;
      else
#endif
           return (int)pSiS->chtvcvbscolor;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x03) & 0x40) >> 6) ^ 0x01);
      case CHRONTEL_701x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x02) & 0x20) >> 5) ^ 0x01);
      default:
           return -2;
      }
   }
}

void SiS_SetCHTVtextenhance(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvtextenhance = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvtextenhance = val;
#endif

   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
           val /= 6;
           if((val >= 0) && (val <= 2)) {
	      unsigned short reg = 0;
	      reg = SiS_GetCH70xx(pSiS->SiS_Pr, 0x01);
	      reg = (reg & 0xf0) | ((reg & 0x03) << 2) | val;
              SiS_SetCH70xx(pSiS->SiS_Pr, ((reg << 8) | 0x01));
           }
	   break;
       case CHRONTEL_701x:
           val /= 2;
	   if((val >= 0) && (val <= 7)) {
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x03),0xF8);
	   }
           break;
   }
}

int SiS_GetCHTVtextenhance(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvtextenhance;
      else
#endif      
           return (int)pSiS->chtvtextenhance;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
	   return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x0c) >> 2) * 6);
      case CHRONTEL_701x:
	   return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x03) & 0x07) * 2);
      default:
           return -2;
      }
   }
}

void SiS_SetCHTVcontrast(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->chtvcontrast = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->chtvcontrast = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_CHRONTEL)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   val /= 2;
   if((val >= 0) && (val <= 7)) {
       switch(pSiS->ChrontelType) {
       case CHRONTEL_700x:
              SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x11),0xF8);
	      break;
       case CHRONTEL_701x:
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x08),0xF8);
              break;
       }
       SiS_DDC2Delay(pSiS->SiS_Pr, 1000);
   }
}

int SiS_GetCHTVcontrast(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_CHRONTEL && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->chtvcontrast;
      else
#endif      
           return (int)pSiS->chtvcontrast;
   } else {
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif   
      switch(pSiS->ChrontelType) {
      case CHRONTEL_700x:
           return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x11) & 0x07) * 2);
      case CHRONTEL_701x:
	   return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x08) & 0x07) * 2);
      default:
           return -2;   
      }
   }
}

void SiS_SetSISTVedgeenhance(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->sistvedgeenhance = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->sistvedgeenhance = val;
#endif

   if(!(pSiS->VBFlags & VB_301))  return;
   if(!(pSiS->VBFlags & CRT2_TV)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   val /= 2;
   if((val >= 0) && (val <= 7)) {
      setSISIDXREG(SISPART2,0x3A, 0x1F, (val << 5));
   }
}

int SiS_GetSISTVedgeenhance(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   int result = pSiS->sistvedgeenhance;
   unsigned char temp;
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode) result = pSiSEnt->sistvedgeenhance;
#endif

   if(!(pSiS->VBFlags & VB_301))  return result;
   if(!(pSiS->VBFlags & CRT2_TV)) return result;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   inSISIDXREG(SISPART2, 0x3a, temp);
   return(int)(((temp & 0xe0) >> 5) * 2);
}

void SiS_SetSISTVantiflicker(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->sistvantiflicker = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->sistvantiflicker = val;
#endif
   
   if(!(pSiS->VBFlags & CRT2_TV))      return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE)) return;
   if(pSiS->VBFlags & TV_HIVISION)     return;
   if((pSiS->VBFlags & TV_YPBPR) &&
      (pSiS->VBFlags & (TV_YPBPR525P | TV_YPBPR750P | TV_YPBPR1080I))) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   /* Valid values: 0=off, 1=low, 2=med, 3=high, 4=adaptive */
   if((val >= 0) && (val <= 4)) {
      setSISIDXREG(SISPART2,0x0A,0x8F, (val << 4));
   }
}

int SiS_GetSISTVantiflicker(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   int result = pSiS->sistvantiflicker;
   unsigned char temp;
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode) result = pSiSEnt->sistvantiflicker;
#endif

   if(!(pSiS->VBFlags & VB_SISBRIDGE)) return result;
   if(!(pSiS->VBFlags & CRT2_TV))      return result;
   if(pSiS->VBFlags & TV_HIVISION)     return result;
   if((pSiS->VBFlags & TV_YPBPR) &&
      (pSiS->VBFlags & (TV_YPBPR525P | TV_YPBPR750P | TV_YPBPR1080I))) return result;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   inSISIDXREG(SISPART2, 0x0a, temp);
   return(int)((temp & 0x70) >> 4);
}

void SiS_SetSISTVsaturation(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->sistvsaturation = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->sistvsaturation = val;
#endif

   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE)) return;
   if(pSiS->VBFlags & VB_301) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   val /= 2;
   if((val >= 0) && (val <= 7)) {
      setSISIDXREG(SISPART4,0x21,0xF8, val);
   }
}

int SiS_GetSISTVsaturation(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   int result = pSiS->sistvsaturation;
   unsigned char temp;
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)  result = pSiSEnt->sistvsaturation;
#endif

   if(!(pSiS->VBFlags & VB_SISBRIDGE)) return result;
   if(pSiS->VBFlags & VB_301)          return result;
   if(!(pSiS->VBFlags & CRT2_TV))      return result;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   inSISIDXREG(SISPART4, 0x21, temp);
   return(int)((temp & 0x07) * 2);
}

void SiS_SetSISTVcolcalib(ScrnInfoPtr pScrn, int val, Bool coarse)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
   int ccoarse, cfine, cbase = pSiS->sistvccbase;
   unsigned char temp;

#ifdef SISDUALHEAD
   if(pSiSEnt && pSiS->DualHeadMode) cbase = pSiSEnt->sistvccbase;
#endif

   if(coarse) {
      pSiS->sistvcolcalibc = ccoarse = val;
      cfine = pSiS->sistvcolcalibf;
#ifdef SISDUALHEAD
      if(pSiSEnt) {
         pSiSEnt->sistvcolcalibc = val;
	 if(pSiS->DualHeadMode) cfine = pSiSEnt->sistvcolcalibf;
      }
#endif
   } else {
      pSiS->sistvcolcalibf = cfine = val;
      ccoarse = pSiS->sistvcolcalibc;
#ifdef SISDUALHEAD
      if(pSiSEnt) {
         pSiSEnt->sistvcolcalibf = val;
         if(pSiS->DualHeadMode) ccoarse = pSiSEnt->sistvcolcalibc;
      }
#endif
   }

   if(!(pSiS->VBFlags & CRT2_TV))               return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE))          return;
   if(pSiS->VBFlags & (TV_HIVISION | TV_YPBPR)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   if((cfine >= -128) && (cfine <= 127) && (ccoarse >= -120) && (ccoarse <= 120)) {
      long finalcc = cbase + (((ccoarse * 256) + cfine) * 256);

      inSISIDXREG(SISPART4,0x1f,temp);
      if(!(temp & 0x01)) {
#if 0
         if(pSiS->VBFlags & TV_NTSC) finalcc += 0x21ed8620;
	 else if(pSiS->VBFlags & TV_PALM) finalcc += ?;
	 else if(pSiS->VBFlags & TV_PALM) finalcc += ?;
	 else finalcc += 0x2a05d300;
#endif
      }
      setSISIDXREG(SISPART2,0x31,0x80,((finalcc >> 24) & 0x7f));
      outSISIDXREG(SISPART2,0x32,((finalcc >> 16) & 0xff));
      outSISIDXREG(SISPART2,0x33,((finalcc >> 8) & 0xff));
      outSISIDXREG(SISPART2,0x34,(finalcc & 0xff));
   }
}

int SiS_GetSISTVcolcalib(ScrnInfoPtr pScrn, Bool coarse)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
      if(coarse)  return (int)pSiSEnt->sistvcolcalibc;
      else        return (int)pSiSEnt->sistvcolcalibf;
   else
#endif
   if(coarse)     return (int)pSiS->sistvcolcalibc;
   else           return (int)pSiS->sistvcolcalibf;
}

void SiS_SetSISTVcfilter(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   pSiS->sistvcfilter = val ? 1 : 0;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->sistvcfilter = pSiS->sistvcfilter;
#endif

   if(!(pSiS->VBFlags & CRT2_TV))               return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE))          return;
   if(pSiS->VBFlags & (TV_HIVISION | TV_YPBPR)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   setSISIDXREG(SISPART2,0x30,~0x10,((pSiS->sistvcfilter << 4) & 0x10));
}

int SiS_GetSISTVcfilter(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   int result = pSiS->sistvcfilter;
   unsigned char temp;
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode) result = pSiSEnt->sistvcfilter;
#endif

   if(!(pSiS->VBFlags & VB_SISBRIDGE))          return result;
   if(!(pSiS->VBFlags & CRT2_TV))               return result;
   if(pSiS->VBFlags & (TV_HIVISION | TV_YPBPR)) return result;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   inSISIDXREG(SISPART2, 0x30, temp);
   return(int)((temp & 0x10) ? 1 : 0);
}

void SiS_SetSISTVyfilter(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
   unsigned char p35,p36,p37,p38,p48,p49,p4a,p30;
   int i,j;

   pSiS->sistvyfilter = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->sistvyfilter = pSiS->sistvyfilter;
#endif

   if(!(pSiS->VBFlags & CRT2_TV))               return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE))          return;
   if(pSiS->VBFlags & (TV_HIVISION | TV_YPBPR)) return;

   p35 = pSiS->p2_35; p36 = pSiS->p2_36;
   p37 = pSiS->p2_37; p38 = pSiS->p2_38;
   p48 = pSiS->p2_48; p49 = pSiS->p2_49;
   p4a = pSiS->p2_4a; p30 = pSiS->p2_30;
#ifdef SISDUALHEAD
   if(pSiSEnt && pSiS->DualHeadMode) {
      p35 = pSiSEnt->p2_35; p36 = pSiSEnt->p2_36;
      p37 = pSiSEnt->p2_37; p38 = pSiSEnt->p2_38;
      p48 = pSiSEnt->p2_48; p49 = pSiSEnt->p2_49;
      p4a = pSiSEnt->p2_4a; p30 = pSiSEnt->p2_30;
   }
#endif
   p30 &= 0x20;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   switch(pSiS->sistvyfilter) {
   case 0:
      andSISIDXREG(SISPART2,0x30,0xdf);
      break;
   case 1:
      outSISIDXREG(SISPART2,0x35,p35);
      outSISIDXREG(SISPART2,0x36,p36);
      outSISIDXREG(SISPART2,0x37,p37);
      outSISIDXREG(SISPART2,0x38,p38);
      if(!(pSiS->VBFlags & VB_301)) {
         outSISIDXREG(SISPART2,0x48,p48);
         outSISIDXREG(SISPART2,0x49,p49);
         outSISIDXREG(SISPART2,0x4a,p4a);
      }
      setSISIDXREG(SISPART2,0x30,0xdf,p30);
      break;
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
   case 8:
      if(!(pSiS->VBFlags & (TV_PALM | TV_PALN | TV_NTSCJ))) {
         int yindex301 = -1, yindex301B = -1;
	 unsigned char p3d4_34;

	 inSISIDXREG(SISCR,0x34,p3d4_34);

	 switch((p3d4_34 & 0x7f)) {
	 case 0x59:  /* 320x200 */
	 case 0x41:
	 case 0x4f:
	 case 0x50:  /* 320x240 */
	 case 0x56:
	 case 0x53:
	    yindex301  = (pSiS->VBFlags & TV_NTSC) ? 0 : 4;
	    break;
	 case 0x2f:  /* 640x400 */
	 case 0x5d:
	 case 0x5e:
	 case 0x2e:  /* 640x480 */
	 case 0x44:
	 case 0x62:
	    yindex301  = (pSiS->VBFlags & TV_NTSC) ? 1 : 5;
	    yindex301B = (pSiS->VBFlags & TV_NTSC) ? 0 : 4;
	    break;
	 case 0x31:   /* 720x480 */
	 case 0x33:
	 case 0x35:
	 case 0x32:   /* 720x576 */
	 case 0x34:
	 case 0x36:
	 case 0x5f:   /* 768x576 */
	 case 0x60:
	 case 0x61:
	    yindex301  = (pSiS->VBFlags & TV_NTSC) ? 2 : 6;
	    yindex301B = (pSiS->VBFlags & TV_NTSC) ? 1 : 5;
	    break;
	 case 0x51:   /* 400x300 */
	 case 0x57:
	 case 0x54:
	 case 0x30:   /* 800x600 */
	 case 0x47:
	 case 0x63:
	    yindex301  = (pSiS->VBFlags & TV_NTSC) ? 3 : 7;
	    yindex301B = (pSiS->VBFlags & TV_NTSC) ? 2 : 6;
	    break;
	 case 0x52:   /* 512x384 */
	 case 0x58:
	 case 0x5c:
	 case 0x38:   /* 1024x768 */
	 case 0x4a:
	 case 0x64:
	    yindex301B = (pSiS->VBFlags & TV_NTSC) ? 3 : 7;
	    break;
	 }
         if(pSiS->VBFlags & VB_301) {
            if(yindex301 >= 0) {
	       for(i=0, j=0x35; i<=3; i++, j++) {
	          outSISIDXREG(SISPART2,j,(SiSTVFilter301[yindex301].filter[pSiS->sistvyfilter-2][i]));
	       }
	    }
         } else {
            if(yindex301B >= 0) {
	       for(i=0, j=0x35; i<=3; i++, j++) {
	          outSISIDXREG(SISPART2,j,(SiSTVFilter301B[yindex301B].filter[pSiS->sistvyfilter-2][i]));
	       }
	       for(i=4, j=0x48; i<=6; i++, j++) {
	          outSISIDXREG(SISPART2,j,(SiSTVFilter301B[yindex301B].filter[pSiS->sistvyfilter-2][i]));
	       }
	    }
         }
         orSISIDXREG(SISPART2,0x30,0x20);
      }
   }
}

int SiS_GetSISTVyfilter(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
      return (int)pSiSEnt->sistvyfilter;
   else
#endif
      return (int)pSiS->sistvyfilter;
}

void SiS_SetSIS6326TVantiflicker(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;

   pSiS->sistvantiflicker = val;

   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) return;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
  
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) return;
   
   /* Valid values: 0=off, 1=low, 2=med, 3=high, 4=adaptive */
   if(val >= 0 && val <= 4) {
      tmp &= 0x1f;
      tmp |= (val << 5);
      SiS6326SetTVReg(pScrn,0x00,tmp);
   }
}

int SiS_GetSIS6326TVantiflicker(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;
   
   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) {
      return (int)pSiS->sistvantiflicker;
   }
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) {
      return (int)pSiS->sistvantiflicker;
   } else {
      return (int)((tmp >> 5) & 0x07);    
   }
}

void SiS_SetSIS6326TVenableyfilter(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;

   if(val) val = 1;   
   pSiS->sis6326enableyfilter = val;

   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
  
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) return;
   
   tmp = SiS6326GetTVReg(pScrn,0x43);
   tmp &= ~0x10;
   tmp |= ((val & 0x01) << 4);
   SiS6326SetTVReg(pScrn,0x43,tmp);
}

int SiS_GetSIS6326TVenableyfilter(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;
   
   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) {
      return (int)pSiS->sis6326enableyfilter;
   }
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) {
      return (int)pSiS->sis6326enableyfilter;
   } else {
      tmp = SiS6326GetTVReg(pScrn,0x43);
      return (int)((tmp >> 4) & 0x01);
   }
}

void SiS_SetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;
   
   if(val) val = 1;
   pSiS->sis6326yfilterstrong = val;

   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
  
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) return;
   
   tmp = SiS6326GetTVReg(pScrn,0x43);
   if(tmp & 0x10) {
      tmp &= ~0x40;
      tmp |= ((val & 0x01) << 6);
      SiS6326SetTVReg(pScrn,0x43,tmp);
   }
}

int SiS_GetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;
   
   if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) {
      return (int)pSiS->sis6326yfilterstrong;
   }
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) {
      return (int)pSiS->sis6326yfilterstrong;
   } else {
      tmp = SiS6326GetTVReg(pScrn,0x43);
      if(!(tmp & 0x10)) {
         return (int)pSiS->sis6326yfilterstrong;
      } else {
         return (int)((tmp >> 6) & 0x01);
      }
   }
}
   
void SiS_SetTVxposoffset(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   pSiS->tvxpos = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->tvxpos = val;
#endif

   if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

      if(pSiS->VBFlags & CRT2_TV) {

         if(pSiS->VBFlags & VB_CHRONTEL) {

	    int x = pSiS->tvx;
#ifdef SISDUALHEAD
	    if(pSiSEnt && pSiS->DualHeadMode) x = pSiSEnt->tvx;
#endif
	    switch(pSiS->ChrontelType) {
	    case CHRONTEL_700x:
	       if((val >= -32) && (val <= 32)) {
		   x += val;
		   if(x < 0) x = 0;
		   SiS_SetCH700x(pSiS->SiS_Pr, (((x & 0xff) << 8) | 0x0a));
		   SiS_SetCH70xxANDOR(pSiS->SiS_Pr, (((x & 0x0100) << 1) | 0x08),0xFD);
	       }
	       break;
	    case CHRONTEL_701x:
	       /* Not supported by hardware */
	       break;
	    }

	 } else if(pSiS->VBFlags & VB_SISBRIDGE) {

	    if((val >= -32) && (val <= 32)) {

	        unsigned char p2_1f,p2_20,p2_2b,p2_42,p2_43;
		unsigned short temp;

		p2_1f = pSiS->p2_1f;
		p2_20 = pSiS->p2_20;
		p2_2b = pSiS->p2_2b;
		p2_42 = pSiS->p2_42;
		p2_43 = pSiS->p2_43;
#ifdef SISDUALHEAD
	        if(pSiSEnt && pSiS->DualHeadMode) {
		   p2_1f = pSiSEnt->p2_1f;
		   p2_20 = pSiSEnt->p2_20;
		   p2_2b = pSiSEnt->p2_2b;
		   p2_42 = pSiSEnt->p2_42;
		   p2_43 = pSiSEnt->p2_43;
		}
#endif

		temp = p2_1f | ((p2_20 & 0xf0) << 4);
		temp += (val * 2);
		p2_1f = temp & 0xff;
		p2_20 = (temp & 0xf00) >> 4;
		p2_2b = ((p2_2b & 0x0f) + (val * 2)) & 0x0f;
		temp = p2_43 | ((p2_42 & 0xf0) << 4);
		temp += (val * 2);
		p2_43 = temp & 0xff;
		p2_42 = (temp & 0xf00) >> 4;
		SISWaitRetraceCRT2(pScrn);
	        outSISIDXREG(SISPART2,0x1f,p2_1f);
		setSISIDXREG(SISPART2,0x20,0x0F,p2_20);
		setSISIDXREG(SISPART2,0x2b,0xF0,p2_2b);
		setSISIDXREG(SISPART2,0x42,0x0F,p2_42);
		outSISIDXREG(SISPART2,0x43,p2_43);
	     }
	 }
      }

   } else if(pSiS->Chipset == PCI_CHIP_SIS6326) {

      if(pSiS->SiS6326Flags & SIS6326_TVDETECTED) {

         unsigned char tmp;
	 unsigned short temp1, temp2, temp3;

         tmp = SiS6326GetTVReg(pScrn,0x00);
         if(tmp & 0x04) {

	    temp1 = pSiS->tvx1;
            temp2 = pSiS->tvx2;
            temp3 = pSiS->tvx3;
            if((val >= -16) && (val <= 16)) {
	       if(val > 0) {
	          temp1 += (val * 4);
	          temp2 += (val * 4);
	          while((temp1 > 0x0fff) || (temp2 > 0x0fff)) {
	             temp1 -= 4;
		     temp2 -= 4;
	          }
	       } else {
	          val = -val;
	          temp3 += (val * 4);
	          while(temp3 > 0x03ff) {
	     	     temp3 -= 4;
	          }
	       }
            }
            SiS6326SetTVReg(pScrn,0x3a,(temp1 & 0xff));
            tmp = SiS6326GetTVReg(pScrn,0x3c);
            tmp &= 0xf0;
            tmp |= ((temp1 & 0x0f00) >> 8);
            SiS6326SetTVReg(pScrn,0x3c,tmp);
            SiS6326SetTVReg(pScrn,0x26,(temp2 & 0xff));
            tmp = SiS6326GetTVReg(pScrn,0x27);
            tmp &= 0x0f;
            tmp |= ((temp2 & 0x0f00) >> 4);
            SiS6326SetTVReg(pScrn,0x27,tmp);
            SiS6326SetTVReg(pScrn,0x12,(temp3 & 0xff));
            tmp = SiS6326GetTVReg(pScrn,0x13);
            tmp &= ~0xC0;
            tmp |= ((temp3 & 0x0300) >> 2);
            SiS6326SetTVReg(pScrn,0x13,tmp);
	 }
      }
   }
}

int SiS_GetTVxposoffset(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
        return (int)pSiSEnt->tvxpos;
   else
#endif
        return (int)pSiS->tvxpos;
}

void SiS_SetTVyposoffset(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   pSiS->tvypos = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->tvypos = val;
#endif

   if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

      if(pSiS->VBFlags & CRT2_TV) {

         if(pSiS->VBFlags & VB_CHRONTEL) {

	    int y = pSiS->tvy;
#ifdef SISDUALHEAD
	    if(pSiSEnt && pSiS->DualHeadMode) y = pSiSEnt->tvy;
#endif
	    switch(pSiS->ChrontelType) {
	    case CHRONTEL_700x:
	       if((val >= -32) && (val <= 32)) {
		   y -= val;
		   if(y < 0) y = 0;
		   SiS_SetCH700x(pSiS->SiS_Pr, (((y & 0xff) << 8) | 0x0b));
		   SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((y & 0x0100) | 0x08),0xFE);
	       }
	       break;
	    case CHRONTEL_701x:
	       /* Not supported by hardware */
	       break;
	    }

	 } else if(pSiS->VBFlags & VB_SISBRIDGE) {

	    if((val >= -32) && (val <= 32)) {
		char p2_01, p2_02;
		val /= 2;  /* 4 */
		p2_01 = pSiS->p2_01;
		p2_02 = pSiS->p2_02;
#ifdef SISDUALHEAD
	        if(pSiSEnt && pSiS->DualHeadMode) {
		   p2_01 = pSiSEnt->p2_01;
		   p2_02 = pSiSEnt->p2_02;
		}
#endif
		p2_01 += val; /* val * 2 */
		p2_02 += val; /* val * 2 */
		while((p2_01 <= 0) || (p2_02 <= 0)) {
		      p2_01 += 2;
		      p2_02 += 2;
		}
		SISWaitRetraceCRT2(pScrn);
		outSISIDXREG(SISPART2,0x01,p2_01);
		outSISIDXREG(SISPART2,0x02,p2_02);
	     }
	 }

      }

   } else if(pSiS->Chipset == PCI_CHIP_SIS6326) {

      if(pSiS->SiS6326Flags & SIS6326_TVDETECTED) {

         unsigned char tmp;
	 int temp1, limit;

         tmp = SiS6326GetTVReg(pScrn,0x00);
         if(tmp & 0x04) {

	    if((val >= -16) && (val <= 16)) {
	      temp1 = (unsigned short)pSiS->tvy1;
	      limit = (pSiS->SiS6326Flags & SIS6326_TVPAL) ? 625 : 525;
	      if(val > 0) {
                temp1 += (val * 4);
	        if(temp1 > limit) temp1 -= limit;
	      } else {
	        val = -val;
	        temp1 -= (val * 2);
	        if(temp1 <= 0) temp1 += (limit -1);
	      }
	      SiS6326SetTVReg(pScrn,0x11,(temp1 & 0xff));
	      tmp = SiS6326GetTVReg(pScrn,0x13);
	      tmp &= ~0x30;
	      tmp |= ((temp1 & 0x300) >> 4);
	      SiS6326SetTVReg(pScrn,0x13,tmp);
	      if(temp1 == 1)                                 tmp = 0x10;
	      else {
	       if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
	         if((temp1 <= 3) || (temp1 >= (limit - 2)))  tmp = 0x08;
	         else if(temp1 < 22)		 	     tmp = 0x02;
	         else 					     tmp = 0x04;
	       } else {
	         if((temp1 <= 5) || (temp1 >= (limit - 4)))  tmp = 0x08;
	         else if(temp1 < 19)			     tmp = 0x02;
	         else 					     tmp = 0x04;
	       }
	     }
	     SiS6326SetTVReg(pScrn,0x21,tmp);
           }
	 }
      }
   }
}

int SiS_GetTVyposoffset(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
        return (int)pSiSEnt->tvypos;
   else
#endif
        return (int)pSiS->tvypos;
}

void SiS_SetTVxscale(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   pSiS->tvxscale = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->tvxscale = val;
#endif

   if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

      if((pSiS->VBFlags & CRT2_TV) && (pSiS->VBFlags & VB_SISBRIDGE)) {

	 if((val >= -16) && (val <= 16)) {

	    unsigned char p2_44,p2_45,p2_46;
	    int scalingfactor;

	    p2_44 = pSiS->p2_44;
	    p2_45 = pSiS->p2_45 & 0x3f;
	    p2_46 = pSiS->p2_46 & 0x07;
#ifdef SISDUALHEAD
	    if(pSiSEnt && pSiS->DualHeadMode) {
	       p2_44 = pSiSEnt->p2_44;
	       p2_45 = pSiSEnt->p2_45 & 0x3f;
	       p2_46 = pSiSEnt->p2_46 & 0x07;
	    }
#endif
	    scalingfactor = (p2_46 << 13) | ((p2_45 & 0x1f) << 8) | p2_44;

	    if(val < 0) {
	       p2_45 &= 0xdf;
	       scalingfactor += ((-val) * 64);
	       if(scalingfactor > 0xffff) scalingfactor = 0xffff;
	    } else if(val > 0) {
	       p2_45 &= 0xdf;
	       scalingfactor -= (val * 64);
	       if(scalingfactor < 1) scalingfactor = 1;
	    }

	    p2_44 = scalingfactor & 0xff;
	    p2_45 &= 0xe0;
	    p2_45 |= ((scalingfactor >> 8) & 0x1f);
	    p2_46 = ((scalingfactor >> 13) & 0x07);

	    SISWaitRetraceCRT2(pScrn);
	    outSISIDXREG(SISPART2,0x44,p2_44);
	    setSISIDXREG(SISPART2,0x45,0xC0,p2_45);
	    if(!(pSiS->VBFlags & VB_301)) {
	       setSISIDXREG(SISPART2,0x46,0xF8,p2_46);
	    }
	 }

      }

   }
}

int SiS_GetTVxscale(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
        return (int)pSiSEnt->tvxscale;
   else
#endif
        return (int)pSiS->tvxscale;
}

void SiS_SetTVyscale(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
   Bool usentsc = FALSE;
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   if(val < -4) val = -4;
   if(val > 3)  val = 3;

   pSiS->tvyscale = val;
#ifdef SISDUALHEAD
   if(pSiSEnt) pSiSEnt->tvyscale = val;
#endif

   if(pSiS->VGAEngine == SIS_315_VGA || pSiS->VGAEngine == SIS_315_VGA) {

      if((pSiS->VBFlags & CRT2_TV) && (pSiS->VBFlags & VB_SISBRIDGE)) {

	 int srindex = -1, newvde, i = 0, j, vlimit, temp;
	 unsigned char p3d4_34;

	 if(pSiS->VBFlags & TV_HIVISION) return;
         if((pSiS->VBFlags & TV_YPBPR) &&
            (pSiS->VBFlags & (TV_YPBPR1080I | TV_YPBPR750P | TV_YPBPR525P))) return;

	 if(pSiS->VBFlags & TV_YPBPR)                 usentsc = TRUE;
         else if(pSiS->VBFlags & (TV_NTSC | TV_PALM)) usentsc = TRUE;

	 vlimit = usentsc ? 259 : 309;

	 inSISIDXREG(SISCR,0x34,p3d4_34);

	 switch((p3d4_34 & 0x7f)) {
#if 0
	 case 0x50:   /* 320x240 - hdclk mode */
	 case 0x56:
	 case 0x53:
#endif
	 case 0x2e:   /* 640x480 */
	 case 0x44:
	 case 0x62:
	    srindex  = usentsc ? 0 : 21;
	    break;
	 case 0x31:   /* 720x480 */
	 case 0x33:
	 case 0x35:
	 case 0x32:   /* 720x576 */
	 case 0x34:
	 case 0x36:
	 case 0x5f:   /* 768x576 */
	 case 0x60:
	 case 0x61:
            if(pSiS->VGAEngine == SIS_315_VGA) {
	       srindex  = usentsc ? 7 : 28;
	    }
	    break;
#if 0
	 case 0x51:   /* 400x300 - hdclk mode */
	 case 0x57:
	 case 0x54:
#endif
	 case 0x30:   /* 800x600 */
	 case 0x47:
	 case 0x63:
	    srindex  = usentsc ? 14 : 35;
	 }

	 if(srindex >= 0) {
	    Bool found = FALSE;
	    if(pSiS->tvyscale != 0) {
	       for(j=0; j<=1; j++) {
	          for(i=0; i<=6; i++) {
	             if(SiSTVVScale[srindex+i].sindex == pSiS->tvyscale) {
		        found = TRUE;
		        break;
		     }
	          }
		  if(found) break;
	          if(pSiS->tvyscale > 0) pSiS->tvyscale--;
		  else pSiS->tvyscale++;
	       }
	    }
#ifdef SISDUALHEAD
	    if(pSiSEnt) pSiSEnt->tvyscale = pSiS->tvyscale;
#endif
	    if(pSiS->tvyscale == 0) {
	       unsigned char p2_0a = pSiS->p2_0a;
	       unsigned char p2_2f = pSiS->p2_2f;
	       unsigned char p2_30 = pSiS->p2_30;
	       unsigned char p2_46 = pSiS->p2_46;
	       unsigned char p2_47 = pSiS->p2_47;
	       unsigned char p1scaling[9], p4scaling[9];
	       unsigned char *p2scaling;

	       for(i=0; i<9; i++) {
	          p1scaling[i] = pSiS->scalingp1[i];
	       }
	       for(i=0; i<9; i++) {
	          p4scaling[i] = pSiS->scalingp4[i];
	       }
	       p2scaling = &pSiS->scalingp2[0];
#ifdef SISDUALHEAD
               if(pSiSEnt && pSiS->DualHeadMode) {
	          p2_0a = pSiSEnt->p2_0a;
		  p2_2f = pSiSEnt->p2_2f;
	          p2_30 = pSiSEnt->p2_30;
		  p2_46 = pSiSEnt->p2_46;
		  p2_47 = pSiSEnt->p2_47;
	          for(i=0; i<9; i++) {
	             p1scaling[i] = pSiSEnt->scalingp1[i];
	          }
	          for(i=0; i<9; i++) {
	             p4scaling[i] = pSiSEnt->scalingp4[i];
	          }
		  p2scaling = &pSiSEnt->scalingp2[0];
	       }
#endif
               SISWaitRetraceCRT2(pScrn);
	       if(pSiS->VBFlags & (VB_301C|VB_302ELV)) {
	          for(i=0; i<64; i++) {
	             outSISIDXREG(SISPART2,(0xc0 + i),p2scaling[i]);
	          }
	       }
	       for(i=0; i<9; i++) {
	          outSISIDXREG(SISPART1,SiSScalingP1Regs[i],p1scaling[i]);
	       }
	       for(i=0; i<9; i++) {
	          outSISIDXREG(SISPART4,SiSScalingP4Regs[i],p4scaling[i]);
	       }

	       setSISIDXREG(SISPART2,0x0a,0x7f,(p2_0a & 0x80));
	       outSISIDXREG(SISPART2,0x2f,p2_2f);
	       setSISIDXREG(SISPART2,0x30,0x3f,(p2_30 & 0xc0));
	       if(!(pSiS->VBFlags & VB_301)) {
	          setSISIDXREG(SISPART2,0x46,0x9f,(p2_46 & 0x60));
		  outSISIDXREG(SISPART2,0x47,p2_47);
	       }

	    } else {

	       int so = (pSiS->VGAEngine == SIS_300_VGA) ? 12 : 0;
	       int realvde, j, srindex301c, myypos, watchdog = 32;
	       unsigned long calctemp;

	       srindex += i;
	       srindex301c = srindex * 64;
	       newvde = SiSTVVScale[srindex].ScaleVDE;
	       realvde = SiSTVVScale[srindex].RealVDE;

	       do {
	          inSISIDXREG(SISPART2,0x01,temp);
	          temp = vlimit - (temp & 0x7f);
	          if((temp - (((newvde >> 1) - 2) + 9)) > 0) break;
		  myypos = pSiS->tvypos - 1;
#ifdef SISDUALHEAD
		  if(pSiSEnt && pSiS->DualHeadMode) myypos = pSiSEnt->tvypos - 1;
#endif
		  SiS_SetTVyposoffset(pScrn, myypos);
	       } while(watchdog--);

	       SISWaitRetraceCRT2(pScrn);

	       if(pSiS->VBFlags & (VB_301C|VB_302ELV)) {
#ifdef TWDEBUG
		  xf86DrvMsg(0, X_INFO, "301C scaler: Table index %d\n");
#endif
	          for(j=0; j<64; j++) {
		     outSISIDXREG(SISPART2,(0xc0 + j), SiS301CScaling[srindex301c + j]);
		  }
	       }

	       if(!(pSiS->VBFlags & VB_301)) {
	          temp = (newvde >> 1) - 3;
	          setSISIDXREG(SISPART2,0x46,0x9f,((temp & 0x0300) >> 3));
	          outSISIDXREG(SISPART2,0x47,(temp & 0xff));
	       }
	       outSISIDXREG(SISPART1,0x08,(SiSTVVScale[srindex].reg[so+0] & 0xff));
	       setSISIDXREG(SISPART1,0x09,0x0f,((SiSTVVScale[srindex].reg[so+0] >> 4) & 0xf0));
	       outSISIDXREG(SISPART1,0x0b,(SiSTVVScale[srindex].reg[so+1] & 0xff));
	       setSISIDXREG(SISPART1,0x0c,0xf0,((SiSTVVScale[srindex].reg[so+1] >> 8) & 0x0f));
	       outSISIDXREG(SISPART1,0x0d,(SiSTVVScale[srindex].reg[so+2] & 0xff));
	       outSISIDXREG(SISPART1,0x0e,(SiSTVVScale[srindex].reg[so+3] & 0xff));
	       setSISIDXREG(SISPART1,0x12,0xf8,((SiSTVVScale[srindex].reg[so+3] >> 8 ) & 0x07));
	       outSISIDXREG(SISPART1,0x10,(SiSTVVScale[srindex].reg[so+4] & 0xff));
	       setSISIDXREG(SISPART1,0x11,0x8f,((SiSTVVScale[srindex].reg[so+4] >> 4) & 0x70));
	       setSISIDXREG(SISPART1,0x11,0xf0,(SiSTVVScale[srindex].reg[so+5] & 0x0f));

	       setSISIDXREG(SISPART2,0x0a,0x7f,((SiSTVVScale[srindex].reg[so+6] << 7) & 0x80));
	       outSISIDXREG(SISPART2,0x2f,((newvde / 2) - 2));
	       setSISIDXREG(SISPART2,0x30,0x3f,((((newvde / 2) - 2) >> 2) & 0xc0));

	       outSISIDXREG(SISPART4,0x13,(SiSTVVScale[srindex].reg[so+7] & 0xff));
	       outSISIDXREG(SISPART4,0x14,(SiSTVVScale[srindex].reg[so+8] & 0xff));
	       setSISIDXREG(SISPART4,0x15,0x7f,((SiSTVVScale[srindex].reg[so+8] >> 1) & 0x80));

	       outSISIDXREG(SISPART4,0x16,(SiSTVVScale[srindex].reg[so+9] & 0xff));
	       setSISIDXREG(SISPART4,0x15,0x87,((SiSTVVScale[srindex].reg[so+9] >> 5) & 0x78));

	       outSISIDXREG(SISPART4,0x17,(SiSTVVScale[srindex].reg[so+10] & 0xff));
	       setSISIDXREG(SISPART4,0x15,0xf8,((SiSTVVScale[srindex].reg[so+10] >> 8) & 0x07));

	       outSISIDXREG(SISPART4,0x18,(SiSTVVScale[srindex].reg[so+11] & 0xff));
	       setSISIDXREG(SISPART4,0x19,0xf0,((SiSTVVScale[srindex].reg[so+11] >> 8) & 0x0f));

               temp = 0x40;
	       if(realvde <= newvde) temp = 0;
	       else realvde -= newvde;

	       calctemp = (realvde * 256 * 1024) / newvde;
	       if((realvde * 256 * 1024) % newvde) calctemp++;
	       outSISIDXREG(SISPART4,0x1b,(calctemp & 0xff));
	       outSISIDXREG(SISPART4,0x1a,((calctemp >> 8) & 0xff));
	       setSISIDXREG(SISPART4,0x19,0x8f,(((calctemp >> 12) & 0x30) | temp));
	    }
	 }

      }

   }
}

int SiS_GetTVyscale(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   if(pSiSEnt && pSiS->DualHeadMode)
        return (int)pSiSEnt->tvyscale;
   else
#endif
        return (int)pSiS->tvyscale;
}

/* PostSetMode:
 * -) Disable CRT1 for saving bandwidth. This doesn't work with VESA;
 *    VESA uses the bridge in SlaveMode and switching CRT1 off while
 *    the bridge is in SlaveMode not that clever...
 * -) Check if overlay can be used (depending on dotclock)
 * -) Check if Panel Scaler is active on LVDS for overlay re-scaling
 * -) Save TV registers for further processing
 * -) Apply TV settings
 */
static void
SiSPostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
    unsigned char usScratchCR17;
    Bool flag = FALSE;
    Bool doit = TRUE;
    int myclock, temp;
    unsigned char  sr2b, sr2c, tmpreg;
    float          num, denum, postscalar, divider;

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	"CRT1off is %d\n", pSiS->CRT1off);
#endif
    pSiS->CRT1isoff = pSiS->CRT1off;

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if(pSiS->sishw_ext.jChipType >= SIS_661) {
       inSISIDXREG(SISSR,0x11,tmpreg);
       if(tmpreg & 0x20) {
          inSISIDXREG(SISSR,0x3e,tmpreg);
	  tmpreg = (tmpreg + 1) & 0xff;
	  outSISIDXREG(SISSR,0x3e,tmpreg);
       }
    }

    if((!pSiS->UseVESA) && (pSiS->VBFlags & CRT2_ENABLE)) {

	if(pSiS->VBFlags != pSiS->VBFlags_backup) {
	   pSiS->VBFlags = pSiS->VBFlags_backup;
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"VBFlags restored to %0lx\n", pSiS->VBFlags);
	}

	/* -) We can't switch off CRT1 if bridge is in SlaveMode.
	 * -) If we change to a SlaveMode-Mode (like 512x384), we
	 *    need to adapt VBFlags for eg. Xv.
	 */
#ifdef SISDUALHEAD
	if(!pSiS->DualHeadMode) {
#endif
	   if(SiSBridgeIsInSlaveMode(pScrn)) {
	      doit = FALSE;
	      temp = pSiS->VBFlags;
	      pSiS->VBFlags &= (~VB_DISPMODE_SINGLE);
	      pSiS->VBFlags |= (VB_DISPMODE_MIRROR | DISPTYPE_DISP1);
              if(temp != pSiS->VBFlags) {
		 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 	"VBFlags changed to 0x%0lx\n", pSiS->VBFlags);
	      }
	   }
#ifdef SISDUALHEAD
	}
#endif

	if(pSiS->VGAEngine == SIS_315_VGA) {

	   if((pSiS->CRT1off) && (doit)) {
	      orSISIDXREG(SISCR,pSiS->myCR63,0x40);
	      orSISIDXREG(SISSR,0x1f,0xc0);
	   } else {
	      andSISIDXREG(SISCR,pSiS->myCR63,0xBF);
	      andSISIDXREG(SISSR,0x1f,0x3f);
	   }

	} else {

	   if(doit) {
              inSISIDXREG(SISCR, 0x17, usScratchCR17);
    	      if(pSiS->CRT1off) {
	         if(usScratchCR17 & 0x80) {
		    flag = TRUE;
		    usScratchCR17 &= ~0x80;
		 }
		 orSISIDXREG(SISSR,0x1f,0xc0);
    	      } else {
	         if(!(usScratchCR17 & 0x80)) {
		    flag = TRUE;
        	    usScratchCR17 |= 0x80;
		 }
		 andSISIDXREG(SISSR,0x1f,0x3f);
              }
	      /* Reset only if status changed */
	      if(flag) {
	         outSISIDXREG(SISCR, 0x17, usScratchCR17);
	         outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
	         usleep(10000);
                 outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */
	      }
	   }
	}

    }

    /* Determine if the video overlay can be used */
    if(!pSiS->NoXvideo) {
       inSISIDXREG(SISSR,0x2b,sr2b);
       inSISIDXREG(SISSR,0x2c,sr2c);
       divider = (sr2b & 0x80) ? 2.0 : 1.0;
       postscalar = (sr2c & 0x80) ?
              ( (((sr2c >> 5) & 0x03) == 0x02) ? 6.0 : 8.0 ) :
	      ( ((sr2c >> 5) & 0x03) + 1.0 );
       num = (sr2b & 0x7f) + 1.0;
       denum = (sr2c & 0x1f) + 1.0;
       myclock = (int)((14318 * (divider / postscalar) * (num / denum)) / 1000);

       pSiS->MiscFlags &= ~(MISC_CRT1OVERLAY | MISC_CRT1OVERLAYGAMMA);
       switch(pSiS->sishw_ext.jChipType) {
         case SIS_300:
         case SIS_540:
         case SIS_630:
         case SIS_730:
            if(myclock < 150) {
               pSiS->MiscFlags |= MISC_CRT1OVERLAY;
            }
            break;
         case SIS_550:
         case SIS_650:
         case SIS_740:
	    if(myclock < 175) {
               pSiS->MiscFlags |= MISC_CRT1OVERLAY;
	       if(myclock < 166) {
	          pSiS->MiscFlags |= MISC_CRT1OVERLAYGAMMA;
	       }
            }
            break;
	 case SIS_315H:
         case SIS_315:
         case SIS_315PRO:
         case SIS_330:
	 case SIS_661:
	 case SIS_741:
	 case SIS_660:
	 case SIS_760:
            if(myclock < 180) {
               pSiS->MiscFlags |= MISC_CRT1OVERLAY;
	       if(myclock < 166) {
	          pSiS->MiscFlags |= MISC_CRT1OVERLAYGAMMA;
	       }
            }
            break;
       }
       if(!(pSiS->MiscFlags & MISC_CRT1OVERLAY)) {
#ifdef SISDUALHEAD
          if((!pSiS->DualHeadMode) || (pSiS->SecondHead))
#endif
             xf86DrvMsgVerb(pScrn->scrnIndex, X_WARNING, 3,
	     	"Current dotclock (%dMhz) too high for video overlay on CRT1\n",
		myclock);
       }
    }

    /* Determine if the Panel Link scaler is active */
    pSiS->MiscFlags &= ~MISC_PANELLINKSCALER;
    if(pSiS->VBFlags & (CRT2_LCD | CRT1_LCDA)) {
       if(pSiS->VGAEngine == SIS_300_VGA) {
          if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH)) {
	     inSISIDXREG(SISPART1,0x1e,tmpreg);
	     tmpreg &= 0x3f;
	     if(tmpreg) pSiS->MiscFlags |= MISC_PANELLINKSCALER;
	  }
       } else {
          if(pSiS->VBFlags & (VB_LVDS | VB_30xBDH | CRT1_LCDA)) {
	     inSISIDXREG(SISPART1,0x35,tmpreg);
	     tmpreg &= 0x04;
	     if(!tmpreg) pSiS->MiscFlags |= MISC_PANELLINKSCALER;
	  }
       }
    }

    /* Determine if our very special TV mode is active */
    pSiS->MiscFlags &= ~MISC_TVNTSC1024;
    if((pSiS->VBFlags & VB_SISBRIDGE) && (pSiS->VBFlags & CRT2_TV) && (!(pSiS->VBFlags & TV_HIVISION))) {
       if( ((pSiS->VBFlags & TV_YPBPR) && (pSiS->VBFlags & TV_YPBPR525I)) ||
           ((!(pSiS->VBFlags & TV_YPBPR)) && (pSiS->VBFlags & (TV_NTSC | TV_PALM))) ) {
          inSISIDXREG(SISCR,0x34,tmpreg);
	  tmpreg &= 0x7f;
	  if((tmpreg == 0x64) || (tmpreg == 0x4a) || (tmpreg == 0x38)) {
	     pSiS->MiscFlags |= MISC_TVNTSC1024;
	  }
       }
    }

#ifdef SISVRAMQ
    if(pSiS->VGAEngine == SIS_315_VGA) {
       int i;
       /* Re-Enable command queue */
       SiSEnableTurboQueue(pScrn);
       /* Get HWCursor register contents for backup */
       for(i = 0; i < 16; i++) {
          pSiS->HWCursorBackup[i] = MMIO_IN32(pSiS->IOBase, 0x8500 + (i << 2));
       }
       if(pSiS->sishw_ext.jChipType >= SIS_330) {
          /* Enable HWCursor protection (Y pos as trigger) */
          andSISIDXREG(SISCR, 0x5b, ~0x30);
       }
    }
#endif

    /* Reset XV gamma correction */
    if(pSiS->ResetXvGamma) {
       (pSiS->ResetXvGamma)(pScrn);
    }

    /*  Apply TV settings given by options
           Do this even in DualHeadMode:
	   - if this is called by SetModeCRT1, CRT2 mode has been reset by SetModeCRT1
	   - if this is called by SetModeCRT2, CRT2 mode has changed (duh!)
	   -> Hence, in both cases, the settings must be re-applied.
     */
    if(pSiS->VBFlags & CRT2_TV) {
       int val;
       if(pSiS->VBFlags & VB_CHRONTEL) {
          int mychtvlumabandwidthcvbs = pSiS->chtvlumabandwidthcvbs;
	  int mychtvlumabandwidthsvideo = pSiS->chtvlumabandwidthsvideo;
	  int mychtvlumaflickerfilter = pSiS->chtvlumaflickerfilter;
	  int mychtvchromabandwidth = pSiS->chtvchromabandwidth;
	  int mychtvchromaflickerfilter = pSiS->chtvchromaflickerfilter;
	  int mychtvcvbscolor = pSiS->chtvcvbscolor;
	  int mychtvtextenhance = pSiS->chtvtextenhance;
	  int mychtvcontrast = pSiS->chtvcontrast;
	  int mytvxpos = pSiS->tvxpos;
	  int mytvypos = pSiS->tvypos;
#ifdef SISDUALHEAD
	  if(pSiSEnt && pSiS->DualHeadMode) {
	     mychtvlumabandwidthcvbs = pSiSEnt->chtvlumabandwidthcvbs;
	     mychtvlumabandwidthsvideo = pSiSEnt->chtvlumabandwidthsvideo;
	     mychtvlumaflickerfilter = pSiSEnt->chtvlumaflickerfilter;
	     mychtvchromabandwidth = pSiSEnt->chtvchromabandwidth;
	     mychtvchromaflickerfilter = pSiSEnt->chtvchromaflickerfilter;
	     mychtvcvbscolor = pSiSEnt->chtvcvbscolor;
	     mychtvtextenhance = pSiSEnt->chtvtextenhance;
	     mychtvcontrast = pSiSEnt->chtvcontrast;
	     mytvxpos = pSiSEnt->tvxpos;
	     mytvypos = pSiSEnt->tvypos;
	  }
#endif	  
	  if((val = mychtvlumabandwidthcvbs) != -1) {
	     SiS_SetCHTVlumabandwidthcvbs(pScrn, val);
	  }
	  if((val = mychtvlumabandwidthsvideo) != -1) {
	     SiS_SetCHTVlumabandwidthsvideo(pScrn, val);
	  }
	  if((val = mychtvlumaflickerfilter) != -1) {
	     SiS_SetCHTVlumaflickerfilter(pScrn, val);
	  }
	  if((val = mychtvchromabandwidth) != -1) {
	     SiS_SetCHTVchromabandwidth(pScrn, val);      
	  }
	  if((val = mychtvchromaflickerfilter) != -1) {
	     SiS_SetCHTVchromaflickerfilter(pScrn, val);
	  }
	  if((val = mychtvcvbscolor) != -1) {
	     SiS_SetCHTVcvbscolor(pScrn, val);
	  }
	  if((val = mychtvtextenhance) != -1) {
	     SiS_SetCHTVtextenhance(pScrn, val);
	  }
	  if((val = mychtvcontrast) != -1) {
	     SiS_SetCHTVcontrast(pScrn, val);
	  }
	  /* Backup default TV position registers */
	  switch(pSiS->ChrontelType) {
	  case CHRONTEL_700x:
	     pSiS->tvx = SiS_GetCH700x(pSiS->SiS_Pr, 0x0a);
	     pSiS->tvx |= (((SiS_GetCH700x(pSiS->SiS_Pr, 0x08) & 0x02) >> 1) << 8);
	     pSiS->tvy = SiS_GetCH700x(pSiS->SiS_Pr, 0x0b);
	     pSiS->tvy |= ((SiS_GetCH700x(pSiS->SiS_Pr, 0x08) & 0x01) << 8);
#ifdef SISDUALHEAD
	     if(pSiSEnt) {
	        pSiSEnt->tvx = pSiS->tvx;
		pSiSEnt->tvy = pSiS->tvy;
	     }
#endif
	     break;
	  case CHRONTEL_701x:
	     /* Not supported by hardware */
	     break;
	  }
	  if((val = mytvxpos) != 0) {
	     SiS_SetTVxposoffset(pScrn, val);
	  }
	  if((val = mytvypos) != 0) {
	     SiS_SetTVyposoffset(pScrn, val);
	  }
       }
       if(pSiS->VBFlags & VB_301) {
          int mysistvedgeenhance = pSiS->sistvedgeenhance;
#ifdef SISDUALHEAD
          if(pSiSEnt && pSiS->DualHeadMode) {
	     mysistvedgeenhance = pSiSEnt->sistvedgeenhance;
	  }
#endif
          if((val = mysistvedgeenhance) != -1) {
	     SiS_SetSISTVedgeenhance(pScrn, val);
	  }
       }
       if(pSiS->VBFlags & VB_SISBRIDGE) {
          int mysistvantiflicker = pSiS->sistvantiflicker;
	  int mysistvsaturation = pSiS->sistvsaturation;
	  int mysistvcolcalibf = pSiS->sistvcolcalibf;
	  int mysistvcolcalibc = pSiS->sistvcolcalibc;
	  int mysistvcfilter = pSiS->sistvcfilter;
	  int mysistvyfilter = pSiS->sistvyfilter;
	  int mytvxpos = pSiS->tvxpos;
	  int mytvypos = pSiS->tvypos;
	  int mytvxscale = pSiS->tvxscale;
	  int mytvyscale = pSiS->tvyscale;
	  int i;
	  unsigned long cbase;
	  unsigned char ctemp;
#ifdef SISDUALHEAD
          if(pSiSEnt && pSiS->DualHeadMode) {
	     mysistvantiflicker = pSiSEnt->sistvantiflicker;
	     mysistvsaturation = pSiSEnt->sistvsaturation;
	     mysistvcolcalibf = pSiSEnt->sistvcolcalibf;
	     mysistvcolcalibc = pSiSEnt->sistvcolcalibc;
	     mysistvcfilter = pSiSEnt->sistvcfilter;
	     mysistvyfilter = pSiSEnt->sistvyfilter;
	     mytvxpos = pSiSEnt->tvxpos;
	     mytvypos = pSiSEnt->tvypos;
	     mytvxscale = pSiSEnt->tvxscale;
	     mytvyscale = pSiSEnt->tvyscale;
	  }
#endif
          /* Backup default TV position, scale and colcalib registers */
	  inSISIDXREG(SISPART2,0x1f,pSiS->p2_1f);
	  inSISIDXREG(SISPART2,0x20,pSiS->p2_20);
	  inSISIDXREG(SISPART2,0x2b,pSiS->p2_2b);
	  inSISIDXREG(SISPART2,0x42,pSiS->p2_42);
	  inSISIDXREG(SISPART2,0x43,pSiS->p2_43);
	  inSISIDXREG(SISPART2,0x01,pSiS->p2_01);
	  inSISIDXREG(SISPART2,0x02,pSiS->p2_02);
	  inSISIDXREG(SISPART2,0x44,pSiS->p2_44);
	  inSISIDXREG(SISPART2,0x45,pSiS->p2_45);
	  if(!(pSiS->VBFlags & VB_301)) {
	     inSISIDXREG(SISPART2,0x46,pSiS->p2_46);
	  } else {
	     pSiS->p2_46 = 0;
	  }
	  inSISIDXREG(SISPART2,0x0a,pSiS->p2_0a);
	  inSISIDXREG(SISPART2,0x31,cbase);
	  cbase = (cbase & 0x7f) << 8;
	  inSISIDXREG(SISPART2,0x32,ctemp);
	  cbase = (cbase | ctemp) << 8;
	  inSISIDXREG(SISPART2,0x33,ctemp);
	  cbase = (cbase | ctemp) << 8;
	  inSISIDXREG(SISPART2,0x34,ctemp);
	  pSiS->sistvccbase = (cbase | ctemp);
	  inSISIDXREG(SISPART2,0x35,pSiS->p2_35);
	  inSISIDXREG(SISPART2,0x36,pSiS->p2_36);
	  inSISIDXREG(SISPART2,0x37,pSiS->p2_37);
	  inSISIDXREG(SISPART2,0x38,pSiS->p2_38);
	  if(!(pSiS->VBFlags & VB_301)) {
	     inSISIDXREG(SISPART2,0x47,pSiS->p2_47);
	     inSISIDXREG(SISPART2,0x48,pSiS->p2_48);
	     inSISIDXREG(SISPART2,0x49,pSiS->p2_49);
	     inSISIDXREG(SISPART2,0x4a,pSiS->p2_4a);
	  }
	  inSISIDXREG(SISPART2,0x2f,pSiS->p2_2f);
	  inSISIDXREG(SISPART2,0x30,pSiS->p2_30);
	  for(i=0; i<9; i++) {
	     inSISIDXREG(SISPART1,SiSScalingP1Regs[i],pSiS->scalingp1[i]);
	  }
	  for(i=0; i<9; i++) {
	     inSISIDXREG(SISPART4,SiSScalingP4Regs[i],pSiS->scalingp4[i]);
	  }
	  if(pSiS->VBFlags & (VB_301C | VB_302ELV)) {
	     for(i=0; i<64; i++) {
	        inSISIDXREG(SISPART2,(0xc0 + i),pSiS->scalingp2[i]);
  	     }
	  }
#ifdef SISDUALHEAD
	  if(pSiSEnt) {
	     pSiSEnt->p2_1f = pSiS->p2_1f; pSiSEnt->p2_20 = pSiS->p2_20;
	     pSiSEnt->p2_42 = pSiS->p2_42; pSiSEnt->p2_43 = pSiS->p2_43;
	     pSiSEnt->p2_2b = pSiS->p2_2b;
	     pSiSEnt->p2_01 = pSiS->p2_01; pSiSEnt->p2_02 = pSiS->p2_02;
	     pSiSEnt->p2_44 = pSiS->p2_44; pSiSEnt->p2_45 = pSiS->p2_45;
	     pSiSEnt->p2_46 = pSiS->p2_46; pSiSEnt->p2_0a = pSiS->p2_0a;
	     pSiSEnt->sistvccbase = pSiS->sistvccbase;
	     pSiSEnt->p2_35 = pSiS->p2_35; pSiSEnt->p2_36 = pSiS->p2_36;
	     pSiSEnt->p2_37 = pSiS->p2_37; pSiSEnt->p2_38 = pSiS->p2_38;
	     pSiSEnt->p2_48 = pSiS->p2_48; pSiSEnt->p2_49 = pSiS->p2_49;
	     pSiSEnt->p2_4a = pSiS->p2_4a; pSiSEnt->p2_2f = pSiS->p2_2f;
	     pSiSEnt->p2_30 = pSiS->p2_30; pSiSEnt->p2_47 = pSiS->p2_47;
	     for(i=0; i<9; i++) {
	        pSiSEnt->scalingp1[i] = pSiS->scalingp1[i];
	     }
	     for(i=0; i<9; i++) {
	        pSiSEnt->scalingp4[i] = pSiS->scalingp4[i];
	     }
	     if(pSiS->VBFlags & (VB_301C | VB_302ELV)) {
	        for(i=0; i<64; i++) {
	           pSiSEnt->scalingp2[i] = pSiS->scalingp2[i];
  	        }
	     }
	  }
#endif
          if((val = mysistvantiflicker) != -1) {
	     SiS_SetSISTVantiflicker(pScrn, val);
	  }
	  if((val = mysistvsaturation) != -1) {
	     SiS_SetSISTVsaturation(pScrn, val);
	  }
	  if((val = mysistvcfilter) != -1) {
	     SiS_SetSISTVcfilter(pScrn, val);
	  }
	  if((val = mysistvyfilter) != 1) {
	     SiS_SetSISTVyfilter(pScrn, val);
	  }
	  if((val = mysistvcolcalibc) != 0) {
	     SiS_SetSISTVcolcalib(pScrn, val, TRUE);
	  }
	  if((val = mysistvcolcalibf) != 0) {
	     SiS_SetSISTVcolcalib(pScrn, val, FALSE);
	  }
	  if((val = mytvxpos) != 0) {
	     SiS_SetTVxposoffset(pScrn, val);
	  }
	  if((val = mytvypos) != 0) {
	     SiS_SetTVyposoffset(pScrn, val);
	  }
	  if((val = mytvxscale) != 0) {
	     SiS_SetTVxscale(pScrn, val);
	  }
	  if((val = mytvyscale) != 0) {
	     SiS_SetTVyscale(pScrn, val);
	  }
       }
    }

}

/* Post-set SiS6326 TV registers */
static void
SiS6326PostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char tmp;
    int val;

    if(!(pSiS->SiS6326Flags & SIS6326_TVDETECTED)) return;

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Backup default TV position registers */
    pSiS->tvx1 = SiS6326GetTVReg(pScrn,0x3a);
    pSiS->tvx1 |= ((SiS6326GetTVReg(pScrn,0x3c) & 0x0f) << 8);
    pSiS->tvx2 = SiS6326GetTVReg(pScrn,0x26);
    pSiS->tvx2 |= ((SiS6326GetTVReg(pScrn,0x27) & 0xf0) << 4);
    pSiS->tvx3 = SiS6326GetTVReg(pScrn,0x12);
    pSiS->tvx3 |= ((SiS6326GetTVReg(pScrn,0x13) & 0xC0) << 2);
    pSiS->tvy1 = SiS6326GetTVReg(pScrn,0x11);
    pSiS->tvy1 |= ((SiS6326GetTVReg(pScrn,0x13) & 0x30) << 4);
    
    /* Handle TVPosOffset options (BEFORE switching on TV) */
    if((val = pSiS->tvxpos) != 0) {
       SiS_SetTVxposoffset(pScrn, val);
    }
    if((val = pSiS->tvypos) != 0) {
       SiS_SetTVyposoffset(pScrn, val);
    }

    /* Switch on TV output. This is rather complicated, but
     * if we don't do it, TV output will flicker terribly.
     */
    if(pSiS->SiS6326Flags & SIS6326_TVON) {
       orSISIDXREG(SISSR, 0x01, 0x20);
       tmp = SiS6326GetTVReg(pScrn,0x00);
       tmp &= ~0x04;
       while(!(inSISREG(SISINPSTAT) & 0x08));    /* Wait while NOT vb */
       SiS6326SetTVReg(pScrn,0x00,tmp);
       for(val=0; val < 2; val++) {
         while(!(inSISREG(SISINPSTAT) & 0x08));  /* Wait while NOT vb */
         while(inSISREG(SISINPSTAT) & 0x08);     /* wait while vb     */
       }
       SiS6326SetTVReg(pScrn, 0x00, sisReg->sis6326tv[0]);
       tmp = inSISREG(SISINPSTAT);
       outSISREG(SISAR, 0x20);
       tmp = inSISREG(SISINPSTAT);
       while(inSISREG(SISINPSTAT) & 0x01);
       while(!(inSISREG(SISINPSTAT) & 0x01));
       andSISIDXREG(SISSR, 0x01, ~0x20);
       for(val=0; val < 10; val++) {
         while(!(inSISREG(SISINPSTAT) & 0x08));  /* Wait while NOT vb */
         while(inSISREG(SISINPSTAT) & 0x08);     /* wait while vb     */
       }
       andSISIDXREG(SISSR, 0x01, ~0x20);
    }

    tmp = SiS6326GetTVReg(pScrn,0x00);
    if(!(tmp & 0x04)) return;

    /* Apply TV settings given by options */
    if((val = pSiS->sistvantiflicker) != -1) {
       SiS_SetSIS6326TVantiflicker(pScrn, val);
    }
    if((val = pSiS->sis6326enableyfilter) != -1) {
       SiS_SetSIS6326TVenableyfilter(pScrn, val);
    }
    if((val = pSiS->sis6326yfilterstrong) != -1) {
       SiS_SetSIS6326TVyfilterstrong(pScrn, val);
    }

}

/* Check if video bridge is in slave mode */
BOOLEAN
SiSBridgeIsInSlaveMode(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char usScratchP1_00;

    if(!(pSiS->VBFlags & VB_VIDEOBRIDGE)) return FALSE;

    inSISIDXREG(SISPART1,0x00,usScratchP1_00);
    if( ((pSiS->VGAEngine == SIS_300_VGA) && (usScratchP1_00 & 0xa0) == 0x20) ||
        ((pSiS->VGAEngine == SIS_315_VGA) && (usScratchP1_00 & 0x50) == 0x10) ) {
	   return TRUE;
    } else {
           return FALSE;
    }
}

/* Build a list of the VESA modes the BIOS reports as valid */
static void
SiSBuildVesaModeList(ScrnInfoPtr pScrn, vbeInfoPtr pVbe, VbeInfoBlock *vbe)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i = 0;

    while(vbe->VideoModePtr[i] != 0xffff) {
	sisModeInfoPtr m;
	VbeModeInfoBlock *mode;
	int id = vbe->VideoModePtr[i++];
	int bpp;

	if((mode = VBEGetModeInfo(pVbe, id)) == NULL)
	    continue;

	bpp = mode->BitsPerPixel;

	m = xnfcalloc(sizeof(sisModeInfoRec),1);
	m->width = mode->XResolution;
	m->height = mode->YResolution;
	m->bpp = bpp;
	m->n = id;
	m->next = pSiS->SISVESAModeList;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      "BIOS supports VESA mode 0x%x: x:%i y:%i bpp:%i\n",
	       m->n, m->width, m->height, m->bpp);

	pSiS->SISVESAModeList = m;

	VBEFreeModeInfo(mode);
    }
}

/* Calc VESA mode from given resolution/depth */
static UShort
SiSCalcVESAModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    SISPtr pSiS = SISPTR(pScrn);
    sisModeInfoPtr m = pSiS->SISVESAModeList;
    UShort i = (pScrn->bitsPerPixel+7)/8 - 1;
    UShort ModeIndex = 0;
    
    while(m) {
	if(pScrn->bitsPerPixel == m->bpp &&
	   mode->HDisplay == m->width &&
	   mode->VDisplay == m->height)
	    return m->n;
	m = m->next;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
             "No valid BIOS VESA mode found for %dx%dx%d; searching built-in table.\n",
             mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel);

    switch(mode->HDisplay) {
      case 320:
          if(mode->VDisplay == 200)
             ModeIndex = VESAModeIndex_320x200[i];
	  else if(mode->VDisplay == 240)
             ModeIndex = VESAModeIndex_320x240[i];
          break;
      case 400:
          if(mode->VDisplay == 300)
             ModeIndex = VESAModeIndex_400x300[i];
          break;
      case 512:
          if(mode->VDisplay == 384)
             ModeIndex = VESAModeIndex_512x384[i];
          break;
      case 640:
          if(mode->VDisplay == 480)
             ModeIndex = VESAModeIndex_640x480[i];
	  else if(mode->VDisplay == 400)
             ModeIndex = VESAModeIndex_640x400[i];
          break;
      case 800:
          if(mode->VDisplay == 600)
             ModeIndex = VESAModeIndex_800x600[i];
          break;
      case 1024:
          if(mode->VDisplay == 768)
             ModeIndex = VESAModeIndex_1024x768[i];
          break;
      case 1280:
          if(mode->VDisplay == 1024)
             ModeIndex = VESAModeIndex_1280x1024[i];
          break;
      case 1600:
          if(mode->VDisplay == 1200)
             ModeIndex = VESAModeIndex_1600x1200[i];
          break;
      case 1920:
          if(mode->VDisplay == 1440)
             ModeIndex = VESAModeIndex_1920x1440[i];
          break;
   }

   if(!ModeIndex) xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
        "No valid mode found for %dx%dx%d in built-in table either.\n",
	mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel);

   return(ModeIndex);
}

USHORT
SiS_CalcModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode, unsigned long VBFlags, BOOLEAN havecustommodes)
{
   SISPtr pSiS = SISPTR(pScrn);
   UShort i = (pSiS->CurrentLayout.bitsPerPixel+7)/8 - 1;

   if(!(VBFlags & CRT1_LCDA)) {
      if((havecustommodes) && (!(mode->type & M_T_DEFAULT))) {
         return 0xfe;
      }
   } else {
      if((mode->HDisplay > pSiS->LCDwidth) ||
         (mode->VDisplay > pSiS->LCDheight)) {
	 return 0;
      }
   }

   return(SiS_GetModeID(pSiS->VGAEngine, VBFlags, mode->HDisplay, mode->VDisplay,
   			i, pSiS->FSTN, pSiS->LCDwidth, pSiS->LCDheight));
}

USHORT
SiS_CheckCalcModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode, unsigned long VBFlags, BOOLEAN havecustommodes)
{
   SISPtr pSiS = SISPTR(pScrn);
   UShort i = (pSiS->CurrentLayout.bitsPerPixel+7)/8 - 1;
   UShort ModeIndex = 0;
   int    j;

#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "Inside CheckCalcModeIndex (VBFlags %x, mode %dx%d)\n",
   	VBFlags,mode->HDisplay, mode->VDisplay);
#endif

   if(VBFlags & CRT2_LCD) {			/* CRT2 is LCD */

      if(pSiS->SiS_Pr->CP_HaveCustomData) {
         for(j=0; j<7; j++) {
            if((pSiS->SiS_Pr->CP_DataValid[j]) &&
               (mode->HDisplay == pSiS->SiS_Pr->CP_HDisplay[j]) &&
               (mode->VDisplay == pSiS->SiS_Pr->CP_VDisplay[j]) &&
               (mode->type & M_T_BUILTIN))
               return 0xfe;
	 }
      }

      if((pSiS->AddedPlasmaModes) && (mode->type & M_T_BUILTIN))
         return 0xfe;

      if((havecustommodes) &&
         (pSiS->LCDwidth) &&		/* = test if LCD present */
         (!(mode->type & M_T_DEFAULT)) &&
	 (!(mode->Flags & V_INTERLACE)))
         return 0xfe;

      if( ((mode->HDisplay <= pSiS->LCDwidth) &&
           (mode->VDisplay <= pSiS->LCDheight)) ||
	  ((pSiS->SiS_Pr->SiS_CustomT == CUT_PANEL848) &&
	   (((mode->HDisplay == 1360) && (mode->HDisplay == 768)) ||
	    ((mode->HDisplay == 1024) && (mode->HDisplay == 768)) ||
	    ((mode->HDisplay ==  800) && (mode->HDisplay == 600)))) ) {

         ModeIndex = SiS_GetModeID_LCD(pSiS->VGAEngine, VBFlags, mode->HDisplay, mode->VDisplay, i,
	 		       pSiS->FSTN, pSiS->SiS_Pr->SiS_CustomT, pSiS->LCDwidth, pSiS->LCDheight);

      }

   } else if(VBFlags & CRT2_TV) {		/* CRT2 is TV */

      ModeIndex = SiS_GetModeID_TV(pSiS->VGAEngine, VBFlags, mode->HDisplay, mode->VDisplay, i);

   } else if(VBFlags & CRT2_VGA) {		/* CRT2 is VGA2 */

      if((pSiS->AddedPlasmaModes) && (mode->type & M_T_BUILTIN))
	 return 0xfe;

      if((havecustommodes) &&
	 (!(mode->type & M_T_DEFAULT)) &&
	 (!(mode->Flags & V_INTERLACE)))
         return 0xfe;

      ModeIndex = SiS_GetModeID_VGA2(pSiS->VGAEngine, VBFlags, mode->HDisplay, mode->VDisplay, i);

   } else {					/* CRT1 only, no CRT2 */

      ModeIndex = SiS_CalcModeIndex(pScrn, mode, VBFlags, havecustommodes);

   }

   return(ModeIndex);
}

/* Calculate the vertical refresh rate from a mode */
int
SiSCalcVRate(DisplayModePtr mode)
{
   float hsync, refresh = 0;

   if(mode->HSync > 0.0)
       	hsync = mode->HSync;
   else if(mode->HTotal > 0)
       	hsync = (float)mode->Clock / (float)mode->HTotal;
   else
       	hsync = 0.0;

   if(mode->VTotal > 0)
       	refresh = hsync * 1000.0 / mode->VTotal;

   if(mode->Flags & V_INTERLACE)
       	refresh *= 2.0;

   if(mode->Flags & V_DBLSCAN)
       	refresh /= 2.0;

   if(mode->VScan > 1)
        refresh /= mode->VScan;

   if(mode->VRefresh > 0.0)
    	refresh = mode->VRefresh;

   if(hsync == 0 || refresh == 0) return(0);

   return((int)(refresh));
}

/* Calculate CR33 (rate index) for CRT1.
 * Calculation is done using currentmode, therefore it is
 * recommended to set VertRefresh and HorizSync to correct
 * values in config file.
 */
unsigned char
SISSearchCRT1Rate(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   SISPtr         pSiS = SISPTR(pScrn);
   int            i = 0;
   int            irefresh;
   unsigned short xres = mode->HDisplay;
   unsigned short yres = mode->VDisplay;
   unsigned char  index;
   BOOLEAN	  checksis730 = FALSE;

   irefresh = SiSCalcVRate(mode);
   if(!irefresh) {
      if(xres == 800 || xres == 1024 || xres == 1280) return 0x02;
      else return 0x01;
   }
   
   /* SiS730 has troubles on CRT2 if CRT1 is at 32bpp */
   if( (pSiS->sishw_ext.jChipType == SIS_730) && 
       (pSiS->VBFlags & VB_VIDEOBRIDGE) &&
       (pSiS->CurrentLayout.bitsPerPixel == 32) ) {
#ifdef SISDUALHEAD   
      if(pSiS->DualHeadMode) {
         if(pSiS->SecondHead) {
	    checksis730 = TRUE;
	 }
      } else
#endif      
      if((!pSiS->UseVESA) && (pSiS->VBFlags & CRT2_ENABLE) && (!pSiS->CRT1off)) {
         checksis730 = TRUE;
      }
   }   
   
#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "Debug: CalcVRate returned %d\n", irefresh);
#endif    

   /* We need the REAL refresh rate here */
   if(mode->Flags & V_INTERLACE)
       	irefresh /= 2;

   /* Do not multiply by 2 when DBLSCAN! */
   
#ifdef TWDEBUG 
   xf86DrvMsg(0, X_INFO, "Debug: Rate after correction = %d\n", irefresh);
#endif

   index = 0;
   while((sisx_vrate[i].idx != 0) && (sisx_vrate[i].xres <= xres)) {
	if((sisx_vrate[i].xres == xres) && (sisx_vrate[i].yres == yres)) {
	    if((checksis730 == FALSE) || (sisx_vrate[i].SiS730valid32bpp == TRUE)) {
	       if(sisx_vrate[i].refresh == irefresh) {
		   index = sisx_vrate[i].idx;
		   break;
	       } else if(sisx_vrate[i].refresh > irefresh) {
		   if((sisx_vrate[i].refresh - irefresh) <= 3) {
		      index = sisx_vrate[i].idx;
		   } else if( ((checksis730 == FALSE) || (sisx_vrate[i - 1].SiS730valid32bpp == TRUE)) && 
		              ((irefresh - sisx_vrate[i - 1].refresh) <=  2) &&
			      (sisx_vrate[i].idx != 1) ) {
		      index = sisx_vrate[i - 1].idx;
		   }
		   break;
	       } else if((irefresh - sisx_vrate[i].refresh) <= 2) {
	           index = sisx_vrate[i].idx;
		   break;
	       }
	    }
	}
	i++;
   }
   if(index > 0)
	return index;
   else {
        /* Default Rate index */
        if(xres == 800 || xres == 1024 || xres == 1280) return 0x02; 
   	else return 0x01;
   }
}

void
SISWaitRetraceCRT1(ScrnInfoPtr pScrn)
{
   SISPtr        pSiS = SISPTR(pScrn);
   int           watchdog;
   unsigned char temp;

   inSISIDXREG(SISCR,0x17,temp);
   if(!(temp & 0x80)) return;

   inSISIDXREG(SISSR,0x1f,temp);
   if(temp & 0xc0) return;

   watchdog = 65536;
   while((inSISREG(SISINPSTAT) & 0x08) && --watchdog);
   watchdog = 65536;
   while((!(inSISREG(SISINPSTAT) & 0x08)) && --watchdog);
}

void
SISWaitRetraceCRT2(ScrnInfoPtr pScrn)
{
   SISPtr        pSiS = SISPTR(pScrn);
   int           watchdog;
   unsigned char temp, reg;

   if(SiSBridgeIsInSlaveMode(pScrn)) {
      SISWaitRetraceCRT1(pScrn);
      return;
   }

   switch(pSiS->VGAEngine) {
   case SIS_300_VGA:
   	reg = 0x25;
	break;
   case SIS_315_VGA:
   	reg = 0x30;
	break;
   default:
        return;
   }

   watchdog = 65536;
   do {
   	inSISIDXREG(SISPART1, reg, temp);
	if(!(temp & 0x02)) break;
   } while(--watchdog);
   watchdog = 65536;
   do {
   	inSISIDXREG(SISPART1, reg, temp);
	if(temp & 0x02) break;
   } while(--watchdog);
}

static void
SISWaitVBRetrace(ScrnInfoPtr pScrn)
{
   SISPtr  pSiS = SISPTR(pScrn);

   if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
#ifdef SISDUALHEAD
      if(pSiS->DualHeadMode) {
   	 if(pSiS->SecondHead)
	    SISWaitRetraceCRT1(pScrn);
         else
	    SISWaitRetraceCRT2(pScrn);
      } else {
#endif
	 if(pSiS->VBFlags & DISPTYPE_DISP1) {
	    SISWaitRetraceCRT1(pScrn);
	 }
	 if(pSiS->VBFlags & DISPTYPE_DISP2) {
	    if(!(SiSBridgeIsInSlaveMode(pScrn))) {
	       SISWaitRetraceCRT2(pScrn);
	    }
	 }
#ifdef SISDUALHEAD
      }
#endif
   } else {
      SISWaitRetraceCRT1(pScrn);
   }
}

#define MODEID_OFF 0x449

unsigned char
SiS_GetSetModeID(ScrnInfoPtr pScrn, unsigned char id)
{
    return(SiS_GetSetBIOSScratch(pScrn, MODEID_OFF, id));
}

unsigned char
SiS_GetSetBIOSScratch(ScrnInfoPtr pScrn, USHORT offset, unsigned char value)
{
    unsigned char ret = 0;
#if (defined(i386) || defined(__i386) || defined(__i386__) || defined(__AMD64__))
    unsigned char *base;

    base = xf86MapVidMem(pScrn->scrnIndex, VIDMEM_MMIO, 0, 0x2000);
    if(!base) {
       SISErrorLog(pScrn, "(Could not map BIOS scratch area)\n");
       return 0;
    }

    ret = *(base + offset);

    /* value != 0xff means: set register */
    if(value != 0xff)
       *(base + offset) = value;

    xf86UnMapVidMem(pScrn->scrnIndex, base, 0x2000);
#endif
    return ret;
}

void
sisSaveUnlockExtRegisterLock(SISPtr pSiS, unsigned char *reg1, unsigned char *reg2)
{
    register unsigned char val;
    unsigned long mylockcalls;

    pSiS->lockcalls++;
    mylockcalls = pSiS->lockcalls;

    /* check if already unlocked */
    inSISIDXREG(SISSR, 0x05, val);
    if(val != 0xa1) {
       /* save State */
       if(reg1) *reg1 = val;
       /* unlock */
       outSISIDXREG(SISSR, 0x05, 0x86);
       inSISIDXREG(SISSR, 0x05, val);
       if(val != 0xA1) {
#ifdef TWDEBUG
	  unsigned char val1, val2;
	  int i;
#endif
          SISErrorLog(pSiS->pScrn,
               "Failed to unlock sr registers (%p, %lx, 0x%02x; %ld)\n",
	       (void *)pSiS, (unsigned long)pSiS->RelIO, val, mylockcalls);
#ifdef TWDEBUG
          for(i = 0; i <= 0x3f; i++) {
	  	inSISIDXREG(SISSR, i, val1);
		inSISIDXREG(0x3c4, i, val2);
		xf86DrvMsg(pSiS->pScrn->scrnIndex, X_INFO,
			"SR%02d: RelIO=0x%02x 0x3c4=0x%02x (%d)\n",
			i, val1, val2, mylockcalls);
	  }
#endif
          if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
	     /* Emergency measure: unlock at 0x3c4, and try to enable Relocated IO ports */
	     outSISIDXREG(0x3c4,0x05,0x86);
	     andSISIDXREG(0x3c4,0x33,~0x20);
	     outSISIDXREG(SISSR, 0x05, 0x86);
          }
       }
    }
    if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
       inSISIDXREG(SISCR, 0x80, val);
       if(val != 0xa1) {
          /* save State */
          if(reg2) *reg2 = val;
          outSISIDXREG(SISCR, 0x80, 0x86);
	  inSISIDXREG(SISCR, 0x80, val);
	  if(val != 0xA1) {
	     SISErrorLog(pSiS->pScrn,
	        "Failed to unlock cr registers (%p, %lx, 0x%02x)\n",
	       (void *)pSiS, (unsigned long)pSiS->RelIO, val);
	  }
       }
    }
}

void
sisRestoreExtRegisterLock(SISPtr pSiS, unsigned char reg1, unsigned char reg2)
{
    /* restore lock */
#ifndef UNLOCK_ALWAYS
    outSISIDXREG(SISSR, 0x05, reg1 == 0xA1 ? 0x86 : 0x00);
    if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
       outSISIDXREG(SISCR, 0x80, reg2 == 0xA1 ? 0x86 : 0x00);
    }
#endif
}

