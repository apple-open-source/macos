/*
 *  CS8420_hw.h
 *  AppleOnboardAudio
 *
 *  Created by Raymond Montagne on Wed Feb 19 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef __CS8420
#define	__CS8420

#include <libkern/OSTypes.h>

enum gpio{
		intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
		positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
		dualEdge				=	1,		//		1 = enable both edges
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
		gpioDATA				=	0,		//	bit address:	the gpio itself
		gpioBIT_MASK			=	1		//	value shifted by bit position to be used to determine a GPIO bit state
};


#endif
