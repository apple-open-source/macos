/* $XConsortium: mga_bios.h /main/2 1996/10/28 04:48:23 kaleb $ */
#ifndef MGA_BIOS_H
#define MGA_BIOS_H

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_bios.h,v 1.3 1998/07/25 16:55:51 dawes Exp $ */

/*
 * MGABiosInfo - This struct describes the video BIOS info block.
 *
 * DESCRIPTION
 *   Do not mess with this, unless you know what you are doing.
 *   The data lengths and types are critical.
 *
 * HISTORY
 *   October 7, 1996 - [aem] Andrew E. Mileski
 *   This struct was shamelessly stolen from the MGA DDK.
 *   It has been reformatted, and the data types changed.
 */
typedef struct {
	/* Length of this structure in bytes */
	CARD16 StructLen;

	/*
	 * Unique number identifying the product type
	 * 0 : MGA-S1P20 (2MB base with 175MHz Ramdac)
	 * 1 : MGA-S1P21 (2MB base with 220MHz Ramdac)
	 * 2 : Reserved
	 * 3 : Reserved
	 * 4 : MGA-S1P40 (4MB base with 175MHz Ramdac)
	 * 5 : MGA-S1P41 (4MB base with 220MHz Ramdac)
	 */
	CARD16 ProductID;

	/* Serial number of the board */
	CARD8 SerNo[ 10 ];

	/*
	 * Manufacturing date of the board (at product test)
	 * Format: yyyy yyym mmmd dddd
	 */
	CARD16 ManufDate;

	/* Identification of manufacturing site */
	CARD16 ManufId;

	/*
	 * Number and revision level of the PCB
	 * Format: nnnn nnnn nnnr rrrr
	 *         n = PCB number ex:576 (from 0->2047)
	 *         r = PCB revision      (from 0->31)
	 */
	CARD16 PCBInfo;

	/* Identification of any PMBs */
	CARD16 PMBInfo;

	/*
	 * Bit  0-7  : Ramdac speed (0=175MHz, 1=220MHz)
	 * Bit  8-15 : Ramdac type  (0=TVP3026, 1=TVP3027)
	 */
	CARD16 RamdacType;

	/* Maximum PCLK of the ramdac */
	CARD16 PclkMax;

	/* Maximum LDCLK supported by the WRAM memory */
	CARD16 LclkMax;

	/* Maximum MCLK of base board */
	CARD16 ClkBase;

	/* Maximum MCLK of 4Mb board */
	CARD16 Clk4MB;

	/* Maximum MCLK of 8Mb board */
	CARD16 Clk8MB;

	/* Maximum MCLK of board with multimedia module */
	CARD16 ClkMod;

	/* Diagnostic test pass frequency */
	CARD16 TestClk;

	/* Default VGA mode1 pixel frequency */
	CARD16 VGAFreq1;

	/* Default VGA mode2 pixel frequency */
	CARD16 VGAFreq2;

	/* Date of last BIOS programming/update */
	CARD16 ProgramDate;

	/* Number of times BIOS has been programmed */
	CARD16 ProgramCnt;

	/* Support for up to 32 hardware/software options */
	CARD32 Options;

	/* Support for up to 32 hardware/software features */
	CARD32 FeatFlag;

	/* Definition of VGA mode MCLK */
	CARD16 VGAClk;

	/* Indicate the revision level of this header struct */
	CARD16 StructRev;

	CARD16 Reserved[ 3 ];
} MGABiosInfo;

/* from the PINS structure, refer pins info from MGA */
typedef struct tagParamMGA {
	CARD16 	PinID;		/* 0 */
	CARD8	StructLen;	/* 2 */
	CARD8	Rsvd1;		/* 3 */
	CARD16	StructRev;	/* 4 */
	CARD16	ProgramDate;	/* 6 */
	CARD16	ProgramCnt;	/* 8 */
	CARD16	ProductID;	/* 10 */
	CARD8	SerNo[16];	/* 12 */
	CARD8	PLInfo[6];	/* 28 */
	CARD16	PCBInfo;	/* 34 */
	CARD32	FeatFlag;	/* 36 */
	CARD8	RamdacType;	/* 40 */
	CARD8	RamdacSpeed;	/* 41 */
	CARD8	PclkMax;	/* 42 */
	CARD8	ClkGE;		/* 43 */
	CARD8   ClkMem;		/* 44 */
	CARD8	Clk4MB;		/* 45 */
	CARD8	Clk8MB;		/* 46 */
	CARD8	ClkMod;		/* 47 */
	CARD8	TestClk;	/* 48 */
	CARD8	VGAFreq1;	/* 49 */
	CARD8	VGAFreq2;	/* 50 */
	CARD8	MCTLWTST;	/* 51 */
	CARD8	VidCtrl;	/* 52 */
	CARD8	Clk12MB;	/* 53 */
	CARD8	Clk16MB;	/* 54 */
	CARD8	Reserved[8];	/* 55-62 */
	CARD8	PinCheck;	/* 63 */
}	MGABios2Info;

#endif
