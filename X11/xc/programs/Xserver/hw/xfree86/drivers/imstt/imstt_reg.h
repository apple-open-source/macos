/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/imstt/imstt_reg.h,v 1.6 2004/02/19 22:38:14 tsi Exp $ */

#ifndef _IMSTT_REG_H
#define _IMSTT_REG_H



#if defined(__powerpc__)

static __inline__ void regw(unsigned long base_addr, unsigned long regindex, unsigned long regdata)
{
	__asm__ __volatile__ ("stwbrx %1,%2,%3; eieio"
		      : "=m" (*(volatile unsigned *)(base_addr+regindex))
		      : "r" (regdata), "b" (regindex), "r" (base_addr));
}


static __inline__ unsigned long regr(unsigned long base_addr, unsigned long regindex)
{
	register unsigned long val;

	__asm__ __volatile__ ("lwbrx %0,%1,%2; eieio"
		      : "=r" (val)
		      : "b" (regindex), "r" (base_addr),
		      "m" (*(volatile unsigned *)(base_addr+regindex)));

	return (val);
}


#define INREG(addr)		regr(((unsigned long)(iptr->MMIOBase)), (addr))
#define OUTREG(addr, val)	regw(((unsigned long)(iptr->MMIOBase)), (addr), (val))

#else

#define INREG(addr)		MMIO_IN32(iptr->MMIOBase, addr)
#define OUTREG(addr, val)	MMIO_OUT32(iptr->MMIOBase, addr, val)

#endif

#define OUTREGPI(addr, val)	iptr->CMAPBase[IBM624_PIDXLO] = addr;	eieio(); \
				iptr->CMAPBase[IBM624_PIDXDATA] = val;	eieio()
#define OUTREGPT(addr, val)	iptr->CMAPBase[TVP_ADDRW] = addr;	eieio(); \
				iptr->CMAPBase[TVP_IDATA] = val;	eieio()

#define IMSTTMMIO_VARS()	\
	unsigned long *IMSTTMMIO = IMSTTPTR(pScrn)->MMIOBase


/* TwinTurbo (Cosmo) registers */

#define	IMSTT_S1SA	0x00
#define IMSTT_S2SA	0x04
#define IMSTT_SP	0x08
#define IMSTT_DSA	0x0c
#define IMSTT_CNT	0x10
#define IMSTT_DP_OCTL	0x14
#define	IMSTT_CLR	0x18
#define	IMSTT_BI	0x20
#define	IMSTT_MBC	0x24
#define	IMSTT_BLTCTL	0x28

/* scan timing generator registers */

#define	IMSTT_HES	0x30
#define	IMSTT_HEB	0x34
#define	IMSTT_HSB	0x38
#define	IMSTT_HT	0x3c
#define	IMSTT_VES	0x40
#define	IMSTT_VEB	0x44
#define	IMSTT_VSB	0x48
#define	IMSTT_VT	0x4c
#define	IMSTT_HCIV	0x50
#define	IMSTT_VCIV	0x54
#define	IMSTT_TCDR	0x58
#define	IMSTT_VIL	0x5c
#define	IMSTT_STGCTL	0x60

/* screen refresh generator registers */

#define	IMSTT_SSR	0x64
#define	IMSTT_HRIR	0x68
#define	IMSTT_SPR	0x6c
#define	IMSTT_CMR	0x70
#define	IMSTT_SRGCTL	0x74

/* RAM refresh generator registers */

#define	IMSTT_RRCIV	0x78
#define	IMSTT_RRSC	0x7c
#define	IMSTT_RRCR	0x88

/* system registers */

#define	IMSTT_GIOE	0x80
#define	IMSTT_GIO	0x84
#define	IMSTT_SCR	0x8c
#define	IMSTT_SSTATUS	0x90
#define	IMSTT_PRC	0x94


/* IBM 624 RAMDAC direct registers */

#define	IBM624_PADDRW	0x00
#define	IBM624_PDATA	0x04
#define	IBM624_PPMASK	0x08
#define	IBM624_PADDRR	0x0c
#define	IBM624_PIDXLO	0x10
#define	IBM624_PIDXHI	0x14
#define	IBM624_PIDXDATA	0x18
#define	IBM624_PIDXCTL	0x1c

/* IBM 624 RAMDAC indirect registers */

#define IBM624_CLKCTL	0x02	/* Misc Clock Control */
#define IBM624_SYNCCTL	0x03	/* Sync Control */
#define IBM624_HSYNCPOS	0x04	/* Horiz Sync Position */
#define IBM624_PWRMNGMT	0x05	/* Power Management */
#define IBM624_DACOP	0x06	/* DAC Operation */
#define IBM624_PALETCTL	0x07	/* Palette Control */
#define IBM624_SYSCLKCTL 0x08	/* System Clock Control */
#define IBM624_PIXFMT	0x0a	/* Pixel Format [bpp >> 3 + 2] */
#define IBM624_BPP8	0x0b	/* 8bpp */
#define IBM624_BPP16	0x0c	/* 16bpp */
#define IBM624_BPP24	0x0d	/* 24bpp */
#define IBM624_BPP32	0x0e	/* 32bpp */
#define IBM624_PIXCTL1	0x10	/* Pixel PLL Control 1 */
#define IBM624_PIXCTL2	0x11	/* Pixel PLL Control 2 */
#define IBM624_SYSCLKN	0x15	/* System Clock N */
#define IBM624_SYSCLKM	0x16	/* System Clock M */
#define IBM624_SYSCLKP	0x17	/* System Clock P */
#define IBM624_SYSCLKC	0x18	/* System Clock C */
#define IBM624_PIXM0	0x20	/* Pixel M 0 */
#define IBM624_PIXN0	0x21	/* Pixel N 0 */
#define IBM624_PIXP0	0x22	/* Pixel P 0 */
#define IBM624_PIXC0	0x23	/* Pixel C 0 */
#define IBM624_CURSCTL	0x30	/* Cursor Control */
#define IBM624_CURSXLO	0x31	/* Cursor X position, low 8 bits */
#define IBM624_CURSXHI	0x32	/* Cursor X position, high 8 bits */
#define IBM624_CURSYLO	0x33	/* Cursor Y position, low 8 bits */
#define IBM624_CURSYHI	0x34	/* Cursor Y postition, high 8 bits */
#define IBM624_CURSHOTX	0x35	/* Cursor Hot Spot X */
#define IBM624_CURSHOTY	0x36	/* Cursor Hot Spot Y */
#define IBM624_CURSACCTL 0x37	/* Advanced Cursor Control Enable */
#define IBM624_CURSACATTR 0x38	/* Advanced Cursor Attribute */
#define IBM624_CURS1R	0x40	/* Cursor 1 red */
#define IBM624_CURS1G	0x41	/* Cursor 1 green */
#define IBM624_CURS1B	0x42	/* Cursor 1 blue */
#define IBM624_CURS2R	0x43	/* Cursor 2 red */
#define IBM624_CURS2G	0x44	/* Cursor 2 green */
#define IBM624_CURS2B	0x45	/* Cursor 2 blue */
#define IBM624_CURS3R	0x46	/* Cursor 3 red */
#define IBM624_CURS3G	0x47	/* Cursor 3 green */
#define IBM624_CURS3B	0x48	/* Cursor 3 blue */
#define IBM624_BORDR	0x60	/* Border color red */
#define IBM624_BORDG	0x61	/* Border color green */
#define IBM624_BORDB	0x62	/* Border color blue */
#define IBM624_MISCTL1	0x70	/* Misc control 1 */
#define IBM624_MISCTL2	0x71	/* Misc control 2 */
#define IBM624_MISCTL3	0x72	/* Misc control 3 */
#define IBM624_KEYCTL	0x78	/* Key Control/DB operation */

/* TI TVP 3030 RAMDAC direct registers */

#define	TVP_ADDRW	0x00
#define	TVP_PDATA	0x04
#define	TVP_PMASK	0x08
#define	TVP_PADRR	0x0c
#define	TVP_CADRW	0x10
#define	TVP_CDATA	0x14
#define	TVP_CADRR	0x1c
#define	TVP_DCCTL	0x24
#define	TVP_IDATA	0x28
#define	TVP_CRDAT	0x2c
#define	TVP_CXPOL	0x30
#define	TVP_CXPOH	0x34
#define	TVP_CYPOL	0x38
#define	TVP_CYPOH	0x3c

/* TI TVP 3030 RAMDAC indirect registers */


#define	TVP_IRREV	0x01
#define	TVP_IRICC	0x06
#define	TVP_IRBRC	0x07
#define	TVP_IRLAC	0x0f
#define	TVP_IRTCC	0x18
#define	TVP_IRMXC	0x19
#define	TVP_IRCLS	0x1a
#define	TVP_IRPPG	0x1c
#define	TVP_IRGEC	0x1d
#define	TVP_IRMIC	0x1e
#define	TVP_IRPLA	0x2c
#define	TVP_IRPPD	0x2d
#define	TVP_IRMPD	0x2e
#define	TVP_IRLPD	0x2f
#define	TVP_IRCKL	0x30
#define	TVP_IRCKH	0x31
#define	TVP_IRCRL	0x32
#define	TVP_IRCRH	0x33
#define	TVP_IRCGL	0x34
#define	TVP_IRCGH	0x35
#define	TVP_IRCBL	0x36
#define	TVP_IRCBH	0x37
#define	TVP_IRCKC	0x38
#define	TVP_IRMLC	0x39
#define	TVP_IRSEN	0x3a
#define	TVP_IRTMD	0x3b
#define	TVP_IRRML	0x3c
#define	TVP_IRRMM	0x3d
#define	TVP_IRRMS	0x3e
#define	TVP_IRDID	0x3f
#define	TVP_IRRES	0xff

#endif /* _IMSTT_REG_H */
