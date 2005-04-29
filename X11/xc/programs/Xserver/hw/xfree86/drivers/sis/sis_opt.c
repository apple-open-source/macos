/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_opt.c,v 1.57 2004/02/25 17:45:13 twini Exp $ */
/*
 * SiS driver option evaluation
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
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
 * "NoAccel", "NoXVideo", "SWCursor", "HWCursor" and "Rotate" option portions
 * Copyright (C) 1999-2004 The XFree86 Project, Inc. Licensed under the terms
 * of the XFree86 license (http://www.xfree86.org/current/LICENSE1.html).
 *
 * Authors:  	Thomas Winischhofer <thomas@winischhofer.net>
 *              ?
 */

#include "xf86.h"
#include "xf86PciInfo.h"
#include "xf86str.h"
#include "xf86Cursor.h"

#include "sis.h"

extern const customttable mycustomttable[];

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL,
    OPTION_TURBOQUEUE,
    OPTION_FAST_VRAM,
    OPTION_NOHOSTBUS,
    OPTION_RENDER,
    OPTION_FORCE_CRT1TYPE,
    OPTION_FORCE_CRT2TYPE,
    OPTION_YPBPRAR,
    OPTION_SHADOW_FB,
    OPTION_DRI,
    OPTION_AGP_SIZE,
    OPTION_AGP_SIZE2,
    OPTION_ROTATE,
    OPTION_NOXVIDEO,
    OPTION_VESA,
    OPTION_MAXXFBMEM,
    OPTION_FORCECRT1,
    OPTION_XVONCRT2,
    OPTION_PDC,
    OPTION_PDCA,
    OPTION_PDCS,
    OPTION_PDCAS,
    OPTION_EMI,
    OPTION_TVSTANDARD,
    OPTION_USEROMDATA,
    OPTION_NOINTERNALMODES,
    OPTION_USEOEM,
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
    OPTION_SISTVCHROMAFILTER,
    OPTION_SISTVLUMAFILTER,
    OPTION_SISTVCOLCALIBFINE,
    OPTION_SISTVCOLCALIBCOARSE,
    OPTION_TVXPOSOFFSET,
    OPTION_TVYPOSOFFSET,
    OPTION_TVXSCALE,
    OPTION_TVYSCALE,
    OPTION_SIS6326ANTIFLICKER,
    OPTION_SIS6326ENABLEYFILTER,
    OPTION_SIS6326YFILTERSTRONG,
    OPTION_SIS6326FORCETVPPLUG,
    OPTION_SIS6326FSCADJUST,
    OPTION_CHTVTYPE,
    OPTION_USERGBCURSOR,
    OPTION_USERGBCURSORBLEND,
    OPTION_USERGBCURSORBLENDTH,
    OPTION_RESTOREBYSET,
    OPTION_NODDCFORCRT2,
    OPTION_FORCECRT2REDETECTION,
    OPTION_SENSEYPBPR,
    OPTION_CRT1GAMMA,
    OPTION_CRT2GAMMA,
    OPTION_XVGAMMA,
    OPTION_XVDEFCONTRAST,
    OPTION_XVDEFBRIGHTNESS,
    OPTION_XVDEFHUE,
    OPTION_XVDEFSATURATION,
    OPTION_XVDEFDISABLEGFX,
    OPTION_XVDEFDISABLEGFXLR,
    OPTION_XVMEMCPY,
    OPTION_XVUSECHROMAKEY,
    OPTION_XVCHROMAMIN,
    OPTION_XVCHROMAMAX,
    OPTION_XVDISABLECOLORKEY,
    OPTION_XVINSIDECHROMAKEY,
    OPTION_XVYUVCHROMAKEY,
    OPTION_SCALELCD,
    OPTION_CENTERLCD,
    OPTION_SPECIALTIMING,
    OPTION_LVDSHL,
    OPTION_ENABLEHOTKEY,
    OPTION_MERGEDFB,
    OPTION_MERGEDFBAUTO,
    OPTION_CRT2HSYNC,
    OPTION_CRT2VREFRESH,
    OPTION_CRT2POS,
    OPTION_METAMODES,
    OPTION_MERGEDFB2,
    OPTION_CRT2HSYNC2,
    OPTION_CRT2VREFRESH2,
    OPTION_CRT2POS2,
    OPTION_NOSISXINERAMA,
    OPTION_NOSISXINERAMA2,
    OPTION_CRT2ISSCRN0,
    OPTION_MERGEDDPI,
    OPTION_ENABLESISCTRL,
    OPTION_STOREDBRI,
    OPTION_STOREDPBRI,
#ifdef SIS_CP
    SIS_CP_OPT_OPTIONS
#endif
    OPTION_PSEUDO
} SISOpts;

static const OptionInfoRec SISOptions[] = {
    { OPTION_SW_CURSOR,         	"SWcursor",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_HW_CURSOR,         	"HWcursor",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOACCEL,           	"NoAccel",                OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_TURBOQUEUE,        	"TurboQueue",             OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_FAST_VRAM,         	"FastVram",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOHOSTBUS,         	"NoHostBus",              OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_RENDER,        		"RenderAcceleration",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_FORCE_CRT1TYPE,    	"ForceCRT1Type",          OPTV_STRING,    {0}, FALSE },
    { OPTION_FORCE_CRT2TYPE,    	"ForceCRT2Type",          OPTV_STRING,    {0}, FALSE },
    { OPTION_YPBPRAR,  		  	"YPbPrAspectRatio",       OPTV_STRING,    {0}, FALSE },
    { OPTION_SHADOW_FB,         	"ShadowFB",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_DRI,         		"DRI",               	  OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_AGP_SIZE,			"AGPSize",      	  OPTV_INTEGER,   {0}, FALSE },
    { OPTION_AGP_SIZE2,			"GARTSize",      	  OPTV_INTEGER,   {0}, FALSE },
    { OPTION_ROTATE,            	"Rotate",                 OPTV_STRING,    {0}, FALSE },
    { OPTION_NOXVIDEO,          	"NoXvideo",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_VESA,			"Vesa",		          OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_MAXXFBMEM,         	"MaxXFBMem",              OPTV_INTEGER,   {0}, -1    },
    { OPTION_FORCECRT1,         	"ForceCRT1",              OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_XVONCRT2,          	"XvOnCRT2",               OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_PDC,               	"PanelDelayCompensation", OPTV_INTEGER,   {0}, -1    },
    { OPTION_PDCA,               	"PanelDelayCompensation1",OPTV_INTEGER,   {0}, -1    },
    { OPTION_PDCS,               	"PDC", 			  OPTV_INTEGER,   {0}, -1    },
    { OPTION_PDCAS,               	"PDC1",			  OPTV_INTEGER,   {0}, -1    },
    { OPTION_EMI,               	"EMI", 			  OPTV_INTEGER,   {0}, -1    },
    { OPTION_LVDSHL,			"LVDSHL", 	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SPECIALTIMING,        	"SpecialTiming",          OPTV_STRING,    {0}, -1    },
    { OPTION_TVSTANDARD,        	"TVStandard",             OPTV_STRING,    {0}, -1    },
    { OPTION_USEROMDATA,		"UseROMData",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_NOINTERNALMODES,   	"NoInternalModes",        OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_USEOEM, 			"UseOEMData",		  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_NOYV12, 			"NoYV12",		  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVTYPE,			"CHTVType",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVOVERSCAN,		"CHTVOverscan",	          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVSOVERSCAN,		"CHTVSuperOverscan",      OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVLUMABANDWIDTHCVBS,	"CHTVLumaBandwidthCVBS",  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVLUMABANDWIDTHSVIDEO,	"CHTVLumaBandwidthSVIDEO",OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVLUMAFLICKERFILTER,	"CHTVLumaFlickerFilter",  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCHROMABANDWIDTH,	"CHTVChromaBandwidth",    OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCHROMAFLICKERFILTER,	"CHTVChromaFlickerFilter",OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCVBSCOLOR,		"CHTVCVBSColor",          OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CHTVTEXTENHANCE,		"CHTVTextEnhance",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_CHTVCONTRAST,		"CHTVContrast",		  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVEDGEENHANCE,		"SISTVEdgeEnhance",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVANTIFLICKER,		"SISTVAntiFlicker",	  OPTV_STRING,    {0}, FALSE },
    { OPTION_SISTVSATURATION,		"SISTVSaturation",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVCHROMAFILTER,		"SISTVCFilter",      	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SISTVLUMAFILTER,		"SISTVYFilter",	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVCOLCALIBFINE,		"SISTVColorCalibFine",	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SISTVCOLCALIBCOARSE,	"SISTVColorCalibCoarse",  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVXSCALE,			"SISTVXScale", 	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVYSCALE,			"SISTVYScale", 	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVXPOSOFFSET,		"TVXPosOffset", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_TVYPOSOFFSET,		"TVYPosOffset", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_SIS6326ANTIFLICKER,	"SIS6326TVAntiFlicker",   OPTV_STRING,    {0}, FALSE  },
    { OPTION_SIS6326ENABLEYFILTER,	"SIS6326TVEnableYFilter", OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SIS6326YFILTERSTRONG,	"SIS6326TVYFilterStrong", OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SIS6326FORCETVPPLUG,	"SIS6326TVForcePlug",     OPTV_STRING,    {0}, -1    },
    { OPTION_SIS6326FSCADJUST,		"SIS6326FSCAdjust", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_USERGBCURSOR, 		"UseColorHWCursor",	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_USERGBCURSORBLEND,		"ColorHWCursorBlending",  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_USERGBCURSORBLENDTH,	"ColorHWCursorBlendThreshold", OPTV_INTEGER,{0},-1   },
    { OPTION_RESTOREBYSET,		"RestoreBySetMode", 	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_NODDCFORCRT2,		"NoCRT2Detection", 	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_FORCECRT2REDETECTION,	"ForceCRT2ReDetection",   OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SENSEYPBPR,		"SenseYPbPr",   	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CRT1GAMMA,			"CRT1Gamma", 	  	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CRT2GAMMA,			"CRT2Gamma", 	  	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_STOREDBRI,			"StoredGammaBrightness",  OPTV_STRING,    {0}, -1    },
    { OPTION_STOREDPBRI,		"StoredGammaPreBrightness",OPTV_STRING,   {0}, -1    },
    { OPTION_XVGAMMA,			"XvGamma", 	  	  OPTV_STRING,    {0}, -1    },
    { OPTION_XVDEFCONTRAST,		"XvDefaultContrast", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVDEFBRIGHTNESS,		"XvDefaultBrightness", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVDEFHUE,			"XvDefaultHue", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVDEFSATURATION,		"XvDefaultSaturation", 	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVDEFDISABLEGFX,		"XvDefaultDisableGfx", 	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVDEFDISABLEGFXLR,		"XvDefaultDisableGfxLR",  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVCHROMAMIN,		"XvChromaMin", 	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVCHROMAMAX,		"XvChromaMax", 	  	  OPTV_INTEGER,   {0}, -1    },
    { OPTION_XVUSECHROMAKEY,		"XvUseChromaKey",         OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVINSIDECHROMAKEY,		"XvInsideChromaKey",      OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVYUVCHROMAKEY,		"XvYUVChromaKey",         OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVDISABLECOLORKEY,		"XvDisableColorKey",      OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_XVMEMCPY,			"XvUseMemcpy",  	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_SCALELCD,			"ScaleLCD",	   	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_CENTERLCD,			"CenterLCD",	   	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_ENABLEHOTKEY,		"EnableHotkey",	   	  OPTV_BOOLEAN,   {0}, -1    },
    { OPTION_ENABLESISCTRL,		"EnableSiSCtrl",   	  OPTV_BOOLEAN,   {0}, -1    },
#ifdef SISMERGED
    { OPTION_MERGEDFB,			"MergedFB",		  OPTV_BOOLEAN,	  {0}, FALSE },
    { OPTION_MERGEDFB2,			"TwinView",		  OPTV_BOOLEAN,	  {0}, FALSE },	  /* alias */
    { OPTION_MERGEDFBAUTO,		"MergedFBAuto",		  OPTV_BOOLEAN,	  {0}, FALSE },
    { OPTION_CRT2HSYNC,			"CRT2HSync",		  OPTV_STRING,	  {0}, FALSE },
    { OPTION_CRT2HSYNC2,		"SecondMonitorHorizSync", OPTV_STRING,	  {0}, FALSE },   /* alias */
    { OPTION_CRT2VREFRESH,		"CRT2VRefresh",		  OPTV_STRING,    {0}, FALSE },
    { OPTION_CRT2VREFRESH2,		"SecondMonitorVertRefresh", OPTV_STRING,  {0}, FALSE },   /* alias */
    { OPTION_CRT2POS,   		"CRT2Position",		  OPTV_STRING,	  {0}, FALSE },
    { OPTION_CRT2POS2,   		"TwinViewOrientation",	  OPTV_STRING,	  {0}, FALSE },   /* alias */
    { OPTION_METAMODES,   		"MetaModes",  		  OPTV_STRING,	  {0}, FALSE },
    { OPTION_MERGEDDPI,			"MergedDPI", 		  OPTV_STRING,	  {0}, FALSE },
#ifdef SISXINERAMA
    { OPTION_NOSISXINERAMA,		"NoMergedXinerama",	  OPTV_BOOLEAN,	  {0}, FALSE },
    { OPTION_NOSISXINERAMA2,		"NoTwinviewXineramaInfo", OPTV_BOOLEAN,   {0}, FALSE },   /* alias */
    { OPTION_CRT2ISSCRN0,		"MergedXineramaCRT2IsScreen0",OPTV_BOOLEAN,{0},FALSE },
#endif
#endif
#ifdef SIS_CP
    SIS_CP_OPTION_DETAIL
#endif
    { -1,                       	NULL,                     OPTV_NONE,      {0}, FALSE }
};

void
SiSOptions(ScrnInfoPtr pScrn)
{
    SISPtr      pSiS = SISPTR(pScrn);
    MessageType from;
    char        *strptr;
    static const char *mybadparm = "\"%s\" is is not a valid parameter for option \"%s\"\n";
    static const char *disabledstr = "disabled";
    static const char *enabledstr = "enabled";
    static const char *ilrangestr = "Illegal %s parameter. Valid range is %d through %d\n";

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);

    /* Process the options */
    if(!(pSiS->Options = xalloc(sizeof(SISOptions)))) return;

    memcpy(pSiS->Options, SISOptions, sizeof(SISOptions));

    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pSiS->Options);

    /* Set defaults */
    
    pSiS->newFastVram = -1;
    pSiS->NoHostBus = FALSE;
    pSiS->TurboQueue = TRUE;
#ifdef SISVRAMQ
    /* TODO: Option (315 series VRAM command queue) */
    /* But beware: sisfb does not know about this!!! */
    pSiS->cmdQueueSize = 512*1024;
#endif
    pSiS->doRender = TRUE;
    pSiS->HWCursor = TRUE;
    pSiS->Rotate = FALSE;
    pSiS->ShadowFB = FALSE;
    pSiS->loadDRI = FALSE;
    pSiS->agpWantedPages = AGP_PAGES;
    pSiS->VESA = -1;
    pSiS->NoXvideo = FALSE;
    pSiS->maxxfbmem = 0;
    pSiS->forceCRT1 = -1;
    pSiS->DSTN = FALSE;
    pSiS->FSTN = FALSE;
    pSiS->XvOnCRT2 = FALSE;
    pSiS->NoYV12 = -1;
    pSiS->PDC = -1;
    pSiS->PDCA = -1;
    pSiS->EMI = -1;
    pSiS->OptTVStand = -1;
    pSiS->OptROMUsage = -1;
    pSiS->noInternalModes = FALSE;
    pSiS->OptUseOEM = -1;
    pSiS->OptTVOver = -1;
    pSiS->OptTVSOver = -1;
    pSiS->chtvlumabandwidthcvbs = -1;
    pSiS->chtvlumabandwidthsvideo = -1;
    pSiS->chtvlumaflickerfilter = -1;
    pSiS->chtvchromabandwidth = -1;
    pSiS->chtvchromaflickerfilter = -1;
    pSiS->chtvcvbscolor = -1;
    pSiS->chtvtextenhance = -1;
    pSiS->chtvcontrast = -1;
    pSiS->sistvedgeenhance = -1;
    pSiS->sistvantiflicker = -1;
    pSiS->sistvsaturation = -1;
    pSiS->sistvcfilter = -1;
    pSiS->sistvyfilter = 1; /* 0 = off, 1 = default, 2-8 = filter no */
    pSiS->sistvcolcalibc = 0;
    pSiS->sistvcolcalibf = 0;
    pSiS->sis6326enableyfilter = -1;
    pSiS->sis6326yfilterstrong = -1;
    pSiS->sis6326tvplug = -1;
    pSiS->sis6326fscadjust = 0;
    pSiS->tvxpos = 0;
    pSiS->tvypos = 0;
    pSiS->tvxscale = 0;
    pSiS->tvyscale = 0;
    pSiS->NonDefaultPAL = pSiS->NonDefaultNTSC = -1;
    pSiS->chtvtype = -1;
    pSiS->restorebyset = TRUE;
    pSiS->nocrt2ddcdetection = FALSE;
    pSiS->forcecrt2redetection = TRUE;   /* default changed since 13/09/2003 */
    pSiS->SenseYPbPr = TRUE;
    pSiS->ForceCRT1Type = CRT1_VGA;
    pSiS->ForceCRT2Type = CRT2_DEFAULT;
    pSiS->ForceYPbPrAR = TV_YPBPR169;
    pSiS->ForceTVType = -1;
    pSiS->CRT1gamma = TRUE;
    pSiS->CRT1gammaGiven = FALSE;
    pSiS->CRT2gamma = TRUE;
    pSiS->XvGamma = FALSE;
    pSiS->XvGammaGiven = FALSE;
    pSiS->enablesisctrl = FALSE;
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       pSiS->XvDefBri = 10;
       pSiS->XvDefCon = 2;
    } else {
       pSiS->XvDefBri = 0;
       pSiS->XvDefCon = 4;
    }
    pSiS->XvDefHue = 0; 
    pSiS->XvDefSat = 0;
    pSiS->XvDefDisableGfx = FALSE;
    pSiS->XvDefDisableGfxLR = FALSE;
    pSiS->UsePanelScaler = -1;
    pSiS->CenterLCD = -1;
    pSiS->XvUseMemcpy = TRUE;
    pSiS->XvUseChromaKey = FALSE;
    pSiS->XvDisableColorKey = FALSE;
    pSiS->XvInsideChromaKey = FALSE;
    pSiS->XvYUVChromaKey = FALSE;
    pSiS->XvChromaMin = 0x000101fe;
    pSiS->XvChromaMax = 0x000101ff;
    pSiS->XvGammaRed = pSiS->XvGammaGreen = pSiS->XvGammaBlue =
       pSiS->XvGammaRedDef = pSiS->XvGammaGreenDef = pSiS->XvGammaBlueDef = 1000;
    pSiS->GammaBriR = pSiS->GammaBriG = pSiS->GammaBriB = 1000;
    pSiS->GammaPBriR = pSiS->GammaPBriG = pSiS->GammaPBriB = 1000;
    pSiS->HideHWCursor = FALSE;
    pSiS->HWCursorIsVisible = FALSE;
#ifdef SISMERGED
    pSiS->MergedFB = pSiS->MergedFBAuto = FALSE;
    pSiS->CRT2Position = sisRightOf;
    pSiS->CRT2HSync = NULL;
    pSiS->CRT2VRefresh = NULL;
    pSiS->MetaModes = NULL;
    pSiS->MergedFBXDPI = pSiS->MergedFBYDPI = 0;
#ifdef SISXINERAMA
    pSiS->UseSiSXinerama = TRUE;
    pSiS->CRT2IsScrn0 = FALSE;
#endif
#endif
#ifdef SIS_CP
    SIS_CP_OPT_DEFAULT
#endif

    /* Chipset dependent defaults */

    if(pSiS->Chipset == PCI_CHIP_SIS530) {
    	 /* TW: TQ still broken on 530/620? */
	 pSiS->TurboQueue = FALSE;
    }

    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
         pSiS->newFastVram = 1;
    }

    if(pSiS->sishw_ext.jChipType == SIS_315H ||
       pSiS->sishw_ext.jChipType == SIS_315) {
         /* Cursor engine seriously broken */
         pSiS->HWCursor = FALSE;
    }

    /* DRI only supported on 300 series,
     * so don't load DRI by default on
     * others.
     */
    if(pSiS->VGAEngine == SIS_300_VGA) {
       pSiS->loadDRI = TRUE;
    }

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

    if(pSiS->VGAEngine == SIS_300_VGA) {
       pSiS->AllowHotkey = 0;
    } else if(pSiS->VGAEngine == SIS_315_VGA) {
       pSiS->AllowHotkey = 1;
    }

    /* Collect the options */

    /* FastVRAM (5597/5598, 6326 and 530/620 only)
     */
    if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_FAST_VRAM, &pSiS->newFastVram)) {
          from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "Fast VRAM %s\n",
                   (pSiS->newFastVram == -1) ?
		         ((pSiS->oldChipset == OC_SIS620) ? "enabled (for read only)" :
			                                    "enabled (for write only)") :
		   	 (pSiS->newFastVram ? "enabled (for read and write)" : disabledstr));
    }

    /* NoHostBus (5597/5598 only)
     */
    if((pSiS->Chipset == PCI_CHIP_SIS5597)) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_NOHOSTBUS, &pSiS->NoHostBus)) {
          from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "SiS5597/5598 VGA-to-CPU host bus %s\n",
                   pSiS->NoHostBus ? disabledstr : enabledstr);
    }

    /* MaxXFBMem
     * This options limits the amount of video memory X uses for screen
     * and off-screen buffers. This option should be used if using DRI
     * is intended. The kernel framebuffer driver required for DRM will
     * start its memory heap at 12MB if it detects more than 16MB, at 8MB if
     * between 8 and 16MB are available, otherwise at 4MB. So, if the amount
     * of memory X uses, a clash between the framebuffer's memory heap
     * and X is avoided. The amount is to be specified in KB.
     */
    if(xf86GetOptValULong(pSiS->Options, OPTION_MAXXFBMEM,
                                &pSiS->maxxfbmem)) {
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "MaxXFBMem: Framebuffer memory shall be limited to %ld KB\n",
		    pSiS->maxxfbmem);
	    pSiS->maxxfbmem *= 1024;
    }

    /* NoAccel
     * Turns off 2D acceleration
     */
    if(xf86ReturnOptValBool(pSiS->Options, OPTION_NOACCEL, FALSE)) {
        pSiS->NoAccel = TRUE;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	pSiS->NoXvideo = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "2D Acceleration and Xv disabled\n");
#else
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "2D Acceleration disabled\n");
#endif

    }

    /* RenderAcceleration
     * En/Disables RENDER acceleration (315/330 series only)
     */
    if((pSiS->VGAEngine == SIS_315_VGA) && (!pSiS->NoAccel)) {
       if(xf86GetOptValBool(pSiS->Options, OPTION_RENDER, &pSiS->doRender)) {
          if(!pSiS->doRender) {
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "RENDER Acceleration disabled\n");
	  }
       }
    }

    /* SWCursor
     * HWCursor
     * Chooses whether to use the hardware or software cursor
     */
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

    /*
     * UseColorHWCursor
     * ColorHWCursorBlending
     * ColorHWCursorBlendThreshold
     *
     * Enable/disable color hardware cursors;
     * enable/disable color hw cursor emulation for 300 series
     * select emultation transparency threshold for 300 series
     *
     */
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
#ifdef ARGB_CURSOR
#ifdef SIS_ARGB_CURSOR
    if((pSiS->HWCursor) && ((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA))) {
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_USERGBCURSOR, &pSiS->OptUseColorCursor)) {
    	   from = X_CONFIG;
       }
       xf86DrvMsg(pScrn->scrnIndex, from, "Color HW cursor is %s\n",
                    pSiS->OptUseColorCursor ? enabledstr : disabledstr);

       if(pSiS->VGAEngine == SIS_300_VGA) {
          from = X_DEFAULT;
          if(xf86GetOptValBool(pSiS->Options, OPTION_USERGBCURSORBLEND, &pSiS->OptUseColorCursorBlend)) {
    	     from = X_CONFIG;
          }
          if(pSiS->OptUseColorCursor) {
             xf86DrvMsg(pScrn->scrnIndex, from,
	   	"HW cursor color blending emulation is %s\n",
		(pSiS->OptUseColorCursorBlend) ? enabledstr : disabledstr);
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
	  } else {
	        temp = pSiS->OptColorCursorBlendThreshold >> 24;
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

    /*
     * MergedFB
     *
     * Enable/disable and configure merged framebuffer mode
     *
     */
#ifdef SISMERGED
#ifdef SISDUALHEAD
    if(pSiS->DualHeadMode) {
       Bool val;
       if(xf86GetOptValBool(pSiS->Options, OPTION_MERGEDFB, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     "Option \"MergedFB\" cannot be used in Dual Head mode\n");
       }
    } else
#endif
    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       Bool val;
       if(xf86GetOptValBool(pSiS->Options, OPTION_MERGEDFB, &val)) {
	  if(val) {
	     pSiS->MergedFB = TRUE;
	     pSiS->MergedFBAuto = FALSE;
	  }
       } else if(xf86GetOptValBool(pSiS->Options, OPTION_MERGEDFB2, &val)) {
          if(val) {
	     pSiS->MergedFB = TRUE;
	     pSiS->MergedFBAuto = FALSE;
	  }
       }

       if(xf86GetOptValBool(pSiS->Options, OPTION_MERGEDFBAUTO, &val)) {
          if(!pSiS->MergedFB) {
	     if(val) {
	        pSiS->MergedFB = pSiS->MergedFBAuto = TRUE;
	     }
	  } else {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"Option \"MergedFB\" overrules option \"MergedFBAuto\"\n");
	  }
       }

       if(pSiS->MergedFB) {
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2POS);
	  if(!strptr) {
	     strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2POS2);
	  }
      	  if(strptr) {
       	     if(!xf86NameCmp(strptr,"LeftOf")) {
                pSiS->CRT2Position = sisLeftOf;
#ifdef SISXINERAMA
		pSiS->CRT2IsScrn0 = TRUE;
#endif
 	     } else if(!xf86NameCmp(strptr,"RightOf")) {
                pSiS->CRT2Position = sisRightOf;
#ifdef SISXINERAMA
		pSiS->CRT2IsScrn0 = FALSE;
#endif
	     } else if(!xf86NameCmp(strptr,"Above")) {
                pSiS->CRT2Position = sisAbove;
#ifdef SISXINERAMA
		pSiS->CRT2IsScrn0 = FALSE;
#endif
	     } else if(!xf86NameCmp(strptr,"Below")) {
                pSiS->CRT2Position = sisBelow;
#ifdef SISXINERAMA
		pSiS->CRT2IsScrn0 = TRUE;
#endif
	     } else if(!xf86NameCmp(strptr,"Clone")) {
                pSiS->CRT2Position = sisClone;
#ifdef SISXINERAMA
		pSiS->CRT2IsScrn0 = TRUE;
#endif
	     } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "CRT2Position");
	    	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	            "Valid parameters are \"RightOf\", \"LeftOf\", \"Above\", \"Below\", or \"Clone\"\n");
	     }
	  }
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_METAMODES);
	  if(strptr) {
	     pSiS->MetaModes = xalloc(strlen(strptr) + 1);
	     if(pSiS->MetaModes) memcpy(pSiS->MetaModes, strptr, strlen(strptr) + 1);
	  }
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2HSYNC);
	  if(!strptr) {
	     strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2HSYNC2);
	  }
	  if(strptr) {
	     pSiS->CRT2HSync = xalloc(strlen(strptr) + 1);
	     if(pSiS->CRT2HSync) memcpy(pSiS->CRT2HSync, strptr, strlen(strptr) + 1);
	  }
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2VREFRESH);
	  if(!strptr) {
	     strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CRT2VREFRESH2);
	  }
	  if(strptr) {
	     pSiS->CRT2VRefresh = xalloc(strlen(strptr) + 1);
	     if(pSiS->CRT2VRefresh) memcpy(pSiS->CRT2VRefresh, strptr, strlen(strptr) + 1);
	  }
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_MERGEDDPI);
	  if(strptr) {
	     int val1 = 0, val2 = 0;
	     sscanf(strptr, "%d %d", &val1, &val2);
	     if(val1 && val2) {
	        pSiS->MergedFBXDPI = val1;
		pSiS->MergedFBYDPI = val2;
	     } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "MergedDPI");
	     }
	  }
#ifdef SISXINERAMA
	  if(pSiS->MergedFB) {
	     if(xf86GetOptValBool(pSiS->Options, OPTION_NOSISXINERAMA, &val)) {
	        if(val) pSiS->UseSiSXinerama = FALSE;
	     } else if(xf86GetOptValBool(pSiS->Options, OPTION_NOSISXINERAMA2, &val)) {
	        if(val) pSiS->UseSiSXinerama = FALSE;
	     }
	     if(pSiS->UseSiSXinerama) {
	        if(xf86GetOptValBool(pSiS->Options, OPTION_CRT2ISSCRN0, &val)) {
		   pSiS->CRT2IsScrn0 = val ? TRUE : FALSE;
		}
	     }
	  }
#endif
       }
    }
#endif

    /* Some options can only be specified in the Master Head's Device
     * section. Here we give the user a hint in the log.
     */
#ifdef SISDUALHEAD
    if((pSiS->DualHeadMode) && (pSiS->SecondHead)) {
       static const char *mystring = "Option \"%s\" is only accepted in Master Head's device section\n";
       Bool val;
       int vali;
       if(pSiS->VGAEngine != SIS_315_VGA) {
          if(xf86GetOptValBool(pSiS->Options, OPTION_TURBOQUEUE, &val)) {
             xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "TurboQueue");
          }
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_RESTOREBYSET, &val)) {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "RestoreBySetMode");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_ENABLEHOTKEY, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "EnableHotKey");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_ENABLESISCTRL, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "EnableSiSCtrl");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_USEROMDATA, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "UseROMData");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_USEOEM, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "UseOEMData");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_FORCECRT1, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "ForceCRT1");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_NODDCFORCRT2, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "NoCRT2Detection");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_FORCECRT2REDETECTION, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "ForceCRT2ReDetection");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_SENSEYPBPR, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "SenseYPbPr");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_FORCE_CRT1TYPE)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "ForceCRT1Type");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_FORCE_CRT2TYPE)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "ForceCRT2Type");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_YPBPRAR)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "YPbPrAspectRatio");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_SCALELCD, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "ScaleLCD");
       }
        if(xf86GetOptValBool(pSiS->Options, OPTION_CENTERLCD, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "CenterLCD");
       }
       if((xf86GetOptValInteger(pSiS->Options, OPTION_PDC, &vali)) ||
          (xf86GetOptValInteger(pSiS->Options, OPTION_PDCS, &vali))) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "PanelDelayCompensation (PDC)");
       }
       if((xf86GetOptValInteger(pSiS->Options, OPTION_PDCA, &vali)) ||
          (xf86GetOptValInteger(pSiS->Options, OPTION_PDCAS, &vali))) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "PanelDelayCompensation1 (PDC1)");
       }
       if(xf86GetOptValInteger(pSiS->Options, OPTION_EMI, &vali)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "EMI");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_SPECIALTIMING)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "SpecialTiming");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_LVDSHL)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "LVDSHL");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_TVSTANDARD)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "TVStandard");
       }
       if(xf86GetOptValString(pSiS->Options, OPTION_CHTVTYPE)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "CHTVType");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVOVERSCAN, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "CHTVOverscan");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVSOVERSCAN, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "CHTVSuperOverscan");
       }
       if((xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMABANDWIDTHCVBS, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMABANDWIDTHSVIDEO, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVLUMAFLICKERFILTER, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCHROMABANDWIDTH, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCHROMAFLICKERFILTER, &vali)) ||
          (xf86GetOptValBool(pSiS->Options, OPTION_CHTVCVBSCOLOR, &val)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVTEXTENHANCE, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_CHTVCONTRAST, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_SISTVEDGEENHANCE, &vali)) ||
	  (xf86GetOptValString(pSiS->Options, OPTION_SISTVANTIFLICKER)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_SISTVSATURATION, &vali)) ||
	  (xf86GetOptValBool(pSiS->Options, OPTION_SISTVCHROMAFILTER, &val)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_SISTVLUMAFILTER, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_SISTVCOLCALIBCOARSE, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_SISTVCOLCALIBFINE, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_TVXPOSOFFSET, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_TVYPOSOFFSET, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_TVXSCALE, &vali)) ||
	  (xf86GetOptValInteger(pSiS->Options, OPTION_TVYSCALE, &vali))) {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      "TV related options are only accepted in Master Head's device section");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_CRT2GAMMA, &val)) {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "CRT2Gamma");
       }
       if(xf86GetOptValBool(pSiS->Options, OPTION_XVONCRT2, &val)) {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mystring, "XvOnCRT2");
       }
#ifdef SIS_CP
       SIS_CP_OPT_DH_WARN
#endif
    } else
#endif    
    {
       if(pSiS->VGAEngine == SIS_315_VGA) {

#ifdef SISVRAMQ
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using VRAM command queue, size %dk\n",
	  	pSiS->cmdQueueSize / 1024);
#else
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using MMIO command queue, size 512k\n");
#endif

       } else {

	  /* TurboQueue */

          from = X_DEFAULT;
          if(xf86GetOptValBool(pSiS->Options, OPTION_TURBOQUEUE, &pSiS->TurboQueue)) {
    	     from = X_CONFIG;
          }
          xf86DrvMsg(pScrn->scrnIndex, from, "TurboQueue %s\n",
                    pSiS->TurboQueue ? enabledstr : disabledstr);
       }

       if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {

          Bool val;

	  /* RestoreBySetMode (300/315/330 series only)
           * Set this to force the driver to set the old mode instead of restoring
           * the register contents. This can be used to overcome problems with
           * LCD panels and video bridges.
           */
          if(xf86GetOptValBool(pSiS->Options, OPTION_RESTOREBYSET, &val)) {
             pSiS->restorebyset = val ? TRUE : FALSE;
          }

	  /* EnableHotkey (300/315/330 series only)
	   * Enables or disables the BIOS hotkey switch for
	   * switching the output device on laptops.
	   * This key causes a total machine hang on many 300 series
	   * machines, it is therefore by default disabled on such.
	   * In dual head mode, using the hotkey is lethal, so we
	   * forbid it then in any case.
	   * However, although the driver disables the hotkey as
	   * BIOS developers intented to do that, some buggy BIOSes
	   * still cause the machine to freeze. Hence the warning.
	   */
          {
	  int flag = 0;
#ifdef SISDUALHEAD
          if(pSiS->DualHeadMode) {
	     pSiS->AllowHotkey = 0;
	     flag = 1;
	  } else
#endif
	  if(xf86GetOptValBool(pSiS->Options, OPTION_ENABLEHOTKEY, &val)) {
	     pSiS->AllowHotkey = val ? 1 : 0;
          }
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Hotkey display switching is %s%s\n",
	        pSiS->AllowHotkey ? enabledstr : disabledstr,
	        flag ? " in dual head mode" : "");
	  if(pSiS->Chipset == PCI_CHIP_SIS630 ||
	     pSiS->Chipset == PCI_CHIP_SIS650 ||
	     pSiS->Chipset == PCI_CHIP_SIS660) {
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	         "WARNING: Using the Hotkey might freeze your machine, regardless\n");
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "\twhether enabled or disabled. This is no driver bug.\n");
	  }
	  }

          /* UseROMData (300/315/330 series only)
           * This option is enabling/disabling usage of some machine
           * specific data from the BIOS ROM. This option can - and
           * should - be used in case the driver makes problems
           * because SiS changed the location of this data.
           */
	  if(xf86GetOptValBool(pSiS->Options, OPTION_USEROMDATA, &val)) {
	     pSiS->OptROMUsage = val ? 1 : 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "Video ROM data usage shall be %s\n",
	          val ? enabledstr : disabledstr);
	  }

          /* UseOEMData (300/315/330 series only)
           * The driver contains quite a lot data for OEM LCD panels
           * and TV connector specifics which override the defaults.
           * If this data is incorrect, the TV may lose color and
           * the LCD panel might show some strange effects. Use this
           * option to disable the usage of this data.
           */
	  if(xf86GetOptValBool(pSiS->Options, OPTION_USEOEM, &val)) {
	     pSiS->OptUseOEM = val ? 1 : 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "Internal LCD/TV/VGA2 OEM data usage shall be %s\n",
	         val ? enabledstr : disabledstr);
	  }

	  /* NoCRT2DDCDetection (315/330 series only)
           * If set to true, this disables CRT2 detection using DDC. This is
           * to avoid problems with not entirely DDC compiant LCD panels or
           * VGA monitors connected to the secondary VGA plug. Since LCD and
           * VGA share the same DDC channel, it might in some cases be impossible
           * to determine if the device is a CRT monitor or a flat panel.
           */
          if(xf86GetOptValBool(pSiS->Options, OPTION_NODDCFORCRT2, &val)) {
             pSiS->nocrt2ddcdetection = val ? TRUE : FALSE;
          }

	  /* ForceCRT2ReDetection (315/330 series only)
           * If set to true, it forces re-detection of the LCD panel and
	   * a secondary VGA connection even if the BIOS already had found
	   * about it. This is meant for custom panels (ie such with
	   * non-standard resolutions) which the BIOS will "detect" according
	   * to the established timings, resulting in only a very vague idea
	   * about the panels real resolution. As for secondary VGA, this
	   * enables us to include a Plasma panel's proprietary modes.
           */
          if(xf86GetOptValBool(pSiS->Options, OPTION_FORCECRT2REDETECTION, &val)) {
             if(val) {
	             pSiS->forcecrt2redetection = TRUE;
		     pSiS->nocrt2ddcdetection = FALSE;
	     } else  pSiS->forcecrt2redetection = FALSE;
          }

	  /* SenseYPbPr (315/330 series only)
           * If set to true, the driver will sense for YPbPr TV. This is
	   * inconvenient for folks connecting SVideo and CVBS at the same
	   * time, because this condition will be detected as YPbPr (since
	   * the TV output pins are shared). "False" will not sense for
	   * YPbPr and detect SVideo or CVBS only.
           */
          if(xf86GetOptValBool(pSiS->Options, OPTION_SENSEYPBPR, &val)) {
             if(val) pSiS->SenseYPbPr = TRUE;
	     else    pSiS->SenseYPbPr = FALSE;
          }


	  /* ForceCRT1Type (315/330 series only)
	   * Used for forcing the driver to initialize CRT1 as
	   * VGA (analog) or LCDA (for simultanious LCD and TV
           * display) - on M650/651 and 661 or later with 301C/30xLV only!
           */
	  if(pSiS->VGAEngine == SIS_315_VGA) {
             strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_FORCE_CRT1TYPE);
             if(strptr != NULL) {
                if(!xf86NameCmp(strptr,"VGA")) {
                   pSiS->ForceCRT1Type = CRT1_VGA;
		} else if( (!xf86NameCmp(strptr,"LCD")) ||
		         (!xf86NameCmp(strptr,"LCDA")) ||
			 (!xf86NameCmp(strptr,"LCD-A")) ) {
		   pSiS->ForceCRT1Type = CRT1_LCDA;
		} else {
		   xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "ForceCRT1Type");
	           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	               "Valid parameters are \"VGA\" or \"LCD\"\n");
		}
	     }
	  }

	  /* ForceCRT1 (300/315/330 series only)
           * This option can be used to force CRT1 to be switched on/off. Its
           * intention is mainly for old monitors that can't be detected
           * automatically. This is only useful on machines with a video bridge.
           * In normal cases, this option won't be necessary.
           */
	  if(xf86GetOptValBool(pSiS->Options, OPTION_FORCECRT1, &val)) {
	     pSiS->forceCRT1 = val ? 1 : 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "CRT1 shall be forced to %s\n",
	         val ? "ON" : "OFF");
	     if(!pSiS->forceCRT1) pSiS->ForceCRT1Type = CRT1_VGA;
	  }

	  /* ForceCRT2Type (300/315/330 series only)
	   * Used for forcing the driver to initialize a given
	   * CRT2 device type.
           * (SVIDEO, COMPOSITE and SCART for overriding detection)
           */
          strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_FORCE_CRT2TYPE);
          if(strptr != NULL) {
             if(!xf86NameCmp(strptr,"TV"))
                pSiS->ForceCRT2Type = CRT2_TV;
 	     else if( (!xf86NameCmp(strptr,"SVIDEO")) ||
	     	      (!xf86NameCmp(strptr,"SVHS")) ) {
                pSiS->ForceCRT2Type = CRT2_TV;
	        pSiS->ForceTVType = TV_SVIDEO;
             } else if( (!xf86NameCmp(strptr,"COMPOSITE")) ||
	     		(!xf86NameCmp(strptr,"CVBS")) ) {
                pSiS->ForceCRT2Type = CRT2_TV;
	        pSiS->ForceTVType = TV_AVIDEO;
	     } else if( (!xf86NameCmp(strptr,"COMPOSITE SVIDEO")) || /* Ugly, but shorter than a parsing function */
	                (!xf86NameCmp(strptr,"COMPOSITE+SVIDEO")) ||
			(!xf86NameCmp(strptr,"SVIDEO+COMPOSITE")) ||
			(!xf86NameCmp(strptr,"SVIDEO COMPOSITE")) ) {
	        pSiS->ForceCRT2Type = CRT2_TV;
		pSiS->ForceTVType = (TV_SVIDEO | TV_AVIDEO);
             } else if(!xf86NameCmp(strptr,"SCART")) {
                pSiS->ForceCRT2Type = CRT2_TV;
	        pSiS->ForceTVType = TV_SCART;
             } else if((!xf86NameCmp(strptr,"LCD")) || (!xf86NameCmp(strptr,"DVI-D"))) {
	        if(pSiS->ForceCRT1Type == CRT1_VGA) {
                   pSiS->ForceCRT2Type = CRT2_LCD;
		} else {
		   pSiS->ForceCRT2Type = 0;
		   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   		"Can't set both CRT1 and CRT2 type to LCD; CRT2 disabled\n");
		}
             } else if((!xf86NameCmp(strptr,"VGA")) || (!xf86NameCmp(strptr,"DVI-A")))
                pSiS->ForceCRT2Type = CRT2_VGA;
             else if(!xf86NameCmp(strptr,"NONE"))
                pSiS->ForceCRT2Type = 0;
	     else if((!xf86NameCmp(strptr,"DSTN")) && (pSiS->Chipset == PCI_CHIP_SIS550)) {
		if(pSiS->ForceCRT1Type == CRT1_VGA) {
		   pSiS->ForceCRT2Type = CRT2_LCD;
		   pSiS->DSTN = TRUE;
		}
	     } else if((!xf86NameCmp(strptr,"FSTN")) && (pSiS->Chipset == PCI_CHIP_SIS550)) {
		if(pSiS->ForceCRT1Type == CRT1_VGA) {
		   pSiS->ForceCRT2Type = CRT2_LCD;
		   pSiS->FSTN = TRUE;
		}
#ifdef ENABLE_YPBPR
	     } else if(!xf86NameCmp(strptr,"HIVISION")) {
		pSiS->ForceCRT2Type = CRT2_TV;
	        pSiS->ForceTVType = TV_HIVISION;
	     } else if((!xf86NameCmp(strptr,"YPBPR1080I")) && (pSiS->VGAEngine == SIS_315_VGA)) {
	        pSiS->ForceCRT2Type = CRT2_TV;
		pSiS->ForceTVType = TV_YPBPR;
		pSiS->ForceYPbPrType = TV_YPBPR1080I;
	     } else if(((!xf86NameCmp(strptr,"YPBPR525I")) || (!xf86NameCmp(strptr,"YPBPR480I"))) &&
	               (pSiS->VGAEngine == SIS_315_VGA)) {
		pSiS->ForceCRT2Type = CRT2_TV;
		pSiS->ForceTVType = TV_YPBPR;
		pSiS->ForceYPbPrType = TV_YPBPR525I;
	     } else if(((!xf86NameCmp(strptr,"YPBPR525P")) || (!xf86NameCmp(strptr,"YPBPR480P"))) &&
	               (pSiS->VGAEngine == SIS_315_VGA)) {
		pSiS->ForceCRT2Type = CRT2_TV;
		pSiS->ForceTVType = TV_YPBPR;
		pSiS->ForceYPbPrType = TV_YPBPR525P;
	     } else if(((!xf86NameCmp(strptr,"YPBPR750P")) || (!xf86NameCmp(strptr,"YPBPR720P"))) &&
	               (pSiS->VGAEngine == SIS_315_VGA)) {
		pSiS->ForceCRT2Type = CRT2_TV;
		pSiS->ForceTVType = TV_YPBPR;
		pSiS->ForceYPbPrType = TV_YPBPR750P;
#endif
	     } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "ForceCRT2Type");
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	            "Valid parameters are \"LCD\" (=\"DVI-D\"), \"TV\", \"SVIDEO\", \"COMPOSITE\",\n"
		    "\t\"SVIDEO+COMPOSITE\", \"SCART\", \"VGA\" (=\"DVI-A\") or \"NONE\"; on the SiS550\n"
		    "\talso \"DSTN\" and \"FSTN\""
#ifdef ENABLE_YPBPR
		    				"; on SiS 301/301B bridges also \"HIVISION\", and on\n"
		    "\tSiS315/330 series with 301C/30xLV bridge also \"YPBPR480I\", \"YPBPR480P\",\n"
		    "\t\"YPBPR720P\" and \"YPBPR1080I\""
#endif
		    "\n");
	     }

             if(pSiS->ForceCRT2Type != CRT2_DEFAULT)
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "CRT2 type shall be %s\n", strptr);
          }

	  if(pSiS->ForceTVType == TV_YPBPR) {
	     strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_YPBPRAR);
             if(strptr != NULL) {
	        if(!xf86NameCmp(strptr,"4:3LB"))
                   pSiS->ForceYPbPrAR = TV_YPBPR43LB;
		else if(!xf86NameCmp(strptr,"4:3"))
                   pSiS->ForceYPbPrAR = TV_YPBPR43;
		else if(!xf86NameCmp(strptr,"16:9"))
                   pSiS->ForceYPbPrAR = TV_YPBPR169;
		else {
		   xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "YPbPrAspectRatio");
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	            	"Valid parameters are \"4:3LB\", \"4:3\" and \"16:9\"\n");
		}
	     }
	  }

	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SPECIALTIMING);
          if(strptr != NULL) {
	     int i = 0;
	     BOOLEAN found = FALSE;
	     if(!xf86NameCmp(strptr,"NONE")) {
	        pSiS->SiS_Pr->SiS_CustomT = CUT_FORCENONE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			"Special timing disabled\n");
	     } else {
	        while(mycustomttable[i].chipID != 0) {
	           if(!xf86NameCmp(strptr,mycustomttable[i].optionName)) {
		      pSiS->SiS_Pr->SiS_CustomT = mycustomttable[i].SpecialID;
		      found = TRUE;
		      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   	  "Special timing for %s %s forced\n",
			  mycustomttable[i].vendorName, mycustomttable[i].cardName);
		      break;
		   }
		   i++;
	        }
		if(!found) {
		   xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "SpecialTiming");
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Valid parameters are:\n");
		   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "\t\"NONE\" (to disable special timings)\n");
		   i = 0;
		   while(mycustomttable[i].chipID != 0) {
		      xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		        	"\t\"%s\" (for %s %s)\n",
				mycustomttable[i].optionName,
				mycustomttable[i].vendorName,
				mycustomttable[i].cardName);
		      i++;
		   }
                }
 	     }
	  }

	  /* EnableSiSCtrl */
	  /* Allow sisctrl tool to change driver settings */
	  from = X_DEFAULT;
	  if(xf86GetOptValBool(pSiS->Options, OPTION_ENABLESISCTRL, &val)) {
             if(val) pSiS->enablesisctrl = TRUE;
	     from = X_CONFIG;
          }
	  xf86DrvMsg(pScrn->scrnIndex, from, "SiSCtrl utility interface is %s\n",
	  	pSiS->enablesisctrl ? enabledstr : disabledstr);

         /* ScaleLCD (300/315/330 series only)
          * Can be used to force the bridge/panel link to [do|not do] the
	  * scaling of modes lower than the panel's native resolution.
          * Setting this to TRUE will force the bridge/panel link
	  * to scale; FALSE will rely on the panel's capabilities.
	  * Not supported on all machines.
          */
	  if(xf86GetOptValBool(pSiS->Options, OPTION_SCALELCD, &val)) {
	     pSiS->UsePanelScaler = val ? 0 : 1;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "LCD scaling is %s\n",
	         pSiS->UsePanelScaler ? disabledstr : enabledstr);
	  }

	 /* CenterLCD (300/315/330 + SiS video bridge only)
          * If LCD shall not be scaled, this selects whether 1:1 data
	  * will be sent to the output, or the image shall be centered
	  * on the LCD. For LVDS panels, screen will always be centered,
	  * since these have no built-in scaler. For TMDS, this is
	  * selectable. Non-centered means that the driver will pass
	  * 1:1 data to the output and that the panel will have to
	  * scale by itself (if supported by the panel).
          */
	  if(xf86GetOptValBool(pSiS->Options, OPTION_CENTERLCD, &val)) {
	     pSiS->CenterLCD = val ? 1 : 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Non-scaled LCD output will %sbe centered\n",
	         pSiS->CenterLCD ? "not " : "");
	  }

         /* PanelDelayCompensation (300/315/330 series only)
          * This might be required if the LCD panel shows "small waves"
	  * or wrong colors.
          * The parameter is an integer, (on 300 series usually either
	  * 4, 32 or 24; on 315 series + LV bridge usually 3 or 51)
          * Why this option? Simply because SiS did poor BIOS design.
          * The PDC value depends on the very LCD panel used in a
          * particular machine. For most panels, the driver is able
          * to detect the correct value. However, some panels require
          * a different setting. For 300 series, the value given must
	  * be within the mask 0x3c. For 661 and later, if must be
	  * within the range of 0 to 31.
          */
	  {
	  int val = -1;
          xf86GetOptValInteger(pSiS->Options, OPTION_PDC, &val);
	  xf86GetOptValInteger(pSiS->Options, OPTION_PDCS, &val);
	  if(val != -1) {
	     pSiS->PDC = val;
	     if((pSiS->VGAEngine == SIS_300_VGA) && (pSiS->PDC & ~0x3c)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	            "Illegal PanelDelayCompensation parameter\n");
	        pSiS->PDC = -1;
	     } else {
	        if(pSiS->VGAEngine == SIS_315_VGA) pSiS->PDC &= 0x1f;
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "Panel delay compensation shall be %d (for LCD=CRT2)\n",
	             pSiS->PDC);
	     }
          }

	 /* PanelDelayCompensation1 (315 series only)
          * Same as above, but for LCD-via-CRT1 ("LCDA")
          */
	  if(pSiS->VGAEngine == SIS_315_VGA) {
	     val = -1;
             xf86GetOptValInteger(pSiS->Options, OPTION_PDCA, &val);
	     xf86GetOptValInteger(pSiS->Options, OPTION_PDCAS, &val);
	     if(val != -1) {
	        pSiS->PDCA = val;
	        if(pSiS->PDCA > 0x1f) {
	           xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	                "Illegal PanelDelayCompensation1 (PDC1) parameter (0 <= PDC1 <= 31\n");
	           pSiS->PDCA = -1;
	        } else {
                   xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       	"Panel delay compensation shall be %d (for LCD=CRT1)\n",
	                pSiS->PDCA);
	        }
	     }
	  }
	  }

	 /* LVDSHL (300/315/330 series + 30xLV bridge only)
          * This might be required if the LCD panel is too dark.
          * The parameter is an integer from 0 to 3.
          */
          if(xf86GetOptValInteger(pSiS->Options, OPTION_LVDSHL, &pSiS->SiS_Pr->LVDSHL)) {
	     if((pSiS->SiS_Pr->LVDSHL < 0) || (pSiS->SiS_Pr->LVDSHL > 3)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	            "Illegal LVDSHL parameter, valid is 0 through 3\n");
	        pSiS->SiS_Pr->LVDSHL = -1;
	     } else {
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "LVDSHL will be %d\n",
	             pSiS->SiS_Pr->LVDSHL);
	     }
          }

	 /* EMI (315/330 series + 302LV/302ELV bridge only)
          * This might be required if the LCD panel loses sync on
	  * mode switches. So far, this problem should not show up
	  * due to the auto-detection (from reading the values set
	  * by the BIOS; however, the BIOS values are wrong sometimes
	  * such as in the case of some Compal machines with a
	  * 1400x1050, or some Inventec(Compaq) machines with a
	  * 1280x1024 panel.
          * The parameter is an integer from 0 to 0x60ffffff.
          */
          if(xf86GetOptValInteger(pSiS->Options, OPTION_EMI, &pSiS->EMI)) {
	     if((pSiS->EMI < 0) || (pSiS->EMI > 0x60ffffff)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	            "Illegal EMI parameter, valid is 0 through 0x60ffffff\n");
	        pSiS->EMI = -1;
	     } else {
	        pSiS->EMI &= 0x60ffffff;
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                    "EMI will be 0x%04x\n", pSiS->EMI);
	     }
          }

       }


      /* TVStandard (300/315/330 series and 6326 w/ TV only)
       * This option is for overriding the autodetection of
       * the BIOS/Jumper option for PAL / NTSC
       */
       if((pSiS->VGAEngine == SIS_300_VGA) ||
          (pSiS->VGAEngine == SIS_315_VGA) ||
          ((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV))) {
          strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_TVSTANDARD);
          if(strptr != NULL) {
             if(!xf86NameCmp(strptr,"PAL"))
	        pSiS->OptTVStand = 1;
	     else if((!xf86NameCmp(strptr,"PALM")) ||
	             (!xf86NameCmp(strptr,"PAL-M"))) {
	        pSiS->OptTVStand = 1;
	        pSiS->NonDefaultPAL = 1;
  	     } else if((!xf86NameCmp(strptr,"PALN")) ||
	               (!xf86NameCmp(strptr,"PAL-N"))) {
	        pSiS->OptTVStand = 1;
	        pSiS->NonDefaultPAL = 0;
	     } else if((!xf86NameCmp(strptr,"NTSCJ")) ||
	               (!xf86NameCmp(strptr,"NTSC-J"))) {
	        pSiS->OptTVStand = 0;
	        pSiS->NonDefaultNTSC = 1;
  	     } else if(!xf86NameCmp(strptr,"NTSC"))
	        pSiS->OptTVStand = 0;
	     else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "TVStandard");
                xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	            "Valid parameters are \"PAL\", \"PALM\", \"PALN\", \"NTSC\", \"NTSCJ\"\n");
	     }

	     if(pSiS->OptTVStand != -1) {
	        static const char *tvstdstr = "TV standard shall be %s\n";
	        if(pSiS->Chipset == PCI_CHIP_SIS6326) {
	           pSiS->NonDefaultPAL = -1;
		   pSiS->NonDefaultNTSC = -1;
	           xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, tvstdstr,
	               pSiS->OptTVStand ? "PAL" : "NTSC");
	        } else {
	           xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, tvstdstr,
		       (pSiS->OptTVStand ?
		           ( (pSiS->NonDefaultPAL == -1) ? "PAL" :
			      ((pSiS->NonDefaultPAL) ? "PALM" : "PALN") ) :
				(pSiS->NonDefaultNTSC == -1) ? "NTSC" : "NTSCJ"));
	        }
	     }
          }
       }

      /* CHTVType  (315/330 series + Chrontel only)
       * Used for telling the driver if the TV output shall
       * be 525i YPbPr or SCART.
       */
       if(pSiS->VGAEngine == SIS_315_VGA) {
          strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_CHTVTYPE);
          if(strptr != NULL) {
             if(!xf86NameCmp(strptr,"SCART"))
                pSiS->chtvtype = 1;
 	     else if(!xf86NameCmp(strptr,"YPBPR525I"))
	        pSiS->chtvtype = 0;
	     else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "CHTVType");
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	          "Valid parameters are \"SCART\" or \"YPBPR525I\"\n");
	     }
             if(pSiS->chtvtype != -1)
                xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                  "Chrontel: TV type shall be %s\n", strptr);
          }
       }

       /* CHTVOverscan (300/315/330 series only)
        * CHTVSuperOverscan (300/315/330 series only)
        * These options are for overriding the BIOS option for
        * TV Overscan. Some BIOS don't even have such an option.
        * SuperOverscan is only supported with PAL.
        * Both options are only effective on machines with a
        * CHRONTEL TV encoder. SuperOverscan is only available
        * on the 700x.
        */
       if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	  Bool val;
	  if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVOVERSCAN, &val)) {
	     pSiS->OptTVOver = val ? 1 : 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "Chrontel: TV overscan shall be %s\n",
	         val ? enabledstr : disabledstr);
	  }
          if(xf86GetOptValBool(pSiS->Options, OPTION_CHTVSOVERSCAN, &pSiS->OptTVSOver)) {
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	         "Chrontel: TV super overscan shall be %s\n",
	         pSiS->OptTVSOver ? enabledstr : disabledstr);
	  }
       }

       /* Various parameters for TV output via SiS bridge, Chrontel or SiS6326
        */
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
	  xf86GetOptValInteger(pSiS->Options, OPTION_SISTVSATURATION,
                                &pSiS->sistvsaturation);
	  xf86GetOptValInteger(pSiS->Options, OPTION_SISTVLUMAFILTER,
                                &pSiS->sistvyfilter);
          if((pSiS->sistvyfilter < 0) || (pSiS->sistvyfilter > 8)) {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"Illegal Y Filter number; valid is 0 (off), 1 (default), 2-8 (filter number 1-7)\n");
	     pSiS->sistvyfilter = 1;
	  }
 	  xf86GetOptValBool(pSiS->Options, OPTION_SISTVCHROMAFILTER,
				&pSiS->sistvcfilter);
	  xf86GetOptValInteger(pSiS->Options, OPTION_SISTVCOLCALIBCOARSE,
                                &pSiS->sistvcolcalibc);
          xf86GetOptValInteger(pSiS->Options, OPTION_SISTVCOLCALIBFINE,
                                &pSiS->sistvcolcalibf);
	  if((pSiS->sistvcolcalibf > 127) || (pSiS->sistvcolcalibf < -128) ||
	     (pSiS->sistvcolcalibc > 120) || (pSiS->sistvcolcalibc < -120)) {
	     pSiS->sistvcolcalibf = pSiS->sistvcolcalibc = 0;
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"Illegal Color Calibration. Range is -128 to 127 (fine), -120 to 120 (coarse)\n");
	  }
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
          tmp = 0;
	  xf86GetOptValInteger(pSiS->Options, OPTION_TVXSCALE,
                                &pSiS->tvxscale);
	  xf86GetOptValInteger(pSiS->Options, OPTION_TVYSCALE,
                                &pSiS->tvyscale);
	  if(pSiS->tvxscale > 16)  { pSiS->tvxscale = 16;  tmp = 1; }
	  if(pSiS->tvxscale < -16) { pSiS->tvxscale = -16; tmp = 1; }
	  if(pSiS->tvyscale > 3)  { pSiS->tvyscale = 3;  tmp = 1; }
	  if(pSiS->tvyscale < -4) { pSiS->tvyscale = -4; tmp = 1; }
	  if(tmp) xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		      "Illegal TV x or y scaling parameter. Range is from -16 to 16 (X), -4 to 3 (Y)\n");
       }

       if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
          int tmp = 0;
	  strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SIS6326FORCETVPPLUG);
          if(strptr) {
             if(!xf86NameCmp(strptr,"COMPOSITE"))
	        pSiS->sis6326tvplug = 1;
  	     else if(!xf86NameCmp(strptr,"SVIDEO"))
	        pSiS->sis6326tvplug = 0;
	     else {
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "SIS6326TVForcePlug");
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "Valid parameters are \"COMPOSITE\" or \"SVIDEO\"\n");
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
          xf86GetOptValInteger(pSiS->Options, OPTION_SIS6326FSCADJUST,
                                &pSiS->sis6326fscadjust);
	  if(pSiS->sis6326fscadjust) {
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	"Adjusting the default FSC by %d\n",
		pSiS->sis6326fscadjust);
	  }
       }

       if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA) ||
          ((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV))) {
          strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SISTVANTIFLICKER);
          if(!strptr) {
             strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_SIS6326ANTIFLICKER);
          }
          if(strptr) {
             if(!xf86NameCmp(strptr,"OFF"))
	        pSiS->sistvantiflicker = 0;
  	     else if(!xf86NameCmp(strptr,"LOW"))
	        pSiS->sistvantiflicker = 1;
	     else if(!xf86NameCmp(strptr,"MED"))
	        pSiS->sistvantiflicker = 2;
	     else if(!xf86NameCmp(strptr,"HIGH"))
	        pSiS->sistvantiflicker = 3;
	     else if(!xf86NameCmp(strptr,"ADAPTIVE"))
	        pSiS->sistvantiflicker = 4;
	     else {
	        pSiS->sistvantiflicker = -1;
	        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "SISTVAntiFlicker");
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	            "Valid parameters are \"OFF\", \"LOW\", \"MED\", \"HIGH\" or \"ADAPTIVE\"\n");
	     }
	  }
       }

       /* CRT2Gamma - enable/disable gamma correction for CRT2
        */
       if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
          Bool val;
          if(xf86GetOptValBool(pSiS->Options, OPTION_CRT2GAMMA, &val)) {
	     pSiS->CRT2gamma = val;
          }
       }

#ifdef SIS_CP
       SIS_CP_OPT_DOOPT
#endif

    }  /* DualHead */

    /* CRT1Gamma - enable/disable gamma correction for CRT1
     */
    {
       Bool val;
       if(xf86GetOptValBool(pSiS->Options, OPTION_CRT1GAMMA, &val)) {
	  pSiS->CRT1gamma = val;
	  pSiS->CRT1gammaGiven = TRUE;
       }
    }

    /* VESA - DEPRECATED
     * This option is for forcing the driver to use
     * the VESA BIOS extension for mode switching.
     */
    {
	Bool val;
	if(xf86GetOptValBool(pSiS->Options, OPTION_VESA, &val)) {
	    pSiS->VESA = val ? 1 : 0;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	        "VESA: VESA usage shall be %s\n",
		val ? enabledstr : disabledstr);
 	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    	"*** Option \"VESA\" is deprecated. *** \n");
	    if(pSiS->VESA) pSiS->ForceCRT1Type = CRT1_VGA;
	}
    }

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {

       /* NoInternalModes (300/315/330 series only)
        * Since the mode switching code for these chipsets is a
        * Asm-to-C translation of BIOS code, we only have timings
        * for a pre-defined number of modes. The default behavior
        * is to replace XFree's default modes with a mode list
        * generated out of the known and supported modes. Use
        * this option to disable this. NOT RECOMMENDED.
        */
       from = X_DEFAULT;
       if(xf86GetOptValBool(pSiS->Options, OPTION_NOINTERNALMODES, &pSiS->noInternalModes))
		from = X_CONFIG;
       xf86DrvMsg(pScrn->scrnIndex, from, "Usage of built-in modes is %s\n",
		       pSiS->noInternalModes ? disabledstr : enabledstr);
    }

    /* ShadowFB */
    from = X_DEFAULT;
    if(xf86GetOptValBool(pSiS->Options, OPTION_SHADOW_FB, &pSiS->ShadowFB)) {
#ifdef SISMERGED
       if(pSiS->MergedFB) {
          pSiS->ShadowFB = FALSE;
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      "Shadow Frame Buffer not supported in MergedFB mode\n");
       } else
#endif
          from = X_CONFIG;
    }
    if(pSiS->ShadowFB) {
	pSiS->NoAccel = TRUE;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	pSiS->NoXvideo = TRUE;
    	xf86DrvMsg(pScrn->scrnIndex, from,
	   "Using \"Shadow Frame Buffer\" - 2D acceleration and Xv disabled\n");
#else
    	xf86DrvMsg(pScrn->scrnIndex, from,
	   "Using \"Shadow Frame Buffer\" - 2D acceleration disabled\n");
#endif
    }

    /* Rotate */
    if((strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_ROTATE))) {
#ifdef SISMERGED
       if(pSiS->MergedFB) {
	  xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	      "Screen rotation not supported in MergedFB mode\n");
       } else
#endif
       if(!xf86NameCmp(strptr, "CW")) {
          pSiS->Rotate = 1;
       } else if(!xf86NameCmp(strptr, "CCW")) {
          pSiS->Rotate = -1;
       } else {
          xf86DrvMsg(pScrn->scrnIndex, X_WARNING, mybadparm, strptr, "Rotate");
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
              "Valid parameters are \"CW\" or \"CCW\"\n");
       }

       if(pSiS->Rotate) {
          pSiS->ShadowFB = TRUE;
          pSiS->NoAccel  = TRUE;
          pSiS->HWCursor = FALSE;
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
	  pSiS->NoXvideo = TRUE;
          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
              "Rotating screen %sclockwise; (2D acceleration and Xv disabled)\n",
	      (pSiS->Rotate == -1) ? "counter " : "");
#else
	  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
              "Rotating screen %sclockwise (2D acceleration %sdisabled)\n",
	      (pSiS->Rotate == -1) ? "counter " : "",
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,0,0,0)
              "and RandR extension "
#else
	      ""
#endif
	      );

#endif

       }
    }

#ifdef XF86DRI
    /* DRI */
    from = X_DEFAULT;
    if(xf86GetOptValBool(pSiS->Options, OPTION_DRI, &pSiS->loadDRI)) {
       from = X_CONFIG;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "DRI %s\n",
       pSiS->loadDRI ? enabledstr : disabledstr);

    /* AGPSize */
    {
       int vali;
       Bool gotit = FALSE;
       if(xf86GetOptValInteger(pSiS->Options, OPTION_AGP_SIZE, &vali)) {
          gotit = TRUE;
       } else if(xf86GetOptValInteger(pSiS->Options, OPTION_AGP_SIZE2, &vali)) {
          gotit = TRUE;
       }
       if(gotit) {
	  if((vali >= 8) && (vali <= 512)) {
	     pSiS->agpWantedPages = (vali * 1024 * 1024) / AGP_PAGE_SIZE;
	  } else {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr, "AGPSize (alias GARTSize)", 8, 512);
	  }
       }
    }
#endif

    /* NoXVideo
     * Set this to TRUE to disable Xv hardware video acceleration
     */
#if XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,2,99,0,0)
    if((!pSiS->NoAccel) && (!pSiS->NoXvideo)) {
#else
    if(!pSiS->NoXvideo) {
#endif
       if(xf86ReturnOptValBool(pSiS->Options, OPTION_NOXVIDEO, FALSE)) {
          pSiS->NoXvideo = TRUE;
          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "XVideo extension disabled\n");
       }

       if(!pSiS->NoXvideo) {
          Bool val;
	  int tmp;

	  if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
      	     /* XvOnCRT2
	      * On chipsets with only one overlay (315, 650, 740, 330), the user can
	      * choose to display the overlay on CRT1 or CRT2. By setting this
	      * option to TRUE, the overlay will be displayed on CRT2. The
	      * default is: CRT1 if only CRT1 available, CRT2 if only CRT2
	      * available, and CRT1 if both is available and detected.
	      * Since implementation of the XV_SWITCHCRT Xv property this only
	      * selects the default CRT.
	      */
             if(xf86GetOptValBool(pSiS->Options, OPTION_XVONCRT2, &val)) {
	        pSiS->XvOnCRT2 = val ? TRUE : FALSE;
	     }
	  }

	  if((pSiS->VGAEngine == SIS_OLD_VGA) || (pSiS->VGAEngine == SIS_530_VGA)) {
	     /* NoYV12 (for 5597/5598, 6326 and 530/620 only)
	      * YV12 has problems with videos larger than 384x288. So
	      * allow the user to disable YV12 support to force the
	      * application to use YUV2 instead.
	      */
             if(xf86GetOptValBool(pSiS->Options, OPTION_NOYV12, &val)) {
	        pSiS->NoYV12 = val ? 1 : 0;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			"Xv YV12/I420 support is %s\n",
			pSiS->NoYV12 ? disabledstr : enabledstr);
	     }
	  }

	  /* Some Xv properties' defaults can be set by options */
          if(xf86GetOptValInteger(pSiS->Options, OPTION_XVDEFCONTRAST, &tmp)) {
             if((tmp >= 0) && (tmp <= 7)) pSiS->XvDefCon = tmp;
             else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       		      "XvDefaultContrast" ,0, 7);
          }
          if(xf86GetOptValInteger(pSiS->Options, OPTION_XVDEFBRIGHTNESS, &tmp)) {
             if((tmp >= -128) && (tmp <= 127)) pSiS->XvDefBri = tmp;
             else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       		      "XvDefaultBrightness", -128, 127);
          }
          if(pSiS->VGAEngine == SIS_315_VGA) {
             if(xf86GetOptValInteger(pSiS->Options, OPTION_XVDEFHUE, &tmp)) {
                if((tmp >= -8) && (tmp <= 7)) pSiS->XvDefHue = tmp;
                else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       	              "XvDefaultHue", -8, 7);
             }
             if(xf86GetOptValInteger(pSiS->Options, OPTION_XVDEFSATURATION, &tmp)) {
                if((tmp >= -7) && (tmp <= 7)) pSiS->XvDefSat = tmp;
                else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       	              "XvDefaultSaturation", -7, 7);
             }
          }
	  if(xf86GetOptValBool(pSiS->Options, OPTION_XVDEFDISABLEGFX, &val)) {
	     if(val)  pSiS->XvDefDisableGfx = TRUE;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	        "Graphics display will be %s during Xv usage\n",
	     	val ? disabledstr : enabledstr);
          }
	  if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
	     if(xf86GetOptValBool(pSiS->Options, OPTION_XVDEFDISABLEGFXLR, &val)) {
	        if(val)  pSiS->XvDefDisableGfxLR = TRUE;
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	   "Graphics display left/right of overlay will be %s during Xv usage\n",
		   val ? disabledstr : enabledstr);
             }
	     if(xf86GetOptValBool(pSiS->Options, OPTION_XVDISABLECOLORKEY, &val)) {
	        if(val) pSiS->XvDisableColorKey = TRUE;
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	   "Xv Color key is %s\n",
		   val ? disabledstr : enabledstr);
             }
	     if(xf86GetOptValBool(pSiS->Options, OPTION_XVUSECHROMAKEY, &val)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	   "Xv Chroma-keying is %s\n",
		   val ? enabledstr : disabledstr);
		if(val) pSiS->XvUseChromaKey = TRUE;
             }
	     if(xf86GetOptValBool(pSiS->Options, OPTION_XVINSIDECHROMAKEY, &val)) {
	        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	   "Xv: Video is transparent if %s chroma key range\n",
		   val ? "inside" : "outside");
		if(val) pSiS->XvInsideChromaKey = TRUE;
             }
	     if(pSiS->VGAEngine == SIS_300_VGA) {
	        if(xf86GetOptValBool(pSiS->Options, OPTION_XVYUVCHROMAKEY, &val)) {
	           xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
	     	      "Xv: Chroma key is in %s format\n",
		      val ? "YUV" : "RGB");
		   if(val) pSiS->XvYUVChromaKey = TRUE;
		}
             } else {
	        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Xv: Chroma key is of same format as video source\n");
	     }
	     if(xf86GetOptValInteger(pSiS->Options, OPTION_XVCHROMAMIN, &tmp)) {
                if((tmp >= 0) && (tmp <= 0xffffff)) pSiS->XvChromaMin = tmp;
                else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       	                       "XvChromaMin", 0, 0xffffff);
             }
	     if(xf86GetOptValInteger(pSiS->Options, OPTION_XVCHROMAMAX, &tmp)) {
                if((tmp >= 0) && (tmp <= 0xffffff)) pSiS->XvChromaMax = tmp;
                else xf86DrvMsg(pScrn->scrnIndex, X_WARNING, ilrangestr,
       	                   "XvChromaMax", 0, 0xffffff);
             }
          }
	  if(xf86GetOptValBool(pSiS->Options, OPTION_XVMEMCPY, &val)) {
	     pSiS->XvUseMemcpy = val ? TRUE : FALSE;
	     xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Xv will %suse memcpy()\n",
	     	val ? "" : "not ");
          }
	  /* XvGamma - enable/disable gamma correction for Xv
	   * Supported for CRT1 only
           */
          if(pSiS->VGAEngine == SIS_315_VGA) {
	     if((strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_XVGAMMA))) {
                if( (!xf86NameCmp(strptr,"off"))   ||
	            (!xf86NameCmp(strptr,"false")) ||
		    (!xf86NameCmp(strptr,"no"))    ||
		    (!xf86NameCmp(strptr,"0")) ) {
		   pSiS->XvGamma = FALSE;
		   pSiS->XvGammaGiven = TRUE;
	        } else if( (!xf86NameCmp(strptr,"on"))   ||
	                   (!xf86NameCmp(strptr,"true")) ||
		           (!xf86NameCmp(strptr,"yes"))  ||
		           (!xf86NameCmp(strptr,"1")) ) {
		   pSiS->XvGamma = pSiS->XvGammaGiven = TRUE;
                } else {
	           float val1 = 0.0, val2 = 0.0, val3 = 0.0;
		   Bool valid = FALSE;
	           int result = sscanf(strptr, "%f %f %f", &val1, &val2, &val3);
		   if(result == 1) {
		      if((val1 >= 0.1) && (val1 <= 10.0)) {
		         pSiS->XvGammaGreen = pSiS->XvGammaBlue = pSiS->XvGammaRed =
			    pSiS->XvGammaGreenDef = pSiS->XvGammaBlueDef = pSiS->XvGammaRedDef = (int)(val1 * 1000);
		         valid = TRUE;
		      }
		   } else if(result == 3) {
		      if((val1 >= 0.1) && (val1 <= 10.0) &&
		         (val2 >= 0.1) && (val2 <= 10.0) &&
		         (val3 >= 0.1) && (val3 <= 10.0)) {
		         pSiS->XvGammaRed = pSiS->XvGammaRedDef = (int)(val1 * 1000);
		         pSiS->XvGammaGreen = pSiS->XvGammaGreenDef = (int)(val2 * 1000);
		         pSiS->XvGammaBlue = pSiS->XvGammaBlueDef = (int)(val3 * 1000);
			 valid = TRUE;
		      }
		   }
		   if(valid) {
		      pSiS->XvGamma = pSiS->XvGammaGiven = TRUE;
		   } else {
		      xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		         "XvGamma expects either a boolean, or 1 or 3 real numbers (0.1 - 10.0)\n");
		   }
	        }
             }
          }
       }
    }

    if((pSiS->VGAEngine == SIS_300_VGA) || (pSiS->VGAEngine == SIS_315_VGA)) {
       if((strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_STOREDBRI))) {
          float val1 = 0.0, val2 = 0.0, val3 = 0.0;
	  Bool valid = FALSE;
	  int result = sscanf(strptr, "%f %f %f", &val1, &val2, &val3);
	  if(result == 1) {
	     if((val1 >= 0.1) && (val1 <= 10.0)) {
	        valid = TRUE;
		pSiS->GammaBriR = pSiS->GammaBriG = pSiS->GammaBriB = (int)(val1 * 1000);
	     }
	  } else if(result == 3) {
	     if((val1 >= 0.1) && (val1 <= 10.0) &&
	        (val2 >= 0.1) && (val2 <= 10.0) &&
		(val3 >= 0.1) && (val3 <= 10.0)) {
		valid = TRUE;
		pSiS->GammaBriR = (int)(val1 * 1000);
		pSiS->GammaBriG = (int)(val2 * 1000);
		pSiS->GammaBriB = (int)(val3 * 1000);
	     }
	  }
	  if(!valid) {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"StoredGammaBrightness expects 1 or 3 real numbers (0.1 - 10.0)\n");
	  }
       }
       if((strptr = (char *)xf86GetOptValString(pSiS->Options, OPTION_STOREDPBRI))) {
          float val1 = 0.0, val2 = 0.0, val3 = 0.0;
	  Bool valid = FALSE;
	  int result = sscanf(strptr, "%f %f %f", &val1, &val2, &val3);
	  if(result == 1) {
	     if((val1 >= 0.1) && (val1 <= 10.0)) {
	        valid = TRUE;
		pSiS->GammaPBriR = pSiS->GammaPBriG = pSiS->GammaPBriB = (int)(val1 * 1000);
	     }
	  } else if(result == 3) {
	     if((val1 >= 0.1) && (val1 <= 10.0) &&
	        (val2 >= 0.1) && (val2 <= 10.0) &&
		(val3 >= 0.1) && (val3 <= 10.0)) {
		valid = TRUE;
		pSiS->GammaPBriR = (int)(val1 * 1000);
		pSiS->GammaPBriG = (int)(val2 * 1000);
		pSiS->GammaPBriB = (int)(val3 * 1000);
	     }
	  }
	  if(!valid) {
	     xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	     	"\"StoredGammaPreBrightness\" expects 1 or 3 real numbers (0.1 - 10.0)\n");
	  }
       }
    }

}

const OptionInfoRec *
SISAvailableOptions(int chipid, int busid)
{
    return SISOptions;
}
