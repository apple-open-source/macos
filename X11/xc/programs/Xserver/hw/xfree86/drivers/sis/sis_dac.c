/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dac.c,v 1.30 2003/02/05 17:53:22 eich Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the provider not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The provider makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE PROVIDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE PROVIDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane <alanh@fairlite.demon.co.uk>
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>.
 *	     Thomas Winischhofer <thomas@winischhofer.net>
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86DDC.h"

#include "sis.h"
#include "sis_dac.h"
#include "sis_regs.h"
#include "sis_vb.h"

static void SiSSave(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiSRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg);

static void SiS300Save(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS310Save(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS301Save(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS301BSave(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiSLVDSChrontelSave(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS300Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS310Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS301Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS301BRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiSLVDSChrontelRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg);
static void SiS301LoadPalette(ScrnInfoPtr pScrn, int numColors,
                      int *indicies, LOCO *colors, VisualPtr pVisual);
static void SiSThreshold(ScrnInfoPtr pScrn, DisplayModePtr mode,
                      unsigned short *Low, unsigned short *High);
static void SetBlock(CARD16 port, CARD8 from, CARD8 to, CARD8 *DataPtr);
#if 0
Bool        SiSI2CInit(ScrnInfoPtr pScrn);
#endif

static const unsigned short ch700xidx[] = {
      0x00,0x07,0x08,0x0a,0x0b,0x04,0x09,0x20,0x21,0x18,0x19,0x1a,
      0x1b,0x1c,0x1d,0x1e,0x1f,  /* 0x0e,  - TW: Don't save the power register */
      0x01,0x03,0x06,0x0d,0x11,0x13,0x14,0x15,0x17,0x22,0x23,0x24
   };

static const unsigned short ch701xidx[] = {
      0x1c,0x5f,0x64,0x6f,0x70,0x71,0x72,0x73,0x74,0x76,0x78,0x7d,
      0x67,0x68,0x69,0x6a,0x6b,0x1e,0x00,0x01,0x02,0x04,0x03,0x05,
      0x06,0x07,0x08,0x15,0x1f,0x0c,0x0d,0x0e,0x0f,0x10
   };

int SiS_compute_vclk(
        int Clock,
        int *out_n,
        int *out_dn,
        int *out_div,
        int *out_sbit,
        int *out_scale)
{
    float f,x,y,t, error, min_error;
    int n, dn, best_n=0, best_dn=0;

    /*
     * Rules
     *
     * VCLK = 14.318 * (Divider/Post Scalar) * (Numerator/DeNumerator)
     * Factor = (Divider/Post Scalar)
     * Divider is 1 or 2
     * Post Scalar is 1, 2, 3, 4, 6 or 8
     * Numberator ranged from 1 to 128
     * DeNumerator ranged from 1 to 32
     * a. VCO = VCLK/Factor, suggest range is 150 to 250 Mhz
     * b. Post Scalar selected from 1, 2, 4 or 8 first.
     * c. DeNumerator selected from 2.
     *
     * According to rule a and b, the VCO ranges that can be scaled by
     * rule b are:
     *      150    - 250    (Factor = 1)
     *       75    - 125    (Factor = 2)
     *       37.5  -  62.5  (Factor = 4)
     *       18.75 -  31.25 (Factor = 8)
     *
     * The following ranges use Post Scalar 3 or 6:
     *      125    - 150    (Factor = 1.5)
     *       62.5  -  75    (Factor = 3)
     *       31.25 -  37.5  (Factor = 6)
     *
     * Steps:
     * 1. divide the Clock by 2 until the Clock is less or equal to 31.25.
     * 2. if the divided Clock is range from 18.25 to 31.25, than
     *    the Factor is 1, 2, 4 or 8.
     * 3. if the divided Clock is range from 15.625 to 18.25, than
     *    the Factor is 1.5, 3 or 6.
     * 4. select the Numberator and DeNumberator with minimum deviation.
     *
     * ** this function can select VCLK ranged from 18.75 to 250 Mhz
     */
    f = (float) Clock;
    f /= 1000.0;
    if ((f > 250.0) || (f < 18.75))
        return 0;

    min_error = f;
    y = 1.0;
    x = f;
    while (x > 31.25) {
        y *= 2.0;
        x /= 2.0;
    }
    if (x >= 18.25) {
        x *= 8.0;
        y = 8.0 / y;
    } else if (x >= 15.625) {
        x *= 12.0;
        y = 12.0 / y;
    }

    t = y;
    if (t == (float) 1.5) {
        *out_div = 2;
        t *= 2.0;
    } else {
        *out_div = 1;
    }
    if (t > (float) 4.0) {
        *out_sbit = 1;
        t /= 2.0;
    } else {
        *out_sbit = 0;
    }

    *out_scale = (int) t;

    for (dn=2;dn<=32;dn++) {
        for (n=1;n<=128;n++) {
            error = x;
            error -= ((float) 14.318 * (float) n / (float) dn);
            if (error < (float) 0)
                    error = -error;
            if (error < min_error) {
                min_error = error;
                best_n = n;
                best_dn = dn;
            }
        }
    }
    *out_n = best_n;
    *out_dn = best_dn;
    PDEBUG(ErrorF("SiS_compute_vclk: Clock=%d, n=%d, dn=%d, div=%d, sbit=%d,"
                    " scale=%d\n", Clock, best_n, best_dn, *out_div,
                    *out_sbit, *out_scale));
    return 1;
}


void
SiSCalcClock(ScrnInfoPtr pScrn, int clock, int max_VLD, unsigned int *vclk)
{
    SISPtr pSiS = SISPTR(pScrn);
    int M, N, P , PSN, VLD , PSNx ;
    int bestM=0, bestN=0, bestP=0, bestPSN=0, bestVLD=0;
    double bestError, abest = 42.0, bestFout;
    double target;
    double Fvco, Fout;
    double error, aerror;

    /*
     *  fd = fref*(Numerator/Denumerator)*(Divider/PostScaler)
     *
     *  M       = Numerator [1:128] 
     *  N       = DeNumerator [1:32]
     *  VLD     = Divider (Vco Loop Divider) : divide by 1, 2
     *  P       = Post Scaler : divide by 1, 2, 3, 4
     *  PSN     = Pre Scaler (Reference Divisor Select) 
     * 
     * result in vclk[]
     */
#define Midx    0
#define Nidx    1
#define VLDidx  2
#define Pidx    3
#define PSNidx  4
#define Fref 14318180
/* stability constraints for internal VCO -- MAX_VCO also determines 
 * the maximum Video pixel clock */
#define MIN_VCO      Fref
#define MAX_VCO      135000000
#define MAX_VCO_5597 353000000
#define MAX_PSN      0          /* no pre scaler for this chip */
#define TOLERANCE    0.01       /* search smallest M and N in this tolerance */
  
  int M_min = 2; 
  int M_max = 128;
  
/*  abest=10000.0; */
 
  target = clock * 1000;
 
  if (pSiS->Chipset == PCI_CHIP_SIS5597 || pSiS->Chipset == PCI_CHIP_SIS6326){
        int low_N = 2;
        int high_N = 5;
 
	PSN = 1;
        P = 1;
        if (target < MAX_VCO_5597 / 2)
            P = 2;
        if (target < MAX_VCO_5597 / 3)
            P = 3;
        if (target < MAX_VCO_5597 / 4)
            P = 4;
        if (target < MAX_VCO_5597 / 6)
            P = 6;
        if (target < MAX_VCO_5597 / 8)
            P = 8;
 
        Fvco = P * target;
 
        for (N = low_N; N <= high_N; N++){
            double M_desired = Fvco / Fref * N;
            if (M_desired > M_max * max_VLD)
                continue;
 
            if ( M_desired > M_max ) {
                M = M_desired / 2 + 0.5;
                VLD = 2;
            } else {
                M = Fvco / Fref * N + 0.5;
                VLD = 1;
            }
 
            Fout = (double)Fref * (M * VLD)/(N * P);
 
            error = (target - Fout) / target;
            aerror = (error < 0) ? -error : error;
/*          if (aerror < abest && abest > TOLERANCE) */
            if (aerror < abest) {
                abest = aerror;
                bestError = error;
                bestM = M;
                bestN = N;
                bestP = P;
                bestPSN = PSN;
                bestVLD = VLD;
                bestFout = Fout;
            }
        }
     }
     else {
         for (PSNx = 0; PSNx <= MAX_PSN ; PSNx++) {
            int low_N, high_N;
            double FrefVLDPSN;
 
            PSN = !PSNx ? 1 : 4;
 
            low_N = 2;
            high_N = 32;
 
            for ( VLD = 1 ; VLD <= max_VLD ; VLD++ ) {
 
                FrefVLDPSN = (double)Fref * VLD / PSN;
                for (N = low_N; N <= high_N; N++) {
                    double tmp = FrefVLDPSN / N;
 
                    for (P = 1; P <= 4; P++) {  
                        double Fvco_desired = target * ( P );
                        double M_desired = Fvco_desired / tmp;
 
                        /* Which way will M_desired be rounded?  
                         *  Do all three just to be safe.  
                         */
                        int M_low = M_desired - 1;
                        int M_hi = M_desired + 1;
 
                        if (M_hi < M_min || M_low > M_max)
                            continue;
 
                        if (M_low < M_min)
                            M_low = M_min;
                        if (M_hi > M_max)
                            M_hi = M_max;
 
                        for (M = M_low; M <= M_hi; M++) {
                            Fvco = tmp * M;
                            if (Fvco <= MIN_VCO)
                                continue;
                            if (Fvco > MAX_VCO)
                                break;
 
                            Fout = Fvco / ( P );
 
                            error = (target - Fout) / target;
                            aerror = (error < 0) ? -error : error;
                            if (aerror < abest) {
                                abest = aerror;
                                bestError = error;
                                bestM = M;
                                bestN = N;
                                bestP = P;
                                bestPSN = PSN;
                                bestVLD = VLD;
                                bestFout = Fout;
                            }
                        xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO,3,
			       "Freq. selected: %.2f MHz, M=%d, N=%d, VLD=%d, P=%d, PSN=%d\n",
                               (float)(clock / 1000.), M, N, P, VLD, PSN);
                        xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO,3,
			       "Freq. set: %.2f MHz\n", Fout / 1.0e6);
                        }
                    }
                }
            }
         }
  }

  vclk[Midx]    = bestM;
  vclk[Nidx]    = bestN;
  vclk[VLDidx]  = bestVLD;
  vclk[Pidx]    = bestP;
  vclk[PSNidx]  = bestPSN;

        PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
                "Freq. selected: %.2f MHz, M=%d, N=%d, VLD=%d, P=%d, PSN=%d\n",
                (float)(clock / 1000.), vclk[Midx], vclk[Nidx], vclk[VLDidx], 
                vclk[Pidx], vclk[PSNidx]));
        PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
                "Freq. set: %.2f MHz\n", bestFout / 1.0e6));
        PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
                "VCO Freq.: %.2f MHz\n", bestFout*bestP / 1.0e6));
}


static void
SiSSave(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i,max;

    PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
             "SiSSave(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n"));

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    switch (pSiS->Chipset) {
        case PCI_CHIP_SIS5597:
           max=0x3C;
           break;
        case PCI_CHIP_SIS6326:
        case PCI_CHIP_SIS530:
           max=0x3F; 
           break;
        default:
           max=0x37;
           break;
        }

    /* Save extended SR registers */
    for (i = 0x06; i <= max; i++) {
        inSISIDXREG(SISSR, i, sisReg->sisRegs3C4[i]);
#ifdef DEBUG
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                 "XR%02X Contents - %02X \n", i, sisReg->sisRegs3C4[i]);
#endif
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "SR%02X - %02X \n", i,sisReg->sisRegs3C4[i]);
#endif
    }

#ifdef TWDEBUG
    for (i = 0x00; i <= 0x3f; i++) {
        inSISIDXREG(SISCR, i, max);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "CR%02X - %02X \n", i,max);
    }
#endif

    /* Save lock (will not be restored in SiSRestore()!) */
    inSISIDXREG(SISCR, 0x80, sisReg->sisRegs3D4[0x80]);

    sisReg->sisRegs3C2 = inSISREG(SISMISCR);	 /* Misc */

    /* TW: Save TV registers */
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
       outSISIDXREG(SISCR, 0x80, 0x86);
       for(i = 0x00; i <= 0x44; i++) {
         sisReg->sis6326tv[i] = SiS6326GetTVReg(pScrn, i);
#ifdef TWDEBUG
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "VR%02X - %02X \n", i,sisReg->sis6326tv[i]);
#endif
       }
    }
}

static void
SiSRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i,max;
    unsigned char tmp;
#ifdef DEBUG
    int temp;
#endif

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                "SiSRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n");

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    switch (pSiS->Chipset) {
        case PCI_CHIP_SIS5597:
           max = 0x3C;
           break;
        case PCI_CHIP_SIS6326:
	case PCI_CHIP_SIS530:
           max = 0x3F;
           break;
        default:
           max = 0x37;
           break;
    }

    /* Disable TV on 6326 before restoring */
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
       outSISIDXREG(SISCR, 0x80, 0x86);
       tmp = SiS6326GetTVReg(pScrn, 0x00);
       tmp &= ~0x04;
       SiS6326SetTVReg(pScrn, 0x00, tmp);
    }

    /* Restore VCLKs */
#if 0  /* TW: No, we didn't save SR2x-1 and SR2x-2! */
    andSISIDXREG(SISSR, 0x38, 0xfc);
    inSISIDXREG(SISSR, 0x13, tmp);
    tmp &= ~0x40;
    tmp |= (sisReg->sisRegs3C4[0x13] & 0x40);
    outSISIDXREG(SISSR, 0x13, tmp);
    outSISIDXREG(SISSR, 0x2a, sisReg->sisRegs3C4[0x2a]);
    outSISIDXREG(SISSR, 0x2b, sisReg->sisRegs3C4[0x2b]);
    orSISIDXREG(SISSR, 0x38, 0x01);
    inSISIDXREG(SISSR, 0x13, tmp);
    tmp &= ~0x40;
    tmp |= (sisReg->sisRegs3C4[0x13] & 0x40);
    outSISIDXREG(SISSR, 0x13, tmp);
    outSISIDXREG(SISSR, 0x2a, sisReg->sisRegs3C4[0x2a]);
    outSISIDXREG(SISSR, 0x2b, sisReg->sisRegs3C4[0x2b]);
    andSISIDXREG(SISSR, 0x38, 0xfc);
    orSISIDXREG(SISSR, 0x38, 0x02);
    inSISIDXREG(SISSR, 0x13, tmp);
    tmp &= ~0x40;
    tmp |= (sisReg->sisRegs3C4[0x13] & 0x40);
    outSISIDXREG(SISSR, 0x13, tmp);
    outSISIDXREG(SISSR, 0x2a, sisReg->sisRegs3C4[0x2a]);
    outSISIDXREG(SISSR, 0x2b, sisReg->sisRegs3C4[0x2b]);
    andSISIDXREG(SISSR, 0x38, 0xfc);
#endif

    /* Restore other extended SR registers */
    for (i = 0x06; i <= max; i++) {
        if((i == 0x13) || (i == 0x2a) || (i == 0x2b)) continue;
#ifdef DEBUG
        inSISIDXREG(SISSR, i, temp);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                    "SR%X Contents: %02X - ", i, temp);
#endif
	outSISIDXREG(SISSR, i, sisReg->sisRegs3C4[i]);
#ifdef DEBUG
	inSISIDXREG(SISSR, i, temp);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                        "Restored to %02X - Read: %02X\n",
                        sisReg->sisRegs3C4[i], temp);
#endif
    }

    /* Now restore VCLK (with correct SR38 setting) */
    outSISIDXREG(SISSR, 0x13, sisReg->sisRegs3C4[0x13]);
    outSISIDXREG(SISSR, 0x2a, sisReg->sisRegs3C4[0x2a]);
    outSISIDXREG(SISSR, 0x2b, sisReg->sisRegs3C4[0x2b]);

    /* Misc */
    outSISREG(SISMISCW, sisReg->sisRegs3C2);

    /* MemClock needs this to take effect */
    outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
    usleep(10000);
    outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */

    /* TW: Restore TV registers */
    pSiS->SiS6326Flags &= ~SIS6326_TVON;
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
       for(i = 0x01; i <= 0x44; i++) {
         SiS6326SetTVReg(pScrn, i, sisReg->sis6326tv[i]);
#ifdef TWDEBUG
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	 	"VR%02x restored to %02x\n",
		i, sisReg->sis6326tv[i]);
#endif
       }
       tmp = SiS6326GetXXReg(pScrn, 0x13);
       SiS6326SetXXReg(pScrn, 0x13, 0xfa);
       tmp = SiS6326GetXXReg(pScrn, 0x14);
       SiS6326SetXXReg(pScrn, 0x14, 0xc8);
       if(!(sisReg->sisRegs3C4[0x0D] & 0x04)) {
    	  tmp = SiS6326GetXXReg(pScrn, 0x13);
	  SiS6326SetXXReg(pScrn, 0x13, 0xf6);
	  tmp = SiS6326GetXXReg(pScrn, 0x14);
	  SiS6326SetXXReg(pScrn, 0x14, 0xbf);
       }
       if(sisReg->sis6326tv[0] & 0x04) pSiS->SiS6326Flags |= SIS6326_TVON;
    }
}

/* Save SiS 300 series register contents */
static void
SiS300Save(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i;

    PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
     		"SiS300Save(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n"));

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Save SR registers */
    for (i = 0x00; i <= 0x3D; i++) {
        inSISIDXREG(SISSR, i, sisReg->sisRegs3C4[i]);
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "SR%02X - %02X \n", i,sisReg->sisRegs3C4[i]);
#endif
    }

    /* Save CR registers */
    for (i = 0x00; i < 0x40; i++)  {
        inSISIDXREG(SISCR, i, sisReg->sisRegs3D4[i]);
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"CR%02X Contents - %02X \n", i,sisReg->sisRegs3D4[i]);
#endif
    }

    /* Save Misc register */
    sisReg->sisRegs3C2 = inSISREG(SISMISCR);	 
    
    /* Save FQBQ and GUI timer settings */
    if(pSiS->Chipset == PCI_CHIP_SIS630) {
       sisReg->sisRegsPCI50 = pciReadLong(0x00000000, 0x50);
       sisReg->sisRegsPCIA0 = pciReadLong(0x00000000, 0xA0);
#ifdef TWDEBUG
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       		"PCI Config 50 = %lx\n", sisReg->sisRegsPCI50);
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       		"PCI Config A0 = %lx\n", sisReg->sisRegsPCIA0);		
#endif       
    }

    /* Save panel link/video bridge registers */
#ifndef TWDEBUG
    if(!pSiS->UseVESA) {
#endif
      if (pSiS->VBFlags & (VB_LVDS|VB_CHRONTEL))
        (*pSiS->SiSSaveLVDSChrontel)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301|VB_303))
        (*pSiS->SiSSave2)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX))
        (*pSiS->SiSSave3)(pScrn, sisReg);
#ifndef TWDEBUG
    }
#endif

    /* Save Mode number */
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
    if(!(pSiS->UseVESA))
#endif
        pSiS->BIOSModeSave = SiS_GetSetModeID(pScrn,0xFF);
	
#ifdef TWDEBUG	
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	"BIOS mode ds:449 = 0x%x\n", pSiS->BIOSModeSave);
#endif	
}


/* Restore SiS300 series register contents */
static void
SiS300Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i,temp;
    CARD32 temp1;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                "SiS300Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n");

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* TW: Wait for accelerator to finish on-going drawing operations. */
    inSISIDXREG(SISSR, 0x1E, temp);
    if(temp & (0x40|0x10|0x02))  {
        while ( (MMIO_IN16(pSiS->IOBase, 0x8242) & 0xE000) != 0xE000){};
	while ( (MMIO_IN16(pSiS->IOBase, 0x8242) & 0xE000) != 0xE000){};
	while ( (MMIO_IN16(pSiS->IOBase, 0x8242) & 0xE000) != 0xE000){};
    }
    
    if (!(pSiS->UseVESA)) {
       if(pSiS->VBFlags & VB_LVDS) {
          SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
          SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
       }
    }

    /* Restore extended CR registers */
    for(i = 0x19; i < 0x40; i++)  {
        outSISIDXREG(SISCR, i, sisReg->sisRegs3D4[i]);
    }

    if(pSiS->Chipset != PCI_CHIP_SIS300)  {
	unsigned char val;
	inSISIDXREG(SISCR, 0x1A, val);
	if(val == sisReg->sisRegs3D4[0x19])
	   outSISIDXREG(SISCR, 0x1A, sisReg->sisRegs3D4[0x19]);
	inSISIDXREG(SISCR,0x19,val);
	if(val == sisReg->sisRegs3D4[0x1A])
           outSISIDXREG(SISCR, 0x19, sisReg->sisRegs3D4[0x1A]);
    }

    /* Set (and leave) PCI_IO_ENABLE on if accelerators are on */
    if(sisReg->sisRegs3C4[0x1e] & 0x50) {
	sisReg->sisRegs3C4[0x20] |= 0x20;
	outSISIDXREG(SISSR, 0x20, sisReg->sisRegs3C4[0x20]);
    }

    /* TW: If TQ is switched on, don't switch it off ever again!
     *     Therefore, always restore registers with TQ enabled.
     */
    if((!pSiS->NoAccel) && (pSiS->TurboQueue)) {
        temp = (pScrn->videoRam/64) - 8;
        sisReg->sisRegs3C4[0x26] = temp & 0xFF;
	sisReg->sisRegs3C4[0x27] = ((temp >> 8) & 3) | 0xF0;
    }

    /* Restore extended SR registers */
    for (i = 0x06; i <= 0x3D; i++) {
        temp = sisReg->sisRegs3C4[i];
        if(!(pSiS->UseVESA)) {
           if(pSiS->VBFlags & VB_LVDS) {
               if(i == 0x11) {
	                inSISIDXREG(SISSR,0x11,temp);
	       		temp &= 0x0c; 
			temp |= (sisReg->sisRegs3C4[i] & 0xf3);
	       }
           }
        }
	outSISIDXREG(SISSR, i, temp);
    }
    
    /* TW: Restore VCLK and ECLK */
    if(pSiS->VBFlags & (VB_LVDS | VB_301B)) {
       outSISIDXREG(SISSR,0x31,0x20);
       outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
       outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
       outSISIDXREG(SISSR,0x2d,0x80);
       outSISIDXREG(SISSR,0x31,0x10);
       outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
       outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
       outSISIDXREG(SISSR,0x2d,0x80);
    }
    outSISIDXREG(SISSR,0x31,0x00);
    outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
    outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
    outSISIDXREG(SISSR,0x2d,0x80);
    if(pSiS->VBFlags & (VB_LVDS | VB_301B)) {
       outSISIDXREG(SISSR,0x31,0x20);
       outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
       outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
       outSISIDXREG(SISSR,0x31,0x10);
       outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
       outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
       outSISIDXREG(SISSR,0x31,0x00);
       outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
       outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
    }
    
    /* Restore Misc register */
    outSISREG(SISMISCW, sisReg->sisRegs3C2);  
    
    /* Restore FQBQ and GUI timer settings */
    if(pSiS->Chipset == PCI_CHIP_SIS630) {
       temp1 = pciReadLong(0x00000000, 0x50);
       if(pciReadLong(0x00000000, 0x00) == 0x06301039) {
          temp1 &= 0xf0ffffff;
          temp1 |= (sisReg->sisRegsPCI50 & ~0xf0ffffff);
       } else {  /* 730 */
          temp1 &= 0xfffff9ff;
          temp1 |= (sisReg->sisRegsPCI50 & ~0xfffff9ff);
       }   
       pciWriteLong(0x00000000, 0x50, temp1);
    
       temp1 = pciReadLong(0x00000000, 0xA0);
       if(pciReadLong(0x00000000, 0x00) == 0x06301039) {
          temp1 &= 0xf0ffffff;
          temp1 |= (sisReg->sisRegsPCIA0 & ~0xf0ffffff);
       } else {	/* 730 */
          temp1 &= 0x00ffffff;
          temp1 |= (sisReg->sisRegsPCIA0 & ~0x00ffffff);
       } 
       pciWriteLong(0x00000000, 0xA0, temp1);
    }

    /* Restore panel link/video bridge registers */
    if (!(pSiS->UseVESA)) {
      if (pSiS->VBFlags & (VB_LVDS|VB_CHRONTEL))
        (*pSiS->SiSRestoreLVDSChrontel)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301|VB_303))
        (*pSiS->SiSRestore2)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX))
        (*pSiS->SiSRestore3)(pScrn, sisReg);
    }
    
    /* MemClock needs this to take effect */
    outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
    outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */

    /* Restore mode number */
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
    if(!(pSiS->UseVESA))
#endif
        SiS_GetSetModeID(pScrn,pSiS->BIOSModeSave);
}

/* Save SiS310 series register contents */
static void
SiS310Save(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i;

    PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
    		"SiS310Save(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n"));

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Save SR registers */
    for (i = 0x00; i <= 0x3F; i++) {
        inSISIDXREG(SISSR, i, sisReg->sisRegs3C4[i]);
#ifdef DEBUG
        xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		       "SR%02X - %02X \n", i,sisReg->sisRegs3C4[i]);
#endif
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			 "SR%02X - %02X \n", i,sisReg->sisRegs3C4[i]);
#endif
    }

    /* TW: Save command queue location */
    sisReg->sisMMIO85C0 = MMIO_IN32(pSiS->IOBase, 0x85C0);

    /* Save CR registers */
    for (i = 0x00; i <= 0x5f; i++)  {
        inSISIDXREG(SISCR, i, sisReg->sisRegs3D4[i]);
#ifdef DEBUG
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
		       "CR%02X - %02X \n", i,sisReg->sisRegs3D4[i]);
#endif
#ifdef TWDEBUG
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"CR%02X Contents - %02X \n", i,sisReg->sisRegs3D4[i]);
#endif
    }

    /* Save Misc register */
    sisReg->sisRegs3C2 = inSISREG(SISMISCR);   

    /* Save panel link/video bridge registers */
#ifndef TWDEBUG
    if (!pSiS->UseVESA) {
#endif
      if (pSiS->VBFlags & (VB_LVDS|VB_CHRONTEL))
        (*pSiS->SiSSaveLVDSChrontel)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301|VB_303))
        (*pSiS->SiSSave2)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX))
        (*pSiS->SiSSave3)(pScrn, sisReg);
#ifndef TWDEBUG
    }
#endif

    /* Save mode number */
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
    if(!(pSiS->UseVESA))
#endif
        pSiS->BIOSModeSave = SiS_GetSetModeID(pScrn,0xFF);
}

/* Restore SiS310 series register contents */
static void
SiS310Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr pSiS = SISPTR(pScrn);
    int i,temp;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                "SiS310Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg)\n");

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* TW: Wait for accelerator to finish on-going drawing operations. */
    inSISIDXREG(SISSR, 0x1E, temp);
    if (temp & (0x40|0x10|0x02))  {	/* TW: 0x40 = 2D, 0x10 = 3D enabled*/
        while ( (MMIO_IN32(pSiS->IOBase, 0x85CC) & 0x80000000) != 0x80000000){};
	while ( (MMIO_IN32(pSiS->IOBase, 0x85CC) & 0x80000000) != 0x80000000){};
	while ( (MMIO_IN32(pSiS->IOBase, 0x85CC) & 0x80000000) != 0x80000000){};
    }

    /* Restore extended CR registers */
    for (i = 0x19; i < 0x5C; i++)  {
        outSISIDXREG(SISCR, i, sisReg->sisRegs3D4[i]);
    }

    /* TW: Leave PCI_IO_ENABLE on if accelerators are on (Is this required?) */
    if (sisReg->sisRegs3C4[0x1e] & 0x50) {  /*0x40=2D, 0x10=3D*/
	sisReg->sisRegs3C4[0x20] |= 0x20;
	outSISIDXREG(SISSR, 0x20, sisReg->sisRegs3C4[0x20]);
    }

    /* TW: We reset the command queue before restoring.
     * This might be required because we never know what
     * console driver (like the kernel framebuffer driver)
     * or application is running and which queue mode it
     * uses.
     */
    outSISIDXREG(SISSR, 0x27, 0x1F);
    outSISIDXREG(SISSR, 0x26, 0x01);

    /* Restore extended SR registers */
    for (i = 0x06; i <= 0x3F; i++) {
#ifdef DEBUG
        inSISIDXREG(SISSR, i, temp);
#endif
	outSISIDXREG(SISSR, i, sisReg->sisRegs3C4[i]);
#ifdef DEBUG
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
                    "XR%X Contents %02X - ", i, temp);
	inSISIDXREG(SISSR, i, temp);
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
        	    "Restored to %02X - Read: %02X\n",
                    sisReg->sisRegs3C4[i], temp);
#endif
    }
    /* TW: Restore VCLK and ECLK */
    andSISIDXREG(SISSR,0x31,0xcf);
    if(pSiS->VBFlags & VB_LVDS) {
        orSISIDXREG(SISSR,0x31,0x20);
	outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
        outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
	outSISIDXREG(SISSR,0x2d,0x80);
	andSISIDXREG(SISSR,0x31,0xcf);
        orSISIDXREG(SISSR,0x31,0x10);
	outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
        outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
	outSISIDXREG(SISSR,0x2d,0x80);
	andSISIDXREG(SISSR,0x31,0xcf);
	outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
        outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
	outSISIDXREG(SISSR,0x2d,0x01);
	outSISIDXREG(SISSR,0x31,0x20);
	outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
        outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
	outSISIDXREG(SISSR,0x31,0x10);
	outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
        outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
	outSISIDXREG(SISSR,0x31,0x00);
	outSISIDXREG(SISSR,0x2e,sisReg->sisRegs3C4[0x2e]);
        outSISIDXREG(SISSR,0x2f,sisReg->sisRegs3C4[0x2f]);
    } else {
        outSISIDXREG(SISSR,0x2b,sisReg->sisRegs3C4[0x2b]);
        outSISIDXREG(SISSR,0x2c,sisReg->sisRegs3C4[0x2c]);
        outSISIDXREG(SISSR,0x2d,0x01);
    }

    /* TW: Initialize read/write pointer for command queue */
    MMIO_OUT32(pSiS->IOBase, 0x85C4, MMIO_IN32(pSiS->IOBase, 0x85C8));
    /* TW: Restore queue location */
    MMIO_OUT32(pSiS->IOBase, 0x85C0, sisReg->sisMMIO85C0);

    /* Restore Misc register */
    outSISREG(SISMISCW, sisReg->sisRegs3C2);   

    /* Restore panel link/video bridge registers */
    if (!(pSiS->UseVESA)) {
      if (pSiS->VBFlags & (VB_LVDS|VB_CHRONTEL))
        (*pSiS->SiSRestoreLVDSChrontel)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301|VB_303))
        (*pSiS->SiSRestore2)(pScrn, sisReg);
      if (pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX))
        (*pSiS->SiSRestore3)(pScrn, sisReg);
    }

    /* MemClock needs this to take effect */
    outSISIDXREG(SISSR, 0x00, 0x01);    /* Synchronous Reset */
    outSISIDXREG(SISSR, 0x00, 0x03);    /* End Reset */

    /* Restore Mode number */
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(4,2,99,0,0)
    if(!(pSiS->UseVESA))
#endif
       SiS_GetSetModeID(pScrn,pSiS->BIOSModeSave);
}

/* Save SiS301 bridge register contents */
static void
SiS301Save(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     i;
    int     Part1max=0, Part2max=0, Part3max=0, Part4max=0;

    /* Highest register number to save/restore */
    switch (pSiS->VGAEngine) {
    case SIS_300_VGA:
         Part1max = 0x1d;
	 Part2max = 0x45;
	 Part3max = 0x3e;
	 Part4max = 0x1b;
	 break;
    case SIS_315_VGA:
         Part1max = 0x2e;  /* 0x23, but we also need 2d-2e */
	 Part2max = 0x45;
	 Part3max = 0x3e;
	 Part4max = 0x1b;
	 break;
    }

    for (i=0; i<=Part1max; i++)  {
        inSISIDXREG(SISPART1, i, sisReg->VBPart1[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301Save: Part1Port 0x%02x = 0x%02x\n", i, sisReg->VBPart1[i]);
#endif
    }
    for (i=0; i<=Part2max; i++)  {
        inSISIDXREG(SISPART2, i, sisReg->VBPart2[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301Save: Part2Port 0x%02x = 0x%02x\n", i, sisReg->VBPart2[i]);
#endif
    }
    for (i=0; i<=Part3max; i++)  {
        inSISIDXREG(SISPART3, i, sisReg->VBPart3[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301Save: Part3Port 0x%02x = 0x%02x\n", i, sisReg->VBPart3[i]);
#endif
    }
    for (i=0; i<=Part4max; i++)  {
        inSISIDXREG(SISPART4, i, sisReg->VBPart4[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301Save: Part4Port 0x%02x = 0x%02x\n", i, sisReg->VBPart4[i]);
#endif
    }

    sisReg->VBPart2[0] &= ~0x20;         /* Disable VB Processor */
    sisReg->sisRegs3C4[0x32] &= ~0x20;   /* Disable Lock Mode */
}

/* Restore SiS301 bridge register contents */
static void
SiS301Restore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     Part1max=0, Part2max=0, Part3max=0, Part4max=0;

    /* Highest register number to save/restore */
    switch (pSiS->VGAEngine) {
    case SIS_300_VGA:
         Part1max = 0x1d;
	 Part2max = 0x45;
	 Part3max = 0x3e;
	 Part4max = 0x1b;
	 break;
    case SIS_315_VGA:
         Part1max = 0x23;
	 Part2max = 0x45;
	 Part3max = 0x3e;
	 Part4max = 0x1b;
	 break;
    }

    SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);

    SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);

    /* Pre-restore Part1 */
    outSISIDXREG(SISPART1, 0x04, 0x00);
    outSISIDXREG(SISPART1, 0x05, 0x00);
    outSISIDXREG(SISPART1, 0x06, 0x00);
    outSISIDXREG(SISPART1, 0x00, sisReg->VBPart1[0]);
    outSISIDXREG(SISPART1, 0x01, sisReg->VBPart1[1]);

    /* Pre-restore Part4 */
    outSISIDXREG(SISPART4, 0x0D, sisReg->VBPart4[0x0D]);
    outSISIDXREG(SISPART4, 0x0C, sisReg->VBPart4[0x0C]);

    if (!(sisReg->sisRegs3D4[0x30] & 0x03) &&
         (sisReg->sisRegs3D4[0x31] & 0x20))  {      /* disable CRT2 */
            SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
            return;
    }

    /* Restore Part1 */
    SetBlock(SISPART1, 0x02, Part1max, &(sisReg->VBPart1[0x02]));
    switch (pSiS->VGAEngine) {
      case SIS_300_VGA:
        /* TW: Nothing special here. */
      	break;
      case SIS_315_VGA:
        /* TW: Restore extra registers on 310 series */
	SetBlock(SISPART1, 0x2C, 0x2E, &(sisReg->VBPart1[0x2C]));
      	break;
    }

    /* Restore Part2 */
    SetBlock(SISPART2, 0x00, Part2max, &(sisReg->VBPart2[0x00]));

    /* Restore Part3 */
    SetBlock(SISPART3, 0x00, Part3max, &(sisReg->VBPart3[0x00]));

    /* Restore Part4 */
    SetBlock(SISPART4, 0x0E, 0x11, &(sisReg->VBPart4[0x0E]));
    SetBlock(SISPART4, 0x13, Part4max, &(sisReg->VBPart4[0x13]));

    /* Post-restore Part4 (CRT2VCLK) */
    outSISIDXREG(SISPART4, 0x0A, 0x01);
    outSISIDXREG(SISPART4, 0x0B, sisReg->VBPart4[0x0B]);
    outSISIDXREG(SISPART4, 0x0A, sisReg->VBPart4[0x0A]);
    outSISIDXREG(SISPART4, 0x12, 0x00);
    outSISIDXREG(SISPART4, 0x12, sisReg->VBPart4[0x12]);

    SiS_EnableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
    SiS_DisplayOn(pSiS->SiS_Pr);
    SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
}

/* Save SiS301B/302B/30xLV bridge register contents */
static void
SiS301BSave(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     i;
    int     Part1max=0, Part2max=0, Part3max=0, Part4max=0;

    switch (pSiS->VGAEngine) {
    case SIS_300_VGA:
         Part1max = 0x37; /* 0x1d, but we also need 2c-2e, 35-37 */
	 Part2max = 0x4d;
	 Part3max = 0x3e;
	 if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX))
	   Part4max = 0x24;
	 else
	   Part4max = 0x23;
	 break;
    case SIS_315_VGA:
         Part1max = 0x37; /* 0x23, but we also need 2c-2e, 35-37 */
	 Part2max = 0x4d;
	 Part3max = 0x3e;
	 if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX))
	   Part4max = 0x24;
	 else
	   Part4max = 0x23;
	 break;
    }

    for (i=0; i<=Part1max; i++)  {
        inSISIDXREG(SISPART1, i, sisReg->VBPart1[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301BSave: Part1Port 0x%02x = 0x%02x\n", i, sisReg->VBPart1[i]);
#endif
    }
    for (i=0; i<=Part2max; i++)  {
        inSISIDXREG(SISPART2, i, sisReg->VBPart2[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301BSave: Part2Port 0x%02x = 0x%02x\n", i, sisReg->VBPart2[i]);
#endif
    }
    for (i=0; i<=Part3max; i++)  {
        inSISIDXREG(SISPART3, i, sisReg->VBPart3[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301BSave: Part3Port 0x%02x = 0x%02x\n", i, sisReg->VBPart3[i]);
#endif
    }
    for (i=0; i<=Part4max; i++)  {
        inSISIDXREG(SISPART4, i, sisReg->VBPart4[i]);
#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "301BSave: Part4Port 0x%02x = 0x%02x\n", i, sisReg->VBPart4[i]);
#endif
    }
    sisReg->VBPart2[0] &= ~0x20;         /* Disable VB Processor */
    sisReg->sisRegs3C4[0x32] &= ~0x20;   /* Disable Lock Mode */
}

/* Restore SiS301B/302B/301LV/302LV bridge register contents */
static void
SiS301BRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     Part1max=0, Part2max=0, Part3max=0, Part4max=0;

    switch (pSiS->VGAEngine) {
    case SIS_300_VGA:
         Part1max = 0x23;
	 Part2max = 0x4d;
	 Part3max = 0x3e;
	 if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX))
	   Part4max = 0x24;
	 else
	   Part4max = 0x22;
	 break;
    case SIS_315_VGA:
         Part1max = 0x23;
	 Part2max = 0x4d;
	 Part3max = 0x3e;
	 if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX))
	   Part4max = 0x24;
	 else
	   Part4max = 0x22;
	 break;
    }

    SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);

    SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);

    /* Pre-restore Part1 */
    outSISIDXREG(SISPART1, 0x04, 0x00);
    outSISIDXREG(SISPART1, 0x05, 0x00);
    outSISIDXREG(SISPART1, 0x06, 0x00);
    outSISIDXREG(SISPART1, 0x00, sisReg->VBPart1[0x00]);
    outSISIDXREG(SISPART1, 0x01, sisReg->VBPart1[0x01]);
    /* Mode reg 0x01 became 0x2e on 310 series (0x01 still contains FIFO) */
    if(pSiS->VGAEngine == SIS_315_VGA)
        outSISIDXREG(SISPART1, 0x2e, sisReg->VBPart1[0x2e]);

    /* Pre-restore Part4 */
    outSISIDXREG(SISPART4, 0x0D, sisReg->VBPart4[0x0D]);
    outSISIDXREG(SISPART4, 0x0C, sisReg->VBPart4[0x0C]);

    if (!(sisReg->sisRegs3D4[0x30] & 0x03) &&
         (sisReg->sisRegs3D4[0x31] & 0x20))  {      /* disable CRT2 */
            SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
            return;
    }

    /* Restore Part1  */
    SetBlock(SISPART1, 0x02, Part1max, &(sisReg->VBPart1[0x02]));
    if(pSiS->VGAEngine == SIS_315_VGA) {
       SetBlock(SISPART1, 0x2C, 0x2D, &(sisReg->VBPart1[0x2C]));
       SetBlock(SISPART1, 0x35, 0x37, &(sisReg->VBPart1[0x35]));
    }

    /* Restore Part2 */
    SetBlock(SISPART2, 0x00, Part2max, &(sisReg->VBPart2[0x00]));

    /* Restore Part3 */
    SetBlock(SISPART3, 0x00, Part3max, &(sisReg->VBPart3[0x00]));

    /* Restore Part4 */
    SetBlock(SISPART4, 0x0E, 0x11, &(sisReg->VBPart4[0x0E]));
    SetBlock(SISPART4, 0x13, Part4max, &(sisReg->VBPart4[0x13]));

    /* Post-restore Part4 (CRT2VCLK) */
    outSISIDXREG(SISPART4, 0x0A, sisReg->VBPart4[0x0A]);
    outSISIDXREG(SISPART4, 0x0B, sisReg->VBPart4[0x0B]);
    outSISIDXREG(SISPART4, 0x12, 0x00);
    outSISIDXREG(SISPART4, 0x12, sisReg->VBPart4[0x12]);

    SiS_EnableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
    SiS_DisplayOn(pSiS->SiS_Pr);
    SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
}

/* Save LVDS bridge (+ Chrontel) register contents */
static void
SiSLVDSChrontelSave(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     i;

    /* Save Part1 */
    for (i=0; i<0x46; i++)  {
        inSISIDXREG(SISPART1, i, sisReg->VBPart1[i]);
#ifdef TWDEBUG
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	                "LVDSSave: Part1Port 0x%02x = 0x%02x\n",
			i, sisReg->VBPart1[i]);
#endif
    }

    /* Save Chrontel registers */
    if (pSiS->VBFlags & VB_CHRONTEL) {
       if (pSiS->ChrontelType == CHRONTEL_700x) {
          for (i=0; i<0x1D; i++)  {
              sisReg->ch70xx[i] = SiS_GetCH700x(pSiS->SiS_Pr, ch700xidx[i]);
#ifdef TWDEBUG
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	                "LVDSSave: Chrontel 0x%02x = 0x%02x\n",
			ch700xidx[i], sisReg->ch70xx[i]);
#endif

	  }
       } else {
          for (i=0; i<34; i++)  {
              sisReg->ch70xx[i] = SiS_GetCH701x(pSiS->SiS_Pr, ch701xidx[i]);
          }
       }
    }

    sisReg->sisRegs3C4[0x32] &= ~0x20;      /* Disable Lock Mode */
}

/* Restore LVDS bridge (+ Chrontel) register contents */
static void
SiSLVDSChrontelRestore(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int i;
    USHORT wtemp;

    SiS_DisableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
    if(pSiS->sishw_ext.jChipType == SIS_730) {
        outSISIDXREG(SISPART1, 0x00, 0x80);
    }

    SiS_UnLockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);

    if(pSiS->VBFlags & VB_CHRONTEL) {
       /* Restore Chrontel registers */
       if(pSiS->ChrontelType == CHRONTEL_700x) {
          for(i=0; i<0x11; i++) {
            wtemp = ((sisReg->ch70xx[i]) << 8) | (ch700xidx[i] & 0x00FF);
            SiS_SetCH700x(pSiS->SiS_Pr, wtemp);
          }
       } else {
          for(i=0; i<34; i++) {
            wtemp = ((sisReg->ch70xx[i]) << 8) | (ch701xidx[i] & 0x00FF);
            SiS_SetCH701x(pSiS->SiS_Pr, wtemp);
          }
       }
    }

    /* pre-restore Part1 */
    outSISIDXREG(SISPART1, 0x04, 0x00);
    outSISIDXREG(SISPART1, 0x05, 0x00);
    outSISIDXREG(SISPART1, 0x06, 0x00);
    outSISIDXREG(SISPART1, 0x00, sisReg->VBPart1[0]);
    if(pSiS->VGAEngine == SIS_300_VGA) {    
       outSISIDXREG(SISPART1, 0x01, (sisReg->VBPart1[1] | 0x80));
    } else {
       outSISIDXREG(SISPART1, 0x01, sisReg->VBPart1[1]);
    }

    if (!(sisReg->sisRegs3D4[0x30] & 0x03) &&
         (sisReg->sisRegs3D4[0x31] & 0x20))  {      /* disable CRT2 */
            SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
            return;
    }

    /* Restore Part1 */
    if(pSiS->VGAEngine == SIS_300_VGA) {    
       outSISIDXREG(SISPART1, 0x02, (sisReg->VBPart1[2] | 0x40));
    } else {
       outSISIDXREG(SISPART1, 0x02, sisReg->VBPart1[2]);
    }
    SetBlock(SISPART1, 0x03, 0x23, &(sisReg->VBPart1[0x03]));
    if(pSiS->VGAEngine == SIS_315_VGA) {
         SetBlock(SISPART1, 0x2C, 0x2E, &(sisReg->VBPart1[0x2C]));
	 SetBlock(SISPART1, 0x35, 0x37, &(sisReg->VBPart1[0x35]));  /* Panel Link Scaler */
    }

    /* TW: For 550 DSTN registers */
    if (pSiS->DSTN) {
        SetBlock(SISPART1, 0x25, 0x2E, &(sisReg->VBPart1[0x25]));
	SetBlock(SISPART1, 0x30, 0x45, &(sisReg->VBPart1[0x30]));
    }

    SiS_EnableBridge(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
    SiS_DisplayOn(pSiS->SiS_Pr);
    SiS_LockCRT2(pSiS->SiS_Pr, &pSiS->sishw_ext, pSiS->RelIO+0x30);
}

/* TW: Restore output selection registers (CR30, 31, 33, 35/38) */
void
SiSRestoreBridge(ScrnInfoPtr pScrn, SISRegPtr sisReg)
{
   SISPtr pSiS = SISPTR(pScrn);
   unsigned char temp = 0;

#ifdef UNLOCK_ALWAYS
   sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

   outSISIDXREG(SISCR, 0x30, sisReg->sisRegs3D4[0x30]);
   outSISIDXREG(SISCR, 0x31, sisReg->sisRegs3D4[0x31]);
   outSISIDXREG(SISCR, 0x33, sisReg->sisRegs3D4[0x33]);
   if(pSiS->Chipset != PCI_CHIP_SIS300) {
      switch(pSiS->VGAEngine) {
        case SIS_300_VGA: temp = 0x35; break;
        case SIS_315_VGA: temp = 0x38; break;
      }
      if(temp) {
         outSISIDXREG(SISCR, temp, sisReg->sisRegs3D4[temp]);
      }
   }
}

unsigned int
SiSddc1Read(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char temp;

#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif

    /* Wait until vertical retrace is in progress. */
    while(inSISREG(SISINPSTAT) & 0x08);
    while(!(inSISREG(SISINPSTAT) & 0x08));

    /* Get the result */
    inSISIDXREG(SISSR, 0x11, temp);

    return((temp & 0x02)>>1);
}

#if 0  /* TW: I2C functions not in use */
/*
static void
SiS_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
  SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
  unsigned char val;

  outSISIDXREG(SISSR, 0x05, 0x86);
  inSISIDXREG(SISSR, pSiS->SiS_DDC2_Index, val);
  *clock = (val & pSiS->SiS_DDC2_Clk) != 0;
  *data  = (val & pSiS->SiS_DDC2_Data) != 0;
}

static void
SiS_I2CPutBits(I2CBusPtr b, int clock, int data)
{
  SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
  unsigned char temp;

  outSISIDXREG(SISSR, 0x05, 0x86);
  inSISIDXREG(SISSR, pSiS->SiS_DDC2_Index, temp);

  temp &= ~(pSiS->SiS_DDC2_Clk | pSiS->SiS_DDC2_Data);

  temp |= ((clock ? pSiS->SiS_DDC2_Clk : 0) | (data ? pSiS->SiS_DDC2_Data : 0));

  outSISIDXREG(SISSR, pSiS->SiS_DDC2_Index, temp);
}
*/

static Bool
SiS_I2CAddress(I2CDevPtr d, I2CSlaveAddr addr)
{
   I2CBusPtr b = d->pI2CBus;
   SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
   SiS_SetSwitchDDC2(pSiS->SiS_Pr);
   return(SiS_I2C_Address(pSiS->SiS_Pr, addr));
}

static void
SiS_I2CStop(I2CDevPtr d)
{
  I2CBusPtr b = d->pI2CBus;
  SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
  SiS_I2C_Stop(pSiS->SiS_Pr);
}

static Bool
SiS_I2CGetByte(I2CDevPtr d, I2CByte *data, Bool last)
{
  I2CBusPtr b = d->pI2CBus;
  SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
  USHORT temp = SiS_I2C_GetByte(pSiS->SiS_Pr);
  if(temp == 0xffff) return FALSE;
  return TRUE;
}

static Bool
SiS_I2CPutByte(I2CDevPtr d, I2CByte data)
{
  I2CBusPtr b = d->pI2CBus;
  SISPtr pSiS = SISPTR(xf86Screens[b->scrnIndex]);
  return(SiS_I2C_PutByte(pSiS->SiS_Pr, (USHORT)data));
}

Bool
SiSI2CInit(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    I2CBusPtr I2CPtr;
    USHORT temp, i;
    unsigned char buffer[256];
    xf86MonPtr    pMonitor;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pSiS->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
/*
    I2CPtr->I2CPutBits = SiS_I2CPutBits;
    I2CPtr->I2CGetBits = SiS_I2CGetBits;
*/
    I2CPtr->I2CPutByte = SiS_I2CPutByte;
    I2CPtr->I2CGetByte = SiS_I2CGetByte;
    I2CPtr->I2CAddress = SiS_I2CAddress;
    I2CPtr->I2CStop    = SiS_I2CStop;
    I2CPtr->AcknTimeout = 30;



    pSiS->SiS_Pr->SiS_DDC_Index = pSiS->SiS_DDC2_Index;
    pSiS->SiS_Pr->SiS_DDC_Data  = pSiS->SiS_DDC2_Data;
    pSiS->SiS_Pr->SiS_DDC_Clk   = pSiS->SiS_DDC2_Clk;
    pSiS->SiS_Pr->SiS_DDC_DataShift = 0x00;

    if (!xf86I2CBusInit(I2CPtr)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    "Could not create I2C bus record\n");
	return FALSE;
    }

    return TRUE;
}
#endif

#if 0  /* TW: The following function should take a threshold value
        *     from predefined tables. This is only needed on some
	*     530 boards, which have an ESS sound device on-board.
	*     However, I don't know how to calculate the index to
	*     be submitted to this function.
	*/
unsigned short
SiS_CalcSpecial530Threshold(ScrnInfoPtr pScrn, DisplayModePtr mode, int index)
{
    SISPtr  pSiS = SISPTR(pScrn);
    static const unsigned char t640x480[3][24] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,A9,   /* b4 - 9d - depth 8 */
	  0, 0,11,14,14, 0, 0, 0, 0, 0, 0,9D },
	{ 0, 0, 0, 0, 0,12,15, 0, 0, 0,92,91,   /* 9c - 85 - depth 16 */
	  0,31,31,31,31, 0, 0, 0, 0, 0, 0,85 },
	{ 0, 0, 0, 0, 0,17,22,25, 0, 0, 0,79,   /* 84 - ?  - depth 32 */
	  0,31,31, 0, 0, 0, 0, 0, 0, 0, 0,6d }
    }
    static const unsigned char t800x600[3][24] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,61,
	  0,18,25,30,27,31,31,31, 0, 0, 0, 0 },
	{55, 0, 0, 0, 0, 9,10,15,18,19, 0, 0,
	... to be continued

    depthindex = (pSiS->CurrentLayout.bitsPerPixel + 1) >> 3;
    if(depthindex == 3) return(0);
    if(depthindex == 4) depthindex--;
    depthindex--;

    switch(mode->HDisplay) {
    case 640:
       if(mode->VDisplay == 480) {
          return(t640x480[depthindex][index];
       } else return(0);
    case 800:
       if(mode->VDisplay == 600) {
          return(t800x600[depthindex][index];
       } else return(0);
    case 1024:
       if(mode->VDisplay == 768) {
          return(t1024x768[depthindex][index];
       } else return(0);
    case 1280:
       if(mode->VDisplay == 1024) {
          return(t1280x1024[depthindex][index];
       } else return(0);
    case 1600:
       if(mode->VDisplay == 1200) {
          return(t1600x1200[depthindex][index];
       } else return(0);
    default: return(0);
    }
}
#endif

/* TW: Stub */
static void
SiSThreshold(ScrnInfoPtr pScrn, DisplayModePtr mode,
             unsigned short *Low, unsigned short *High)
{
    return;
}


/* Auxiliary function to find real memory clock (in Khz) */
/* TW: Not for 530/620 if UMA (on these, the mclk is stored in SR10) */
int
SiSMclk(SISPtr pSiS)
{ 
    int mclk;
    unsigned char Num, Denum, Base;

    switch (pSiS->Chipset)  {
    case PCI_CHIP_SIS5597:
    case PCI_CHIP_SIS6326:
    case PCI_CHIP_SIS530:
        /* Numerator */
        inSISIDXREG(SISSR, MemClock0, Num);
        mclk = 14318 * ((Num & 0x7f) + 1);

        /* Denumerator */
	inSISIDXREG(SISSR, MemClock1, Denum);
        mclk = mclk / ((Denum & 0x1f) + 1);

        /* Divider. Doesn't seem to work for mclk in older cards */
        if((Num & 0x80) != 0)  mclk *= 2;

        /* Post-scaler. Values' meaning depends on SR13 bit 7  */
	inSISIDXREG(SISSR, ClockBase, Base);
        if((Base & 0x80) == 0) {
            mclk = mclk / (((Denum & 0x60) >> 5) + 1);
        } else {
            /* Values 00 and 01 are reserved */
            if ((Denum & 0x60) == 0x40)  mclk /= 6;
            if ((Denum & 0x60) == 0x60)  mclk /= 8;
        }
        break;
    case PCI_CHIP_SIS300:
    case PCI_CHIP_SIS540:
    case PCI_CHIP_SIS630:
    case PCI_CHIP_SIS550:
    case PCI_CHIP_SIS650:
    case PCI_CHIP_SIS315:
    case PCI_CHIP_SIS315H:
    case PCI_CHIP_SIS315PRO:
    case PCI_CHIP_SIS330:
        /* Numerator */
	inSISIDXREG(SISSR, 0x28, Num);
        mclk = 14318 * ((Num & 0x7f) + 1);

        /* Denumerator */
	inSISIDXREG(SISSR, 0x29, Denum);
        mclk = mclk / ((Denum & 0x1f) + 1);

        /* Divider */
        if((Num & 0x80) != 0)  mclk *= 2;

        /* Post-Scaler */
        if((Denum & 0x80) == 0) {
            mclk = mclk / (((Denum & 0x60) >> 5) + 1);
        } else {
            mclk = mclk / ((((Denum & 0x60) >> 5) + 1) * 2);
        }
        break;
    default:
    	xf86DrvMsg(pSiS->pScrn->scrnIndex, X_ERROR,
		"Internal error: SiSMClk() called with invalid chipset (0x%x)\n",
		pSiS->Chipset);
        mclk = 0;
    }

    return(mclk);
}

/* TW: This estimates the CRT2 clock we are going to use.
 *     The total bandwidth is to be reduced by the value
 *     returned here in order to get an idea of the maximum
 *     dotclock left for CRT1.
 *     Since we don't know yet, what mode the user chose,
 *     we return the maximum dotclock used by
 *     - either the LCD attached, or
 *     - TV
 *     For VGA2, we share the bandwith equally.
 */
static int
SiSEstimateCRT2Clock(ScrnInfoPtr pScrn)
{
        SISPtr pSiS = SISPTR(pScrn);

	if(pSiS->VBFlags & CRT2_LCD) {
  	       if(pSiS->VBLCDFlags & (VB_LCD_320x480 | VB_LCD_800x600 | VB_LCD_640x480))
	           return 40000;
	       else if(pSiS->VBLCDFlags & (VB_LCD_1024x768 | VB_LCD_1024x600))
	           return 65000;
	       else if(pSiS->VBLCDFlags & (VB_LCD_1152x768 | VB_LCD_1280x1024 | VB_LCD_1280x960))
		   return 108000;
	       else if(pSiS->VBLCDFlags & VB_LCD_1400x1050)
		   return 122000;
	       else if(pSiS->VBLCDFlags & VB_LCD_1600x1200)
		   return 162000;
	       else
	           return 108000;
	} else if(pSiS->VBFlags & CRT2_TV) {
	    if(pSiS->VBFlags & VB_CHRONTEL) {
	        switch(pSiS->VGAEngine) {
		case SIS_300_VGA:
                   return 50000;
		case SIS_315_VGA:
		default:
		   return 70000;
		}
	    } else if(pSiS->VBFlags & VB_SISBRIDGE) {
	        return 70000;
	    }
	}

	return 0;
}

/* Calculate the maximum dotclock */
int SiSMemBandWidth(ScrnInfoPtr pScrn)
{
        SISPtr pSiS = SISPTR(pScrn);
#ifdef SISDUALHEAD
        SISEntPtr pSiSEnt = pSiS->entityPrivate;
#endif

        int             bus = pSiS->BusWidth;
        int             mclk = pSiS->MemClock;
        int             bpp = pSiS->CurrentLayout.bitsPerPixel;
	int             bytesperpixel = (bpp + 7) / 8;
        float   	magic=0.0, total, crt2used;
	int		crt2clock, max=0;
        const float     magic300[4] = { 1.2,      1.368421, 2.263158, 1.2};
        const float     magic630[4] = { 1.441177, 1.441177, 2.588235, 1.441177 };
	const float     magic315[4] = { 1.2,      1.368421, 1.368421, 1.2 };
	const float     magic550[4] = { 1.441177, 1.441177, 2.588235, 1.441177 };

        switch (pSiS->Chipset) {

        case PCI_CHIP_SIS5597:
	        total = ((mclk * (bus / 8)) * 0.7) / bytesperpixel;
		if(total > 135000) total = 135000;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Maximum pixel clock at %d bpp is %g MHz\n",
			bpp, total/1000);
                return(int)(total);

        case PCI_CHIP_SIS6326:
		total = ((mclk * (bus / 8)) * 0.7) / bytesperpixel;
		if(total > 175500) total = 175500;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Maximum pixel clock at %d bpp is %g MHz\n",
			bpp, total/1000);
		return(int)(total);

        case PCI_CHIP_SIS530:
	        total = ((mclk * (bus / 8)) * 0.7) / bytesperpixel;
		if(total > 230000) total = 230000;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Maximum pixel clock at %d bpp is %g MHz\n",
			bpp, total/1000);
		return(int)(total);

        case PCI_CHIP_SIS300:
        case PCI_CHIP_SIS540:
        case PCI_CHIP_SIS630:
	case PCI_CHIP_SIS315:
	case PCI_CHIP_SIS315H:
	case PCI_CHIP_SIS315PRO:
	case PCI_CHIP_SIS550:
	case PCI_CHIP_SIS650:
	case PCI_CHIP_SIS330:
	        switch(pSiS->Chipset) {
        	case PCI_CHIP_SIS300:
	            magic = magic300[bus/64];
		    max = 540000;
                    break;
        	case PCI_CHIP_SIS540:
       	 	case PCI_CHIP_SIS630:
		    magic = magic630[bus/64];
		    max = 540000;
                    break;
		case PCI_CHIP_SIS315:
		case PCI_CHIP_SIS315H:
		case PCI_CHIP_SIS315PRO:
		case PCI_CHIP_SIS330:
		    magic = magic315[bus/64];
		    max = 780000;
		    break;
		case PCI_CHIP_SIS550:
		case PCI_CHIP_SIS650:
		    magic = magic550[bus/64];
		    max = 780000;
                }

                PDEBUG(ErrorF("mclk: %d, bus: %d, magic: %g, bpp: %d\n",
                                mclk, bus, magic, bpp));

                total = mclk * bus / bpp;

                xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"Memory bandwidth at %d bpp is %g MHz\n", bpp, total/1000);

                if(pSiS->VBFlags & CRT2_ENABLE)  {

		    crt2used = 0.0;
		    crt2clock = SiSEstimateCRT2Clock(pScrn);
		    if(crt2clock) {
		    	crt2used = crt2clock + 2000;
		    }

#ifdef SISDUALHEAD
		    if((pSiS->DualHeadMode) && (pSiSEnt)) {

		        if(!pSiS->SecondHead) {

			     /* TW: First head = CRT2 */

			     if(crt2clock) {
			        /* TW: We use the mem bandwidth as max clock; this
				 *     might exceed the 70% limit a bit, but that
				 *     does not matter; we take care of that limit
				 *     when we calc CRT1. Overall, we might use up
				 *     to 85% of the memory bandwidth, which seems
				 *     enough to use accel and video.
				 *     The "* macic" is just to compensate the
				 *     calculation below.
				*/
			        total = crt2used * magic;
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	           "Bandwidth reserved for CRT2 is %g Mhz\n",
				      crt2used/1000);
			     } else {
			        /*  We don't know about the second head's
				 *  depth yet. So we assume it uses the
			         *  same.
			         */
                                total /= 2;
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	           "Bandwidth reserved for CRT2 is %g Mhz\n",
				      total/1000);
                             }

			} else {

			     /* TW: Second head = CRT1 */

			     /*     Now We know about the first head's depth,
			      *     so we can calculate more accurately.
			      */

			     if(crt2clock) {
			        total -= (crt2used * pSiSEnt->pScrn_1->bitsPerPixel / bpp);
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	           "Bandwidth reserved for CRT2 at %d bpp is %g Mhz\n",
				      bpp,
				      (crt2used * pSiSEnt->pScrn_1->bitsPerPixel / bpp)/1000);
			     } else {
			        total -= (pSiSEnt->maxUsedClock * pSiSEnt->pScrn_1->bitsPerPixel / bpp);
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	           "Bandwidth reserved for CRT2 at %d bpp is %d Mhz\n",
				      bpp,
				      (pSiSEnt->maxUsedClock * pSiSEnt->pScrn_1->bitsPerPixel / bpp)/1000);
			     }

			     xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			        "Bandwidth available for CRT1 is %g MHz\n", total/1000);

			}

		    } else {
#endif
			if(crt2clock) {
			    total -= crt2used;
			    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	       "Bandwidth reserved for CRT2 is %g Mhz\n", crt2used/1000);
			} else {
                            total /= 2;
			    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		       	       "Bandwidth reserved for CRT2 is %g Mhz\n", total/1000);
			}

			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			  "Bandwidth available for CRT1 is %g MHz\n", total/1000);
#ifdef SISDUALHEAD
                    }
#endif

                }
		total /= magic;
		if(total > (max / 2)) total = max / 2;
                return (int)(total);

        default:
                return 135000;    /* guessed */
        }
}

/* TW: Load the palette. We do this for all supported color depths
 *     in order to support gamma correction. We hereby convert the
 *     given colormap to a complete 24bit color palette and enable
 *     the correspoding bit in SR7 to enable the 24bit lookup table.
 *     Gamma correction is only supported on CRT1.
 *     Why are there 6-bit-RGB values submitted even if bpp is 16 and
 *     weight is 565? (Maybe because rgbBits is 6?)
 */
void
SISLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
               VisualPtr pVisual)
{
        SISPtr  pSiS = SISPTR(pScrn);
        int     i, j, index;

        PDEBUG(ErrorF("SiSLoadPalette(%d)\n", numColors));

#ifdef SISDUALHEAD
        /* TW: No palette changes on CRT2 if in dual head mode */
        if((pSiS->DualHeadMode) && (!pSiS->SecondHead)) return;
#endif

        switch(pSiS->CurrentLayout.depth) {
#ifdef SISGAMMA
          case 15:
	     orSISIDXREG(SISSR, 0x07, 0x04);
	     for(i=0; i<numColors; i++) {
                index = indices[i];
		if(index < 32) {   /* Paranoia */
		   for(j=0; j<8; j++) {
		      outSISREG(SISCOLIDX, (index * 8) + j);
                      outSISREG(SISCOLDATA, colors[index].red << (8- pScrn->rgbBits));
                      outSISREG(SISCOLDATA, colors[index].green << (8 - pScrn->rgbBits));
                      outSISREG(SISCOLDATA, colors[index].blue << (8 - pScrn->rgbBits));
		   }
		}
             }
	     break;
	  case 16:
             orSISIDXREG(SISSR, 0x07, 0x04);
	     for(i=0; i<numColors; i++) {
                index = indices[i];
		if(index < 64) {  /* Paranoia */
		   for(j=0; j<4; j++) {
		      outSISREG(SISCOLIDX, (index * 4) + j);
                      outSISREG(SISCOLDATA, colors[index/2].red << (8 - pScrn->rgbBits));
                      outSISREG(SISCOLDATA, colors[index].green << (8 - pScrn->rgbBits));
                      outSISREG(SISCOLDATA, colors[index/2].blue << (8 - pScrn->rgbBits));
		   }
		}
             }
	     break;
          case 24:
	     orSISIDXREG(SISSR, 0x07, 0x04);
             for(i=0; i<numColors; i++)  {
                index = indices[i];
		if(index < 256) {   /* Paranoia */
                   outSISREG(SISCOLIDX, index);
                   outSISREG(SISCOLDATA, colors[index].red);
                   outSISREG(SISCOLDATA, colors[index].green);
                   outSISREG(SISCOLDATA, colors[index].blue);
		}
             }
	     break;
#endif
	  default:
	     if(pScrn->rgbBits == 8)
	        orSISIDXREG(SISSR, 0x07, 0x04);
	     else
	        andSISIDXREG(SISSR, 0x07, ~0x04);
             for(i=0; i<numColors; i++)  {
                index = indices[i];
                outSISREG(SISCOLIDX, index);
                outSISREG(SISCOLDATA, colors[index].red >> (8 - pScrn->rgbBits));
                outSISREG(SISCOLDATA, colors[index].green >> (8 - pScrn->rgbBits));
                outSISREG(SISCOLDATA, colors[index].blue >> (8 - pScrn->rgbBits));
             }
	}

        switch(pSiS->VGAEngine) {
            case SIS_300_VGA:
            case SIS_315_VGA:
                if(pSiS->VBFlags & CRT2_ENABLE) {
		    /* TW: Only the SiS bridges support a CRT2 palette */
		    if(pSiS->VBFlags & VB_SISBRIDGE) {
                        (*pSiS->LoadCRT2Palette)(pScrn, numColors, indices,
                                                 colors, pVisual);
		    }  
                }
                break;
        }

}

static  void
SiS301LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
                                        LOCO *colors, VisualPtr pVisual)
{
        SISPtr  pSiS = SISPTR(pScrn);
        int     i, index;

        PDEBUG(ErrorF("SiS301LoadPalette(%d)\n", numColors));

        for(i=0; i<numColors; i++)  {
                index = indices[i];
                outSISREG(SISCOL2IDX,  index);
                outSISREG(SISCOL2DATA, colors[index].red);
                outSISREG(SISCOL2DATA, colors[index].green);
                outSISREG(SISCOL2DATA, colors[index].blue);
        }
}


#ifdef DEBUG
/* TW: Debug function to dump registers */
void SiSIODump(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     i, max3c4, min3d4, max3d4;
    unsigned char   temp;

    switch (pSiS->Chipset)  {
        case PCI_CHIP_SIS6326:
            max3c4 = 0x3F;
            max3d4 = 0x19;
            min3d4 = 0x26;
            break;
        case PCI_CHIP_SIS530:
            max3c4 = 0x3F;
            max3d4 = 0x19;
            min3d4 = 0x26;
            break;
        case PCI_CHIP_SIS300:
        case PCI_CHIP_SIS630:
        case PCI_CHIP_SIS540:
	    max3c4 = 0x3D;
            max3d4 = 0x37;
            min3d4 = 0x30;
            break;
	case PCI_CHIP_SIS550:
	case PCI_CHIP_SIS650:
	case PCI_CHIP_SIS315:
	case PCI_CHIP_SIS315H:
	case PCI_CHIP_SIS315PRO:
	case PCI_CHIP_SIS330:
            max3c4 = 0x3D;
            max3d4 = 0x5f;
            min3d4 = 0x30;
            break;
        default:
            max3c4 = 0x38;
            max3d4 = 0x19;
            min3d4 = 0x26;
    }
    /* dump Misc Registers */
    temp = inb(pSiS->RelIO+0x4c);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Misc Output 3CC=%x\n", temp);

    temp = inb(pSiS->RelIO+0x4a);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Feature Control 3CA=%x\n", temp);

    /* Dump GR */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Registers 3CE\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    for (i=0; i<=8; i++)  {
        inSISIDXREG(SISGR, i, temp);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[%2x]=%2x\n", i, temp);
    }

    /* dump SR0 ~ SR4 */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Registers 3C4\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    for (i=0; i<=4; i++)  {
        inSISIDXREG(SISSR, i, temp);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[%2x]=%2x\n", i, temp);
    }

    /* dump extended SR */
#ifdef UNLOCK_ALWAYS
    sisSaveUnlockExtRegisterLock(pSiS, NULL, NULL);
#endif
    for (i=5; i<=max3c4; i++)  {
        inSISIDXREG(SISSR, i, temp);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[%2x]=%2x\n", i, temp);
    }

    /* dump CR0 ~ CR18 */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Registers 3D4\n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-------------\n");
    for (i=0; i<=0x18; i++)  {
        inSISIDXREG(SISCR, i, temp);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[%2x]=%2x\n", i, temp);
    }
    /* dump extended CR */
    for (i=min3d4; i<=max3d4; i++)  {
        inSISIDXREG(SISCR, i, temp);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[%2x]=%2x\n", i, temp);
    }
}
#endif

void
SISDACPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);

    switch (pSiS->Chipset)  {
      case PCI_CHIP_SIS550:
      case PCI_CHIP_SIS650:
      case PCI_CHIP_SIS315:
      case PCI_CHIP_SIS315H:
      case PCI_CHIP_SIS315PRO:
      case PCI_CHIP_SIS330:
        pSiS->MaxClock               = SiSMemBandWidth(pScrn);
        pSiS->SiSSave                = SiS310Save;
        pSiS->SiSSave2               = SiS301Save;
        pSiS->SiSSave3               = SiS301BSave;
        pSiS->SiSSaveLVDSChrontel    = SiSLVDSChrontelSave;
        pSiS->SiSRestore             = SiS310Restore;
        pSiS->SiSRestore2            = SiS301Restore;
        pSiS->SiSRestore3            = SiS301BRestore;
        pSiS->SiSRestoreLVDSChrontel = SiSLVDSChrontelRestore;
        pSiS->LoadCRT2Palette        = SiS301LoadPalette;
        pSiS->SetThreshold           = SiSThreshold;
	pSiS->i2cInit		     = NULL; /* SiSI2CInit;  */
        break;
      case PCI_CHIP_SIS300:
      case PCI_CHIP_SIS630:
      case PCI_CHIP_SIS540:
        pSiS->MaxClock               = SiSMemBandWidth(pScrn);
        pSiS->SiSSave                = SiS300Save;
        pSiS->SiSSave2               = SiS301Save;
        pSiS->SiSSave3               = SiS301BSave;
        pSiS->SiSSaveLVDSChrontel    = SiSLVDSChrontelSave;
        pSiS->SiSRestore             = SiS300Restore;
        pSiS->SiSRestore2            = SiS301Restore;
        pSiS->SiSRestore3            = SiS301BRestore;
        pSiS->SiSRestoreLVDSChrontel = SiSLVDSChrontelRestore;
        pSiS->LoadCRT2Palette        = SiS301LoadPalette;
        pSiS->SetThreshold           = SiSThreshold;
	pSiS->i2cInit		     = NULL;  /* SiSI2CInit;  */
        break;
      case PCI_CHIP_SIS5597:
      case PCI_CHIP_SIS6326:
      case PCI_CHIP_SIS530:
      default:
        pSiS->MaxClock               = SiSMemBandWidth(pScrn);
        pSiS->SiSRestore             = SiSRestore;
        pSiS->SiSSave                = SiSSave;
        pSiS->SetThreshold           = SiSThreshold;
	pSiS->i2cInit		     = NULL;
        break;
    }
}

static void
SetBlock(CARD16 port, CARD8 from, CARD8 to, CARD8 *DataPtr)
{
    CARD8   index;

    for (index=from; index <= to; index++, DataPtr++)  {
        outSISIDXREG(port, index, *DataPtr);
    }
}

void
SiS6326SetTVReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data)
{
    SISPtr  pSiS = SISPTR(pScrn);
    outSISIDXREG(SISCR, 0xE0, index);
    outSISIDXREG(SISCR, 0xE1, data);
#ifdef TWDEBUG
    xf86DrvMsg(0, X_INFO, "SiS6326: Setting Tv %02x to %02x\n", index, data);
#endif
}

unsigned char
SiS6326GetTVReg(ScrnInfoPtr pScrn, CARD8 index)
{
   SISPtr  pSiS = SISPTR(pScrn);
   unsigned char data;

   outSISIDXREG(SISCR, 0xE0, index);
   inSISIDXREG(SISCR, 0xE1, data);
   return(data);
}

void
SiS6326SetXXReg(ScrnInfoPtr pScrn, CARD8 index, CARD8 data)
{
    SISPtr  pSiS = SISPTR(pScrn);
    outSISIDXREG(SISCR, 0xE2, index);
    outSISIDXREG(SISCR, 0xE3, data);
}

unsigned char
SiS6326GetXXReg(ScrnInfoPtr pScrn, CARD8 index)
{
   SISPtr  pSiS = SISPTR(pScrn);
   unsigned char data;

   outSISIDXREG(SISCR, 0xE2, index);
   inSISIDXREG(SISCR, 0xE3, data);
   return(data);
}
