/*
 *  DallasROM.h
 *  AppleDallasDriver
 *
 *  Created by Keith Cox on Tue Jul 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#define kROMReadROM		0x33
#define kROMMatchROM		0x55
#define kROMSkipROM		0xCC
#define kROMSearchROM		0xF0
#define kROMSearchROM		0xF0
#define kROMWriteScratch	0x0F
#define kROMReadScratch		0xAA
#define kROMCopyScratch		0x55
#define kROMReadMemory		0xF0
#define kROMWriteAppReg		0x99
#define kROMReadStatusReg	0x66
#define kROMReadAppReg		0xC3
#define kROMCopyLockAppReg	0x5A
#define kROMValidationKey	0xA5

#define kGPIOPin			0x02
#define kGPIODriveOut		0x04
#define kGPIOOutputBit		0x00

#define kROMResetPulseMin	480
#define kROMResetPulseMax	960
#define kROMPresenceDelayMax	 60
#define kROMPresencePulseMax	240
#define kROMTSlot		120
#define kROMWrite1Min		  1
#define kROMWrite1Max		 15
#define kROMWrite0Min		 60
#define kROMWrite0Max		120
#define kROMReadSetup		  2
#define kROMReadTime		 15
#define kROMReadMargin		  1
#define kROMRead1TimeMax	 (kROMReadTime-kROMReadMargin-2)
#define kROMRead0TimeMin	 (kROMReadTime+kROMReadMargin+2)
