/* 
 * Id: newport_shadow.c,v 1.3 2000/11/29 20:58:10 agx Exp $
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/newport/newport_shadow.c,v 1.3 2002/09/30 22:17:55 alanh Exp $ */

#include "newport.h"

void
NewportRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
	int dx, dy, x;
	CARD32 *base, *src;
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = pNewport->pNewportRegs;

#define RA8_BYTES	4	/* burst 4 pixels each time */
#define RA8_BYTE_SHIFT  2 	/* 4 Pixels on each burst, so divide ShadowPitch by 4 */
#define RA8_MASK        0xffc   /* move to 4 byte boundary   */

	TRACE_ENTER("NewportRefreshArea8");
	NewportWait(pNewportRegs);
	pNewportRegs->set.drawmode0 = (NPORT_DMODE0_DRAW | 
					NPORT_DMODE0_BLOCK | 
					NPORT_DMODE0_CHOST);
	while(num--) {
		NewportWait(pNewportRegs);
		x = pbox->x1 & RA8_MASK;  	/* move x to 4 byte boundary */
		base = pNewport->ShadowPtr 
				+ (pbox->y1 * (pNewport->ShadowPitch >> RA8_BYTE_SHIFT) ) 
				+ ( x >> RA8_BYTE_SHIFT);

		pNewportRegs->set.xystarti = (x << 16) | pbox->y1;
		pNewportRegs->set.xyendi = ((pbox->x2-1) << 16) | (pbox->y2-1);

		for ( dy = pbox->y1; dy < pbox->y2; dy++) {

			src = base;
			for ( dx = x; dx < pbox->x2; dx += RA8_BYTES) {
				pNewportRegs->go.hostrw0 = *src;  
				src++;
			}
			base += ( pNewport->ShadowPitch >> RA8_BYTE_SHIFT );
		}
		pbox++;
	}
	TRACE_EXIT("NewportRefreshArea8");
}


void
NewportRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
	int dx, dy;
	CARD8 *src, *base;
	CARD32 dest;
	NewportPtr pNewport = NEWPORTPTR(pScrn);
	NewportRegsPtr pNewportRegs = pNewport->pNewportRegs;

	TRACE_ENTER("NewportRefreshArea24");
	NewportWait(pNewportRegs);

	/* block transfers */
	pNewportRegs->set.drawmode0 = (NPORT_DMODE0_DRAW | 
					NPORT_DMODE0_BLOCK | 
					NPORT_DMODE0_CHOST);

	while(num--) {
		base = (CARD8*)pNewport->ShadowPtr + pbox->y1 * pNewport->ShadowPitch + pbox->x1 * 3;
		
		pNewportRegs->set.xystarti = (pbox->x1 << 16) | pbox->y1;
		pNewportRegs->set.xyendi = ((pbox->x2-1) << 16) | (pbox->y2-1);
		
		for ( dy = pbox->y1; dy < pbox->y2; dy++) {
			src = base;
			for ( dx = pbox->x1 ;  dx < pbox->x2 ; dx++) {
				/* Removing these shifts by using 32bpp fb
				 * yields < 2% percent performance gain and wastes 25% memory 
				 */
				dest = src[0] | src[1] << 8 | src[2] << 16;
				pNewportRegs->go.hostrw0 = dest;	
				src+=3;
 			}
			base += pNewport->ShadowPitch;
 		}
 		pbox++;
 	}
	TRACE_EXIT("NewportRefreshArea24");
}

