/*
 * Copyright 1999 by Matthew Grossman, Seattle, USA.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Matthew
 * Grossman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Matthew Grossman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * MATTHEW GROSSMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL MATTHEW GROSSMAN BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Matthew Grossman, mattg@oz.net
 * 
 * DEC TGA hardware cursor using BT485 ramdac
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_cursor.c,v 1.2 2000/10/20 12:57:26 alanh Exp $ */


/* tga_cursor.c */

#include "xf86.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "BT.h"

#include "tga.h"
#include "tga_regs.h"

/* defines */
/* BT485 also supports 32 bit cursor, but why use it? */
#define CURSOR_SIZE 64

/* protos */

static void TGALoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void TGAShowCursor(ScrnInfoPtr pScrn);
static void TGAHideCursor(ScrnInfoPtr pScrn);
static void TGASetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void TGASetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);

static void
TGALoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
  TGAPtr pTga = TGAPTR(pScrn);
  int i;

  /* set 64 bit cursor */
  pTga->RamDacRec->WriteDAC(pScrn, BT_COMMAND_REG_0, 0x7F, 0x80);
  pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0x00, 0x01);
  pTga->RamDacRec->WriteDAC(pScrn, BT_STATUS_REG, 0xF8, 0x04);

  /* first write address reg @ 0x0, then write 0xb with the data (for 32 bit
     cursor) */

  pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0xFC, 0x00);
  
  for(i = 0; i < ((CURSOR_SIZE * CURSOR_SIZE) / 8); i++)
    pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_RAM_DATA, 0x00, *src++);
  
  for(i = 0; i < ((CURSOR_SIZE * CURSOR_SIZE) / 8); i++)
    pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_RAM_DATA, 0x00, *src++);
  
/*    pTga->RamDacRec->WriteDAC(pScrn, BT_WRITE_ADDR, 0xFC, 0x00); */

  return;
}


static void 
TGAShowCursor(ScrnInfoPtr pScrn)
{
  TGAPtr pTga = TGAPTR(pScrn);

  /* enable BT485 X11 cursor */
  pTga->RamDacRec->WriteDAC(pScrn, BT_COMMAND_REG_2, 0xFC, 0x03);
  
  return;
}


static void
TGAHideCursor(ScrnInfoPtr pScrn)
{
  TGAPtr pTga = TGAPTR(pScrn);

  pTga->RamDacRec->WriteDAC(pScrn, BT_COMMAND_REG_2, 0xFC, 0x00);

  return;
}

static void
TGASetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   TGAPtr pTga = TGAPTR(pScrn);
   unsigned int tmp_x, tmp_y;

   /* translate x && y to BT485 cursor addresses */

   tmp_x = x + CURSOR_SIZE;
   tmp_x &= 0x0FFF;
   
   tmp_y = y + CURSOR_SIZE;
   tmp_y &= 0x0FFF;

   /* write out the addresses */
   pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_X_LOW, 0x00, (tmp_x & 0xFF));
   pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_X_HIGH, 0xF0, (tmp_x >> 8));

   pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_Y_LOW, 0x00, (tmp_y & 0xFF));
   pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_Y_HIGH, 0xF0, (tmp_y >> 8));

   return;
}


static void
TGASetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
     /* set pScrn->cursor_fg and pScrn->cursor_bg */
{
  TGAPtr pTga = TGAPTR(pScrn);

    /* first, load address register at 0x4 with 0x1, then write 3 color
       octets RGB to 0x5 (background), then write three octets to 0x5
       (foreground), then write to address register 0xFC */
  
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_WR_ADDR, 0xFC, 0x01);
  
  /* we don't seem to support the 6 bit DAC option as of 4.0, and why
     would we? */
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (bg & 0x00FF0000) >> 16);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (bg & 0x0000FF00) >> 8);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (bg & 0x000000FF));
  
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (fg & 0x00FF0000) >> 16);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (fg & 0x0000FF00) >> 8);
  pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_DATA, 0x00, (fg & 0x000000FF));

/*    pTga->RamDacRec->WriteDAC(pScrn, BT_CURS_WR_ADDR, 0xFC, 0x00); */
  
  return;
}


Bool 
TGAHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TGAPtr pTga;
    xf86CursorInfoPtr infoPtr;

    pTga = TGAPTR(pScrn);

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pTga->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = CURSOR_SIZE;
    infoPtr->MaxHeight = CURSOR_SIZE;
    infoPtr->Flags =  HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
      HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
      HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

    infoPtr->SetCursorColors = TGASetCursorColors;
    infoPtr->SetCursorPosition = TGASetCursorPosition;
    infoPtr->LoadCursorImage = TGALoadCursorImage;
    infoPtr->HideCursor = TGAHideCursor;
    infoPtr->ShowCursor = TGAShowCursor;
    infoPtr->UseHWCursor = NULL;

    return(xf86InitCursor(pScreen, infoPtr));
}

