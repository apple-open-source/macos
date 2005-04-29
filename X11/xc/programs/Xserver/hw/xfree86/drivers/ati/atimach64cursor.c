/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64cursor.c,v 1.2 2004/01/05 16:42:02 tsi Exp $ */
/*
 * Copyright 2003 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ati.h"
#include "aticrtc.h"
#include "atimach64accel.h"
#include "atimach64cursor.h"
#include "atimach64io.h"

/*
 * ATIMach64SetCursorColours --
 *
 * Set hardware cursor foreground and background colours.
 */
static void
ATIMach64SetCursorColours
(
    ScrnInfoPtr pScreenInfo,
    int         fg,
    int         bg
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    outr(CUR_CLR0, SetBits(fg, CUR_CLR));
    outr(CUR_CLR1, SetBits(bg, CUR_CLR));
}

/*
 * ATIMach64SetCursorPosition --
 *
 * Set position of hardware cursor.
 */
static void
ATIMach64SetCursorPosition
(
    ScrnInfoPtr pScreenInfo,
    int         x,
    int         y
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);
    CARD16 CursorXOffset, CursorYOffset;

    /* Adjust x & y when the cursor is partially obscured */
    if (x < 0)
    {
        if ((CursorXOffset = -x) > 63)
            CursorXOffset = 63;
        x = 0;
    }
    else
    {
        CursorXOffset = pScreenInfo->frameX1 - pScreenInfo->frameX0;
        if (x > CursorXOffset)
            x = CursorXOffset;
        CursorXOffset = 0;
    }

    if (y < 0)
    {
        if ((CursorYOffset = -y) > 63)
            CursorYOffset = 63;
        y = 0;
    }
    else
    {
        CursorYOffset = pScreenInfo->frameY1 - pScreenInfo->frameY0;
        if (y > CursorYOffset)
            y = CursorYOffset;
        CursorYOffset = 0;
    }

    /* Adjust for multiscanned modes */
    if (pScreenInfo->currentMode->Flags & V_DBLSCAN)
        y *= 2;
    if (pScreenInfo->currentMode->VScan > 1)
        y *= pScreenInfo->currentMode->VScan;

    do
    {
        if (CursorYOffset != pATI->CursorYOffset)
        {
            pATI->CursorYOffset = CursorYOffset;
            outr(CUR_OFFSET, ((CursorYOffset << 4) + pATI->CursorOffset) >> 3);
        }
        else if (CursorXOffset == pATI->CursorXOffset)
            break;

        pATI->CursorXOffset = CursorXOffset;
        outr(CUR_HORZ_VERT_OFF, SetBits(CursorXOffset, CUR_HORZ_OFF) |
            SetBits(CursorYOffset, CUR_VERT_OFF));
    } while (0);

    outr(CUR_HORZ_VERT_POSN,
        SetBits(x, CUR_HORZ_POSN) | SetBits(y, CUR_VERT_POSN));
}

/*
 * ATIMach64LoadCursorImage --
 *
 * Copy hardware cursor image into offscreen video memory.
 */
static void
ATIMach64LoadCursorImage
(
    ScrnInfoPtr pScreenInfo,
    CARD8       *pImage
)
{
    ATIPtr           pATI     = ATIPTR(pScreenInfo);
    CARD32          *pSrc     = (pointer)pImage;
    volatile CARD32 *pDst     = pATI->pCursorImage;

    /* Synchronise video memory accesses */
    ATIMach64Sync(pScreenInfo);

#   if defined(ATIMove32)

    {
        ATIMove32(pDst, pSrc, 256);
    }

#   else

    {
        /* This is lengthy, but it does maximise burst modes */
        pDst[  0] = pSrc[  0];  pDst[  1] = pSrc[  1];
        pDst[  2] = pSrc[  2];  pDst[  3] = pSrc[  3];
        pDst[  4] = pSrc[  4];  pDst[  5] = pSrc[  5];
        pDst[  6] = pSrc[  6];  pDst[  7] = pSrc[  7];
        pDst[  8] = pSrc[  8];  pDst[  9] = pSrc[  9];
        pDst[ 10] = pSrc[ 10];  pDst[ 11] = pSrc[ 11];
        pDst[ 12] = pSrc[ 12];  pDst[ 13] = pSrc[ 13];
        pDst[ 14] = pSrc[ 14];  pDst[ 15] = pSrc[ 15];
        pDst[ 16] = pSrc[ 16];  pDst[ 17] = pSrc[ 17];
        pDst[ 18] = pSrc[ 18];  pDst[ 19] = pSrc[ 19];
        pDst[ 20] = pSrc[ 20];  pDst[ 21] = pSrc[ 21];
        pDst[ 22] = pSrc[ 22];  pDst[ 23] = pSrc[ 23];
        pDst[ 24] = pSrc[ 24];  pDst[ 25] = pSrc[ 25];
        pDst[ 26] = pSrc[ 26];  pDst[ 27] = pSrc[ 27];
        pDst[ 28] = pSrc[ 28];  pDst[ 29] = pSrc[ 29];
        pDst[ 30] = pSrc[ 30];  pDst[ 31] = pSrc[ 31];
        pDst[ 32] = pSrc[ 32];  pDst[ 33] = pSrc[ 33];
        pDst[ 34] = pSrc[ 34];  pDst[ 35] = pSrc[ 35];
        pDst[ 36] = pSrc[ 36];  pDst[ 37] = pSrc[ 37];
        pDst[ 38] = pSrc[ 38];  pDst[ 39] = pSrc[ 39];
        pDst[ 40] = pSrc[ 40];  pDst[ 41] = pSrc[ 41];
        pDst[ 42] = pSrc[ 42];  pDst[ 43] = pSrc[ 43];
        pDst[ 44] = pSrc[ 44];  pDst[ 45] = pSrc[ 45];
        pDst[ 46] = pSrc[ 46];  pDst[ 47] = pSrc[ 47];
        pDst[ 48] = pSrc[ 48];  pDst[ 49] = pSrc[ 49];
        pDst[ 50] = pSrc[ 50];  pDst[ 51] = pSrc[ 51];
        pDst[ 52] = pSrc[ 52];  pDst[ 53] = pSrc[ 53];
        pDst[ 54] = pSrc[ 54];  pDst[ 55] = pSrc[ 55];
        pDst[ 56] = pSrc[ 56];  pDst[ 57] = pSrc[ 57];
        pDst[ 58] = pSrc[ 58];  pDst[ 59] = pSrc[ 59];
        pDst[ 60] = pSrc[ 60];  pDst[ 61] = pSrc[ 61];
        pDst[ 62] = pSrc[ 62];  pDst[ 63] = pSrc[ 63];
        pDst[ 64] = pSrc[ 64];  pDst[ 65] = pSrc[ 65];
        pDst[ 66] = pSrc[ 66];  pDst[ 67] = pSrc[ 67];
        pDst[ 68] = pSrc[ 68];  pDst[ 69] = pSrc[ 69];
        pDst[ 70] = pSrc[ 70];  pDst[ 71] = pSrc[ 71];
        pDst[ 72] = pSrc[ 72];  pDst[ 73] = pSrc[ 73];
        pDst[ 74] = pSrc[ 74];  pDst[ 75] = pSrc[ 75];
        pDst[ 76] = pSrc[ 76];  pDst[ 77] = pSrc[ 77];
        pDst[ 78] = pSrc[ 78];  pDst[ 79] = pSrc[ 79];
        pDst[ 80] = pSrc[ 80];  pDst[ 81] = pSrc[ 81];
        pDst[ 82] = pSrc[ 82];  pDst[ 83] = pSrc[ 83];
        pDst[ 84] = pSrc[ 84];  pDst[ 85] = pSrc[ 85];
        pDst[ 86] = pSrc[ 86];  pDst[ 87] = pSrc[ 87];
        pDst[ 88] = pSrc[ 88];  pDst[ 89] = pSrc[ 89];
        pDst[ 90] = pSrc[ 90];  pDst[ 91] = pSrc[ 91];
        pDst[ 92] = pSrc[ 92];  pDst[ 93] = pSrc[ 93];
        pDst[ 94] = pSrc[ 94];  pDst[ 95] = pSrc[ 95];
        pDst[ 96] = pSrc[ 96];  pDst[ 97] = pSrc[ 97];
        pDst[ 98] = pSrc[ 98];  pDst[ 99] = pSrc[ 99];
        pDst[100] = pSrc[100];  pDst[101] = pSrc[101];
        pDst[102] = pSrc[102];  pDst[103] = pSrc[103];
        pDst[104] = pSrc[104];  pDst[105] = pSrc[105];
        pDst[106] = pSrc[106];  pDst[107] = pSrc[107];
        pDst[108] = pSrc[108];  pDst[109] = pSrc[109];
        pDst[110] = pSrc[110];  pDst[111] = pSrc[111];
        pDst[112] = pSrc[112];  pDst[113] = pSrc[113];
        pDst[114] = pSrc[114];  pDst[115] = pSrc[115];
        pDst[116] = pSrc[116];  pDst[117] = pSrc[117];
        pDst[118] = pSrc[118];  pDst[119] = pSrc[119];
        pDst[120] = pSrc[120];  pDst[121] = pSrc[121];
        pDst[122] = pSrc[122];  pDst[123] = pSrc[123];
        pDst[124] = pSrc[124];  pDst[125] = pSrc[125];
        pDst[126] = pSrc[126];  pDst[127] = pSrc[127];
        pDst[128] = pSrc[128];  pDst[129] = pSrc[129];
        pDst[130] = pSrc[130];  pDst[131] = pSrc[131];
        pDst[132] = pSrc[132];  pDst[133] = pSrc[133];
        pDst[134] = pSrc[134];  pDst[135] = pSrc[135];
        pDst[136] = pSrc[136];  pDst[137] = pSrc[137];
        pDst[138] = pSrc[138];  pDst[139] = pSrc[139];
        pDst[140] = pSrc[140];  pDst[141] = pSrc[141];
        pDst[142] = pSrc[142];  pDst[143] = pSrc[143];
        pDst[144] = pSrc[144];  pDst[145] = pSrc[145];
        pDst[146] = pSrc[146];  pDst[147] = pSrc[147];
        pDst[148] = pSrc[148];  pDst[149] = pSrc[149];
        pDst[150] = pSrc[150];  pDst[151] = pSrc[151];
        pDst[152] = pSrc[152];  pDst[153] = pSrc[153];
        pDst[154] = pSrc[154];  pDst[155] = pSrc[155];
        pDst[156] = pSrc[156];  pDst[157] = pSrc[157];
        pDst[158] = pSrc[158];  pDst[159] = pSrc[159];
        pDst[160] = pSrc[160];  pDst[161] = pSrc[161];
        pDst[162] = pSrc[162];  pDst[163] = pSrc[163];
        pDst[164] = pSrc[164];  pDst[165] = pSrc[165];
        pDst[166] = pSrc[166];  pDst[167] = pSrc[167];
        pDst[168] = pSrc[168];  pDst[169] = pSrc[169];
        pDst[170] = pSrc[170];  pDst[171] = pSrc[171];
        pDst[172] = pSrc[172];  pDst[173] = pSrc[173];
        pDst[174] = pSrc[174];  pDst[175] = pSrc[175];
        pDst[176] = pSrc[176];  pDst[177] = pSrc[177];
        pDst[178] = pSrc[178];  pDst[179] = pSrc[179];
        pDst[180] = pSrc[180];  pDst[181] = pSrc[181];
        pDst[182] = pSrc[182];  pDst[183] = pSrc[183];
        pDst[184] = pSrc[184];  pDst[185] = pSrc[185];
        pDst[186] = pSrc[186];  pDst[187] = pSrc[187];
        pDst[188] = pSrc[188];  pDst[189] = pSrc[189];
        pDst[190] = pSrc[190];  pDst[191] = pSrc[191];
        pDst[192] = pSrc[192];  pDst[193] = pSrc[193];
        pDst[194] = pSrc[194];  pDst[195] = pSrc[195];
        pDst[196] = pSrc[196];  pDst[197] = pSrc[197];
        pDst[198] = pSrc[198];  pDst[199] = pSrc[199];
        pDst[200] = pSrc[200];  pDst[201] = pSrc[201];
        pDst[202] = pSrc[202];  pDst[203] = pSrc[203];
        pDst[204] = pSrc[204];  pDst[205] = pSrc[205];
        pDst[206] = pSrc[206];  pDst[207] = pSrc[207];
        pDst[208] = pSrc[208];  pDst[209] = pSrc[209];
        pDst[210] = pSrc[210];  pDst[211] = pSrc[211];
        pDst[212] = pSrc[212];  pDst[213] = pSrc[213];
        pDst[214] = pSrc[214];  pDst[215] = pSrc[215];
        pDst[216] = pSrc[216];  pDst[217] = pSrc[217];
        pDst[218] = pSrc[218];  pDst[219] = pSrc[219];
        pDst[220] = pSrc[220];  pDst[221] = pSrc[221];
        pDst[222] = pSrc[222];  pDst[223] = pSrc[223];
        pDst[224] = pSrc[224];  pDst[225] = pSrc[225];
        pDst[226] = pSrc[226];  pDst[227] = pSrc[227];
        pDst[228] = pSrc[228];  pDst[229] = pSrc[229];
        pDst[230] = pSrc[230];  pDst[231] = pSrc[231];
        pDst[232] = pSrc[232];  pDst[233] = pSrc[233];
        pDst[234] = pSrc[234];  pDst[235] = pSrc[235];
        pDst[236] = pSrc[236];  pDst[237] = pSrc[237];
        pDst[238] = pSrc[238];  pDst[239] = pSrc[239];
        pDst[240] = pSrc[240];  pDst[241] = pSrc[241];
        pDst[242] = pSrc[242];  pDst[243] = pSrc[243];
        pDst[244] = pSrc[244];  pDst[245] = pSrc[245];
        pDst[246] = pSrc[246];  pDst[247] = pSrc[247];
        pDst[248] = pSrc[248];  pDst[249] = pSrc[249];
        pDst[250] = pSrc[250];  pDst[251] = pSrc[251];
        pDst[252] = pSrc[252];  pDst[253] = pSrc[253];
        pDst[254] = pSrc[254];  pDst[255] = pSrc[255];
    }

#endif

}

/*
 * ATIMach64HideCursor --
 *
 * Turn off hardware cursor.
 */
static void
ATIMach64HideCursor
(
    ScrnInfoPtr pScreenInfo
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    if (!(pATI->NewHW.gen_test_cntl & GEN_CUR_EN))
        return;

    pATI->NewHW.gen_test_cntl &= ~GEN_CUR_EN;
    out8(GEN_TEST_CNTL, GetByte(pATI->NewHW.gen_test_cntl, 0));
}

/*
 * ATIMach64ShowCursor --
 *
 * Turn on hardware cursor.
 */
static void
ATIMach64ShowCursor
(
    ScrnInfoPtr pScreenInfo
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);

    if (pATI->NewHW.gen_test_cntl & GEN_CUR_EN)
        return;

    pATI->NewHW.gen_test_cntl |= GEN_CUR_EN;
    out8(GEN_TEST_CNTL, GetByte(pATI->NewHW.gen_test_cntl, 0));
}

/*
 * ATIMach64UseHWCursor --
 *
 * Notify cursor layer whether a hardware cursor is configured.
 */
static Bool
ATIMach64UseHWCursor
(
    ScreenPtr pScreen,
    CursorPtr pCursor
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[pScreen->myNum];
    ATIPtr      pATI        = ATIPTR(pScreenInfo);

    if (!pATI->CursorBase)
        return FALSE;

#ifndef AVOID_CPIO

    /*
     * For some reason, the hardware cursor isn't vertically scaled when a VGA
     * doublescanned or multiscanned mode is in effect.
     */
    if (pATI->NewHW.crtc == ATI_CRTC_MACH64)
        return TRUE;
    if ((pScreenInfo->currentMode->Flags & V_DBLSCAN) ||
        (pScreenInfo->currentMode->VScan > 1))
        return FALSE;

#endif /* AVOID_CPIO */

    return TRUE;
}

/*
 * ATIMach64CursorInit --
 *
 * Initialise xf86CursorInfoRec fields with information specific to Mach64
 * variants.
 */
Bool
ATIMach64CursorInit
(
    xf86CursorInfoPtr pCursorInfo
)
{
    /*
     * For Mach64 variants, toggling hardware cursors off and on causes display
     * artifacts.  Ask the cursor support layers to always paint the cursor
     * (whether or not it is entirely transparent) and to not hide the cursor
     * when reloading its image.  The two remaining reasons for turning off the
     * hardware cursor are when it moves to a different screen or on a switch
     * to a different virtual console.
     */
    pCursorInfo->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
        HARDWARE_CURSOR_INVERT_MASK |
        HARDWARE_CURSOR_SHOW_TRANSPARENT |
        HARDWARE_CURSOR_UPDATE_UNHIDDEN |
        HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |

#if X_BYTE_ORDER != X_LITTLE_ENDIAN

        HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |

#endif /* X_BYTE_ORDER */

        HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1;
    pCursorInfo->MaxWidth = pCursorInfo->MaxHeight = 64;

    pCursorInfo->SetCursorColors = ATIMach64SetCursorColours;
    pCursorInfo->SetCursorPosition = ATIMach64SetCursorPosition;
    pCursorInfo->LoadCursorImage = ATIMach64LoadCursorImage;
    pCursorInfo->HideCursor = ATIMach64HideCursor;
    pCursorInfo->ShowCursor = ATIMach64ShowCursor;
    pCursorInfo->UseHWCursor = ATIMach64UseHWCursor;

    return TRUE;
}
