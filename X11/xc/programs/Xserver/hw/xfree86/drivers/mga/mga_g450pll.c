/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_g450pll.c,v 1.8 2002/09/16 18:05:56 eich Exp $ */

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

static CARD32 G450ApplyPFactor(ScrnInfoPtr pScrn, CARD8 ucP, CARD32 *pulFIn)
{
   if(!(ucP & 0x40))
   {
      *pulFIn = *pulFIn / (2L << (ucP & 3));
   }

   return TRUE;
}


static CARD32 G450RemovePFactor(ScrnInfoPtr pScrn, CARD8 ucP, CARD32 *pulFIn)
{
   if(!(ucP & 0x40))
   {
      *pulFIn = *pulFIn * (2L << (ucP & 3));
   }
  
   return TRUE; 
}


static CARD32 G450CalculVCO(ScrnInfoPtr pScrn, CARD32 ulMNP, CARD32 *pulF)
{
   CARD8 ucM, ucN, ucP;

   ucM = (CARD8)((ulMNP >> 16) & 0xff);
   ucN = (CARD8)((ulMNP >>  8) & 0xff);
   ucP = (CARD8)(ulMNP & 0x03);

   *pulF = (27000 * (2 * (ucN + 2)) + ((ucM + 1) >> 1)) / (ucM + 1);
   
   return TRUE;
}


static CARD32 G450CalculDeltaFreq(ScrnInfoPtr pScrn, CARD32 ulF1,
                                  CARD32 ulF2, CARD32 *pulDelta)
{
   if(ulF2 < ulF1)
   {
      *pulDelta = ((ulF1 - ulF2) * 1000) / ulF1;
   }
   else
   {
      *pulDelta = ((ulF2 - ulF1) * 1000) / ulF1;
   }
 
   return TRUE;
}




static CARD32 G450FindNextPLLParam(ScrnInfoPtr pScrn, CARD32 ulFout,
                                   CARD32 *pulPLLMNP)
{
   CARD8 ucM, ucN, ucP, ucS;
   CARD32 ulVCO, ulVCOMin;

   ucM = (CARD8)((*pulPLLMNP >> 16) & 0xff);
   ucN = (CARD8)((*pulPLLMNP >>  8) & 0xff);
   ucP = (CARD8)(*pulPLLMNP &  0x43);

   ulVCOMin = 256000;

   if(ulVCOMin >= (255L * 8000))
   {
      ulVCOMin = 230000;
   }
   
   if((ucM == 9) && (ucP & 0x40))
   {
      *pulPLLMNP = 0xffffffff;
   } else if (ucM == 9)
   {
      if(ucP)
      {
         ucP--;
      }
      else
      {
         ucP = 0x40;
      }
      ucM = 0;
   }
   else
   {
      ucM++;
   }

   ulVCO = ulFout;

   G450RemovePFactor(pScrn, ucP, &ulVCO);

   if(ulVCO < ulVCOMin)
   {
      *pulPLLMNP = 0xffffffff;
   }

   if(*pulPLLMNP != 0xffffffff)
   {
      ucN = (CARD8)(((ulVCO * (ucM+1) + 27000)/(27000 * 2)) - 2);

      ucS = 5;
      if(ulVCO < 1300000) ucS = 4;
      if(ulVCO < 1100000) ucS = 3;
      if(ulVCO <  900000) ucS = 2;
      if(ulVCO <  700000) ucS = 1;
      if(ulVCO <  550000) ucS = 0;

      ucP |= (CARD8)(ucS << 3);

      *pulPLLMNP &= 0xff000000;
      *pulPLLMNP |= (CARD32)ucM << 16;
      *pulPLLMNP |= (CARD32)ucN << 8;
      *pulPLLMNP |= (CARD32)ucP;
      
#ifdef DEBUG
      ErrorF("FINS_S: VCO = %d, S = %02X, *pulPLLMNP = %08X\n", ulVCO, (ULONG)ucS, *pulPLLMNP);
#endif
  }

   return TRUE;
}

 
static CARD32 G450FindFirstPLLParam(ScrnInfoPtr pScrn, CARD32 ulFout, 
                                    CARD32 *pulPLLMNP)
{
   CARD8 ucP;
   CARD32 ulVCO;
   CARD32 ulVCOMax;

   /* Default value */
   ulVCOMax = 1300000;

   if(ulFout > (ulVCOMax/2))
   {
      ucP = 0x40;
      ulVCO = ulFout;
   }
   else
   {
      ucP = 3;
      ulVCO = ulFout;
      G450RemovePFactor(pScrn, ucP, &ulVCO);
      while(ucP && (ulVCO > ulVCOMax))
      {
         ucP--;
         ulVCO = ulFout;
         G450RemovePFactor(pScrn, ucP, &ulVCO);
      }
   }

   if(ulVCO > ulVCOMax)
   {
      *pulPLLMNP = 0xffffffff;
   }
   else
   {
      /* Pixel clock: 1 */
      *pulPLLMNP = (1 << 24) + 0xff0000 + ucP;
      G450FindNextPLLParam(pScrn, ulFout, pulPLLMNP);
   }

   return TRUE;

}


static CARD32 G450WriteMNP(ScrnInfoPtr pScrn, CARD32 ulMNP)
{
   MGAPtr pMga = MGAPTR(pScrn);
   MGARegPtr pReg;

   pReg = &pMga->ModeReg;

   if (!pMga->SecondCrtc) {
      outMGAdac(MGA1064_PIX_PLLC_M, (CARD8)(ulMNP >> 16));   
      outMGAdac(MGA1064_PIX_PLLC_N, (CARD8)(ulMNP >>  8));   
      outMGAdac(MGA1064_PIX_PLLC_P, (CARD8) ulMNP);   
   } else {
      outMGAdac(MGA1064_VID_PLL_M, (CARD8)(ulMNP >> 16));
      outMGAdac(MGA1064_VID_PLL_N, (CARD8)(ulMNP >> 8)); 
      outMGAdac(MGA1064_VID_PLL_P, (CARD8) ulMNP);
   }
   return TRUE;
}

static CARD32 G450ReadMNP(ScrnInfoPtr pScrn)
{
   MGAPtr pMga = MGAPTR(pScrn);
   MGARegPtr pReg;
   CARD32 ret = 0;

   pReg = &pMga->ModeReg;

   if (!pMga->SecondCrtc) {
      ret = (CARD8)inMGAdac(MGA1064_PIX_PLLC_M) << 16;   
      ret |= (CARD8)inMGAdac(MGA1064_PIX_PLLC_N) << 8;   
      ret |= (CARD8)inMGAdac(MGA1064_PIX_PLLC_P);   
   } else {
      ret = (CARD8)inMGAdac(MGA1064_VID_PLL_M) << 16;
      ret |= (CARD8)inMGAdac(MGA1064_VID_PLL_N) << 8; 
      ret |= (CARD8)inMGAdac(MGA1064_VID_PLL_P);
   }
   return ret;
}


static CARD32 G450CompareMNP(ScrnInfoPtr pScrn, CARD32 ulFout, CARD32 ulMNP1,
                      CARD32 ulMNP2, long *pulResult)
{
   CARD32 ulFreq, ulDelta1, ulDelta2;

   G450CalculVCO(pScrn, ulMNP1, &ulFreq);
   G450ApplyPFactor(pScrn, (CARD8) ulMNP1, &ulFreq);
   G450CalculDeltaFreq(pScrn, ulFout, ulFreq, &ulDelta1);

   G450CalculVCO(pScrn, ulMNP2, &ulFreq);
   G450ApplyPFactor(pScrn, (CARD8) ulMNP2, &ulFreq);
   G450CalculDeltaFreq(pScrn, ulFout, ulFreq, &ulDelta2);

   if(ulDelta1 < ulDelta2)
   {
      *pulResult = -1;
   }
   else if(ulDelta1 > ulDelta2)
   {
      *pulResult = 1;
   }
   else
   {
      *pulResult = 0;
   }

   if((ulDelta1 <= 5) && (ulDelta2 <= 5))
   {
      if((ulMNP1 & 0xff0000) < (ulMNP2 & 0xff0000))
      {
         *pulResult = -1;
      }
      else if((ulMNP1 & 0xff0000) > (ulMNP2 & 0xff0000))
      {
         *pulResult = 1;
      }
   }

   return TRUE;
}


static CARD32 G450IsPllLocked(ScrnInfoPtr pScrn, Bool *lpbLocked)
{
   CARD32 ulFallBackCounter, ulLockCount, ulCount;
   CARD8  ucPLLStatus;

   MGAPtr pMga = MGAPTR(pScrn);

   /* Pixel PLL */
   if (!pMga->SecondCrtc)
      OUTREG8(0x3c00, 0x4f);    /* Pixel PLL */
   else
      OUTREG8(0x3c00, 0x8c);    /* Video PLL */

   ulFallBackCounter = 0;

   do 
   {
      ucPLLStatus = INREG8(0x3c0a);
      ulFallBackCounter++;
   } while(!(ucPLLStatus & PLLLOCK) && (ulFallBackCounter < 1000));

   ulLockCount = 0;
   if(ulFallBackCounter < 1000)
   {
      for(ulCount = 0; ulCount < 100; ulCount++)
      {
         ucPLLStatus = INREG8(0x3c0a);
         if(ucPLLStatus & PLLLOCK)
         {
            ulLockCount++;
         }
      }
   }

   *lpbLocked = ulLockCount >= 90;

   return TRUE;
}


double MGAG450SetPLLFreq(ScrnInfoPtr pScrn, long f_out) 
{
   Bool bFoundValidPLL;
   Bool bLocked;
   CARD8  ucMisc, ucSIndex, ucSTable[4];
   CARD32 ulMaxIndex;
   CARD32 ulMNP;
   CARD32 ulMNPTable[MNP_TABLE_SIZE];
   CARD32 ulIndex;
   CARD32 ulTryMNP;
   long lCompareResult;
   MGAPtr pMga = MGAPTR(pScrn);

#ifdef DEBUG
   xf86DrvMsg(pScrn->scrnIndex,X_INFO, "Restoring PLLClk = %d\n",f_out);
#endif
   G450FindFirstPLLParam(pScrn, f_out, &ulMNP);
   ulMNPTable[0] = ulMNP;
   G450FindNextPLLParam(pScrn, f_out, &ulMNP);
   ulMaxIndex = 1;
   while(ulMNP != 0xffffffff)
   {
      int ulIndex;
      Bool bSkipValue;

      bSkipValue = FALSE;
      if(ulMaxIndex == MNP_TABLE_SIZE)
      {
         G450CompareMNP(pScrn, f_out, ulMNP, ulMNPTable[MNP_TABLE_SIZE - 1],
                        &lCompareResult);

         if(lCompareResult > 0)
         {
            bSkipValue = TRUE;
         }
         else
         {
            ulMaxIndex--;
         }
      }

      if(!bSkipValue)
      {
         for(ulIndex = ulMaxIndex; !bSkipValue && (ulIndex > 0); ulIndex--)
         {
            G450CompareMNP(pScrn, f_out, ulMNP, ulMNPTable[ulIndex - 1],
                           &lCompareResult);

            if(lCompareResult < 0)
            {
               ulMNPTable[ulIndex] = ulMNPTable[ulIndex - 1];
            }
            else
            {
               break;
            }
         }
         ulMNPTable[ulIndex] = ulMNP;
         ulMaxIndex++;
      }

      G450FindNextPLLParam(pScrn, f_out, &ulMNP);
   }

   bFoundValidPLL = FALSE;
   ulMNP = 0;

   /* For pixel pll */
   if (!pMga->SecondCrtc) {
       ucMisc = INREG8(0x1FCC);
       OUTREG8(0x1fc2, (CARD8)(ucMisc | CLKSEL_MGA));    
   }

   for(ulIndex = 0; !bFoundValidPLL && (ulIndex < ulMaxIndex); ulIndex++)
   {
       ulTryMNP = ulMNPTable[ulIndex];
       
       ucSTable[3] = 0xff;
       ucSTable[2] = 0xff;
       ucSTable[0] = (CARD8) (ulTryMNP & 0x38);

       if (ucSTable[0] != 0)  {
	   ucSTable[1] = ucSTable[0] - 8;
	   if (ucSTable[0] != 0x38) {
	       ucSTable[2] = ucSTable[0] + 8;
	   }
       } else {
	   ucSTable[1] = 8;
       }
       
       for(ucSIndex = 0; !bFoundValidPLL && (ucSTable[ucSIndex] != 0xff);
	   ucSIndex++) {
	    ulTryMNP &= 0xffffffc7;
	    ulTryMNP |= (CARD32)ucSTable[ucSIndex];
         
         bLocked = TRUE;
         if((ulMNPTable[ulIndex] & 0xff00) < 0x300 ||
            (ulMNPTable[ulIndex] & 0xff00) > 0x7a00)
         {
            bLocked = FALSE;
         }

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP - 0x300);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP + 0x300);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP - 0x200);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP + 0x200);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP - 0x100);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP + 0x100);
            G450IsPllLocked(pScrn, &bLocked);
         }     

         if(bLocked)
         {
            G450WriteMNP(pScrn, ulTryMNP);
            G450IsPllLocked(pScrn, &bLocked);
         }     
         else if(!ulMNP)
         {
            G450WriteMNP(pScrn, ulTryMNP);
            G450IsPllLocked(pScrn, &bLocked);
            if(bLocked)
            {
               ulMNP = ulMNPTable[ulIndex]; 
            }
            bLocked = FALSE;
         }

         if(bLocked)
         {
            bFoundValidPLL = TRUE;
         }
      }
   }

   if(!bFoundValidPLL)
   {
      if(ulMNP)
      {
         G450WriteMNP(pScrn, ulMNP);
      }
      else
      {
         G450WriteMNP(pScrn, ulMNPTable[0]);
      }
   }
  
   return TRUE;
}

long
MGAG450SavePLLFreq(ScrnInfoPtr pScrn) 
{
    CARD32 ulMNP = G450ReadMNP(pScrn);
    CARD8  ucP;
    CARD32 freq;

    G450CalculVCO(pScrn, ulMNP, &freq);
    ucP = (CARD8)(ulMNP & 0x03);
    G450ApplyPFactor(pScrn, ucP, &freq);

#ifdef DEBUG
    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"Saved PLLClk = %d\n",freq);
#endif
    return freq;
}

#ifdef DEBUG
void
MGAG450PrintPLL(ScrnInfoPtr pScrn)
{
    CARD32 ulMNP = G450ReadMNP(pScrn);
    CARD8  ucP;
    CARD32 freq;

    G450CalculVCO(pScrn, ulMNP, &freq);
    ucP = (CARD8)(ulMNP & 0x03);
    G450ApplyPFactor(pScrn, ucP, &freq);

    xf86DrvMsg(pScrn->scrnIndex,X_INFO,"MGAGClock = %d -- MNP = 0x%x\n",
	       freq,ulMNP);
}
#endif
