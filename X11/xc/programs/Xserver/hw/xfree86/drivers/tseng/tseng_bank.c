
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_bank.c,v 1.4 2000/08/08 08:58:06 eich Exp $ */





#include "tseng.h"

/*
 * Tseng really screwed up when they decided to combine the read and write
 * bank selectors into one register. Now we need to cache the bank
 * registers, because IO reads are too expensive.
 */


int
ET4000W32SetRead(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (pTseng->cache_SegSelL & 0x0f) | (iBank << 4);
    pTseng->cache_SegSelH = (pTseng->cache_SegSelH & 0x03) | (iBank & 0x30);
    outb(0x3CB, pTseng->cache_SegSelH);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}

int
ET4000W32SetWrite(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (pTseng->cache_SegSelL & 0xf0) | (iBank & 0x0f);
    pTseng->cache_SegSelH = (pTseng->cache_SegSelH & 0x30) | (iBank >> 4);
    outb(0x3CB, pTseng->cache_SegSelH);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}

int
ET4000W32SetReadWrite(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (iBank & 0x0f) | (iBank << 4);
    pTseng->cache_SegSelH = (iBank & 0x30) | (iBank >> 4);
    outb(0x3CB, pTseng->cache_SegSelH);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}

int
ET4000SetRead(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (pTseng->cache_SegSelL & 0x0f) | (iBank << 4);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}

int
ET4000SetWrite(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (pTseng->cache_SegSelL & 0xf0) | (iBank & 0x0f);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}

int
ET4000SetReadWrite(ScreenPtr pScreen, unsigned int iBank)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TsengPtr pTseng = TsengPTR(pScrn);

    pTseng->cache_SegSelL = (iBank & 0x0f) | (iBank << 4);
    outb(0x3CD, pTseng->cache_SegSelL);
    return 0;
}
