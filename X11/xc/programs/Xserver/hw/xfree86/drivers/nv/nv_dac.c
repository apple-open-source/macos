/* $XConsortium: nv_driver.c /main/3 1996/10/28 05:13:37 kaleb $ */
/*
 * Copyright 1996-1997  David J. McKay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Hacked together from mga driver and 3.3.4 NVIDIA driver by Jarno Paananen
   <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_dac.c,v 1.31 2003/01/02 20:44:56 mvojkovi Exp $ */

#include "nv_include.h"

Bool
NVDACInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    int i;
    int horizDisplay    = (mode->CrtcHDisplay/8)   - 1;
    int horizStart      = (mode->CrtcHSyncStart/8) - 1;
    int horizEnd        = (mode->CrtcHSyncEnd/8)   - 1;
    int horizTotal      = (mode->CrtcHTotal/8)     - 5;
    int horizBlankStart = (mode->CrtcHDisplay/8)   - 1;
    int horizBlankEnd   = (mode->CrtcHTotal/8)     - 1;
    int vertDisplay     =  mode->CrtcVDisplay      - 1;
    int vertStart       =  mode->CrtcVSyncStart    - 1;
    int vertEnd         =  mode->CrtcVSyncEnd      - 1;
    int vertTotal       =  mode->CrtcVTotal        - 2;
    int vertBlankStart  =  mode->CrtcVDisplay      - 1;
    int vertBlankEnd    =  mode->CrtcVTotal        - 1;
   

    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg = &pNv->ModeReg;
    NVFBLayout *pLayout = &pNv->CurrentLayout;
    vgaRegPtr   pVga;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVDACInit\n"));
    
    /*
     * This will initialize all of the generic VGA registers.
     */
    if (!vgaHWInit(pScrn, mode))
        return(FALSE);

    pVga = &VGAHWPTR(pScrn)->ModeReg;

    /*
     * Set all CRTC values.
     */

    if(mode->Flags & V_INTERLACE) 
        vertTotal |= 1;

    if(pNv->FlatPanel == 1) {
       vertStart = vertTotal - 3;  
       vertEnd = vertTotal - 2;
       vertBlankStart = vertStart;
       horizStart = horizTotal - 3;
       horizEnd = horizTotal - 2;   
       horizBlankEnd = horizTotal + 4;    
    }

    pVga->CRTC[0x0]  = Set8Bits(horizTotal);
    pVga->CRTC[0x1]  = Set8Bits(horizDisplay);
    pVga->CRTC[0x2]  = Set8Bits(horizBlankStart);
    pVga->CRTC[0x3]  = SetBitField(horizBlankEnd,4:0,4:0) 
                       | SetBit(7);
    pVga->CRTC[0x4]  = Set8Bits(horizStart);
    pVga->CRTC[0x5]  = SetBitField(horizBlankEnd,5:5,7:7)
                       | SetBitField(horizEnd,4:0,4:0);
    pVga->CRTC[0x6]  = SetBitField(vertTotal,7:0,7:0);
    pVga->CRTC[0x7]  = SetBitField(vertTotal,8:8,0:0)
                       | SetBitField(vertDisplay,8:8,1:1)
                       | SetBitField(vertStart,8:8,2:2)
                       | SetBitField(vertBlankStart,8:8,3:3)
                       | SetBit(4)
                       | SetBitField(vertTotal,9:9,5:5)
                       | SetBitField(vertDisplay,9:9,6:6)
                       | SetBitField(vertStart,9:9,7:7);
    pVga->CRTC[0x9]  = SetBitField(vertBlankStart,9:9,5:5)
                       | SetBit(6)
                       | ((mode->Flags & V_DBLSCAN) ? 0x80 : 0x00);
    pVga->CRTC[0x10] = Set8Bits(vertStart);
    pVga->CRTC[0x11] = SetBitField(vertEnd,3:0,3:0) | SetBit(5);
    pVga->CRTC[0x12] = Set8Bits(vertDisplay);
    pVga->CRTC[0x13] = ((pLayout->displayWidth/8)*(pLayout->bitsPerPixel/8));
    pVga->CRTC[0x15] = Set8Bits(vertBlankStart);
    pVga->CRTC[0x16] = Set8Bits(vertBlankEnd);

    pVga->Attribute[0x10] = 0x01;

    nvReg->screen = SetBitField(horizBlankEnd,6:6,4:4)
                  | SetBitField(vertBlankStart,10:10,3:3)
                  | SetBitField(vertStart,10:10,2:2)
                  | SetBitField(vertDisplay,10:10,1:1)
                  | SetBitField(vertTotal,10:10,0:0);

    nvReg->horiz  = SetBitField(horizTotal,8:8,0:0) 
                  | SetBitField(horizDisplay,8:8,1:1)
                  | SetBitField(horizBlankStart,8:8,2:2)
                  | SetBitField(horizStart,8:8,3:3);

    nvReg->extra  = SetBitField(vertTotal,11:11,0:0)
                    | SetBitField(vertDisplay,11:11,2:2)
                    | SetBitField(vertStart,11:11,4:4)
                    | SetBitField(vertBlankStart,11:11,6:6);

    if(mode->Flags & V_INTERLACE) {
       horizTotal = (horizTotal >> 1) & ~1;
       nvReg->interlace = Set8Bits(horizTotal);
       nvReg->horiz |= SetBitField(horizTotal,8:8,4:4);
    } else {
       nvReg->interlace = 0xff;  /* interlace off */
    }


    /*
     * Initialize DAC palette.
     */
    if(pLayout->bitsPerPixel != 8 )
    {
        for (i = 0; i < 256; i++)
        {
            pVga->DAC[i*3]     = i;
            pVga->DAC[(i*3)+1] = i;
            pVga->DAC[(i*3)+2] = i;
        }
    }
    
    /*
     * Calculate the extended registers.
     */

    if(pLayout->depth < 24) 
	i = pLayout->depth;
    else i = 32;

    if(pNv->riva.Architecture >= NV_ARCH_10)
	pNv->riva.CURSOR = (U032 *)(pNv->FbStart + pNv->riva.CursorStart);

    pNv->riva.LockUnlock(&pNv->riva, 0);

    pNv->riva.CalcStateExt(&pNv->riva, 
                           nvReg,
                           i,
                           pLayout->displayWidth,
                           mode->CrtcHDisplay,
                           pScrn->virtualY,
                           mode->Clock,
			   mode->Flags);

    nvReg->scale = pNv->riva.PRAMDAC[0x00000848/4] & 0xfff000ff;
    if(pNv->FlatPanel == 1) {
       nvReg->pixel |= (1 << 7);
       nvReg->scale |= (1 << 8) ;
    }
    if(pNv->SecondCRTC) {
       nvReg->head  = pNv->riva.PCRTC0[0x00000860/4] & ~0x00001000;
       nvReg->head2 = pNv->riva.PCRTC0[0x00002860/4] | 0x00001000;
       nvReg->crtcOwner = 3;
       nvReg->pllsel |= 0x20000800;
       nvReg->vpll2 = nvReg->vpll;
    } else 
    if(pNv->riva.twoHeads) {
       nvReg->head  =  pNv->riva.PCRTC0[0x00000860/4] | 0x00001000;
       nvReg->head2 =  pNv->riva.PCRTC0[0x00002860/4] & ~0x00001000;
       nvReg->crtcOwner = 0;
       nvReg->vpll2 = pNv->riva.PRAMDAC0[0x00000520/4];
    }

    nvReg->cursorConfig = 0x00000100;
    if(mode->Flags & V_DBLSCAN)
       nvReg->cursorConfig |= (1 << 4);
    if(pNv->alphaCursor) {
        nvReg->cursorConfig |= 0x04011000;
        nvReg->general |= (1 << 29);

        if((pNv->Chipset & 0x0ff0) == 0x0110) {
            nvReg->dither = pNv->riva.PRAMDAC[0x0528/4] & ~0x00010000;
            if(pNv->riva.flatPanel & FP_DITHER)
               nvReg->dither |= 0x00010000;
            else
               nvReg->cursorConfig |= (1 << 28);
        } else 
        if((pNv->riva.Chipset & 0x0ff0) >= 0x0170) {
           nvReg->dither = pNv->riva.PRAMDAC[0x083C/4] & ~1;
           nvReg->cursorConfig |= (1 << 28);
           if(pNv->riva.flatPanel & FP_DITHER)
              nvReg->dither |= 1;
        } else {
           nvReg->cursorConfig |= (1 << 28);
        }
    } else
       nvReg->cursorConfig |= 0x02000000;

    nvReg->vpllB = 0;
    nvReg->vpll2B = 0;

    return (TRUE);
}

void 
NVDACRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, NVRegPtr nvReg,
             Bool primary)
{
    NVPtr pNv = NVPTR(pScrn);
    int restore = VGA_SR_MODE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVDACRestore\n"));

    if(primary) restore |= VGA_SR_CMAP | VGA_SR_FONTS;
    else if((pNv->Chipset & 0xffff) == 0x0018) 
	restore |= VGA_SR_CMAP;
    pNv->riva.LoadStateExt(&pNv->riva, nvReg);
#if defined(__powerpc__)
    restore &= ~VGA_SR_FONTS;
#endif
    vgaHWRestore(pScrn, vgaReg, restore);
}

/*
 * NVDACSave
 *
 * This function saves the video state.
 */
void
NVDACSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, NVRegPtr nvReg,
          Bool saveFonts)
{
    NVPtr pNv = NVPTR(pScrn);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVDACSave\n"));

#if defined(__powerpc__)
    saveFonts = FALSE;
#endif

    vgaHWSave(pScrn, vgaReg, VGA_SR_CMAP | VGA_SR_MODE | 
                             (saveFonts? VGA_SR_FONTS : 0));
    pNv->riva.UnloadStateExt(&pNv->riva, nvReg);

    if((pNv->Chipset & 0x0ff0) == 0x0110) 
       nvReg->crtcOwner = ((pNv->Chipset & 0x0fff) == 0x0112) ? 3 : 0;
}

#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
#define MAKE_INDEX(in, w) (DEPTH_SHIFT(in, w) * 3)

void
NVDACLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
                 VisualPtr pVisual )
{
    int i, index;
    NVPtr pNv = NVPTR(pScrn);
    vgaRegPtr   pVga;

    pVga = &VGAHWPTR(pScrn)->ModeReg;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVDACLoadPalette\n"));

    if((pNv->riva.Architecture == NV_ARCH_03) && 
       (pNv->CurrentLayout.depth != 8))
           return;

    switch(pNv->CurrentLayout.depth) {
    case 15:
        for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[MAKE_INDEX(index, 5) + 0] = colors[index].red;
            pVga->DAC[MAKE_INDEX(index, 5) + 1] = colors[index].green;
            pVga->DAC[MAKE_INDEX(index, 5) + 2] = colors[index].blue;
        }
        break;
    case 16:
        for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[MAKE_INDEX(index, 6) + 1] = colors[index].green;
	    if(index < 32) {
            	pVga->DAC[MAKE_INDEX(index, 5) + 0] = colors[index].red;
            	pVga->DAC[MAKE_INDEX(index, 5) + 2] = colors[index].blue;
	    }
        }
        break;
    default:
	for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[index*3]     = colors[index].red;
            pVga->DAC[(index*3)+1] = colors[index].green;
            pVga->DAC[(index*3)+2] = colors[index].blue;
	}
	break;
    }
    vgaHWRestore(pScrn, pVga, VGA_SR_CMAP);
}

/*
 * DDC1 support only requires DDC_SDA_MASK,
 * DDC2 support requires DDC_SDA_MASK and DDC_SCL_MASK
 */
#define DDC_SDA_READ_MASK  (1 << 3)
#define DDC_SCL_READ_MASK  (1 << 2)
#define DDC_SDA_WRITE_MASK (1 << 4)
#define DDC_SCL_WRITE_MASK (1 << 5)

static unsigned int
NV_ddc1Read(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    unsigned char val;

    /* wait for Vsync */
    while(VGA_RD08(pNv->riva.PCIO, 0x3da) & 0x08);
    while(!(VGA_RD08(pNv->riva.PCIO, 0x3da) & 0x08));

    /* Get the result */
    VGA_WR08(pNv->riva.PCIO, 0x3d4, pNv->DDCBase);
    val = VGA_RD08(pNv->riva.PCIO, 0x3d5);
    DEBUG(ErrorF("NV_ddc1Read(%p,...) returns %d\n",
                 pScrn, val));
    return (val & DDC_SDA_READ_MASK) != 0;
}

static void
NV_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    /* Get the result. */
    VGA_WR08(pNv->riva.PCIO, 0x3d4, pNv->DDCBase);
    val = VGA_RD08(pNv->riva.PCIO, 0x3d5);

    *clock = (val & DDC_SCL_READ_MASK) != 0;
    *data  = (val & DDC_SDA_READ_MASK) != 0;
    DEBUG(ErrorF("NV_I2CGetBits(%p,...) val=0x%x, returns clock %d, data %d\n",
                 b, val, *clock, *data));
}

static void
NV_I2CPutBits(I2CBusPtr b, int clock, int data)
{
    NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    VGA_WR08(pNv->riva.PCIO, 0x3d4, pNv->DDCBase + 1);
    val = VGA_RD08(pNv->riva.PCIO, 0x3d5) & 0xf0;
    if (clock)
        val |= DDC_SCL_WRITE_MASK;
    else
        val &= ~DDC_SCL_WRITE_MASK;

    if (data)
        val |= DDC_SDA_WRITE_MASK;
    else
        val &= ~DDC_SDA_WRITE_MASK;

    VGA_WR08(pNv->riva.PCIO, 0x3d4, pNv->DDCBase + 1);
    VGA_WR08(pNv->riva.PCIO, 0x3d5, val | 0x1);
    
    DEBUG(ErrorF("NV_I2CPutBits(%p, %d, %d) val=0x%x\n", b, clock, data, val));
}

static Bool
NV_i2cInit(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pNv->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = NV_I2CPutBits;
    I2CPtr->I2CGetBits = NV_I2CGetBits;
    I2CPtr->AcknTimeout = 5;

    if (!xf86I2CBusInit(I2CPtr)) {
        return FALSE;
    }
    return TRUE;
}

/*
 * NVRamdacInit
 */
void
NVRamdacInit(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVRamdacInit\n"));
    pNv->ddc1Read = NV_ddc1Read;
    /* vgaHWddc1SetSpeed will only work if the card is in VGA mode */
    pNv->DDC1SetSpeed = vgaHWddc1SetSpeed;
    pNv->i2cInit = NV_i2cInit;
}

