/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ark/ark.h,v 1.3 2002/01/25 21:55:56 tsi Exp $ */
/*
 * ark
 */

#ifndef _ARK_H
#define _ARK_H

#include "xf86.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xf86_ansic.h"
#include "vgaHW.h"

typedef struct _ARKRegRec {
	unsigned char		sr10, sr11, sr12, sr13, sr14,
				sr15, sr16, sr17, sr18, sr20,
				sr21, sr22, sr23, sr24, sr25,
				sr26, sr27, sr28, sr29, sr2a,
				sr2b;
	unsigned char		sr1c, sr1d;
	unsigned char		cr40, cr41, cr42, cr44, cr46;
	unsigned char		dac_command;
	unsigned char		stg_17xx[3];
	unsigned char		gendac[6];
} ARKRegRec, *ARKRegPtr;


typedef struct _ARKRec {
	pciVideoPtr		PciInfo;
	PCITAG			PciTag;
	EntityInfoPtr		pEnt;
	CARD32			IOAddress;
	CARD32			FBAddress;
	unsigned char *		FBBase;
	unsigned char *		MMIOBase;
	unsigned long		videoRam;
	OptionInfoPtr		Options;
	unsigned int		Flags;
	Bool			NoAccel;
	CARD32			Bus;
	XAAInfoRecPtr		pXAA;
	int			Chipset, ChipRev;
	int			clock_mult;
	int			dac_width;
	int			multiplex_threshold;
	int			ramdac;
	ARKRegRec		SavedRegs;	/* original mode */
	ARKRegRec		ModeRegs;	/* current mode */
	Bool			(*CloseScreen)(int, ScreenPtr);
} ARKRec, *ARKPtr;


#define ARKPTR(p)	((ARKPtr)((p)->driverPrivate))


#define DRIVER_NAME	"ark"
#define DRIVER_VERSION	"0.5.0"
#define VERSION_MAJOR	0
#define VERSION_MINOR	5
#define PATCHLEVEL	0
#define ARK_VERSION	((VERSION_MAJOR << 24) | \
			 (VERSION_MINOR << 16) | \
			  PATCHLEVEL)

#define	ZOOMDAC		0x404
#define ATT490		0x101

Bool ARKAccelInit(ScreenPtr pScreen);

#define rdinx(port, ind)	(outb((port), (ind)), inb((port) + 1))
#define wrinx(port, ind, val)						\
    do {								\
	outb((port), (ind));  outb((port) + 1, (val));			\
    } while(0)
#define modinx(port, ind, mask, bits)					\
    do {								\
	unsigned char tmp;						\
	tmp = (rdinx((port), (ind)) & ~(mask)) | ((bits) & (mask));	\
	wrinx((port), (ind), tmp);					\
    } while(0)

#endif /* _ARK_H */
