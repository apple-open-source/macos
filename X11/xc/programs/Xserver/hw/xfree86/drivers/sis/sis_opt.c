/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_opt.c,v 1.15 2003/02/04 02:44:29 dawes Exp $ */
/*
 *
 * SiS driver option evaluation
 *
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the supplier not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The supplier makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE SUPPLIER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  	?
 *		Thomas Winischhofer <thomas@winischhofer.net>
 */

#include "xf86.h"
#include "xf86PciInfo.h"
#include "xf86str.h"
#include "xf86Cursor.h"

#include "sis.h"

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
/*  OPTION_PCI_RETRY,  */
    OPTION_NOACCEL,
    OPTION_TURBOQUEUE,
    OPTION_FAST_VRAM,
    OPTION_NOHOSTBUS,
/*  OPTION_SET_MEMCLOCK,   */
    OPTION_FORCE_CRT2TYPE,
    OPTION_SHADOW_FB,
    OPTION_ROTATE,
    OPTION_NOXVIDEO,
    OPTION_VESA,
    OPTION_MAXXFBMEM,
    OPTION_FORCECRT1,
    OPTION_DSTN,
    OPTION_XVONCRT2,
    OPTION_PDC,
    OPTION_TVSTANDARD,
    OPTION_USEROMDATA,
    OPTION_NOINTERNALMODES,
    OPTION_USEOEM,
    OPTION_SBIOSN,
    OPTION_NOYV12,
    OPTION_CHTVOVERSCAN,
    OPTION_CHTVSOVERSCAN,
    OPTION_CHTVLUMABANDWIDTHCVBS,
    OPTION_CHTVLUMABANDWIDTHSVIDEO,
    OPTION_CHTVLUMAFLICKERFILTER,
    OPTION_CHTVCHROMABANDWIDTH,
    OPTION_CHTVCHROMAFLICKERFILTER,
    OPTION_CHTVCVBSCOLOR,
    OPTION_CHTVTEXTENHANCE,
    OPTION_CHTVCONTRAST,
    OPTION_SISTVEDGEENHANCE,
    OPTION_SISTVANTIFLICKER,
    OPTION_SISTVSATURATION,
    OPTION_TVXPOSOFFSET,
    OPTION_TVYPOSOFFSET,
    OPTION_SIS6326ANTIFLICKER,
    OPTION_SIS6326ENABLEYFILTER,
    OPTION_SIS6326YFILTERSTRONG,
    OPTION_CHTVTYPE,
    OPTION_USERGBCURSOR,
    OPTION_USERGBCURSORBLEND,
    OPTION_USERGBCURSORBLENDTH,
    OPTION_RESTOREBYSET
} SISOpts;

static const OptionInfoRec SISOptions[] = {
    { OPTION_SW_CURSOR,         "SWcursor",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_HW_CURSOR,         "HWcursor",               OPTV_BOOLEAN,   {0}, FALSE },
/*  { OPTION_PCI_RETRY,         "PciRetry",               OPTV_BOOLEAN,   {0}, FALSE },  */
    { OPTION_NOACCEL,           "NoAccel",                OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_TURBOQUEUE,        "TurboQueue",             OPTV_BOOLEAN,   {0}, FALSE },
/*  { OPTION_SET_MEMCLOCK,      "SetMClk",                OPTV_FREQ,      {0}, -1    },  */
    { OPTION_FAST_VRAM,         "FastVram",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOHOSTBUS,         "NoHostBus",              OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_FORCE_CRT2TYPE,    "ForceCRT2Type",          OPTV_ANYSTR,    {0}, FALSE },
    { OPTION_SHADOW_FB,         "ShadowFB",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_ROTATE,            "Rotate",                 OPTV_ANYSTR,    {0}, FALSE },
    { OPTION_NOXVIDEO,          "NoXvideo",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_VESA,		"Vesa",		          OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_MAXXFBMEM,         "MaxXFBMem",              OPTV_INTEGER,   {0}, -1    },
    { OPTION_FORCECRT1,         "ForceCRT1",              OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_DSTN,              "DSTN",                   OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_XVONCRT2,          "XvOnCRT2",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_PDC,               "PanelDelayCompensation", OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVSTANDARD,        "TVStandard",             OPTV_STRING,    {0}, -1    },
    { OPTION_USEROMDATA,	"UseROMData",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_NOINTERNALMODES,   "NoInternalModes",        OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_USEOEM, 		"UseOEMData",		  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SBIOSN,            "BIOSFile",               OPTV_STRING,    {0}, FALSE },
    { OPTION_NOYV12, 		"NoYV12",		  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVTYPE,		"CHTVType",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVOVERSCAN,	"CHTVOverscan",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVSOVERSCAN,	"CHTVSuperOverscan",      OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVLUMABANDWIDTHCVBS,	"CHTVLumaBandwidthCVBS",  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVLUMABANDWIDTHSVIDEO,	"CHTVLumaBandwidthSVIDEO",OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVLUMAFLICKERFILTER,	"CHTVLumaFlickerFilter",  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCHROMABANDWIDTH,	"CHTVChromaBandwidth",    OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCHROMAFLICKERFILTER,	"CHTVChromaFlickerFilter",OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCVBSCOLOR,	"CHTVCVBSColor",          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVTEXTENHANCE,	"CHTVTextEnhance",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCONTRAST,	"CHTVContrast",		  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVEDGEENHANCE,	"SISTVEdgeEnhance",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVANTIFLICKER,	"SISTVAntiFlicker",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVSATURATION,	"SISTVSaturation",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVXPOSOFFSET,	"TVXPosOffset", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVYPOSOFFSET,	"TVYPosOffset", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SIS6326ANTIFLICKER,	"SIS6326TVAntiFlicker",   OPTV_STRING,    {0}, FALSE  },
    { OPTION_SIS6326ENABLEYFILTER,	"SIS6326TVEnableYFilter", OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SIS6326YFILTERSTRONG,	"SIS6326TVYFilterStrong", OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_USERGBCURSOR, 	"UseColorHWCursor",	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_USERGBCURSORBLEND,	"ColorHWCursorBlending",  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_USERGBCURSORBLENDTH,	"ColorHWCursorBlendThreshold", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_RESTOREBYSET,	"RestoreBySetMode", 	  OPTV_BOOLEAN,   {0}, -1    },
    { -1,                       NULL,                     OPTV_NONE,      {0}, FALSE }
};

void
SiSOptions(ScrnInfoPtr pScrn)
{
    SISPtr      pSiS = SISPTR(pScrn);
    MessageType from;
/*  double      temp;  */
    char        *strptr;

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if (!(pSiS->Options = xalloc(sizeof(SISOptions))))
	return;

    memcpy(pSiS->Options, SISOptions, sizeof(SISOptions));

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pSiS->Options);

    /* initalize some defaults */
    pSiS->newFastVram = -1;	/* TW: Default: write only; if set, read + write */
    pSiS->NoHostBus = FALSE;
/*  pSiS->UsePCIRetry = TRUE; */
    pSiS->TurboQueue = TRUE;
    pSiS->HWCursor = TRUE;
    pSiS->Rotate = FALSE;
    pSiS->ShadowFB = FALSE;
    pSiS->VESA = -1;
    pSiS->NoXvideo = FALSE;
    pSiS->maxxfbmem = 0;
    pSiS->forceCRT1 = -1;
    pSiS->DSTN = FALSE;      /* TW: For using 550 FSTN/DSTN registers */
    pSiS->XvOnCRT2 = FALSE;  /* TW: For chipsets with only one overlay */
    pSiS->NoYV12 = -1;
    pSiS->PDC = -1;          /* TW: Panel Delay Compensation for 300 (and 310/325) series */
    pSiS->OptTVStand = -1;
    pSiS->OptROMUsage = -1;
    pSiS->noInternalModes = FALSE;
    pSiS->OptUseOEM = -1;
    pSiS->OptTVOver = -1;
    pSiS->OptTVSOver = -1;
    pSiS->chtvlumabandwidthcvbs = -1;	/* TW: Chrontel TV settings */
    pSiS->chtvlumabandwidthsvideo = -1;
    pSiS->chtvlumaflickerfilter = -1;
    pSiS->chtvchromabandwidth = -1;
    pSiS->chtvchromaflickerfilter = -1;
    pSiS->chtvcvbscolor = -1;
    pSiS->chtvtextenhance = -1;
    pSiS->chtvcontrast = -1;
    pSiS->sistvedgeenhance = -1;	/* TW: SiS30x TV settings */
    pSiS->sistvantiflicker = -1;
    pSiS->sistvsaturation = -1;
    pSiS->sis6326antiflicker = -1;	/* TW: SiS6326 TV settings */
    pSiS->sis6326enableyfilter = -1;
    pSiS->sis6326yfilterstrong = -1;
    pSiS->tvxpos = 0;			/* TW: Some day hopefully general TV settings */
    pSiS->tvypos = 0;
    pSiS->NonDefaultPAL = -1;
    pSiS->chtvtype = -1;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)    
    pSiS->OptUseColorCursor = 0;
#else
    if(pSiS->VGAEngine == SIS_300_VGA) {
    	pSiS->OptUseColorCursor = 0;
	pSiS->OptUseColorCursorBlend = 1;
	pSiS->OptColorCursorBlendThreshold = 0x37000000;
    } else if(pSiS->VGAEngine == SIS_315_VGA) {
    	pSiS->OptUseColorCursor = 1;
    }
#endif    
    pSiS->restorebyset = 0;

    if(pSiS->Chipset == PCI_CHIP_SIS530) {
    	 /* TW: TQ still broken on 530/620? */
	 pSiS->TurboQueue = FALSE;
    }

    /* sw/hw cursor */
    from = X_DEFAULT;
    if(xf86GetOptValBool(pSiS->Options, OPTION_HW_CURSOR, &pSiS->HWCursor)) {
        from = X_CONFIG;
    }
    if(xf86ReturnOptValBool(pSiS->Options, OPTION_SW_CURSOR, FALSE)) {
        from = X_CONFIG;
        pSiS->HWCursor = FALSE;
	pSiS->OptUseColorCursor = 0;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
                                pSiS->HWCursor ? "HW" : "SW");
				
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#ifdef ARGB_CURSOR
#ifdef SIS_ARGB_CURSOR		
    if((pSiS->HWCursor) && ((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA))) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_USERGBCURSOR, &pSiS->OptUseColorCursor)) {
    	   from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "Color HW cursor is %s\n",
                    pSiS->OptUseColorCursor ? "enabled" : "disabled");
		    
       if(pSiS->VGAEngine == SIS_300_VGA) {
          from = X_DEFAULT;
          if(xf86GetOptValBool(pSiS->Options, OPTION_USERGBCURSORBLEND, &pSiS->OptUseColorCursorBlend)) {
    	     from = X_CONFIG;
          }
          if(pSiS->OptUseColorCursor) {
             xf86DrvMsg(pScrn->scrnIndex, from,
	   	"HW cursor color blending emulation is %s\n",
		(pSiS->OptUseColorCursorBlend) ? "enabled" : "disabled");
	  }
	  { 
	  int temp;
	  from = X_DEFAULT;
	  if(xf86GetOptValInteger(pSiS->Options, OPTION_USERGBCURSORBLENDTH, &temp)) {
	     if((temp >= 0) && (temp <= 255)) {
	        from = X_CONFIG;
		pSiS->OptColorCursorBlendThreshold = (temp << 24);
	     } else {
	        temp = pSiS->OptColorCursorBlendThreshold >> 24;
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Illegal color HW cursor blending threshold, valid range 0-255\n");
	     }
	  }
	  if(pSiS->OptUseColorCursor) {
	     if(pSiS->OptUseColorCursorBlend) {
	        xf86DrvMsg(pScrn->scrnIndex, from,
	          "HW cursor color blending emulation threshold is %d\n", temp);
 	     }
	  }
	  }
       }
    }
#endif
#endif
#endif

    /* Accel */
    if(xf86ReturnOptValBool(pSiS->Options, OPTION_NOACCEL, FALSE)) {
        pSiS->NoAccel = TRUE;
	pSiS->NoXvideo = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration and Xv disabled\n");
    }

#if 0
    /* PCI retry - TW: What the heck is/was this for? */
    from = X_DEFAULT;
    if(xf86GetOptValBool(pSiS->Options, OPTION_PCI_RETRY, &pSiS->UsePCIRetry)) {
        from = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "PCI retry %s\n",
                         pSiS->UsePCIRetry ? "enabled" : "disabled");
#endif

    /* Mem clock */
#if 0  /* TW: This is not used */
    if(xf86GetOptValFreq(pSiS->Options, OPTION_SET_MEMCLOCK, OPTUNITS_MHZ,
                                                            &temp)) {
        pSiS->MemClock = (int)(temp * 1000.0);
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Memory clock set to %.3f MHz\n", pSiS->MemClock/1000.0);
    }
#endif

    /* Fast VRAM (not for 300/310/325 series) */
    if((pSiS->VGAEngine != SIS_300_VGA) && (pSiS->VGAEngine != SIS_315_VGA)) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_FAST_VRAM, &pSiS->newFastVram)) {
          from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "Fast VRAM %s\n",
                   (pSiS->newFastVram == -1) ? "enabled (for write only)" :
		   	(pSiS->newFastVram ? "enabled (for read and write)" : "disabled"));
    }

    /* NoHostBus (5597/5598 only) */
    if((pSiS->Chipset == PCI_CHIP_SIS5597)) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_NOHOSTBUS, &pSiS->NoHostBus)) {
          from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "SiS5597/5598 VGA-to-CPU host bus %s\n",
                   pSiS->NoHostBus ? "disabled" : "enabled");
    }

    if(pSiS->VGAEngine != SIS_315_VGA) {
       /* Turbo QUEUE */
       /* (TW: We always use this on 310/325 series) */
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_TURBOQUEUE, &pSiS->TurboQueue)) {
    	   from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "TurboQueue %s\n",
                    pSiS->TurboQueue ? "enabled" : "disabled");
    }

    /* Force CRT2 type (300/310/325 series only)
       TW: SVIDEO, COMPOSITE and SCART for overriding detection
     */
    pSiS->ForceCRT2Type = CRT2_DEFAULT;
    pSiS->ForceTVType = -1;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
      strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_FORCE_CRT2TYPE);
      if(strptr != NULL) {
        if((!strcmp(strptr,"TV")) || (!strcmp(strptr,"tv")))
            pSiS->ForceCRT2Type = CRT2_TV;
 	else if((!strcmp(strptr,"SVIDEO")) || (!strcmp(strptr,"svideo"))) {
            pSiS->ForceCRT2Type = CRT2_TV;
	    pSiS->ForceTVType = TV_SVIDEO;
        } else if((!strcmp(strptr,"COMPOSITE")) || (!strcmp(strptr,"composite"))) {
            pSiS->ForceCRT2Type = CRT2_TV;
	    pSiS->ForceTVType = TV_AVIDEO;
        } else if((!strcmp(strptr,"SCART")) || (!strcmp(strptr,"scart"))) {
            pSiS->ForceCRT2Type = CRT2_TV;
	    pSiS->ForceTVType = TV_SCART;
        } else if((!strcmp(strptr,"LCD")) || (!strcmp(strptr,"lcd")))
            pSiS->ForceCRT2Type = CRT2_LCD;
        else if((!strcmp(strptr,"DVI")) || (!strcmp(strptr,"dvi")))
            pSiS->ForceCRT2Type = CRT2_LCD;	    
        else if((!strcmp(strptr,"VGA")) || (!strcmp(strptr,"vga")))
            pSiS->ForceCRT2Type = CRT2_VGA;
        else if((!strcmp(strptr,"NONE")) || (!strcmp(strptr,"none")))
            pSiS->ForceCRT2Type = 0;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    	"\"%s\" is not a valid parameter for Option \"ForceCRT2Type\"\n", strptr);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	        "Valid parameters are \"LCD\" (alias \"DVI\"), \"TV\", \"SVIDEO\", \"COMPOSITE\", \"SCART\", \"VGA\" or \"NONE\"\n");
	}

        if(pSiS->ForceCRT2Type != CRT2_DEFAULT)
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "CRT2 type shall be %s\n", strptr);
      }
      strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CHTVTYPE);
      if(strptr != NULL) {
        if((!strcmp(strptr,"SCART")) || (!strcmp(strptr,"scart")))
            pSiS->chtvtype = 1;
 	else if((!strcmp(strptr,"HDTV")) || (!strcmp(strptr,"hdtv")))
	    pSiS->chtvtype = 0;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    	"\"%s\" is not a valid parameter for Option \"CHTVType\"\n", strptr);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	        "Valid parameters are \"SCART\" or \"HDTV\"\n");
	}
        if(pSiS->chtvtype != -1)
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Chrontel TV type shall be %s\n", strptr);
      }
    }

    /* ShadowFB */
    from = X_DEFAULT;
    if(xf86GetOptValBool(pSiS->Options, OPTION_SHADOW_FB, &pSiS->ShadowFB)) {
        from = X_CONFIG;
    }
    if(pSiS->ShadowFB) {
	pSiS->NoAccel = TRUE;
	pSiS->NoXvideo = TRUE;
    	xf86DrvMsg(pScrn->scrnIndex, from,
	   "Using \"Shadow Frame Buffer\" - acceleration and Xv disabled\n");
    }

    /* Rotate */
    if((strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_ROTATE))) {
        if(!xf86NameCmp(strptr, "CW")) {
            pSiS->ShadowFB = TRUE;
            pSiS->NoAccel = TRUE;
	    pSiS->NoXvideo = TRUE;
            pSiS->HWCursor = FALSE;
            pSiS->Rotate = 1;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Rotating screen clockwise (acceleration and Xv disabled)\n");
        } else
        if(!xf86NameCmp(strptr, "CCW")) {
            pSiS->ShadowFB = TRUE;
            pSiS->NoAccel = TRUE;
            pSiS->NoXvideo = TRUE;
            pSiS->HWCursor = FALSE;
            pSiS->Rotate = -1;
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Rotating screen counter clockwise (acceleration and Xv disabled)\n");
        } else {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                    "\"%s\" is not a valid parameter for Option \"Rotate\"\n", strptr);
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                    "Valid parameters are \"CW\" or \"CCW\"\n");
        }
    }
    
    /* RestoreBySetMode */
    /* TW: Set this to force the driver to set the old mode instead of restoring
     *     the register contents. This can be used to overcome problems with
     *     LCD panels and video bridges.
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       Bool val;
       if(xf86GetOptValBool(pSiS->Options, OPTION_RESTOREBYSET, &val)) {
          if(val) pSiS->restorebyset = TRUE;
	  else    pSiS->restorebyset = FALSE;
       }
    }
    
    /* NOXvideo:
     * Set this to TRUE to disable Xv hardware video acceleration
     */
    if(!pSiS->NoAccel) {
      if(xf86ReturnOptValBool(pSiS->Options, OPTION_NOXVIDEO, FALSE)) {
        pSiS->NoXvideo = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "XVideo extension disabled\n");
      }

      if(!pSiS->NoXvideo) {
        Bool val;
        if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
      	  /* TW: XvOnCRT2:
	   * On chipsets with only one overlay (315, 650), the user should
	   * choose to display the overlay on CRT1 or CRT2. By setting this
	   * option to TRUE, the overlay will be displayed on CRT2. The
	   * default is: CRT1 if only CRT1 available, CRT2 if only CRT2
	   * available, and CRT1 if both is available and detected.
	   */
          if(xf86GetOptValBool(pSiS->Options, OPTION_XVONCRT2, &val)) {
	        if(val) pSiS->XvOnCRT2 = TRUE;
	        else    pSiS->XvOnCRT2 = FALSE;
	  }
	}
	if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
	  /* TW: NoYV12 (for 5597/5598, 6326 and 530/620 only)
	   *     YV12 has problems with videos larger than 384x288. So
	   *     allow the user to disable YV12 support to force the
	   *     application to use YUV2 instead.
	   */
          if(xf86GetOptValBool(pSiS->Options, OPTION_NOYV12, &val)) {
	        if(val)  pSiS->NoYV12 = 1;
	        else     pSiS->NoYV12 = 0;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			"Xv YV12/I420 support is %s\n",
			pSiS->NoYV12 ? "disabled" : "enabled");
	  }
	}
      }
    }

    /* TW: VESA - DEPRECATED
     * This option is for forcing the driver to use
     * the VESA BIOS extension for mode switching.
     */
    {
	Bool val;

	if(xf86GetOptValBool(pSiS->Options, OPTION_VESA, &val)) {
	    if(val)  pSiS->VESA = 1;
	    else     pSiS->VESA = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "VESA: VESA usage shall be %s\n",
		       val ? "enabled":"disabled");
	}
    }

   /* TW: MaxXFBMem
    * With the option "MaxXFBMem" you can limit the amount of video memory X
    * uses for screen and off-screen buffers. This option should be used if
    * you intend to use DRI/DRM. The framebuffer driver required for DRM will
    * start its memory heap at 12MB if it detects more than 16MB, at 8MB if
    * between 8 and 16MB are available, otherwise at 4MB. So, if you limit
    * the amount of memory X uses, you avoid a clash between the framebuffer
    * driver and X as regards overwriting memory portions of each other.
    * The amount is to be specified in KB.
    */
    if(xf86GetOptValULong(pSiS->Options, OPTION_MAXXFBMEM,
                                &pSiS->maxxfbmem)) {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "MaxXFBMem: Framebuffer memory shall be limited to %d KB\n",
		    pSiS->maxxfbmem);
	    pSiS->maxxfbmem *= 1024;
    }

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       /* TW: ForceCRT1 (300/310/325 series only)
        * This option can be used to force CRT1 to be switched on/off. Its
        * intention is mainly for old monitors that can't be detected
        * automatically. This is only useful on machines with a video bridge.
        * In normal cases, this option won't be necessary.
        */
	Bool val;
	if(xf86GetOptValBool(pSiS->Options, OPTION_FORCECRT1, &val)) {
	    if(val)  pSiS->forceCRT1 = 1;
	    else     pSiS->forceCRT1 = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "CRT1 shall be forced to %s\n",
		       val ? "ON" : "OFF");
	}
    }

    if(pSiS->Chipset == PCI_CHIP_SIS550) {
      /* TW: SiS 550 DSTN/FSTN
       *     This is for notifying the driver to use the DSTN registers on 550.
       *     DSTN/FSTN is a special LCD port of the SiS550 (notably not the 551
       *     and 552, which I don't know how to detect) that uses an extended
       *     register range. The only effect of this option is that the driver
       *     saves and restores these registers. DSTN display modes are chosen
       *     by using resultion 320x480x8 or 320x480x16.
       */
      if(xf86ReturnOptValBool(pSiS->Options, OPTION_DSTN, FALSE)) {
        pSiS->DSTN = TRUE;
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "SiS 550 DSTN/FSTN enabled\n");
      } else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "SiS 550 DSTN/FSTN disabled\n");
      }
    }

    /* TW: PanelDelayCompensation (300/310/325 series only)
     *     This might be required if the LCD panel shows "small waves".
     *     The parameter is an integer, usually either 4, 32 or 24.
     *     Why this option? Simply because SiS did poor BIOS design.
     *     The PDC value depends on the very LCD panel used in a
     *     particular machine. For most panels, the driver is able
     *     to detect the correct value. However, some panels require
     *     a different setting. The value given must be within the mask 0x3c.
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
        if(xf86GetOptValInteger(pSiS->Options, OPTION_PDC, &pSiS->PDC)) {
	    if(pSiS->PDC & ~0x3c) {
	       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	           "Illegal PanelDelayCompensation value\n");
	       pSiS->PDC = -1;
	    } else {
               xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Panel delay compensation shall be %d\n",
		    pSiS->PDC);
	    }
        }
    }

    /* TW: TVStandard (300/310/325 series and 6326 w/ TV only)
     * This option is for overriding the autodetection of
     * the BIOS option for PAL / NTSC
     */
    if((pSiS->VGAEngine == SIS_300_VGA) ||
       (pSiS->VGAEngine == SIS_315_VGA) ||
       ((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV))) {
       strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_TVSTANDARD);
       if(strptr != NULL) {
          if((!strcmp(strptr,"PAL")) || (!strcmp(strptr,"pal")))
	     pSiS->OptTVStand = 1;
	  else if((!strcmp(strptr,"PALM")) || (!strcmp(strptr,"palm"))) {
	     pSiS->OptTVStand = 1;
	     pSiS->NonDefaultPAL = 1;
  	  } else if((!strcmp(strptr,"PALN")) || (!strcmp(strptr,"paln"))) {
	     pSiS->OptTVStand = 1;
	     pSiS->NonDefaultPAL = 0;
  	  } else if((!strcmp(strptr,"NTSC")) || (!strcmp(strptr,"ntsc")))
	     pSiS->OptTVStand = 0;
	  else {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"\"%s\" is not a valid parameter for Option \"TVStandard\"\n", strptr);
             xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	        "Valid options are \"PAL\", \"PALM\", \"PALN\" or \"NTSC\"\n");
	  }

	  if(pSiS->OptTVStand != -1) {
	     if(pSiS->Chipset == PCI_CHIP_SIS6326) {
	         pSiS->NonDefaultPAL = -1;
	         xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Standard shall be %s\n",
		       pSiS->OptTVStand ? "PAL" : "NTSC");
	     } else {
	         xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "TV Standard shall be %s\n",
		       (pSiS->OptTVStand ? 
		          ( (pSiS->NonDefaultPAL == -1) ? "PAL" : 
			      ((pSiS->NonDefaultPAL) ? "PALM" : "PALN") )
				           : "NTSC"));
	     }
	  }
        }
    }

    /* TW: TVOverscan (300/310/325 series only)
     * This option is for overriding the BIOS option for
     * TV Overscan. Some BIOS don't even have such an option.
     * This is only effective on LVDS+CHRONTEL 70xx systems.
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	Bool val;
	if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVOVERSCAN, &val)) {
	    if(val) pSiS->OptTVOver = 1;
	    else    pSiS->OptTVOver = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	               "Chrontel: TV overscan shall be %s\n",
		       val ? "enabled":"disabled");
	}
        if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVSOVERSCAN, &pSiS->OptTVSOver)) {
	      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	               "Chrontel: TV super overscan shall be %s\n",
		       pSiS->OptTVSOver ? "enabled":"disabled");
	}
    }

    /* TW: UseROMData (300/310/325 series only)
     * This option is enabling/disabling usage of some machine
     * specific data from the BIOS ROM. This option can - and
     * should - be used in case the driver makes problems
     * because SiS changed the location of this data.
     * TW: NoOEM (300/310/325 series only)
     * The driver contains quite a lot data for OEM LCD panels
     * and TV connector specifics which override the defaults.
     * If this data is incorrect, the TV may lose color and
     * the LCD panel might show some strange effects. Use this
     * option to disable the usage of this data.
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	Bool val;
	if(xf86GetOptValBool(pSiS->Options, OPTION_USEROMDATA, &val)) {
	    if(val)  pSiS->OptROMUsage = 1;
	    else     pSiS->OptROMUsage = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	               "Video ROM data usage shall be %s\n",
		       val ? "enabled":"disabled");
	}
	if(xf86GetOptValBool(pSiS->Options, OPTION_USEOEM, &val)) {
	    if(val)  pSiS->OptUseOEM = 1;
	    else     pSiS->OptUseOEM = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	               "Internal LCD/TV OEM data usage shall be %s\n",
		       val ? "enabled":"disabled");
	}
	pSiS->sbiosn = NULL;
        strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SBIOSN);
        if(strptr != NULL) {
	   pSiS->sbiosn = xalloc(strlen(strptr)+1);
           if(pSiS->sbiosn) strcpy(pSiS->sbiosn, strptr);
        }
    }

    /* TW: NoInternalModes (300/310/325 series only)
     *     Since the mode switching code for these chipsets is a
     *     Asm-to-C translation of BIOS code, we only have timings
     *     for a pre-defined number of modes. The default behavior
     *     is to replace XFree's default modes with a mode list
     *     generated out of the known and supported modes. Use
     *     this option to disable this. However, even if using
     *     out built-in mode list will NOT make it possible to
     *     use modelines.
     */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
        from = X_DEFAULT;
	if(xf86GetOptValBool(pSiS->Options, OPTION_NOINTERNALMODES, &pSiS->noInternalModes))
		from = X_CONFIG;

	xf86DrvMsg(pScrn->scrnIndex, from, "Usage of built-in modes is %s\n",
		       pSiS->noInternalModes ? "disabled":"enabled");
    }

    /* TW: Various parameters for TV output via SiS bridge, Chrontel or SiS6326 */
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
        int tmp = 0;
        xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMABANDWIDTHCVBS,
                                &pSiS->chtvlumabandwidthcvbs);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMABANDWIDTHSVIDEO,
                                &pSiS->chtvlumabandwidthsvideo);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMAFLICKERFILTER,
                                &pSiS->chtvlumaflickerfilter);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCHROMABANDWIDTH,
                                &pSiS->chtvchromabandwidth);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCHROMAFLICKERFILTER,
                                &pSiS->chtvchromaflickerfilter);
	xf86GetOptValBool(pSiS->Options, OPTION_CHTVCVBSCOLOR,
				&pSiS->chtvcvbscolor);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVTEXTENHANCE,
                                &pSiS->chtvtextenhance);
	xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCONTRAST,
                                &pSiS->chtvcontrast);
	xf86GetOptValInteger(pSiS->Options, OPTION_SISTVEDGEENHANCE,
                                &pSiS->sistvedgeenhance);
	xf86GetOptValInteger(pSiS->Options, OPTION_SISTVANTIFLICKER,
                                &pSiS->sistvantiflicker);
	xf86GetOptValInteger(pSiS->Options, OPTION_SISTVSATURATION,
                                &pSiS->sistvsaturation);
	xf86GetOptValInteger(pSiS->Options, OPTION_TVXPOSOFFSET,
                                &pSiS->tvxpos);
	xf86GetOptValInteger(pSiS->Options, OPTION_TVYPOSOFFSET,
                                &pSiS->tvypos);
	if(pSiS->tvxpos > 32)  { pSiS->tvxpos = 32;  tmp = 1; }
	if(pSiS->tvxpos < -32) { pSiS->tvxpos = -32; tmp = 1; }
	if(pSiS->tvypos > 32)  { pSiS->tvypos = 32;  tmp = 1; }
	if(pSiS->tvypos < -32) { pSiS->tvypos = -32;  tmp = 1; }
	if(tmp) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		      "Illegal TV x or y offset. Range is from -32 to 32\n");
    }
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
        int tmp = 0;
        strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SIS6326ANTIFLICKER);
        if (strptr != NULL) {
          if((!strcmp(strptr,"OFF")) || (!strcmp(strptr,"off")))
	     pSiS->sis6326antiflicker = 0;
  	  else if((!strcmp(strptr,"LOW")) || (!strcmp(strptr,"low")))
	     pSiS->sis6326antiflicker = 1;
	  else if((!strcmp(strptr,"MED")) || (!strcmp(strptr,"med")))
	     pSiS->sis6326antiflicker = 2;
	  else if((!strcmp(strptr,"HIGH")) || (!strcmp(strptr,"high")))
	     pSiS->sis6326antiflicker = 3;
	  else if((!strcmp(strptr,"ADAPTIVE")) || (!strcmp(strptr,"adaptive")))
	     pSiS->sis6326antiflicker = 4;
	  else {
	     pSiS->sis6326antiflicker = -1;
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"\"%s\" is not a valid parameter for Option \"SIS6326TVAntiFlicker\"\n", strptr);
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	        "Valid parameters are \"OFF\", \"LOW\", \"MED\", \"HIGH\" or \"ADAPTIVE\"\n");
	  }
	}
	xf86GetOptValBool(pSiS->Options, OPTION_SIS6326ENABLEYFILTER,
                                &pSiS->sis6326enableyfilter);
	xf86GetOptValBool(pSiS->Options, OPTION_SIS6326YFILTERSTRONG,
                                &pSiS->sis6326yfilterstrong);
	xf86GetOptValInteger(pSiS->Options, OPTION_TVXPOSOFFSET,
                                &pSiS->tvxpos);
	xf86GetOptValInteger(pSiS->Options, OPTION_TVYPOSOFFSET,
                                &pSiS->tvypos);
	if(pSiS->tvxpos > 16)  { pSiS->tvxpos = 16;  tmp = 1; }
	if(pSiS->tvxpos < -16) { pSiS->tvxpos = -16; tmp = 1; }
	if(pSiS->tvypos > 16)  { pSiS->tvypos = 16;  tmp = 1; }
	if(pSiS->tvypos < -16) { pSiS->tvypos = -16;  tmp = 1; }
	if(tmp) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		      "Illegal TV x or y offset. Range is from -16 to 16\n");
    }
}

const OptionInfoRec *
SISAvailableOptions(int chipid, int busid)
{
    return SISOptions;
}
