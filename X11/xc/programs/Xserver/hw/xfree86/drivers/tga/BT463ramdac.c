/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/BT463ramdac.c,v 1.1 1998/08/29 05:43:34 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "BT.h"
#include "tga_regs.h"
#include "tga.h"


#define BT463_LOAD_ADDR(a) \
        TGA_WRITE_REG(BT463_ADDR_LO<<2, TGA_RAMDAC_SETUP_REG); \
        TGA_WRITE_REG((BT463_ADDR_LO<<10)|((a)&0xff), TGA_RAMDAC_REG); \
        TGA_WRITE_REG(BT463_ADDR_HI<<2, TGA_RAMDAC_SETUP_REG); \
        TGA_WRITE_REG((BT463_ADDR_HI<<10)|(((a)>>8)&0xff), TGA_RAMDAC_REG);

#define BT463_WRITE(m,a,v) \
        BT463_LOAD_ADDR((a)); \
        TGA_WRITE_REG(((m)<<2),TGA_RAMDAC_SETUP_REG); \
        TGA_WRITE_REG(((m)<<10)|((v)&0xff),TGA_RAMDAC_REG);

/*
 * useful defines for managing the BT463 on the 24-plane TGAs
 */
#define BT463_READ_BIT          0x2

#define	BT463_ADDR_LO		0x0
#define	BT463_ADDR_HI		0x1
#define	BT463_REG_ACC		0x2
#define	BT463_PALETTE		0x3

#define	BT463_CUR_CLR_0		0x0100
#define	BT463_CUR_CLR_1		0x0101

#define	BT463_CMD_REG_0		0x0201
#define	BT463_CMD_REG_1		0x0202
#define	BT463_CMD_REG_2		0x0203

#define	BT463_READ_MASK_0	0x0205
#define	BT463_READ_MASK_1	0x0206
#define	BT463_READ_MASK_2	0x0207
#define	BT463_READ_MASK_3	0x0208

#define	BT463_BLINK_MASK_0	0x0209
#define	BT463_BLINK_MASK_1	0x020a
#define	BT463_BLINK_MASK_2	0x020b
#define	BT463_BLINK_MASK_3	0x020c

#define	BT463_WINDOW_TYPE_BASE	0x0300


static unsigned
BT463_READ(TGAPtr pTga, unsigned m, unsigned a)
{
  unsigned val;

  BT463_LOAD_ADDR(a);
  TGA_WRITE_REG((m<<2)|0x2, TGA_RAMDAC_SETUP_REG);
  val = TGA_READ_REG(TGA_RAMDAC_REG);
  val = (val >> 16) & 0xff;
  return val;
}


void
BT463ramdacSave(ScrnInfoPtr pScrn, unsigned char *Bt463)
{
  TGAPtr pTga = TGAPTR(pScrn);
  int i, j;

  Bt463[0] = BT463_READ(pTga, BT463_REG_ACC, BT463_CMD_REG_0);
  Bt463[1] = BT463_READ(pTga, BT463_REG_ACC, BT463_CMD_REG_1);
  Bt463[2] = BT463_READ(pTga, BT463_REG_ACC, BT463_CMD_REG_2);
  
  Bt463[3] = BT463_READ(pTga, BT463_REG_ACC, BT463_READ_MASK_0);
  Bt463[4] = BT463_READ(pTga, BT463_REG_ACC, BT463_READ_MASK_1);
  Bt463[5] = BT463_READ(pTga, BT463_REG_ACC, BT463_READ_MASK_2);
  Bt463[6] = BT463_READ(pTga, BT463_REG_ACC, BT463_READ_MASK_3);
  
  Bt463[7] = BT463_READ(pTga, BT463_REG_ACC, BT463_BLINK_MASK_0);
  Bt463[8] = BT463_READ(pTga, BT463_REG_ACC, BT463_BLINK_MASK_1);
  Bt463[9] = BT463_READ(pTga, BT463_REG_ACC, BT463_BLINK_MASK_2);
  Bt463[10] = BT463_READ(pTga, BT463_REG_ACC, BT463_BLINK_MASK_3);
  
  BT463_LOAD_ADDR(BT463_WINDOW_TYPE_BASE);
  TGA_WRITE_REG((BT463_REG_ACC<<2)|0x2, TGA_RAMDAC_SETUP_REG);
  
  for (i = 0, j = 11; i < 16; i++) {
    Bt463[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
    Bt463[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
    Bt463[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
  }

/*
  fprintf(stderr, "BT463ramdacSave (%p)\n", Bt463);
  for (i=0; i<58; i++)
    fprintf(stderr, "%2d: 0x%02x\n", i, (unsigned)Bt463[i]);
*/
}


void
BT463ramdacRestore(ScrnInfoPtr pScrn, unsigned char *Bt463)
{
  TGAPtr pTga = TGAPTR(pScrn);
  int i, j;

  BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_0, Bt463[0]);
  BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_1, Bt463[1]);
  BT463_WRITE(BT463_REG_ACC, BT463_CMD_REG_2, Bt463[2]);
  
  BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_0, Bt463[3]);
  BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_1, Bt463[4]);
  BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_2, Bt463[5]);
  BT463_WRITE(BT463_REG_ACC, BT463_READ_MASK_3, Bt463[6]);
  
  BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_0, Bt463[7]);
  BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_1, Bt463[8]);
  BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_2, Bt463[9]);
  BT463_WRITE(BT463_REG_ACC, BT463_BLINK_MASK_3, Bt463[10]);
  
  BT463_LOAD_ADDR(BT463_WINDOW_TYPE_BASE);
  TGA_WRITE_REG((BT463_REG_ACC<<2), TGA_RAMDAC_SETUP_REG);
  
  for (i = 0, j = 11; i < 16; i++) {
    TGA_WRITE_REG(Bt463[j++]|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
    TGA_WRITE_REG(Bt463[j++]|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
    TGA_WRITE_REG(Bt463[j++]|(BT463_REG_ACC<<10), TGA_RAMDAC_REG);
  }

/*
  fprintf(stderr, "BT463ramdacRestore (%p)\n", Bt463);
  for (i=0; i<58; i++)
    fprintf(stderr, "%2d: 0x%02x\n", i, (unsigned)Bt463[i]);
*/
}
