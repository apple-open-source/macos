
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_dpms.c,v 1.10 2001/01/21 21:19:35 tsi Exp $ */






#include "tseng.h"

/*
 * TsengCrtcDPMSSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 * This routine is for the ET4000W32P rev. c and later, which can
 * use CRTC indexed register 34 to turn off H/V Sync signals.
 *
 * '97 Harald Nordgård Hansen
 */
void
TsengCrtcDPMSSet(ScrnInfoPtr pScrn,
    int PowerManagementMode, int flags)
{
    unsigned char seq1, crtc34;
    int iobase = VGAHWPTR(pScrn)->IOBase;

    xf86EnableAccess(pScrn);
    switch (PowerManagementMode) {
    case DPMSModeOn:
    default:
	/* Screen: On; HSync: On, VSync: On */
	seq1 = 0x00;
	crtc34 = 0x00;
	break;
    case DPMSModeStandby:
	/* Screen: Off; HSync: Off, VSync: On */
	seq1 = 0x20;
	crtc34 = 0x01;
	break;
    case DPMSModeSuspend:
	/* Screen: Off; HSync: On, VSync: Off */
	seq1 = 0x20;
	crtc34 = 0x20;
	break;
    case DPMSModeOff:
	/* Screen: Off; HSync: Off, VSync: Off */
	seq1 = 0x20;
	crtc34 = 0x21;
	break;
    }
    outb(0x3C4, 0x01);		       /* Select SEQ1 */
    seq1 |= inb(0x3C5) & ~0x20;
    outb(0x3C5, seq1);
    outb(iobase + 4, 0x34);	       /* Select CRTC34 */
    crtc34 |= inb(iobase + 5) & ~0x21;
    outb(iobase + 5, crtc34);
}

/*
 * TsengHVSyncDPMSSet --
 *
 * Sets VESA Display Power Management Signaling (DPMS) Mode.
 * This routine is for Tseng et4000 chips that do not have any
 * registers to disable sync output.
 *
 * The "classic" (standard VGA compatible) method; disabling all syncs,
 * causes video memory corruption on Tseng cards, according to "Tseng
 * ET4000/W32 family tech note #20":
 *
 *   "Setting CRTC Indexed Register 17 bit 7 = 0 will disable the video
 *    syncs (=VESA DPMS power down), but will also disable DRAM refresh cycles"
 *
 * The method used here is derived from the same tech note, which describes
 * a method to disable specific sync signals on chips that do not have
 * direct support for it:
 *
 *    To get vsync off, program VSYNC_START > VTOTAL
 *    (approximately). In particular, the formula used is:
 *
 *        VSYNC.ADJ = (VTOT - VSYNC.NORM) + VTOT + 4
 *
 *        To test for this state, test if VTOT + 1 < VSYNC
 *
 *
 *    To get hsync off, program HSYNC_START > HTOTAL
 *    (approximately). In particular, the following formula is used:
 *
 *        HSYNC.ADJ = (HTOT - HSYNC.NORM) + HTOT + 7
 *
 *        To test for this state, test if HTOT + 3 < HSYNC
 *
 * The advantage of these formulas is that the ON state can be restored by
 * reversing the formula. The original state need not be stored anywhere...
 *
 * The trick in the above approach is obviously to put the start of the sync
 * _beyond_ the total H or V counter range, which causes the sync to never
 * toggle.
 */
void
TsengHVSyncDPMSSet(ScrnInfoPtr pScrn,
    int PowerManagementMode, int flags)
{
    unsigned char seq1, tmpb;
    unsigned int HSync, VSync, HTot, VTot, tmp;
    Bool chgHSync, chgVSync;
    int iobase = VGAHWPTR(pScrn)->IOBase;

    /* Code here to read the current values of HSync through VTot:
     *  HSYNC:
     *    bits 0..7 : CRTC index 0x04
     *    bit 8     : CRTC index 0x3F, bit 4
     */
    outb(iobase + 4, 0x04);
    HSync = inb(iobase + 5);
    outb(iobase + 4, 0x3F);
    HSync += (inb(iobase + 5) & 0x10) << 4;
    /*  VSYNC:
     *    bits 0..7 : CRTC index 0x10
     *    bits 8..9 : CRTC index 0x07 bits 2 (VSYNC bit 8) and 7 (VSYNC bit 9)
     *    bit 10    : CRTC index 0x35 bit 3
     */
    outb(iobase + 4, 0x10);
    VSync = inb(iobase + 5);
    outb(iobase + 4, 0x07);
    tmp = inb(iobase + 5);
    VSync += ((tmp & 0x04) << 6) + ((tmp & 0x80) << 2);
    outb(iobase + 4, 0x35);
    VSync += (inb(iobase + 5) & 0x08) << 7;
    /*  HTOT:
     *    bits 0..7 : CRTC index 0x00.
     *    bit 8     : CRTC index 0x3F, bit 0
     */
    outb(iobase + 4, 0x00);
    HTot = inb(iobase + 5);
    outb(iobase + 4, 0x3F);
    HTot += (inb(iobase + 5) & 0x01) << 8;
    /*  VTOT:
     *    bits 0..7 : CRTC index 0x06
     *    bits 8..9 : CRTC index 0x07 bits 0 (VTOT bit 8) and 5 (VTOT bit 9)
     *    bit 10    : CRTC index 0x35 bit 1
     */
    outb(iobase + 4, 0x06);
    VTot = inb(iobase + 5);
    outb(iobase + 4, 0x07);
    tmp = inb(iobase + 5);
    VTot += ((tmp & 0x01) << 8) + ((tmp & 0x20) << 4);
    outb(iobase + 4, 0x35);
    VTot += (inb(iobase + 5) & 0x02) << 9;

    /* Don't write these unless we have to. */
    chgHSync = chgVSync = FALSE;

    switch (PowerManagementMode) {
    case DPMSModeOn:
    default:
	/* Screen: On; HSync: On, VSync: On */
	seq1 = 0x00;
	if (HSync > HTot + 3) {	       /* Sync is off now, turn it on. */
	    HSync = (HTot - HSync) + HTot + 7;
	    chgHSync = TRUE;
	}
	if (VSync > VTot + 1) {	       /* Sync is off now, turn it on. */
	    VSync = (VTot - VSync) + VTot + 4;
	    chgVSync = TRUE;
	}
	break;
    case DPMSModeStandby:
	/* Screen: Off; HSync: Off, VSync: On */
	seq1 = 0x20;
	if (HSync <= HTot + 3) {       /* Sync is on now, turn it off. */
	    HSync = (HTot - HSync) + HTot + 7;
	    chgHSync = TRUE;
	}
	if (VSync > VTot + 1) {	       /* Sync is off now, turn it on. */
	    VSync = (VTot - VSync) + VTot + 4;
	    chgVSync = TRUE;
	}
	break;
    case DPMSModeSuspend:
	/* Screen: Off; HSync: On, VSync: Off */
	seq1 = 0x20;
	if (HSync > HTot + 3) {	       /* Sync is off now, turn it on. */
	    HSync = (HTot - HSync) + HTot + 7;
	    chgHSync = TRUE;
	}
	if (VSync <= VTot + 1) {       /* Sync is on now, turn it off. */
	    VSync = (VTot - VSync) + VTot + 4;
	    chgVSync = TRUE;
	}
	break;
    case DPMSModeOff:
	/* Screen: Off; HSync: Off, VSync: Off */
	seq1 = 0x20;
	if (HSync <= HTot + 3) {       /* Sync is on now, turn it off. */
	    HSync = (HTot - HSync) + HTot + 7;
	    chgHSync = TRUE;
	}
	if (VSync <= VTot + 1) {       /* Sync is on now, turn it off. */
	    VSync = (VTot - VSync) + VTot + 4;
	    chgVSync = TRUE;
	}
	break;
    }

    /* If the new hsync or vsync overflows, don't change anything. */
    if (HSync >= 1 << 9 || VSync >= 1 << 11) {
	ErrorF("tseng: warning: Cannot go into DPMS from this resolution.\n");
	chgVSync = chgHSync = FALSE;
    }
    /* The code to turn on and off video output is equal for all. */
    if (chgHSync || chgVSync) {
	outb(0x3C4, 0x01);	       /* Select SEQ1 */
	seq1 |= inb(0x3C5) & ~0x20;
	outb(0x3C5, seq1);
    }
    /* Then the code to write VSync and HSync to the card.
     *  HSYNC:
     *    bits 0..7 : CRTC index 0x04
     *    bit 8     : CRTC index 0x3F, bit 4
     */
    if (chgHSync) {
	outb(iobase + 4, 0x04);
	tmpb = HSync & 0xFF;
	outb(iobase + 5, tmpb);
	outb(iobase + 4, 0x3F);
	tmpb = (HSync & 0x100) >> 4;
	tmpb |= inb(iobase + 5) & ~0x10;
	outb(iobase + 5, tmpb);
    }
    /*  VSYNC:
     *    bits 0..7 : CRTC index 0x10
     *    bits 8..9 : CRTC index 0x07 bits 2 (VSYNC bit 8) and 7 (VSYNC bit 9)
     *    bit 10    : CRTC index 0x35 bit 3
     */
    if (chgVSync) {
	outb(iobase + 4, 0x10);
	tmpb = VSync & 0xFF;
	outb(iobase + 5, tmpb);
	outb(iobase + 4, 0x07);
	tmpb = (VSync & 0x100) >> 6;
	tmpb |= (VSync & 0x200) >> 2;
	tmpb |= inb(iobase + 5) & ~0x84;
	outb(iobase + 5, tmpb);
	outb(iobase + 4, 0x35);
	tmpb = (VSync & 0x400) >> 7;
	tmpb |= inb(iobase + 5) & ~0x08;
	outb(iobase + 5, tmpb);
    }
}
