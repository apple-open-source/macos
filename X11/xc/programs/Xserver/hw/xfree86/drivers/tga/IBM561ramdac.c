/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/IBM561ramdac.c,v 1.4 2003/09/24 02:43:28 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "tga_regs.h"
#include "tga.h"

/*
 * useful defines for managing the IBM561 on the 24-plane TGA2s
 */

#define IBM561_HEAD_MASK                0x01
#define IBM561_READ                     0x02
#define IBM561_WRITE                    0x00

#define RAMDAC_ONE_BYTE                    0x0E000
#define RAMDAC_TWO_BYTES                   0x0c000
#define RAMDAC_THREE_BYTES                 0x08000
#define RAMDAC_FOUR_BYTES                  0x00000

#define IBM561_ADDR_LOW                 0x0000
#define IBM561_ADDR_HIGH                0x0100
#define IBM561_CMD_REGS                 0x0200

#define IBM561_CMD_CURS_PIX             0x0200
#define IBM561_CMD_CURS_LUT             0x0300
#define IBM561_CMD_FB_WAT               0x0300
#define IBM561_CMD_AUXFB_WAT            0x0200
#define IBM561_CMD_OL_WAT               0x0300
#define IBM561_CMD_AUXOL_WAT            0x0200
#define IBM561_CMD_GAMMA                0x0300
#define IBM561_CMD_CMAP                 0x0300

#define IBM561_ADDR_EPSR_SHIFT          0
#define IBM561_ADDR_EPDR_SHIFT          8

#define IBM561_CONFIG_REG_1             0x0001
#define IBM561_CONFIG_REG_2             0x0002
#define IBM561_CONFIG_REG_1             0x0001
#define IBM561_CONFIG_REG_2             0x0002
#define IBM561_CONFIG_REG_3             0x0003
#define IBM561_CONFIG_REG_4             0x0004
#define IBM561_WAT_SEG_REG              0x0006
#define IBM561_OL_SEG_REG               0x0007
#define IBM561_CHROMA_KEY_REG0          0x0010
#define IBM561_CHROMA_KEY_REG1          0x0011
#define IBM561_CHROMA_MASK_REG0         0x0012
#define IBM561_CHROMA_MASK_REG1         0x0013
#define IBM561_SYNC_CONTROL             0x0020
#define IBM561_PLL_VCO_DIV_REG          0x0021
#define IBM561_PLL_REF_REG              0x0022
#define IBM561_CURSOR_CTRL_REG          0x0030
#define IBM561_CURSOR_HS_REG            0x0034
#define IBM561_VRAM_MASK_REG            0x0050
#define IBM561_DAC_CTRL                 0x005f
#define IBM561_DIV_DOT_CLK_REG          0x0082

#define IBM561_READ_MASK                0x0205
#define IBM561_BLINK_MASK               0x0209
#define IBM561_FB_WINDOW_TYPE_TABLE     0x1000
#define IBM561_AUXFB_WINDOW_TYPE_TABLE  0x0E00
#define IBM561_OL_WINDOW_TYPE_TABLE     0x1400
#define IBM561_AUXOL_WINDOW_TYPE_TABLE  0x0F00
#define IBM561_RED_GAMMA_TABLE          0x3000
#define IBM561_GREEN_GAMMA_TABLE        0x3400
#define IBM561_BLUE_GAMMA_TABLE         0x3800
#define IBM561_COLOR_LOOKUP_TABLE       0x4000
#define IBM561_CURSOR_LOOKUP_TABLE      0x0a11
#define IBM561_CURSOR_BLINK_TABLE       0x0a15
#define IBM561_CROSS_LOOKUP_TABLE       0x0a19
#define IBM561_CROSS_BLINK_TABLE        0x0a1d
#define IBM561_CURSOR_PIXMAP            0x2000
#define IBM561_CURSOR_X_LOW             0x0036
#define IBM561_CURSOR_X_HIGH            0x0037
#define IBM561_CURSOR_Y_LOW             0x0038
#define IBM561_CURSOR_Y_HIGH            0x0039

#define LO_ADDR		(IBM561_ADDR_LOW  | RAMDAC_ONE_BYTE)
#define HI_ADDR		(IBM561_ADDR_HIGH | RAMDAC_ONE_BYTE)

#define REGS_ADDR	(IBM561_CMD_REGS  | RAMDAC_ONE_BYTE)
#define FBWAT_ADDR      (IBM561_CMD_FB_WAT  | RAMDAC_ONE_BYTE)
#define AUXFBWAT_ADDR	(IBM561_CMD_AUXFB_WAT | RAMDAC_ONE_BYTE)
#define OLWAT_ADDR	(IBM561_CMD_OL_WAT | RAMDAC_ONE_BYTE)
#define AUXOLWAT_ADDR	(IBM561_CMD_AUXOL_WAT | RAMDAC_ONE_BYTE)
#define CMAP_ADDR	(IBM561_CMD_CMAP  | RAMDAC_ONE_BYTE)
#define GAMMA_ADDR	(IBM561_CMD_GAMMA  | RAMDAC_ONE_BYTE)

#define IBM561LoadAddr(reg) \
do { \
  TGA2_WRITE_RAMDAC_REG((reg), LO_ADDR); \
  TGA2_WRITE_RAMDAC_REG((reg) >> 8, HI_ADDR); \
} while (0)

unsigned char
IBM561ReadReg(ScrnInfoPtr pScrn, CARD32 reg)
{
  TGAPtr pTga;
  unsigned char ret;

  pTga  = TGAPTR(pScrn);

  TGA2_WRITE_RAMDAC_REG(reg, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(reg >> 8, HI_ADDR);

  ret = TGA2_READ_RAMDAC_REG(REGS_ADDR);

#if 1
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "IBM561ReadReg: reg 0x%lx data 0x%x\n",
             (unsigned long)reg, ret);
#endif
  return (ret);
}

void
IBM561WriteReg(ScrnInfoPtr pScrn, CARD32 reg,
#if 0
	       unsigned char mask, unsigned char data)
#else
	       unsigned char data)
#endif
{
  TGAPtr pTga;
  unsigned char tmp = 0x00;

  pTga = TGAPTR(pScrn);
  
#if 0
  if (mask != 0x00) {
    TGA2_WRITE_RAMDAC_REG(reg, LO_ADDR);
    TGA2_WRITE_RAMDAC_REG(reg >> 8, HI_ADDR);
    tmp = TGA2_READ_RAMDAC_REG(REGS_ADDR) & mask;
  }
#endif

#if 1
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "IBM561WriteReg: reg 0x%lx data 0x%x\n",
             (unsigned long)reg, tmp | data);
#endif

  TGA2_WRITE_RAMDAC_REG(reg, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(reg >> 8, HI_ADDR);
  TGA2_WRITE_RAMDAC_REG ((tmp | data), REGS_ADDR);
}

void
IBM561ramdacSave(ScrnInfoPtr pScrn, unsigned char *Ibm561)
{
#if 0
  TGAPtr pTga = TGAPTR(pScrn);
  int i, j;

  /* ?? FIXME OR NOT this is from BT463ramdacSave ?? */
  Ibm561[0] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_CMD_REG_0);
  Ibm561[1] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_CMD_REG_1);
  Ibm561[2] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_CMD_REG_2);
  
  Ibm561[3] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_READ_MASK_0);
  Ibm561[4] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_READ_MASK_1);
  Ibm561[5] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_READ_MASK_2);
  Ibm561[6] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_READ_MASK_3);
  
  Ibm561[7] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_BLINK_MASK_0);
  Ibm561[8] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_BLINK_MASK_1);
  Ibm561[9] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_BLINK_MASK_2);
  Ibm561[10] = IBM561_READ(pTga, IBM561_REG_ACC, IBM561_BLINK_MASK_3);
  
  IBM561_LOAD_ADDR(IBM561_WINDOW_TYPE_BASE);
  TGA_WRITE_REG((IBM561_REG_ACC<<2)|0x2, TGA_RAMDAC_SETUP_REG);
  
  for (i = 0, j = 11; i < 16; i++) {
    Ibm561[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
    Ibm561[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
    Ibm561[j++] = (TGA_READ_REG(TGA_RAMDAC_REG)>>16)&0xff;
  }
#endif

/*
  fprintf(stderr, "IBM561ramdacSave (%p)\n", Ibm561);
  for (i=0; i<58; i++)
    fprintf(stderr, "%2d: 0x%02x\n", i, (unsigned)Ibm561[i]);
*/
}

static void
IBM561WindowTagsInit(ScrnInfoPtr pScrn)
{
  TGAPtr pTga = TGAPTR(pScrn);
  unsigned char low, high;
  int i;

/*
  tga.h defines fb_wid_cell_t as a structure containing two bytes,
  low and high in order.  The 561 has 10 bit window tags so only
  part of the high byte is used (actually only 2 bits). Pixel C for
  8-plane indexes uses 16bpp indexing per IBM's application notes
  which describe quad bufering. Note, this array is arranged as
  low byte followed by high byte which will apppear backwards
  relative to the 561 spec( a value of 0x01 in the high byte
  really represents a color table starting address of 256).
  ex  (entry 4):
    {0x28, 0x01},        *4 8-plane index (PIXEL C 561 H/W screw-up) *
  low byte = 0x28
  high byte = 0x01
  wat entry = 0x0128

   from the spec: 8 in the low nibble selects buffer 1
                  2 in the next nibble selects pixformat of 16 bpp
                  1 in the next nibble indicates a start addr of 256
*/
typedef struct {
    unsigned char       low_byte;
    unsigned char       high_byte;
}fb_wid_cell_t;

typedef struct {
  unsigned char         aux_fbwat;
} aux_fb_wid_cell_t;

typedef struct {
    unsigned char       low_byte;
    unsigned char       high_byte;
} ol_wid_cell_t;

typedef struct {
  unsigned char         aux_olwat;
} aux_ol_wid_cell_t;

/*
 * There are actually 256 window tag entries in the FB and OL WAT tables.
 * We will use only 16 for compatability with the BT463 and more importantly
 * to implement the virtual ramdac interface.  This requires us to only
 * report the smallest WAT table size, in this case its the auxillary wat
 * tables which are 16 entries.
 */

#define TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT      256
#define TGA_RAMDAC_561_FB_WINDOW_TAG_MAX_COUNT  16
#define TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT   16
#define TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT      256
#define TGA_RAMDAC_561_OL_WINDOW_TAG_MAX_COUNT  16
#define TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT   16
#define TGA_RAMDAC_561_CMAP_ENTRY_COUNT         1024
#define TGA_RAMDAC_561_GAM_ENTRY_COUNT          256

  static fb_wid_cell_t
    fb_wids_561[TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT] = {
#if 0
    {0x28, 0x00},       /*0 8-plane index (PIXEL C 561 H/W screw-up) */
#else
    {0x36, 0x00},       /*c 24-plane true       */
#endif
    {0x08, 0x00},       /*1 8-plane index (PIXEL B) */
    {0x00, 0x00},       /*2 8-plane index (PIXEL A) */
    {0x34, 0x00},       /*3 24-plane direct, cmap 0 */
    {0x28, 0x01},       /*4 8-plane index (PIXEL C 561 H/W screw-up) */
    {0x08, 0x01},       /*5 8-plane index (PIXEL B) */
    {0x00, 0x01},       /*6 8-plane index (PIXLE A) */
    {0x34, 0x01},       /*7 24-plane direct, cmap 1 */
    {0x1e, 0x00},       /*8 12-plane true       */
    /*{0x16, 0x00},       9 12-plane true       */
    {0x14, 0x00},       /*9 12-plane true(direct)       */
    {0x1e, 0x01},       /*a 12-plane true       */
    {0x16, 0x01},       /*b 12-plane true       */
    {0x36, 0x00},       /*c 24-plane true       */
    {0x36, 0x00},       /*d 24-plane true       */
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0}
  };

  static aux_fb_wid_cell_t
    auxfb_wids_561[TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT] = {
    {0x04},             /*0 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*1 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*2 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*3 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*4 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*5 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*6 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*7 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*8 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*9 GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*a GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*b GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*c GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*d GMA=bypass, XH=disable, PT=dc */
    {0x04},             /*e old cursor colors for 463 don't use*/
    {0x04},             /*f old cursor colors for 463 don't use*/
  };

  static ol_wid_cell_t
    ol_wids_561[TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT] = {
    {0x31, 0x02},       /*0 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*1 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*2 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*3 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*4 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*5 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*6 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*7 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*8 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*9 PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*a PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*b PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*c PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*d PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*e PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0x31, 0x02},       /*f PX=4bpp, BS=0, MODE=index, TR=OPAQ    */
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
    {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}
   };

  static aux_ol_wid_cell_t
    auxol_wids_561[TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT] = {
    {0x0c},             /*0 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*1 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*2 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*3 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},             /*4 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*5 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*6 CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*7 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},             /*8 CK/OT=dc, UL=disabled, OL=disabled, GB=use    */
    {0x0c},             /*9 CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},             /*a CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},             /*b CK/OT=dc, UL=disabled, OL=enabled,  GB=use    */
    {0x0c},             /*c CK/OT=dc, UL=disabled, OL=enabled,  GB=bypass */
    {0x0c},             /*d CK/OT=dc, UL=disabled, OL=disabled, GB=bypass */
    {0x0c},             /*e old cursor color for 463, don't use           */
    {0x0c},             /*f old cursor color for 463, don't use           */
  };

  /* ibm561 so init the window tags's via interrupt.  It must be
   * done either during the vsync interrupt or by blanking, We will
   * actually do both. ???????
   */

  IBM561LoadAddr(IBM561_FB_WINDOW_TYPE_TABLE);
  for ( i = 0; i < TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT; i++ ) {
    low = ((fb_wids_561[i].low_byte & 0xfc) >> 2);
    high =((fb_wids_561[i].high_byte & 0x03) << 6) & 0xff;
    TGA2_WRITE_RAMDAC_REG (low | high, FBWAT_ADDR);

    low = (fb_wids_561[i].low_byte & 0x03) << 6;
    TGA2_WRITE_RAMDAC_REG (low, FBWAT_ADDR);
  }

  IBM561LoadAddr(IBM561_AUXFB_WINDOW_TYPE_TABLE);
  for ( i = 0; i < TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT; i++ ) {
    TGA2_WRITE_RAMDAC_REG (auxfb_wids_561[i].aux_fbwat, AUXFBWAT_ADDR);
  }


  IBM561LoadAddr(IBM561_OL_WINDOW_TYPE_TABLE);
  for ( i = 0; i < TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT; i++ ) {
    low = ((ol_wids_561[i].low_byte & 0xfc) >> 2);
    high =((ol_wids_561[i].high_byte & 0x03) << 6) & 0xff;
    TGA2_WRITE_RAMDAC_REG (low | high, OLWAT_ADDR);

    low = (ol_wids_561[i].low_byte & 0x03) << 6;
    TGA2_WRITE_RAMDAC_REG (low, OLWAT_ADDR);
  }


  IBM561LoadAddr(IBM561_AUXOL_WINDOW_TYPE_TABLE);
  for ( i = 0; i < TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT; i++ ) {
    TGA2_WRITE_RAMDAC_REG (auxol_wids_561[i].aux_olwat, AUXOLWAT_ADDR);
  }
}

/*
 * ibm561_init_color_map
 *
 *      Initialize color map in 561. Note the entire
 *      color map is initialized, both the 8-bit and the 24-bit
 *      portions.
 */
static int
IBM561InitColormap(ScrnInfoPtr pScrn)
{
  TGAPtr pTga = TGAPTR(pScrn);
#if 0
  tga_ibm561_info_t *bti = (tga_ibm561_info_t *) closure;
  tga_info_t *tgap = tga_softc[bti->unit];
#endif
  int i;

  TGA2_WRITE_RAMDAC_REG(IBM561_COLOR_LOOKUP_TABLE, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_COLOR_LOOKUP_TABLE >> 8, HI_ADDR);

  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
  }

  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
  }

  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
  }

  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, CMAP_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
    TGA2_WRITE_RAMDAC_REG (i, CMAP_ADDR);
  }

 /*
  * The ddx layer views the gamma table as an extension of the
  * color pallettes, therefore the gamma table is initialized here.
  * Note, each entry in the table is 10 bits, requiring two writes
  * per entry!! The table are initialized the same way as color tables,
  * a zero entry followed by mulitple ff's. NOTE, the gamma tables are
  * loaded in a strange manner, DO NOT use this code as a guide (we are
  * writing all zero's or all ones).  See the tga_ibm561_load_color_map
  * _entry  code above.
  */

  TGA2_WRITE_RAMDAC_REG(IBM561_RED_GAMMA_TABLE, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_RED_GAMMA_TABLE >> 8, HI_ADDR);

  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
  }

  TGA2_WRITE_RAMDAC_REG(IBM561_GREEN_GAMMA_TABLE, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_GREEN_GAMMA_TABLE >> 8, HI_ADDR);

  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
  }

  TGA2_WRITE_RAMDAC_REG(IBM561_BLUE_GAMMA_TABLE, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_BLUE_GAMMA_TABLE >> 8, HI_ADDR);

  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);
  TGA2_WRITE_RAMDAC_REG (0x00, GAMMA_ADDR);

  for ( i = 1; i <256; i++ ) {
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
    TGA2_WRITE_RAMDAC_REG (0xff, GAMMA_ADDR);
  }


#if 0
  /* ?? no cursor support yet */
  bti->cursor_fg.red = bti->cursor_fg.green = bti->cursor_fg.blue
                = 0xffff;
  bti->cursor_bg.red = bti->cursor_bg.green = bti->cursor_bg.blue
                = 0x0000;
  tga_ibm561_restore_cursor_color( closure, 0 );
#endif

  return 0;
}

void
IBM561ramdacHWInit(ScrnInfoPtr pScrn)
{
  TGAPtr pTga = TGAPTR(pScrn);

    unsigned int temp1[6] = {0,0,0,0,0,0};

    /*
     *  Set-up av9110 to 14.3 Mhz as reference for 561's PLL
     */
    temp1[0] = 0x00000101;
    temp1[1] = 0x01000000;
    temp1[2] = 0x00000001;
    temp1[3] = 0x00010000;
    temp1[4] = 0x01010100;
    temp1[5] = 0x01000000;

    write_av9110(pScrn, temp1);

 /*
  *  Initialize  IBM561 RAMDAC
  */
  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_1, 0x2a );
  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_3, 0x41 );
  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_4, 0x20 );

/* IBM561WriteReg(pScrn, IBM561_PLL_VCO_DIV_REG, 0xc8 ); */
  IBM561WriteReg(pScrn, IBM561_PLL_VCO_DIV_REG, tga_c_table->ibm561_vco_div);

/* IBM561WriteReg(pScrn, IBM561_PLL_REF_REG, 0x08 ); */
  IBM561WriteReg(pScrn, IBM561_PLL_REF_REG, tga_c_table->ibm561_ref );

  IBM561WriteReg(pScrn, IBM561_DIV_DOT_CLK_REG, 0xb0 );

  IBM561WriteReg(pScrn, IBM561_SYNC_CONTROL, 0x01 );

  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_2, 0x19 );

  TGA_WRITE_REG(0xFFFFFFFF, TGA_PLANEMASK_REG);

  /* Configure the RAMDAC, note registers not set either depend on the
   * previous setting (ie what firmaware programmed to be) or what the
   * X-server will set them to
   */


  /*
   * Config Register 1: MUX=4:1 BASIC, OVLY=8 bits, WID=8 bits (bits 4-7 of the
   * overlay and window ID's are tied to ground in the hardware).
   */
  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_1, 0x2a );

  /* SKIP Config Register 2-3 (use Diag settings at least for now) */

  /*
   * Config Register 4: FB_WID=4 bits, SWE=Common, AOW=LSB, AFW=LSB
   */
  IBM561WriteReg(pScrn, IBM561_CONFIG_REG_4, 0x20 );

  /*
   * SKIP Interleave Register (use Diag settings at least for now)
   */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*
   * WAT/OL Segement Registers
   */
  /* ?? we setup the address registers first, then stream the data out ?? */
  TGA2_WRITE_RAMDAC_REG(IBM561_WAT_SEG_REG, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_WAT_SEG_REG >> 8, HI_ADDR);

  /*  WAT Segment Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  OL Segment Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  AUX WAT Segment Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  AUX OL Segment Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*
   * Chroma Key Registers and Masks
   */
  /* ?? we setup the address registers first, then stream the data out ?? */
  TGA2_WRITE_RAMDAC_REG(IBM561_CHROMA_KEY_REG0, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_CHROMA_KEY_REG0 >> 8, HI_ADDR);

  /*  Chroma key register 0 */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Chroma key register 1 */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Chroma key mask register 0 */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Chroma key mask register 1 */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*
   * Cursor Control Register
   */
  IBM561WriteReg(pScrn, IBM561_CURSOR_CTRL_REG, /*pScrn->cursor_on_off*/0);

  /*
   * Cursor Hot Spot X/Y Registers
   */
  TGA2_WRITE_RAMDAC_REG(IBM561_CURSOR_HS_REG, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_CURSOR_HS_REG >> 8, HI_ADDR);

  /*  Cursor "x" Hot Spot Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Cursor "y" Hot Spot Register */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Cursor "x" Location Register (low byte) */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  Cursor "x" Location Register (high byte) */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*  Cursor "y" Location Register (low byte) */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  Cursor "y" Location Register (high byte) */
  TGA2_WRITE_RAMDAC_REG (0x00, REGS_ADDR);

  /*
   *  VRAM Mask regs (used for diag purposes, reset them just in case)
   */
  TGA2_WRITE_RAMDAC_REG(IBM561_VRAM_MASK_REG, LO_ADDR);
  TGA2_WRITE_RAMDAC_REG(IBM561_VRAM_MASK_REG >> 8, HI_ADDR);

  /*  VRAM mask register 1 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 2 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 3 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 4 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 5 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 6 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /*  VRAM mask register 7 */
  TGA2_WRITE_RAMDAC_REG (0xff, REGS_ADDR);

  /* Finally, do colormaps and windowtags */
  IBM561InitColormap(pScrn);
  IBM561WindowTagsInit(pScrn);
}

void
IBM561ramdacRestore(ScrnInfoPtr pScrn, unsigned char *Ibm561)
{
#if 0
  TGAPtr pTga = TGAPTR(pScrn);
#endif

#if 0
  /* ?? finally the stock stuff ?? */
  int i, j;
  /* ?? FIXME OR NOT this is currently copied from the BT463 */
  IBM561_WRITE(IBM561_REG_ACC, IBM561_CMD_REG_0, Ibm561[0]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_CMD_REG_1, Ibm561[1]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_CMD_REG_2, Ibm561[2]);
  
  IBM561_WRITE(IBM561_REG_ACC, IBM561_READ_MASK_0, Ibm561[3]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_READ_MASK_1, Ibm561[4]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_READ_MASK_2, Ibm561[5]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_READ_MASK_3, Ibm561[6]);
  
  IBM561_WRITE(IBM561_REG_ACC, IBM561_BLINK_MASK_0, Ibm561[7]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_BLINK_MASK_1, Ibm561[8]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_BLINK_MASK_2, Ibm561[9]);
  IBM561_WRITE(IBM561_REG_ACC, IBM561_BLINK_MASK_3, Ibm561[10]);
  
  IBM561_LOAD_ADDR(IBM561_WINDOW_TYPE_BASE);
  TGA_WRITE_REG((IBM561_REG_ACC<<2), TGA_RAMDAC_SETUP_REG);
  
  for (i = 0, j = 11; i < 16; i++) {
    TGA_WRITE_REG(Ibm561[j++]|(IBM561_REG_ACC<<10), TGA_RAMDAC_REG);
    TGA_WRITE_REG(Ibm561[j++]|(IBM561_REG_ACC<<10), TGA_RAMDAC_REG);
    TGA_WRITE_REG(Ibm561[j++]|(IBM561_REG_ACC<<10), TGA_RAMDAC_REG);
  }
#endif

/*
  fprintf(stderr, "IBM561ramdacRestore (%p)\n", Ibm561);
  for (i=0; i<58; i++)
    fprintf(stderr, "%2d: 0x%02x\n", i, (unsigned)Ibm561[i]);
*/

}

