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

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_dac.c,v 1.2 2003/09/16 00:17:46 mvojkovi Exp $ */

#include "riva_include.h"

Bool
RivaDACInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
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
   

    RivaPtr pRiva = RivaPTR(pScrn);
    RivaRegPtr rivaReg = &pRiva->ModeReg;
    RivaFBLayout *pLayout = &pRiva->CurrentLayout;
    vgaRegPtr   pVga;

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

    rivaReg->screen = SetBitField(horizBlankEnd,6:6,4:4)
                  | SetBitField(vertBlankStart,10:10,3:3)
                  | SetBitField(vertStart,10:10,2:2)
                  | SetBitField(vertDisplay,10:10,1:1)
                  | SetBitField(vertTotal,10:10,0:0);

    rivaReg->horiz  = SetBitField(horizTotal,8:8,0:0) 
                  | SetBitField(horizDisplay,8:8,1:1)
                  | SetBitField(horizBlankStart,8:8,2:2)
                  | SetBitField(horizStart,8:8,3:3);

    rivaReg->extra  = SetBitField(vertTotal,11:11,0:0)
                    | SetBitField(vertDisplay,11:11,2:2)
                    | SetBitField(vertStart,11:11,4:4)
                    | SetBitField(vertBlankStart,11:11,6:6);

    if(mode->Flags & V_INTERLACE) {
       horizTotal = (horizTotal >> 1) & ~1;
       rivaReg->interlace = Set8Bits(horizTotal);
       rivaReg->horiz |= SetBitField(horizTotal,8:8,4:4);
    } else {
       rivaReg->interlace = 0xff;  /* interlace off */
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

    pRiva->riva.CalcStateExt(&pRiva->riva, 
                           rivaReg,
                           i,
                           pLayout->displayWidth,
                           mode->CrtcHDisplay,
                           pScrn->virtualY,
                           mode->Clock,
			   mode->Flags);

    rivaReg->cursorConfig = 0x02000100;
    if(mode->Flags & V_DBLSCAN)
       rivaReg->cursorConfig |= (1 << 4);

    return (TRUE);
}

void 
RivaDACRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, RivaRegPtr rivaReg,
             Bool primary)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    int restore = VGA_SR_MODE;

    restore |= primary ? (VGA_SR_CMAP | VGA_SR_FONTS) : VGA_SR_CMAP;
    pRiva->riva.LoadStateExt(&pRiva->riva, rivaReg);
    vgaHWRestore(pScrn, vgaReg, restore);
}

/*
 * RivaDACSave
 *
 * This function saves the video state.
 */
void
RivaDACSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, RivaRegPtr rivaReg,
          Bool saveFonts)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    pRiva->riva.LockUnlock(&pRiva->riva, 0);

    vgaHWSave(pScrn, vgaReg, VGA_SR_CMAP | VGA_SR_MODE | 
                             (saveFonts? VGA_SR_FONTS : 0));
    pRiva->riva.UnloadStateExt(&pRiva->riva, rivaReg);
}

#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
#define MAKE_INDEX(in, w) (DEPTH_SHIFT(in, w) * 3)

void
RivaDACLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
                 VisualPtr pVisual )
{
    int i, index;
    RivaPtr pRiva = RivaPTR(pScrn);
    vgaRegPtr   pVga;

    pVga = &VGAHWPTR(pScrn)->ModeReg;

    if(pRiva->CurrentLayout.depth != 8)
           return;

    for(i = 0; i < numColors; i++) {
        index = indices[i];
        pVga->DAC[index*3]     = colors[index].red;
        pVga->DAC[(index*3)+1] = colors[index].green;
        pVga->DAC[(index*3)+2] = colors[index].blue;
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

static void
Riva_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    RivaPtr pRiva = RivaPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    /* Get the result. */
    VGA_WR08(pRiva->riva.PCIO, 0x3d4, pRiva->DDCBase);
    val = VGA_RD08(pRiva->riva.PCIO, 0x3d5);

    *clock = (val & DDC_SCL_READ_MASK) != 0;
    *data  = (val & DDC_SDA_READ_MASK) != 0;
}

static void
Riva_I2CPutBits(I2CBusPtr b, int clock, int data)
{
    RivaPtr pRiva = RivaPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    VGA_WR08(pRiva->riva.PCIO, 0x3d4, pRiva->DDCBase + 1);
    val = VGA_RD08(pRiva->riva.PCIO, 0x3d5) & 0xf0;
    if (clock)
        val |= DDC_SCL_WRITE_MASK;
    else
        val &= ~DDC_SCL_WRITE_MASK;

    if (data)
        val |= DDC_SDA_WRITE_MASK;
    else
        val &= ~DDC_SDA_WRITE_MASK;

    VGA_WR08(pRiva->riva.PCIO, 0x3d4, pRiva->DDCBase + 1);
    VGA_WR08(pRiva->riva.PCIO, 0x3d5, val | 0x1);
}

Bool
RivaDACi2cInit(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pRiva->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = Riva_I2CPutBits;
    I2CPtr->I2CGetBits = Riva_I2CGetBits;
    I2CPtr->AcknTimeout = 5;

    if (!xf86I2CBusInit(I2CPtr)) {
        return FALSE;
    }
    return TRUE;
}

