
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng.h,v 1.37 2002/04/04 14:05:49 eich Exp $ */





#ifndef _TSENG_H
#define _TSENG_H

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"

/* All drivers need this */
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

/* This is used for module versioning */
#include "xf86Version.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* All drivers using the vgahw module need this */
/* All Tseng chips _need_ VGA register access, so multihead operation is out of the question */
#include "vgaHW.h"

/* Drivers using the mi banking wrapper need this */
#include "mibank.h"

/* All drivers using the mi colormap manipulation need this */
#include "micmap.h"

/* Needed for the 1 and 4 bpp framebuffers */
#include "xf1bpp.h"
#include "xf4bpp.h"
#include "fb.h"

/* Drivers using the XAA interface ... */
#include "xaa.h"
#include "xaalocal.h"
#include "xf86Cursor.h"
#include "xf86fbman.h"


/* functions in tseng_driver.c needed outside it */
void TsengBlankScreen(ScrnInfoPtr pScrn, Bool unblank);
void TsengProtect(ScrnInfoPtr pScrn, Bool on);


#define MAX_TSENG_CLOCK 86000	       /* default max clock for standard boards */

/*
 * Contrary to the old driver, we use the "Chip Revision" here intead of
 * multiple chipsets like "TYPE_ET4000W32Pa", "TYPE_ET4000W32Pb", etc.
 */

typedef enum {
    TYPE_ET4000,
    TYPE_ET4000W32,
    TYPE_ET4000W32I,
    TYPE_ET4000W32P,
    TYPE_ET6000,
    TYPE_ET6100,
    TYPE_TSENG
} t_tseng_type;

/* revision ID for W32 chips: currently used for W32i and W32p */
typedef enum {
    TSENGNOREV = 0,
    W32REVID_A,
    W32REVID_B,
    W32REVID_C,
    W32REVID_D
} t_w32_revid;

#define ET6100REVID (0x70)

typedef enum {
    T_BUS_ISA,
    T_BUS_MCA,
    T_BUS_VLB,
    T_BUS_PCI
} t_tseng_bus;

extern SymTabRec TsengDacTable[];

typedef enum {
    UNKNOWN_DAC = -1,
    NORMAL_DAC,
    ATT20C47xA_DAC,
    Sierra1502X_DAC,
    ATT20C497_DAC,
    ATT20C490_DAC,
    ATT20C493_DAC,
    ATT20C491_DAC,
    ATT20C492_DAC,
    ICS5341_DAC,
    ICS5301_DAC,
    STG1700_DAC,
    STG1702_DAC,
    STG1703_DAC,
    ET6000_DAC,
    CH8398_DAC,
    MUSIC4910_DAC
} t_ramdactype;

typedef enum {
    CLOCKCHIP_ICD2061A,
    CLOCKCHIP_ET6000,
    CLOCKCHIP_ICS5341,
    CLOCKCHIP_ICS5301,
    CLOCKCHIP_CH8398,
    CLOCKCHIP_STG1703
} t_clockchip_type;

typedef enum {
    TSENG_MODE_NORMAL,
    TSENG_MODE_PIXMUX,
    TSENG_MODE_DACBUS16
} t_clockrange_type;

typedef struct {
    unsigned char cmd_reg;
    unsigned char f2_M;
    unsigned char f2_N;
    unsigned char ctrl;
    unsigned char w_idx;
    unsigned char r_idx;
    unsigned char timingctrl;	       /* for STG170x */
    unsigned char MClkM;
    unsigned char dummy;   /* FIXME!!! : someone overwrites saved MClkN without this */
    unsigned char MClkN;        /* PLL M/N values for MemClk programming */
} PllState;

typedef struct {
    unsigned char ExtCRTC[16];	       /* CRTC 0x30 .. 0x3F */
    unsigned char ExtTS[2];	       /* TS 0x06 .. 0x07 */
    unsigned char ExtATC;	       /* ATC 0x16 */
    unsigned char ExtSegSel[2];	       /* 0x3CD , 0x3CB */
    unsigned char ExtET6K[0x4F];       /* ET6000 PCI config space registers 0x40 .. 0x8F */
    unsigned char ExtIMACtrl;	       /* IMA port control register (0x217B index 0xF7) */
    PllState pll;		       /* registers in GenDAC-like RAMDAC/clockchips */
    unsigned char ATTdac_cmd;	       /* command register for ATT 49x DACs */
} TsengRegRec, *TsengRegPtr;

typedef struct {
    unsigned char save1, save2, save3, save4;
} clock_save;

typedef struct {
    Bool Programmable;	      	       /* MemClk is programmable if set */
    Bool Set;			       /* reprogram MClk if TRUE */
    int MemClk;                        /* MemClk value in kHz */
    int min, max;	  	       /* MemClk limits */
} TsengMClkInfoRec, *TsengMclkInfoPtr;

typedef struct {
    int saved_cr;
    int rmr;
} dac_save;

typedef struct {
    t_ramdactype DacType;
    Bool NotAttCompat;		       /* avoid treating the RAMDAC as AT&T compatible */
    int RamdacShift;		       /* typically 10 or 8 for 6- or 8-bit dac */
    int RamdacMask;		       /* typically 0x3f for 6 bit, 0xff for 8-bit ramdac */
    Bool Dac8Bit;		       /* dac is 8 bit instead of the default 6 bit */
    Bool DacPort16;		       /* Ramdac port is 16 bits wide instead of default 8 */
    rgb rgb24packed;
} TsengDacInfoRec, *TsengDacInfoPtr;

typedef struct {
    /* we'll put variables that we want to access _fast_ at the beginning (just a hunch) */
    unsigned char cache_SegSelL, cache_SegSelH;  /* for tseng_bank.c */
    int Bytesperpixel;		       /* a shorthand for the XAA code */
    Bool need_wait_acl;		       /* always need a full "WAIT" for ACL finish */
    int line_width;		       /* framebuffer width in bytes per scanline */
    int planemask_mask;		       /* mask for active bits in planemask */
    int neg_x_pixel_offset;
    int powerPerPixel;		       /* power-of-2 version of bytesperpixel */
    unsigned char *BresenhamTable;
    /* normal stuff starts here */
    pciVideoPtr PciInfo;
    PCITAG PciTag;
    int Save_Divide;
    Bool UsePCIRetry;		       /* Do we use PCI-retry or busy-waiting */
    Bool UseAccel;		       /* Do we use the XAA acceleration architecture */
    Bool HWCursor;		       /* Do we use the hardware cursor (if supported) */
    Bool Linmem_1meg;		       /* Is this a card limited to 1Mb of linear memory */
    Bool UseLinMem;
    Bool SlowDram;
    Bool FastDram;
    Bool MedDram;
    Bool SetPCIBurst;
    Bool PCIBurst;
    Bool SetW32Interleave;
    Bool W32Interleave;
    Bool ShowCache;
    Bool Legend;		       /* Sigma Legend clock select method */
    Bool NoClockchip;		       /* disable clockchip programming clockchip (=use set-of-clocks) */
    TsengRegRec SavedReg;	       /* saved Tseng registers at server start */
    TsengRegRec ModeReg;
    unsigned long icd2061_dwv;	       /* To hold the clock data between Init and Restore */
    t_tseng_bus Bustype;	       /* W32 bus type (currently used for lin mem on W32i) */
    t_tseng_type ChipType;	       /* "Chipset" causes confusion with pScrn->chipset */
    int ChipRev;
    memType LinFbAddress;
    unsigned char *FbBase;
    memType LinFbAddressMask;
    long FbMapSize;
    miBankInfoRec BankInfo;
    CARD32 IOAddress;		       /* PCI config space base address for ET6000 */
    CARD32 MMIOBase;
    int MinClock;
    int MaxClock;
    int MemClk;
    ClockRangePtr clockRange[2];
    TsengDacInfoRec DacInfo;
    TsengMClkInfoRec MClkInfo;
    t_clockchip_type ClockChip;
    int max_vco_freq;                  /* max internal VCO frequency */
    CloseScreenProcPtr CloseScreen;
    int save_divide;
    XAAInfoRecPtr AccelInfoRec;
    xf86CursorInfoPtr CursorInfoRec;
    CARD32 AccelColorBufferOffset;     /* offset in video memory where FG and BG colors will be stored */
    CARD32 AccelColorExpandBufferOffsets[3];   /* offset in video memory for ColorExpand buffers */
    unsigned char * XAAColorExpandBuffers[3];  /* pointers to colorexpand buffers */
    CARD32 AccelImageWriteBufferOffsets[2];    /* offset in video memory for ImageWrite Buffers */
    unsigned char * XAAScanlineImageWriteBuffers[2];   /* pointers to ImageWrite Buffers */
    CARD32 HWCursorBufferOffset;
    unsigned char *HWCursorBuffer;
    unsigned char * XAAScanlineColorExpandBuffers[1];
    int acl_blitxdir;
    int acl_blitydir;
    CARD32 acl_iw_dest;
    CARD32 acl_skipleft;
    CARD32 acl_ColorExpandDst;
    int acl_colexp_width_dwords;
    int acl_colexp_width_bytes;
    dac_save dac;
    CARD32* ColExpLUT;
    clock_save save_clock;
    EntityInfoPtr       pEnt;
    char * MMioBase;
    pointer scratchMemBase;
    pointer tsengCPU2ACLBase;
    /* These will hold the ping-pong registers. */
    int tsengFg;
    int tsengBg;
    int tsengPat;
    int tseng_old_dir;
    int old_x;
    int old_y;
    int DGAnumModes;
    Bool DGAactive;
    DGAModePtr DGAModes;
    int	DGAViewportStatus;
    OptionInfoPtr Options;
} TsengRec, *TsengPtr;

#define TsengPTR(p) ((TsengPtr)((p)->driverPrivate))

#define Is_stdET4K  ( pTseng->ChipType == TYPE_ET4000 )
#define Is_W32      ( pTseng->ChipType == TYPE_ET4000W32 )
#define Is_W32i     ( pTseng->ChipType == TYPE_ET4000W32I )
#define Is_W32p     ( pTseng->ChipType == TYPE_ET4000W32P)
#define Is_ET6000   ( pTseng->ChipType == TYPE_ET6000 )
#define Is_ET6100   ( pTseng->ChipType == TYPE_ET6100 )

#define Is_W32_W32i ( Is_W32 || Is_W32i )
#define Is_W32_any  ( Is_W32 || Is_W32i || Is_W32p )
#define Is_W32p_ab  ( Is_W32p && ( (pTseng->ChipRev == W32REVID_A) || (pTseng->ChipRev == W32REVID_B) ) )
#define Is_W32p_cd  ( Is_W32p && ( (pTseng->ChipRev == W32REVID_C) || (pTseng->ChipRev == W32REVID_D) ) )
#define Is_ET6K     ( Is_ET6000 || Is_ET6100 )

#define CHIP_SUPPORTS_LINEAR ( Is_W32i || Is_W32p || Is_ET6K )

#define DAC_IS_ATT49x ( (pTseng->DacInfo.DacType == ATT20C490_DAC) \
                     || (pTseng->DacInfo.DacType == ATT20C491_DAC) \
                     || (pTseng->DacInfo.DacType == ATT20C492_DAC) \
                     || (pTseng->DacInfo.DacType == ATT20C493_DAC) \
                     || (pTseng->DacInfo.DacType == MUSIC4910_DAC) )

#define DAC_is_GenDAC ( (pTseng->DacInfo.DacType == ICS5341_DAC) \
                     || (pTseng->DacInfo.DacType == ICS5301_DAC) )

#define DAC_is_STG170x ( (pTseng->DacInfo.DacType == STG1700_DAC) \
                      || (pTseng->DacInfo.DacType == STG1702_DAC) \
                      || (pTseng->DacInfo.DacType == STG1703_DAC) )

#define DAC_IS_CHRONTEL (pTseng->DacInfo.DacType == CH8398_DAC)

#define Gendac_programmable_clock \
        ( pScrn->progClock && \
          (   (pTseng->ClockChip == CLOCKCHIP_ICS5341) \
           || (pTseng->ClockChip == CLOCKCHIP_ICS5301) \
          ) \
        )

#define STG170x_programmable_clock \
        ( pScrn->progClock && (pTseng->ClockChip == CLOCKCHIP_STG1703) )

#define ICD2061a_programmable_clock \
        ( pScrn->progClock && (pTseng->ClockChip == CLOCKCHIP_ICD2061A) )

#define CH8398_programmable_clock \
        ( pScrn->progClock && (pTseng->ClockChip == CLOCKCHIP_CH8398) )

#define ET6000_programmable_clock \
        ( pScrn->progClock && (pTseng->ClockChip == CLOCKCHIP_ET6000) )


/*
 * tseng_driver.c for DGA
 */
Bool TsengModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
void TsengAdjustFrame(int scrnIndex, int x, int y, int flags);

/*
 * tseng_dga.c
 */
Bool TsengDGAInit(ScreenPtr pScreen);

/*
 * From tseng_bank.c
 */

int ET4000SetRead(ScreenPtr pScrn, unsigned int iBank);
int ET4000SetWrite(ScreenPtr pScrn, unsigned int iBank);
int ET4000SetReadWrite(ScreenPtr pScrn, unsigned int iBank);
int ET4000W32SetRead(ScreenPtr pScrn, unsigned int iBank);
int ET4000W32SetWrite(ScreenPtr pScrn, unsigned int iBank);
int ET4000W32SetReadWrite(ScreenPtr pScrn, unsigned int iBank);

/*
 * From tseng_clocks.c
 */

Bool Tseng_check_clockchip(ScrnInfoPtr pScrn);
void tseng_clock_setup(ScrnInfoPtr pScrn);
void TsengcommonCalcClock(long freq,
    int min_m, int min_n1, int max_n1, int min_n2, int max_n2,
    long freq_min, long freq_max,
    unsigned char *mdiv, unsigned char *ndiv);

/*
 * From tseng_ramdac.c
 */

void tseng_dactopel(void);
unsigned char tseng_dactocomm(void);
unsigned char tseng_getdaccomm(void);
void tseng_setdaccomm(unsigned char comm);

Bool Check_Tseng_Ramdac(ScrnInfoPtr pScrn);
void tseng_set_ramdac_bpp(ScrnInfoPtr pScrn, DisplayModePtr mode);

/*
 * From tseng_cursor.c
 */

Bool TsengHWCursorInit(ScreenPtr pScreen);

/*
 * From tseng_dpms.c
 */

void TsengHVSyncDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags);
void TsengCrtcDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags);

/*
 * For debugging
 */

#undef TSENG_DEBUG

#ifdef TSENG_DEBUG
#define PDEBUG(arg) do { ErrorF(arg); } while (0)
#else
#define PDEBUG(arg) do {} while (0)
#endif

#endif
