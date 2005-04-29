/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis.h,v 1.111 2004/02/26 09:16:20 twini Exp $ */
/*
 * Main global data and definitions
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
 * Authors:   Thomas Winischhofer <thomas@winischhofer.net>
 *            others (old code base)
 *
 */

#ifndef _SIS_H
#define _SIS_H_

/* Always unlock the registers (should be set!) */
#define UNLOCK_ALWAYS

#define SISDRIVERVERSIONYEAR    4
#define SISDRIVERVERSIONMONTH   2
#define SISDRIVERVERSIONDAY     26
#define SISDRIVERREVISION       1

#define SISDRIVERIVERSION (SISDRIVERVERSIONYEAR << 16) |  \
			  (SISDRIVERVERSIONMONTH << 8) |  \
                          SISDRIVERVERSIONDAY 	       |  \
			  (SISDRIVERREVISION << 24)

#if 0
#define TWDEBUG    /* for debugging */
#endif

#if 0
#include "siscp.H"
#else
#undef SIS_CP
#endif

#include "xf86Pci.h"
#include "xf86Cursor.h"
#include "xf86_ansic.h"
#include "xf86xv.h"
#include "compiler.h"
#include "xaa.h"
#include "vgaHW.h"
#include "vbe.h"

#include "osdef.h"
#include "vgatypes.h"
#include "vstruct.h"

#ifdef XF86DRI
#undef SISNEWDRI
#undef SISNEWDRI2
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,3,99,14,0)
#define SISNEWDRI
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,4,99,99,0)	/* Adapt this when the time has come */
#define SISNEWDRI2
#endif
#endif
#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "xf86dri.h"
#include "dri.h"
#include "GL/glxint.h"
#include "sis_dri.h"
#endif

#if 1
#define SISDUALHEAD  		/* Include Dual Head code  */
#endif

#if 1
#define SISMERGED		/* Include Merged-FB mode */
#endif

#ifdef SISMERGED
#if 1
#define SISXINERAMA		/* Include SiS Pseudo-Xinerama for MergedFB mode */
#define SIS_XINERAMA_MAJOR_VERSION  1
#define SIS_XINERAMA_MINOR_VERSION  1
#endif
#endif

#if 1
#define SISGAMMA		/* Include code for gamma correction */
#endif

#if 1				/* Include code for color hardware cursors */
#define SIS_ARGB_CURSOR
#endif

#if 1				/* Include YPbPr support on SiS bridges (315 series and 661/741/760) */
#define ENABLE_YPBPR
#endif

#ifdef SISMERGED
#ifdef SISXINERAMA
#define NEED_REPLIES  		/* ? */
#define EXTENSION_PROC_ARGS void *
#include "extnsionst.h"  	/* required */
#include "panoramiXproto.h"  	/* required */
#endif
#endif

#if 1
#define SISVRAMQ		/* Use VRAM queue mode on 315 series */
#endif

#undef SIS315DRI		/* define this if dri is adapted for 315/330 series */

/* For SiS315/550/650/740/330/660 - these should be moved elsewhere! */
#ifndef PCI_CHIP_SIS315H
#define PCI_CHIP_SIS315H		0x0310
#endif
#ifndef PCI_CHIP_SIS315
#define PCI_CHIP_SIS315			0x0315
#endif
#ifndef PCI_CHIP_SIS315PRO
#define PCI_CHIP_SIS315PRO		0x0325
#endif
#ifndef PCI_CHIP_SIS550
#define PCI_CHIP_SIS550			0x5315	/* 550_VGA */
#endif
#ifndef PCI_CHIP_SIS650
#define PCI_CHIP_SIS650 		0x6325  /* 650_VGA and 740_VGA */
#endif
#ifndef PCI_CHIP_SIS330
#define PCI_CHIP_SIS330 		0x0330
#endif
#ifndef PCI_CHIP_SIS660
#define PCI_CHIP_SIS660 		0x6330	/* 661_VGA, 741_VGA, 760_VGA */
#endif

#define SIS_NAME                "SIS"
#define SIS_DRIVER_NAME         "sis"
#define SIS_MAJOR_VERSION       0
#define SIS_MINOR_VERSION       7
#define SIS_PATCHLEVEL          0
#define SIS_CURRENT_VERSION     ((SIS_MAJOR_VERSION << 16) | \
                                 (SIS_MINOR_VERSION << 8) | SIS_PATCHLEVEL )

/* pSiS->Flags (old series only) */
#define SYNCDRAM                0x00000001
#define RAMFLAG                 0x00000002
#define ESS137xPRESENT          0x00000004
#define SECRETFLAG              0x00000008
#define A6326REVAB              0x00000010
#define MMIOMODE                0x00010000
#define LFBQMODE                0x00020000
#define AGPQMODE                0x00040000
#define UMA                     0x80000000

#define BIOS_BASE               0xC0000
#define BIOS_SIZE               0x10000

#define SR_BUFFER_SIZE          5
#define CR_BUFFER_SIZE          5

#define SIS_VBFlagsVersion	1

/* VBFlags - if anything is changed here, increase VBFlagsVersion! */
#define CRT2_DEFAULT            0x00000001
#define CRT2_LCD                0x00000002  /* Never change the order of the CRT2_XXX entries */
#define CRT2_TV                 0x00000004  
#define CRT2_VGA                0x00000008
#define TV_NTSC                 0x00000010
#define TV_PAL                  0x00000020
#define TV_HIVISION             0x00000040
#define TV_YPBPR                0x00000080
#define TV_AVIDEO               0x00000100
#define TV_SVIDEO               0x00000200
#define TV_SCART                0x00000400
#define VB_CONEXANT		0x00000800   /* 661 series only */
#define VB_TRUMPION		VB_CONEXANT  /* 300 series only */
#define TV_PALM                 0x00001000
#define TV_PALN                 0x00002000
#define TV_NTSCJ		0x00001000
#define VB_302ELV		0x00004000
#define TV_CHSCART              0x00008000
#define TV_CHYPBPR525I          0x00010000
#define CRT1_VGA		0x00000000
#define CRT1_LCDA		0x00020000
#define VGA2_CONNECTED          0x00040000
#define DISPTYPE_CRT1		0x00080000  	/* CRT1 connected and used */
#define VB_301                  0x00100000	/* Video bridge type */
#define VB_301B                 0x00200000
#define VB_302B                 0x00400000
#define VB_30xBDH		0x00800000      /* 30xB DH version (w/o LCD support) */
#define VB_LVDS                 0x01000000
#define VB_CHRONTEL             0x02000000
#define VB_301LV                0x04000000
#define VB_302LV                0x08000000
#define VB_301C			0x10000000
#define SINGLE_MODE             0x20000000   	/* CRT1 or CRT2; determined by DISPTYPE_CRTx */
#define MIRROR_MODE		0x40000000   	/* CRT1 + CRT2 identical (mirror mode) */
#define DUALVIEW_MODE		0x80000000   	/* CRT1 + CRT2 independent (dual head mode) */

/* Aliases: */
#define CRT2_ENABLE		(CRT2_LCD | CRT2_TV | CRT2_VGA)
#define TV_STANDARD             (TV_NTSC | TV_PAL | TV_PALM | TV_PALN | TV_NTSCJ)
#define TV_INTERFACE            (TV_AVIDEO|TV_SVIDEO|TV_SCART|TV_HIVISION|TV_YPBPR)

/* Only if TV_YPBPR is set: */
#define TV_YPBPR525I		TV_NTSC
#define TV_YPBPR525P		TV_PAL
#define TV_YPBPR750P		TV_PALM
#define TV_YPBPR1080I	        TV_PALN
#define TV_YPBPRALL 		(TV_YPBPR525I | TV_YPBPR525P | TV_YPBPR750P | TV_YPBPR1080I)

#define TV_YPBPR43LB		TV_CHSCART
#define TV_YPBPR43		TV_CHYPBPR525I
#define TV_YPBPR169 		(TV_CHSCART | TV_CHYPBPR525I)
#define TV_YPBPRAR              (TV_CHSCART | TV_CHYPBPR525I)

#define VB_SISBRIDGE            (VB_301|VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV|VB_302ELV)
#define VB_SISTVBRIDGE          (VB_301|VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV)
#define VB_VIDEOBRIDGE		(VB_SISBRIDGE | VB_LVDS | VB_CHRONTEL | VB_CONEXANT)

#define DISPTYPE_DISP2		CRT2_ENABLE
#define DISPTYPE_DISP1		DISPTYPE_CRT1
#define VB_DISPMODE_SINGLE	SINGLE_MODE  	/* alias */
#define VB_DISPMODE_MIRROR	MIRROR_MODE  	/* alias */
#define VB_DISPMODE_DUAL	DUALVIEW_MODE 	/* alias */
#define DISPLAY_MODE            (SINGLE_MODE | MIRROR_MODE | DUALVIEW_MODE)

/* pSiS->VBLCDFlags */
#define VB_LCD_320x480		0x00000001	/* DSTN/FSTN for 550 */
#define VB_LCD_640x480          0x00000002
#define VB_LCD_800x600          0x00000004
#define VB_LCD_1024x768         0x00000008
#define VB_LCD_1280x1024        0x00000010
#define VB_LCD_1280x960    	0x00000020
#define VB_LCD_1600x1200	0x00000040
#define VB_LCD_2048x1536	0x00000080
#define VB_LCD_1400x1050        0x00000100
#define VB_LCD_1152x864         0x00000200
#define VB_LCD_1152x768         0x00000400
#define VB_LCD_1280x768         0x00000800
#define VB_LCD_1024x600         0x00001000
#define VB_LCD_640x480_2	0x00002000  	/* DSTN/FSTN */
#define VB_LCD_640x480_3	0x00004000  	/* DSTN/FSTN */
#define VB_LCD_848x480		0x00008000	/* LVDS only, otherwise handled as custom */
#define VB_LCD_1280x800		0x00010000
#define VB_LCD_1680x1050	0x00020000
#define VB_LCD_1280x720         0x00040000
#define VB_LCD_BARCO1366        0x20000000
#define VB_LCD_CUSTOM  		0x40000000
#define VB_LCD_EXPANDING	0x80000000

/* PresetMode argument */
#define SIS_MODE_SIMU 		0
#define SIS_MODE_CRT1 		1
#define SIS_MODE_CRT2 		2

/* pSiS->MiscFlags */
#define MISC_CRT1OVERLAY	0x00000001  /* Current display mode supports overlay */
#define MISC_PANELLINKSCALER    0x00000002  /* Panel link is currently scaling */
#define MISC_CRT1OVERLAYGAMMA	0x00000004  /* Current display mode supports overlay gamma corr on CRT1 */
#define MISC_TVNTSC1024		0x00000008  /* Current display mode is TV NTSC/PALM/YPBPR525I 1024x768  */

/* SiS6326Flags */
#define SIS6326_HASTV		0x00000001
#define SIS6326_TVSVIDEO        0x00000002
#define SIS6326_TVCVBS          0x00000004
#define SIS6326_TVPAL		0x00000008
#define SIS6326_TVDETECTED      0x00000010
#define SIS6326_TVON            0x80000000

#define HW_DEVICE_EXTENSION	SIS_HW_INFO

#ifdef  DEBUG
#define PDEBUG(p)       p
#else
#define PDEBUG(p)
#endif

#define BITMASK(h,l)             (((unsigned)(1U << ((h)-(l)+1))-1)<<(l))
#define GENMASK(mask)            BITMASK(1?mask,0?mask)

typedef unsigned long ULong;
typedef unsigned short UShort;
typedef unsigned char UChar;

/* VGA engine types */
#define UNKNOWN_VGA 0
#define SIS_530_VGA 1
#define SIS_OLD_VGA 2
#define SIS_300_VGA 3
#define SIS_315_VGA 4   /* Includes 330/660/661/741/760 and M versions thereof */

/* oldChipset */
#define OC_UNKNOWN   0
#define OC_SIS86201  1
#define OC_SIS86202  2
#define OC_SIS6205A  3
#define OC_SIS6205B  4
#define OC_SIS82204  5
#define OC_SIS6205C  6
#define OC_SIS6225   7
#define OC_SIS5597   8
#define OC_SIS6326   9
#define OC_SIS530A  11
#define OC_SIS530B  12
#define OC_SIS620   13

/* Chrontel type */
#define CHRONTEL_700x 0
#define CHRONTEL_701x 1

/* ChipFlags */
/* Use only lower 16 bit for chip id! (sisctrl) */
#define SiSCF_LARGEOVERLAY  0x00000001
#define SiSCF_Is651         0x00000002
#define SiSCF_IsM650        0x00000004
#define SiSCF_IsM652        0x00000008
#define SiSCF_IsM653        0x00000010
#define SiSCF_Is652         0x00000020
#define SiSCF_Is65x         (SiSCF_Is651|SiSCF_IsM650|SiSCF_IsM652|SiSCF_IsM653|SiSCF_Is652)
#define SiSCF_IsM661        0x00000100  /* M661FX */
#define SiSCF_IsM741        0x00000200
#define SiSCF_IsM760        0x00000400
#define SiSCF_IsM661M       0x00000800  /* M661MX */
#define SiSCF_IsM66x        (SiSCF_IsM661 | SiSCF_IsM741 | SiSCF_IsM760 | SiSCF_IsM661M)
#define SiSCF_315Core       0x00010000  /* 3D: Real 315 */
#define SiSCF_Real256ECore  0x00020000  /* 3D: Similar to 315 core, no T&L? (65x, 661, 740, 741) */
#define SiSCF_XabreCore     0x00040000  /* 3D: Real Xabre */
#define SiSCF_Ultra256Core  0x00080000  /* 3D: Similar to Xabre, no T&L?, no P:Shader? (660, 760) */
#define SiSCF_UseLCDA       0x01000000  
#define SiSCF_760UMA        0x10000000  /* 760: UMA active */
#define SiSCF_CRT2HWCKaputt 0x20000000  /* CRT2 Mono HWCursor engine buggy (SiS 330) */
#define SiSCF_Glamour3      0x40000000
#define SiSCF_Integrated    0x80000000

/* SiS Direct Xv-API */
#define SiS_SD_IS300SERIES     0x00000001
#define SiS_SD_IS315SERIES     0x00000002
#define SiS_SD_IS330SERIES     0x00000004
#define SiS_SD_SUPPORTPALMN    0x00000008   /* tv chip supports pal-m, pal-n */
#define SiS_SD_SUPPORT2OVL     0x00000010   /* set = 2 overlays, clear = support SWITCHCRT xv prop */
#define SiS_SD_SUPPORTTVPOS    0x00000020   /* supports changing tv position */
#define SiS_SD_ISDUALHEAD      0x00000040   /* Driver is in dual head mode */
#define SiS_SD_ISMERGEDFB      0x00000080   /* Driver is in merged fb mode */
#define SiS_SD_ISDHSECONDHEAD  0x00000100   /* Dual head: This is CRT1 (=second head) */
#define SiS_SD_ISDHXINERAMA    0x00000200   /* Dual head: We are running Xinerama */
#define SiS_SD_VBHASSCART      0x00000400   /* videobridge has SCART instead of VGA2 */
#define SiS_SD_ISDEPTH8        0x00000800   /* Depth is 8, no independent gamma correction */
#define SiS_SD_SUPPORTSOVER    0x00001000   /* Support for Chrontel Super Overscan */
#define SiS_SD_ENABLED         0x00002000   /* sisctrl is enabled (by option) */
#define SiS_SD_PSEUDOXINERAMA  0x00004000   /* pseudo xinerama is active */
#define SiS_SD_SUPPORTLCDA     0x00008000   /* Support LCD Channel A */
#define SiS_SD_SUPPORTNTSCJ    0x00010000   /* tv chip supports ntsc-j */
#define SiS_SD_ADDLSUPFLAG     0x00020000   /* 1 = the following flags are valid */
#define SiS_SD_SUPPORTVGA2     0x00040000   /* CRT2=VGA supported */
#define SiS_SD_SUPPORTSCART    0x00080000   /* CRT2=SCART supported */
#define SiS_SD_SUPPORTOVERSCAN 0x00100000   /* Overscan flag supported */
#define SiS_SD_SUPPORTXVGAMMA1 0x00200000   /* Xv Gamma correction for CRT1 supported */
#define SiS_SD_SUPPORTTV       0x00400000   /* CRT2=TV supported */
#define SiS_SD_SUPPORTYPBPR    0x00800000   /* CRT2=YPbPr (525i, 525p, 750p, 1080i) is supported */
#define SiS_SD_SUPPORTHIVISION 0x01000000   /* CRT2=HiVision is supported */
#define SiS_SD_SUPPORTYPBPRAR  0x02000000   /* YPbPr aspect ratio is supported */
#define SiS_SD_SUPPORTSCALE    0x04000000   /* Scaling of LCD panel supported */
#define SiS_SD_SUPPORTCENTER   0x08000000   /* If scaling supported: Centering of screen [NOT] supported (TMDS only) */

#define SIS_DIRECTKEY         0x03145792

/* SiSCtrl: Check mode for CRT2 */
#define SiS_CF2_LCD          0x01
#define SiS_CF2_TV           0x02
#define SiS_CF2_VGA2         0x04
#define SiS_CF2_TVPAL        0x08
#define SiS_CF2_TVNTSC       0x10  /* + NTSC-J */
#define SiS_CF2_TVPALM       0x20
#define SiS_CF2_TVPALN       0x40
#define SiS_CF2_CRT1LCDA     0x80
#define SiS_CF2_TYPEMASK     (SiS_CF2_LCD | SiS_CF2_TV | SiS_CF2_VGA2 | SiS_CF2_CRT1LCDA)
#define SiS_CF2_TVSPECIAL    (SiS_CF2_LCD | SiS_CF2_TV)
#define SiS_CF2_TVSPECMASK   (SiS_CF2_TVPAL | SiS_CF2_TVNTSC | SiS_CF2_TVPALM | SiS_CF2_TVPALN)
#define SiS_CF2_TVHIVISION   SiS_CF2_TVPAL
#define SiS_CF2_TVYPBPR525I  SiS_CF2_TVNTSC
#define SiS_CF2_TVYPBPR525P  (SiS_CF2_TVPAL | SiS_CF2_TVNTSC)
#define SiS_CF2_TVYPBPR750P  SiS_CF2_TVPALM
#define SiS_CF2_TVYPBPR1080I (SiS_CF2_TVPALM | SiS_CF2_TVPAL)

/* AGP stuff for DRI */
#define AGP_PAGE_SIZE 4096
#define AGP_PAGES     2048	 /* Default: 2048 pages @ 4096 = 8MB */
/* 300 */
#define AGP_CMDBUF_PAGES 256
#define AGP_CMDBUF_SIZE (AGP_PAGE_SIZE * AGP_CMDBUF_PAGES)
/* 315/330 */
#define AGP_VTXBUF_PAGES 512
#define AGP_VTXBUF_SIZE (AGP_PAGE_SIZE * AGP_VTXBUF_PAGES)

/* For backup of register contents */
typedef struct {
    unsigned char sisRegs3C4[0x50];
    unsigned char sisRegs3D4[0x90];
    unsigned char sisRegs3C2;
    unsigned char sisCapt[0x60];
    unsigned char sisVid[0x50];
    unsigned char VBPart1[0x50];
    unsigned char VBPart2[0x100];
    unsigned char VBPart3[0x50];
    unsigned char VBPart4[0x50];
    unsigned short ch70xx[64];
    unsigned long sisMMIO85C0;
    unsigned char sis6326tv[0x46];
    unsigned long sisRegsPCI50, sisRegsPCIA0;
} SISRegRec, *SISRegPtr;

typedef struct _sisModeInfoPtr {
    int width;
    int height;
    int bpp;
    int n;
    struct _sisModeInfoPtr *next;
} sisModeInfoRec, *sisModeInfoPtr;

/* SISFBLayout is mainly there because of DGA. It holds the
 * current layout parameters needed for acceleration and other
 * stuff. When switching mode using DGA, these are set up
 * accordingly and not necessarily match pScrn's. Therefore,
 * driver modules should read these values instead of pScrn's.
 */
typedef struct {
    int                bitsPerPixel;   	/* = pScrn->bitsPerPixel */
    int                depth;		/* = pScrn->depth */
    int                displayWidth;	/* = pScrn->displayWidth */
    DisplayModePtr     mode;		/* = pScrn->currentMode */
} SISFBLayout;

/* Dual head private entity structure */
#ifdef SISDUALHEAD
typedef struct {
    ScrnInfoPtr         pScrn_1;
    ScrnInfoPtr         pScrn_2;
    unsigned char *     BIOS;
    SiS_Private   *     SiS_Pr;
    unsigned long 	agpHandle;
    unsigned long	agpAddr;
    unsigned char 	*agpBase;
    unsigned int 	agpSize;
    unsigned int        agpWantedSize;
    unsigned int        agpWantedPages;
    unsigned long	agpCmdBufAddr;  /* 300 series */
    unsigned char 	*agpCmdBufBase;
    unsigned int 	agpCmdBufSize;
    unsigned int 	agpCmdBufFree;
    unsigned long	agpVtxBufAddr;	/* 315 series */
    unsigned char       *agpVtxBufBase;
    unsigned int        agpVtxBufSize;
    unsigned int        agpVtxBufFree;
#ifdef XF86DRI
    sisRegion 		agp;
    int 		drmSubFD;
#endif
    Bool		AGPInitOK;
    int			CRT1ModeNo;		/* Current display mode for CRT1 */
    DisplayModePtr	CRT1DMode;		/* Current display mode for CRT1 */
    int 		CRT2ModeNo;		/* Current display mode for CRT2 */
    DisplayModePtr	CRT2DMode;		/* Current display mode for CRT2 */
    Bool		CRT2ModeSet;		/* CRT2 mode has been set */
    Bool		CRT2IsCustom;
    unsigned char	CRT2CR30, CRT2CR31, CRT2CR35, CRT2CR38;
    int			refCount;
    int 		lastInstance;		/* number of entities */
    Bool		DisableDual;		/* Emergency flag */
    Bool		ErrorAfterFirst;	/* Emergency flag: Error after first init -> Abort second */
    Bool		HWCursor;		/* Backup master settings for use on slave */
    Bool                TurboQueue;
    int                 ForceCRT1Type;
    int                 ForceCRT2Type;
    int                 OptTVStand;
    int                 OptTVOver;
    int			OptTVSOver;
    int                 OptROMUsage;
    int			OptUseOEM;
    Bool                NoAccel;
    int			forceCRT1;
    int			DSTN, FSTN;
    Bool		XvOnCRT2;
    int                 maxUsedClock;  		/* Max used pixelclock on master head */
    unsigned long       masterFbAddress;	/* Framebuffer addresses and sizes */
    unsigned long	masterFbSize;
    unsigned long       slaveFbAddress;
    unsigned long	slaveFbSize;
    unsigned char *     FbBase;         	/* VRAM linear address */
    unsigned char *     IOBase;         	/* MMIO linear address */
    unsigned short      MapCountIOBase;		/* map/unmap queue counter */
    unsigned short      MapCountFbBase;		/* map/unmap queue counter */
    Bool 		forceUnmapIOBase;	/* ignore counter and unmap */
    Bool		forceUnmapFbBase;	/* ignore counter and unmap */
#ifdef __alpha__
    unsigned char *     IOBaseDense;    	/* MMIO for Alpha platform */
    unsigned short      MapCountIOBaseDense;
    Bool		forceUnmapIOBaseDense;  /* ignore counter and unmap */
#endif
    int			chtvlumabandwidthcvbs;  /* TV settings for Chrontel TV encoder */
    int			chtvlumabandwidthsvideo;
    int			chtvlumaflickerfilter;
    int			chtvchromabandwidth;
    int			chtvchromaflickerfilter;
    int			chtvcvbscolor;
    int			chtvtextenhance;
    int			chtvcontrast;
    int			sistvedgeenhance;	/* TV settings for SiS bridge */
    int			sistvantiflicker;
    int			sistvsaturation;
    int       		sistvcolcalibc;
    int       		sistvcolcalibf;
    int			sistvcfilter;
    int			sistvyfilter;
    int			tvxpos, tvypos;
    int		      	tvxscale, tvyscale;
    int			ForceTVType, SenseYPbPr;
    unsigned long	ForceYPbPrType, ForceYPbPrAR;
    int			chtvtype;
    int                 NonDefaultPAL, NonDefaultNTSC;
    unsigned short	tvx, tvy;
    unsigned char	p2_01, p2_02, p2_1f, p2_20, p2_43, p2_42, p2_2b;
    unsigned char	p2_44, p2_45, p2_46;
    unsigned long       sistvccbase;
    unsigned char       p2_35, p2_36, p2_37, p2_38, p2_48, p2_49, p2_4a;
    unsigned char	p2_0a, p2_2f, p2_30, p2_47;
    unsigned char       scalingp1[9], scalingp4[9], scalingp2[64];
    unsigned short      cursorBufferNum;
    BOOLEAN		restorebyset;
    BOOLEAN		CRT1gamma, CRT1gammaGiven, CRT2gamma, XvGamma, XvGammaGiven;
    int			XvGammaRed, XvGammaGreen, XvGammaBlue;
    int			GammaBriR, GammaBriG, GammaBriB;	/* strictly for Xinerama */
    int			GammaPBriR, GammaPBriG, GammaPBriB;	/* strictly for Xinerama */
    int			curxvcrtnum;
    int			UsePanelScaler, CenterLCD;
    int			AllowHotkey;
    BOOLEAN		enablesisctrl;
    unsigned long	cmdQ_SharedWritePort_2D;
    unsigned char       *RenderAccelArray;
    unsigned char *	FbBase1;
    unsigned long	OnScreenSize1;
    unsigned char       OldMode;
    int			HWCursorMBufNum, HWCursorCBufNum;
    BOOLEAN		ROM661New;
#ifdef SIS_CP
    SIS_CP_H_ENT
#endif
} SISEntRec, *SISEntPtr;
#endif

#define SISPTR(p)       ((SISPtr)((p)->driverPrivate))
#define XAAPTR(p)       ((XAAInfoRecPtr)(SISPTR(p)->AccelInfoPtr))

/* Relative merge position */
typedef enum {
   sisLeftOf,
   sisRightOf,
   sisAbove,
   sisBelow,
   sisClone
} SiSScrn2Rel;

typedef struct {
    ScrnInfoPtr         pScrn;		/* -------------- DON'T INSERT ANYTHING HERE --------------- */
    pciVideoPtr         PciInfo;	/* -------- OTHERWISE sis_dri.so MUST BE RECOMPILED -------- */
    PCITAG              PciTag;
    EntityInfoPtr       pEnt;
    int                 Chipset;
    int                 ChipRev;
    int			VGAEngine;      /* see above */
    int	                hasTwoOverlays; /* Chipset supports two video overlays? */
    HW_DEVICE_EXTENSION sishw_ext;      /* For new mode switching code */
    SiS_Private   *     SiS_Pr;         /* For new mode switching code */
    int			DSTN; 		/* For 550 FSTN/DSTN; set by option, no detection */
    unsigned long       FbAddress;      /* VRAM physical address (in DHM: for each Fb!) */
    unsigned long       realFbAddress;  /* For DHM/PCI mem mapping: store global FBAddress */
    unsigned char *     FbBase;         /* VRAM virtual linear address */
    CARD32              IOAddress;      /* MMIO physical address */
    unsigned char *     IOBase;         /* MMIO linear address */
    IOADDRESS           IODBase;        /* Base of PIO memory area */
#ifdef __alpha__
    unsigned char *     IOBaseDense;    /* MMIO for Alpha platform */
#endif
    SISIOADDRESS        RelIO;          /* Relocated IO Ports baseaddress */
    unsigned char *     BIOS;
    int                 MemClock;
    int                 BusWidth;
    int                 MinClock;
    int                 MaxClock;
    int                 Flags;          /* HW config flags */
    long                FbMapSize;	/* Used for Mem Mapping - DON'T CHANGE THIS */
    long                availMem;       /* Really available Fb mem (minus TQ, HWCursor) */
    unsigned long	maxxfbmem;      /* limit fb memory X is to use to this (KB) */
    unsigned long       sisfbMem;       /* heapstart of sisfb (if running) */
#ifdef SISDUALHEAD
    unsigned long	dhmOffset;	/* Offset to memory for each head (0 or ..) */
#endif
    DGAModePtr          DGAModes;
    int                 numDGAModes;
    Bool                DGAactive;
    int                 DGAViewportStatus;
    unsigned char       OldMode;        /* Back old modeNo (if available) */
    Bool                NoAccel;
    Bool                NoXvideo;
    Bool		XvOnCRT2;       /* see sis_opt.c */
    Bool                HWCursor;
    Bool                UsePCIRetry;
    Bool                TurboQueue;
    int			VESA;
    int                 ForceCRT1Type;
    int                 ForceCRT2Type;
    int                 OptTVStand;
    int                 OptTVOver;
    int                 OptROMUsage;
    int                 UseCHOverScan;
    Bool                ValidWidth;
    Bool                FastVram;		/* now unused */
    int			forceCRT1;
    Bool		CRT1changed;
    unsigned char       oldCR17, oldCR63, oldSR1F;
    unsigned char       oldCR32, oldCR36, oldCR37;
    unsigned char       myCR32, myCR36, myCR37, myCR63;
    unsigned char       newCR32;
    unsigned long   	VBFlags;		/* Video bridge configuration */
    unsigned long       VBFlags_backup;         /* Backup for SlaveMode-modes */
    unsigned long	VBLCDFlags;             /* Moved LCD panel size bits here */
    int                 ChrontelType;           /* CHRONTEL_700x or CHRONTEL_701x */
    unsigned int        PDC, PDCA;		/* PanelDelayCompensation */
    short               scrnOffset;		/* Screen pitch (data) */
    short               scrnPitch;		/* Screen pitch (display; regarding interlace) */
    short               DstColor;
    int                 xcurrent;               /* for temp use in accel */
    int                 ycurrent;               /* for temp use in accel */
    long		SiS310_AccelDepth; 	/* used in accel for 315 series */
    int                 Xdirection;  		/* for temp use in accel */
    int                 Ydirection;  		/* for temp use in accel */
    int                 sisPatternReg[4];
    int                 ROPReg;
    int                 CommandReg;
    int                 MaxCMDQueueLen;
    int                 CurCMDQueueLen;
    int                 MinCMDQueueLen;
    CARD16		CursorSize;  		/* Size of HWCursor area (bytes) */
    CARD32		cursorOffset;		/* see sis_driver.c and sis_cursor.c */
    int                 DstX;
    int                 DstY;
    unsigned char *     XAAScanlineColorExpandBuffers[2];
    CARD32              AccelFlags;
    Bool                ClipEnabled;
    Bool                DoColorExpand;
    Bool                ColorExpandBusy;
    Bool                alphaBlitBusy;
    SISRegRec           SavedReg;
    SISRegRec           ModeReg;
    xf86CursorInfoPtr   CursorInfoPtr;
    XAAInfoRecPtr       AccelInfoPtr;
    CloseScreenProcPtr  CloseScreen;
    Bool        	(*ModeInit)(ScrnInfoPtr pScrn, DisplayModePtr mode);
    void        	(*SiSSave)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSave2)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSave3)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSSaveLVDSChrontel)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore2)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestore3)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*SiSRestoreLVDSChrontel)(ScrnInfoPtr pScrn, SISRegPtr sisreg);
    void        	(*LoadCRT2Palette)(ScrnInfoPtr pScrn, int numColors,
                		int *indicies, LOCO *colors, VisualPtr pVisual);

    int       		cmdQueueLen;		/* Current cmdQueueLength (for 2D and 3D) */
    unsigned long	cmdQueueLenMax;
    unsigned long	cmdQueueLenMin;
    unsigned long	*cmdQueueBase;
    int			*cmdQueueLenPtr;	/* Ptr to variable holding the current queue length */
    int			*cmdQueueLenPtrBackup;	/* Backup for DRI init/restore */
    unsigned int        cmdQueueOffset;
    unsigned int        cmdQueueSize;
    unsigned long       cmdQueueSizeMask;
    unsigned long	cmdQ_SharedWritePort_2D;
    unsigned long	*cmdQ_SharedWritePort;
    unsigned long	*cmdQ_SharedWritePortBackup;
    unsigned int        cmdQueueSize_div2;
    unsigned int        cmdQueueSize_div4;
    unsigned int        cmdQueueSize_4_3;
    unsigned long 	agpHandle;
    unsigned long	agpAddr;
    unsigned char 	*agpBase;
    unsigned int 	agpSize;
    unsigned int        agpWantedSize;
    unsigned int        agpWantedPages;
    unsigned long	agpCmdBufAddr;  /* 300 series */
    unsigned char 	*agpCmdBufBase;
    unsigned int 	agpCmdBufSize;
    unsigned int 	agpCmdBufFree;
    unsigned long	agpVtxBufAddr;	/* 315 series */
    unsigned char       *agpVtxBufBase;
    unsigned int        agpVtxBufSize;
    unsigned int        agpVtxBufFree;
#ifdef XF86DRI
    sisRegion 		agp;
#endif
    Bool		AGPInitOK;
    Bool 		irqEnabled;
    int 		irq;
    Bool		IsAGPCard;
    unsigned long	DRIheapstart, DRIheapend;

    void		(*RenderCallback)(ScrnInfoPtr);
    Time		RenderTime;
    unsigned char       *RenderAccelArray;
    Bool		doRender;

    int 		ColorExpandRingHead;
    int 		ColorExpandRingTail;
    int 		PerColorExpandBufferSize;
    int 		ColorExpandBufferNumber;
    int 		ColorExpandBufferCountMask;
    unsigned char 	*ColorExpandBufferAddr[32];
    int 		ColorExpandBufferScreenOffset[32];
    long       		ColorExpandBase;
    int 		ImageWriteBufferSize;
    unsigned char 	*ImageWriteBufferAddr;

    int 		Rotate;
    void        	(*PointerMoved)(int index, int x, int y);

    /* ShadowFB support */
    Bool 		ShadowFB;
    unsigned char 	*ShadowPtr;
    int  		ShadowPitch;

    Bool		loadDRI;

#ifdef XF86DRI
    Bool 		directRenderingEnabled;
    DRIInfoPtr 		pDRIInfo;
    int 		drmSubFD;
    int 		numVisualConfigs;
    __GLXvisualConfig* 	pVisualConfigs;
    SISConfigPrivPtr 	pVisualConfigsPriv;
    SISRegRec 		DRContextRegs;
#endif

    XF86VideoAdaptorPtr adaptor;
    ScreenBlockHandlerProcPtr BlockHandler;
    void                (*VideoTimerCallback)(ScrnInfoPtr, Time);
    void		(*ResetXv)(ScrnInfoPtr);
    void		(*ResetXvGamma)(ScrnInfoPtr);

    OptionInfoPtr 	Options;
    unsigned char 	LCDon;
#ifdef SISDUALHEAD
    Bool		BlankCRT1, BlankCRT2;
#endif
    Bool 		Blank;
    unsigned char 	BIOSModeSave;
    int 		CRT1off;		/* 1=CRT1 off, 0=CRT1 on */
    CARD16 		LCDheight;		/* Vertical resolution of LCD panel */
    CARD16 		LCDwidth;		/* Horizontal resolution of LCD panel */
    vbeInfoPtr 		pVbe;			/* For VESA mode switching */
    CARD16 		vesamajor;
    CARD16 		vesaminor;
    VbeInfoBlock 	*vbeInfo;
    int 		UseVESA;
    sisModeInfoPtr      SISVESAModeList;
    xf86MonPtr 		monitor;
    CARD16 		maxBytesPerScanline;
    CARD32 		*pal, *savedPal;
    int 		mapPhys, mapOff, mapSize;
    int 		statePage, stateSize, stateMode;
    CARD8 		*fonts;
    CARD8 		*state, *pstate;
    void 		*base, *VGAbase;
#ifdef SISDUALHEAD
    BOOL 		DualHeadMode;		/* TRUE if we use dual head mode */
    BOOL 		SecondHead;		/* TRUE is this is the second head */
    SISEntPtr 		entityPrivate;		/* Ptr to private entity (see above) */
    BOOL		SiSXinerama;		/* Do we use Xinerama mode? */
#endif
    SISFBLayout         CurrentLayout;		/* Current framebuffer layout */
    USHORT              SiS_DDC2_Index;
    USHORT              SiS_DDC2_Data;
    USHORT              SiS_DDC2_Clk;
    BOOL		Primary;		/* Display adapter is primary */
    xf86Int10InfoPtr    pInt;			/* Our int10 */
    int                 oldChipset;		/* Type of old chipset */
    int              	RealVideoRam;		/* 6326 can only address 4MB, but TQ can be above */
    CARD32              CmdQueLenMask;		/* Mask of queue length in MMIO register */
    CARD32              CmdQueLenFix;           /* Fix value to subtract from QueLen (530/620) */
    CARD32              CmdQueMaxLen;           /* (6326/5597/5598) Amount of cmds the queue can hold */
    CARD32              TurboQueueLen;		/* For future use */
    CARD32              detectedCRT2Devices;	/* detected CRT2 devices before mask-out */
    Bool                NoHostBus;		/* Enable/disable 5597/5598 host bus */
    Bool		noInternalModes;	/* Use our own default modes? */
    int			OptUseOEM;		/* Use internal OEM data? */
    int			chtvlumabandwidthcvbs;  /* TV settings for Chrontel TV encoder */
    int			chtvlumabandwidthsvideo;
    int			chtvlumaflickerfilter;
    int			chtvchromabandwidth;
    int			chtvchromaflickerfilter;
    int			chtvcvbscolor;
    int			chtvtextenhance;
    int			chtvcontrast;
    int			sistvedgeenhance;	/* TV settings for SiS bridges */
    int			sistvantiflicker;
    int			sistvsaturation;
    int       		sistvcolcalibc;
    int       		sistvcolcalibf;
    int			sistvcfilter;
    int			sistvyfilter;
    int			OptTVSOver;		/* Chrontel 7005: Superoverscan */
    int			tvxpos, tvypos;
    int		      	tvxscale, tvyscale;
    int			SiS6326Flags;		/* SiS6326 TV settings */
    int			sis6326enableyfilter;
    int			sis6326yfilterstrong;
    int			sis6326tvplug;
    int			sis6326fscadjust;
    BOOL		sisfbfound;
    BOOL		donttrustpdc;		/* Don't trust the detected PDC */
    unsigned char	sisfbpdc, sisfbpdca;
    unsigned char       sisfblcda;
    int			sisfbscalelcd;
    unsigned long	sisfbspecialtiming;
    BOOL		sisfb_haveemi, sisfb_haveemilcd;
    unsigned char	sisfb_emi30,sisfb_emi31,sisfb_emi32,sisfb_emi33;
    int			EMI;
    int			NoYV12;			/* Disable Xv YV12 support (old series) */
    unsigned char       postVBCR32;
    int			newFastVram;		/* Replaces FastVram */
    int			ForceTVType, SenseYPbPr;
    int                 NonDefaultPAL, NonDefaultNTSC;
    unsigned long	ForceYPbPrType, ForceYPbPrAR;
    unsigned long       lockcalls;		/* Count unlock calls for debug */
    unsigned short	tvx, tvy;		/* Backup TV position registers */
    unsigned char	p2_01, p2_02, p2_1f, p2_20, p2_43, p2_42, p2_2b; /* Backup TV position registers */
    unsigned short      tvx1, tvx2, tvx3, tvy1; /* Backup TV position registers */
    unsigned char	p2_44, p2_45, p2_46;
    unsigned long       sistvccbase;
    unsigned char       p2_35, p2_36, p2_37, p2_38, p2_48, p2_49, p2_4a;
    unsigned char	p2_0a, p2_2f, p2_30, p2_47;
    unsigned char       scalingp1[9], scalingp4[9], scalingp2[64];
    BOOLEAN		ForceCursorOff;
    BOOLEAN		HaveCustomModes;
    BOOLEAN		IsCustom;
    DisplayModePtr	backupmodelist;
    int			chtvtype;
    Atom                xvBrightness, xvContrast, xvColorKey, xvHue, xvSaturation;
    Atom                xvAutopaintColorKey, xvSetDefaults, xvSwitchCRT;
    Atom		xvDisableGfx, xvDisableGfxLR, xvTVXPosition, xvTVYPosition;
    Atom		xvDisableColorkey, xvUseChromakey, xvChromaMin, xvChromaMax;
    Atom		xvInsideChromakey, xvYUVChromakey;
    Atom		xvGammaRed, xvGammaGreen, xvGammaBlue;
    Atom		xv_QVF, xv_QVV, xv_USD, xv_SVF, xv_QDD, xv_TAF, xv_TSA, xv_TEE, xv_GSF;
    Atom		xv_TTE, xv_TCO, xv_TCC, xv_TCF, xv_TLF, xv_CMD, xv_CMDR, xv_CT1, xv_SGA;
    Atom		xv_GDV, xv_GHI, xv_OVR, xv_GBI, xv_TXS, xv_TYS, xv_CFI, xv_COC, xv_COF;
    Atom		xv_YFI, xv_GSS, xv_BRR, xv_BRG, xv_BRB, xv_PBR, xv_PBG, xv_PBB, xv_SHC;
    Atom		xv_BRR2, xv_BRG2, xv_BRB2, xv_PBR2, xv_PBG2, xv_PBB2, xv_PMD;
#ifdef TWDEBUG
    Atom		xv_STR;
#endif        
    int			xv_sisdirectunlocked;
    unsigned long	xv_sd_result;
    int			CRT1isoff;
#ifdef SIS_CP
    SIS_CP_H
#endif
    unsigned long       ChipFlags;
    unsigned long       SiS_SD_Flags;
    BOOLEAN		UseHWARGBCursor;
    int			OptUseColorCursor;
    int			OptUseColorCursorBlend;
    CARD32		OptColorCursorBlendThreshold;
    unsigned short      cursorBufferNum;
    int                 vb;
    BOOLEAN		restorebyset;
    BOOLEAN		nocrt2ddcdetection;
    BOOLEAN		forcecrt2redetection;
    BOOLEAN		CRT1gamma, CRT1gammaGiven, CRT2gamma, XvGamma, XvGammaGiven;
    int			XvDefCon, XvDefBri, XvDefHue, XvDefSat;
    BOOLEAN		XvDefDisableGfx, XvDefDisableGfxLR;
    BOOLEAN		XvUseMemcpy;
    BOOLEAN		XvUseChromaKey, XvDisableColorKey;
    BOOLEAN		XvInsideChromaKey, XvYUVChromaKey;
    int			XvChromaMin, XvChromaMax;
    int			XvGammaRed, XvGammaGreen, XvGammaBlue;
    int			XvGammaRedDef, XvGammaGreenDef, XvGammaBlueDef;
    CARD8		XvGammaRampRed[256], XvGammaRampGreen[256], XvGammaRampBlue[256];
    BOOLEAN		disablecolorkeycurrent;
    CARD32		colorKey;
    CARD32		MiscFlags;
    int			UsePanelScaler, CenterLCD;
    FBLinearPtr		AccelLinearScratch;
    void		(*AccelRenderCallback)(ScrnInfoPtr);
    float		zClearVal;
    unsigned long	bClrColor, dwColor;
    int			AllowHotkey;
    BOOLEAN		enablesisctrl;
    short		Video_MaxWidth, Video_MaxHeight;
    int			FSTN;
    BOOLEAN		AddedPlasmaModes;
    short               scrnPitch2;
    CARD32		CurFGCol, CurBGCol;
    unsigned char *	CurMonoSrc;
    CARD32 *            CurARGBDest;
    int			GammaBriR, GammaBriG, GammaBriB;
    int			GammaPBriR, GammaPBriG, GammaPBriB;
    Bool		HideHWCursor;  /* Custom application */
    Bool		HWCursorIsVisible;
    unsigned long       HWCursorBackup[16];
    int			HWCursorMBufNum, HWCursorCBufNum;
    unsigned long	mmioSize;
    BOOLEAN		ROM661New;
#ifdef SISMERGED
    Bool		MergedFB, MergedFBAuto;
    SiSScrn2Rel		CRT2Position;
    char *		CRT2HSync;
    char *		CRT2VRefresh;
    char *		MetaModes;
    ScrnInfoPtr		CRT2pScrn;
    DisplayModePtr	CRT1Modes;
    DisplayModePtr	CRT1CurrentMode;
    int			CRT1frameX0;
    int			CRT1frameY0;
    int			CRT1frameX1;
    int			CRT1frameY1;
    Bool		CheckForCRT2;
    Bool		IsCustomCRT2;
    BOOLEAN 		HaveCustomModes2;
    int			maxCRT1_X1, maxCRT1_X2, maxCRT1_Y1, maxCRT1_Y2;
    int			maxCRT2_X1, maxCRT2_X2, maxCRT2_Y1, maxCRT2_Y2;
    int			maxClone_X1, maxClone_X2, maxClone_Y1, maxClone_Y2;
    int			MergedFBXDPI, MergedFBYDPI;
#ifdef SISXINERAMA
    Bool		UseSiSXinerama;
    Bool		CRT2IsScrn0;
    ExtensionEntry 	*XineramaExtEntry;
    int			SiSXineramaVX, SiSXineramaVY;
    Bool		AtLeastOneNonClone;
#endif
#endif
} SISRec, *SISPtr;

typedef struct _ModeInfoData {
    int mode;
    VbeModeInfoBlock *data;
    VbeCRTCInfoBlock *block;
} ModeInfoData;

#define SDMPTR(x) ((SiSMergedDisplayModePtr)(x->currentMode->Private))
#define CDMPTR    ((SiSMergedDisplayModePtr)(pSiS->CurrentLayout.mode->Private))

#define BOUND(test,low,hi) { \
    if(test < low) test = low; \
    if(test > hi) test = hi; }

#define REBOUND(low,hi,test) { \
    if(test < low) { \
        hi += test-low; \
        low = test; } \
    if(test > hi) { \
        low += test-hi; \
        hi = test; } }

typedef struct _MergedDisplayModeRec {
    DisplayModePtr CRT1;
    DisplayModePtr CRT2;
    SiSScrn2Rel    CRT2Position;
} SiSMergedDisplayModeRec, *SiSMergedDisplayModePtr;


typedef struct _region {
    int x0,x1,y0,y1;
} region;

typedef struct _myhddctiming {
    int           whichone;
    unsigned char mask;
    float         rate;
} myhddctiming;

typedef struct _myvddctiming {
    int           whichone;
    unsigned char mask;
    int           rate;
} myvddctiming;

typedef struct _myddcstdmodes {
    int hsize;
    int vsize;
    int refresh;
    float hsync;
} myddcstdmodes;

typedef struct _pdctable {
    int subsysVendor;
    int subsysCard;
    int pdc;
    char *vendorName;
    char *cardName;
} pdctable;

typedef struct _chswtable {
    int subsysVendor;
    int subsysCard;
    char *vendorName;
    char *cardName;
} chswtable;

typedef struct _customttable {
    unsigned short chipID;
    char *biosversion;
    char *biosdate;
    unsigned long bioschksum;
    unsigned short biosFootprintAddr[5];
    unsigned char biosFootprintData[5];
    unsigned short pcisubsysvendor;
    unsigned short pcisubsyscard;
    char *vendorName;
    char *cardName;
    unsigned long SpecialID;
    char *optionName;
} customttable;

#ifdef SISMERGED
#ifdef SISXINERAMA
typedef struct _SiSXineramaData {
    int x;
    int y;
    int width;
    int height;
} SiSXineramaData;
#endif
#endif

#if 0
/* SiS Direct access for config utility */

#define SDC_ID 0x53495321

#define SDC_VERSION 1

#define SDC_NUM_PARM 20
typedef struct _sisdirectcommand {
    unsigned long sdc_id;
    unsigned long sdc_chksum;
    unsigned long sdc_header;
    unsigned long sdc_command;
    unsigned long sdc_parm[SDC_NUM_PARM];
} sisdirectcommand;

#define SDC_RESULT_OK  		0x66670000
#define SDC_RESULT_UNDEFCMD	0x66670001

#define SDC_CMD_GETVERSION 		0x98980001
#define SDC_CMD_CHECKMODEFORCRT2 	0x98980002
/* more to come */
#endif

extern void  sisSaveUnlockExtRegisterLock(SISPtr pSiS, unsigned char *reg1, unsigned char *reg2);
extern void  sisRestoreExtRegisterLock(SISPtr pSiS, unsigned char reg1, unsigned char reg2);
extern void  SiSOptions(ScrnInfoPtr pScrn);
extern const OptionInfoRec * SISAvailableOptions(int chipid, int busid);
extern void  SiSSetup(ScrnInfoPtr pScrn);
extern void  SISVGAPreInit(ScrnInfoPtr pScrn);
extern Bool  SiSAccelInit(ScreenPtr pScreen);
extern Bool  SiS300AccelInit(ScreenPtr pScreen);
extern Bool  SiS315AccelInit(ScreenPtr pScreen);
extern Bool  SiS530AccelInit(ScreenPtr pScreen);
extern Bool  SiSHWCursorInit(ScreenPtr pScreen);
extern Bool  SISDGAInit(ScreenPtr pScreen);
extern void  SISInitVideo(ScreenPtr pScreen);
extern void  SIS6326InitVideo(ScreenPtr pScreen);

extern void  SiS_SetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVlumaflickerfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVchromabandwidth(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVchromaflickerfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVcvbscolor(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVtextenhance(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetCHTVcontrast(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVedgeenhance(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVantiflicker(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVsaturation(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVcfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVyfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSISTVcolcalib(ScrnInfoPtr pScrn, int val, Bool coarse);
extern void  SiS_SetSIS6326TVantiflicker(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSIS6326TVenableyfilter(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVxposoffset(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVyposoffset(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVxscale(ScrnInfoPtr pScrn, int val);
extern void  SiS_SetTVyscale(ScrnInfoPtr pScrn, int val);
extern Bool  SISSwitchCRT2Type(ScrnInfoPtr pScrn, unsigned long newvbflags);
extern int   SISCheckModeIndexForCRT2Type(ScrnInfoPtr pScrn, unsigned short cond,
					  unsigned short index, Bool quiet);
extern Bool  SISSwitchCRT1Status(ScrnInfoPtr pScrn, int onoff);
extern int   SiS_GetCHTVlumabandwidthcvbs(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVlumabandwidthsvideo(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVlumaflickerfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVchromabandwidth(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVchromaflickerfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVcvbscolor(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVtextenhance(ScrnInfoPtr pScrn);
extern int   SiS_GetCHTVcontrast(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVedgeenhance(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVantiflicker(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVsaturation(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVcfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVyfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetSISTVcolcalib(ScrnInfoPtr pScrn, Bool coarse);
extern int   SiS_GetSIS6326TVantiflicker(ScrnInfoPtr pScrn);
extern int   SiS_GetSIS6326TVenableyfilter(ScrnInfoPtr pScrn);
extern int   SiS_GetSIS6326TVyfilterstrong(ScrnInfoPtr pScrn);
extern int   SiS_GetTVxposoffset(ScrnInfoPtr pScrn);
extern int   SiS_GetTVyposoffset(ScrnInfoPtr pScrn);
extern int   SiS_GetTVxscale(ScrnInfoPtr pScrn);
extern int   SiS_GetTVyscale(ScrnInfoPtr pScrn);
#endif
