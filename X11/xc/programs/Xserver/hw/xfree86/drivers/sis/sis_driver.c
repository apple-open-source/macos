/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_driver.c,v 1.86 2003/02/04 02:44:29 dawes Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002, 2003 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>.
 *
 *	     Thomas Winischhofer <thomas@winischhofer.net>:
 *              - 310/325 series (315/550/650/651/740/M650) support
 *		- (possibly incomplete) Xabre (SiS330) support
 *              - new mode switching code for 300, 310/325 and 330 series
 *              - many fixes for 300/540/630/730 chipsets,
 *              - many fixes for 5597/5598, 6326 and 530/620 chipsets,
 *              - VESA mode switching (deprecated),
 *              - extended CRT2/video bridge handling support,
 *              - dual head support on 300, 310/325 and 330 series
 *              - 650/LVDS (up to 1400x1050), 650/Chrontel 701x support
 *              - 30xB/30xLV/30xLVX video bridge support (300, 310/325, 330 series)
 *              - Xv support for 5597/5598, 6326, 530/620 and 310/325 series
 *              - video overlay enhancements for 300 series
 *              - TV and hi-res support for the 6326
 *		- Color HW cursor support for 300(emulated), 310/325 and 330 series
 *              - etc.
 */

#include "fb.h"
#include "xf1bpp.h"
#include "xf4bpp.h"
#include "mibank.h"
#include "micmap.h"
#include "xf86.h"
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

#ifdef XvExtension
#include "xf86xv.h"
#include "Xv.h"
#endif

#ifdef XF86DRI
#include "dri.h"
#endif

/* Mandatory functions */
static void SISIdentify(int flags);
static Bool SISProbe(DriverPtr drv, int flags);
static Bool SISPreInit(ScrnInfoPtr pScrn, int flags);
static Bool SISScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);
static Bool SISEnterVT(int scrnIndex, int flags);
static void SISLeaveVT(int scrnIndex, int flags);
static Bool SISCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool SISSaveScreen(ScreenPtr pScreen, int mode);
static Bool SISSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
static void SISAdjustFrame(int scrnIndex, int x, int y, int flags);
#ifdef SISDUALHEAD
static Bool SISSaveScreenDH(ScreenPtr pScreen, int mode);
#endif

/* Optional functions */
static void SISFreeScreen(int scrnIndex, int flags);
static int  SISValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
                 int flags);

/* Internally used functions */
static Bool    SISMapMem(ScrnInfoPtr pScrn);
static Bool    SISUnmapMem(ScrnInfoPtr pScrn);
static void    SISSave(ScrnInfoPtr pScrn);
static void    SISRestore(ScrnInfoPtr pScrn);
static void    SISVESARestore(ScrnInfoPtr pScrn);
static Bool    SISModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void    SISModifyModeInfo(DisplayModePtr mode);
static void    SiSPreSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void    SiSPostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void    SiS6326PostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static Bool    SiSSetVESAMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
static void    SiSBuildVesaModeList(ScrnInfoPtr pScrn, vbeInfoPtr pVbe, VbeInfoBlock *vbe);
static UShort  SiSCalcVESAModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void    SISVESASaveRestore(ScrnInfoPtr pScrn, vbeSaveRestoreFunction function);
static void    SISBridgeRestore(ScrnInfoPtr pScrn);
static void    SiSEnableTurboQueue(ScrnInfoPtr pScrn);
unsigned char  SISSearchCRT1Rate(ScrnInfoPtr pScrn, DisplayModePtr mode);
static void    SISWaitVBRetrace(ScrnInfoPtr pScrn);

void           SISWaitRetraceCRT1(ScrnInfoPtr pScrn);
void           SISWaitRetraceCRT2(ScrnInfoPtr pScrn);

BOOLEAN        SiSBridgeIsInSlaveMode(ScrnInfoPtr pScrn);
#ifdef CYCLECRT2
Bool           SISCycleCRT2Type(int scrnIndex, DisplayModePtr mode);
#endif

#ifdef DEBUG
static void SiSDumpModeInfo(ScrnInfoPtr pScrn, DisplayModePtr mode);
#endif

/* TW: New mode switching functions */
extern BOOLEAN 	SiSBIOSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                               ScrnInfoPtr pScrn, DisplayModePtr mode, BOOLEAN IsCustom);
extern BOOLEAN  SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                           ScrnInfoPtr pScrn,USHORT ModeNo, BOOLEAN dosetpitch);
extern USHORT 	SiS_CalcModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode);
extern USHORT   SiS_CheckCalcModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode, int VBFlags);
extern void	SiSRegInit(SiS_Private *SiS_Pr, USHORT BaseAddr);
extern DisplayModePtr  SiSBuildBuiltInModeList(ScrnInfoPtr pScrn);
#ifdef SISDUALHEAD
extern BOOLEAN 	SiSBIOSSetModeCRT1(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
				   ScrnInfoPtr pScrn, DisplayModePtr mode, BOOLEAN IsCustom);
extern BOOLEAN 	SiSBIOSSetModeCRT2(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
				   ScrnInfoPtr pScrn, DisplayModePtr mode);
#endif

/* TW: For power management for 310/325 series */
extern void SiS_Chrontel701xBLOn(SiS_Private *SiS_Pr);
extern void SiS_Chrontel701xBLOff(SiS_Private *SiS_Pr);
extern void SiS_SiS30xBLOn(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension);
extern void SiS_SiS30xBLOff(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension);

#ifdef SISDUALHEAD
static int      SISEntityIndex = -1;
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
#ifdef INCL_SIS330 /* TW: New for SiS330 (untested) */
    { PCI_CHIP_SIS330,      "SIS330(Xabre)" },
#endif
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
#ifdef INCL_SIS330 /* TW: New for SiS330 */
    { PCI_CHIP_SIS330,      PCI_CHIP_SIS330,    RES_SHARED_VGA },
#endif
    { -1,                   -1,                 RES_UNDEFINED }
};

static const char *xaaSymbols[] = {
    "XAACopyROP",
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAFillSolidRects",
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
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    "vgaHWUnlock",
    NULL
};

static const char *miscfbSymbols[] = {
    "xf1bppScreenInit",
    "xf4bppScreenInit",
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
    "xf86DoEDID_DDC1",
#ifdef SISI2C
    "xf86DoEDID_DDC2",
#endif
    NULL
};

static const char *i2cSymbols[] = {
    "xf86I2CBusInit",
    "xf86CreateI2CBusRec",
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

    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&SIS, module, 0);
        LoaderRefSymLists(vgahwSymbols, fbSymbols, i2cSymbols, xaaSymbols,
			  miscfbSymbols, shadowSymbols, ramdacSymbols,
			  vbeSymbols, int10Symbols,
#ifdef XF86DRI
			  drmSymbols, driSymbols,
#endif
			  NULL);
        return (pointer)TRUE;
    } 

    if (errmaj) *errmaj = LDR_ONCEONLY;
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
    if (pScrn->driverPrivate != NULL)
        return TRUE;

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

    /* TW: Just to make sure... */
    if(!pSiS) return;

    pSiSEnt = pSiS->entityPrivate;

    if(pSiS->pstate) xfree(pSiS->pstate);
    pSiS->pstate = NULL;
    if(pSiS->fonts) xfree(pSiS->fonts);
    pSiS->fonts = NULL;
#ifdef SISDUALHEAD
    if(pSiSEnt) {
      if(!pSiS->SecondHead) {
          /* TW: Free memory only if we are first head; in case of an error
	   *     during init of the second head, the server will continue -
	   *     and we need the BIOS image and SiS_Private for the first
	   *     head.
	   */
	  if(pSiSEnt->BIOS) xfree(pSiSEnt->BIOS);
          pSiSEnt->BIOS = pSiS->BIOS = NULL;
	  if(pSiSEnt->SiS_Pr) xfree(pSiSEnt->SiS_Pr);
          pSiSEnt->SiS_Pr = pSiS->SiS_Pr = NULL;
      } else {
      	  pSiS->BIOS = NULL;
	  pSiS->SiS_Pr = NULL;
      }
    } else {
#endif
      if(pSiS->BIOS) xfree(pSiS->BIOS);
      pSiS->BIOS = NULL;
      if(pSiS->SiS_Pr) xfree(pSiS->SiS_Pr);
      pSiS->SiS_Pr = NULL;
#ifdef SISDUALHEAD
    }
#endif
    if (pSiS->pVbe) vbeFree(pSiS->pVbe);
    pSiS->pVbe = NULL;
    if (pScrn->driverPrivate == NULL)
        return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void
SISDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char extDDC_PCR=0;
    unsigned char crtc17, seq1;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
          "SISDisplayPowerManagementSet(%d)\n",PowerManagementMode);

    /* unlock registers */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Read CR17 */
    inSISIDXREG(SISCR, 0x17, crtc17);

    /* Read SR1 */
    inSISIDXREG(SISSR, 0x01, seq1);

    if(pSiS->VBFlags & CRT2_LCD) {
      if(((pSiS->VGAEngine == SIS_300_VGA) &&
    	  (!(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)))) ||
         ((pSiS->VGAEngine == SIS_315_VGA) &&
          ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
         /* Read Power Control Register (SR11) */
         inSISIDXREG(SISSR, 0x11, extDDC_PCR);
         /* if not blanked, obtain state of LCD blank flags set by BIOS */
         if(!pSiS->Blank) {
	    pSiS->LCDon = extDDC_PCR;
         }
         /* erase LCD blank flags */
         extDDC_PCR &= ~0x0C;
      }
    }

    switch (PowerManagementMode) {

       case DPMSModeOn:      /* HSync: On, VSync: On */

	    pSiS->Blank = FALSE;
            seq1 &= ~0x20;
            crtc17 |= 0x80;
	    if(pSiS->VBFlags & CRT2_LCD) {
	       if(pSiS->VGAEngine == SIS_315_VGA) {
	          if(pSiS->VBFlags & VB_CHRONTEL) {
		      SiS_Chrontel701xBLOn(pSiS->SiS_Pr);
		  } else if(pSiS->VBFlags & VB_LVDS) {
		      extDDC_PCR |= (pSiS->LCDon & 0x0C);
		  } else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		  }
	       } else if(pSiS->VGAEngine == SIS_300_VGA) {
	           if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   } else {
	              extDDC_PCR |= (pSiS->LCDon & 0x0C);
		   }
	       }
	    }
            break;

       case DPMSModeStandby: /* HSync: Off, VSync: On */
       case DPMSModeSuspend: /* HSync: On, VSync: Off */

       	    pSiS->Blank = TRUE;
            seq1 |= 0x20 ;
	    if(pSiS->VBFlags & CRT2_LCD) {
		if(pSiS->VGAEngine == SIS_315_VGA) {
		   if(pSiS->VBFlags & VB_CHRONTEL) {
		      SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		   } else if(pSiS->VBFlags & VB_LVDS) {
		      extDDC_PCR |= 0x08;
		   } else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   }
		} else if(pSiS->VGAEngine == SIS_300_VGA) {
		   if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   } else {
		      extDDC_PCR |= 0x08;
		   }
		}
	    }
            break;

       case DPMSModeOff:     /* HSync: Off, VSync: Off */

            pSiS->Blank = TRUE;
            seq1 |= 0x20;
	    if(pSiS->VGAEngine == SIS_300_VGA ||
	       pSiS->VGAEngine == SIS_315_VGA) {
	       /* TW: We can't switch off CRT1 if bridge is in slavemode */
	       if(pSiS->VBFlags & CRT2_ENABLE) {
	          if(!(SiSBridgeIsInSlaveMode(pScrn))) crtc17 &= ~0x80;
	       } else crtc17 &= ~0x80;
	    } else {
	       crtc17 &= ~0x80;
	    }
	    if(pSiS->VBFlags & CRT2_LCD) {
		if(pSiS->VGAEngine == SIS_315_VGA) {
		   if(pSiS->VBFlags & VB_CHRONTEL) {
		      SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		   } else if(pSiS->VBFlags & VB_LVDS) {
		      extDDC_PCR |= 0x0C;
		   } else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   }
		} else if(pSiS->VGAEngine == SIS_300_VGA) {
		   if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		      SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   } else {
		      extDDC_PCR |= 0x0C;
		   }
		}
            }
	    break;

    }

    outSISIDXREG(SISSR, 0x01, seq1);    /* Set/Clear "Display On" bit */

    outSISIDXREG(SISCR, 0x17, crtc17);

    if(pSiS->VBFlags & CRT2_LCD) {
      if(((pSiS->VGAEngine == SIS_300_VGA) &&
          (!(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)))) ||
         ((pSiS->VGAEngine == SIS_315_VGA) &&
          ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
            outSISIDXREG(SISSR, 0x11, extDDC_PCR);
      }
    }

    outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
    usleep(10000);
    outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */

}

#ifdef SISDUALHEAD
/* TW: DPMS for dual head mode */
static void
SISDisplayPowerManagementSetDH(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char crtc17 = 0;
    unsigned char extDDC_PCR=0;
    unsigned char seq1 = 0;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
           "SISDisplayPowerManagementSetDH(%d)\n",PowerManagementMode);

    /* unlock registers */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if (pSiS->SecondHead) {

	/* TW: Second (slave) head is always CRT1 */

	/* Read CR17 and SR01 */
        inSISIDXREG(SISCR, 0x17, crtc17);
        inSISIDXREG(SISSR, 0x01, seq1);

    	switch (PowerManagementMode)
    	{
          case DPMSModeOn:       /* HSync: On, VSync: On */
            seq1 &= ~0x20 ;
            crtc17 |= 0x80;
	    pSiS->BlankCRT1 = FALSE;
            break;

          case DPMSModeStandby:  /* HSync: Off, VSync: On */
          case DPMSModeSuspend:  /* HSync: On, VSync: Off */
	    seq1 |= 0x20;
	    pSiS->BlankCRT1 = TRUE;
            break;

          case DPMSModeOff:      /* HSync: Off, VSync: Off */
            seq1 |= 0x20 ;
	    pSiS->BlankCRT1 = TRUE;
            crtc17 &= ~0x80;
            break;
    	}
	outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */

	outSISIDXREG(SISSR, 0x01, seq1);    /* Set/Clear "Display On" bit */

    	usleep(10000);

	outSISIDXREG(SISCR, 0x17, crtc17);

	outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */

    } else {

    	/* TW: Master head is always CRT2 */

	/* TV can not be managed */
	if(!(pSiS->VBFlags & CRT2_LCD)) return;

        if(((pSiS->VGAEngine == SIS_300_VGA) &&
	    (!(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)))) ||
	   ((pSiS->VGAEngine == SIS_315_VGA) &&
	    ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
	   /* Read Power Control Register (SR11) */
           inSISIDXREG(SISSR, 0x11, extDDC_PCR);
      	   /* if not blanked obtain state of LCD blank flags set by BIOS */
    	   if(!pSiS->BlankCRT2) {
		pSiS->LCDon = extDDC_PCR;
	   }
       	   /* erase LCD blank flags */
    	   extDDC_PCR &= ~0xC;
	}

    	switch (PowerManagementMode) {

          case DPMSModeOn:
	    pSiS->BlankCRT2 = FALSE;
	    if(pSiS->VGAEngine == SIS_315_VGA) {
		if(pSiS->VBFlags & VB_CHRONTEL) {
		   SiS_Chrontel701xBLOn(pSiS->SiS_Pr);
		} else if(pSiS->VBFlags & VB_LVDS) {
		   extDDC_PCR |= (pSiS->LCDon & 0x0C);
		} else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOn(pSiS->SiS_Pr, &pSiS->sishw_ext);
		}
	    } else if(pSiS->VGAEngine == SIS_300_VGA) {
	        if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOn(pSiS->SiS_Pr, &pSiS->sishw_ext);
	        } else {
		   extDDC_PCR |= (pSiS->LCDon & 0x0C);
		}
            }
            break;

          case DPMSModeStandby:
	  case DPMSModeSuspend:
	    pSiS->BlankCRT2 = TRUE;
	    if(pSiS->VGAEngine == SIS_315_VGA) {
	    	if(pSiS->VBFlags & VB_CHRONTEL) {
		   SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		} else if(pSiS->VBFlags & VB_LVDS) {
		   extDDC_PCR |= 0x08;
		} else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr, &pSiS->sishw_ext);
		}
	    } else if(pSiS->VGAEngine == SIS_300_VGA) {
	        if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr, &pSiS->sishw_ext);
	        } else {
	    	   extDDC_PCR |= 0x08;
		}
	    }
            break;

          case DPMSModeOff:
	    pSiS->BlankCRT2 = TRUE;
	    if(pSiS->VGAEngine == SIS_315_VGA) {
		if(pSiS->VBFlags & VB_CHRONTEL) {
		   SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		} else if(pSiS->VBFlags & VB_LVDS) {
		   extDDC_PCR |= 0x0C;
		} else if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr, &pSiS->sishw_ext);
		}
	    } else if(pSiS->VGAEngine == SIS_300_VGA) {
	        if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   SiS_SiS30xBLOff(pSiS->SiS_Pr, &pSiS->sishw_ext);
	        } else {
                   extDDC_PCR |= 0x0C;
		}
            }
            break;
	}

	if(((pSiS->VGAEngine == SIS_300_VGA) &&
	    (!(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)))) ||
	   ((pSiS->VGAEngine == SIS_315_VGA) &&
	    ((pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) == VB_LVDS))) {
	   outSISIDXREG(SISSR, 0x11, extDDC_PCR);
	}

    }
}
#endif

/* Mandatory */
static void
SISIdentify(int flags)
{
    xf86PrintChipsets(SIS_NAME, "driver for SiS chipsets", SISChipsets);
}

static void
SIS1bppColorMap(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);

   outSISREG(SISCOLIDX, 0x00);
   outSISREG(SISCOLDATA, 0x00);
   outSISREG(SISCOLDATA, 0x00);
   outSISREG(SISCOLDATA, 0x00);

   outSISREG(SISCOLIDX, 0x3f);
   outSISREG(SISCOLDATA, 0x3f);
   outSISREG(SISCOLDATA, 0x3f);
   outSISREG(SISCOLDATA, 0x3f);
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

    if ((numDevSections = xf86MatchDevice(SIS_DRIVER_NAME,
                      &devSections)) <= 0) {
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
    if (xf86GetPciVideoInfo() == NULL) {
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
    if (numUsed <= 0)
        return FALSE;

    if (flags & PROBE_DETECT)
        foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
        ScrnInfoPtr pScrn;
#ifdef SISDUALHEAD
	EntityInfoPtr pEnt;
#endif

        /* Allocate a ScrnInfoRec and claim the slot */
        pScrn = NULL;

        if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
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

	/* TW: I assume these chipsets as - basically - dual head capable. */
	if (pEnt->chipset == PCI_CHIP_SIS630 || pEnt->chipset == PCI_CHIP_SIS540 ||
	    pEnt->chipset == PCI_CHIP_SIS650 || pEnt->chipset == PCI_CHIP_SIS550 ||
	    pEnt->chipset == PCI_CHIP_SIS315 || pEnt->chipset == PCI_CHIP_SIS315H ||
	    pEnt->chipset == PCI_CHIP_SIS315PRO || pEnt->chipset == PCI_CHIP_SIS330 ||
	    pEnt->chipset == PCI_CHIP_SIS300) {

	    SISEntPtr pSiSEnt = NULL;
	    DevUnion  *pPriv;

	    xf86SetEntitySharable(usedChips[i]);
	    if (SISEntityIndex < 0)
	        SISEntityIndex = xf86AllocateEntityPrivateIndex();
	    pPriv = xf86GetEntityPrivate(pScrn->entityList[0], SISEntityIndex);
	    if (!pPriv->ptr) {
	        pPriv->ptr = xnfcalloc(sizeof(SISEntRec), 1);
		pSiSEnt = pPriv->ptr;
		pSiSEnt->lastInstance = -1;
		pSiSEnt->DisableDual = FALSE;
		pSiSEnt->ErrorAfterFirst = FALSE;
		pSiSEnt->MapCountIOBase = pSiSEnt->MapCountFbBase = 0;
		pSiSEnt->FbBase = pSiSEnt->IOBase = NULL;
  		pSiSEnt->forceUnmapIOBase = FALSE;
		pSiSEnt->forceUnmapFbBase = FALSE;
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


/* TW: If monitor section has no HSync/VRefresh data,
 *     derive it from DDC data.
 */
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
       { 1280, 1024, 85, 91.1 },
       { 1600, 1200, 60, 75.0 },
       { 1600, 1200, 65, 81.3 },
       { 1600, 1200, 70, 87.5 },
       { 1600, 1200, 75, 93.8 },
       { 1600, 1200, 85, 106.3 },
       { 1920, 1440, 60, 90.0 },
       { 1920, 1440, 75, 112.5 }
   };

   if(flag) { /* HSync */
      for (i = 0; i < 4; i++) {
    	 if (ddc->det_mon[i].type == DS_RANGES) {
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

      for (i = 0; i < 4; i++) {
         if (ddc->det_mon[i].type == DS_RANGES) {
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

static xf86MonPtr
SiSInternalDDC(ScrnInfoPtr pScrn, int crtno)
{
   SISPtr        pSiS = SISPTR(pScrn);
   USHORT        temp, i;
   unsigned char buffer[256];
   xf86MonPtr    pMonitor = NULL;

   /* TW: If CRT1 is off, skip DDC */
   if((pSiS->CRT1off) && (!crtno)) return NULL;

   temp = SiS_HandleDDC(pSiS->SiS_Pr, pSiS, crtno, 0, &buffer[0]);
   if((!temp) || (temp == 0xffff)) {
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                "CRT%d DDC probing failed, now trying via VBE\n", crtno + 1);
      return(NULL);
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT%d DDC supported\n", crtno + 1);
      xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT%d DDC level: %s%s%s%s\n",
	     crtno + 1,
	     (temp & 0x1a) ? "" : "[none of the supported]",
	     (temp & 0x02) ? "2 " : "",
	     (temp & 0x08) ? "3 " : "",
             (temp & 0x10) ? "4" : "");
      if(temp & 0x02) {
	 i = 3;  /* Number of retrys */
	 do {
	    temp = SiS_HandleDDC(pSiS->SiS_Pr, pSiS, crtno, 1, &buffer[0]);
	 } while((temp) && i--);
         if(!temp) {
	    if((pMonitor = xf86InterpretEDID(pScrn->scrnIndex, &buffer[0]))) {
	       return(pMonitor);
	    } else {
	       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	           "CRT%d DDC EDID corrupt\n", crtno + 1);
	       return(NULL);
	    }
	 } else {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"CRT%d DDC reading failed\n", crtno + 1);
	    return(NULL);
	 }
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "DDC levels 3 and 4 not supported by this driver yet.\n");
         return(NULL);
      }
   }
}

static xf86MonPtr
SiSDoPrivateDDC(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);

#ifdef SISDUALHEAD
    if((pSiS->DualHeadMode) && (!pSiS->SecondHead))
	return(SiSInternalDDC(pScrn, 1));
    else
#endif
        return(SiSInternalDDC(pScrn, 0)); 
}

/* Mandatory */
static Bool
SISPreInit(ScrnInfoPtr pScrn, int flags)
{
    SISPtr pSiS;
    MessageType from;
    unsigned char usScratchCR17, CR5F;
    unsigned char usScratchCR32;
    unsigned long int i;
    int temp;
    ClockRangePtr clockRanges;
    char *mod = NULL;
    const char *Sym = NULL;
    int pix24flags;
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = NULL;
#endif
    DisplayModePtr first, p, n;
    DisplayModePtr tempmode, delmode, mymodes;
    unsigned char srlockReg,crlockReg;
    unsigned char tempreg;
    xf86MonPtr pMonitor = NULL;
    Bool didddc2;

    vbeInfoPtr pVbe;
    VbeInfoBlock *vbe;

    if (flags & PROBE_DETECT) {
        if (xf86LoadSubModule(pScrn, "vbe")) {
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
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Number of entities is not 1\n");
        return FALSE;
    }

    /* The vgahw module should be loaded here when needed */
    if(!xf86LoadSubModule(pScrn, "vgahw")) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load vgahw module\n");
        return FALSE;
    }

    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
           "SiS driver (31/01/03-1) by "
	   "Thomas Winischhofer <thomas@winischhofer.net>\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
           "See http://www.winischhofer.net/linuxsisvga.shtml "
	   "for documentation and updates\n");	   

    /* Allocate a vgaHWRec */
    if(!vgaHWGetHWRec(pScrn)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not allocate VGA private\n");
        return FALSE;
    }

    /* Allocate the SISRec driverPrivate */
    if(!SISGetRec(pScrn)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not allocate memory for pSiS private\n");
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
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Entity's bus type is not PCI\n");
    	SISFreeRec(pScrn);
        return FALSE;
    }

#ifdef SISDUALHEAD
    /* TW: Allocate an entity private if necessary */
    if(xf86IsEntityShared(pScrn->entityList[0])) {
        pSiSEnt = xf86GetEntityPrivate(pScrn->entityList[0],
					SISEntityIndex)->ptr;
        pSiS->entityPrivate = pSiSEnt;

	/* TW: If something went wrong, quit here */
    	if ((pSiSEnt->DisableDual) || (pSiSEnt->ErrorAfterFirst)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	   "First head encountered fatal error, can't continue\n");
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
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not map VGA memory\n");
          SISFreeRec(pScrn);
          return FALSE;
       }
    }
    vgaHWGetIOBase(VGAHWPTR(pScrn));

    /* TW: We "patch" the PIOOffset inside vgaHW in order to force
     *     the vgaHW module to use our relocated i/o ports.
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
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load int10 module\n");
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
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load ramdac module\n");
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
    if (pSiS->pEnt->device->chipset && *pSiS->pEnt->device->chipset)  {
        pScrn->chipset = pSiS->pEnt->device->chipset;
        pSiS->Chipset = xf86StringToToken(SISChipsets, pScrn->chipset);
        from = X_CONFIG;
    } else if (pSiS->pEnt->device->chipID >= 0) {
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
    if (pSiS->pEnt->device->chipRev >= 0) {
        pSiS->ChipRev = pSiS->pEnt->device->chipRev;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
                        pSiS->ChipRev);
    } else {
        pSiS->ChipRev = pSiS->PciInfo->chipRev;
    }
    pSiS->sishw_ext.jChipRevision = pSiS->ChipRev;

    /* TW: Determine SiS6326 chiprevision. This is not yet used for
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
		          (((pSiS->ChipRev & 0xf0) == 0xd0) ? "DVD (Dx)" :
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
    if (pScrn->chipset == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "ChipID 0x%04X is not recognised\n", pSiS->Chipset);
#ifdef SISDUALHEAD
        if (pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
        SISFreeRec(pScrn);
        return FALSE;
    }
    if (pSiS->Chipset < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Chipset \"%s\" is not recognised\n", pScrn->chipset);
#ifdef SISDUALHEAD
        if (pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
        SISFreeRec(pScrn);
        return FALSE;
    }

    /* TW: Determine chipset and VGA engine type for new mode switching code */
    switch(pSiS->Chipset) {
	case PCI_CHIP_SIS300:
		pSiS->sishw_ext.jChipType = SIS_300;
		pSiS->VGAEngine = SIS_300_VGA;
		break;
	case PCI_CHIP_SIS630: /* 630 + 730 */
		pSiS->sishw_ext.jChipType = SIS_630;
		if(pciReadLong(0x00000000, 0x00) == 0x07301039) {
			pSiS->sishw_ext.jChipType = SIS_730;
		}
		pSiS->VGAEngine = SIS_300_VGA;
		break;
	case PCI_CHIP_SIS540:
		pSiS->sishw_ext.jChipType = SIS_540;
		pSiS->VGAEngine = SIS_300_VGA;
		break;
	case PCI_CHIP_SIS315H:
		pSiS->sishw_ext.jChipType = SIS_315H;
		pSiS->VGAEngine = SIS_315_VGA;
		break;
	case PCI_CHIP_SIS315:
		/* TW: Override for simplicity */
	        pSiS->Chipset = PCI_CHIP_SIS315H;
		pSiS->sishw_ext.jChipType = SIS_315;
		pSiS->VGAEngine = SIS_315_VGA;
		break;
	case PCI_CHIP_SIS315PRO:
		/* TW: Override for simplicity */
		pSiS->Chipset = PCI_CHIP_SIS315H;
		pSiS->sishw_ext.jChipType = SIS_315PRO;
		pSiS->VGAEngine = SIS_315_VGA;
		break;
	case PCI_CHIP_SIS550:
		pSiS->sishw_ext.jChipType = SIS_550;
		pSiS->VGAEngine = SIS_315_VGA;
		break;
	case PCI_CHIP_SIS650: /* 650 + 740 */
		pSiS->sishw_ext.jChipType = SIS_650;
		pSiS->VGAEngine = SIS_315_VGA;
		break;
	case PCI_CHIP_SIS330:
		pSiS->sishw_ext.jChipType = SIS_330;
		pSiS->VGAEngine = SIS_315_VGA;  
		break;
	case PCI_CHIP_SIS530:
		pSiS->sishw_ext.jChipType = SIS_530;
		pSiS->VGAEngine = SIS_530_VGA;
		break;
	default:
		pSiS->sishw_ext.jChipType = SIS_OLD;
		pSiS->VGAEngine = SIS_OLD_VGA;
		break;
    }

    /* TW: Now check if sisfb is loaded. Since sisfb only supports
     * the 300 and 310/325 series, we only do this for these chips.
     * We use this for checking where sisfb starts its memory
     * heap in order to automatically detect the correct MaxXFBMem
     * setting (which normally is given by the option of the same name).
     * That only works if sisfb is completely running, ie with
     * a video mode (because the fbdev will not be installed otherwise.)
     */

    pSiS->donttrustpdc = FALSE;
    pSiS->sisfbpdc = 0;

    if(pSiS->VGAEngine == SIS_300_VGA || pSiS->VGAEngine == SIS_315_VGA) {

       int fd, i;
       sisfb_info mysisfbinfo;
       BOOL found = FALSE;
       char name[10];

       i=0;
       do {
         sprintf(name, "/dev/fb%1d", i);
         if((fd = open(name, 'r'))) {

	   if(!ioctl(fd, SISFB_GET_INFO, &mysisfbinfo)) {

	      if(mysisfbinfo.sisfb_id == SISFB_ID) {

	         if((mysisfbinfo.sisfb_version >= 1) &&
		    (mysisfbinfo.sisfb_revision >=5) &&
  		    (mysisfbinfo.sisfb_patchlevel >= 8)) {
		    /* TW: Added PCI bus/slot/func into in sisfb Version 1.5.08.
		           Check this to make sure we run on the same card as sisfb
		     */
		    if((mysisfbinfo.sisfb_pcibus == pSiS->PciInfo->bus) &&
		       (mysisfbinfo.sisfb_pcislot == pSiS->PciInfo->device) &&
		       (mysisfbinfo.sisfb_pcifunc == pSiS->PciInfo->func) ) {
	         	found = TRUE;
		    }
		 } else found = TRUE;

		 if(found) {
		   xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      	     "%s: SiS kernel fb driver (sisfb) %d.%d.%d detected (PCI: %02d:%02d.%d)\n",
		     &name[5],
		     mysisfbinfo.sisfb_version,
		     mysisfbinfo.sisfb_revision,
		     mysisfbinfo.sisfb_patchlevel,
		     pSiS->PciInfo->bus,
		     pSiS->PciInfo->device,
		     pSiS->PciInfo->func);
		   /* TW: Added version/rev/pl in sisfb 1.4.0 */
		   if(mysisfbinfo.sisfb_version == 0) {
		     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Old version of sisfb found. Please update\n");
		   }
		   pSiS->sisfbMem = mysisfbinfo.heapstart;
		   /* TW: Basically, we can't trust the pdc register if sisfb is loaded */
		   pSiS->donttrustpdc = TRUE;
		   xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		      "sisfb: memory heap starts at %dKB\n", pSiS->sisfbMem);
		   xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		      "sisfb: using video mode 0x%02x\n", mysisfbinfo.fbvidmode);
		   if((mysisfbinfo.sisfb_version >= 1) &&
		      (mysisfbinfo.sisfb_revision >=5) &&
  		      (mysisfbinfo.sisfb_patchlevel >= 6)) {
		     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		        "sisfb: %sreserved hardware cursor, using %s command queue\n",
			(mysisfbinfo.sisfb_caps & 0x80) ? "" : "not ",
			(mysisfbinfo.sisfb_caps & 0x40) ? "SiS300 series Turbo" :
			   (mysisfbinfo.sisfb_caps & 0x20) ? "SiS310/325 series AGP" :
			      (mysisfbinfo.sisfb_caps & 0x10) ? "SiS310/325 series VRAM" :
			         (mysisfbinfo.sisfb_caps & 0x08) ? "SiS310/325 series MMIO" :
				    "no");
		   }
		   if((mysisfbinfo.sisfb_version >= 1) &&
		      (mysisfbinfo.sisfb_revision >=5) &&
  		      (mysisfbinfo.sisfb_patchlevel >= 10)) {
		      /* TW: We can trust the pdc value if sisfb is of recent version */
		      pSiS->donttrustpdc = FALSE;
		      if(mysisfbinfo.sisfb_patchlevel >= 11) {
		      	 pSiS->sisfbpdc = mysisfbinfo.sisfb_lcdpdc;
		      }
		   }
		 }
	      }
	   }
	   close (fd);
         }
	 i++;

       } while((i <= 7) && (!found));
    }

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     * TW: Additionally, determine the size of the HWCursor memory
     * area.
     */
    switch (pSiS->VGAEngine) {
      case SIS_300_VGA:
        pSiS->CursorSize = 4096;
    	pix24flags = Support32bppFb |
	             SupportConvert24to32;
	break;
      case SIS_315_VGA:
        pSiS->CursorSize = 16384;
    	pix24flags = Support32bppFb |
	             SupportConvert24to32;
	break;
      case SIS_530_VGA:
        pSiS->CursorSize = 2048;
    	pix24flags = Support32bppFb |
	             Support24bppFb |
                     SupportConvert24to32 |
		     SupportConvert32to24;
        break;
      default:
        pSiS->CursorSize = 2048;
        pix24flags = Support24bppFb |
	             SupportConvert32to24 |
	             PreferConvert32to24;
	break;
    }

#ifdef SISDUALHEAD
    /* TW: In case of Dual Head, we need to determine if we are the "master" head or
     *     the "slave" head. In order to do that, we set PrimInit to DONE in the
     *     shared entity at the end of the first initialization. The second
     *     initialization then knows that some things have already been done. THIS
     *     ALWAYS ASSUMES THAT THE FIRST DEVICE INITIALIZED IS THE MASTER!
     */

    if(xf86IsEntityShared(pScrn->entityList[0])) {
      if(pSiSEnt->lastInstance > 0) {
     	if(!xf86IsPrimInitDone(pScrn->entityList[0])) {
		/* First Head (always CRT2) */
		pSiS->SecondHead = FALSE;
		pSiSEnt->pScrn_1 = pScrn;
		pSiSEnt->CRT1ModeNo = pSiSEnt->CRT2ModeNo = -1;
		pSiS->DualHeadMode = TRUE;
		pSiSEnt->DisableDual = FALSE;
		pSiSEnt->BIOS = NULL;
		pSiSEnt->SiS_Pr = NULL;
	} else {
		/* Second Head (always CRT1) */
		pSiS->SecondHead = TRUE;
		pSiSEnt->pScrn_2 = pScrn;
		pSiS->DualHeadMode = TRUE;
	}
      } else {
        /* TW: Only one screen in config file - disable dual head mode */
        pSiS->SecondHead = FALSE;
	pSiS->DualHeadMode = FALSE;
	pSiSEnt->DisableDual = TRUE;
      }
    } else {
        /* TW: Entity is not shared - disable dual head mode */
        pSiS->SecondHead = FALSE;
	pSiS->DualHeadMode = FALSE;
    }
#endif

    pSiS->ForceCursorOff = FALSE;

    /* TW: Allocate SiS_Private (for mode switching code) and initialize it */
    pSiS->SiS_Pr = NULL;
#ifdef SISDUALHEAD
    if(pSiSEnt) {
       if(pSiSEnt->SiS_Pr) pSiS->SiS_Pr = pSiSEnt->SiS_Pr;
    }
#endif
    if(!pSiS->SiS_Pr) {
       if(!(pSiS->SiS_Pr = xnfcalloc(sizeof(SiS_Private), 1))) {
           xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not allocate memory for SiS_Pr private\n");
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
    }
    pSiS->SiS_Pr->SiS_Backup70xx = 0xff;
    pSiS->SiS_Pr->SiS_CHOverScan = -1;
    pSiS->SiS_Pr->SiS_ChSW = FALSE;
    pSiS->SiS_Pr->CRT1UsesCustomMode = FALSE;

    /* TW: Get our relocated IO registers */
    pSiS->RelIO = (pSiS->PciInfo->ioBase[2] & 0xFFFC) + pSiS->IODBase;
    pSiS->sishw_ext.ulIOAddress = pSiS->RelIO + 0x30;
    xf86DrvMsg(pScrn->scrnIndex, from, "Relocated IO registers at 0x%lX\n",
           (unsigned long)pSiS->RelIO);

    /* TW: Initialize SiS Port Reg definitions for externally used
     *     BIOS emulation (init.c/init301.c) functions.
     */
    SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO + 0x30);

    /* TW: The following identifies the old chipsets. This is only
     *     partly used since the really old chips are not supported,
     *     but I keep it here for future use.
     */
    if(pSiS->VGAEngine == SIS_OLD_VGA || pSiS->VGAEngine == SIS_530_VGA) {
       switch(pSiS->Chipset) {
       case PCI_CHIP_SG86C205:    /* Just for making it complete */
          {
	  unsigned char temp;
	  sisSaveUnlockExtRegisterLock(pSiS, &srlockReg, &crlockReg);
	  inSISIDXREG(SISSR, 0x10, temp);
	  if(temp & 0x80) pSiS->oldChipset = OC_SIS6205B;
	  else pSiS->oldChipset = (pSiS->ChipRev == 0x11) ?
	  		OC_SIS6205C : OC_SIS6205A;
          break;
	  }
       case PCI_CHIP_SIS82C204:   /* Just for making it complete */
       	  pSiS->oldChipset = OC_SIS82204; break;
       case 0x6225:		  /* Just for making it complete */
          pSiS->oldChipset = OC_SIS6225; break;
       case PCI_CHIP_SIS5597:
          pSiS->oldChipset = OC_SIS5597; break;
       case PCI_CHIP_SIS6326:
          pSiS->oldChipset = OC_SIS6326; break;
       case PCI_CHIP_SIS530:
          if((pSiS->ChipRev & 0x0f) < 0x0a)
	  	pSiS->oldChipset = OC_SIS530A;
	  else  pSiS->oldChipset = OC_SIS530B;
	  break;
       default:
          pSiS->oldChipset = OC_UNKNOWN;
       }
    }

    if(!xf86SetDepthBpp(pScrn, 8, 8, 8, pix24flags)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86SetDepthBpp() error\n");
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
	    (pSiS->VGAEngine == SIS_315_VGA))
		temp = 1;
         break;
      default:
	 temp = 1;
    }

    if(temp) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "Given depth (%d) is not supported by this driver/chipset\n",
               pScrn->depth);
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	SISFreeRec(pScrn);
        return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    /* Get the depth24 pixmap format */
    if(pScrn->depth == 24 && pix24bpp == 0)
        pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if(pScrn->depth > 8) {
        /* The defaults are OK for us */
        rgb zeros = {0, 0, 0};

        if(!xf86SetWeight(pScrn, zeros, zeros)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86SetWeight() error\n");
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
	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	      	"RGB Weight %d%d%d at depth %d not supported by hardware\n",
		pScrn->weight.red, pScrn->weight.green,
		pScrn->weight.blue, pScrn->depth);
#ifdef SISDUALHEAD
	      if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	      if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	      SISFreeRec(pScrn);
              return FALSE;
	   }
        }
    }

    /* TW: Set the current layout parameters */
    pSiS->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pSiS->CurrentLayout.depth        = pScrn->depth;
    /* (Inside this function, we can use pScrn's contents anyway) */

    if(!xf86SetDefaultVisual(pScrn, -1)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86SetDefaultVisual() error\n");
#ifdef SISDUALHEAD
	if (pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
        SISFreeRec(pScrn);
        return FALSE;
    } else {
        /* We don't support DirectColor at > 8bpp */
        if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual "
                        "(%s) is not supported at depth %d\n",
                        xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
#ifdef SISDUALHEAD
	    if (pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	    if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	    SISFreeRec(pScrn);
            return FALSE;
        }
    }

    /*
     * The cmap layer needs this to be initialised.
     */
    {
        Gamma zeros = {0.0, 0.0, 0.0};

        if(!xf86SetGamma(pScrn, zeros)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86SetGamma() error\n");
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

    pSiS->ddc1Read = SiSddc1Read;

    from = X_DEFAULT;

    /* Unlock registers */
    sisSaveUnlockExtRegisterLock(pSiS, &srlockReg, &crlockReg);

    /* TW: We need no backup area (300/310/325 new mode switching code) */
    pSiS->sishw_ext.pSR = NULL;
    pSiS->sishw_ext.pCR = NULL;

    /* TW: Read BIOS for 300 and 310/325 series customization */
    pSiS->sishw_ext.pjVirtualRomBase = NULL;
    pSiS->BIOS = NULL;
    pSiS->sishw_ext.UseROM = FALSE;

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
#ifdef SISDUALHEAD
      if(pSiSEnt) {
          if(pSiSEnt->BIOS)  {
	  	pSiS->BIOS = pSiSEnt->BIOS;
		pSiS->sishw_ext.pjVirtualRomBase = pSiS->BIOS;
          }
      }
#endif
      if(!pSiS->BIOS) {
          if(!(pSiS->BIOS = xcalloc(1, BIOS_SIZE))) {
             xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Could not allocate memory for video BIOS image\n");
          } else {
	     unsigned long  segstart;
             unsigned short romptr;
	     BOOLEAN found;
             int  i;
             static const char sis_rom_sig[] = "Silicon Integrated Systems";
             static const char *sis_sig[10] = {
                  "300", "540", "630", "730",
		  "315", "315", "315", "5315", "6325",
		  "Xabre"
             };
	     static const unsigned short sis_nums[10] = {
	          SIS_300, SIS_540, SIS_630, SIS_730,
		  SIS_315PRO, SIS_315H, SIS_315, SIS_550, SIS_650,
		  SIS_330
	     };

	     found = FALSE;
             for(segstart=BIOS_BASE; segstart<0x000f0000; segstart+=0x00001000) {

#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
                if(xf86ReadBIOS(segstart, 0, pSiS->BIOS, BIOS_SIZE) != BIOS_SIZE) continue;
#else
                if(xf86ReadDomainMemory(pSiS->PciTag, segstart, BIOS_SIZE, pSiS->BIOS) != BIOS_SIZE) continue;
#endif

		if((pSiS->BIOS[0] != 0x55) || (pSiS->BIOS[1] != 0xaa)) continue;

		romptr = pSiS->BIOS[0x12] | (pSiS->BIOS[0x13] << 8);
		if(romptr > (BIOS_SIZE - strlen(sis_rom_sig))) continue;
                if(strncmp(sis_rom_sig, (char *)&pSiS->BIOS[romptr], strlen(sis_rom_sig)) != 0) continue;

		romptr = pSiS->BIOS[0x14] | (pSiS->BIOS[0x15] << 8);
		if(romptr > (BIOS_SIZE - 5)) continue;
		for(i = 0; (i < 10) && (!found); i++) {
                    if(strncmp(sis_sig[i], (char *)&pSiS->BIOS[romptr], strlen(sis_sig[i])) == 0) {
                        if(sis_nums[i] == pSiS->sishw_ext.jChipType) {
			   found = TRUE;
                           break;
			} else {
			   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   	"Ignoring BIOS for SiS %s at %p\n", sis_sig[i], segstart);
			}
                    }
                }
		if(found) break;
             }

	     if(!found) {
	     	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		         "Could not find/read video BIOS\n");
 	   	xfree(pSiS->BIOS);
	        pSiS->BIOS = NULL;
             } else {
#ifdef SISDUALHEAD
                if(pSiSEnt)  pSiSEnt->BIOS = pSiS->BIOS;
#endif
                pSiS->sishw_ext.pjVirtualRomBase = pSiS->BIOS;
		romptr = pSiS->BIOS[0x16] | (pSiS->BIOS[0x17] << 8);
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Video BIOS version \"%7s\" found at %p\n",
			&pSiS->BIOS[romptr], segstart);
             }
	     
          }
      }
      if(pSiS->BIOS) pSiS->sishw_ext.UseROM = TRUE;
      else           pSiS->sishw_ext.UseROM = FALSE;
    }

    /* Evaluate options */
    SiSOptions(pScrn);

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        if(!pSiS->SecondHead) {
	     /* TW: Copy some option settings to entity private */
             pSiSEnt->HWCursor = pSiS->HWCursor;
	     pSiSEnt->ForceCRT2Type = pSiS->ForceCRT2Type;
	     pSiSEnt->ForceTVType = pSiS->ForceTVType;
	     pSiSEnt->TurboQueue = pSiS->TurboQueue;
	     pSiSEnt->PDC = pSiS->PDC;
	     pSiSEnt->OptTVStand = pSiS->OptTVStand;
	     pSiSEnt->NonDefaultPAL = pSiS->NonDefaultPAL;
	     pSiSEnt->OptTVOver = pSiS->OptTVOver;
	     pSiSEnt->OptTVSOver = pSiS->OptTVSOver;
	     pSiSEnt->OptROMUsage = pSiS->OptROMUsage;
	     pSiSEnt->DSTN = pSiS->DSTN;
	     pSiSEnt->XvOnCRT2 = pSiS->XvOnCRT2;
	     pSiSEnt->NoAccel = pSiS->NoAccel;
	     pSiSEnt->NoXvideo = pSiS->NoXvideo;
	     pSiSEnt->forceCRT1 = pSiS->forceCRT1;
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
	     pSiSEnt->tvxpos = pSiS->tvxpos;
	     pSiSEnt->tvypos = pSiS->tvypos;
	     pSiSEnt->restorebyset = pSiS->restorebyset;
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
	     /* We need to use identical CRT2 Type setting */
	     if(pSiS->ForceCRT2Type != pSiSEnt->ForceCRT2Type) {
                  if(pSiS->ForceCRT2Type != CRT2_DEFAULT) {
		     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		  	"Ignoring inconsistent ForceCRT2Type setting. Master head rules\n");
	          }
	          pSiS->ForceCRT2Type = pSiSEnt->ForceCRT2Type;
	     }
	     if(pSiS->ForceTVType != pSiSEnt->ForceTVType) {
                  if(pSiS->ForceTVType != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		  	"Ignoring inconsistent ForceTVType setting. Master head rules\n");
	          }
	          pSiS->ForceTVType = pSiSEnt->ForceTVType;
	     }
	     /* We need identical TurboQueue setting */
	     if(pSiS->TurboQueue != pSiSEnt->TurboQueue) {
	          pSiS->TurboQueue = pSiSEnt->TurboQueue;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent TurboQueue setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: Turboqueue shall be %s\n",
			pSiS->TurboQueue ? "enabled" : "disabled");
	     }
	     /* We need identical PDC setting */
	     if(pSiS->PDC != pSiSEnt->PDC) {
	          pSiS->PDC = pSiSEnt->PDC;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent PanelDelayCompensation setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: PanelDelayCompensation shall be %d%s\n",
			pSiS->PDC,
			(pSiS->PDC == -1) ? " (autodetected)" : "");
	     }
	     /* We need identical TVStandard setting */
	     if( (pSiS->OptTVStand != pSiSEnt->OptTVStand) || 
	         (pSiS->NonDefaultPAL != pSiSEnt->NonDefaultPAL) ) {
                  if(pSiS->OptTVStand != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent TVStandard setting\n");
	             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: TVStandard shall be %s\n",
			(pSiSEnt->OptTVStand ? 
		          ( (pSiSEnt->NonDefaultPAL == -1) ? "PAL" : 
			      ((pSiSEnt->NonDefaultPAL) ? "PALM" : "PALN") )
				           : "NTSC"));
	          }
	          pSiS->OptTVStand = pSiSEnt->OptTVStand;
		  pSiS->NonDefaultPAL = pSiSEnt->NonDefaultPAL;
	     }
	     /* We need identical UseROMData setting */
	     if(pSiS->OptROMUsage != pSiSEnt->OptROMUsage) {
                  if(pSiS->OptROMUsage != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent UseROMData setting\n");
	             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: Video ROM data usage shall be %s\n",
			pSiSEnt->OptROMUsage ? "enabled" : "disabled");
	          }
	          pSiS->OptROMUsage = pSiSEnt->OptROMUsage;
	     }
	     /* We need identical DSTN setting */
	     if(pSiS->DSTN != pSiSEnt->DSTN) {
	          pSiS->DSTN = pSiSEnt->DSTN;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent DSTN setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: DSTN shall be %s\n",
			pSiS->DSTN ? "enabled" : "disabled");
	     }
	     /* We need identical XvOnCRT2 setting */
	     if(pSiS->XvOnCRT2 != pSiSEnt->XvOnCRT2) {
	          pSiS->XvOnCRT2 = pSiSEnt->XvOnCRT2;
		  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent XvOnCRT2 setting\n");
	          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: Xv shall be used on CRT%d\n",
			pSiS->XvOnCRT2 ? 2 : 1);
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
	     /* We need identical ForceCRT1 setting */
	     if(pSiS->forceCRT1 != pSiSEnt->forceCRT1) {
	          if(pSiS->forceCRT1 != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent ForceCRT1 setting\n");
	             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: CRT1 shall be %s\n",
			pSiSEnt->forceCRT1 ? "enabled" : "disabled");
	          }
	          pSiS->forceCRT1 = pSiSEnt->forceCRT1;
	     }
	     /* We need identical TVOverscan setting */
	     if(pSiS->OptTVOver != pSiSEnt->OptTVOver) {
                  if(pSiS->OptTVOver != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  	"Ignoring inconsistent CHTVOverscan setting\n");
	             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: CHTVOverscan shall be %s\n",
			pSiSEnt->OptTVOver ? "true (=overscan)" : "false (=underscan)");
	          }
	          pSiS->OptTVOver = pSiSEnt->OptTVOver;
	     }
	     /* We need identical TVSOverscan setting */
	     if(pSiS->OptTVSOver != pSiSEnt->OptTVSOver) {
	          if(pSiS->OptTVSOver != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVSuperOverscan setting\n");
	 	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        "Master head ruled: CHTVSuperOverscan shall be %s\n",
			pSiSEnt->OptTVSOver ? "true" : "false");
		  }
	          pSiS->OptTVSOver = pSiSEnt->OptTVSOver;
	     }
	     /* We need identical TV settings */
	     if(pSiS->chtvtype != pSiSEnt->chtvtype) {
	        if(pSiS->chtvtype != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVType setting; set to %s\n",
			(pSiSEnt->chtvtype) ? "SCART" : "HDTV");
		}
		pSiS->chtvtype = pSiSEnt->chtvtype;
	     }
             if(pSiS->chtvlumabandwidthcvbs != pSiSEnt->chtvlumabandwidthcvbs) {
	        if(pSiS->chtvlumabandwidthcvbs != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVLumaBandWidthCVBS setting; set to %d\n",
			pSiSEnt->chtvlumabandwidthcvbs);
		}
		pSiS->chtvlumabandwidthcvbs = pSiSEnt->chtvlumabandwidthcvbs;
	     }
	     if(pSiS->chtvlumabandwidthsvideo != pSiSEnt->chtvlumabandwidthsvideo) {
	        if(pSiS->chtvlumabandwidthsvideo != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVLumaBandWidthSVIDEO setting; set to %d\n",
			pSiSEnt->chtvlumabandwidthsvideo);
		}
		pSiS->chtvlumabandwidthsvideo = pSiSEnt->chtvlumabandwidthsvideo;
	     }
	     if(pSiS->chtvlumaflickerfilter != pSiSEnt->chtvlumaflickerfilter) {
	        if(pSiS->chtvlumaflickerfilter != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVLumaFlickerFilter setting; set to %d\n",
			pSiSEnt->chtvlumaflickerfilter);
		}
		pSiS->chtvlumaflickerfilter = pSiSEnt->chtvlumaflickerfilter;
	     }
	     if(pSiS->chtvchromabandwidth != pSiSEnt->chtvchromabandwidth) {
	        if(pSiS->chtvchromabandwidth != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVChromaBandWidth setting; set to %d\n",
			pSiSEnt->chtvchromabandwidth);
		}
		pSiS->chtvchromabandwidth = pSiSEnt->chtvchromabandwidth;
	     }
	     if(pSiS->chtvchromaflickerfilter != pSiSEnt->chtvchromaflickerfilter) {
	        if(pSiS->chtvchromaflickerfilter != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVChromaFlickerFilter setting; set to %d\n",
			pSiSEnt->chtvchromaflickerfilter);
		}
		pSiS->chtvchromaflickerfilter = pSiSEnt->chtvchromaflickerfilter;
	     }
	     if(pSiS->chtvcvbscolor != pSiSEnt->chtvcvbscolor) {
	        if(pSiS->chtvcvbscolor != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVCVBSColor setting; set to %s\n",
			pSiSEnt->chtvcvbscolor ? "true" : "false");
		}
		pSiS->chtvcvbscolor = pSiSEnt->chtvcvbscolor;
	     }
	     if(pSiS->chtvtextenhance != pSiSEnt->chtvtextenhance) {
	        if(pSiS->chtvtextenhance != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVTextEnhance setting; set to %d\n",
			pSiSEnt->chtvtextenhance);
		}
		pSiS->chtvtextenhance = pSiSEnt->chtvtextenhance;
	     }
	     if(pSiS->chtvcontrast != pSiSEnt->chtvcontrast) {
	        if(pSiS->chtvcontrast != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent CHTVContrast setting; set to %d\n",
			pSiSEnt->chtvcontrast);
		}
		pSiS->chtvcontrast = pSiSEnt->chtvcontrast;
	     }
	     if(pSiS->sistvedgeenhance != pSiSEnt->sistvedgeenhance) {
	        if(pSiS->sistvedgeenhance != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent SISTVEdgeEnhance setting; set to %d\n",
			pSiSEnt->sistvedgeenhance);
		}
		pSiS->sistvedgeenhance = pSiSEnt->sistvedgeenhance;
	     }
	     if(pSiS->sistvantiflicker != pSiSEnt->sistvantiflicker) {
	        if(pSiS->sistvantiflicker != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent SISTVAntiFlicker setting; set to %d\n",
			pSiSEnt->sistvantiflicker);
		}
		pSiS->sistvantiflicker = pSiSEnt->sistvantiflicker;
	     }
	     if(pSiS->sistvsaturation != pSiSEnt->sistvsaturation) {
	        if(pSiS->sistvsaturation != -1) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent SISTVSaturation setting; set to %d\n",
			pSiSEnt->sistvsaturation);
		}
		pSiS->sistvsaturation = pSiSEnt->sistvsaturation;
	     }
	     if(pSiS->tvxpos != pSiSEnt->tvxpos) {
	        if(pSiS->tvxpos != 0) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent TVXPosOffset setting; set to %d\n",
			pSiSEnt->tvxpos);
		}
		pSiS->tvxpos = pSiSEnt->tvxpos;
	     }
	     if(pSiS->tvypos != pSiSEnt->tvypos) {
	        if(pSiS->tvypos != 0) {
		     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		     	"Ignoring inconsistent TVYPosOffset setting; set to %d\n",
			pSiSEnt->tvypos);
		}
		pSiS->tvypos = pSiSEnt->tvypos;
	     }
	     if(pSiS->restorebyset != pSiSEnt->restorebyset) {
		pSiS->restorebyset = pSiSEnt->restorebyset;
	     }
	}
    }
#endif
    /* TW: Handle UseROMData and NoOEM options */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       from = X_PROBED;
       if(pSiS->OptROMUsage == 0)  {
       		pSiS->sishw_ext.UseROM = FALSE;
		from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "Video ROM data usage is %s\n",
    	   pSiS->sishw_ext.UseROM ? "enabled" : "disabled");

       if(!pSiS->OptUseOEM)
          xf86DrvMsg(pScrn->scrnIndex, from, "Internal OEM LCD/TV data usage is disabled\n");

       if(pSiS->sbiosn) {
         if(pSiS->BIOS) {
           FILE *fd = NULL;
	   int i;
           if((fd = fopen(pSiS->sbiosn, "w" ))) {
	     i = fwrite(pSiS->BIOS, 65536, 1, fd);
	     fclose(fd);
	   }
         }
         xfree(pSiS->sbiosn);
       }
    }

    /* Do basic configuration */
    SiSSetup(pScrn);

    from = X_PROBED;
    if (pSiS->pEnt->device->MemBase != 0) {
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

    if (pSiS->pEnt->device->IOBase != 0) {
        /*
         * XXX Should check that the config file value matches one of the
         * PCI base address values.
         */
        pSiS->IOAddress = pSiS->pEnt->device->IOBase;
        from = X_CONFIG;
    } else {
        pSiS->IOAddress = pSiS->PciInfo->memBase[1] & 0xFFFFFFF0;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
           (unsigned long)pSiS->IOAddress);
    pSiS->sishw_ext.bIntegratedMMEnabled = TRUE;

    /* Register the PCI-assigned resources. */
    if(xf86RegisterResources(pSiS->pEnt->index, NULL, ResExclusive)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
               "xf86RegisterResources() found resource conflicts\n");
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
	SISFreeRec(pScrn);
        return FALSE;
    }

    from = X_PROBED;
    if (pSiS->pEnt->device->videoRam != 0)  {
        pScrn->videoRam = pSiS->pEnt->device->videoRam;
        from = X_CONFIG;
    }

    pSiS->RealVideoRam = pScrn->videoRam;
    if((pSiS->Chipset == PCI_CHIP_SIS6326)
			&& (pScrn->videoRam > 4096)
			&& (from != X_CONFIG)) {
        pScrn->videoRam = 4096;
        xf86DrvMsg(pScrn->scrnIndex, from,
	       "SiS6326: Detected %d KB VideoRAM, limiting to %d KB\n",
               pSiS->RealVideoRam, pScrn->videoRam);
    } else
        xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %d KB\n",
               pScrn->videoRam);

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

    /* TW: Calculate real availMem according to Accel/TurboQueue and
     *     HWCursur setting. Also, initialize some variables used
     *     in other modules.
     */
    pSiS->cursorOffset = 0;
    switch (pSiS->VGAEngine) {
      case SIS_300_VGA:
      	pSiS->TurboQueueLen = 512;
       	if(pSiS->TurboQueue) {
			      pSiS->availMem -= (pSiS->TurboQueueLen*1024);
			      pSiS->cursorOffset = 512;
        }
	if(pSiS->HWCursor)   {
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
       	if(pSiS->TurboQueue) {
			      pSiS->availMem -= (512*1024);  		/* Command Queue is 512k */
			      pSiS->cursorOffset = 512;
	}
	if(pSiS->HWCursor)   {
			      pSiS->availMem -= pSiS->CursorSize;
			      if(pSiS->OptUseColorCursor) pSiS->availMem -= pSiS->CursorSize;
	}
	pSiS->cursorBufferNum = 0;
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->cursorBufferNum = 0;
#endif	
	break;
      default:
        /* TW: cursorOffset not used in cursor functions for 530 and
	 *     older chips, because the cursor is *above* the TQ.
	 *     On 5597 and older revisions of the 6326, the TQ is
	 *     max 32K, on newer 6326 revisions and the 530 either 30
	 *     (or 32?) or 62K (or 64?). However, to make sure, we
	 *     use only 30K (or 32?), but reduce the available memory
	 *     by 64, and locate the TQ at the beginning of this last
	 *     64K block. (We do this that way even when using the
	 *     HWCursor, because the cursor only takes 2K, and the queue
	 *     does not seem to last that far anyway.)
	 *     The TQ must be located at 32KB boundaries.
	 */
	if(pSiS->RealVideoRam < 3072) {
	        if(pSiS->TurboQueue) {
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			    "Not enough video RAM for TurboQueue. TurboQueue disabled\n");
		}
		pSiS->TurboQueue = FALSE;
	}
	pSiS->CmdQueMaxLen = 32;
     	if(pSiS->TurboQueue) {
	              	      pSiS->availMem -= (64*1024);
			      pSiS->CmdQueMaxLen = 900;   /* TW: To make sure; should be 992 */
	} else if (pSiS->HWCursor) {
	                      pSiS->availMem -= pSiS->CursorSize;
	}
	if(pSiS->Chipset == PCI_CHIP_SIS530) {
		/* TW: Check if Flat Panel is enabled */
		inSISIDXREG(SISSR, 0x0e, tempreg);
		if(!tempreg & 0x04) pSiS->availMem -= pSiS->CursorSize;

		/* TW: Set up mask for MMIO register */
		pSiS->CmdQueLenMask = (pSiS->TurboQueue) ? 0x1FFF : 0x00FF;
	} else {
	        /* TW: TQ is never used on 6326/5597, because the accelerator
		 *     always Syncs. So this is just cosmentic work. (And I
		 *     am not even sure that 0x7fff is correct. MMIO 0x83a8
		 *     holds 0xec0 if (30k) TQ is enabled, 0x20 if TQ disabled.
		 *     The datasheet has no real explanation on the queue length
		 *     if the TQ is enabled. Not syncing and waiting for a
		 *     suitable queue length instead does not work.
		 */
	        pSiS->CmdQueLenMask = (pSiS->TurboQueue) ? 0x7FFF : 0x003F;
	}

	/* TW: This is to be subtracted from MMIO queue length register contents
	 *     for getting the real Queue length.
	 */
	pSiS->CmdQueLenFix  = (pSiS->TurboQueue) ? 32 : 0;
    }

#ifdef SISDUALHEAD
    /* TW: In dual head mode, we share availMem equally - so align it
     *     to 8KB; this way, the address of the FB of the second
     *     head is aligned to 4KB for mapping.
     */
   if (pSiS->DualHeadMode)
	pSiS->availMem &= 0xFFFFE000;
#endif

    /* TW: Check MaxXFBMem setting */
#ifdef SISDUALHEAD
    /* TW: Since DRI is not supported in dual head mode, we
           don't need MaxXFBMem setting. */
    if (pSiS->DualHeadMode) {
        if(pSiS->maxxfbmem) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"MaxXFBMem not used in Dual Head mode. Using all VideoRAM.\n");
        }
	pSiS->maxxfbmem = pSiS->availMem;
    } else
#endif
      if (pSiS->maxxfbmem) {
    	if (pSiS->maxxfbmem > pSiS->availMem) {
	    if (pSiS->sisfbMem) {
	       pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
	       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                "Invalid MaxXFBMem setting. Using sisfb heap start information\n");
	    } else {
	       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                "Invalid MaxXFBMem setting. Using all VideoRAM for framebuffer\n");
	       pSiS->maxxfbmem = pSiS->availMem;
	    }
	} else if (pSiS->sisfbMem) {
	   if (pSiS->maxxfbmem > pSiS->sisfbMem * 1024) {
	       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "MaxXFBMem beyond sisfb heap start. Using sisfb heap start information\n");
               pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
	   }
	}
    } else if (pSiS->sisfbMem) {
         pSiS->maxxfbmem = pSiS->sisfbMem * 1024;
    }
    else pSiS->maxxfbmem = pSiS->availMem;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using %dK of framebuffer memory\n",
    	pSiS->maxxfbmem / 1024);

    /* TW: Check if the chipset supports two video overlays */
    pSiS->Flags650 = 0;
    if ( (!pSiS->NoXvideo) &&
            ( pSiS->VGAEngine == SIS_300_VGA ||
              pSiS->VGAEngine == SIS_315_VGA ||
	      pSiS->Chipset == PCI_CHIP_SIS530 ||
	      pSiS->Chipset == PCI_CHIP_SIS6326 ||
	      pSiS->Chipset == PCI_CHIP_SIS5597 ) ) {
       pSiS->hasTwoOverlays = FALSE;
       switch (pSiS->Chipset) {
         case PCI_CHIP_SIS300:
         case PCI_CHIP_SIS630:
         case PCI_CHIP_SIS550: 
         case PCI_CHIP_SIS330:  /* ? */
           pSiS->hasTwoOverlays = TRUE;
	   break;
         case PCI_CHIP_SIS650:
	   {
	     static const char *id650str[] = {
	   	"0",       "0",        "0",       "0",
		"0 A0 AA", "0 A2 CA",  "0",       "0",
		"0M A0",   "0M A1 AA", "1 A0 AA", "1 A1 AA"
		"0",       "0",        "0",       "0"
	     };
             inSISIDXREG(SISCR, 0x5F, CR5F);
	     CR5F &= 0xf0;
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	"SiS650 revision ID %x (SiS65%s)\n", CR5F, id650str[CR5F >> 4]);
	     if((CR5F == 0x80) || (CR5F == 0x90) || (CR5F == 0xa0) || (CR5F == 0xb0)) {
	        pSiS->hasTwoOverlays = TRUE;  /* TW: This is an M650 or 651 */
		pSiS->Flags650 |= SiS650_LARGEOVERLAY;
	     }
             break;
	   }
       }
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Hardware supports %s video overlay%s\n",
		pSiS->hasTwoOverlays ? "two" : "one",
		pSiS->hasTwoOverlays ? "s" : "");
    }

    /* TW: Backup VB connection and CRT1 on/off register */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
    	inSISIDXREG(SISCR, 0x32, pSiS->oldCR32);
	inSISIDXREG(SISCR, 0x17, pSiS->oldCR17);
	pSiS->postVBCR32 = pSiS->oldCR32;
    }

    if(pSiS->forceCRT1 != -1) {
        if(pSiS->forceCRT1) pSiS->CRT1off = 0;
	else                pSiS->CRT1off = 1;
    } else                  pSiS->CRT1off = -1;

    /* TW: There are some strange machines out there which require a special
     *     manupulation of ISA bridge registers in order to make the Chrontel
     *     work. Try to find out if we're running on such a machine.
     */
    pSiS->SiS_Pr->SiS_ChSW = FALSE;
    if(pSiS->Chipset == PCI_CHIP_SIS630) {
        int i=0;
        do {
	    if(mychswtable[i].subsysVendor == pSiS->PciInfo->subsysVendor &&
	       mychswtable[i].subsysCard == pSiS->PciInfo->subsysCard) {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "PCI card/vendor found in list for Chrontel/ISA bridge poking\n");
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		   "Vendor: %s (ID %04x)\n",
		   mychswtable[i].vendorName, pSiS->PciInfo->subsysCard);
		pSiS->SiS_Pr->SiS_ChSW = TRUE;
            }
            i++;
        } while(mychswtable[i].subsysVendor != 0);
    }

    /* TW: Detect video bridge and sense connected devices */
    SISVGAPreInit(pScrn);
    /* TW: Detect CRT1  */
    SISCRT1PreInit(pScrn);
    /* TW: Detect CRT2-LCD and LCD size */
    SISLCDPreInit(pScrn);
    /* TW: Detect CRT2-TV and PAL/NTSC mode */
    SISTVPreInit(pScrn);
    /* TW: Detect CRT2-VGA */
    SISCRT2PreInit(pScrn);

    /* TW: Backup detected CRT2 devices */
    pSiS->detectedCRT2Devices = pSiS->VBFlags & (CRT2_LCD | CRT2_TV | CRT2_VGA);

    /* TW: Eventually overrule detected CRT2 type */
    if(pSiS->ForceCRT2Type == CRT2_DEFAULT) {
        if(pSiS->VBFlags & CRT2_VGA)
           pSiS->ForceCRT2Type = CRT2_VGA;
        else if(pSiS->VBFlags & CRT2_LCD)
           pSiS->ForceCRT2Type = CRT2_LCD;
        else if(pSiS->VBFlags & CRT2_TV)
           pSiS->ForceCRT2Type = CRT2_TV;
    }

    switch(pSiS->ForceCRT2Type) {
      case CRT2_TV:
        pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_LCD | CRT2_VGA);
        if(pSiS->VBFlags & VB_VIDEOBRIDGE)
            pSiS->VBFlags = pSiS->VBFlags | CRT2_TV;
        else
            pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_TV);
        break;
      case CRT2_LCD:
        pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_TV | CRT2_VGA);
        if((pSiS->VBFlags & VB_VIDEOBRIDGE) && (pSiS->VBLCDFlags))
            pSiS->VBFlags = pSiS->VBFlags | CRT2_LCD;
        else {
            pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_LCD);
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Can't force CRT2 to LCD, no panel detected\n");
	}
        break;
      case CRT2_VGA:
        if(pSiS->VBFlags & VB_LVDS) {
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	      "LVDS does not support secondary VGA\n");
	   break;
	}
	if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	      "SiS30xLV bridge does not support secondary VGA\n");
	   break;
	}
        pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_TV | CRT2_LCD);
        if(pSiS->VBFlags & VB_VIDEOBRIDGE)
            pSiS->VBFlags = pSiS->VBFlags | CRT2_VGA;
        else
            pSiS->VBFlags = pSiS->VBFlags & ~(CRT2_VGA);
        break;
      default:
        pSiS->VBFlags &= ~(CRT2_TV | CRT2_LCD | CRT2_VGA);
    }

    /* TW: Eventually overrule TV Type (SVIDEO, COMPOSITE, SCART) */
    if(pSiS->ForceTVType != -1) {
        if(pSiS->VBFlags & VB_SISBRIDGE) {
    	   pSiS->VBFlags &= ~(TV_INTERFACE);
	   pSiS->VBFlags |= pSiS->ForceTVType;
	}
    }

    /* TW: Handle ForceCRT1 option */
    pSiS->CRT1changed = FALSE;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       usScratchCR17 = pSiS->oldCR17;
       usScratchCR32 = pSiS->postVBCR32;
       if(pSiS->VESA != 1) {
          /* TW: Copy forceCRT1 option to CRT1off if option is given */
#ifdef SISDUALHEAD
          /* TW: In DHM, handle this option only for master head, not the slave */
          if( (pSiS->forceCRT1 != -1) &&
	       (!(pSiS->DualHeadMode && pSiS->SecondHead)) ) {
#else
          if(pSiS->forceCRT1 != -1) {
#endif
             xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "CRT1 detection overruled by ForceCRT1 option\n");
    	     if(pSiS->forceCRT1) {
		 pSiS->CRT1off = 0;
		 if (!(usScratchCR17 & 0x80)) pSiS->CRT1changed = TRUE;
		 usScratchCR17 |= 0x80;
		 usScratchCR32 |= 0x20;
	     } else {
	         if( ! ( (pScrn->bitsPerPixel == 8) &&
		         ( (pSiS->VBFlags & VB_LVDS) ||
		           ((pSiS->VGAEngine == SIS_300_VGA) && (pSiS->VBFlags & VB_301B)) ) ) ) {
		    pSiS->CRT1off = 1;
		    if (usScratchCR17 & 0x80) pSiS->CRT1changed = TRUE;
		    usScratchCR32 &= ~0x20;
		    /* TW: We must not actually switch off CRT1 before we changed the mode! */
		 }
	     }
	     outSISIDXREG(SISCR, 0x17, usScratchCR17);
	     outSISIDXREG(SISCR, 0x32, usScratchCR32);
	     if(pSiS->CRT1changed) {
                outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
	        usleep(10000);
                outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   		"CRT1 status changed by ForceCRT1 option\n");
	     }
          }
       }
       /* TW: Store the new VB connection register contents for later mode changes */
       pSiS->newCR32 = usScratchCR32;
    }

    /* TW: Check if CRT1 used (or needed; this eg. if no CRT2 detected) */
    if (pSiS->VBFlags & VB_VIDEOBRIDGE) {
    
        /* TW: No CRT2 output? Then we NEED CRT1!
	 *     We also need CRT1 if depth = 8 and bridge=LVDS|630+301B
	 */
        if ( (!(pSiS->VBFlags & (CRT2_VGA | CRT2_LCD | CRT2_TV))) ||
	     ( (pScrn->bitsPerPixel == 8) &&
	       ( (pSiS->VBFlags & (VB_LVDS | VB_CHRONTEL)) ||
	         ((pSiS->VGAEngine == SIS_300_VGA) && (pSiS->VBFlags & VB_301B)) ) ) ) {
	    pSiS->CRT1off = 0;
	}
	/* TW: No CRT2 output? Then we can't use Xv on CRT2 */
	if (!(pSiS->VBFlags & (CRT2_VGA | CRT2_LCD | CRT2_TV)))
	    pSiS->XvOnCRT2 = FALSE;

    } else { /* TW: no video bridge? */

        /* Then we NEED CRT1... */
        pSiS->CRT1off = 0;
	/* ... and can't use CRT2 for Xv output */
	pSiS->XvOnCRT2 = FALSE;
    }

    /* TW: Handle TVStandard option */
    if(pSiS->NonDefaultPAL != -1) {
        if( (!(pSiS->VBFlags & VB_SISBRIDGE)) &&
	    (!((pSiS->VBFlags & VB_CHRONTEL)) && (pSiS->ChrontelType == CHRONTEL_701x)) ) {
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	"PALM and PALN only supported on Chrontel 701x and SiS30x/B/LV\n");
 	   pSiS->NonDefaultPAL = -1; 
	   pSiS->VBFlags &= ~(TV_PALN | TV_PALM);
	}
    }
    if(pSiS->NonDefaultPAL != -1) {
        if((pSiS->Chipset == PCI_CHIP_SIS300) || (pSiS->Chipset == PCI_CHIP_SIS540)) {
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	"PALM and PALN not supported on SiS300 and SiS540\n");
	   pSiS->NonDefaultPAL = -1; 
	   pSiS->VBFlags &= ~(TV_PALN | TV_PALM);
	}
    }
    if(pSiS->OptTVStand != -1) {
        if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	   if(!(pSiS->Flags & (TV_CHSCART | TV_CHHDTV))) {
    	      pSiS->VBFlags &= ~(TV_PAL | TV_NTSC | TV_PALN | TV_PALM);
    	      if(pSiS->OptTVStand) pSiS->VBFlags |= TV_PAL;
	      else                 pSiS->VBFlags |= TV_NTSC;
              if(pSiS->NonDefaultPAL == 1)  pSiS->VBFlags |= TV_PALM;
	      else if(!pSiS->NonDefaultPAL) pSiS->VBFlags |= TV_PALN;
	   } else {
	      pSiS->OptTVStand = pSiS->NonDefaultPAL = -1;
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	"Option TVStandard ignored for SCART and 480i HDTV\n");
	   }
	} else if(pSiS->Chipset == PCI_CHIP_SIS6326) {
	   pSiS->SiS6326Flags &= ~SIS6326_TVPAL;
	   if(pSiS->OptTVStand) pSiS->SiS6326Flags |= SIS6326_TVPAL;
	}
    }

    /* TW: Do some checks */
    if(pSiS->OptTVOver != -1) {
        if(pSiS->VBFlags & VB_CHRONTEL) {
	   pSiS->UseCHOverScan = pSiS->OptTVOver;
	} else {
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
	    	"CHTVOverscan option only supported on CHRONTEL 70xx\n");
           pSiS->UseCHOverScan = -1;
	}
    } else pSiS->UseCHOverScan = -1;
    
    if(pSiS->sistvedgeenhance != -1) {
        if(!(pSiS->VBFlags & VB_301)) {
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
	       "SISTVEdgeEnhance option only supported on SiS301\n");
	   pSiS->sistvedgeenhance = -1;
	}
    }

    /* TW: Determine CRT1<>CRT2 mode
     *     Note: When using VESA or if the bridge is in slavemode, display
     *           is ALWAYS in MIRROR_MODE!
     *           This requires extra checks in functions using this flag!
     *           (see sis_video.c for example)
     */
    if(pSiS->VBFlags & DISPTYPE_DISP2) {
        if(pSiS->CRT1off) {	/* TW: CRT2 only ------------------------------- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
	     	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "CRT1 not detected or forced off. Dual Head mode can't initialize.\n");
	     	if(pSiSEnt) pSiSEnt->DisableDual = TRUE;
	        if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
		if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
		sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
		SISFreeRec(pScrn);
		return FALSE;
	     }
#endif
	     pSiS->VBFlags |= VB_DISPMODE_SINGLE;
	     /* TW: No CRT1? Then we use the video overlay on CRT2 */
	     pSiS->XvOnCRT2 = TRUE;
	} else			/* TW: CRT1 and CRT2 - mirror or dual head ----- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
		pSiS->VBFlags |= (VB_DISPMODE_DUAL | DISPTYPE_CRT1);
	        if(pSiS->VESA != -1) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"VESA option not used in Dual Head mode. VESA disabled.\n");
		}
		if (pSiSEnt) pSiSEnt->DisableDual = FALSE;
		pSiS->VESA = 0;
	     } else
#endif
		 pSiS->VBFlags |= (VB_DISPMODE_MIRROR | DISPTYPE_CRT1);
    } else {			/* TW: CRT1 only ------------------------------- */
#ifdef SISDUALHEAD
	     if(pSiS->DualHeadMode) {
	     	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "No CRT2 output selected or no bridge detected. "
		   "Dual Head mode can't initialize.\n");
	        if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
		if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
		sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
		SISFreeRec(pScrn);
		return FALSE;
	     }
#endif
             pSiS->VBFlags |= (VB_DISPMODE_SINGLE | DISPTYPE_CRT1);
    }

    if( (pSiS->VGAEngine == SIS_315_VGA) ||
        (pSiS->VGAEngine == SIS_300_VGA) ) {
       if ( (!pSiS->NoXvideo) &&
         (!pSiS->hasTwoOverlays) ) {
	       xf86DrvMsg(pScrn->scrnIndex, from,
	  		"Using Xv overlay on CRT%d\n",
			pSiS->XvOnCRT2 ? 2 : 1);
       }
    }

    /* TW: Init Ptrs for Save/Restore functions and calc MaxClock */
    SISDACPreInit(pScrn);

    /* ********** end of VBFlags setup ********** */

    /* TW: VBFlags are initialized now. Back them up for SlaveMode modes. */
    pSiS->VBFlags_backup = pSiS->VBFlags;

    /* TW: Find out about paneldelaycompensation and evaluate option */
    pSiS->sishw_ext.pdc = 0;

    if(pSiS->VGAEngine == SIS_300_VGA) {

        if(pSiS->VBFlags & (VB_LVDS | VB_301B | VB_302B)) {
	   /* TW: Save the current PDC if the panel is used at the moment.
	    *     This seems by far the safest way to find out about it.
	    *     If the system is using an old version of sisfb, we can't
	    *     trust the pdc register value. If sisfb saved the pdc for
	    *     us, use it.
	    */
	   if(pSiS->sisfbpdc) {
	      pSiS->sishw_ext.pdc = pSiS->sisfbpdc;
	   } else {
	     if(!(pSiS->donttrustpdc)) {
	       unsigned char tmp;
	       inSISIDXREG(SISCR, 0x30, tmp);
	       if(tmp & 0x20) {
	         inSISIDXREG(SISPART1, 0x13, pSiS->sishw_ext.pdc);
               } else {
	         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       	     "Unable to detect LCD PanelDelayCompensation, LCD is not active\n");
	       }
	     } else {
	       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       	  "Unable to detect LCD PanelDelayCompensation, please update sisfb\n");
	     }
	   }
	   pSiS->sishw_ext.pdc &= 0x3c;
	   if(pSiS->sishw_ext.pdc) {
	      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	       	  "Detected LCD PanelDelayCompensation %d\n",
		  pSiS->sishw_ext.pdc);
	   }

	   /* If we haven't been able to find out, use our other methods */
	   if(pSiS->sishw_ext.pdc == 0) {

                 int i=0;
                 do {
	            if(mypdctable[i].subsysVendor == pSiS->PciInfo->subsysVendor &&
	               mypdctable[i].subsysCard == pSiS->PciInfo->subsysCard) {
	                  xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   	            "PCI card/vendor found in list for non-default PanelDelayCompensation\n");
		          xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		             "Vendor: %s, card: %s (ID %04x), PanelDelayCompensation: %d\n",
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
		             "BIOS contains custom LCD Panel Delay Compensation %d\n",
		             pSiS->BIOS[0x220] & 0x3c);
	                pSiS->BIOS[0x220] &= 0x7f;
		     }
	          }
	          if(pSiS->VBFlags & (VB_301B|VB_302B)) {
	             if(pSiS->BIOS[0x220] & 0x80) {
                        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		             "BIOS contains custom LCD Panel Delay Compensation %d\n",
		               (  (pSiS->VBLCDFlags & VB_LCD_1280x1024) ?
			                 pSiS->BIOS[0x223] : pSiS->BIOS[0x224]  ) & 0x3c);
	                pSiS->BIOS[0x220] &= 0x7f;
		     }
		  }
	       }
	       pSiS->sishw_ext.pdc = (pSiS->PDC & 0x3c);
	       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	            "Using LCD Panel Delay Compensation %d\n", pSiS->PDC);
	    }
	}
    }

#ifdef SISDUALHEAD
    /* TW: In dual head mode, both heads (currently) share the maxxfbmem equally.
     *     If memory sharing is done differently, the following has to be changed;
     *     the other modules (eg. accel and Xv) use dhmOffset for hardware
     *     pointer settings relative to VideoRAM start and won't need to be changed.
     */
    if (pSiS->DualHeadMode) {
        if (pSiS->SecondHead == FALSE) {
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
	    		"%dKB video RAM at 0x%lx available for master head (CRT2)\n",
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
	    		"%dKB video RAM at 0x%lx available for slave head (CRT1)\n",
	    		pSiS->maxxfbmem/1024,  pSiS->FbAddress);
	}
    } else
        pSiS->dhmOffset = 0;
#endif

    /* TW: Note: Do not use availMem for anything from now. Use
     *     maxxfbmem instead. (availMem does not take dual head
     *     mode into account.)
     */

    /* TW: Now for something completely different: DDC.
           For 300 and 310/325 series, we provide our
	   own functions (in order to probe CRT2 as well)
	   If these fail, use the VBE.
	   All other chipsets will use VBE. No need to re-invent
	   the wheel there.
     */

    pSiS->pVbe = NULL;
    didddc2 = FALSE;

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	if(xf86LoadSubModule(pScrn, "ddc")) {
          xf86LoaderReqSymLists(ddcSymbols, NULL);
	  if((pMonitor = SiSDoPrivateDDC(pScrn))) {
	     didddc2 = TRUE;
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	"DDC monitor info:\n");
	     xf86PrintEDID(pMonitor);
	     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	"End of DDC monitor info\n");
	     xf86SetDDCproperties(pScrn, pMonitor);
	     pScrn->monitor->DDC = pMonitor;
          }
	}
    }

#ifdef SISDUALHEAD
    /* TW: In dual head mode, probe DDC using VBE only for CRT1 (second head) */
    if((pSiS->DualHeadMode) && (!didddc2) && (!pSiS->SecondHead))
         didddc2 = TRUE;
#endif

    /* TW: If CRT1 is off (eventually forced), skip DDC */
    if((!didddc2) && (pSiS->CRT1off)) didddc2 = TRUE;

    /* TW: Now (re-)load and initialize the DDC module */
    if(!didddc2) {

       if(xf86LoadSubModule(pScrn, "ddc")) {

          xf86LoaderReqSymLists(ddcSymbols, NULL);

          /* TW: Now load and initialize VBE module. */
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
	   	      "VBE DDC monitor info:\n");
                 xf86SetDDCproperties(pScrn, xf86PrintEDID(pMonitor));
		 xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	   	      "End of VBE DDC monitor info:\n");
		 pScrn->monitor->DDC = pMonitor;
              }
          } else {
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Could not retrieve DDC data\n");
	  }
       }
    }

#if 0	/* TW: DDC1 obviously no longer supported by SiS chipsets */
    if (!ret && pSiS->ddc1Read)
        xf86SetDDCproperties(pScrn, xf86PrintEDID(xf86DoEDID_DDC1(
             pScrn->scrnIndex,vgaHWddc1SetSpeed,pSiS->ddc1Read )));
#endif

    /* end of DDC */

    /* Set the min pixel clock */
    pSiS->MinClock = 12000;  /* XXX Guess, need to check this (TW: good for even 50Hz interlace) */
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

    /* TW: If there is no HSync or VRefresh data for the monitor,
           derive it from DDC data. (Idea taken from radeon driver)
     */
    if(pScrn->monitor->DDC) {
       if(pScrn->monitor->nHsync <= 0) {
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	 	"Substituting missing monitor HSync data by DDC data\n");
         SiSSetSyncRangeFromEdid(pScrn, 1);
       }
       if(pScrn->monitor->nVrefresh <= 0) {
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	 	"Substituting missing monitor VRefresh data by DDC data\n");
         SiSSetSyncRangeFromEdid(pScrn, 0);
       }
    }

    /*
     * TW: Since we have lots of built-in modes for 300/310/325/330 series
     *     with vb support, we replace the given default mode list with our 
     *     own. In case the video bridge is to be used, no other than our 
     *     built-in modes are supported; therefore, delete the entire modelist
     *     given.
     */
     
    pSiS->HaveCustomModes = FALSE;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
      if(!(pSiS->noInternalModes)) {
        if((mymodes = SiSBuildBuiltInModeList(pScrn))) {
#ifdef SISDUALHEAD
    	   if( (pSiS->UseVESA) || 
	       ((pSiS->DualHeadMode) && (!pSiS->SecondHead)) ||
	       ((!pSiS->DualHeadMode) && (pSiS->VBFlags & DISPTYPE_DISP2)) ) {
#else	
	   if((pSiS->UseVESA) || (pSiS->VBFlags & DISPTYPE_DISP2)) {
#endif	   
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
	      if(tempmode) pSiS->HaveCustomModes = TRUE;
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
      }
    }

    /*
     * TW: Add our built-in modes for TV on the 6326
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
	     	"\"PAL800x600\" \"PAL800x600U\" \"PAL720x540\" \"PAL640x480\"\n");
	 } else {
	    SiS6326NTSC640x480Mode.next = pScrn->monitor->Modes;
            pScrn->monitor->Modes = &SiS6326NTSC640x400Mode;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	     	"\"NTSC640x480\" \"NTSC640x480U\" \"NTSC640x400\"\n");
	 }
      }
    }

    /*
     * TW: Add our built-in hi-res modes on the 6326
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
     * Assuming min pitch 256, max 4096 ==> 8192
     * Assuming min height 128, max 4096
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                      pScrn->display->modes, clockRanges,
                      NULL, 256, 8192,
                      pScrn->bitsPerPixel * 8, 128, 4096,
                      pScrn->display->virtualX,
                      pScrn->display->virtualY,
                      pSiS->maxxfbmem,
                      LOOKUP_BEST_REFRESH);

    if(i == -1) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86ValidateModes() error\n");
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }

    /* TW: Go through mode list and mark all those modes as bad,
     *     - which are unsuitable for dual head mode (if running dhm),
     *     - which exceed the LCD panels specs (if running on LCD)
     *     - TODO: which exceed TV capabilities (if running on TV)
     *     Also, find the highest used pixelclock on the master head.
     */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       if(!pSiS->SecondHead) pSiSEnt->maxUsedClock = 0;
    }
#endif
    if((p = first = pScrn->modes)) {
         do {
	       n = p->next;

               /* TW: Check the modes if they comply with our built-in tables.
	        *     This is of practical use only if the user disabled the
	        *     usage of the internal (built-in) modes.
		*/
               if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	         if(p->type & M_T_DEFAULT) {  
	           if( ( (strcmp(p->name, "320x200") != 0) &&
		         (strcmp(p->name, "320x240") != 0) &&
		         (strcmp(p->name, "400x300") != 0) &&
		         (strcmp(p->name, "512x384") != 0) )  &&
                       (p->Flags & V_DBLSCAN) ) {
		           p->status = MODE_NO_DBLESCAN;
		           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (mode not supported as doublescan)\n", p->name);
                   }
		   if( ( (strcmp(p->name, "1024x768")  != 0) &&
		         (strcmp(p->name, "1280x1024") != 0) &&
			 (strcmp(p->name, "848x480")   != 0) &&
			 (strcmp(p->name, "856x480")   != 0))  &&
                       (p->Flags & V_INTERLACE) ) {
		           p->status = MODE_NO_INTERLACE;
		           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (mode not supported as interlaced)\n", p->name);
                   }
		   if( ( (strcmp(p->name, "320x200") == 0) ||
		         (strcmp(p->name, "320x240") == 0) ||
		         (strcmp(p->name, "400x300") == 0) ||
		         (strcmp(p->name, "512x384") == 0) )  &&
		       (!(p->Flags & V_DBLSCAN)) ) {
		       	   p->status = MODE_CLOCK_RANGE;
			   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (only supported as doublescan)\n", p->name);
		   }
		 }
	       }
#ifdef SISDUALHEAD
	       /* TW: Modes that require the bridge to operate in SlaveMode
                *     are not suitable for Dual Head mode. Also check for
		*     modes that exceed panel dimension.
                */
               if(pSiS->DualHeadMode) {
	          if(pSiS->SecondHead == FALSE) {
	             if( (strcmp(p->name, "320x200") == 0) ||
		         (strcmp(p->name, "320x240") == 0) ||
		         (strcmp(p->name, "400x300") == 0) ||
		         (strcmp(p->name, "512x384") == 0) ||
		         (strcmp(p->name, "640x400") == 0) )  {
	    	        p->status = MODE_BAD;
		        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Not using mode \"%s\" (not suitable for dual head mode)\n",
			   p->name);
		     }
                  }
                  if(pSiS->VBFlags & DISPTYPE_DISP2) {
	            if(pSiS->VBFlags & CRT2_LCD) {
		      if(pSiS->SecondHead == FALSE) {
		        if((p->HDisplay > pSiS->LCDwidth) || (p->VDisplay > pSiS->LCDheight)) {
		            p->status = MODE_PANEL;
		            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			      "Not using mode \"%s\" (exceeds LCD panel dimension)\n", p->name);
	                }
			if(p->Flags & V_INTERLACE) {
			    p->status = MODE_BAD;
			    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (interlace on LCD not supported)\n",
				p->name);
			}
		      }
		    }
		    /* TO DO: TV */
	          }
		  /* TW: Search for the highest clock on first head in order to calculate
		   *     max clock for second head (CRT1)
		   */
		  if(!pSiS->SecondHead) {
		     if((p->status == MODE_OK) && (p->Clock > pSiSEnt->maxUsedClock)) {
		  		pSiSEnt->maxUsedClock = p->Clock;
		     }
		  }
	       } else {
#endif
	          if(pSiS->VBFlags & DISPTYPE_DISP2) {
	              if(pSiS->VBFlags & CRT2_LCD) {
		          if((p->HDisplay > pSiS->LCDwidth) || (p->VDisplay > pSiS->LCDheight)) {
		            p->status = MODE_PANEL;
		            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (exceeds LCD panel dimension)\n", p->name);
	                  }
			  if(p->Flags & V_INTERLACE) {
			    p->status = MODE_BAD;
			    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			        "Not using mode \"%s\" (interlace on LCD not supported)\n",
				p->name);
			  }
		      }
	          }
#ifdef SISDUALHEAD
               }
#endif
	       p = n;
         } while (p != NULL && p != first);
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if(i == 0 || pScrn->modes == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
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

    /* TW: Copy to CurrentLayout */
    pSiS->CurrentLayout.mode = pScrn->currentMode;
    pSiS->CurrentLayout.displayWidth = pScrn->displayWidth;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

#ifdef SISDUALHEAD
    /* TW: Due to palette & timing problems we don't support 8bpp in DHM */
    if((pSiS->DualHeadMode) && (pScrn->bitsPerPixel == 8)) {
    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Colordepth 8 not supported in Dual Head mode.\n");
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }
#endif

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    /* Load bpp-specific modules */
    switch(pScrn->bitsPerPixel) {
      case 1:
        mod = "xf1bpp";
        Sym = "xf1bppScreenInit";
        break;
      case 4:
        mod = "xf4bpp";
        Sym = "xf4bppScreenInit";
        break;
      case 8:
      case 16:
      case 24:
      case 32:
        mod = "fb";
	break;
    }

    if(mod && xf86LoadSubModule(pScrn, mod) == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load %s module", mod);
#ifdef SISDUALHEAD
	if(pSiSEnt) pSiSEnt->ErrorAfterFirst = TRUE;
#endif
	if(pSiS->pInt) xf86FreeInt10(pSiS->pInt);
	sisRestoreExtRegisterLock(pSiS,srlockReg,crlockReg);
        SISFreeRec(pScrn);
        return FALSE;
    }

    if(mod) {
	if(Sym) {
	    xf86LoaderReqSymbols(Sym, NULL);
	} else {
	    xf86LoaderReqSymLists(fbSymbols, NULL);
	}
    }

    /* Load XAA if needed */
    if(!pSiS->NoAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Accel enabled\n");
        if(!xf86LoadSubModule(pScrn, "xaa")) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load xaa module\n");
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
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not load shadowfb module\n");
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


    /* TW: Now load and initialize VBE module for VESA. */
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
	     SiSBuildVesaModeList(pScrn, pSiS->pVbe, vbe);
	     VBEFreeVBEInfo(vbe);
	     pSiS->UseVESA = 1;
       } else {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"Could not load and initialize VBE module. VESA disabled.\n");
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
	     /* TW: Only map if not mapped previously */
    	     pSiSEnt->IOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                         pSiS->PciTag, pSiS->IOAddress, 0x10000);
        }
        pSiS->IOBase = pSiSEnt->IOBase;
    } else
#endif
    	pSiS->IOBase = xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                        pSiS->PciTag, pSiS->IOAddress, 0x10000);

    if(pSiS->IOBase == NULL) {
    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not map MMIO area\n");
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
	     /* TW: Only map if not mapped previously */
	     pSiSEnt->IOBaseDense = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
                    pSiS->PciTag, pSiS->IOAddress, 0x10000);
	}
	pSiS->IOBaseDense = pSiSEnt->IOBaseDense;
    } else
#endif
    	pSiS->IOBaseDense = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
                    pSiS->PciTag, pSiS->IOAddress, 0x10000);

    if(pSiS->IOBaseDense == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not map MMIO dense area\n");
        return FALSE;
    }

#endif /* __alpha__ */

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        pSiSEnt->MapCountFbBase++;
        if(!(pSiSEnt->FbBase)) {
	     /* TW: Only map if not mapped previously */
    	     pSiSEnt->FbBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                         pSiS->PciTag, (unsigned long)pSiS->realFbAddress,
                         pSiS->FbMapSize);
	     pSiS->sishw_ext.pjVideoMemoryAddress = (UCHAR *)pSiSEnt->FbBase;
        }
        pSiS->FbBase = pSiSEnt->FbBase;
     	/* TW: Adapt FbBase (for DHM; dhmOffset is 0 otherwise) */
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
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not map framebuffer area\n");
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

/* TW: In dual head mode, we must not unmap if the other head still
 *     assumes memory as mapped
*/
#ifdef SISDUALHEAD
    if (pSiS->DualHeadMode) {
        if (pSiSEnt->MapCountIOBase) {
	    pSiSEnt->MapCountIOBase--;
	    if ((pSiSEnt->MapCountIOBase == 0) || (pSiSEnt->forceUnmapIOBase)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->IOBase, 0x10000);
	    	pSiSEnt->IOBase = NULL;
		pSiSEnt->MapCountIOBase = 0;
		pSiSEnt->forceUnmapIOBase = FALSE;
	    }
	    pSiS->IOBase = NULL;
	}
#ifdef __alpha__
	if (pSiSEnt->MapCountIOBaseDense) {
	    pSiSEnt->MapCountIOBaseDense--;
	    if ((pSiSEnt->MapCountIOBaseDense == 0) || (pSiSEnt->forceUnmapIOBaseDense)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->IOBaseDense, 0x10000);
	    	pSiSEnt->IOBaseDense = NULL;
		pSiSEnt->MapCountIOBaseDense = 0;
		pSiSEnt->forceUnmapIOBaseDense = FALSE;
	    }
	    pSiS->IOBaseDense = NULL;
	}
#endif /* __alpha__ */
	if (pSiSEnt->MapCountFbBase) {
	    pSiSEnt->MapCountFbBase--;
	    if ((pSiSEnt->MapCountFbBase == 0) || (pSiSEnt->forceUnmapFbBase)) {
	    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiSEnt->FbBase, pSiS->FbMapSize);
	    	pSiSEnt->FbBase = NULL;
		pSiSEnt->MapCountFbBase = 0;
		pSiSEnt->forceUnmapFbBase = FALSE;

	    }
	    pSiS->FbBase = NULL;
	}
    } else {
#endif
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiS->IOBase, 0x10000);
    	pSiS->IOBase = NULL;
#ifdef __alpha__
    	xf86UnMapVidMem(pScrn->scrnIndex, (pointer)pSiS->IOBaseDense, 0x10000);
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
    /* TW: We always save master & slave */
    if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif

    vgaReg = &VGAHWPTR(pScrn)->SavedReg;
    sisReg = &pSiS->SavedReg;

    vgaHWSave(pScrn, vgaReg, VGA_SR_ALL);

    sisSaveUnlockExtRegisterLock(pSiS,&sisReg->sisRegs3C4[0x05],&sisReg->sisRegs3D4[0x80]);

    (*pSiS->SiSSave)(pScrn, sisReg);
    
    if(pSiS->UseVESA) SISVESASaveRestore(pScrn, MODE_SAVE);
    
    /* TW: Save these as they may have been changed prior to SISSave() call */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
    	sisReg->sisRegs3D4[0x17] = pSiS->oldCR17;
	if(vgaReg->numCRTC >= 0x17) vgaReg->CRTC[0x17] = pSiS->oldCR17;
	sisReg->sisRegs3D4[0x32] = pSiS->oldCR32;
    }
}

/*
 * TW: Just adapted from the std* functions in vgaHW.c
 */
static void
SiS_WriteAttr(SISPtr pSiS, int index, int value)
{
    CARD8 tmp;

    tmp = inb(pSiS->IODBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(pSiS->IODBase + VGA_ATTR_INDEX, index);
    outb(pSiS->IODBase + VGA_ATTR_DATA_W, value);
}

static int
SiS_ReadAttr(SISPtr pSiS, int index)
{
    CARD8 tmp;

    tmp = inb(pSiS->IODBase + VGA_IOBASE_COLOR + VGA_IN_STAT_1_OFFSET);

    index |= 0x20;
    outb(pSiS->IODBase + VGA_ATTR_INDEX, index);
    return (inb(pSiS->IODBase + VGA_ATTR_DATA_R));
}


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

    if (pSiS->fonts != NULL)
	return;

    /* If in graphics mode, don't save anything */
    attr10 = SiS_ReadAttr(pSiS, 0x10);
    if (attr10 & 0x01)
	return;

    pSiS->fonts = xalloc(16384);

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
    slowbcopy_frombus(vgaIOBase, pSiS->fonts, 8192);

    /* font2 */
    outSISIDXREG(SISSR, 0x02, 0x08);  /* write to plane 3 */
    outSISIDXREG(SISSR, 0x04, 0x06);  /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x03);  /* read plane 3 */
    outSISIDXREG(SISGR, 0x05, 0x00);  /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05);  /* set graphics */
    slowbcopy_frombus(vgaIOBase, pSiS->fonts + 8192, 8192);

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

    if (pSiS->fonts == NULL)
	return;

#if 0
    if (pVesa->mapPhys == 0xa0000 && pVesa->curBank != 0)
	VESABankSwitch(pScrn->pScreen, 0);
#endif

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
    if (pScrn->depth == 4) {
        outSISIDXREG(SISGR, 0x03, 0x00);  /* don't rotate, write unmodified */
	outSISIDXREG(SISGR, 0x08, 0xFF);  /* write all bits in a byte */
	outSISIDXREG(SISGR, 0x01, 0x00);  /* all planes come from CPU */
    }

    outSISIDXREG(SISSR, 0x02, 0x04); /* write to plane 2 */
    outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x02); /* read plane 2 */
    outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
    slowbcopy_tobus(pSiS->fonts, vgaIOBase, 8192);

    outSISIDXREG(SISSR, 0x02, 0x08); /* write to plane 3 */
    outSISIDXREG(SISSR, 0x04, 0x06); /* enable plane graphics */
    outSISIDXREG(SISGR, 0x04, 0x03); /* read plane 3 */
    outSISIDXREG(SISGR, 0x05, 0x00); /* write mode 0, read mode 0 */
    outSISIDXREG(SISGR, 0x06, 0x05); /* set graphics */
    slowbcopy_tobus(pSiS->fonts + 8192, vgaIOBase, 8192);

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

/* TW: VESASaveRestore taken from vesa driver */
static void
SISVESASaveRestore(ScrnInfoPtr pScrn, vbeSaveRestoreFunction function)
{
    SISPtr pSiS;

    pSiS = SISPTR(pScrn);

    /* Query amount of memory to save state */
    if (function == MODE_QUERY ||
	(function == MODE_SAVE && pSiS->state == NULL)) {

	/* Make sure we save at least this information in case of failure */
	(void)VBEGetVBEMode(pSiS->pVbe, &pSiS->stateMode);
	SiS_SaveFonts(pScrn);

        if (pSiS->vesamajor > 1) {
	    if (!VBESaveRestore(pSiS->pVbe,function,(pointer)&pSiS->state,
				&pSiS->stateSize,&pSiS->statePage))
	        return;

	}
    }

    /* Save/Restore Super VGA state */
    if (function != MODE_QUERY) {
        Bool retval = TRUE;

	if (pSiS->vesamajor > 1) {
	    if (function == MODE_RESTORE)
		memcpy(pSiS->state, pSiS->pstate, pSiS->stateSize);

	    if ((retval = VBESaveRestore(pSiS->pVbe,function,
					 (pointer)&pSiS->state,
					 &pSiS->stateSize,&pSiS->statePage))
		&& function == MODE_SAVE) {
	        /* don't rely on the memory not being touched */
	        if (pSiS->pstate == NULL)
		    pSiS->pstate = xalloc(pSiS->stateSize);
		memcpy(pSiS->pstate, pSiS->state, pSiS->stateSize);
	    }
	}

	if (function == MODE_RESTORE) {
	    VBESetVBEMode(pSiS->pVbe, pSiS->stateMode, NULL);
	    SiS_RestoreFonts(pScrn);
	}
#if 0
	if (!retval)
	    return (FALSE);
#endif

    }
#if 0
    if ( (pSiS->vesamajor > 1) &&
	 (function == MODE_SAVE || pSiS->pstate) ) {
	if (function == MODE_RESTORE)
	    memcpy(pSiS->state, pSiS->pstate, pSiS->stateSize);
	if ((VBESaveRestore(pSiS->pVbe,function,
				     (pointer)&pSiS->state,
			    &pSiS->stateSize,&pSiS->statePage))) {
	    if (function == MODE_SAVE) {
		/* don't rely on the memory not being touched */
		if (pSiS->pstate == NULL)
		    pSiS->pstate = xalloc(pSiS->stateSize);
		memcpy(pSiS->pstate, pSiS->state, pSiS->stateSize);
	    }
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	    		"VBESaveRestore done with success\n");
	    return;
	}
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
			"VBESaveRestore done\n");
    } else {
	if (function == MODE_SAVE)
	    (void)VBEGetVBEMode(pSiS->pVbe, &pSiS->stateMode);
	else
	    VBESetVBEMode(pSiS->pVbe, pSiS->stateMode, NULL);
    }
#endif
}

/*
 * Initialise a new mode.  This is currently done using the
 * "initialise struct, restore/write struct to HW" model for
 * the old chipsets (5597/530/6326). For newer chipsets,
 * we use either VESA or our own mode switching code.
 */

static Bool
SISModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    SISPtr pSiS = SISPTR(pScrn);
    SISRegPtr sisReg;

    vgaHWUnlock(hwp);

    SISModifyModeInfo(mode);

    /* TW: Initialize SiS Port Register definitions for externally used
     *     BIOS emulation (native code switching) functions.
     */
    if( pSiS->VGAEngine == SIS_300_VGA ||
		        pSiS->VGAEngine == SIS_315_VGA ) {
       SiSRegInit(pSiS->SiS_Pr, pSiS->RelIO+0x30);
    }

    if (pSiS->UseVESA) {  /* With VESA: */

#ifdef SISDUALHEAD
	/* TW: No dual head mode when using VESA */
	if (pSiS->SecondHead) return TRUE;
#endif
	/*
	 * TW: This order is required:
	 * The video bridge needs to be adjusted before the
	 * BIOS is run as the BIOS sets up CRT2 according to
	 * these register settings.
	 * After the BIOS is run, the bridges and turboqueue
	 * registers need to be readjusted as the BIOS may
	 * very probably have messed them up.
	 */
	if( pSiS->VGAEngine == SIS_300_VGA ||
		        pSiS->VGAEngine == SIS_315_VGA ) {
		SiSPreSetMode(pScrn, mode);
	}
 	if(!SiSSetVESAMode(pScrn, mode)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"SiSSetVESAMode() failed\n");
	    return FALSE;
	}
	sisSaveUnlockExtRegisterLock(pSiS,NULL,NULL);
	if( pSiS->VGAEngine == SIS_300_VGA ||
		        pSiS->VGAEngine == SIS_315_VGA ) {
		SiSPreSetMode(pScrn, mode);
		SiSPostSetMode(pScrn, &pSiS->ModeReg);
	}
	/* TW: Prepare some register contents and set
	 *     up some mode dependent variables.
	 */
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"REAL REGISTER CONTENTS AFTER SETMODE:\n");
#endif
	if (!(*pSiS->ModeInit)(pScrn, mode)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"ModeInit() failed\n");
	    return FALSE;
	}

	pScrn->vtSema = TRUE;

	/* Program the registers */
	vgaHWProtect(pScrn, TRUE);
	(*pSiS->SiSRestore)(pScrn, &pSiS->ModeReg);
	vgaHWProtect(pScrn, FALSE);
	PDEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			  "HDisplay: %d, VDisplay: %d  \n",
			  mode->HDisplay, mode->VDisplay));

    } else { /* Without VESA: */
#ifdef SISDUALHEAD
	if(pSiS->DualHeadMode) {
                if(!(*pSiS->ModeInit)(pScrn, mode)) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	       "ModeInit() failed\n");
	            return FALSE;
	        }

	        pScrn->vtSema = TRUE;

		if(!(pSiS->SecondHead)) {
			/* TW: Head 1 (master) is always CRT2 */
			SiSPreSetMode(pScrn, mode);
			if (!SiSBIOSSetModeCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, mode)) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    				"SiSBIOSSetModeCRT2() failed\n");
				return FALSE;
			}
			SiSPostSetMode(pScrn, &pSiS->ModeReg);
		} else {
			/* TW: Head 2 (slave) is always CRT1 */
			SiSPreSetMode(pScrn, mode);
			if (!SiSBIOSSetModeCRT1(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, mode, pSiS->IsCustom)) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    				"SiSBIOSSetModeCRT1() failed\n");
				return FALSE;
			}
			SiSPostSetMode(pScrn, &pSiS->ModeReg);
		}
	} else {
#endif
		if(pSiS->VGAEngine == SIS_300_VGA ||
		                       pSiS->VGAEngine == SIS_315_VGA) {

	                /* TW: Prepare the register contents; On 300/310/325,
	                 *     we actually "abuse" this only for setting
	                 *     up some variables; the registers are NOT
	                 *     being written to the hardware as the BIOS
	                 *     emulation (native mode switching code)
	                 *     takes care of this.
	                 */
                        if(!(*pSiS->ModeInit)(pScrn, mode)) {
			    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    			"ModeInit() failed\n");
	                    return FALSE;
		        }

	                pScrn->vtSema = TRUE;

		        /* 300/310/325 series: Use our own code for mode switching */
	    		SiSPreSetMode(pScrn, mode);

	    		if(!SiSBIOSSetMode(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, mode, pSiS->IsCustom)) {
			  	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    				"SiSBIOSSetMode() failed\n");
				return FALSE;
		        }

	    		SiSPostSetMode(pScrn, &pSiS->ModeReg);
#ifdef TWDEBUG
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"REAL REGISTER CONTENTS AFTER SETMODE:\n");
                        (*pSiS->ModeInit)(pScrn, mode);
#endif
		} else {

		   /* For other chipsets, use the old method */

		   /* Initialise the ModeReg values */
    	           if(!vgaHWInit(pScrn, mode)) {
		       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		   "vgaHWInit() failed\n");
	               return FALSE;
		   }

		   /* Reset our PIOOffset as vgaHWInit might have reset it */
      		   VGAHWPTR(pScrn)->PIOOffset = pSiS->IODBase + (pSiS->PciInfo->ioBase[2] & 0xFFFC) - 0x380;

		   /* Prepare the register contents */
	           if(!(*pSiS->ModeInit)(pScrn, mode)) {
		       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		   "ModeInit() failed\n");
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

    /* TW: Update Currentlayout */
    pSiS->CurrentLayout.mode = mode;

    /* Debug */
/*  SiSDumpModeInfo(pScrn, mode);  */

    return TRUE;
}

static Bool
SiSSetVESAMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    SISPtr pSiS;
    int mode;

    pSiS = SISPTR(pScrn);

    if (!(mode = SiSCalcVESAModeIndex(pScrn, pMode))) return FALSE;

    mode |= 1 << 15;	/* TW: Don't clear framebuffer */
    mode |= 1 << 14;   	/* TW: Use linear adressing */

    if(VBESetVBEMode(pSiS->pVbe, mode, NULL) == FALSE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		"Setting VESA mode 0x%x failed\n",
	             	mode & 0x0fff);
	    return (FALSE);
    }

    if(pMode->HDisplay != pScrn->virtualX)
	VBESetLogicalScanline(pSiS->pVbe, pScrn->virtualX);

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
    		"Setting VESA mode 0x%x succeeded\n",
	       	mode & 0x0fff);

    return (TRUE);
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

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {

#ifdef SISDUALHEAD
       /* TW: We always restore master AND slave */
       if(pSiS->DualHeadMode && pSiS->SecondHead) return;
#endif

       /* TW: We must not disable the sequencer if the bridge is in SlaveMode! */
       if(!(SiSBridgeIsInSlaveMode(pScrn))) {
          vgaHWProtect(pScrn, TRUE);
       }

#ifdef UNLOCK_ALWAYS
       sisSaveUnlockExtRegisterLock(pSiS, NULL,NULL);
#endif

       /* TW: First, restore CRT1 on/off and VB connection registers */
       outSISIDXREG(SISCR, 0x32, pSiS->oldCR32);
       if(!(pSiS->oldCR17 & 0x80)) {			/* TW: CRT1 was off */
           if(!(SiSBridgeIsInSlaveMode(pScrn))) {       /* TW: Bridge is NOT in SlaveMode now -> do it */
	      doit = TRUE;
	   } else {
	      doitlater = TRUE;
	   }
       } else {						/* TW: CRT1 was on -> do it now */
           doit = TRUE;
       }
       
       if(doit) {
           outSISIDXREG(SISCR, 0x17, pSiS->oldCR17);
       }

       /* TW: For 30xB/LV, restoring the registers does not
        *     work. We "manually" set the old mode, instead.
	*     The same applies for SiS730 machines with LVDS.
	*     Finally, this behavior can be forced by setting
	*     the option RestoreBySetMode.
        */
        if( ( (pSiS->restorebyset) ||
	      (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) ||
	      ((pSiS->sishw_ext.jChipType == SIS_730) && (pSiS->VBFlags & VB_LVDS)) ) &&
	    (pSiS->OldMode) ) {

           if(pSiS->AccelInfoPtr) {
             (*pSiS->AccelInfoPtr->Sync)(pScrn);
           }

           xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	         "Restoring by setting old mode 0x%02x\n", pSiS->OldMode);

  	   if( (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) &&
	       (!pSiS->restorebyset) ) {		 
	      if(pSiS->OldMode == 0x03) pSiS->OldMode = 0x13;	 
	   }
		 
	   pSiS->SiS_Pr->UseCustomMode = FALSE;
	   pSiS->SiS_Pr->CRT1UsesCustomMode = FALSE;
	   SiSSetMode(pSiS->SiS_Pr, &pSiS->sishw_ext, pScrn, pSiS->OldMode, FALSE);
#ifdef TWDEBUG
		{
		   SISRegPtr      pReg = &pSiS->ModeReg;
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"REAL REGISTER CONTENTS AFTER RESTORE BY SETMODE:\n");
		   (*pSiS->SiSSave)(pScrn, pReg);
		}
#endif

        } else {

	   if(pSiS->VBFlags & VB_VIDEOBRIDGE) {
	      /* TW: If a video bridge is present, we need to restore
	       *     non-extended (=standard VGA) SR and CR registers
	       *     before restoring the extended ones and the bridge
	       *     registers itself. Unfortunately, the vgaHWRestore
	       *     routine clears CR17[7] - which must not be done if
	       *     the bridge is in slave mode.
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

	sisRestoreExtRegisterLock(pSiS,sisReg->sisRegs3C4[0x05],sisReg->sisRegs3D4[0x80]);
	
	if( ( (pSiS->sishw_ext.jChipType == SIS_730) && (pSiS->VBFlags & VB_LVDS)) ||
	    (pSiS->restorebyset) ) {
	   
	   /* TW: SiS730/LVDS has extreme problems restoring the text display due
	    *     to over-sensible LCD panels
	    */
   
	   vgaHWProtect(pScrn, TRUE);  
	    
	   if(pSiS->Primary) {
	      vgaHWRestore(pScrn, vgaReg, (VGA_SR_FONTS | VGA_SR_CMAP));
	   }
	   
	   vgaHWProtect(pScrn, FALSE); 
	
	} else {
	
	   vgaHWProtect(pScrn, TRUE);
	
	   if(pSiS->Primary) {
	      vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);
	   }
       
	   vgaHWProtect(pScrn, FALSE);
	
	}
    
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

	/* TW: Restore TV. This is rather complicated, but if we don't do it,
	 *     TV output will flicker terribly
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

   if(pSiS->UseVESA) SISVESASaveRestore(pScrn, MODE_RESTORE);
}

/* TW: Restore bridge registers - to be called BEFORE VESARestore */
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

/* TW: Our generic BlockHandler for Xv */
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
}

/* Mandatory
 * This gets called at the start of each server generation
 *
 * TW: We use pScrn and not CurrentLayout here, because the
 *     properties we use have not changed (displayWidth,
 *     depth, bitsPerPixel)
 */
static Bool
SISScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    SISPtr pSiS;
    int ret;
    int init_picture = 0;
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

    if(pSiS->UseVESA) {
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	pSiS->pVbe = VBEInit(NULL, pSiS->pEnt->index);
#else
        pSiS->pVbe = VBEExtendedInit(NULL, pSiS->pEnt->index,
	                   SET_BIOS_SCRATCH | RESTORE_BIOS_SCRATCH);
#endif
    }

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
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Could not map VGA window\n");
          return FALSE;
       }
    }
    vgaHWGetIOBase(hwp);

    /* TW: Patch the PIOOffset inside vgaHW to use
     *     our relocated IO ports.
     */
    VGAHWPTR(pScrn)->PIOOffset = pSiS->IODBase + (pSiS->PciInfo->ioBase[2] & 0xFFFC) - 0x380;

    /* Map the SIS memory and MMIO areas */
    if(!SISMapMem(pScrn)) {
    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"SiSMapMem() failed\n");
        return FALSE;
    }

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* TW: Enable TurboQueue so that SISSave() saves it in enabled
     *     state. If we don't do this, X will hang after a restart!
     *     (Happens for some unknown reason only when using VESA
     *     for mode switching; assumingly a BIOS issue.)
     *     This is done on 300 and 310/325 series only.
     */
    if(pSiS->UseVESA) {
	SiSEnableTurboQueue(pScrn);
    }

    /* Save the current state */
    SISSave(pScrn);

    /* TW: Save the current mode number */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
        inSISIDXREG(SISCR, 0x34, pSiS->OldMode);
    }

    /* Initialise the first mode */
    if(!SISModeInit(pScrn, pScrn->currentMode)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"SiSModeInit() failed\n");
        return FALSE;
    }

    /* Darken the screen for aesthetic reasons */
    /* TW: Not using Dual Head variant on purpose; we darken
     *     the screen for both displays, and un-darken
     *     it when the second head is finished
     */
    SISSaveScreen(pScreen, SCREEN_SAVER_ON);

    /* Set the viewport */
    SISAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /* Clear frame buffer */
    OnScreenSize = pScrn->displayWidth * pScrn->currentMode->VDisplay
                               * (pScrn->bitsPerPixel / 8);
    bzero(pSiS->FbBase, OnScreenSize);

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
    if(pScrn->bitsPerPixel > 8) {
        if(!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits,
                              pScrn->defaultVisual)) {
	 	SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		"miSetVisualTypes() failed (bpp %d)\n", pScrn->bitsPerPixel);
	    	return FALSE;
	}
    } else {
        if(!miSetVisualTypes(pScrn->depth,
                              miGetDefaultVisualMask(pScrn->depth),
                              pScrn->rgbBits, pScrn->defaultVisual)) {
		SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		"miSetVisualTypes() failed (bpp %d)\n", pScrn->bitsPerPixel);
		return FALSE;
	}
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
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"miSetPixmapDepths() failed\n");
	return FALSE;
    }

#ifdef SISDUALHEAD
    if(pSiS->SecondHead)
	   pSiS->cmdQueueLenPtr = &(SISPTR(pSiSEnt->pScrn_1)->cmdQueueLen);
    else
#endif
           pSiS->cmdQueueLenPtr = &(pSiS->cmdQueueLen);

    pSiS->cmdQueueLen = 0; /* TW: Force an EngineIdle() at start */

#ifdef XF86DRI
#ifdef SISDUALHEAD
    /* TW: No DRI in dual head mode */
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
#endif

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    switch(pScrn->bitsPerPixel) {
      case 1:
        ret = xf1bppScreenInit(pScreen, FBStart, width,
                        height, pScrn->xDpi, pScrn->yDpi,
                        displayWidth);
        break;
      case 4:
        ret = xf4bppScreenInit(pScreen, FBStart, width,
                        height, pScrn->xDpi, pScrn->yDpi,
                        displayWidth);
        break;
      case 8:
      case 16:
      case 24:
      case 32:
        ret = fbScreenInit(pScreen, FBStart, width,
                        height, pScrn->xDpi, pScrn->yDpi,
                        displayWidth, pScrn->bitsPerPixel);

	init_picture = 1;
        break;
      default:
        xf86DrvMsg(scrnIndex, X_ERROR,
               "Internal error: invalid bpp (%d) in SISScrnInit\n",
               pScrn->bitsPerPixel);
            ret = FALSE;
        break;
    }
    if (!ret) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf1bpp/xf4bpp/fbScreenInit() failed\n");
  	SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
        return FALSE;
    }

    if(pScrn->bitsPerPixel > 8) {
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
    } else if(pScrn->depth == 1) {
        SIS1bppColorMap(pScrn);
    }

    /* Initialize RENDER ext; must be after RGB ordering fixed */
    if(init_picture)  fbPictureInit(pScreen, 0, 0);

    /* hardware cursor needs to wrap this layer    <-- TW: what does that mean? */
    if(!pSiS->ShadowFB)  SISDGAInit(pScreen);

    xf86SetBlackWhitePixels(pScreen);

    if(!pSiS->NoAccel) {
        switch(pSiS->VGAEngine) {
	  case SIS_530_VGA:
	  case SIS_300_VGA:
            SiS300AccelInit(pScreen);
	    break;
	  case SIS_315_VGA:
	    SiS310AccelInit(pScreen);
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

    if(pSiS->HWCursor)
        SiSHWCursorInit(pScreen);

    /* Initialise default colourmap */
    if(!miCreateDefColormap(pScreen)) {
        SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"miCreateDefColormap() failed\n");
        return FALSE;
    }
    
    if(!xf86HandleColormaps(pScreen, 256, (pScrn->depth == 8) ? 8 : pScrn->rgbBits,
                    SISLoadPalette, NULL,
                    CMAP_PALETTED_TRUECOLOR | CMAP_RELOAD_ON_MODE_SWITCH)) {
        SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"xf86HandleColormaps() failed\n");
        return FALSE;
    }

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
       }

       ShadowFBInit(pScreen, refreshArea);
    }

#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode)
    	/* TW: DPMS for dual head mode */
        xf86DPMSInit(pScreen, (DPMSSetProcPtr)SISDisplayPowerManagementSetDH, 0);
    else
#endif
        xf86DPMSInit(pScreen, (DPMSSetProcPtr)SISDisplayPowerManagementSet, 0);

    /* Init memPhysBase and fbOffset in pScrn */
    pScrn->memPhysBase = pSiS->FbAddress;
    pScrn->fbOffset = 0;

#ifdef XvExtension
    if(!pSiS->NoXvideo) {
#ifdef SISDUALHEAD
        /* TW: On chipsets with only one overlay, we support
	 *     Xv only in "real" dual head mode, not Xinerama
	 */
	if ( ((pSiS->VGAEngine == SIS_300_VGA) ||
	      (pSiS->VGAEngine == SIS_315_VGA) )
	     &&
	     ((pSiS->hasTwoOverlays)  ||
	      (!pSiS->DualHeadMode)   ||
	      (noPanoramiXExtension) ) ) {
#else
        if (  (pSiS->VGAEngine == SIS_300_VGA) ||
	      (pSiS->VGAEngine == SIS_315_VGA) ) {
#endif
#ifdef SISDUALHEAD
              if (pSiS->DualHeadMode) {
	         if ( pSiS->hasTwoOverlays ||
		     (pSiS->XvOnCRT2 && (!pSiS->SecondHead)) ||
		     ((!pSiS->XvOnCRT2 && pSiS->SecondHead)) ) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Using SiS300/310/325 series HW Xv on CRT%d\n",
			(pSiS->SecondHead ? 1 : 2));
                    SISInitVideo(pScreen);
                 } else {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Not using SiS300/310/325 series HW Xv on CRT%d\n",
			(pSiS->SecondHead ? 1 : 2));
		 }
 	      } else {
#endif
	        if (pSiS->hasTwoOverlays)
                    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Using SiS300/310/325 series HW Xv\n" );
                else
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Using SiS300/310/325 series HW Xv on CRT%d\n",
			(pSiS->XvOnCRT2 ? 2 : 1));
	        SISInitVideo(pScreen);
#ifdef SISDUALHEAD
              }
#endif
#ifdef USE6326VIDEO
        } else if( pSiS->Chipset == PCI_CHIP_SIS6326 ||
	           pSiS->Chipset == PCI_CHIP_SIS530  ||
		   pSiS->Chipset == PCI_CHIP_SIS5597 ) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		        "Using SiS5597/5598/6326/530/620 HW Xv\n" );
		SIS6326InitVideo(pScreen);
#endif
	} else { /* generic Xv */

            XF86VideoAdaptorPtr *ptr;
            int n;

            n = xf86XVListGenericAdaptors(pScrn, &ptr);
            if (n) {
                xf86XVScreenInit(pScreen, ptr, n);
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using generic Xv\n" );
            }
	    if (!noPanoramiXExtension)
	    	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"HW Xv not supported in Xinerama mode\n");
        }
    }
#endif

#ifdef XF86DRI
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

    /* Turn on the screen now */
    /* TW: We do this in dual head mode after second head is finished */
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
        if(pSiS->SecondHead)
    	     SISSaveScreen(pScreen, SCREEN_SAVER_OFF);
    } else
#endif
        SISSaveScreen(pScreen, SCREEN_SAVER_OFF);

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

    return SISModeInit(xf86Screens[scrnIndex], mode);
}

#ifdef CYCLECRT2
/* TW: Cycle CRT2 output devices */
Bool
SISCycleCRT2Type(int scrnIndex, DisplayModePtr mode)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SISPtr pSiS = SISPTR(pScrn);
    int i = 0;

    /* TW: Only on 300 and 310/325 series */
    if(pSiS->VGAEngine != SIS_300_VGA &&
       pSiS->VGAEngine != SIS_315_VGA) return FALSE;

    /* TW: Only if there is a video bridge */
    if(pSiS->VBFlags & VB_VIDEOBRIDGE) return FALSE;

    /* TW: Only if there were more than 1 CRT2 devices detected */
    if(pSiS->detectedCRT2Devices & CRT2_VGA) i++;
    if(pSiS->detectedCRT2Devices & CRT2_LCD) i++;
    if(pSiS->detectedCRT2Devices & CRT2_TV)  i++;
    if(i <= 1) return FALSE;

    /* TW: Cycle CRT2 type */
    i = (pSiS->VBFlags & DISPTYPE_DISP2) << 1;
    while(!(i & pSiS->detectedCRT2Devices)) {
      i <<= 1;
      if(i > CRT2_VGA) i = CRT2_LCD;
    }

    /* TW: Check if mode is suitable for desired output device */
    if(!SiS_CheckCalcModeIndex(pScrn, pScrn->currentMode,
    			       ((pSiS->VBFlags & ~(DISPTYPE_DISP2)) | i))) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Current mode not suitable for desired CRT2 output device\n");
    	return FALSE;
    }

    /* TW: Sync the accelerators */
    if(!pSiS->NoAccel) {
         if(pSiS->AccelInfoPtr) {
            (*pSiS->AccelInfoPtr->Sync)(pScrn);
	 }
    }

    pSiS->VBFlags &= ~(DISPTYPE_DISP2);
    pSiS->VBFlags |= i;

    return SISModeInit(xf86Screens[scrnIndex], mode);
}
#endif

/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void
SISAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    SISPtr pSiS;
    vgaHWPtr hwp;
    int base;
    unsigned char temp;

    hwp = VGAHWPTR(pScrn);
    pSiS = SISPTR(pScrn);

    base = y * pSiS->CurrentLayout.displayWidth + x;

    if(pSiS->UseVESA) {

        /* TW: Let BIOS adjust frame if using VESA */
	VBESetDisplayStart(pSiS->pVbe, x, y, TRUE);

    } else {

#ifdef UNLOCK_ALWAYS
        sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

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

#ifdef SISDUALHEAD
        if (pSiS->DualHeadMode) {
		/* TW: We assume that DualHeadMode only can be true for
		 *     dual head capable chipsets (and thus save the check
		 *     for chipset here)
		 */
		if (!pSiS->SecondHead) {
			/* TW: Head 1 (master) is always CRT2 */
   			SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
                	outSISIDXREG(SISPART1, 0x06, GETVAR8(base));
                	outSISIDXREG(SISPART1, 0x05, GETBITS(base, 15:8));
                	outSISIDXREG(SISPART1, 0x04, GETBITS(base, 23:16));
			if (pSiS->VGAEngine == SIS_315_VGA) {
			   setSISIDXREG(SISPART1, 0x02, 0x7F, ((base >> 24) & 0x01) << 7);
			}
                	SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
		} else {
			/* TW: Head 2 (slave) is always CRT1 */
			base += (pSiS->dhmOffset/4);
			outSISIDXREG(SISCR, 0x0D, base & 0xFF);
			outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
			outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
			if (pSiS->VGAEngine == SIS_315_VGA) {
			    setSISIDXREG(SISSR, 0x37, 0xFE, (base >> 24) & 0x01);
			}
		}
	} else {
#endif
    	   switch (pSiS->VGAEngine)  {
           	case SIS_300_VGA:
			outSISIDXREG(SISCR, 0x0D, base & 0xFF);
			outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
			outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
            		if (pSiS->VBFlags & CRT2_ENABLE) {
                		SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
                		outSISIDXREG(SISPART1, 0x06, GETVAR8(base));
                		outSISIDXREG(SISPART1, 0x05, GETBITS(base, 15:8));
                		outSISIDXREG(SISPART1, 0x04, GETBITS(base, 23:16));
                		SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
            		}
            		break;
	   	case SIS_315_VGA:
			outSISIDXREG(SISCR, 0x0D, base & 0xFF);
			outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
			outSISIDXREG(SISSR, 0x0D, (base >> 16) & 0xFF);
			setSISIDXREG(SISSR, 0x37, 0xFE, (base >> 24) & 0x01);
            		if (pSiS->VBFlags & CRT2_ENABLE) {
                		SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
                		outSISIDXREG(SISPART1, 0x06, GETVAR8(base));
                		outSISIDXREG(SISPART1, 0x05, GETBITS(base, 15:8));
                		outSISIDXREG(SISPART1, 0x04, GETBITS(base, 23:16));
				setSISIDXREG(SISPART1, 0x02, 0x7F, ((base >> 24) & 0x01) << 7);
                		SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
            		}
            		break;
        	default:
		        outSISIDXREG(SISCR, 0x0D, base & 0xFF);
			outSISIDXREG(SISCR, 0x0C, (base >> 8) & 0xFF);
			inSISIDXREG(SISSR, 0x27, temp);
			temp &= 0xF0;
			temp |= (base & 0x0F0000) >> 16;
			outSISIDXREG(SISSR, 0x27, temp);
    		}
#ifdef SISDUALHEAD
	}
#endif
    } /* if not VESA */

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

    if(!SISModeInit(pScrn, pScrn->currentMode)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"SiSEnterVT: SISModeInit() failed\n");
	return FALSE;
    }

    SISAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

#ifdef XF86DRI
    /* TW: this is to be done AFTER switching the mode */
    if(pSiS->directRenderingEnabled)
        DRIUnlock(screenInfo.screens[scrnIndex]);
#endif

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

    /* TW: to be done before mode change */
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

        /* TW: This is a q&d work-around for a BIOS bug. In case we disabled CRT2,
    	 *     VBESaveRestore() does not restore CRT1. So we set any mode now,
	 *     because VBESetVBEMode correctly restores CRT1. Afterwards, we
	 *     can call VBESaveRestore to restore original mode.
	 */
        if ( (pSiS->VBFlags & VB_VIDEOBRIDGE) && (!(pSiS->VBFlags & DISPTYPE_DISP2)) )
	           VBESetVBEMode(pSiS->pVbe, (pSiS->SISVESAModeList->n) | 0xc000, NULL);

        SISVESARestore(pScrn);

    } else {
       
       SISRestore(pScrn);
       
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

	  /* TW: This is a q&d work-around for a BIOS bug. In case we disabled CRT2,
    	   *     VBESaveRestore() does not restore CRT1. So we set any mode now,
	   *     because VBESetVBEMode correctly restores CRT1. Afterwards, we
	   *     can call VBESaveRestore to restore original mode.
	   */
           if( (pSiS->VBFlags & VB_VIDEOBRIDGE) && (!(pSiS->VBFlags & DISPTYPE_DISP2)))
	           VBESetVBEMode(pSiS->pVbe, (pSiS->SISVESAModeList->n) | 0xc000, NULL);

	   SISVESARestore(pScrn);

	} else {

	   SISRestore(pScrn);

	}

        vgaHWLock(hwp);
    }

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
    }

    pScrn->vtSema = FALSE;

    /* Restore Blockhandler */
    pScreen->BlockHandler = pSiS->BlockHandler;

    pScreen->CloseScreen = pSiS->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}


/* Free up any per-generation data structures */

/* Optional */
static void
SISFreeScreen(int scrnIndex, int flags)
{
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
        vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    SISFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static int
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
        if((pSiS->DualHeadMode) && (pSiS->SecondHead)) {
	    /* DHM: Only check modes for CRT1 */
	    if(SiS_CalcModeIndex(pScrn, mode) < 0x14)
	      	return(MODE_BAD);
	} else
#endif
	    if(SiS_CheckCalcModeIndex(pScrn, mode, pSiS->VBFlags) < 0x14)
	        return(MODE_BAD);

    }
    
    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
SISSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    if ((pScrn != NULL) && pScrn->vtSema) {

    	SISPtr pSiS = SISPTR(pScrn);

	/* enable access to extended sequencer registers */
#ifdef UNLOCK_ALWAYS
        sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

	if(pSiS->VGAEngine == SIS_300_VGA) {

	   if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
	       if(!xf86IsUnblank(mode)) {
	          pSiS->Blank = TRUE;
	  	  SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
	       } else {
	          pSiS->Blank = FALSE;
	          SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
	       }
	   } else {
	      /* if not blanked obtain state of LCD blank flags set by BIOS */
	      if(!pSiS->Blank) {
	         inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	      }

	      if(!xf86IsUnblank(mode)) {
    		 pSiS->Blank = TRUE;
		 outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
	      } else {
    		 pSiS->Blank = FALSE;
    		 /* don't just unblanking; use LCD state set by BIOS */
		 outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	      }
	  }

	} else if(pSiS->VGAEngine == SIS_315_VGA) {

	   if(!pSiS->Blank) {
		inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	   }

	   if(pSiS->VBFlags & VB_CHRONTEL) {
	       if(!xf86IsUnblank(mode)) {
		  pSiS->Blank = TRUE;
		  SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
	       } else {
	          pSiS->Blank = FALSE;
	          SiS_Chrontel701xBLOn(pSiS->SiS_Pr);
	       }
	   } else if(pSiS->VBFlags & VB_LVDS) {
	       if(!xf86IsUnblank(mode)) {
	          pSiS->Blank = TRUE;
	 	  outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
	       } else {
	          pSiS->Blank = FALSE;
	  	  outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
	       }
	   } else if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
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

    return vgaHWSaveScreen(pScreen, mode);
}

#ifdef SISDUALHEAD
/* TW: SaveScreen for dual head mode */
static Bool
SISSaveScreenDH(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    if ((pScrn != NULL) && pScrn->vtSema) {

    	SISPtr pSiS = SISPTR(pScrn);
	if (pSiS->SecondHead) {

	    /* Slave head is always CRT1 */
	    return vgaHWSaveScreen(pScreen, mode);

	} else {

	    /* Master head is always CRT2 */

	    /* We can only blank LCD, not other CRT2 devices */
	    if(!(pSiS->VBFlags & CRT2_LCD)) return TRUE;

	    /* enable access to extended sequencer registers */
#ifdef UNLOCK_ALWAYS
            sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

 	    if(pSiS->VGAEngine == SIS_300_VGA) {

	        if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
		   if(!xf86IsUnblank(mode)) {
		         pSiS->BlankCRT2 = TRUE;
			 SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   } else {
		         pSiS->BlankCRT2 = FALSE;
		         SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		   }
		} else {
		   /* if not blanked obtain state of LCD blank flags set by BIOS */
		   if(!pSiS->BlankCRT2) {
		        inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		   }

		   if (!xf86IsUnblank(mode)) {
    			pSiS->BlankCRT2 = TRUE;
			outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
		   } else {
    			pSiS->BlankCRT2 = FALSE;
    			/* don't just unblank; use LCD state set by BIOS */
			outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		   }
		}

            } else if(pSiS->VGAEngine == SIS_315_VGA) {

 		if(!pSiS->BlankCRT2) {
			inSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		}

	        if(pSiS->VBFlags & VB_CHRONTEL) {
		      if(!xf86IsUnblank(mode)) {
			  pSiS->BlankCRT2 = TRUE;
			  SiS_Chrontel701xBLOff(pSiS->SiS_Pr);
		      } else {
		          pSiS->BlankCRT2 = FALSE;
		          SiS_Chrontel701xBLOn(pSiS->SiS_Pr);
		      }
		} else if(pSiS->VBFlags & VB_LVDS) {
		      if(!xf86IsUnblank(mode)) {
		         pSiS->BlankCRT2 = TRUE;
			 outSISIDXREG(SISSR, 0x11, pSiS->LCDon | 0x08);
		      } else {
		         pSiS->BlankCRT2 = FALSE;
			 outSISIDXREG(SISSR, 0x11, pSiS->LCDon);
		      }
		} else if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
		      if(!xf86IsUnblank(mode)) {
		         pSiS->BlankCRT2 = TRUE;
			 SiS_SiS30xBLOff(pSiS->SiS_Pr,&pSiS->sishw_ext);
		      } else {
		         pSiS->BlankCRT2 = FALSE;
		         SiS_SiS30xBLOn(pSiS->SiS_Pr,&pSiS->sishw_ext);
		      }
		}

	    }
	}
    }
    return TRUE;
}
#endif

#ifdef DEBUG
/* locally used for debug */
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

/* local used for debug */
static void
SISModifyModeInfo(DisplayModePtr mode)
{
#if 1
    if(mode->CrtcHBlankStart == mode->CrtcHDisplay)
        mode->CrtcHBlankStart++;
    if(mode->CrtcHBlankEnd == mode->CrtcHTotal)
        mode->CrtcHBlankEnd--;
    if(mode->CrtcVBlankStart == mode->CrtcVDisplay)
        mode->CrtcVBlankStart++;
    if(mode->CrtcVBlankEnd == mode->CrtcVTotal)
        mode->CrtcVBlankEnd--;
#endif
}

/* TW: Enable the TurboQueue (For 300 and 310/325 series only) */
void
SiSEnableTurboQueue(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned short SR26, SR27;
    unsigned long  temp;

    switch (pSiS->VGAEngine) {
	case SIS_300_VGA:
	   if ((!pSiS->NoAccel) && (pSiS->TurboQueue)) {
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
	   if (!pSiS->NoAccel) {
	      /* TW: On 310/325 series, there are three queue modes available
	       *     which are chosen by setting bits 7:5 in SR26:
	       * 1. MMIO queue mode (bit 5, 0x20). The hardware will keep
	       *    track of the queue, the FIFO, command parsing and so
	       *    on. This is the one comparable to the 300 series.
	       * 2. VRAM queue mode (bit 6, 0x40). In this case, one will
	       *    have to do queue management himself. Register 0x85c4 will
	       *    hold the location of the next free queue slot, 0x85c8
	       *    is the "queue read pointer" whose way of working is
	       *    unknown to me. Anyway, this mode would require a
	       *    translation of the MMIO commands to some kind of
	       *    accelerator assembly and writing these commands
	       *    to the memory location pointed to by 0x85c4.
	       *    We will not use this, as nobody knows how this
	       *    "assembly" works, and as it would require a complete
	       *    re-write of the accelerator code.
	       * 3. AGP queue mode (bit 7, 0x80). Works as 2., but keeps the
	       *    queue in AGP memory space.
	       * We go MMIO here.
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
#if 0
	      if (pSiS->TurboQueue) {
#endif
	   	/* TW: We only use MMIO Cmd Queue, not VRAM or AGP */
	   	/* TW: Set Command Queue Threshold to max value 11111b */
	   	outSISIDXREG(SISSR, 0x27, 0x1F);
	   	/* TW: Syncronous reset for Command Queue */
	   	outSISIDXREG(SISSR, 0x26, 0x01);
	   	/* TW: Do some magic (cp readport to writeport) */
	   	temp = MMIO_IN32(pSiS->IOBase, 0x85C8);
	   	MMIO_OUT32(pSiS->IOBase, 0x85C4, temp);
	   	/* TW: Enable MMIO Command Queue mode (0x20),
		 *     Enable_command_queue_auto_correction (0x02)   
		 *             (no idea, but sounds good, so use it)
		 *     512k (0x00) (does this apply to MMIO mode?) */
    	   	outSISIDXREG(SISSR, 0x26, 0x22);
	   	/* TW: Calc Command Queue position (Q is always 512k)*/
	   	temp = (pScrn->videoRam - 512) * 1024;
	   	/* TW: Set Q position */
	   	MMIO_OUT32(pSiS->IOBase, 0x85C0, temp);
#if 0
              } else {
	      	/* TW: Is there a non-TurboQueue mode within MMIO mode? */
	      }
#endif
	   }
	   break;
	default:
	   break;
    }
}

/* TW: Things to do before a ModeSwitch. We set up the
 *     video bridge configuration and the TurboQueue.
 */
void SiSPreSetMode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    SISPtr         pSiS = SISPTR(pScrn);
    unsigned char  usScratchCR30, usScratchCR31;
    unsigned char  usScratchCR32, usScratchCR33;
    unsigned char  usScratchCR17, usScratchCR38 = 0;
    int            vbflag, temp = 0;
    int 	   crt1rateindex = 0;

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);    /* Unlock Registers */
#endif

    vbflag = pSiS->VBFlags;
    pSiS->IsCustom = FALSE;
    
    if(pSiS->HaveCustomModes) {
       if(!(mode->type & M_T_DEFAULT)) {
	 pSiS->IsCustom = TRUE;
       }
    }

    /* TW: The CR3x registers are for communicating with our BIOS emulation
     * code (native code in init.c/init301.c) or the BIOS (via VESA)
     */
    inSISIDXREG(SISCR, 0x30, usScratchCR30);  /* Bridge config */
    inSISIDXREG(SISCR, 0x31, usScratchCR31);  /* Bridge config */
    usScratchCR32 = pSiS->newCR32;            /* Bridge connection info (use our new value) */
    inSISIDXREG(SISCR, 0x33, usScratchCR33);  /* CRT1 refresh rate index */
    if(pSiS->Chipset != PCI_CHIP_SIS300) {
       switch(pSiS->VGAEngine) {
       case SIS_300_VGA: temp = 0x35; break;
       case SIS_315_VGA: temp = 0x38; break;
       }
    }
    if(temp) inSISIDXREG(SISCR, temp, usScratchCR38); /* PAL-M, PAL-N selection */

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "VBFlags=0x%x\n", pSiS->VBFlags);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_PROBED, 3, 
	   "Before: CR30=0x%02x, CR31=0x%02x, CR32=0x%02x, CR33=0x%02x, CR%02x=0x%02x\n",
              usScratchCR30, usScratchCR31, usScratchCR32, usScratchCR33, temp, usScratchCR38);

    usScratchCR30 = 0;
    usScratchCR31 &= ~0x60;  /* TW: Clear VB_Drivermode & VB_OutputDisable */
#if 0  /* TW: For future use */
    if( (pSiS->VBFlags & VB_LVDS) ||
        (pSiS->VBFlags & VB_301)  ||
        ( (pSiS->VBFlags & (VB_301B | VB_302B |VB_30xLV | VB_30xLVX)) &&
	  (!(pSiS->VBLCDFlags & VB_LCD_1400x1050)) ) ) {
#endif
       usScratchCR31 |= 0x04;   /* TW: Set VB_NotSimuMode (not for 30xB/1400x1050?) */
#if 0
    }
#endif

    switch(vbflag & (CRT2_TV|CRT2_LCD|CRT2_VGA)) {
      case CRT2_TV:
          	if(vbflag & TV_CHSCART) {
				usScratchCR38 |= 0x04;
				usScratchCR31 |= 0x01;
		} else if(vbflag & TV_CHHDTV) {
				usScratchCR38 |= 0x08;
				usScratchCR31 &= ~0x01;
        	} else if(vbflag & TV_HIVISION)
		          	usScratchCR30 |= 0x80;
		else if(vbflag & TV_SVIDEO)
				usScratchCR30 |= 0x08;
		else if(vbflag & TV_AVIDEO)
				usScratchCR30 |= 0x04;
		else if(vbflag & TV_SCART)
				usScratchCR30 |= 0x10;
		else 
			        usScratchCR30 |= 0x08;    /* default: SVIDEO */

		if(!(vbflag & (TV_CHSCART | TV_CHHDTV))) {
		    if(vbflag & TV_PAL) {
				usScratchCR31 |= 0x01;
				usScratchCR38 &= ~0xC0;
		                if( (vbflag & VB_SISBRIDGE) ||
		                    ((vbflag & VB_CHRONTEL) && (pSiS->ChrontelType == CHRONTEL_701x)) )  {
					if(vbflag & TV_PALM)      usScratchCR38 |= 0x40;
					else if(vbflag & TV_PALN) usScratchCR38 |= 0x80;
		    		}
		    } else
				usScratchCR31 &= ~0x01;
		}

		usScratchCR30 |= 0x01;    /* Set SimuScanMode  */

		usScratchCR31 &= ~0x04;   /* Clear NotSimuMode */
		pSiS->SiS_Pr->SiS_CHOverScan = pSiS->UseCHOverScan;
		if(pSiS->OptTVSOver == 1) {
			pSiS->SiS_Pr->SiS_CHSOverScan = TRUE;
		} else {
		        pSiS->SiS_Pr->SiS_CHSOverScan = FALSE;
		}
                break;
      case CRT2_LCD:
                usScratchCR30 |= 0x21;    /* LCD + SimuScanMode */
                break;
      case CRT2_VGA:
                usScratchCR30 |= 0x41;    /* VGA2 + SimuScanMode */
                break;
      default:
            	usScratchCR30 |= 0x00;
            	usScratchCR31 |= 0x20;    /* VB_OUTPUT_DISABLE */
		if(pSiS->UseVESA) {
		   crt1rateindex = SISSearchCRT1Rate(pScrn, mode);
		}
    }
    /* TW: for VESA: no DRIVERMODE, otherwise
     * -) CRT2 will not be initialized correctly when using mode
     *    where LCD has to scale, and
     * -) CRT1 will have too low rate
     */
     if (pSiS->UseVESA) {
        usScratchCR31 &= 0x40;  /* TW: Clear Drivermode */
#ifdef TWDEBUG
        usScratchCR31 |= 0x40;  /* DEBUG (for non-slave mode VESA) */
	crt1rateindex = SISSearchCRT1Rate(pScrn, mode);
#endif
     } else {
        usScratchCR31 |=  0x40;                 /* TW: Set Drivermode */
	if(!pSiS->IsCustom) {
           crt1rateindex = SISSearchCRT1Rate(pScrn, mode);
	} else {
	   crt1rateindex = usScratchCR33;
	}
     }
     outSISIDXREG(SISCR, 0x30, usScratchCR30);
     outSISIDXREG(SISCR, 0x31, usScratchCR31);
     if(temp) {
        usScratchCR38 &= ~0x03;   /* Clear LCDA/DualEdge bits */
     	outSISIDXREG(SISCR, temp, usScratchCR38);
     }

     pSiS->SiS_Pr->SiS_UseOEM = pSiS->OptUseOEM;

#ifdef SISDUALHEAD
     if(pSiS->DualHeadMode) {
        if(pSiS->SecondHead) {
	    /* CRT1 */
	    usScratchCR33 &= 0xf0;
	    usScratchCR33 |= (crt1rateindex & 0x0f);
	} else {
	    /* CRT2 */
	    usScratchCR33 &= 0x0f;
	    if(vbflag & CRT2_VGA) usScratchCR33 |= ((crt1rateindex << 4) & 0xf0);
	}
     } else {
#endif
        if(vbflag & CRT2_VGA) {
           usScratchCR33 = (crt1rateindex & 0x0f) | ((crt1rateindex & 0x0f) << 4);
	} else {
	   usScratchCR33 = crt1rateindex & 0x0f;
	}
	if((!(pSiS->UseVESA)) && (vbflag & CRT2_ENABLE)) {
#ifndef TWDEBUG	
	   if(pSiS->CRT1off) usScratchCR33 &= 0xf0;
#endif	   
	}
#ifdef SISDUALHEAD
     }
#endif
     outSISIDXREG(SISCR, 0x33, usScratchCR33);

     xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"After:  CR30=0x%02x, CR31=0x%02x, CR33=0x%02x\n",
		    usScratchCR30, usScratchCR31, usScratchCR33);

     /* Enable TurboQueue */
     SiSEnableTurboQueue(pScrn);

     if((!pSiS->UseVESA) && (pSiS->VBFlags & CRT2_ENABLE)) {
        /* Switch on CRT1 for modes that require the bridge in SlaveMode */
	inSISIDXREG(SISCR, 0x17, usScratchCR17);
	if(!(usScratchCR17 & 0x80)) {
          orSISIDXREG(SISCR, 0x17, 0x80);
	  outSISIDXREG(SISSR, 0x00, 0x01);
	  usleep(10000);
          outSISIDXREG(SISSR, 0x00, 0x03);
	}
     }

}

/* Functions for adjusting various TV settings */

/* These are used by the PostSetMode() functions as well as
 * the (hopefully) upcoming display properties extension/tool.
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
 * The values will be stored in pSiSEnt if we're running dual.
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
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 10) | 0x01),0xF3);
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
           return(int)(((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x0c) >> 2) * 6);
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
	      SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 12) | 0x01),0xCF);
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
              SiS_SetCH70xxANDOR(pSiS->SiS_Pr, ((val << 8) | 0x01),0xFC);
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
	   return(int)((SiS_GetCH70xx(pSiS->SiS_Pr, 0x01) & 0x03) * 6);
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
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_301)) return;
   
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
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_301 && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->sistvedgeenhance;
      else 
#endif      
           return (int)pSiS->sistvedgeenhance;
   } else {
      unsigned char temp;
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif      
      inSISIDXREG(SISPART2, 0x3a, temp);
      return(int)(((temp & 0xe0) >> 5) * 2);
   }
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
   
   if(!(pSiS->VBFlags & CRT2_TV)) return;
   if(!(pSiS->VBFlags & VB_SISBRIDGE)) return;
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   val /= 2;
   if((val >= 0) && (val <= 7)) {
      setSISIDXREG(SISPART2,0x0A,0x8F, (val << 4));
   }
}

int SiS_GetSISTVantiflicker(ScrnInfoPtr pScrn)
{
   SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

   if(!(pSiS->VBFlags & VB_SISBRIDGE && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->sistvantiflicker;
      else
#endif      
           return (int)pSiS->sistvantiflicker;  
   } else {
      unsigned char temp;
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif      
      inSISIDXREG(SISPART2, 0x0a, temp);
      return(int)(((temp & 0x70) >> 4) * 2);
   }
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
#ifdef SISDUALHEAD
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
   
   if(!(pSiS->VBFlags & VB_SISBRIDGE && pSiS->VBFlags & CRT2_TV)) {
#ifdef SISDUALHEAD   
      if(pSiSEnt && pSiS->DualHeadMode) 
           return (int)pSiSEnt->sistvsaturation;
      else
#endif      
           return (int)pSiS->sistvsaturation;
   } else {
      unsigned char temp;
#ifdef UNLOCK_ALWAYS
      sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif      
      inSISIDXREG(SISPART4, 0x21, temp);
      return(int)((temp & 0x07) * 2);
   }
}

void SiS_SetSIS6326TVantiflicker(ScrnInfoPtr pScrn, int val)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char tmp;
   
   pSiS->sis6326antiflicker = val;

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
      return (int)pSiS->sis6326antiflicker;
   }
   
#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
   
   tmp = SiS6326GetTVReg(pScrn,0x00);
   if(!(tmp & 0x04)) {
      return (int)pSiS->sis6326antiflicker;
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
	       /* TO DO */
	       break;
	    }
	    
	 } else if(pSiS->VBFlags & VB_SISBRIDGE) {
	 
	    if((val >= -32) && (val <= 32)) {
		unsigned char p2_1f,p2_2b,p2_2c,p2_2d,p2_43;
		const unsigned char p2_left_ntsc[8][4] = {
			{ 0x48, 0x63, 0x49, 0xf4 },
			{ 0x45, 0x60, 0x46, 0xf1 },
			{ 0x43, 0x6e, 0x44, 0xff },
			{ 0x40, 0x6b, 0x41, 0xfc },
			{ 0x3e, 0x69, 0x3f, 0xfa },
			{ 0x3c, 0x67, 0x3d, 0xf8 },
			{ 0x39, 0x64, 0x3a, 0xf5 },
			{ 0x37, 0x62, 0x38, 0xf3 }
		};
		const unsigned char p2_right_ntsc[8][4] = {
			{ 0x4b, 0x66, 0x4c, 0xf7 },
			{ 0x4c, 0x67, 0x4d, 0xf8 },
			{ 0x4e, 0x69, 0x4f, 0xfa },
			{ 0x4f, 0x6a, 0x50, 0xfb },
			{ 0x51, 0x6c, 0x52, 0xfd },
			{ 0x53, 0x6e, 0x54, 0xff },
			{ 0x55, 0x60, 0x56, 0xf1 },
			{ 0x56, 0x61, 0x57, 0xf2 }
		};
		const unsigned char p2_left_pal[8][4] = {
			{ 0x5b, 0x66, 0x5c, 0x87 },
			{ 0x59, 0x64, 0x5a, 0x85 },
			{ 0x56, 0x61, 0x57, 0x82 },
			{ 0x53, 0x6e, 0x54, 0x8f },
			{ 0x50, 0x6b, 0x51, 0x8c },
			{ 0x4d, 0x68, 0x4e, 0x89 },
			{ 0x4a, 0x65, 0x4b, 0x86 },
			{ 0x49, 0x64, 0x4a, 0x85 }
		};
		const unsigned char p2_right_pal[8][4] = {
			{ 0x5f, 0x6a, 0x60, 0x8b },
			{ 0x61, 0x6c, 0x62, 0x8d },
			{ 0x63, 0x6e, 0x64, 0x8f },
			{ 0x65, 0x60, 0x66, 0x81 },
			{ 0x66, 0x61, 0x67, 0x82 },
			{ 0x68, 0x63, 0x69, 0x84 },
			{ 0x69, 0x64, 0x6a, 0x85 },
			{ 0x6b, 0x66, 0x6c, 0x87 }
		};
		val /= 4;
		p2_2d = pSiS->p2_2d;
#ifdef SISDUALHEAD
	        if(pSiSEnt && pSiS->DualHeadMode) p2_2d = pSiSEnt->p2_2d;
#endif		
		p2_2d &= 0xf0;
		if(val < 0) {
		      val = -val;
		      if(val == 8) val = 7;
		      if(pSiS->VBFlags & TV_PAL) {
		         p2_1f = p2_left_pal[val][0];
		         p2_2b = p2_left_pal[val][1];
		         p2_2c = p2_left_pal[val][2];
		         p2_2d |= (p2_left_pal[val][3] & 0x0f);
		      } else {
		         p2_1f = p2_left_ntsc[val][0];
		         p2_2b = p2_left_ntsc[val][1];
		         p2_2c = p2_left_ntsc[val][2];
		         p2_2d |= (p2_left_ntsc[val][3] & 0x0f);
		      }
		} else {
		      if(val == 8) val = 7;
		      if(pSiS->VBFlags & TV_PAL) {
		         p2_1f = p2_right_pal[val][0];
		         p2_2b = p2_right_pal[val][1];
		         p2_2c = p2_right_pal[val][2];
		         p2_2d |= (p2_right_pal[val][3] & 0x0f);
		      } else {
		         p2_1f = p2_right_ntsc[val][0];
		         p2_2b = p2_right_ntsc[val][1];
		         p2_2c = p2_right_ntsc[val][2];
		         p2_2d |= (p2_right_ntsc[val][3] & 0x0f);
		      }
		}
		p2_43 = p2_1f + 3;
		SISWaitRetraceCRT2(pScrn);
	        outSISIDXREG(SISPART2,0x1f,p2_1f);
		outSISIDXREG(SISPART2,0x2b,p2_2b);
		outSISIDXREG(SISPART2,0x2c,p2_2c);
		outSISIDXREG(SISPART2,0x2d,p2_2d);
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
	       /* TO DO */
	       break;
	    }
	    
	 } else if(pSiS->VBFlags & VB_SISBRIDGE) {
	 
	    if((val >= -32) && (val <= 32)) {
		char p2_01, p2_02;
		val /= 4;
		p2_01 = pSiS->p2_01;
		p2_02 = pSiS->p2_02;
#ifdef SISDUALHEAD
	        if(pSiSEnt && pSiS->DualHeadMode) {
		   p2_01 = pSiSEnt->p2_01;
		   p2_02 = pSiSEnt->p2_02;
		}
#endif
		p2_01 += (val * 2);
		p2_02 += (val * 2);
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

/* TW: Disable CRT1 for saving bandwidth. This doesn't work with VESA;
 *     VESA uses the bridge in SlaveMode and switching CRT1 off while the
 *     bridge is in SlaveMode not that clever...
 */
void SiSPostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
    SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif
    unsigned char usScratchCR17;
    Bool flag = FALSE;
    Bool doit = TRUE;
    int temp;

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	"CRT1off is %d\n", pSiS->CRT1off);
#endif

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    if((!pSiS->UseVESA) && (pSiS->VBFlags & CRT2_ENABLE)) {

	if(pSiS->VBFlags != pSiS->VBFlags_backup) {
		pSiS->VBFlags = pSiS->VBFlags_backup;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"VBFlags restored to %0lx\n", pSiS->VBFlags);
	}

	/* TW: -) We can't switch off CRT1 if bridge is in SlaveMode.
	 *     -) If we change to a SlaveMode-Mode (like 512x384), we
	 *        need to adapt VBFlags for eg. Xv.
	 */
#ifdef SISDUALHEAD
	if(!pSiS->DualHeadMode) {
#endif
	   if(SiSBridgeIsInSlaveMode(pScrn))  {
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
	if(doit) {
           inSISIDXREG(SISCR, 0x17, usScratchCR17);
    	   if(pSiS->CRT1off) {
	        if(usScratchCR17 & 0x80) flag = TRUE;
		usScratchCR17 &= ~0x80;
    	   } else {
	        if(!(usScratchCR17 & 0x80)) flag = TRUE;
        	usScratchCR17 |= 0x80;
           }
	   outSISIDXREG(SISCR, 0x17, usScratchCR17);
	   /* TW: Reset only if status changed */
	   if(flag) {
	      outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
	      usleep(10000);
              outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */
	   }
	}
    }

    /* TW: Apply TV settings given by options
           Do this even in DualHeadMode:
	   - if this is called by SetModeCRT1, CRT2 mode has been reset by SetModeCRT1
	   - if this is called by SetModeCRT2, CRT2 mode has changed (duh!)
	   -> In both cases, the settings must be re-applied.
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
	     if(pSiSEnt && pSiS->DualHeadMode) {
	        pSiSEnt->tvx = pSiS->tvx;
		pSiSEnt->tvy = pSiS->tvy;
	     }
#endif	
	     break;
	  case CHRONTEL_701x:
	     /* TO DO */
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
	  int mytvxpos = pSiS->tvxpos;
	  int mytvypos = pSiS->tvypos;
#ifdef SISDUALHEAD
          if(pSiSEnt && pSiS->DualHeadMode) {
	     mysistvantiflicker = pSiSEnt->sistvantiflicker;
	     mysistvsaturation = pSiSEnt->sistvsaturation;
	     mytvxpos = pSiSEnt->tvxpos;
	     mytvypos = pSiSEnt->tvypos;
	  }
#endif	  
          /* Backup default TV position registers */
	  inSISIDXREG(SISPART2,0x2d,pSiS->p2_2d);
	  inSISIDXREG(SISPART2,0x01,pSiS->p2_01);
	  inSISIDXREG(SISPART2,0x02,pSiS->p2_02);
#ifdef SISDUALHEAD
	  if(pSiSEnt && pSiS->DualHeadMode) {
	        pSiSEnt->p2_2d = pSiS->p2_2d;
		pSiSEnt->p2_01 = pSiS->p2_01;
		pSiSEnt->p2_02 = pSiS->p2_02;
	  }
#endif	
          if((val = mysistvantiflicker) != -1) {
	     SiS_SetSISTVantiflicker(pScrn, val);
	  }
	  if((val = mysistvsaturation) != -1) {
	     SiS_SetSISTVsaturation(pScrn, val);
	  }
	  if((val = mytvxpos) != 0) {
	     SiS_SetTVxposoffset(pScrn, val);
	  }
	  if((val = mytvypos) != 0) {
	     SiS_SetTVyposoffset(pScrn, val); 
	 }
       }
    }

}

/* Post-set SiS6326 TV registers */
void SiS6326PostSetMode(ScrnInfoPtr pScrn, SISRegPtr sisReg)
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
    
    /* TW: Handle TVPosOffset options (BEFORE switching on TV) */
    if((val = pSiS->tvxpos) != 0) {
       SiS_SetTVxposoffset(pScrn, val);
    }
    if((val = pSiS->tvypos) != 0) {
       SiS_SetTVyposoffset(pScrn, val);
    }

    /* TW: Switch on TV output. This is rather complicated, but
     *     if we don't do it, TV output will flicker terribly.
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

    /* TW: Apply TV settings given by options */
    if((val = pSiS->sis6326antiflicker) != -1) {
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

/* TW: Build a list of the VESA modes the BIOS reports as valid */
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

	if ((mode = VBEGetModeInfo(pVbe, id)) == NULL)
	    continue;

	bpp = mode->BitsPerPixel;

	m = xnfcalloc(sizeof(sisModeInfoRec),1);
	m->width = mode->XResolution;
	m->height = mode->YResolution;
	m->bpp = bpp;
	m->n = id;
	m->next = pSiS->SISVESAModeList;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      "BIOS reported VESA mode 0x%x: x:%i y:%i bpp:%i\n",
	       m->n, m->width, m->height, m->bpp);

	pSiS->SISVESAModeList = m;

	VBEFreeModeInfo(mode);
    }
}

/* TW: Calc VESA mode from given resolution/depth */
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
      case 512:
          if(mode->VDisplay == 384)
             ModeIndex = VESAModeIndex_512x384[i];
          break;
      case 640:
          if(mode->VDisplay == 480)
             ModeIndex = VESAModeIndex_640x480[i];
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

/* TW: Calculate the vertical refresh rate from a mode */
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

/* TW: Calculate CR33 (rate index) for CRT1.
 *     Calculation is done using currentmode, therefore it is
 *     recommended to set VertRefresh and HorizSync to correct
 *     values in config file.
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
	       }
	    }
	}
	i++;
   }
   if(index > 0)
	return index;
   else {
        /* TW: Default Rate index */
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

   watchdog = 65536;
   while((!(inSISREG(SISINPSTAT) & 0x08)) && --watchdog);
   watchdog = 65536;
   while((inSISREG(SISINPSTAT) & 0x08) && --watchdog);
}

void
SISWaitRetraceCRT2(ScrnInfoPtr pScrn)
{
   SISPtr        pSiS = SISPTR(pScrn);
   int           watchdog;
   unsigned char temp, reg;

   switch(pSiS->VGAEngine) {
   case SIS_300_VGA:
   	reg = 0x28;
	break;
   case SIS_315_VGA:
   	reg = 0x33;
	break;
   default:
        return;
   }

   watchdog = 65536;
   do {
   	inSISIDXREG(SISPART1, reg, temp);
	if(temp & 0x80) break;
   } while(--watchdog);
   watchdog = 65536;
   do {
   	inSISIDXREG(SISPART1, reg, temp);
	if(!(temp & 0x80)) break;
   } while(--watchdog);
}

static void
SISWaitVBRetrace(ScrnInfoPtr pScrn)
{
   SISPtr  pSiS = SISPTR(pScrn);

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
          xf86DrvMsg(pSiS->pScrn->scrnIndex, X_ERROR,
               "Failed to unlock sr registers (%p, %x, 0x%02x; %d)\n",
	       pSiS, pSiS->RelIO, val, mylockcalls);
#ifdef TWDEBUG
          for(i = 0; i <= 0x3f; i++) {
	  	inSISIDXREG(SISSR, i, val1);
		inSISIDXREG(0x3c4, i, val2);
		xf86DrvMsg(pSiS->pScrn->scrnIndex, X_INFO,
			"SR%02d: RelIO=0x%02x 0x3c4=0x%02x (%d)\n", i, val1, val2, mylockcalls);
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
	     xf86DrvMsg(pSiS->pScrn->scrnIndex, X_ERROR,
	        "Failed to unlock cr registers (%p, %x, 0x%02x)\n",
	       pSiS, pSiS->RelIO, val);
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

