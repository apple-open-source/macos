/*
 * Id: newport_cmap.c,v 1.1 2000/11/29 20:58:10 agx Exp $
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/newport/newport_cmap.c,v 1.3 2002/12/10 04:03:00 dawes Exp $ */

#include "newport.h"

#ifndef USEFIFOWAIT
#define USEFIFOWAIT 0
#endif

static void NewportCmapGetRGB( NewportRegsPtr pNewportRegs, unsigned short addr, LOCO *color);
#if USEFIFOWAIT
static void NewportCmapFifoWait( NewportRegsPtr pNewportRegs);
#endif

/* Load a colormap into the hardware */
void NewportLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, \
			LOCO* colors, VisualPtr pVisual)
{
	int i,index;
	NewportRegsPtr pNewportRegs = NEWPORTPTR(pScrn)->pNewportRegs;
	
	for(i = 0; i < numColors; i++) {
		index=indices[i];
 		NewportBfwait(pNewportRegs);
		NewportCmapSetRGB(pNewportRegs, index, colors[index]);
        }
}

void NewportBackupPalette(ScrnInfoPtr pScrn)
{
	int i;
	NewportPtr pNewport = NEWPORTPTR(pScrn);

	NewportWait(pNewport->pNewportRegs);
	for(i = 0; i < 256; i++) {
		NewportCmapGetRGB(pNewport->pNewportRegs, i, &(pNewport->txt_colormap[i]));
	}
}

#ifdef linux
/* stolen from kernel :) */
static unsigned char color_table[] = { 0, 4, 2, 6, 1, 5, 3, 7,
				       8,12,10,14, 9,13,11,15 };
 
/* the default colour table, for VGA+ colour systems */
static int default_red[] = {0x00,0xaa,0x00,0xaa,0x00,0xaa,0x00,0xaa,
    0x55,0xff,0x55,0xff,0x55,0xff,0x55,0xff};
static int default_grn[] = {0x00,0x00,0xaa,0x55,0x00,0x00,0xaa,0xaa,
    0x55,0x55,0xff,0xff,0x55,0x55,0xff,0xff};
static int default_blu[] = {0x00,0x00,0x00,0x00,0xaa,0xaa,0xaa,0xaa,
    0x55,0x55,0x55,0x55,0xff,0xff,0xff,0xff};
#endif

/* restore the default colormap */
void NewportRestorePalette(ScrnInfoPtr pScrn)
{
	int i;
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	
#ifdef linux
        for (i = 0; i < 16; i++) {
		pNewport->txt_colormap[color_table[i]].red = default_red[i];
		pNewport->txt_colormap[color_table[i]].green = default_grn[i];
		pNewport->txt_colormap[color_table[i]].blue = default_blu[i];
	}
#endif
	for(i = 0; i < 256; i++) {
		NewportCmapSetRGB(pNewport->pNewportRegs, i, pNewport->txt_colormap[i]);
	}
}

#if USEFIFOWAIT
/* wait 'til cmap fifo is completely empty */
static void NewportCmapFifoWait(NewportRegsPtr pNewportRegs)
{
        while(1) {
		pNewportRegs->set.dcbmode = (NPORT_DMODE_ACM0 |  NCMAP_PROTOCOL |
						NCMAP_REGADDR_SREG | NPORT_DMODE_W1);
		if(!(pNewportRegs->set.dcbdata0.bytes.b3 & 4))
			break;
        }
}
#endif

/* set the colormap entry at addr to color */
void NewportCmapSetRGB( NewportRegsPtr pNewportRegs, unsigned short addr, LOCO color)
{
	NewportWait(pNewportRegs);	/* this one should not be necessary */
	NewportBfwait(pNewportRegs);
	pNewportRegs->set.dcbmode = (NPORT_DMODE_ACMALL | NCMAP_PROTOCOL |
				NPORT_DMODE_SENDIAN | NPORT_DMODE_ECINC |
				NCMAP_REGADDR_AREG | NPORT_DMODE_W2);
	pNewportRegs->set.dcbdata0.hwords.s1 = addr;
	pNewportRegs->set.dcbmode = (NPORT_DMODE_ACMALL | NCMAP_PROTOCOL |
				 NCMAP_REGADDR_PBUF | NPORT_DMODE_W3);
	pNewportRegs->set.dcbdata0.all = (color.red << 24) |
						(color.green << 16) |
						(color.blue << 8);
}

/* get the colormap entry at addr */
static void NewportCmapGetRGB( NewportRegsPtr pNewportRegs, unsigned short addr, LOCO* color)
{
	npireg_t tmp;

	NewportBfwait(pNewportRegs);
	pNewportRegs->set.dcbmode = (NPORT_DMODE_ACM0 | NCMAP_PROTOCOL |
				NPORT_DMODE_SENDIAN | NPORT_DMODE_ECINC |
				NCMAP_REGADDR_AREG | NPORT_DMODE_W2);
	pNewportRegs->set.dcbdata0.hwords.s1 = addr;
	pNewportRegs->set.dcbmode = (NPORT_DMODE_ACM0 | NCMAP_PROTOCOL |
				 NCMAP_REGADDR_PBUF | NPORT_DMODE_W3);
	tmp = pNewportRegs->set.dcbdata0.all;
	color->red = (tmp >> 24) & 0xff;
	color->green = (tmp >> 16) & 0xff;
	color->blue = (tmp >> 8) & 0xff;
}

