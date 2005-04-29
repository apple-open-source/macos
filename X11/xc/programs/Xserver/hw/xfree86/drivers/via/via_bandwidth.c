/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_bandwidth.c,v 1.3 2004/01/05 00:34:17 dawes Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "via_driver.h"
#include "via_bandwidth.h"

void VIADisabledExtendedFIFO(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD32  dwGE230, dwGE298;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIADisabledExtendedFIFO\n"));
    /* Cause of exit XWindow will dump back register value, others chipset no
     * need to set extended fifo value */
    if (pBIOSInfo->Chipset == VIA_CLE266 && pBIOSInfo->ChipRev < 15 &&
        (pBIOSInfo->HDisplay > 1024 || pBIOSInfo->HasSecondary)) {
        /* Turn off Extend FIFO */
        /* 0x298[29] */
        dwGE298 = VIAGETREG(0x298);
        VIASETREG(0x298, dwGE298 | 0x20000000);
        /* 0x230[21] */
        dwGE230 = VIAGETREG(0x230);
        VIASETREG(0x230, dwGE230 & ~0x00200000);
        /* 0x298[29] */
        dwGE298 = VIAGETREG(0x298);
        VIASETREG(0x298, dwGE298 & ~0x20000000);
    }
}

void VIAEnabledPrimaryExtendedFIFO(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8   bRegTemp;
    CARD32  dwGE230, dwGE298;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAEnabledPrimaryExtendedFIFO\n"));
    switch (pBIOSInfo->Chipset) {
    case VIA_CLE266:
        if (pBIOSInfo->ChipRev > 14) {  /* For 3123Cx */
            if (pBIOSInfo->HasSecondary) {  /* SAMM or DuoView case */
                if (pBIOSInfo->HDisplay >= 1024)
    	        {
    	            /* 3c5.16[0:5] */
        	        VGAOUT8(0x3C4, 0x16);
            	    bRegTemp = VGAIN8(0x3C5);
    	            bRegTemp &= ~0x3F;
        	        bRegTemp |= 0x1C;
            	    VGAOUT8(0x3C5, bRegTemp);
        	        /* 3c5.17[0:6] */
            	    VGAOUT8(0x3C4, 0x17);
                	bRegTemp = VGAIN8(0x3C5);
    	            bRegTemp &= ~0x7F;
        	        bRegTemp |= 0x3F;
            	    VGAOUT8(0x3C5, bRegTemp);
    	        }
            }
            else   /* Single view or Simultaneous case */
            {
                if (pBIOSInfo->HDisplay > 1024)
    	        {
    	            /* 3c5.16[0:5] */
        	        VGAOUT8(0x3C4, 0x16);
            	    bRegTemp = VGAIN8(0x3C5);
    	            bRegTemp &= ~0x3F;
        	        bRegTemp |= 0x17;
            	    VGAOUT8(0x3C5, bRegTemp);
        	        /* 3c5.17[0:6] */
            	    VGAOUT8(0x3C4, 0x17);
                	bRegTemp = VGAIN8(0x3C5);
    	            bRegTemp &= ~0x7F;
        	        bRegTemp |= 0x2F;
            	    VGAOUT8(0x3C5, bRegTemp);
    	        }
            }
            /* 3c5.18[0:5] */
            VGAOUT8(0x3C4, 0x18);
            bRegTemp = VGAIN8(0x3C5);
            bRegTemp &= ~0x3F;
            bRegTemp |= 0x17;
            bRegTemp |= 0x40;  /* force the preq always higher than treq */
            VGAOUT8(0x3C5, bRegTemp);
        }
        else {      /* for 3123Ax */
            if (pBIOSInfo->HDisplay > 1024 || pBIOSInfo->HasSecondary) {
                /* Turn on Extend FIFO */
                /* 0x298[29] */
                dwGE298 = VIAGETREG(0x298);
                VIASETREG(0x298, dwGE298 | 0x20000000);
                /* 0x230[21] */
                dwGE230 = VIAGETREG(0x230);
                VIASETREG(0x230, dwGE230 | 0x00200000);
                /* 0x298[29] */
                dwGE298 = VIAGETREG(0x298);
                VIASETREG(0x298, dwGE298 & ~0x20000000);

                /* 3c5.16[0:5] */
                VGAOUT8(0x3C4, 0x16);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp |= 0x17;
                /* bRegTemp |= 0x10; */
                VGAOUT8(0x3C5, bRegTemp);
                /* 3c5.17[0:6] */
                VGAOUT8(0x3C4, 0x17);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x7F;
                bRegTemp |= 0x2F;
                /*bRegTemp |= 0x1F;*/
                VGAOUT8(0x3C5, bRegTemp);
                /* 3c5.18[0:5] */
                VGAOUT8(0x3C4, 0x18);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp |= 0x17;
                bRegTemp |= 0x40;  /* force the preq always higher than treq */
                VGAOUT8(0x3C5, bRegTemp);
            }
        }
        break;
    case VIA_KM400:
    case VIA_K8M800:
        if (pBIOSInfo->HasSecondary) {  /* SAMM or DuoView case */
            if ((pBIOSInfo->HDisplay >= 1600) &&
                (pBIOSInfo->MemClk <= VIA_MEM_DDR200)) {
        	    /* enable CRT extendded FIFO */
            	VGAOUT8(0x3C4, 0x17);
                VGAOUT8(0x3C5, 0x1C);
    	        /* revise second display queue depth and read threshold */
        	    VGAOUT8(0x3C4, 0x16);
            	bRegTemp = VGAIN8(0x3C5);
    	        bRegTemp &= ~0x3F;
    	        bRegTemp = (bRegTemp) | (0x09);
                VGAOUT8(0x3C5, bRegTemp);
            }
            else {
                /* enable CRT extended FIFO */
                VGAOUT8(0x3C4, 0x17);
                VGAOUT8(0x3C5,0x3F);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3C4, 0x16);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp = (bRegTemp) | (0x1C);
                VGAOUT8(0x3C5, bRegTemp);
            }
            /* 3c5.18[0:5] */
            VGAOUT8(0x3C4, 0x18);
            bRegTemp = VGAIN8(0x3C5);
            bRegTemp &= ~0x3F;
            bRegTemp |= 0x17;
            bRegTemp |= 0x40;  /* force the preq always higher than treq */
            VGAOUT8(0x3C5, bRegTemp);
        }
        else {
            if ( (pBIOSInfo->HDisplay > 1024) && (pBIOSInfo->HDisplay <= 1280) )
            {
                /* enable CRT extendded FIFO */
                VGAOUT8(0x3C4, 0x17);
                VGAOUT8(0x3C5, 0x3F);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3C4, 0x16);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp = (bRegTemp) | (0x17);
                VGAOUT8(0x3C5, bRegTemp);
            }
            else if ((pBIOSInfo->HDisplay > 1280))
            {
                /* enable CRT extendded FIFO */
                VGAOUT8(0x3C4, 0x17);
                VGAOUT8(0x3C5, 0x3F);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3C4, 0x16);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp = (bRegTemp) | (0x1C);
                VGAOUT8(0x3C5, bRegTemp);
            }
            else
            {
                /* enable CRT extendded FIFO */
                VGAOUT8(0x3C4, 0x17);
                VGAOUT8(0x3C5, 0x3F);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3C4, 0x16);
                bRegTemp = VGAIN8(0x3C5);
                bRegTemp &= ~0x3F;
                bRegTemp = (bRegTemp) | (0x10);
                VGAOUT8(0x3C5, bRegTemp);
            }
            /* 3c5.18[0:5] */
            VGAOUT8(0x3C4, 0x18);
            bRegTemp = VGAIN8(0x3C5);
            bRegTemp &= ~0x3F;
            bRegTemp |= 0x17;
            bRegTemp |= 0x40;  /* force the preq always higher than treq */
            VGAOUT8(0x3C5, bRegTemp);
        }
        break;
    default:
        break;
    }
}

void VIAEnabledSecondaryExtendedFIFO(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8   bRegTemp;
    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAEnabledSecondaryExtendedFIFO\n"));
    switch (pBIOSInfo->Chipset) {
    case VIA_CLE266:
        if (pBIOSInfo->ChipRev > 15) {  /* for 3123Cx */
            if (((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                 (pBIOSInfo->panelX >= 1024)) || (pBIOSInfo->HDisplay >= 1024)) {
                /* enable extendded FIFO */
                VGAOUT8(0x3D4, 0x6a);
                bRegTemp = VGAIN8(0x3D5);
                bRegTemp |= 0x20;
                VGAOUT8(0x3D5, bRegTemp);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3D4, 0x68);
                VGAOUT8(0x3D5, 0xab);
            }
            else
            {
                /* disable extendded FIFO */
                VGAOUT8(0x3D4, 0x6a);
                bRegTemp = VGAIN8(0x3D5);
                bRegTemp &= ~0x20;
                VGAOUT8(0x3D5, bRegTemp);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3D4, 0x68);
                VGAOUT8(0x3D5, 0x67);
            }
        }
        else {      /* for 3123Ax */
            /* TV highest X-Resolution is smaller than 1280,
             * pBIOSInfo->HDisplay >= 1280 don't care. */
            if ((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                (((pBIOSInfo->panelY > 768) && (pBIOSInfo->bitsPerPixel >= 24) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR200)) ||
                 ((pBIOSInfo->panelX > 1280) && (pBIOSInfo->bitsPerPixel >= 24) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR266)))) {
                /* enable extendded FIFO */
                VGAOUT8(0x3D4, 0x6a);
                bRegTemp = VGAIN8(0x3D5);
                bRegTemp |= 0x20;
                VGAOUT8(0x3D5, bRegTemp);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3D4, 0x68);
                VGAOUT8(0x3D5, 0xab);
            }
            else {
                /* disable extendded FIFO */
                VGAOUT8(0x3D4, 0x6a);
                bRegTemp = VGAIN8(0x3D5);
                bRegTemp &= ~0x20;
                VGAOUT8(0x3D5, bRegTemp);
                /* revise second display queue depth and read threshold */
                VGAOUT8(0x3D4, 0x68);
                VGAOUT8(0x3D5, 0x67);
            }
        }
        break;
    case VIA_KM400:
    case VIA_K8M800:
        if ((((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
              (pBIOSInfo->panelX >= 1600)) || (pBIOSInfo->HDisplay >= 1600)) &&
            (pBIOSInfo->MemClk <= VIA_MEM_DDR200)) {
    	    /* enable extendded FIFO */
            VGAOUT8(0x3D4, 0x6a);
            bRegTemp = VGAIN8(0x3D5);
            bRegTemp |= 0x20;
            VGAOUT8(0x3D5, bRegTemp);
            /* revise second display queue depth and read threshold */
            VGAOUT8(0x3D4, 0x68);
            VGAOUT8(0x3D5, 0xeb);
        }
        else if (((((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                    (pBIOSInfo->panelX > 1024)) || (pBIOSInfo->HDisplay > 1024)) &&
                  (pBIOSInfo->bitsPerPixel == 32) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR333)) ||
                 ((((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                    (pBIOSInfo->panelX == 1024)) || (pBIOSInfo->HDisplay == 1024)) &&
                  (pBIOSInfo->bitsPerPixel == 32) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR200))) {
    	    /* enable extendded FIFO */
            VGAOUT8(0x3D4, 0x6a);
            bRegTemp = VGAIN8(0x3D5);
            bRegTemp |= 0x20;
            VGAOUT8(0x3D5, bRegTemp);
            /* revise second display queue depth and read threshold */
            VGAOUT8(0x3D4, 0x68);
            VGAOUT8(0x3D5, 0xca);
        }
        else if (((((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                    (pBIOSInfo->panelX > 1280)) || (pBIOSInfo->HDisplay > 1280)) &&
                  (pBIOSInfo->bitsPerPixel == 16) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR333)) ||
                 ((((pBIOSInfo->ActiveDevice & (VIA_DEVICE_LCD | VIA_DEVICE_DFP)) &&
                    (pBIOSInfo->panelX == 1280)) || (pBIOSInfo->HDisplay == 1280)) &&
                  (pBIOSInfo->bitsPerPixel == 16) &&
                  (pBIOSInfo->MemClk <= VIA_MEM_DDR200))) {
    	    /* enable extendded FIFO */
            VGAOUT8(0x3D4, 0x6a);
            bRegTemp = VGAIN8(0x3D5);
            bRegTemp |= 0x20;
            VGAOUT8(0x3D5, bRegTemp);
            /* revise second display queue depth and read threshold */
            VGAOUT8(0x3D4, 0x68);
            VGAOUT8(0x3D5, 0xab);
        }
        else {
    	    /* disable extendded FIFO */
        	VGAOUT8(0x3D4, 0x6a);
	        bRegTemp = VGAIN8(0x3D5);
    	    bRegTemp &= ~0x20;
        	VGAOUT8(0x3D5, bRegTemp);
	        /* revise second display queue depth and read threshold */
    	    VGAOUT8(0x3D4, 0x68);
        	VGAOUT8(0x3D5, 0x67);
        }
        break;
    default:
        break;
    }
}

void VIAFillExpireNumber(VIABIOSInfoPtr pBIOSInfo)
{
    VIABIOSInfoPtr  pVia = pBIOSInfo;
    CARD8   bRegTemp;
    const VIAPanel3C522Tue*    TuneExpireNum;

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAFillExpireNumber\n"));
    switch (pBIOSInfo->Chipset) {
    case VIA_CLE266:
        if (pBIOSInfo->ChipRev > 14) {
            TuneExpireNum = Panel_Tuning_LstC0;
        }
        else {
            TuneExpireNum = Panel_Tuning_Lst;
        }
        break;
    case VIA_KM400:
    case VIA_K8M800:
        TuneExpireNum = Panel_Tuning_Lst3205;
        break;
    default:
        return;
        break;
    }
    while (TuneExpireNum->wPanelXres != 0) {
        if (TuneExpireNum->wPanelXres == pBIOSInfo->panelX &&
            TuneExpireNum->wPanelYres == pBIOSInfo->panelY &&
            TuneExpireNum->wPanelBpp == pBIOSInfo->bitsPerPixel &&
            TuneExpireNum->bRamClock == pBIOSInfo->MemClk) {
            VGAOUT8(0x3C4, 0x22);
            bRegTemp = VGAIN8(0x3C5) & ~0x1F;
            VGAOUT8(0x3C5, (bRegTemp | TuneExpireNum->bTuningValue));
        }
        TuneExpireNum ++;
    }
}
