/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_image.c,v 1.6 2002/05/14 20:19:52 alanh Exp $ */

#include "savage_driver.h"
#include "xaarop.h"
#include "savage_bci.h"

void SavageSubsequentImageWriteRect (
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft);

void SavageSetupForImageWrite (
    ScrnInfoPtr pScrn,
    int rop,
    unsigned planemask,
    int transparency_color,
    int bpp,
    int depth);

void SavageWriteBitmapCPUToScreenColorExpand (
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char * src,
    int srcwidth,
    int skipleft,
    int fg, int bg,
    int rop,
    unsigned int planemask);

#if 0
void SavageWriteBitmapScreenToScreenColorExpand(
    ScrnPtr pScrn,
    int x, 
    int y,
    int w,
    int h,
    unsigned char * src,
    int srcwidth,
    int srcx,
    int srcy,
    int bg,
    int fg,
    int rop,
    unsigned int planemask
)
{
    SavagePtr psav = SAVPTR(pScrn);
    BCI_GET_PTR;
    unsigned int cmd;
    unsigned char * bd_offset;
    unsigned int bd;
    
    cmd = BCI_CMD_RECT | BCI_CMD_RECT_XP | BCI_CMD_RECT_YP
        | BCI_CMD_SEND_COLOR
        | BCI_CMD_DEST_GBD | BCI_CMD_SRC_SBD_MONO_NEW;
    cmd |= (bg != -1) ? BCI_CMD_SEND_COLOR : BCI_CMD_SRC_TRANSPARENT;
    cmd |= s3vAlu[rop];

    bd |= BCI_BD_BW_DISABLE;
    BCI_BD_SET_BPP(bd, 1);
    BCI_BD_SET_STRIDE(bd, srcwidth);
    bd_offset = srcwidth * srcy + (srcx >> 3) + src;

    psav->WaitQueue(psav,10);
    BCI_SEND(cmd);
    BCI_SEND((unsigned int)bd_offset);
    BCI_SEND(bd);
    BCI_SEND(fg);
    BCI_SEND((bg != -1) ? bg : 0);
    BCI_SEND(BCI_X_Y(srcx, srcy));
    BCI_SEND(BCI_X_Y(x, y));
    BCI_SEND(BCI_W_H(w, h));
}
#endif

void 
SavageWriteBitmapCPUToScreenColorExpand (
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char * src,
    int srcwidth,
    int skipleft,
    int fg, int bg,
    int rop,
    unsigned int planemask
)
{
    SavagePtr psav = SAVPTR(pScrn);
    BCI_GET_PTR;
    int i, j, count, reset;
    unsigned int cmd;
    CARD32 * srcp;

/* We aren't using planemask at all here... */

    if( !srcwidth )
	return;

    cmd = BCI_CMD_RECT | BCI_CMD_RECT_XP | BCI_CMD_RECT_YP
        | BCI_CMD_SEND_COLOR | BCI_CMD_CLIP_LR
        | BCI_CMD_DEST_GBD | BCI_CMD_SRC_MONO;
    cmd |= XAACopyROP[rop] << 16;

    if( bg == -1 )
        cmd |= BCI_CMD_SRC_TRANSPARENT;

    BCI_SEND(cmd);
    BCI_SEND(BCI_CLIP_LR(x+skipleft, x+w-1));
    BCI_SEND(fg);
    if( bg != -1 )
	BCI_SEND(bg);

    /* Bitmaps come in in units of DWORDS, LSBFirst.  This is exactly */
    /* reversed of what we expect.  */

    count = (w + 31) / 32;
/*    src += ((srcx & ~31) / 8); */

    /* The BCI region is 128k bytes.  A screen-sized mono bitmap can */
    /* exceed that. */

    reset = 65536 / count;
    
    for (j = 0; j < h; j ++) {
        BCI_SEND(BCI_X_Y(x, y+j));
        BCI_SEND(BCI_W_H(w, 1));
        srcp = (CARD32 *) src;
        for (i = count; i > 0; srcp ++, i --) {
            /* We have to invert the bits in each byte. */
            CARD32 u = *srcp;
            u = ((u & 0x0f0f0f0f) << 4) | ((u & 0xf0f0f0f0) >> 4);
            u = ((u & 0x33333333) << 2) | ((u & 0xcccccccc) >> 2);
            u = ((u & 0x55555555) << 1) | ((u & 0xaaaaaaaa) >> 1);
            BCI_SEND(u);
        }
        src += srcwidth;
        if( !--reset ) {
	    BCI_RESET;
            reset = 65536 / count;
        }
    }
}

void
SavageSetupForImageWrite(
    ScrnInfoPtr pScrn,
    int rop,
    unsigned planemask,
    int transparency_color,
    int bpp,
    int depth)
{
    SavagePtr psav = SAVPTR(pScrn);
    int cmd;

    cmd = BCI_CMD_RECT | BCI_CMD_RECT_XP | BCI_CMD_RECT_YP
        | BCI_CMD_CLIP_LR
        | BCI_CMD_DEST_GBD | BCI_CMD_SRC_COLOR;

    cmd |= XAACopyROP[rop] << 16;

    if( transparency_color != -1 )
        cmd |= BCI_CMD_SRC_TRANSPARENT;

    psav->SavedBciCmd = cmd;
    psav->SavedBgColor = transparency_color;
}


void SavageSubsequentImageWriteRect
(
    ScrnInfoPtr pScrn,
    int x,
    int y,
    int w,
    int h,
    int skipleft)
{
    SavagePtr psav = SAVPTR(pScrn);
    BCI_GET_PTR;
    int count;

    count = ((w * pScrn->bitsPerPixel + 31) / 32) * h;
    psav->WaitQueue( psav, count );
    BCI_SEND(psav->SavedBciCmd);
    BCI_SEND(BCI_CLIP_LR(x+skipleft, x+w-1));
    if( psav->SavedBgColor != -1 )
        BCI_SEND(psav->SavedBgColor);
    BCI_SEND(BCI_X_Y(x, y));
    BCI_SEND(BCI_W_H(w, h));
}
