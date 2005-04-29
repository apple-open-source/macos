/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_hwdiff.c,v 1.2 2003/08/27 15:16:09 tsi Exp $ */
/*************************************************************************
 *
 *   HWDiff.c 
 *
 *   Implement all Video Function for the Driver 
 *
 *   DATE     04/07/2003
 *
 *************************************************************************/

#include "via_driver.h" 

void VIAvfInitHWDiff(VIAPtr pVia)
{
    switch(pVia->ChipId)
    {
        case PCI_CHIP_VT3204:
             /*
              *	HW Difference Flag
              */
             pVia->ViaHW.dwThreeHQVBuffer = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3SrcHeightSetting = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwSupportExtendFIFO = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwHQVInitPatch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwSupportV3Gamma=VID_HWDIFF_TRUE;
             pVia->ViaHW.dwUpdFlip = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwHQVDisablePatch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwSUBFlip = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwNeedV3Prefetch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwNeedV4Prefetch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwUseSystemMemory = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwExpandVerPatch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwExpandVerHorPatch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwV3ExpireNumTune = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3FIFOThresholdTune = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwCheckHQVFIFOEmpty = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwUseMPEGAGP = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3FIFOPatch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwSupportTwoColorKey = VID_HWDIFF_FALSE; 
             break;
        case PCI_CHIP_VT3205:
             /*
              *	HW Difference Flag
              */
             pVia->ViaHW.dwThreeHQVBuffer = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3SrcHeightSetting = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwSupportExtendFIFO = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwHQVInitPatch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwSupportV3Gamma=VID_HWDIFF_FALSE;
             pVia->ViaHW.dwUpdFlip = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwHQVDisablePatch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwSUBFlip = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwNeedV3Prefetch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwNeedV4Prefetch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwUseSystemMemory = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwExpandVerPatch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwExpandVerHorPatch = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3ExpireNumTune = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3FIFOThresholdTune = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwCheckHQVFIFOEmpty = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwUseMPEGAGP = VID_HWDIFF_TRUE;
             pVia->ViaHW.dwV3FIFOPatch = VID_HWDIFF_FALSE;
             pVia->ViaHW.dwSupportTwoColorKey = VID_HWDIFF_FALSE;
             break;
        case PCI_CHIP_CLE3022:
        case PCI_CHIP_CLE3122:
             switch (pVia->ChipRev)
             {
                case VIA_REVISION_CLEC0:
                case VIA_REVISION_CLEC1:
                     /*
                      * HW Difference Flag
                      */
                     pVia->ViaHW.dwThreeHQVBuffer = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwV3SrcHeightSetting = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwSupportExtendFIFO = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwHQVFetchByteUnit = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwHQVInitPatch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwSupportV3Gamma=VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwUpdFlip = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwHQVDisablePatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwSUBFlip = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwNeedV3Prefetch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwNeedV4Prefetch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwUseSystemMemory = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwExpandVerPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwExpandVerHorPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwV3ExpireNumTune = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwV3FIFOThresholdTune = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwCheckHQVFIFOEmpty = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwUseMPEGAGP = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwV3FIFOPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwSupportTwoColorKey = VID_HWDIFF_TRUE;
                     /*pVia->ViaHW.dwCxColorSpace = VID_HWDIFF_TRUE;*/
                     break;
                default:
                     /*
                      * HW Difference Flag
                      */
                     pVia->ViaHW.dwThreeHQVBuffer = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwV3SrcHeightSetting = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwSupportExtendFIFO = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwHQVFetchByteUnit = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwHQVInitPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwSupportV3Gamma=VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwUpdFlip = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwHQVDisablePatch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwSUBFlip = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwNeedV3Prefetch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwNeedV4Prefetch = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwUseSystemMemory = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwExpandVerPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwExpandVerHorPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwV3ExpireNumTune = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwV3FIFOThresholdTune = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwCheckHQVFIFOEmpty = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwUseMPEGAGP = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwV3FIFOPatch = VID_HWDIFF_TRUE;
                     pVia->ViaHW.dwSupportTwoColorKey = VID_HWDIFF_FALSE;
                     pVia->ViaHW.dwCxColorSpace = VID_HWDIFF_FALSE;
                     break;
             }/*CLEC0 Switch*/
             break;             
    }
}     
