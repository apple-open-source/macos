/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/imstt/imstt.h,v 1.5 2001/05/16 06:48:09 keithp Exp $ */

#ifndef _IMSTT_H
#define _IMSTT_H

#include "xf86.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xf86_ansic.h"


typedef struct _IMSTTRec {
	pciVideoPtr		PciInfo;
	PCITAG			PciTag;
	EntityInfoPtr		pEnt;
	CARD32			IOAddress;
	CARD32			FBAddress;
	unsigned char *		FBBase;
	unsigned long *		MMIOBase;
	unsigned char *		CMAPBase;
	long			FBMapSize;
	unsigned long		videoRam;
	unsigned long		ramdac;
	int			rev;
	OptionInfoPtr		Options;
	unsigned int		Flags;
	CARD32			Bus;
	XAAInfoRecPtr		AccelInfoRec;
/*	xf86CursorInfoPtr	CursorInfoRec; */
	Bool			NoAccel;
	Bool			HWCursor;
	Bool			InitDAC;
	Bool			FBDev;
	int			Chipset, ChipRev;
	int			ydir;
	int			color;
	unsigned long		pitch;
	unsigned long		ll;
	unsigned long		screen_width;
	unsigned long		sp;
	unsigned long		dp;
	unsigned long		cnt;
	unsigned long		bltctl;
	unsigned short		hes;
	unsigned short		heb;
	unsigned short		hsb;
	unsigned short		ht;
	unsigned short		ves;
	unsigned short		veb;
	unsigned short		vsb;
	unsigned short		vt;
	unsigned short		vil;
	unsigned char		pclk_m;
	unsigned char		pclk_n;
	unsigned char		pclk_p;
	unsigned char		mlc[3];
	unsigned char		lckl_p[3];
} IMSTTRec, *IMSTTPtr;

#if 0
#define IMSTTTRACE(s)	ErrorF(s)
#define IMSTTTRACE1(s,a)	ErrorF(s,a)
#else
#define IMSTTTRACE(s)
#define IMSTTTRACE1(s,a)
#endif

#define IMSTTPTR(p)	((IMSTTPtr)((p)->driverPrivate))

#define	PCI_IMSTT128	0x9128	/* IMS,tt128mbA */
#define PCI_IMSTT3D	0x9135	/* IMS,tt3d */

#define RAMDAC_IBM	1
#define RAMDAC_TVP	2

Bool IMSTTAccelInit(ScreenPtr pScreen);
    
#endif /* _IMSTT_H */
