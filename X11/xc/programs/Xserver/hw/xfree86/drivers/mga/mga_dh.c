/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dh.c,v 1.4 2002/01/11 15:42:57 dawes Exp $ */
/*********************************************************************
*  	G450: This is for Dual Head. 
*       Matrox Graphics
*       Author : Luugi Marsan
**********************************************************************/


/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h" 

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */  
#include "xf86Pci.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"

#define MNP_TABLE_SIZE 64
#define CLKSEL_MGA     0x0c
#define PLLLOCK        0x40

/* CRTC2 control field*/
#define C2_EN_A                     0
#define C2_EN_M                     (1 << C2_EN_A)
#define C2_HIPRILVL_A               4
#define C2_HIPRILVL_M               (7 << C2_HIPRILVL_A)
#define C2_MAXHIPRI_A               8
#define C2_MAXHIPRI_M               (7 << C2_MAXHIPRI_A)
                                    
#define C2CTL_PIXCLKSEL_SHIFT       1L
#define C2CTL_PIXCLKSEL_MASK        (3L << C2CTL_PIXCLKSEL_SHIFT)
#define C2CTL_PIXCLKSELH_SHIFT      14L
#define C2CTL_PIXCLKSELH_MASK       (1L << C2CTL_PIXCLKSELH_SHIFT)
#define C2CTL_PIXCLKSEL_PCICLK      0L
#define C2CTL_PIXCLKSEL_VDOCLK      (1L << C2CTL_PIXCLKSEL_SHIFT)
#define C2CTL_PIXCLKSEL_PIXELPLL    (2L << C2CTL_PIXCLKSEL_SHIFT)
#define C2CTL_PIXCLKSEL_VIDEOPLL    (3L << C2CTL_PIXCLKSEL_SHIFT)
#define C2CTL_PIXCLKSEL_VDCLK       (1L << C2CTL_PIXCLKSELH_SHIFT)

#define C2CTL_PIXCLKSEL_CRISTAL     (1L << C2CTL_PIXCLKSEL_SHIFT) | (1L << C2CTL_PIXCLKSELH_SHIFT)
#define C2CTL_PIXCLKSEL_SYSTEMPLL   (2L << C2CTL_PIXCLKSEL_SHIFT) | (1L << C2CTL_PIXCLKSELH_SHIFT)

#define C2CTL_PIXCLKDIS_SHIFT       3L
#define C2CTL_PIXCLKDIS_MASK        (1L << C2CTL_PIXCLKDIS_SHIFT)
#define C2CTL_PIXCLKDIS_DISABLE     (1L << C2CTL_PIXCLKDIS_SHIFT)

#define C2CTL_CRTCDACSEL_SHIFT      20L
#define C2CTL_CRTCDACSEL_MASK       (1L << C2CTL_CRTCDACSEL_SHIFT)
#define C2CTL_CRTCDACSEL_CRTC1       0
#define C2CTL_CRTCDACSEL_CRTC2      (1L << C2CTL_CRTCDACSEL_SHIFT)

/* Misc field*/
#define IOADDSEL        0x01
#define RAMMAPEN        0x02
#define CLKSEL_25175    0x00
#define CLKSEL_28322    0x04
#define CLKSEL_MGA      0x0c
#define VIDEODIS        0x10
#define HPGODDEV        0x20
#define HSYNCPOL        0x40
#define VSYNCPOL        0x80

/* XSYNCCTRL field */
#define XSYNCCTRL_DAC1HSPOL_SHIFT                   2
#define XSYNCCTRL_DAC1HSPOL_MASK                    (1 << XSYNCCTRL_DAC1HSPOL_SHIFT)
#define XSYNCCTRL_DAC1HSPOL_NEG                     (1 << XSYNCCTRL_DAC1HSPOL_SHIFT)
#define XSYNCCTRL_DAC1HSPOL_POS                     0
#define XSYNCCTRL_DAC1VSPOL_SHIFT                   3
#define XSYNCCTRL_DAC1VSPOL_MASK                    (1 << XSYNCCTRL_DAC1VSPOL_SHIFT)
#define XSYNCCTRL_DAC1VSPOL_NEG                     (1 << XSYNCCTRL_DAC1VSPOL_SHIFT)
#define XSYNCCTRL_DAC1VSPOL_POS                     0
#define XSYNCCTRL_DAC2HSPOL_SHIFT                   6
#define XSYNCCTRL_DAC2HSPOL_MASK                    (1 << XSYNCCTRL_DAC2HSPOL_SHIFT)
#define XSYNCCTRL_DAC2HSPOL_NEG                     (1 << XSYNCCTRL_DAC2HSPOL_SHIFT)
#define XSYNCCTRL_DAC2HSPOL_POS                     0
#define XSYNCCTRL_DAC2VSPOL_SHIFT                   7
#define XSYNCCTRL_DAC2VSPOL_MASK                    (1 << XSYNCCTRL_DAC2VSPOL_SHIFT)
#define XSYNCCTRL_DAC2VSPOL_NEG                     (1 << XSYNCCTRL_DAC2VSPOL_SHIFT)
#define XSYNCCTRL_DAC2VSPOL_POS                     0
#define XSYNCCTRL_DAC1HSOFF_SHIFT                   0
#define XSYNCCTRL_DAC1HSOFF_MASK                    (1 << XSYNCCTRL_DAC1HSOFF_SHIFT)
#define XSYNCCTRL_DAC1HSOFF_OFF                     (1 << XSYNCCTRL_DAC1HSOFF_SHIFT)
#define XSYNCCTRL_DAC1HSOFF_ON                      1
#define XSYNCCTRL_DAC1VSOFF_SHIFT                   1
#define XSYNCCTRL_DAC1VSOFF_MASK                    (1 << XSYNCCTRL_DAC1VSOFF_SHIFT)
#define XSYNCCTRL_DAC1VSOFF_OFF                     (1 << XSYNCCTRL_DAC1VSOFF_SHIFT)
#define XSYNCCTRL_DAC1VSOFF_ON                      0
#define XSYNCCTRL_DAC2HSOFF_SHIFT                   4
#define XSYNCCTRL_DAC2HSOFF_MASK                    (1 << XSYNCCTRL_DAC2HSOFF_SHIFT)
#define XSYNCCTRL_DAC2HSOFF_OFF                     (1 << XSYNCCTRL_DAC2HSOFF_SHIFT)
#define XSYNCCTRL_DAC2HSOFF_ON                      0
#define XSYNCCTRL_DAC2VSOFF_SHIFT                   5
#define XSYNCCTRL_DAC2VSOFF_MASK                    (1 << XSYNCCTRL_DAC2VSOFF_SHIFT)
#define XSYNCCTRL_DAC2VSOFF_OFF                     (1 << XSYNCCTRL_DAC2VSOFF_SHIFT)
#define XSYNCCTRL_DAC2VSOFF_ON                      0


/* XDISPCTRL field */
#define XDISPCTRL_DAC1OUTSEL_SHIFT                  0L
#define XDISPCTRL_DAC1OUTSEL_MASK                   1L
#define XDISPCTRL_DAC1OUTSEL_DIS                    0L
#define XDISPCTRL_DAC1OUTSEL_EN                     1L
#define XDISPCTRL_DAC2OUTSEL_SHIFT                  2L
#define XDISPCTRL_DAC2OUTSEL_MASK                   (3L << XDISPCTRL_DAC2OUTSEL_SHIFT)
#define XDISPCTRL_DAC2OUTSEL_DIS                    0L
#define XDISPCTRL_DAC2OUTSEL_CRTC1                  (1L << XDISPCTRL_DAC2OUTSEL_SHIFT)
#define XDISPCTRL_DAC2OUTSEL_CRTC2                  (2L << XDISPCTRL_DAC2OUTSEL_SHIFT)
#define XDISPCTRL_DAC2OUTSEL_TVE                    (3L << XDISPCTRL_DAC2OUTSEL_SHIFT)
#define XDISPCTRL_PANOUTSEL_SHIFT                   5L
#define XDISPCTRL_PANOUTSEL_MASK                    (3L << XDISPCTRL_PANOUTSEL_SHIFT)
#define XDISPCTRL_PANOUTSEL_DIS                     0L
#define XDISPCTRL_PANOUTSEL_CRTC1                   (1L << XDISPCTRL_PANOUTSEL_SHIFT)
#define XDISPCTRL_PANOUTSEL_CRTC2RGB                (2L << XDISPCTRL_PANOUTSEL_SHIFT)
#define XDISPCTRL_PANOUTSEL_CRTC2656                (3L << XDISPCTRL_PANOUTSEL_SHIFT)

/* XPWRCTRL field*/
#define XPWRCTRL_DAC2PDN_SHIFT                      0
#define XPWRCTRL_DAC2PDN_MASK                       (1 << XPWRCTRL_DAC2PDN_SHIFT)
#define XPWRCTRL_VIDPLLPDN_SHIFT                    1
#define XPWRCTRL_VIDPLLPDN_MASK                     (1 << XPWRCTRL_VIDPLLPDN_SHIFT)
#define XPWRCTRL_PANPDN_SHIFT                       2
#define XPWRCTRL_PANPDN_MASK                        (1 << XPWRCTRL_PANPDN_SHIFT)
#define XPWRCTRL_RFIFOPDN_SHIFT                     3
#define XPWRCTRL_RFIFOPDN_MASK                      (1 << XPWRCTRL_RFIFOPDN_SHIFT)
#define XPWRCTRL_CFIFOPDN_SHIFT                     4
#define XPWRCTRL_CFIFOPDN_MASK                      (1 << XPWRCTRL_CFIFOPDN_SHIFT)



#define POS_HSYNC                  0x00000004
#define POS_VSYNC                  0x00000008


/* Set CRTC 2*/
/* Uses the mode given by xfree86 to setup the registry */
/* Does not write to the hard yet */
void MGACRTC2Get(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo)
{
    
    
    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg = &pMga->ModeReg;
    
    xMODEINFO tmpModeInfo;
    CARD32 ulHTotal;
    CARD32 ulHDispEnd;
    CARD32 ulHBlkStr;
    CARD32 ulHBlkEnd;
    CARD32 ulHSyncStr;
    CARD32 ulHSyncEnd;
    CARD32 ulVTotal;
    CARD32 ulVDispEnd;
    CARD32 ulVBlkStr;
    CARD32 ulVBlkEnd;
    CARD32 ulVSyncStr;
    CARD32 ulVSyncEnd;
    CARD32 ulOffset;
    CARD32 ulCtl2;
    CARD32 ulDataCtl2;
    CARD32 ulDispHeight = pModeInfo->ulDispHeight;
    
#ifdef DEBUG 
    ErrorF("ENTER MGACRTC2Get\n");
#endif
    
    tmpModeInfo = *pModeInfo;
    
    
    /*  First compute the Values */
    
    ulHTotal = tmpModeInfo.ulDispWidth +
        tmpModeInfo.ulHFPorch +
        tmpModeInfo.ulHBPorch +
        tmpModeInfo.ulHSync;
    
    ulHDispEnd = tmpModeInfo.ulDispWidth;
    ulHBlkStr  = ulHDispEnd;
    ulHBlkEnd  = ulHBlkStr + tmpModeInfo.ulHBPorch + tmpModeInfo.ulHFPorch + tmpModeInfo.ulHSync;
    ulHSyncStr = ulHBlkStr + tmpModeInfo.ulHFPorch;
    ulHSyncEnd = ulHSyncStr + tmpModeInfo.ulHSync;
    
    ulVTotal =  ulDispHeight +
        tmpModeInfo.ulVFPorch +
        tmpModeInfo.ulVBPorch +
        tmpModeInfo.ulVSync;
    
    
    ulVDispEnd = ulDispHeight;
    ulVBlkStr = ulVDispEnd;
    ulVBlkEnd = ulVBlkStr + tmpModeInfo.ulVBPorch + tmpModeInfo.ulVFPorch + tmpModeInfo.ulVSync;
    ulVSyncStr = ulVBlkStr + tmpModeInfo.ulVFPorch;
    ulVSyncEnd = ulVSyncStr + tmpModeInfo.ulVSync;
    
    ulOffset = tmpModeInfo.ulFBPitch;
    
    
    
    ulCtl2 = INREG(MGAREG_C2CTL);
    ulDataCtl2 = INREG(MGAREG_C2DATACTL);
    
    ulCtl2      &= 0xFF1FFFFF;
    ulDataCtl2  &= 0xFFFFFF00;
    
    switch (tmpModeInfo.ulBpp)
    {
    case 15:    ulCtl2      |= 0x00200000;
        ulOffset <<= 1;
        break;
    case 16:    ulCtl2      |= 0x00400000;
        ulOffset <<= 1;
        break;
    case 32:    ulCtl2      |= 0x00800000;
        ulOffset <<= 2;
        break;
    }
    
    
    pReg->crtc2[ MGAREG2_C2CTL ] = ulCtl2;
    pReg->crtc2[ MGAREG2_C2DATACTL ] = ulDataCtl2;
    
    /* Horizontal Value*/
    pReg->crtc2[MGAREG2_C2HPARAM] = (((ulHDispEnd-8) << 16) | (ulHTotal-8)) ;
    pReg->crtc2[MGAREG2_C2HSYNC] = (((ulHSyncEnd-8) << 16) | (ulHSyncStr-8)) ;  
    
    
    /*Vertical Value*/
    pReg->crtc2[MGAREG2_C2VPARAM] = (((ulVDispEnd-1) << 16) | (ulVTotal-1))  ;  
    pReg->crtc2[MGAREG2_C2VSYNC] =  (((ulVSyncEnd-1) << 16) | (ulVSyncStr-1)) ;  
    
    /** Offset value*/
    
    pReg->crtc2[MGAREG2_C2OFFSET] = ulOffset;

#ifdef DEBUG
    ErrorF("EXIT MGACRTC2Get\n");
#endif
    
}

/* Set CRTC 2*/
/* Writes to the hardware */
void MGACRTC2Set(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo)
{
    
    
    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg = &pMga->ModeReg;

#ifdef DEBUG
    ErrorF("ENTER MGACRTC2Set\n");
#endif 

    
    /* This writes to the registers manually */
    OUTREG(MGAREG_C2CTL, pReg->crtc2[MGAREG2_C2CTL]);     
    OUTREG(MGAREG_C2DATACTL,pReg->crtc2[MGAREG2_C2DATACTL]);  
    
    
    /* Horizontal Value*/
    OUTREG(MGAREG_C2HPARAM, pReg->crtc2[MGAREG2_C2HPARAM]);      
    OUTREG(MGAREG_C2HSYNC, pReg->crtc2[MGAREG2_C2HSYNC]);  
    
    
    /*Vertical Value*/
    OUTREG(MGAREG_C2VPARAM, pReg->crtc2[MGAREG2_C2VPARAM]);  
    OUTREG(MGAREG_C2VSYNC,  pReg->crtc2[MGAREG2_C2VSYNC]);  
    
    /** Offset value*/
    
    OUTREG(MGAREG_C2OFFSET, pReg->crtc2[MGAREG2_C2VSYNC]);
#ifdef DEBUG    
    ErrorF("EXIT MGACRTC2Set\n");
#endif
    
}


/* Set CRTC2 on the right output */
void MGAEnableSecondOutPut(ScrnInfoPtr pScrn, xMODEINFO *pModeInfo)
{
    CARD8   ucByte, ucXDispCtrl;
    CARD32   ulC2CTL, ulStatusReg;
    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg;
    pReg = &pMga->ModeReg;
#ifdef DEBUG
    ErrorF("ENTER MGAEnableSecondOutPut\n");
#endif


    /*  Route Video PLL on second CRTC */
    ulC2CTL = INREG( MGAREG_C2CTL);

    /*--- Disable Pixel clock oscillations On Crtc1 */
    OUTREG( MGAREG_C2CTL, ulC2CTL | C2CTL_PIXCLKDIS_MASK);
    /*--- Have to wait minimum time (2 acces will be ok) */
    ulStatusReg = INREG( MGAREG_Status);
    ulStatusReg = INREG( MGAREG_Status);
    
    
    ulC2CTL &= ~(C2CTL_PIXCLKSEL_MASK | C2CTL_PIXCLKSELH_MASK);
    
    ulC2CTL |= C2CTL_PIXCLKSEL_VIDEOPLL; 

    
    OUTREG( MGAREG_C2CTL, ulC2CTL);
    /*--- Enable Pixel clock oscillations on CRTC2*/
    OUTREG( MGAREG_C2CTL, ulC2CTL & ~C2CTL_PIXCLKDIS_MASK);
    
    
    /* We don't use MISC synch pol, must be 0*/
    ucByte = inMGAdreg( MGAREG_MISC_READ);
    
    OUTREG8(MGAREG_MISC_WRITE, (CARD8)(ucByte & ~(HSYNCPOL| VSYNCPOL) ));

    


    /* Set Rset to 0.7 V*/
    ucByte = inMGAdac(MGA1064_GEN_IO_CTL);
    ucByte &= ~0x40;
    pReg->DacRegs[MGA1064_GEN_IO_CTL] = ucByte;   
    outMGAdac (MGA1064_GEN_IO_CTL, ucByte);
    
    ucByte = inMGAdac( MGA1064_GEN_IO_DATA);
    ucByte &= ~0x40;
    pReg->DacRegs[MGA1064_GEN_IO_DATA]= ucByte;
    outMGAdac (MGA1064_GEN_IO_DATA, ucByte);
   
    /* Since G550 can swap outputs at BIOS initialisation, we must check which
     * DAC is 'logically' used as the secondary (don't assume its DAC2 anymore) */
    
    ulC2CTL = INREG(MGAREG_C2CTL);
    ucXDispCtrl = inMGAdac(MGA1064_DISP_CTL);

    ucXDispCtrl &= ~XDISPCTRL_DAC2OUTSEL_MASK;
    ucXDispCtrl |=  XDISPCTRL_DAC2OUTSEL_CRTC2;

    if (!pMga->SecondOutput) {
        /* Route Crtc2 on Output1 */
        ucXDispCtrl &= ~XDISPCTRL_DAC2OUTSEL_MASK;
        ucXDispCtrl |=  XDISPCTRL_DAC2OUTSEL_CRTC1;
        ulC2CTL     |=  C2CTL_CRTCDACSEL_CRTC2; 
    }
    else {
        /* Route Crtc2 on Output2*/
        ucXDispCtrl &= ~XDISPCTRL_DAC2OUTSEL_MASK;
        ucXDispCtrl |=  XDISPCTRL_DAC2OUTSEL_CRTC2;
        ulC2CTL     &= ~C2CTL_CRTCDACSEL_MASK;
    }
    
    /* Enable CRTC2*/
    ulC2CTL |= C2_EN_M;

    pReg->dac2[ MGA1064_DISP_CTL - 0x80] =  ucXDispCtrl; 



    OUTREG( MGAREG_C2CTL,  ulC2CTL);

   /* Set DAC2 Synch polarity*/
    ucByte = inMGAdac( MGA1064_SYNC_CTL);
    ucByte &= ~(XSYNCCTRL_DAC2HSPOL_MASK | XSYNCCTRL_DAC2VSPOL_MASK);
    if ( !(pModeInfo->flSignalMode & POS_HSYNC) )
    {
        ucByte |= XSYNCCTRL_DAC2HSPOL_NEG;
    }
    if ( !(pModeInfo->flSignalMode & POS_VSYNC) )
    {
        ucByte |= XSYNCCTRL_DAC2VSPOL_NEG;
    }

   /* Enable synch output*/
    ucByte &= ~(XSYNCCTRL_DAC2HSOFF_MASK | XSYNCCTRL_DAC2VSOFF_MASK);
    pReg->dac2[ MGA1064_SYNC_CTL - 0x80] = ucByte;

   /* Powerup DAC2*/
    ucByte = inMGAdac( MGA1064_PWR_CTL);
    pReg->dac2[ MGA1064_PWR_CTL - 0x80] = /*  0x0b; */ (ucByte | XPWRCTRL_DAC2PDN_MASK);
               


   /* Power up Fifo*/
    ucByte = inMGAdac( MGA1064_PWR_CTL);
    pReg->dac2[ MGA1064_PWR_CTL - 0x80] =   0x1b; /*   (ucByte | XPWRCTRL_CFIFOPDN_MASK) */;

    
#ifdef DEBUG
    ErrorF("EXIT MGAEnableSecondOutPut\n");
#endif
}





void MGACRTC2GetPitch (ScrnInfoPtr pScrn, xMODEINFO *pModeInfo)
{
    CARD32 ulOffset;
    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg;
    
    pReg = &pMga->ModeReg;
#ifdef DEBUG
    ErrorF("ENTER MGACRTC2GetPitch\n");
#endif

    
    switch(pModeInfo->ulBpp)
    {
        case 15:
        case 16:
            ulOffset = pModeInfo->ulFBPitch * 2;
            break;
        case 32:
            ulOffset = pModeInfo->ulFBPitch * 4;
            break;
	default:	/* Muffle compiler */
            ulOffset = pModeInfo->ulFBPitch;
	    break;
    }

    pReg->crtc2[MGAREG2_C2OFFSET] = ulOffset;
    
#ifdef DEBUG
    ErrorF("EXIT MGACRTC2GetPitch\n");
#endif

}

void MGACRTC2SetPitch (ScrnInfoPtr pScrn, xMODEINFO *pModeInfo)
{

    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg;
    pReg = &pMga->ModeReg;
    
#ifdef DEBUG
    ErrorF("ENTER CRCT2SetPitch\n");
#endif

    
    OUTREG(MGAREG_C2OFFSET,  pReg->crtc2[MGAREG2_C2OFFSET]);  
#ifdef DEBUG
    ErrorF("EXIT CRCT2SetPitch\n");
#endif
    
}


    /* Set Display Start*/
    /* base in bytes*/
void MGACRTC2GetDisplayStart (ScrnInfoPtr pScrn, xMODEINFO *pModeInfo, CARD32 base, CARD32 ulX, CARD32 ulY)
{

    CARD32 ulAddress;
   MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg;
    pReg = &pMga->ModeReg;

#ifdef DEBUG
    ErrorF("ENTER MGACRTC2GetDisplayStart\n");
#endif


    pReg = &pMga->ModeReg;

   ulAddress       = (pModeInfo->ulFBPitch * ulY + ulX);
    switch(pModeInfo->ulBpp)
    {
        case 15:
        case 16:
            ulAddress <<= 1;
            break;
        case 32:
            ulAddress <<= 2;
            break;
    }

    pReg->crtc2[MGAREG2_C2STARTADD0] = ulAddress + base;
#ifdef DEBUG
    ErrorF("EXIT MGACRTC2GetDisplayStart\n");
#endif
    
}

void MGACRTC2SetDisplayStart (ScrnInfoPtr pScrn, xMODEINFO *pModeInfo, CARD32 base, CARD32 ulX, CARD32 ulY)
{
    MGAPtr pMga = MGAPTR(pScrn);
    MGARegPtr pReg;
    pReg = &pMga->ModeReg;
#ifdef DEBUG
    ErrorF("ENTER MGACRTC2SetDisplayStart\n");
#endif
    
    OUTREG(MGAREG2_C2STARTADD0,  pReg->crtc2[MGAREG2_C2STARTADD0]);  
#ifdef DEBUG
    ErrorF("EXIT MGACRTC2GetDisplayStart\n");
#endif
    
}











