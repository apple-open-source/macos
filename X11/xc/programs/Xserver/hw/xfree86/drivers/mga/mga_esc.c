/****************************************************************************
* mga_esc.c
*
* ESC call implementation
*
* (C) Matrox Graphics, Inc.
*****************************************************************************/

#ifdef USEMGAHAL

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

/* All drivers need this */
#include "xf86_ansic.h"

#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

#include "micmap.h"

#include "xf86DDC.h"
#include "xf86RAC.h"
#include "vbe.h"

#include "fb.h"
#include "cfb8_32.h"
#include "dixstruct.h"

#include "mga_reg.h"
#include "mga.h"
#include "mga_macros.h"

/* ESC */
LPMGAMODEINFO pMgaModeInfo[2] = {NULL};
MGAMODEINFO   TmpMgaModeInfo[2] = {{0}};

/* ESC Implementation */
static void EscHLeft(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscHRight(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscVUp(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscVDown(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscHLarger(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscHSmaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscVTaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscVSmaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscRefresh(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscRestoreVidParm(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscRead(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscWrite(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscHal(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscTest(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);
static void EscMerged(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode);

static LPMGAMODEINFO  GetModeInfoPtr(ULONG ulScreen);
static void GetVideoParameterStr(LPMGAMODEINFO pModeInfo, char *sResult);
static Bool convertNumber(unsigned long *pulNumber, char *sNumber);


static MGAEscFuncRec FunctionTable[] = {
    {"hal",  EscHal},
    {"test",  EscTest},
    {"read",  EscRead},
	{"write", EscWrite},
    {"left",  EscHLeft},
    {"right", EscHRight},
    {"down",  EscVDown},
    {"up",    EscVUp},
    {"h+",    EscHLarger},
    {"h-",    EscHSmaller},
    {"v+",    EscVTaller},
    {"v-",    EscVSmaller},
    {"refresh", EscRefresh},
    {"undo",  EscRestoreVidParm},
    {"merged", EscMerged},
    {NULL,NULL}
};


void MGAFillDisplayModeStruct(DisplayModePtr pMode, LPMGAMODEINFO pModeInfo)
{
   pMode->Clock = pModeInfo->ulPixClock;

   pMode->HDisplay = pModeInfo->ulDispWidth;
   pMode->HSyncStart =  pModeInfo->ulDispWidth
                       + pModeInfo->ulHFPorch;
   pMode->HSyncEnd = pModeInfo->ulDispWidth
                       + pModeInfo->ulHFPorch
                        + pModeInfo->ulHSync;
   pMode->HTotal = pModeInfo->ulDispWidth
                       + pModeInfo->ulHFPorch
                       + pModeInfo->ulHSync
                       + pModeInfo->ulHBPorch;

   pMode->VDisplay = pModeInfo->ulDispHeight;
   pMode->VSyncStart =  pModeInfo->ulDispHeight
                       + pModeInfo->ulVFPorch;
   pMode->VSyncEnd = pModeInfo->ulDispHeight
                       + pModeInfo->ulVFPorch
                       + pModeInfo->ulVSync;
   pMode->VTotal = pModeInfo->ulDispHeight
                       + pModeInfo->ulVFPorch
                       + pModeInfo->ulVSync
                       + pModeInfo->ulVBPorch;

   pMode->VRefresh = pModeInfo->ulRefreshRate;
}

static LPMGAMODEINFO  GetModeInfoPtr(ULONG ulScreen)
{

    if ( !TmpMgaModeInfo[ulScreen].ulDispWidth )
    {
        TmpMgaModeInfo[ulScreen] = *pMgaModeInfo[ulScreen];
    }

    return &TmpMgaModeInfo[ulScreen];
}


static void GetVideoParameterStr(LPMGAMODEINFO pModeInfo, char *sResult)
{
    sprintf(sResult, "%d %d %d %d %d %d %d %d %d %d %d",
            pModeInfo->ulDispWidth,
            pModeInfo->ulDispHeight,
            pModeInfo->ulBpp,
            pModeInfo->ulPixClock,
            pModeInfo->ulHFPorch,
            pModeInfo->ulHSync,
            pModeInfo->ulHBPorch,
            pModeInfo->ulVFPorch,
            pModeInfo->ulVSync,
            pModeInfo->ulVBPorch,
            pModeInfo->flSignalMode);
}


static float  GetVRefresh(LPMGAMODEINFO pModeInfo)
{
    ULONG ulHTotal;
    ULONG ulVTotal;

    ulHTotal =
        pModeInfo->ulDispWidth +
        pModeInfo->ulHFPorch +
        pModeInfo->ulHSync +
        pModeInfo->ulHBPorch;

    ulVTotal =
        pModeInfo->ulDispHeight +
        pModeInfo->ulVFPorch    +
        pModeInfo->ulVSync      +
        pModeInfo->ulVBPorch;

    return ((float)pModeInfo->ulPixClock * 1000.0) / (ulHTotal * ulVTotal);
}

static void EscHal(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
	MGAPtr pMGA = MGAPTR(pScrn);

	if(pMGA->HALLoaded)
		strcpy(sResult, "YES");
	else
		strcpy(sResult, "NO");

}

static void EscTest(ScrnInfoPtr pScrn, unsigned long *param, char
*sResult, DisplayModePtr pMode)
{
	strcpy(sResult, "YES");
}

static void EscMerged(ScrnInfoPtr pScrn, unsigned long *param, char
*sResult, DisplayModePtr pMode)
{
	strcpy(sResult, "YES");
}

static void  EscRead(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    ULONG  ulSource, ulAddr, ulData;
    UCHAR  ucIndex;

    if ( (param[0] & 0xffff) < 2 )
    {
        strcpy(sResult, "#error 1");
        return;
    }


    ulSource = param[1] >> 16;
    ulAddr = param[1] & 0xffff;


    switch( ulSource )
    {
    case 0:
        ulData = INREG(ulAddr);
        sprintf(sResult, "MGA[%04X] = 0x%08X", ulAddr, ulData);
        break;
    case 1:
        ucIndex = INREG8(0x3c00);
        OUTREG(0x3c00, (UCHAR)ulAddr);
        ulData = (ULONG)INREG8(0x3c0a);
        OUTREG(0x3c00, ucIndex);
        sprintf(sResult, "DAC[%02X] = 0x%02X", ulAddr, ulData);
        break;
    case 2:
        ucIndex = INREG8(0x1fd4);
        OUTREG(0x1fd4, (UCHAR)ulAddr);
        ulData = (ULONG)INREG8(0x1fd5);
        OUTREG(0x1fd4, ucIndex);
        sprintf(sResult, "CRTC[%02X] = 0x%02X", ulAddr, ulData);
        break;
    case 3:
        ucIndex = INREG8(0x1fde);
        OUTREG(0x1fde, (UCHAR)ulAddr);
        ulData = (ULONG)INREG8(0x1fdf);
        OUTREG(0x1fde, ucIndex);
        sprintf(sResult, "CRTCEXT[%02X] = 0x%02X", ulAddr, ulData);
        break;
    default:
        strcpy(sResult, "ERROR# 2");
        break;
    }
}

static void  EscWrite(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    ULONG  ulSource, ulAddr, ulData;
    UCHAR  ucIndex;

    if ( (param[0] & 0xffff) < 3 )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    ulSource = param[1] >> 16;
    ulAddr = param[1] & 0xffff;
    ulData = param[2];


    switch( ulSource )
    {
    case 0:
        OUTREG(ulAddr, ulData);
        strcpy(sResult, "OK");
        break;
    case 1:
        ucIndex = INREG8(0x3c00);
        OUTREG(0x3c00, (UCHAR)ulAddr);
        OUTREG(0x3c0a, (UCHAR)ulData);
        OUTREG(0x3c00, ucIndex);
        strcpy(sResult, "OK");
        break;
    case 2:
        ucIndex = INREG8(0x1fd4);
        OUTREG(0x1fd4, (UCHAR)ulAddr);
        OUTREG(0x1fd5, (UCHAR)ulData);
        OUTREG(0x1fd4, ucIndex);
        strcpy(sResult, "OK");
        break;
    case 3:
        ucIndex = INREG8(0x1fde);
        OUTREG(0x1fde, (UCHAR)ulAddr);
        OUTREG(0x1fdf, (UCHAR)ulData);
        OUTREG(0x1fde, ucIndex);
        strcpy(sResult, "OK");
        break;
    default:
        strcpy(sResult, "ERROR# 2");
        break;
    }
}

static void  EscHLeft(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }


    if (pModeInfo->ulHBPorch > (8 * param[1]) )
    {
        pModeInfo->ulHBPorch -=8 * param[1];
        pModeInfo->ulHFPorch +=8 * param[1];
        MGASetMode(pMga->pBoard, pModeInfo);
    }

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}


static void  EscHRight(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if (pModeInfo->ulHFPorch > (8 * param[1]) )
    {
        pModeInfo->ulHFPorch -=8 * param[1];
        pModeInfo->ulHBPorch +=8 * param[1];
        MGASetMode(pMga->pBoard, pModeInfo);
    }

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}



static void  EscVUp(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if (pModeInfo->ulVBPorch > (param[1]) )
    {
        pModeInfo->ulVBPorch -= param[1];
        pModeInfo->ulVFPorch += param[1];
        MGASetMode(pMga->pBoard, pModeInfo);
    }

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}


static void  EscVDown(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if (pModeInfo->ulVFPorch >= (param[1]) )
    {
        pModeInfo->ulVFPorch -= param[1];
        pModeInfo->ulVBPorch += param[1];
        MGASetMode(pMga->pBoard, pModeInfo);
    }

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}


static void EscHLarger(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;
    float fRefresh, fPixelClock;
    ULONG ulStep;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if ((param[0] & 0xffff) > 1)
    {

        ulStep = param[1] * 8;
    }
    else
    {

        ulStep = 8;
    }

    fRefresh = GetVRefresh(pModeInfo);
    fPixelClock = (float)pModeInfo->ulPixClock;
    if (pModeInfo->ulHBPorch >= ulStep )
    {
        pModeInfo->ulHBPorch -= ulStep;
    }
    else
    {
        pModeInfo->ulHBPorch = 0;
    }
    pModeInfo->ulPixClock = (ULONG)( (fRefresh * fPixelClock) / GetVRefresh(pModeInfo));
    MGASetMode(pMga->pBoard, pModeInfo);

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}


static void EscHSmaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;
    float fRefresh, fPixelClock;
    ULONG ulStep;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if ((param[0] & 0xffff) > 1)
    {

        ulStep = param[1] * 8;
    }
    else
    {

        ulStep = 8;
    }


    fRefresh = GetVRefresh(pModeInfo);
    fPixelClock = (float)pModeInfo->ulPixClock;
    pModeInfo->ulHBPorch += ulStep;
    pModeInfo->ulPixClock = (ULONG)( (fRefresh * fPixelClock) / GetVRefresh(pModeInfo));

    MGASetMode(pMga->pBoard, pModeInfo);

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}

static void EscVTaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;
    float fRefresh, fPixelClock;
    ULONG ulStep;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if ((param[0] & 0xffff) > 1)
    {

        ulStep = param[1];
    }
    else
    {

        ulStep = 1;
    }

    fRefresh = GetVRefresh(pModeInfo);
    fPixelClock = (float)pModeInfo->ulPixClock;

    if (pModeInfo->ulVBPorch >= ulStep )
    {
        pModeInfo->ulVBPorch -= ulStep;
    }
    else
    {
        pModeInfo->ulVBPorch = 0;
    }

    pModeInfo->ulPixClock = (ULONG)( (fRefresh * fPixelClock) / GetVRefresh(pModeInfo));
    MGASetMode(pMga->pBoard, pModeInfo);

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}

static void EscVSmaller(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;
    float fRefresh, fPixelClock;
    ULONG ulStep;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if ((param[0] & 0xffff) > 1)
    {

        ulStep = param[1];
    }
    else
    {

        ulStep = 1;
    }


    fRefresh = GetVRefresh(pModeInfo);
    fPixelClock = (float)pModeInfo->ulPixClock;
    pModeInfo->ulVFPorch += ulStep;
    pModeInfo->ulPixClock = (ULONG)( (fRefresh * fPixelClock) / GetVRefresh(pModeInfo));
    MGASetMode(pMga->pBoard, pModeInfo);

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}


static void EscRefresh(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);
    LPMGAMODEINFO pModeInfo;
    float fRefresh, fPixelClock;

    pModeInfo = GetModeInfoPtr(param[0] >> 16);

    if ( !pMgaModeInfo )
    {
        strcpy(sResult, "#error 1");
        return;
    }

    if ((param[0] & 0xffff) < 2)
    {
        strcpy(sResult, "#error 1");
        return;
    }

    fRefresh = GetVRefresh(pModeInfo);

    fPixelClock = (float)pModeInfo->ulPixClock;
    pModeInfo->ulPixClock = (ULONG)( ((float)param[1] * fPixelClock) / fRefresh);

    pModeInfo->ulRefreshRate = param[1];

    MGASetMode(pMga->pBoard, pModeInfo);

    MGAFillDisplayModeStruct(pMode, pModeInfo);

    GetVideoParameterStr(pModeInfo, sResult);
}

static void EscRestoreVidParm(ScrnInfoPtr pScrn, unsigned long *param, char *sResult, DisplayModePtr pMode)
{
    MGAPtr pMga = MGAPTR(pScrn);

    TmpMgaModeInfo[param[0] >> 16].ulDispWidth = 0;
    MGASetMode(pMga->pBoard, pMgaModeInfo[param[0] >> 16]);

    MGAFillDisplayModeStruct(pMode, pMgaModeInfo[param[0] >> 16]);

    GetVideoParameterStr(pMgaModeInfo[param[0] >> 16], sResult);
}

static Bool convertNumber(unsigned long *pulNumber, char *sNumber)
{
    unsigned long i, ulDigit, shiftHex;
    Bool bResult = TRUE;

    if (sNumber == NULL)
    {
        return FALSE;
    }


    /* Convert number */
    if ( (sNumber[0] == '0') && (sNumber[1] == 'x') )
    {
        shiftHex = 0;
        *pulNumber = 0;

        for (i = strlen(sNumber) - 1; i > 1; i--)
        {
            if (shiftHex > 28)
            {
                bResult = FALSE;
                break;
            }

            if ( !isxdigit(sNumber[i]) )
            {
                bResult = FALSE;
                break;
            }

            ulDigit = toupper(sNumber[i]) - '0';
            if (ulDigit > 9)
            {
                ulDigit -= 7;
            }
            *pulNumber += ulDigit << shiftHex;
            shiftHex += 4;
        }
    }
    else
    {
        for (i = 0; i < strlen(sNumber); i++)
        {
            if ( !isdigit(sNumber[i]) )
            {
                bResult = FALSE;
                break;
            }
        }
        *pulNumber = atoi(sNumber);
    }

    return bResult;
}

static Bool GetEscCommand(char *cmdline, EscCmdStruct *escCmd)
{
    unsigned long i, paramIndex, ulHI;
    Bool bResult;
    char *pParameter, *function;

    bResult = TRUE; /* success */

    function = strtok(cmdline, " \t\n,");


    escCmd->parameters[0] = 0;
    if (function)
    {
        /* Find Screen */
        if (function[1] == ':' )
        {
            escCmd->parameters[0] = (unsigned long)(function[0] - '0') << 16;
            strncpy(escCmd->function, function+2, 32);
        }
        else
        {
            strncpy(escCmd->function, function, 32);
        }

    }
    else
    {
        strcpy(escCmd->function, "#ERROR -1");
        escCmd->parameters[0] = 0;
        return FALSE;
    }

    paramIndex = 1;
    while ( (pParameter = strtok(NULL, " \t\n,")) != NULL )
    {
        if (paramIndex > 31)
        {
            /* 32 parameters supported */
            break;
        }

        i = 0;
        while(pParameter[i] && pParameter[i] != ':')
        {
            i++;
        }

        if ( pParameter[i] )
        {
            pParameter[i] = '\0';
            bResult  = convertNumber(&escCmd->parameters[paramIndex], &pParameter[i+1]);
            bResult |= convertNumber(&ulHI, pParameter);
            escCmd->parameters[paramIndex] &= 0xffff;
            escCmd->parameters[paramIndex] += ulHI << 16;
            pParameter[i] = ':';
        }
        else
        {
            bResult = convertNumber(&escCmd->parameters[paramIndex], pParameter);
        }


        if (!bResult)
        {
            break;
        }
        paramIndex++;
    }

    escCmd->parameters[0] += paramIndex;
    return bResult;

}

void MGAExecuteEscCmd(ScrnInfoPtr pScrn, char *cmdline , char *sResult, DisplayModePtr pMode)
{
    int i = 0;
    int ulScreen = 0;
    MGAPtr pMga = MGAPTR(pScrn);
    EscCmdStruct EscCmd;

    if (pMga->SecondCrtc)
    {
     	ulScreen = 1;
    }
    else
    {
	ulScreen = 0;
    }
	

    if (FunctionTable[0].function &&  GetEscCommand(cmdline,  &EscCmd) )
    {
        i = 0;

        while ( FunctionTable[i].function && strcmp(FunctionTable[i].function, EscCmd.function) )
        {
            i++;
        }

        if (FunctionTable[i].function)
        {
	    EscCmd.parameters[0] &= 0xffff;
	    EscCmd.parameters[0] |= ulScreen << 16;
            
            FunctionTable[i].funcptr(pScrn, EscCmd.parameters, sResult, pMode);
        }
        else
        {
            strcpy(sResult, "error# -1");
        }
    }
    else
    {
        strcpy(sResult, "error# -1");
    }
}
#else
int mga_foo;
#endif
