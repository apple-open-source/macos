/*
 *  DallasROM.h
 *  AppleDallasDriver
 *
 *  Created by Keith Cox on Tue Jul 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

enum extInt_gpio{
		intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
		positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
		dualEdge				=	1		//		1 = enable both edges
};

enum gpio{
		gpioOS					=	4,		//	bit address:	output select
		gpioBit0isOutput		=	0,		//		use gpio bit 0 as output (default)
		gpioMediaBayIsOutput	=	1,		//		use media bay power
		gpioReservedOutputSel	=	2,		//		reserved
		gpioMPICopenCollector	=	3,		//		MPIC CPUInt2_1 (open collector)
		
		gpioAltOE				=	3,		//	bit address:	alternate output enable
		gpioOE_DDR				=	0,		//		use DDR for output enable
		gpioOE_Use_OS			=	1,		//		use gpioOS for output enable
		
		gpioDDR					=	2,		//	bit address:	r/w data direction
		gpioDDR_INPUT			=	0,		//		use for input (default)
		gpioDDR_OUTPUT			=	1,		//		use for output
		
		gpioPIN_RO				=	1,		//	bit address:	read only level on pin
		
		gpioDATA				=	0		//	bit address:	the gpio itself
};

enum READ_ROM_STATES {
	kSTATE_RESET_READ_MEMORY,
	kSTATE_CMD_SKIPROM_READ_MEMORY,
	kSTATE_CMD_READ_MEMORY,
	kSTATE_READ_MEMORY_ADDRESS,
	kSTATE_READ_MEMORY_DATA,
	kSTATE_RESET_READ_SCRATCHPAD,
	kSTATE_CMD_SKIPROM_SCRATCHPAD,
	kSTATE_CMD_SCRATCHPAD,
	kSTATE_SCRATCHPAD_ADDRESS,
	kSTATE_READ_SCRATCHPAD,
	kSTATE_COMPLETED
};

#define	kSCRATCHPAD_RETRY_STATE	kSTATE_RESET_READ_SCRATCHPAD
#define	kUSE_DESCRETE_BYTE_TRANSFER		0

#define kROMReadROM				0x33
#define kROMMatchROM			0x55
#define kROMSkipROM				0xCC
#define kROMSearchROM			0xF0
#define kROMSearchROM			0xF0
#define kROMWriteScratch		0x0F
#define kROMReadScratch			0xAA
#define kROMCopyScratch			0x55
#define kROMReadMemory			0xF0
#define kROMWriteAppReg			0x99
#define kROMReadStatusReg		0x66
#define kROMReadAppReg			0xC3
#define kROMCopyLockAppReg		0x5A
#define kROMValidationKey		0xA5

#define kROMResetPulseMin		480
#define kROMResetPulseMax		960
#define kROMPresenceDelayMax	 60
#define kROMPresencePulseMax	240
#define	kTSLOT_minimum			 60
#define	kTSLOT_maximum			120
#define	kTREC					  1
#define	kTLOW0_minimum			 60
#define	kTLOW0_maximum			( kTSLOT_maximum - 1 )
#define	kTLOW1_minimum			  1
#define	kTLOW1_maximum			 15
#define kROMWrite0Min			 60
#define kROMWrite0Max			120
#define kTSU					  1			/*	tSU indicates how long before 2430 drives bus		*/
#define	kTLOWR_minimum			  1			/*	tLOWR indicates how long master should drive bus	*/
#define	kTLOWR_maximum			 15			/*	tLOWR indicates how long master should drive bus	*/
#define	kTRDV					 15			/*	master ampling window								*/
#define	kTRELEASE_maximum		 45
#define kROMReadSetup			  2
#define kROMReadTime			 15
#define kROMReadMargin			  1
#define kROMRead1TimeMax		 (kROMReadTime-kROMReadMargin-2)
#define kROMRead0TimeMin		 (kROMReadTime+kROMReadMargin+2)

#define kNANOSECONDS_PER_MICROSECOND	1000
#define	kBITS_PER_BYTE					   8

#define	kSCRATCHPAD_XFER_DELAY_MAX	   11000	/*	Transfer to scratchpad will occur in under 11 milliseconds	*/

