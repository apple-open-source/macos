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

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_setup.c,v 1.1 2003/07/31 20:24:31 mvojkovi Exp $ */

#include "riva_include.h"

/*
 * Override VGA I/O routines.
 */
static void RivaWriteCrtc(vgaHWPtr pVga, CARD8 index, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PCIO, pVga->IOBase + VGA_CRTC_INDEX_OFFSET, index);
    VGA_WR08(pRiva->riva.PCIO, pVga->IOBase + VGA_CRTC_DATA_OFFSET,  value);
}
static CARD8 RivaReadCrtc(vgaHWPtr pVga, CARD8 index)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PCIO, pVga->IOBase + VGA_CRTC_INDEX_OFFSET, index);
    return (VGA_RD08(pRiva->riva.PCIO, pVga->IOBase + VGA_CRTC_DATA_OFFSET));
}
static void RivaWriteGr(vgaHWPtr pVga, CARD8 index, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PVIO, VGA_GRAPH_INDEX, index);
    VGA_WR08(pRiva->riva.PVIO, VGA_GRAPH_DATA,  value);
}
static CARD8 RivaReadGr(vgaHWPtr pVga, CARD8 index)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PVIO, VGA_GRAPH_INDEX, index);
    return (VGA_RD08(pRiva->riva.PVIO, VGA_GRAPH_DATA));
}
static void RivaWriteSeq(vgaHWPtr pVga, CARD8 index, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PVIO, VGA_SEQ_INDEX, index);
    VGA_WR08(pRiva->riva.PVIO, VGA_SEQ_DATA,  value);
}
static CARD8 RivaReadSeq(vgaHWPtr pVga, CARD8 index)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PVIO, VGA_SEQ_INDEX, index);
    return (VGA_RD08(pRiva->riva.PVIO, VGA_SEQ_DATA));
}
static void RivaWriteAttr(vgaHWPtr pVga, CARD8 index, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    volatile CARD8 tmp;

    tmp = VGA_RD08(pRiva->riva.PCIO, pVga->IOBase + VGA_IN_STAT_1_OFFSET);
    if (pVga->paletteEnabled)
        index &= ~0x20;
    else
        index |= 0x20;
    VGA_WR08(pRiva->riva.PCIO, VGA_ATTR_INDEX,  index);
    VGA_WR08(pRiva->riva.PCIO, VGA_ATTR_DATA_W, value);
}
static CARD8 RivaReadAttr(vgaHWPtr pVga, CARD8 index)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    volatile CARD8 tmp;

    tmp = VGA_RD08(pRiva->riva.PCIO, pVga->IOBase + VGA_IN_STAT_1_OFFSET);
    if (pVga->paletteEnabled)
        index &= ~0x20;
    else
        index |= 0x20;
    VGA_WR08(pRiva->riva.PCIO, VGA_ATTR_INDEX, index);
    return (VGA_RD08(pRiva->riva.PCIO, VGA_ATTR_DATA_R));
}
static void RivaWriteMiscOut(vgaHWPtr pVga, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PVIO, VGA_MISC_OUT_W, value);
}
static CARD8 RivaReadMiscOut(vgaHWPtr pVga)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    return (VGA_RD08(pRiva->riva.PVIO, VGA_MISC_OUT_R));
}
static void RivaEnablePalette(vgaHWPtr pVga)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    volatile CARD8 tmp;

    tmp = VGA_RD08(pRiva->riva.PCIO, pVga->IOBase + VGA_IN_STAT_1_OFFSET);
    VGA_WR08(pRiva->riva.PCIO, VGA_ATTR_INDEX, 0x00);
    pVga->paletteEnabled = TRUE;
}
static void RivaDisablePalette(vgaHWPtr pVga)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    volatile CARD8 tmp;

    tmp = VGA_RD08(pRiva->riva.PCIO, pVga->IOBase + VGA_IN_STAT_1_OFFSET);
    VGA_WR08(pRiva->riva.PCIO, VGA_ATTR_INDEX, 0x20);
    pVga->paletteEnabled = FALSE;
}
static void RivaWriteDacMask(vgaHWPtr pVga, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PDIO, VGA_DAC_MASK, value);
}
static CARD8 RivaReadDacMask(vgaHWPtr pVga)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    return (VGA_RD08(pRiva->riva.PDIO, VGA_DAC_MASK));
}
static void RivaWriteDacReadAddr(vgaHWPtr pVga, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PDIO, VGA_DAC_READ_ADDR, value);
}
static void RivaWriteDacWriteAddr(vgaHWPtr pVga, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PDIO, VGA_DAC_WRITE_ADDR, value);
}
static void RivaWriteDacData(vgaHWPtr pVga, CARD8 value)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    VGA_WR08(pRiva->riva.PDIO, VGA_DAC_DATA, value);
}
static CARD8 RivaReadDacData(vgaHWPtr pVga)
{
    RivaPtr pRiva = (RivaPtr)pVga->MMIOBase;
    return (VGA_RD08(pRiva->riva.PDIO, VGA_DAC_DATA));
}



static xf86MonPtr 
RivaProbeDDC (ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    xf86MonPtr MonInfo = NULL;

    if(!pRiva->I2C) return NULL;

    pRiva->DDCBase = 0x3e;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Probing for EDID...\n");

    if ((MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, pRiva->I2C))) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                  "  ... found one\n");
       xf86PrintEDID( MonInfo );
    } else {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
                  "  ... none found\n");
    }

    return MonInfo;
}

void
Riva3Setup(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    vgaHWPtr pVga = VGAHWPTR(pScrn);
    CARD32 regBase = pRiva->IOAddress;
    CARD32 frameBase = pRiva->FbAddress;
    xf86MonPtr monitor;
    int mmioFlags;
    
    pRiva->Save = RivaDACSave;
    pRiva->Restore = RivaDACRestore;
    pRiva->ModeInit = RivaDACInit;

    pRiva->Dac.LoadPalette = RivaDACLoadPalette;

    /*
     * Override VGA I/O routines.
     */
    pVga->writeCrtc         = RivaWriteCrtc;
    pVga->readCrtc          = RivaReadCrtc;
    pVga->writeGr           = RivaWriteGr;
    pVga->readGr            = RivaReadGr;
    pVga->writeAttr         = RivaWriteAttr;
    pVga->readAttr          = RivaReadAttr;
    pVga->writeSeq          = RivaWriteSeq;
    pVga->readSeq           = RivaReadSeq;
    pVga->writeMiscOut      = RivaWriteMiscOut;
    pVga->readMiscOut       = RivaReadMiscOut;
    pVga->enablePalette     = RivaEnablePalette;
    pVga->disablePalette    = RivaDisablePalette;
    pVga->writeDacMask      = RivaWriteDacMask;
    pVga->readDacMask       = RivaReadDacMask;
    pVga->writeDacWriteAddr = RivaWriteDacWriteAddr;
    pVga->writeDacReadAddr  = RivaWriteDacReadAddr;
    pVga->writeDacData      = RivaWriteDacData;
    pVga->readDacData       = RivaReadDacData;
    /*
     * Note: There are different pointers to the CRTC/AR and GR/SEQ registers.
     * Bastardize the intended uses of these to make it work.
     */
    pVga->MMIOBase   = (CARD8 *)pRiva;
    pVga->MMIOOffset = 0;
    
    /*
     * No IRQ in use.
     */
    pRiva->riva.EnableIRQ = 0;
    pRiva->riva.IO      = VGA_IOBASE_COLOR;

    mmioFlags = VIDMEM_MMIO | VIDMEM_READSIDEEFFECT;

    pRiva->riva.PRAMDAC = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00680000, 0x00003000);
    pRiva->riva.PFB     = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00100000, 0x00001000);
    pRiva->riva.PFIFO   = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00002000, 0x00002000);
    pRiva->riva.PGRAPH  = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00400000, 0x00002000);
    pRiva->riva.PEXTDEV = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00101000, 0x00001000);
    pRiva->riva.PTIMER  = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00009000, 0x00001000);
    pRiva->riva.PMC     = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00000000, 0x00009000);
    pRiva->riva.FIFO    = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                      regBase+0x00800000, 0x00010000);
    pRiva->riva.PRAMIN = xf86MapPciMem(pScrn->scrnIndex, mmioFlags, pRiva->PciTag,
                                     frameBase+0x00C00000, 0x00008000);

    /*
     * These registers are read/write as 8 bit values.  Probably have to map
     * sparse on alpha.
     */
    pRiva->riva.PCIO = (U008 *)xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                                           pRiva->PciTag, regBase+0x00601000,
                                           0x00003000);
    pRiva->riva.PDIO = (U008 *)xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                                           pRiva->PciTag, regBase+0x00681000,
                                           0x00003000);
    pRiva->riva.PVIO = (U008 *)xf86MapPciMem(pScrn->scrnIndex, mmioFlags,
                                           pRiva->PciTag, regBase+0x000C0000,
                                           0x00001000);

    pRiva->riva.PCRTC = pRiva->riva.PGRAPH;

    RivaGetConfig(pRiva);

    pRiva->riva.LockUnlock(&pRiva->riva, 0);

    RivaI2CInit(pScrn);

    monitor = RivaProbeDDC(pScrn);

    if(monitor)
      xf86SetDDCproperties(pScrn, monitor);

    pRiva->Dac.maxPixelClock = pRiva->riva.MaxVClockFreqKHz;
}
