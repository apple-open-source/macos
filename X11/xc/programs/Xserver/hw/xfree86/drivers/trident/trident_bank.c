/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_bank.c,v 1.4 2000/12/07 16:48:04 alanh Exp $ */

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

#include "compiler.h"
#include "trident.h"
#include "trident_regs.h"

int TVGA8900SetRead(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTW(0x3c4, 0xC000 | (((bank & 0x3f) ^ 0x02)<<8)|0x0E);
  return 0;
}
int TGUISetRead(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTB(0x3d9, bank & 0xff);
  return 0;
}
int TVGA8900SetWrite(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTW(0x3c4, 0xC000 | (((bank & 0x3f) ^ 0x02)<<8)|0x0E);
  return 0;
}
int TGUISetWrite(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTB(0x3d8, bank & 0xff);
  return 0;
}
int TVGA8900SetReadWrite(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTW(0x3c4, 0xC000 | (((bank & 0x3f) ^ 0x02)<<8)|0x0E);
  return 0;
}
int TGUISetReadWrite(ScreenPtr pScreen, int bank)
{
  ScrnInfoPtr pScrn;
  TRIDENTPtr pTrident;
  pScrn = xf86Screens[pScreen->myNum];
  pTrident = TRIDENTPTR(pScrn);
  OUTB(0x3d8, bank & 0xff);
  OUTB(0x3d9, bank & 0xff);
  return 0;
}
