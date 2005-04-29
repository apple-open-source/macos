
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740.h,v 1.8 2003/08/23 16:09:17 dawes Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#ifndef _I740_H_
#define _I740_H_

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "i740_reg.h"
#include "i740_macros.h"

#include "xaa.h"
#include "xf86Cursor.h"

/* Globals */
/* Memory mapped register access macros */
#define INREG8(addr)        *(volatile CARD8  *)(pI740->MMIOBase + (addr))
#define INREG16(addr)       *(volatile CARD16 *)(pI740->MMIOBase + (addr))
#define INREG(addr)         *(volatile CARD32 *)(pI740->MMIOBase + (addr))
#define OUTREG8(addr, val)  *(volatile CARD8  *)(pI740->MMIOBase + (addr)) = (val)
#define OUTREG16(addr, val) *(volatile CARD16 *)(pI740->MMIOBase + (addr)) = (val)
#define OUTREG(addr, val)   *(volatile CARD32 *)(pI740->MMIOBase + (addr)) = (val)

typedef struct _I740Rec *I740Ptr;

typedef void (*I740WriteIndexedByteFunc)(I740Ptr pI740, int addr, 
					 unsigned char index, char value);
typedef char (*I740ReadIndexedByteFunc)(I740Ptr pI740, int addr, 
					unsigned char index);
typedef void (*I740WriteByteFunc)(I740Ptr pI740, int addr, unsigned char value);
typedef char (*I740ReadByteFunc)(I740Ptr pI740, int addr);

typedef struct {
  unsigned char DisplayControl;
  unsigned char PixelPipeCfg0;
  unsigned char PixelPipeCfg1;
  unsigned char PixelPipeCfg2;
  unsigned char VideoClk2_M;
  unsigned char VideoClk2_N;
  unsigned char VideoClk2_MN_MSBs;
  unsigned char VideoClk2_DivisorSel;
  unsigned char PLLControl;
  unsigned char AddressMapping;
  unsigned char IOControl;
  unsigned char BitBLTControl;
  unsigned char ExtVertTotal;
  unsigned char ExtVertDispEnd;
  unsigned char ExtVertSyncStart;
  unsigned char ExtVertBlankStart;
  unsigned char ExtHorizTotal;
  unsigned char ExtHorizBlank;
  unsigned char ExtOffset;
  unsigned char InterlaceControl;
  unsigned int  LMI_FIFO_Watermark;
} I740RegRec, *I740RegPtr;

typedef struct _I740Rec {
  unsigned char *MMIOBase;
  unsigned char *FbBase;
  long FbMapSize;
  int cpp;
  int MaxClock;
  int CursorStart;
  int Chipset;
  unsigned long LinearAddr;
  unsigned long MMIOAddr;
  EntityInfoPtr pEnt;
  pciVideoPtr PciInfo;
  PCITAG PciTag;
  int HasSGRAM;
  I740RegRec SavedReg;
  I740RegRec ModeReg;
  XAAInfoRecPtr AccelInfoRec;
  xf86CursorInfoPtr CursorInfoRec;
  FBAreaPtr CursorData;
  CloseScreenProcPtr CloseScreen;
  GFX2DOPREG_BLTER_FULL_LOAD bltcmd;
  Bool usePIO;
  I740WriteIndexedByteFunc writeControl;
  I740ReadIndexedByteFunc readControl;
  I740WriteByteFunc writeStandard;
  I740ReadByteFunc readStandard;
  OptionInfoPtr Options;

  /*DGA*/
  DGAModePtr DGAModes;
  int numDGAModes;
  Bool DGAactive;
  int DGAViewportStatus;
  BoxRec FbMemBox;
  /*-*/

  /*I2C*/
  I2CBusPtr             rc_i2c;
  /*-*/

  /*-*/ /*Overlay*/
  XF86VideoAdaptorPtr adaptor;
  unsigned long OverlayStart;
  unsigned long OverlayPhysical;
  int colorKey;
  ScreenBlockHandlerProcPtr BlockHandler;
  int ov_offset_x,ov_offset_y;
  /*-*/

  Bool usevgacompat;
} I740Rec;

#define I740PTR(p) ((I740Ptr)((p)->driverPrivate))

extern Bool I740CursorInit(ScreenPtr pScreen);
extern Bool I740AccelInit(ScreenPtr pScreen);
void I740SetPIOAccess(I740Ptr pI740);
void I740SetMMIOAccess(I740Ptr pI740);
void I740InitVideo(ScreenPtr pScreen);

Bool I740SwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
void I740AdjustFrame(int scrnIndex, int x, int y, int flags);

Bool I740_I2CInit(ScrnInfoPtr pScrn);

#define minb(p) MMIO_IN8(pI740->MMIOBase, (p))
#define moutb(p,v) MMIO_OUT8(pI740->MMIOBase, (p),(v))

#endif
