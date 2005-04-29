/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_utility.c,v 1.5 2003/12/31 05:42:05 dawes Exp $ */
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

/*
 * I N C L U D E S
 */
#include "via_driver.h"
#include "via_utility.h"


void VIAXVUtilityProc(ScrnInfoPtr pScrn, unsigned char *buf)
{
    VIAPtr pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr pBIOSInfo = pVia->pBIOSInfo;
    VIAUserSettingPtr pUserSetting = pBIOSInfo->UserSetting;
    VIAModeTablePtr pViaModeTable = pBIOSInfo->pModeTable;
    CARD8 *TV = NULL;
    int i, HPos, VPos, ADWHS, ADWHE;
    unsigned int tvIndx = pBIOSInfo->resTVMode;
    CARD32 dwFunc, dwAction = 0, dwInData = 0;
    UTBIOSVERSION pUTBIOSVERSION;
    UTBIOSDATE pUTBIOSDATE;
    UTPANELINFO pUTPANELINFO;
    UTXYVALUE MaxViewSizeValue, ViewSizeValue, MaxViewPosValue, ViewPosValue;
    UTSETTVTUNINGINFO pUTSETTVTUNINGINFO;
    UTSETTVITEMSTATE pUTSETTVITEMSTATE;
    UTGAMMAINFO pUTGAMMAINFO;
    CARD32 dwVideoRam, dwSupportState = 0, dwConnectState = 0, dwActiveState = 0;
    CARD32 dwSAMMState, dwRotateState = 0, dwExpandState, dwStandard = 0;
    CARD32 dwSignalType, dwMaxValue, dwItemID = 0, dwValue, dwState;
    CARD32 value;
    long dwUTRetOK = 1, dwUTRetFail = 0, dwUTRetNoFunc = -1;
    unsigned char *InParam;
    I2CDevPtr dev = NULL;
    unsigned char W_Buffer[3];
    unsigned char R_Buffer[3];

    WaitIdle();

    if ((pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) && (!pUserSetting->DefaultSetting)) {
	VIAUTGetInfo(pBIOSInfo);
    }

    switch (pBIOSInfo->TVEncoder) {
    case VIA_TV2PLUS:
	if (pBIOSInfo->TVType == TVTYPE_PAL) {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
		    TV = pViaModeTable->tv2Table[tvIndx].TVPALC;
		else
		    TV = pViaModeTable->tv2Table[tvIndx].TVPALS;
		break;
	    case VIA_TVOVER:
		if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
		    TV = pViaModeTable->tv2OverTable[tvIndx].TVPALC;
		else
		    TV = pViaModeTable->tv2OverTable[tvIndx].TVPALS;
		break;
	    }
	} else {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
		    TV = pViaModeTable->tv2Table[tvIndx].TVNTSCC;
		else
		    TV = pViaModeTable->tv2Table[tvIndx].TVNTSCS;
		break;
	    case VIA_TVOVER:
		if (pBIOSInfo->TVOutput == TVOUTPUT_COMPOSITE)
		    TV = pViaModeTable->tv2OverTable[tvIndx].TVNTSCC;
		else
		    TV = pViaModeTable->tv2OverTable[tvIndx].TVNTSCS;
		break;
	    }
	}
	break;
    case VIA_TV3:
	if (pBIOSInfo->TVType == TVTYPE_PAL) {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->tv3Table[tvIndx].TVPAL;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->tv3OverTable[tvIndx].TVPAL;
		break;
	    }
	} else {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->tv3Table[tvIndx].TVNTSC;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->tv3OverTable[tvIndx].TVNTSC;
		break;
	    }
	}
	break;
    case VIA_VT1622A:
    case VIA_VT1623:
	if (pBIOSInfo->TVType == TVTYPE_PAL) {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->vt1622aTable[tvIndx].TVPAL;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->vt1622aOverTable[tvIndx].TVPAL;
		break;
	    }
	} else {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->vt1622aTable[tvIndx].TVNTSC;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->vt1622aOverTable[tvIndx].TVNTSC;
		break;
	    }
	}
	break;
    case VIA_CH7009:
    case VIA_CH7019:
	if (pBIOSInfo->TVType == TVTYPE_PAL) {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->ch7019Table[tvIndx].TVPAL;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->ch7019OverTable[tvIndx].TVPAL;
		break;
	    }
	} else {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->ch7019Table[tvIndx].TVNTSC;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->ch7019OverTable[tvIndx].TVNTSC;
		break;
	    }
	}
	break;
    case VIA_SAA7108:
	if (pBIOSInfo->TVType == TVTYPE_PAL) {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->saa7108Table[tvIndx].TVPAL;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->saa7108OverTable[tvIndx].TVPAL;
		break;
	    }
	} else {
	    switch (pBIOSInfo->TVVScan) {
	    case VIA_TVNORMAL:
		TV = pViaModeTable->saa7108Table[tvIndx].TVNTSC;
		break;
	    case VIA_TVOVER:
		TV = pViaModeTable->saa7108OverTable[tvIndx].TVNTSC;
		break;
	    }
	}
	break;
	/*case VIA_FS454:
	   if (pBIOSInfo->TVType == TVTYPE_PAL) {
	   switch (pBIOSInfo->TVVScan) {
	   case VIA_TVNORMAL:
	   TV = pViaModeTable->fs454Table[tvIndx].TVPAL;
	   break;
	   case VIA_TVOVER:
	   TV = pViaModeTable->fs454OverTable[tvIndx].TVPAL;
	   break;
	   }
	   }
	   else {
	   switch (pBIOSInfo->TVVScan) {
	   case VIA_TVNORMAL:
	   TV = pViaModeTable->fs454Table[tvIndx].TVNTSC;
	   break;
	   case VIA_TVOVER:
	   TV = pViaModeTable->fs454OverTable[tvIndx].TVNTSC;
	   break;
	   }
	   }
	   break; */
    }

    InParam = buf;
    dwFunc = *((CARD32 *) InParam);
    InParam += 4;

    dwAction = *((CARD32 *) InParam);
    InParam += 4;

    switch (dwFunc) {
    case UT_XV_FUNC_BIOS:
	switch (dwAction) {
	case UT_XV_FUNC_BIOS_GetChipID:
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &pVia->ChipId, sizeof(CARD32));
	    InParam += 4;
	    break;
	case UT_XV_FUNC_BIOS_GetVersion:
	    pUTBIOSVERSION.dwVersion = pBIOSInfo->BIOSMajorVersion;
	    pUTBIOSVERSION.dwRevision = pBIOSInfo->BIOSMinorVersion;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(long));
	    InParam += 4;
	    memcpy((void *) InParam, &pUTBIOSVERSION, sizeof(UTBIOSVERSION));
	    break;
	case UT_XV_FUNC_BIOS_GetDate:
	    if (!pBIOSInfo->BIOSDateYear)
		VIABIOS_GetBIOSDate(pScrn);

	    pUTBIOSDATE.dwYear = (CARD32) pBIOSInfo->BIOSDateYear + 2000;
	    pUTBIOSDATE.dwMonth = (CARD32) pBIOSInfo->BIOSDateMonth;
	    pUTBIOSDATE.dwDay = (CARD32) pBIOSInfo->BIOSDateDay;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(long));
	    InParam += 4;
	    memcpy((void *) InParam, &pUTBIOSDATE, sizeof(UTBIOSDATE));
	    break;
	case UT_XV_FUNC_BIOS_GetVideoMemSizeMB:
	    dwVideoRam = pScrn->videoRam >> 10;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwVideoRam, sizeof(CARD32));
	    break;
	default:
	    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    InParam += 4;
	    ErrorF(" via_utility.c : dwAction not supported\n");
	    break;
	}
	break;
    case UT_XV_FUNC_DEVICE:
	switch (dwAction) {
	case UT_XV_FUNC_DEVICE_GetSupportState:
	    dwSupportState = UT_DEVICE_CRT1;
	    if (pBIOSInfo->TVEncoder)
		dwSupportState |= UT_DEVICE_TV;
	    if (pBIOSInfo->TMDS) {
		dwSupportState |= UT_DEVICE_DFP;
	    }
	    if (pBIOSInfo->LVDS) {
		dwSupportState |= UT_DEVICE_LCD;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_GetConnectState:
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    dwConnectState = VIAGetDeviceDetect(pBIOSInfo);
	    memcpy((void *) InParam, &dwConnectState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_GetActiveState:
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    if (pBIOSInfo->HasSecondary)	/* if SAMM */
		dwActiveState = pVia->ActiveDevice;
	    else
		dwActiveState = pBIOSInfo->ActiveDevice;
	    memcpy((void *) InParam, &dwActiveState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_SetActiveState:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    /* DuoView not implement yet, we do nothing */
	    if ((dwInData == (VIA_DEVICE_TV | VIA_DEVICE_LCD)) || (dwInData == (VIA_DEVICE_TV | VIA_DEVICE_DFP))) {
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    }
	    pBIOSInfo->ActiveDevice = (unsigned char) dwInData;
	    /* Avoid device switch in panning mode or expansion */
	    pBIOSInfo->scaleY = FALSE;
	    pScrn->frameX1 = pBIOSInfo->SaveframeX1;
	    pScrn->frameY1 = pBIOSInfo->SaveframeY1;
	    pScrn->currentMode->HDisplay = pBIOSInfo->SaveHDisplay;
	    pScrn->currentMode->VDisplay = pBIOSInfo->SaveVDisplay;
	    pScrn->currentMode->CrtcHDisplay = pBIOSInfo->SaveCrtcHDisplay;
	    pScrn->currentMode->CrtcVDisplay = pBIOSInfo->SaveCrtcVDisplay;

	    /*BIOS_SetActiveDevice(pScrn); */
	    VIASwitchMode(0, pScrn->currentMode, 0);
	    InParam = buf + 8;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_GetSAMMState:
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    if (!pBIOSInfo->HasSecondary)
		dwSAMMState = UT_STATE_SAMM_OFF;
	    else
		dwSAMMState = UT_STATE_SAMM_ON;
	    memcpy((void *) InParam, &dwSAMMState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_GetRoateState:
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    dwRotateState = UT_ROTATE_NONE;
	    memcpy((void *) InParam, &dwRotateState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_SetRoateState:
	    memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
	    break;
	default:
	    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    ErrorF(" via_utility.c : dwAction not supported\n");
	    break;
	}
	break;
    case UT_XV_FUNC_PANEL:
	switch (dwAction) {
	case UT_XV_FUNC_DEVICE_SetTargetPanel:
	    memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_GetPanelInfo:
	    pUTPANELINFO.dwType = UT_PANEL_TYPE_TFT;
	    pUTPANELINFO.dwResX = pBIOSInfo->panelX;
	    pUTPANELINFO.dwResY = pBIOSInfo->panelY;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &pUTPANELINFO, sizeof(UTPANELINFO));
	    break;
	case UT_XV_FUNC_DEVICE_GetExpandState:
	    if (pBIOSInfo->Center)
		dwExpandState = UT_STATE_EXPAND_OFF;
	    else
		dwExpandState = UT_STATE_EXPAND_ON;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwExpandState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_DEVICE_SetExpandState:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    if (dwInData == UT_STATE_EXPAND_OFF) {
		pBIOSInfo->Center = TRUE;
		VIASwitchMode(0, pScrn->currentMode, 0);
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    } else if (dwInData == UT_STATE_EXPAND_ON) {
		pBIOSInfo->Center = FALSE;
		VIASwitchMode(0, pScrn->currentMode, 0);
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    } else {
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    }
	    break;
	case UT_XV_FUNC_DEVICE_GetSupportExpand:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    if (pBIOSInfo->LVDS) {
		switch (pBIOSInfo->PanelSize) {
		case VIA_PANEL6X4:
		    if (pBIOSInfo->SaveHDisplay < 640 && pBIOSInfo->SaveVDisplay < 480) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL8X6:
		    if (pBIOSInfo->SaveHDisplay < 800 && pBIOSInfo->SaveVDisplay < 600) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL10X7:
		    if (pBIOSInfo->SaveHDisplay < 1024 && pBIOSInfo->SaveVDisplay < 768) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL12X7:
		    if (pBIOSInfo->SaveHDisplay < 1280 && pBIOSInfo->SaveVDisplay < 768) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL12X10:
		    if (pBIOSInfo->SaveHDisplay < 1280 && pBIOSInfo->SaveVDisplay < 1024) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL14X10:
		    if (pBIOSInfo->SaveHDisplay < 1400 && pBIOSInfo->SaveVDisplay < 1050) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		case VIA_PANEL16X12:
		    if (pBIOSInfo->SaveHDisplay < 1600 && pBIOSInfo->SaveVDisplay < 1200) {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_ON;
		    } else {
			dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    }
		    break;
		default:
		    dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
		    break;
		}
	    } else {
		dwSupportState = UT_PANEL_SUPPORT_EXPAND_OFF;
	    }
	    InParam = buf + 8;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(CARD32));
	    break;
	default:
	    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    InParam += 4;
	    ErrorF(" via_utility.c : dwAction not supported\n");
	    break;
	}
	break;
    case UT_XV_FUNC_TV:
	switch (dwAction) {
	case UT_XV_FUNC_TV_GetSupportStandardState:
	    dwSupportState = UT_TV_STD_NTSC | UT_TV_STD_PAL;
	    if (pBIOSInfo->TVEncoder == VIA_FS454)
		dwSupportState = UT_TV_STD_NTSC;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_GetStandard:
	    dwStandard = pBIOSInfo->TVType;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwStandard, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_SetStandard:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    switch (dwInData) {
	    case UT_TV_STD_NTSC:
		pBIOSInfo->TVType = TVTYPE_NTSC;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_STD_PAL:
		pBIOSInfo->TVType = TVTYPE_PAL;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		break;
	    }
	    break;
	case UT_XV_FUNC_TV_GetSupportSignalTypeState:
	    VIAGetActiveDisplay(pBIOSInfo);
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
		dwSupportState = UT_TV_SGNL_COMPOSITE | UT_TV_SGNL_S_VIDEO;
		break;
	    case VIA_TV3:
	    case VIA_VT1622A:
		dwSupportState = UT_TV_SGNL_COMPOSITE | UT_TV_SGNL_S_VIDEO | UT_TV_SGNL_SCART | UT_TV_SGNL_COMP_OUTPUT;
		break;
	    case VIA_SAA7108:
		dwSupportState = UT_TV_SGNL_COMPOSITE | UT_TV_SGNL_S_VIDEO | UT_TV_SGNL_SCART | UT_TV_SGNL_COMP_OUTPUT;
		break;
	    case VIA_FS454:
		dwSupportState = UT_TV_SGNL_COMPOSITE | UT_TV_SGNL_S_VIDEO | UT_TV_SGNL_SCART;
		break;
	    case VIA_CH7009:
	    case VIA_CH7019:
		dwSupportState = UT_TV_SGNL_COMPOSITE | UT_TV_SGNL_S_VIDEO;
		break;
	    default:
		dwSupportState = 0;
		break;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_GetSignalType:
	    dwSignalType = pBIOSInfo->TVOutput;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSignalType, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_SetSignalType:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    switch (dwInData) {
	    case UT_TV_SGNL_COMPOSITE:
		pBIOSInfo->TVOutput = TVOUTPUT_COMPOSITE;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SGNL_S_VIDEO:
		pBIOSInfo->TVOutput = TVOUTPUT_SVIDEO;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SGNL_SCART:
		pBIOSInfo->TVOutput = TVOUTPUT_RGB;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SGNL_COMP_OUTPUT:
		pBIOSInfo->TVOutput = TVOUTPUT_YCBCR;
		if (pBIOSInfo->ActiveDevice & VIA_DEVICE_TV) {
		    VIASwitchMode(0, pScrn->currentMode, 0);
		}
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		break;
	    }
	    break;
	case UT_XV_FUNC_TV_GetMaxViewSizeValue:
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
	    case VIA_TV3:
	    case VIA_VT1622A:
	    case VIA_VT1623:
	    case VIA_SAA7108:
	    case VIA_FS454:
	    case VIA_CH7009:
	    case VIA_CH7019:
		MaxViewSizeValue.dwX = 0;
		MaxViewSizeValue.dwY = 2;
		break;
	    default:
		MaxViewSizeValue.dwX = 0;
		MaxViewSizeValue.dwY = 0;
		break;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &MaxViewSizeValue, sizeof(UTXYVALUE));
	    break;
	case UT_XV_FUNC_TV_GetViewSizeValue:
	    ViewSizeValue.dwX = 0;
	    ViewSizeValue.dwY = pBIOSInfo->TVVScan + 1;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &ViewSizeValue, sizeof(UTXYVALUE));
	    break;
	case UT_XV_FUNC_TV_SetViewSizeValue:
	    InParam = buf + 12;
	    memcpy(&ViewSizeValue, (void *) InParam, sizeof(UTXYVALUE));
	    if (ViewSizeValue.dwY == 0xFFFF) {
		pBIOSInfo->TVVScan = VIA_TVNORMAL;
	    } else {
		pBIOSInfo->TVVScan = (int) ViewSizeValue.dwY - 1;
	    }
	    VIASwitchMode(0, pScrn->currentMode, 0);
	    InParam = buf + 8;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    break;

	case UT_XV_FUNC_TV_GetMaxViewPositionValue:
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
		MaxViewPosValue.dwX = 0;
		MaxViewPosValue.dwY = 0;
		break;
	    case VIA_TV3:
	    case VIA_VT1622A:
	    case VIA_VT1623:
		MaxViewPosValue.dwX = 11;	/* 9 bit, [0:7]08H, [2]1CH */
		MaxViewPosValue.dwY = 11;	/* 9 bit, [0:7]09H, [1]1CH */
		break;
	    case VIA_SAA7108:
		MaxViewPosValue.dwX = 11;
		MaxViewPosValue.dwY = 11;
		break;
	    case VIA_FS454:
	    case VIA_CH7009:
	    case VIA_CH7019:
	    default:
		MaxViewPosValue.dwX = 0;
		MaxViewPosValue.dwY = 0;
		break;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &MaxViewPosValue, sizeof(UTXYVALUE));
	    break;
	case UT_XV_FUNC_TV_GetViewPositionValue:
	    InParam = buf + 12;
	    memcpy(&ViewPosValue, (void *) InParam, sizeof(UTXYVALUE));
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
		ViewPosValue.dwX = 0;
		ViewPosValue.dwY = 0;
		break;
	    case VIA_TV3:
	    case VIA_VT1622A:
	    case VIA_VT1623:
		if (ViewPosValue.dwX == 0xFFFF && ViewPosValue.dwY == 0xFFFF) {
		    /*value = TV[0x1C];
		       ViewPosValue.dwX = (CARD32)(TV[0x08] & 0xFF);
		       ViewPosValue.dwX |= (CARD32)((value & 0x04) << 6);
		       ViewPosValue.dwY = (CARD32)(TV[0x09] & 0xFF);
		       ViewPosValue.dwY |= (CARD32)((value & 0x02) << 7); */
		    ViewPosValue.dwX = 6;
		    ViewPosValue.dwY = 6;
		} else {
		    ViewPosValue.dwX = pUserSetting->tvHPosition;
		    ViewPosValue.dwY = pUserSetting->tvVPosition;
		}
		break;
	    case VIA_SAA7108:
		if (ViewPosValue.dwX == 0xFFFF && ViewPosValue.dwY == 0xFFFF) {
		    ViewPosValue.dwX = 6;
		    ViewPosValue.dwY = 6;
		} else {
		    ViewPosValue.dwX = pUserSetting->tvHPosition;
		    ViewPosValue.dwY = pUserSetting->tvVPosition;
		}
		break;
	    case VIA_FS454:
	    case VIA_CH7009:
	    case VIA_CH7019:
	    default:
		ViewPosValue.dwX = 0;
		ViewPosValue.dwY = 0;
		break;
	    }
	    InParam = buf + 8;
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &ViewPosValue, sizeof(UTXYVALUE));
	    break;
	case UT_XV_FUNC_TV_SetViewPositionValue:
	    InParam = buf + 12;
	    memcpy(&ViewPosValue, (void *) InParam, sizeof(UTXYVALUE));
	    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		dev = xf86CreateI2CDevRec();
		dev->DevName = "TV";
		dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		dev->pI2CBus = pBIOSInfo->I2C_Port2;

		if (xf86I2CDevInit(dev)) {
		    switch (pBIOSInfo->TVEncoder) {
		    case VIA_TV2PLUS:
			pUserSetting->tvHPosition = 0;
			pUserSetting->tvVPosition = 0;
			break;
		    case VIA_TV3:
		    case VIA_VT1622A:
			if (ViewPosValue.dwX == 0xFFFF && ViewPosValue.dwX == 0xFFFF) {
			    W_Buffer[0] = 0x1C;
			    W_Buffer[1] = TV[0x1C];
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    W_Buffer[0] = 0x08;
			    W_Buffer[1] = TV[0x08];
			    W_Buffer[2] = TV[0x09];
			    xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);
			    /*value = TV[0x1C];
			       pUserSetting->tvHPosition = TV[0x08] & 0xFF;
			       pUserSetting->tvHPosition |= (value & 0x04) << 6;
			       pUserSetting->tvVPosition = TV[0x09] & 0xFF;
			       pUserSetting->tvVPosition |= (value & 0x02) << 7; */
			    pUserSetting->tvHPosition = 6;
			    pUserSetting->tvVPosition = 6;
			} else {
			    W_Buffer[0] = 0x08;
			    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 2);
			    HPos = R_Buffer[0];
			    VPos = R_Buffer[1];
			    W_Buffer[0] = 0x1C;
			    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
			    HPos |= (R_Buffer[0] & 0x04) << 6;
			    VPos |= (R_Buffer[0] & 0x02) << 7;

			    HPos += ViewPosValue.dwX - pUserSetting->tvHPosition;
			    VPos += ViewPosValue.dwY - pUserSetting->tvVPosition;

			    W_Buffer[0] = 0x08;
			    W_Buffer[1] = HPos & 0xFF;
			    W_Buffer[2] = VPos & 0xFF;
			    xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);

			    W_Buffer[0] = 0x1C;
			    W_Buffer[1] = R_Buffer[0] & ~0x06;	/* [2]1CH, [1]1CH */
			    W_Buffer[1] |= (HPos >> 6) & 0x04;
			    W_Buffer[1] |= (VPos >> 7) & 0x02;
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);

			    pUserSetting->tvHPosition = ViewPosValue.dwX;
			    pUserSetting->tvVPosition = ViewPosValue.dwY;
			}
			break;
		    case VIA_SAA7108:
			if (ViewPosValue.dwX == 0xFFFF && ViewPosValue.dwX == 0xFFFF) {
			    for (i = 0; i < 3; i++) {
				W_Buffer[0] = VIASAA7108PostionOffset[i];
				if (pBIOSInfo->TVVScan == VIA_TVNORMAL)
				    W_Buffer[1] = VIASAA7108PostionNormalTabRec[pBIOSInfo->TVType - 1][tvIndx][5][i];
				else
				    W_Buffer[1] = VIASAA7108PostionOverTabRec[pBIOSInfo->TVType - 1][tvIndx][5][i];
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    }
			    W_Buffer[0] = 0x72;
			    W_Buffer[1] = TV[0x72];
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    W_Buffer[0] = 0x70;
			    W_Buffer[1] = TV[0x70];
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    W_Buffer[0] = 0x72;
			    W_Buffer[1] = TV[0x72];
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    W_Buffer[0] = 0x71;
			    W_Buffer[1] = TV[0x71];
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    pUserSetting->tvHPosition = 6;
			    pUserSetting->tvVPosition = 6;
			} else {
			    for (i = 0; i < 3; i++) {
				W_Buffer[0] = VIASAA7108PostionOffset[i];
				if (pBIOSInfo->TVVScan == VIA_TVNORMAL)
				    W_Buffer[1] = VIASAA7108PostionNormalTabRec[pBIOSInfo->TVType - 1][tvIndx][ViewPosValue.dwY][i];
				else
				    W_Buffer[1] = VIASAA7108PostionOverTabRec[pBIOSInfo->TVType - 1][tvIndx][ViewPosValue.dwY][i];
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    }
			    W_Buffer[0] = 0x70;
			    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 3);
			    ADWHS = R_Buffer[0] | ((R_Buffer[2] & 0x07) << 8);
			    ADWHE = R_Buffer[1] | ((R_Buffer[2] & 0x70) << 4);
			    switch (ViewPosValue.dwX - pUserSetting->tvHPosition) {
			    case 1:	/* Moving Right by 1 unit */
				ADWHS++;
				ADWHE++;
				W_Buffer[0] = 0x72;
				W_Buffer[1] = (R_Buffer[2] & ~0x77) | ((ADWHS & 0x700) >> 8) | ((ADWHE & 0x700) >> 4);
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x70;
				W_Buffer[1] = ADWHS & 0xFF;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x72;
				W_Buffer[1] = (R_Buffer[2] & ~0x77) | ((ADWHS & 0x700) >> 8) | ((ADWHE & 0x700) >> 4);
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x71;
				W_Buffer[1] = ADWHE & 0xFF;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				break;
			    case -1:	/* Moving Left by 1 unit */
				ADWHS--;
				ADWHE--;
				W_Buffer[0] = 0x72;
				W_Buffer[1] = (R_Buffer[2] & ~0x77) | ((ADWHS & 0x700) >> 8) | ((ADWHE & 0x700) >> 4);
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x71;
				W_Buffer[1] = ADWHE & 0xFF;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x72;
				W_Buffer[1] = (R_Buffer[2] & ~0x77) | ((ADWHS & 0x700) >> 8) | ((ADWHE & 0x700) >> 4);
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x70;
				W_Buffer[1] = ADWHS & 0xFF;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				break;
			    default:
				break;
			    }
			    pUserSetting->tvHPosition = ViewPosValue.dwX;
			    pUserSetting->tvVPosition = ViewPosValue.dwY;
			}
			break;
		    case VIA_FS454:
		    case VIA_CH7009:
		    case VIA_CH7019:
		    default:
			pUserSetting->tvHPosition = 0;
			pUserSetting->tvVPosition = 0;
			break;
		    }
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
	    }
	    break;
	case UT_XV_FUNC_TV_GetSupportTuningState:
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
		dwSupportState = UT_TV_TUNING_FFILTER | UT_TV_TUNING_BRIGHTNESS | UT_TV_TUNING_CONTRAST | UT_TV_TUNING_SATURATION | UT_TV_TUNING_TINT;
		break;
	    case VIA_TV3:
	    case VIA_VT1622A:
	    case VIA_VT1623:
		dwSupportState = UT_TV_TUNING_FFILTER | UT_TV_TUNING_ADAPTIVE_FFILTER | UT_TV_TUNING_BRIGHTNESS | UT_TV_TUNING_CONTRAST | UT_TV_TUNING_SATURATION | UT_TV_TUNING_TINT;
		break;
	    case VIA_SAA7108:
		dwSupportState = UT_TV_TUNING_FFILTER;
		break;
	    case VIA_CH7009:
	    case VIA_CH7019:
		dwSupportState = UT_TV_TUNING_FFILTER | UT_TV_TUNING_ADAPTIVE_FFILTER | UT_TV_TUNING_BRIGHTNESS | UT_TV_TUNING_CONTRAST;
		break;
	    case VIA_FS454:
	    default:
		dwSupportState = 0;
		break;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(UTXYVALUE));
	    break;
	case UT_XV_FUNC_TV_GetTuningItemMaxValue:	/* 0 BASE */
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    switch (dwInData) {
	    case UT_TV_TUNING_FFILTER:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 0;
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 3;	/* 2bit, [1:0]03H */
		    break;
		case VIA_CH7009:
		case VIA_CH7019:
		case VIA_SAA7108:
		    dwMaxValue = 3;
		    break;
		case VIA_FS454:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    case UT_TV_TUNING_ADAPTIVE_FFILTER:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 0;	/* TV2Plus No Adaptive Flicker */
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 255;	/* 8bit, [7:0]61H */
		    break;
		case VIA_SAA7108:
		case VIA_FS454:
		case VIA_CH7009:
		case VIA_CH7019:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    case UT_TV_TUNING_BRIGHTNESS:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 0;
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 255;	/* 8bit, [7:0]0BH */
		    break;
		case VIA_SAA7108:
		case VIA_FS454:
		case VIA_CH7009:
		case VIA_CH7019:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    case UT_TV_TUNING_CONTRAST:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 0;
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 255;	/* 8bit, [7:0]0CH */
		    break;
		case VIA_SAA7108:
		case VIA_FS454:
		case VIA_CH7009:
		case VIA_CH7019:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    case UT_TV_TUNING_SATURATION:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 0;
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 65535;	/* 16bit, [7:0]0AH, [7:0]0DH */
		    break;
		case VIA_SAA7108:
		case VIA_FS454:
		case VIA_CH7009:
		case VIA_CH7019:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    case UT_TV_TUNING_TINT:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		switch (pBIOSInfo->TVEncoder) {
		case VIA_TV2PLUS:
		    dwMaxValue = 2047;	/* 11bit, [7:0]10H, [7:5]11H */
		    break;
		case VIA_TV3:
		case VIA_VT1622A:
		case VIA_VT1623:
		    dwMaxValue = 2047;	/* 11bit, [7:0]10H, [2:0]11H */
		    break;
		case VIA_SAA7108:
		case VIA_FS454:
		case VIA_CH7009:
		case VIA_CH7019:
		default:
		    dwMaxValue = 0;
		    break;
		}
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		dwMaxValue = 0;
		break;
	    }
	    InParam += 4;
	    memcpy((void *) InParam, &dwMaxValue, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_GetTuningItemValue:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    switch (dwInData) {
	    case UT_TV_TUNING_FFILTER:
		dwValue = pUserSetting->tvFFilter;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_TUNING_ADAPTIVE_FFILTER:
		dwValue = pUserSetting->tvAdaptiveFFilter;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_TUNING_BRIGHTNESS:
		dwValue = pUserSetting->tvBrightness;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_TUNING_CONTRAST:
		dwValue = pUserSetting->tvContrast;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_TUNING_SATURATION:
		dwValue = pUserSetting->tvSaturation;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_TUNING_TINT:
		dwValue = pUserSetting->tvTint;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		dwValue = 0;
		break;
	    }
	    InParam += 4;
	    memcpy((void *) InParam, &dwValue, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_SetTuningItemValue:
	    InParam = buf + 12;
	    memcpy(&pUTSETTVTUNINGINFO, (void *) InParam, sizeof(UTSETTVTUNINGINFO));
	    switch (pUTSETTVTUNINGINFO.dwItemID) {
	    case UT_TV_TUNING_FFILTER:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    pUserSetting->tvFFilter = 0;
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x03;
				W_Buffer[1] = TV[0x03];
				pUserSetting->tvFFilter = TV[0x03] & 0x03;
				if (pUserSetting->tvFFilter == 0)
				    pUserSetting->AdaptiveFilterOn = TRUE;
			    } else {
				W_Buffer[0] = 0x03;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x03;
				W_Buffer[1] |= pUTSETTVTUNINGINFO.dwValue;
				pUserSetting->tvFFilter = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    break;
			case VIA_SAA7108:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x37;
				W_Buffer[1] = TV[0x37];
				pUserSetting->tvFFilter = (TV[0x37] & 0x30) + 1;
			    } else {
				W_Buffer[0] = 0x37;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x30;
				W_Buffer[1] |= (unsigned char) (pUTSETTVTUNINGINFO.dwValue - 1);
				pUserSetting->tvFFilter = pUTSETTVTUNINGINFO.dwValue;
			    }
			    break;
			case VIA_CH7009:
			case VIA_CH7019:
			case VIA_FS454:
			default:
			    pUserSetting->tvFFilter = 0;
			    break;
			}

			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_TUNING_ADAPTIVE_FFILTER:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    pUserSetting->tvAdaptiveFFilter = 0;
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x61;
				W_Buffer[1] = TV[0x61];
				pUserSetting->tvAdaptiveFFilter = TV[0x61] & 0xFF;
			    } else {
				W_Buffer[0] = 0x61;
				W_Buffer[1] = (unsigned char) pUTSETTVTUNINGINFO.dwValue;
				pUserSetting->tvAdaptiveFFilter = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    break;
			case VIA_SAA7108:
			case VIA_FS454:
			case VIA_CH7009:
			case VIA_CH7019:
			default:
			    pUserSetting->tvAdaptiveFFilter = 0;
			    break;
			}
			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_TUNING_BRIGHTNESS:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    pUserSetting->tvBrightness = 0;
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    W_Buffer[0] = 0x0B;
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[1] = TV[0x0B];
				pUserSetting->tvBrightness = TV[0x0B] & 0xFF;
			    } else {
				W_Buffer[1] = (unsigned char) pUTSETTVTUNINGINFO.dwValue;
				pUserSetting->tvBrightness = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    break;
			case VIA_SAA7108:
			case VIA_FS454:
			case VIA_CH7009:
			case VIA_CH7019:
			default:
			    pUserSetting->tvBrightness = 0;
			    break;
			}

			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_TUNING_CONTRAST:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    pUserSetting->tvContrast = 0;
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    W_Buffer[0] = 0x0C;
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[1] = TV[0x0C];
				pUserSetting->tvContrast = TV[0x0C] & 0xFF;
			    } else {
				W_Buffer[1] = (unsigned char) pUTSETTVTUNINGINFO.dwValue;
				pUserSetting->tvContrast = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    break;
			case VIA_SAA7108:
			case VIA_FS454:
			case VIA_CH7009:
			case VIA_CH7019:
			default:
			    pUserSetting->tvContrast = 0;
			    break;
			}

			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_TUNING_SATURATION:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    pUserSetting->tvSaturation = 0;
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x0D;
				W_Buffer[1] = TV[0x0D];
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x0A;
				W_Buffer[1] = TV[0x0A];
				value = TV[0x0D];
				pUserSetting->tvSaturation = TV[0x0A] & 0xFF;
				pUserSetting->tvSaturation |= value << 8;
			    } else {
				W_Buffer[0] = 0x0D;
				W_Buffer[1] = (unsigned char) ((pUTSETTVTUNINGINFO.dwValue >> 8) & 0xFF);
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x0A;
				W_Buffer[1] = (unsigned char) (pUTSETTVTUNINGINFO.dwValue & 0xFF);
				pUserSetting->tvSaturation = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);
			    break;
			case VIA_SAA7108:
			case VIA_FS454:
			case VIA_CH7009:
			case VIA_CH7019:
			default:
			    pUserSetting->tvSaturation = 0;
			    break;
			}

			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_TUNING_TINT:
		if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
		    dev = xf86CreateI2CDevRec();
		    dev->DevName = "TV";
		    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
		    dev->pI2CBus = pBIOSInfo->I2C_Port2;

		    if (xf86I2CDevInit(dev)) {
			switch (pBIOSInfo->TVEncoder) {
			case VIA_TV2PLUS:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x10;
				W_Buffer[1] = TV[0x10];
				W_Buffer[2] = TV[0x11];
				pUserSetting->tvTint = TV[0x10] & 0xFF;
				pUserSetting->tvTint |= (TV[0x11] & 0xE0) << 3;
			    } else {
				W_Buffer[0] = 0x11;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[2] = R_Buffer[0] & ~0xE0;
				W_Buffer[2] |= (unsigned char) ((pUTSETTVTUNINGINFO.dwValue >> 3) & 0xFF);
				W_Buffer[0] = 0x10;
				W_Buffer[1] = (unsigned char) (pUTSETTVTUNINGINFO.dwValue & 0xFF);
				pUserSetting->tvTint = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);
			    break;
			case VIA_TV3:
			case VIA_VT1622A:
			case VIA_VT1623:
			    if (pUTSETTVTUNINGINFO.dwValue == 0xFFFF) {
				W_Buffer[0] = 0x10;
				W_Buffer[1] = TV[0x10];
				W_Buffer[2] = TV[0x11];
				value = TV[0x11];
				pUserSetting->tvTint = TV[0x10] & 0xFF;
				pUserSetting->tvTint |= TV[0x11] << 8;
			    } else {
				W_Buffer[0] = 0x11;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[2] = R_Buffer[0] & ~0x07;
				W_Buffer[2] |= (unsigned char) (pUTSETTVTUNINGINFO.dwValue >> 8) & 0xFF;
				W_Buffer[0] = 0x10;
				W_Buffer[1] = (unsigned char) (pUTSETTVTUNINGINFO.dwValue & 0xFF);
				pUserSetting->tvTint = pUTSETTVTUNINGINFO.dwValue;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);
			    break;
			case VIA_SAA7108:
			case VIA_FS454:
			case VIA_CH7009:
			case VIA_CH7019:
			default:
			    pUserSetting->tvTint = 0;
			    break;
			}

			xf86DestroyI2CDevRec(dev, TRUE);
		    } else {
			xf86DestroyI2CDevRec(dev, TRUE);
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    xf86DestroyI2CDevRec(dev, TRUE);
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		break;
	    }
	    break;
	case UT_XV_FUNC_TV_GetSupportSettingState:
	    dwItemID |= UT_TV_SETTING_FFILTER;
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV3:
	    case VIA_VT1622A:
	    case VIA_VT1623:
		dwItemID |= UT_TV_SETTING_ADAPTIVE_FFILTER;
		if (pBIOSInfo->TVType == TVTYPE_NTSC)
		    dwItemID |= UT_TV_SETTING_DOT_CRAWL;
		break;
	    case VIA_FS454:
	    case VIA_CH7009:
	    case VIA_CH7019:
		if (pBIOSInfo->TVType == TVTYPE_NTSC)
		    dwItemID |= UT_TV_SETTING_DOT_CRAWL;
		break;
	    default:
		break;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwItemID, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_GetSettingItemState:
	    InParam += 4;
	    dwInData = *((CARD32 *) InParam);
	    switch (dwInData) {
	    case UT_TV_SETTING_FFILTER:
		if (pUserSetting->tvFFilter)
		    dwState = UT_STATE_ON;
		else
		    dwState = UT_STATE_OFF;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SETTING_ADAPTIVE_FFILTER:
		if (pUserSetting->tvFFilter)
		    dwState = UT_STATE_OFF;
		else
		    dwState = UT_STATE_ON;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SETTING_DOT_CRAWL:
		if (pBIOSInfo->TVDotCrawl)
		    dwState = UT_STATE_ON;
		else
		    dwState = UT_STATE_OFF;
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		break;
	    case UT_TV_SETTING_LOCK_ASPECT_RATIO:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
		dwState = UT_STATE_OFF;
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		dwState = UT_STATE_OFF;
		break;
	    }
	    InParam += 4;
	    memcpy((void *) InParam, &dwState, sizeof(CARD32));
	    break;
	case UT_XV_FUNC_TV_SetSettingItemState:
	    InParam = buf + 12;
	    memcpy(&pUTSETTVITEMSTATE, (void *) InParam, sizeof(UTSETTVITEMSTATE));
	    switch (pUTSETTVITEMSTATE.dwItemID) {
	    case UT_TV_SETTING_FFILTER:
		if (pUTSETTVITEMSTATE.dwState == UT_STATE_OFF) {
		    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
			dev = xf86CreateI2CDevRec();
			dev->DevName = "TV";
			dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
			dev->pI2CBus = pBIOSInfo->I2C_Port2;

			if (xf86I2CDevInit(dev)) {
			    switch (pBIOSInfo->TVEncoder) {
			    case VIA_TV2PLUS:
			    case VIA_TV3:
			    case VIA_VT1622A:
			    case VIA_VT1623:
				W_Buffer[0] = 0x03;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x03;
				break;
			    case VIA_SAA7108:
				W_Buffer[0] = 0x37;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x30;
				break;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    xf86DestroyI2CDevRec(dev, TRUE);
			    pUserSetting->AdaptiveFilterOn = TRUE;
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));

			} else {
			    xf86DestroyI2CDevRec(dev, TRUE);
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
			}
		    }
		} else if (pUTSETTVITEMSTATE.dwState == UT_STATE_ON) {
		    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
			dev = xf86CreateI2CDevRec();
			dev->DevName = "TV";
			dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
			dev->pI2CBus = pBIOSInfo->I2C_Port2;

			if (xf86I2CDevInit(dev)) {
			    switch (pBIOSInfo->TVEncoder) {
			    case VIA_TV2PLUS:
			    case VIA_TV3:
			    case VIA_VT1622A:
			    case VIA_VT1623:
				W_Buffer[0] = 0x03;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x03;
				W_Buffer[1] |= (unsigned char) (pUserSetting->tvFFilter - 1);
				break;
			    case VIA_SAA7108:
				W_Buffer[0] = 0x37;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x30;
				W_Buffer[1] |= (unsigned char) ((pUserSetting->tvFFilter - 1) << 4);
				break;
			    }
			    xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
			    xf86DestroyI2CDevRec(dev, TRUE);
			    pUserSetting->AdaptiveFilterOn = FALSE;
			} else {
			    xf86DestroyI2CDevRec(dev, TRUE);
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
			}
		    }
		} else {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_SETTING_ADAPTIVE_FFILTER:
		if (pUTSETTVITEMSTATE.dwState == UT_STATE_OFF) {
		    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
			dev = xf86CreateI2CDevRec();
			dev->DevName = "TV";
			dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
			dev->pI2CBus = pBIOSInfo->I2C_Port2;

			if (xf86I2CDevInit(dev)) {
			    switch (pBIOSInfo->TVEncoder) {
			    case VIA_TV2PLUS:
				pUserSetting->AdaptiveFilterOn = FALSE;
				break;
			    case VIA_TV3:
			    case VIA_VT1622A:
			    case VIA_VT1623:
				W_Buffer[0] = 0x61;
				W_Buffer[1] = 0x00;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				xf86DestroyI2CDevRec(dev, TRUE);
				pUserSetting->AdaptiveFilterOn = FALSE;
				break;
			    case VIA_SAA7108:
			    case VIA_FS454:
			    case VIA_CH7009:
			    case VIA_CH7019:
			    default:
				pUserSetting->AdaptiveFilterOn = FALSE;
				break;
			    }
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
			} else {
			    xf86DestroyI2CDevRec(dev, TRUE);
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
			}
		    }
		} else if (pUTSETTVITEMSTATE.dwState == UT_STATE_ON) {
		    if (xf86I2CProbeAddress(pBIOSInfo->I2C_Port2, pBIOSInfo->TVI2CAdd)) {
			dev = xf86CreateI2CDevRec();
			dev->DevName = "TV";
			dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
			dev->pI2CBus = pBIOSInfo->I2C_Port2;

			if (xf86I2CDevInit(dev)) {
			    switch (pBIOSInfo->TVEncoder) {
			    case VIA_TV2PLUS:
				pUserSetting->AdaptiveFilterOn = FALSE;
				pUserSetting->tvAdaptiveFFilter = 0;
				break;
			    case VIA_TV3:
			    case VIA_VT1622A:
			    case VIA_VT1623:
				W_Buffer[0] = 0x03;
				xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
				W_Buffer[1] = R_Buffer[0] & ~0x03;
				xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
				W_Buffer[0] = 0x61;
				W_Buffer[1] = (unsigned char) pUserSetting->tvAdaptiveFFilter;
				xf86DestroyI2CDevRec(dev, TRUE);
				pUserSetting->AdaptiveFilterOn = TRUE;
				break;
			    case VIA_SAA7108:
			    case VIA_FS454:
			    case VIA_CH7009:
			    case VIA_CH7019:
			    default:
				pUserSetting->AdaptiveFilterOn = FALSE;
				pUserSetting->tvAdaptiveFFilter = 0;
				break;
			    }
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
			} else {
			    xf86DestroyI2CDevRec(dev, TRUE);
			    InParam = buf + 8;
			    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
			}
		    }
		} else {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_SETTING_DOT_CRAWL:
		if (pUTSETTVITEMSTATE.dwState == UT_STATE_OFF || pUTSETTVITEMSTATE.dwState == UT_STATE_DEFAULT) {
		    pBIOSInfo->TVDotCrawl = FALSE;
		    switch (pBIOSInfo->TVEncoder) {
		    case VIA_TV2PLUS:
			VIAPreSetTV2Mode(pBIOSInfo);
			VIAPostSetTV2Mode(pBIOSInfo);
			break;
		    case VIA_TV3:
		    case VIA_VT1622A:
			VIAPreSetTV3Mode(pBIOSInfo);
			VIAPostSetTV3Mode(pBIOSInfo);
			break;
		    case VIA_FS454:
			VIAPreSetFS454Mode(pBIOSInfo);
			VIAPostSetFS454Mode(pBIOSInfo);
			break;
		    case VIA_CH7009:
		    case VIA_CH7019:
			VIAPreSetCH7019Mode(pBIOSInfo);
			VIAPostSetCH7019Mode(pBIOSInfo);
			break;
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else if (pUTSETTVITEMSTATE.dwState == UT_STATE_ON) {
		    pBIOSInfo->TVDotCrawl = TRUE;
		    switch (pBIOSInfo->TVEncoder) {
		    case VIA_TV2PLUS:
			VIAPreSetTV2Mode(pBIOSInfo);
			VIAPostSetTV2Mode(pBIOSInfo);
			break;
		    case VIA_TV3:
		    case VIA_VT1622A:
		    case VIA_VT1623:
			VIAPreSetTV3Mode(pBIOSInfo);
			VIAPostSetTV3Mode(pBIOSInfo);
			break;
		    case VIA_FS454:
			VIAPreSetFS454Mode(pBIOSInfo);
			VIAPostSetFS454Mode(pBIOSInfo);
			break;
		    case VIA_CH7009:
		    case VIA_CH7019:
			VIAPreSetCH7019Mode(pBIOSInfo);
			VIAPostSetCH7019Mode(pBIOSInfo);
			break;
		    }
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		} else {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    case UT_TV_SETTING_LOCK_ASPECT_RATIO:
		if (pUTSETTVITEMSTATE.dwState == UT_STATE_OFF) {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
		} else if (pUTSETTVITEMSTATE.dwState == UT_STATE_ON) {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
		} else {
		    InParam = buf + 8;
		    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		}
		break;
	    default:
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
		break;
	    }
	    break;
	default:
	    memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    InParam += 4;
	    ErrorF(" via_utility.c : dwAction not supported\n");
	    break;
	}
	break;
    case UT_XV_FUNC_GAMMA:
	switch (dwAction) {
	case UT_ESC_FUNC_GAMMA_GetDeviceSupportState:
	    if (pScrn->bitsPerPixel == 8)
		dwSupportState = UT_DEVICE_NONE;
	    else
		dwSupportState = UT_DEVICE_CRT1 | UT_DEVICE_TV | UT_DEVICE_DFP | UT_DEVICE_LCD;
	    /*dwSupportState = UT_DEVICE_NONE; */
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, &dwSupportState, sizeof(CARD32));
	    break;
	case UT_ESC_FUNC_GAMMA_GetLookUpTable:
	    if (VIASavePalette(pScrn, pBIOSInfo->colors)) {
		for (i = 0; i < 256; i++) {
		    pUTGAMMAINFO.LookUpTable[i] = ((CARD32) pBIOSInfo->colors[i].red) << 16;
		    pUTGAMMAINFO.LookUpTable[i] |= ((CARD32) pBIOSInfo->colors[i].green) << 8;
		    pUTGAMMAINFO.LookUpTable[i] |= (CARD32) pBIOSInfo->colors[i].blue;
		}
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
		InParam += 4;
		memcpy((void *) InParam, pUTGAMMAINFO.LookUpTable, sizeof(pUTGAMMAINFO.LookUpTable));
	    } else {
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    }
	    break;
	case UT_ESC_FUNC_GAMMA_SetLookUpTable:
	    InParam = buf + 12;
	    memcpy(&pUTGAMMAINFO, (void *) InParam, sizeof(UTGAMMAINFO));
	    if (pUTGAMMAINFO.LookUpTable[0] == 0xFFFFFFFF) {	/* Set default gamma value */
		for (i = 0; i < 256; i++) {
		    pBIOSInfo->colors[i].red = i;
		    pBIOSInfo->colors[i].green = i;
		    pBIOSInfo->colors[i].blue = i;
		}
	    } else {
		for (i = 0; i < 256; i++) {
		    pBIOSInfo->colors[i].red = (unsigned short) (pUTGAMMAINFO.LookUpTable[i] >> 16);
		    pBIOSInfo->colors[i].green = (unsigned short) (pUTGAMMAINFO.LookUpTable[i] >> 8) & 0x00FF;
		    pBIOSInfo->colors[i].blue = (unsigned short) (pUTGAMMAINFO.LookUpTable[i] & 0x000000FF);
		}
	    }
	    if (VIARestorePalette(pScrn, pBIOSInfo->colors)) {
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    } else {
		InParam = buf + 8;
		memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	    }
	    break;
	case UT_ESC_FUNC_GAMMA_GetDefaultLookUpTable:
	    for (i = 0; i < 256; i++) {
		pUTGAMMAINFO.LookUpTable[i] = (CARD32) i << 16;
		pUTGAMMAINFO.LookUpTable[i] |= (CARD32) i << 8;
		pUTGAMMAINFO.LookUpTable[i] |= (CARD32) i;
	    }
	    memcpy((void *) InParam, &dwUTRetOK, sizeof(CARD32));
	    InParam += 4;
	    memcpy((void *) InParam, pUTGAMMAINFO.LookUpTable, sizeof(pUTGAMMAINFO.LookUpTable));
	    break;
	default:
	    memcpy((void *) InParam, &dwUTRetNoFunc, sizeof(CARD32));
	    InParam += 4;
	    ErrorF(" via_utility.c : dwAction not supported\n");
	    break;
	}
	break;
    default:
	ErrorF(" via_utility.c : dwFunc not supported\n");
	memcpy((void *) InParam, &dwUTRetFail, sizeof(CARD32));
	InParam += 4;
	break;
    }				/* end of switch */
}

Bool VIAUTGetInfo(VIABIOSInfoPtr pBIOSInfo)
{
    VIAUserSettingPtr pUserSetting = pBIOSInfo->UserSetting;
    I2CDevPtr dev;
    unsigned char W_Buffer[3];
    unsigned char R_Buffer[2];

    DEBUG(xf86DrvMsg(pBIOSInfo->scrnIndex, X_INFO, "VIAUTGetInfo\n"));
    if (pBIOSInfo->TVEncoder) {
	dev = xf86CreateI2CDevRec();
	dev->DevName = "TV";
	dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
	dev->pI2CBus = pBIOSInfo->I2C_Port2;

	if (xf86I2CDevInit(dev)) {
	    switch (pBIOSInfo->TVEncoder) {
	    case VIA_TV2PLUS:
	    case VIA_TV3:
	    case VIA_VT1622A:
		pUserSetting->tvHPosition = 6;
		pUserSetting->tvVPosition = 6;

		W_Buffer[0] = 0x03;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvFFilter = (R_Buffer[0] & 0x03) + 1;

		W_Buffer[0] = 0x0B;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvBrightness = R_Buffer[0];

		W_Buffer[0] = 0x0C;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvContrast = R_Buffer[0];

		W_Buffer[0] = 0x0D;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvSaturation = R_Buffer[0] << 8;
		W_Buffer[0] = 0x0A;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvSaturation += R_Buffer[0];

		break;
	    case VIA_VT1623:
		VIAGPIOI2C_Initial(pBIOSInfo, 0x40);
		pUserSetting->tvHPosition = 6;
		pUserSetting->tvVPosition = 6;

		VIAGPIOI2C_Read(pBIOSInfo, 0x03, R_Buffer, 1);
		pUserSetting->tvFFilter = (R_Buffer[0] & 0x03) + 1;

		VIAGPIOI2C_Read(pBIOSInfo, 0x0B, R_Buffer, 1);
		pUserSetting->tvBrightness = R_Buffer[0];

		VIAGPIOI2C_Read(pBIOSInfo, 0x0C, R_Buffer, 1);
		pUserSetting->tvContrast = R_Buffer[0];

		VIAGPIOI2C_Read(pBIOSInfo, 0x0D, R_Buffer, 1);
		pUserSetting->tvSaturation = R_Buffer[0] << 8;
		VIAGPIOI2C_Read(pBIOSInfo, 0x0A, R_Buffer, 1);
		pUserSetting->tvSaturation += R_Buffer[0];

		break;
	    case VIA_SAA7108:
		pUserSetting->tvHPosition = 6;
		pUserSetting->tvVPosition = 6;

		W_Buffer[0] = 0x37;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvFFilter = ((R_Buffer[0] & 0x30) >> 4) + 1;

		pUserSetting->tvBrightness = 0;
		pUserSetting->tvContrast = 0;
		pUserSetting->tvSaturation = 0;
		pUserSetting->tvAdaptiveFFilter = 0;
		pUserSetting->tvTint = 0;
		pUserSetting->AdaptiveFilterOn = FALSE;
		break;
	    case VIA_CH7009:
	    case VIA_CH7019:
	    case VIA_FS454:
	    default:
		pUserSetting->tvHPosition = 0;
		pUserSetting->tvVPosition = 0;
		pUserSetting->tvFFilter = 0;
		pUserSetting->tvBrightness = 0;
		pUserSetting->tvContrast = 0;
		pUserSetting->tvSaturation = 0;
		pUserSetting->tvAdaptiveFFilter = 0;
		pUserSetting->tvTint = 0;
		pUserSetting->AdaptiveFilterOn = FALSE;
		break;
	    }
	    if (pBIOSInfo->TVEncoder == VIA_TV2PLUS) {
		pUserSetting->tvAdaptiveFFilter = 0;

		W_Buffer[0] = 0x10;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvTint = R_Buffer[0];
		W_Buffer[0] = 0x11;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvTint += (R_Buffer[0] & 0xE0) << 8;
	    } else if (pBIOSInfo->TVEncoder == VIA_TV3 || pBIOSInfo->TVEncoder == VIA_VT1622A) {
		W_Buffer[0] = 0x61;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvAdaptiveFFilter = R_Buffer[0];
		W_Buffer[0] = 0x10;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvTint = R_Buffer[0];
		W_Buffer[0] = 0x11;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		pUserSetting->tvTint += (R_Buffer[0] & 0x07) << 8;
		if (pUserSetting->tvAdaptiveFFilter && !pUserSetting->tvFFilter)
		    pUserSetting->AdaptiveFilterOn = TRUE;
		else
		    pUserSetting->AdaptiveFilterOn = FALSE;
	    } else if (pBIOSInfo->TVEncoder == VIA_VT1623) {
		VIAGPIOI2C_Read(pBIOSInfo, 0x61, R_Buffer, 1);
		pUserSetting->tvAdaptiveFFilter = R_Buffer[0];
		VIAGPIOI2C_Read(pBIOSInfo, 0x10, R_Buffer, 1);
		pUserSetting->tvTint = R_Buffer[0];
		VIAGPIOI2C_Read(pBIOSInfo, 0x11, R_Buffer, 1);
		pUserSetting->tvTint += (R_Buffer[0] & 0x07) << 8;
		if (pUserSetting->tvAdaptiveFFilter && !pUserSetting->tvFFilter)
		    pUserSetting->AdaptiveFilterOn = TRUE;
		else
		    pUserSetting->AdaptiveFilterOn = FALSE;
	    }
	    xf86DestroyI2CDevRec(dev, TRUE);
	    pUserSetting->DefaultSetting = TRUE;
	} else {
	    xf86DestroyI2CDevRec(dev, TRUE);
	    DEBUG(xf86Msg(X_DEFAULT, "DevInit fail!\n"));
	}
    } else
	DEBUG(xf86Msg(X_DEFAULT, "No TVEncoder Exist!\n"));
    return TRUE;
}


Bool VIARestoreUserSetting(VIABIOSInfoPtr pBIOSInfo)
{
    VIAUserSettingPtr pUserSetting = pBIOSInfo->UserSetting;
    I2CDevPtr dev;
    int i, HPos, VPos, ADWHS, ADWHE;
    unsigned char W_Buffer[4], R_Buffer[3];
    dev = xf86CreateI2CDevRec();
    dev->DevName = "TV";
    dev->SlaveAddr = pBIOSInfo->TVI2CAdd;
    dev->pI2CBus = pBIOSInfo->I2C_Port2;
    if (xf86I2CDevInit(dev)) {

	switch (pBIOSInfo->TVEncoder) {
	case VIA_TV3:
	case VIA_VT1622A:
	    if ((pUserSetting->tvHPosition != 6)
		|| (pUserSetting->tvVPosition != 6)) {
		W_Buffer[0] = 0x08;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 2);
		HPos = R_Buffer[0];
		VPos = R_Buffer[1];
		W_Buffer[0] = 0x1C;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
		HPos |= (R_Buffer[0] & 0x04) << 6;
		VPos |= (R_Buffer[0] & 0x02) << 7;

		HPos += pUserSetting->tvHPosition - 6;
		VPos += pUserSetting->tvVPosition - 6;

		W_Buffer[0] = 0x08;
		W_Buffer[1] = (unsigned char) (HPos & 0xFF);
		W_Buffer[2] = (unsigned char) (VPos & 0xFF);
		xf86I2CWriteRead(dev, W_Buffer, 3, NULL, 0);

		W_Buffer[1] = R_Buffer[0] & ~0x06;	/* [2]1CH, [1]1CH */
		W_Buffer[1] |= (unsigned char) ((HPos >> 6) & 0x04);
		W_Buffer[1] |= (unsigned char) ((VPos >> 7) & 0x02);
		xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
	    }

	    W_Buffer[0] = 0x03;	/* Fflick */
	    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
	    if (pUserSetting->AdaptiveFilterOn) {
		W_Buffer[1] = R_Buffer[0] & ~0x03;
	    } else {
		W_Buffer[1] = R_Buffer[0] & ~0x03;
		W_Buffer[1] |= (unsigned char) (pUserSetting->tvFFilter - 1);
	    }
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);

	    W_Buffer[0] = 0x61;	/* Adaptive Fflick */
	    W_Buffer[1] = (unsigned char) pUserSetting->tvAdaptiveFFilter;
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);

	    W_Buffer[0] = 0x0B;	/* BRIGHTNESS */
	    W_Buffer[1] = (unsigned char) pUserSetting->tvBrightness;
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);

	    W_Buffer[0] = 0x0C;	/* CONTRAST */
	    W_Buffer[1] = (unsigned char) pUserSetting->tvContrast;
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);

	    W_Buffer[0] = 0x0D;	/* SATURATION highbyte */
	    W_Buffer[1] = (unsigned char) (pUserSetting->tvSaturation >> 8);
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);
	    W_Buffer[0] = 0x0A;	/* SATURATION lowbyte */
	    W_Buffer[1] = (unsigned char) pUserSetting->tvSaturation & 0xFF;
	    xf86I2CWriteRead(dev, W_Buffer, 2, R_Buffer, 0);

	    W_Buffer[0] = 0x11;	/* TINT highbyte */
	    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
	    W_Buffer[2] = R_Buffer[0] & ~0xE0;
	    W_Buffer[2] |= (unsigned char) (pUserSetting->tvTint >> 8);
	    W_Buffer[0] = 0x10;	/* TINT lowbyte */
	    W_Buffer[1] = (unsigned char) pUserSetting->tvTint & 0xFF;
	    xf86I2CWriteRead(dev, W_Buffer, 3, R_Buffer, 0);
	    break;
	case VIA_SAA7108:
	    if ((pUserSetting->tvHPosition - 6) != 0) {
		W_Buffer[0] = 0x70;
		xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 3);
		ADWHS = (R_Buffer[0] | ((R_Buffer[2] & 0x07) << 8))
		    + (pUserSetting->tvHPosition - 6);
		ADWHE = (R_Buffer[1] | ((R_Buffer[2] & 0x70) << 4))
		    + (pUserSetting->tvHPosition - 6);
		W_Buffer[0] = 0x70;
		W_Buffer[1] = ADWHS & 0xFF;
		W_Buffer[2] = ADWHE & 0xFF;
		W_Buffer[3] = (R_Buffer[2] & ~0x77) | ((ADWHS & 0x700) >> 8)
		    | ((ADWHE & 0x700) >> 4);
		xf86I2CWriteRead(dev, W_Buffer, 4, NULL, 0);
	    }
	    for (i = 0; i < 3; i++) {
		W_Buffer[0] = VIASAA7108PostionOffset[i];
		if (pBIOSInfo->TVVScan == VIA_TVNORMAL)
		    W_Buffer[1] = VIASAA7108PostionNormalTabRec[pBIOSInfo->TVType - 1][pBIOSInfo->resTVMode][pUserSetting->tvVPosition][i];
		else
		    W_Buffer[1] = VIASAA7108PostionOverTabRec[pBIOSInfo->TVType - 1][pBIOSInfo->resTVMode][pUserSetting->tvVPosition][i];
		xf86I2CWriteRead(dev, W_Buffer, 2, NULL, 0);
	    }
	    W_Buffer[0] = 0x37;
	    W_Buffer[1] = R_Buffer[0] & ~0x30;
	    if (!pUserSetting->AdaptiveFilterOn) {
		W_Buffer[1] |= (unsigned char) ((pUserSetting->tvFFilter - 1) << 4);
	    }
	    xf86I2CWriteRead(dev, W_Buffer, 1, R_Buffer, 1);
	    break;
	case VIA_VT1623:
	case VIA_CH7009:
	case VIA_CH7019:
	case VIA_FS454:
	default:
	    break;
	}

	xf86DestroyI2CDevRec(dev, TRUE);
	return TRUE;
    } else {
	xf86DestroyI2CDevRec(dev, TRUE);
	return FALSE;
    }
}
