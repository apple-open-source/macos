/*
 * Id: newport_regs.c,v 1.3 2000/11/29 20:58:10 agx Exp $
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/newport/newport_regs.c,v 1.7 2002/12/10 04:03:00 dawes Exp $ */

#include "newport.h"

static void NewportXmap9FifoWait(NewportRegsPtr pNewportRegs, unsigned long xmapChip);

void 
NewportVc2Set(NewportRegsPtr pNewportRegs, unsigned char vc2Ireg, unsigned short val)
{
	pNewportRegs->set.dcbmode = (NPORT_DMODE_AVC2 | VC2_REGADDR_INDEX | NPORT_DMODE_W3 |
					NPORT_DMODE_ECINC | VC2_PROTOCOL);
	pNewportRegs->set.dcbdata0.all = (vc2Ireg << 24) | (val << 8);
}

unsigned short 
NewportVc2Get(NewportRegsPtr pNewportRegs, unsigned char vc2Ireg)
{
	pNewportRegs->set.dcbmode = (NPORT_DMODE_AVC2 | VC2_REGADDR_INDEX | NPORT_DMODE_W1 |
					NPORT_DMODE_ECINC | VC2_PROTOCOL);
	pNewportRegs->set.dcbdata0.bytes.b3 = vc2Ireg;
	pNewportRegs->set.dcbmode = (NPORT_DMODE_AVC2 | VC2_REGADDR_IREG | NPORT_DMODE_W2 |
					NPORT_DMODE_ECINC | VC2_PROTOCOL);
	return pNewportRegs->set.dcbdata0.hwords.s1;
}

void 
NewportBackupVc2( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	pNewport->txt_vc2ctrl = NewportVc2Get( pNewportRegs, VC2_IREG_CONTROL );
	pNewport->txt_vc2cur_x = NewportVc2Get( pNewportRegs, VC2_IREG_CURSX );
	pNewport->txt_vc2cur_y = NewportVc2Get( pNewportRegs, VC2_IREG_CURSY );
}

void 
NewportRestoreVc2( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	NewportVc2Set( pNewportRegs, VC2_IREG_CONTROL, pNewport->txt_vc2ctrl );
	NewportVc2Set( pNewportRegs, VC2_IREG_CURSX, pNewport->txt_vc2cur_x );
	NewportVc2Set( pNewportRegs, VC2_IREG_CURSY, pNewport->txt_vc2cur_y );
}

void
NewportRestoreVc2Cursor( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportLoadCursorImage( pScrn, (CARD8*)pNewport->txt_vc2cur_data);
}

void
NewportBackupVc2Cursor( ScrnInfoPtr pScrn)
{
	int i;
	CARD16 tmp, *data;
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	data = (CARD16*)pNewport->txt_vc2cur_data;
	/* address of cursor data in vc2's ram */
        tmp = NewportVc2Get( pNewportRegs, VC2_IREG_CENTRY);
 	/* this is where we want to write to: */
        NewportVc2Set( pNewportRegs, VC2_IREG_RADDR, tmp);
        pNewportRegs->set.dcbmode = (NPORT_DMODE_AVC2 | VC2_REGADDR_RAM |
                               		NPORT_DMODE_W2 | VC2_PROTOCOL);
	/* read cursor data */
        for (i = 0; i < 128; i++) {
		NewportBfwait(pNewportRegs);
		*data = pNewportRegs->set.dcbdata0.hwords.s1;
		data++;
        }
}

/* Sometimes we just have to wait until we can do anything */
void
NewportWait(NewportRegsPtr pNewportRegs)
{
	while(1)
		if(!(pNewportRegs->cset.stat & NPORT_STAT_GBUSY))
			break;
}

void
NewportBfwait(NewportRegsPtr pNewportRegs)
{
	while(1)
		if(!(pNewportRegs->cset.stat & NPORT_STAT_BBUSY))
			break;
}

/* wait til an entry in the Xmap9's Mode Fifo is free (xmapChip = DCB_XMAP0 | DCB_XMAP1) */
static void
NewportXmap9FifoWait(NewportRegsPtr pNewportRegs, unsigned long xmapChip)
{
	while(1) {
		NewportBfwait( pNewportRegs);
		pNewportRegs->set.dcbmode = (xmapChip | R_DCB_XMAP9_PROTOCOL |
						XM9_CRS_FIFO_AVAIL | NPORT_DMODE_W1);
		if( (pNewportRegs->set.dcbdata0.bytes.b3) & 7 ) 
			break;
	}
}

void
NewportXmap9SetModeRegister(NewportRegsPtr pNewportRegs, CARD8 address, CARD32 mode)
{
	NewportXmap9FifoWait( pNewportRegs, DCB_XMAP0);
	NewportXmap9FifoWait( pNewportRegs, DCB_XMAP1);

	pNewportRegs->set.dcbmode = (DCB_XMAP_ALL | W_DCB_XMAP9_PROTOCOL |
			XM9_CRS_MODE_REG_DATA | NPORT_DMODE_W4 );
	pNewportRegs->set.dcbdata0.all = (address << 24) | ( mode & 0xffffff );
}

CARD32
NewportXmap9GetModeRegister(NewportRegsPtr pNewportRegs, unsigned chip, CARD8 address)
{
	CARD32 dcbaddr, i, val, mode = 0;
	CARD8 index = ( address << 2 );

	if(chip)
		dcbaddr = DCB_XMAP1;
	else
		dcbaddr = DCB_XMAP0;

	/* we have to read one byte at at time */
	for( i = 0; i < 4; i++ ) {
		NewportXmap9FifoWait( pNewportRegs, dcbaddr);

		pNewportRegs->set.dcbmode = ( dcbaddr | W_DCB_XMAP9_PROTOCOL |
				XM9_CRS_MODE_REG_INDEX | NPORT_DMODE_W1 );
		pNewportRegs->set.dcbdata0.bytes.b3 = (index | i);
		pNewportRegs->set.dcbmode = ( dcbaddr | W_DCB_XMAP9_PROTOCOL |
				XM9_CRS_MODE_REG_DATA | NPORT_DMODE_W1 );
		val = pNewportRegs->set.dcbdata0.bytes.b3;
		mode |= (val << ( i * 8 ) );
	}
	return mode;
}

void 
NewportBackupRex3( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	pNewport->txt_drawmode0 = pNewportRegs->set.drawmode0;
	pNewport->txt_drawmode1 = pNewportRegs->set.drawmode1;
	pNewport->txt_wrmask = pNewportRegs->set.wrmask;
	pNewport->txt_smask1x = pNewportRegs->cset.smask1x;
	pNewport->txt_smask1y = pNewportRegs->cset.smask1y;
	pNewport->txt_smask2x = pNewportRegs->cset.smask2x;
	pNewport->txt_smask2y = pNewportRegs->cset.smask2y;
	pNewport->txt_clipmode = pNewportRegs->cset.clipmode;
}

void
NewportRestoreRex3( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	pNewportRegs->set.drawmode0 = pNewport->txt_drawmode0;
	pNewportRegs->set.drawmode1 = pNewport->txt_drawmode1;
	pNewportRegs->set.wrmask = pNewport->txt_wrmask;
	pNewportRegs->cset.smask1x = pNewport->txt_smask1x;
	pNewportRegs->cset.smask1y = pNewport->txt_smask1y;
	pNewportRegs->cset.smask2x = pNewport->txt_smask2x;
	pNewportRegs->cset.smask2y = pNewport->txt_smask2y;
	pNewportRegs->cset.clipmode = pNewport->txt_clipmode;
}

void NewportBackupXmap9s( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	NewportBfwait(pNewport->pNewportRegs);
	/* config of xmap0 */
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | R_DCB_XMAP9_PROTOCOL |
			XM9_CRS_CONFIG | NPORT_DMODE_W1 );
	pNewport->txt_xmap9_cfg0 = pNewportRegs->set.dcbdata0.bytes.b3;
	NewportBfwait(pNewport->pNewportRegs);
	/* config of xmap1 */
	pNewportRegs->set.dcbmode = (DCB_XMAP1 | R_DCB_XMAP9_PROTOCOL |
				XM9_CRS_CONFIG | NPORT_DMODE_W1 );
	pNewport->txt_xmap9_cfg1 = pNewportRegs->set.dcbdata0.bytes.b3;
	NewportBfwait(pNewport->pNewportRegs);
	/* mode index register of xmap0 */
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | R_DCB_XMAP9_PROTOCOL |
				XM9_CRS_MODE_REG_INDEX | NPORT_DMODE_W1 );
	pNewport->txt_xmap9_mi = pNewportRegs->set.dcbdata0.bytes.b3; 
	/* mode register 0 of xmap 0 */
	pNewport->txt_xmap9_mod0 = NewportXmap9GetModeRegister(pNewportRegs, 0, 0);
	/* cursor cmap msb */
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | R_DCB_XMAP9_PROTOCOL |
			XM9_CRS_CURS_CMAP_MSB | NPORT_DMODE_W1 );
	pNewport->txt_xmap9_ccmsb = pNewportRegs->set.dcbdata0.bytes.b3;
}

void NewportRestoreXmap9s( ScrnInfoPtr pScrn)
{
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = NEWPORTREGSPTR(pScrn);

	/* mode register 0 */
	NewportXmap9SetModeRegister( pNewportRegs , 0, pNewport->txt_xmap9_mod0 );
	NewportBfwait(pNewport->pNewportRegs);
	/* mode index register */
	pNewportRegs->set.dcbmode = (DCB_XMAP_ALL | W_DCB_XMAP9_PROTOCOL |
				XM9_CRS_MODE_REG_INDEX | NPORT_DMODE_W1 );
	pNewportRegs->set.dcbdata0.bytes.b3 = pNewport->txt_xmap9_mi;
	NewportBfwait(pNewport->pNewportRegs);
	/* cfg xmap0 */
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | W_DCB_XMAP9_PROTOCOL |
				XM9_CRS_CONFIG | NPORT_DMODE_W1 );
	pNewportRegs->set.dcbdata0.bytes.b3 = pNewport->txt_xmap9_cfg0;
	NewportBfwait(pNewport->pNewportRegs);
	/* cfg xmap1 */
	pNewportRegs->set.dcbmode = (DCB_XMAP1 | W_DCB_XMAP9_PROTOCOL |
				XM9_CRS_CONFIG | NPORT_DMODE_W1 );
	pNewportRegs->set.dcbdata0.bytes.b3 = pNewport->txt_xmap9_cfg1;
	/* cursor cmap msb */
	pNewportRegs->set.dcbmode = (DCB_XMAP0 | R_DCB_XMAP9_PROTOCOL |
			XM9_CRS_CURS_CMAP_MSB | NPORT_DMODE_W1 );
	pNewportRegs->set.dcbdata0.bytes.b3 = pNewport->txt_xmap9_ccmsb;
}

