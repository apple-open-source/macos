/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *   TAS3001C - Texas 
 *
 *   Notes:
 *
 *
 */

#ifndef __TEXAS_HW__
#define __TEXAS_HW__

#include <libkern/OSTypes.h>

/*
 * I2S registers:
 */

#define		kDontRestoreOnNormal		0
#define		kRestoreOnNormal			1

#define		kNumberOfBiquadsPerChannel				6
#define		kNumberOfCoefficientsPerBiquad			5
#define		kNumberOfBiquadCoefficientsPerChannel	( kNumberOfBiquadsPerChannel * kNumberOfCoefficientsPerBiquad )
#define		kNumberOfBiquadCoefficients				( kNumberOfBiquadCoefficientsPerChannel * kTumblerMaxStreamCnt )
/*
 * Status register:
 */
#define		kHeadphoneBit		0x02

typedef UInt8	biquadParams[15];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  CONSTANTS

static biquadParams	kBiquad0db = {									//	biquad coefficients - unity gain all pass
			0x10, 0x00, 0x00,  										//	B0(23-16), B0(15-8), B0(7-0)
			0x00, 0x00, 0x00,  										//	B1(23-16), B0(15-8), B0(7-0)
			0x00, 0x00, 0x00,  										//	B2(23-16), B0(15-8), B0(7-0)
			0x00, 0x00, 0x00,  										//	A1(23-16), B0(15-8), B0(7-0)
			0x00, 0x00, 0x00  										//	A2(23-16), B0(15-8), B0(7-0)
};

#if 0
static UInt8	kTrebleRegValues[] = {
	0x01,	0x09,	0x10,	0x16,	0x1C,	0x22,					//	+18.0, +17.5, +17.0, +16.5, +16.0, +15.5	[dB]
	0x28,	0x2D,	0x32,	0x36,	0x3A,	0x3E,					//	+15.0, +14.5, +14.0, +13.5, +13.0, +12.5	[dB]
	0x42,	0x45,	0x49,	0x4C,	0x4F,	0x52,					//	+12.0, +11.5, +11.0, +10.5, +10.0, + 9.5	[dB]
	0x55,	0x57,	0x5A,	0x5C,	0x5E,	0x60,					//	+ 9.0, + 8.5, + 8.0, + 7.5, + 7.0, + 6.5	[dB]
	0x62,	0x63,	0x65,	0x66,	0x68,	0x69,					//	+ 6.0, + 5.5, + 5.0, + 4.5, + 4.0, + 3.5	[dB]
	0x6B,	0x6C,	0x6D,	0x6E,	0x70,	0x71,					//	+ 3.0, + 2.5, + 2.0, + 1.5, + 1.0, + 0.5	[dB]
	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,					//	  0.0, - 0.5, - 1.0, - 1.5, - 2.0, - 2.5	[dB]
	0x78,	0x79,	0x7A,	0x7B,	0x7C,	0x7D,					//	- 3.0, - 3.5, - 4.0, - 4.5, - 5.0, - 5.5	[dB]
	0x7E,	0x7F,	0x80,	0x81,	0x82,	0x83,					//	- 6.0, - 6.5, - 7.0, - 7.5, - 8.0, - 8.5	[dB]
	0x84,	0x85,	0x86,	0x87,	0x88,	0x89,					//	- 9.0, - 9.5, -10.0, -10.5, -11.0, -11.5	[dB]
	0x8A,	0x8B,	0x8C,	0x8D,	0x8E,	0x8F,					//	-12.0, -12.5, -13.0, -13.5, -14.0, -14.5	[dB]
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,					//	-15.0, -15.5, -16.0, -16.5, -17.0, -17.5	[dB]
	0x96															//	-18.0										[dB]
};

static UInt8	kBassRegValues[] = {
	0x01,	0x03,	0x06,	0x08,	0x0A,	0x0B,					//	+18.0, +17.5, +17.0, +16.5, +16.0, +15.5	[dB]
	0x0D,	0x0F,	0x10,	0x12,	0x13,	0x14,					//	+15.0, +14.5, +14.0, +13.5, +13.0, +12.5	[dB]
	0x16,	0x17,	0x18,	0x19,	0x1C,	0x1F,					//	+12.0, +11.5, +11.0, +10.5, +10.0, + 9.5	[dB]
	0x21,	0x23,	0x25,	0x26,	0x28,	0x29,					//	+ 9.0, + 8.5, + 8.0, + 7.5, + 7.0, + 6.5	[dB]
	0x2B,	0x2C,	0x2E,	0x30,	0x31,	0x33,					//	+ 6.0, + 5.5, + 5.0, + 4.5, + 4.0, + 3.5	[dB]
	0x35,	0x36,	0x28,	0x39,	0x3B,	0x3C,					//	+ 3.0, + 2.5, + 2.0, + 1.5, + 1.0, + 0.5	[dB]
	0x3E,	0x40,	0x42,	0x44,	0x46,	0x49,					//	  0.0, - 0.5, - 1.0, - 1.5, - 2.0, - 2.5	[dB]
	0x4B,	0x4D,	0x4F,	0x51,	0x53,	0x54,					//	- 3.0, - 3.5, - 4.0, - 4.5, - 5.0, - 5.5	[dB]
	0x55,	0x56,	0x58,	0x59,	0x5A,	0x5C,					//	- 6.0, - 6.5, - 7.0, - 7.5, - 8.0, - 8.5	[dB]
	0x5D,	0x5F,	0x61,	0x64,	0x66,	0x69,					//	- 9.0, - 9.5, -10.0, -10.5, -11.0, -11.5	[dB]
	0x6B,	0x6D,	0x6E,	0x70,	0x72,	0x74,					//	-12.0, -12.5, -13.0, -13.5, -14.0, -14.5	[dB]
	0x76,	0x78,	0x7A,	0x7D,	0x7F,	0x82,					//	-15.0, -15.5, -16.0, -16.5, -17.0, -17.5	[dB]
	0x86															//	-18.0										[dB]
};
#endif

//#define kUSE_DRC		/*	when defined, enable DRC at -30.0 dB	*/

static UInt32	volumeTable[] = {					// db = 20 LOG(x) but we just use table. from 0.0 to -70 db
	0x00000000,														// -infinity
	0x00000015,		0x00000016,		0x00000017,		0x00000019,		// -70.0,	-69.5,	-69.0,	-68.5,
	0x0000001A,		0x0000001C,		0x0000001D,		0x0000001F,		// -68.0,	-67.5,	-67.0,	-66.5,
	0x00000021,		0x00000023,		0x00000025,		0x00000027,		// -66.0,	-65.5,	-65.0,	-64.5,
	0x00000029,		0x0000002C,		0x0000002E,		0x00000031,		// -64.0,	-63.5,	-63.0,	-62.5,
	0x00000034,		0x00000037,		0x0000003A,		0x0000003E,		// -62.0,	-61.5,	-61.0,	-60.5,
	0x00000042,		0x00000045,		0x0000004A,		0x0000004E,		// -60.0,	-59.5,	-59.0,	-58.5,
	0x00000053,		0x00000057,		0x0000005D,		0x00000062,		// -58.0,	-57.5,	-57.0,	-56.5,
	0x00000068,		0x0000006E,		0x00000075,		0x0000007B,		// -56.0,	-55.5,	-55.0,	-54.5,
	0x00000083,		0x0000008B,		0x00000093,		0x0000009B,		// -54.0,	-53.5,	-53.0,	-52.5,
	0x000000A5,		0x000000AE,		0x000000B9,		0x000000C4,		// -52.0,	-51.5,	-51.0,	-50.5,
	0x000000CF,		0x000000DC,		0x000000E9,		0x000000F6,		// -50.0,	-49.5,	-49.0,	-48.5,
	0x00000105,		0x00000114,		0x00000125,		0x00000136,		// -48.0,	-47.5,	-47.0,	-46.5,
	0x00000148,		0x0000015C,		0x00000171,		0x00000186,		// -46.0,	-45.5,	-45.0,	-44.5,
	0x0000019E,		0x000001B6,		0x000001D0,		0x000001EB,		// -44.0,	-43.5,	-43.0,	-42.5,
	0x00000209,		0x00000227,		0x00000248,		0x0000026B,		// -42.0,	-41.5,	-41.0,	-40.5,
	0x0000028F,		0x000002B6,		0x000002DF,		0x0000030B,		// -40.0,	-39.5,	-39.0,	-38.5,
	0x00000339,		0x0000036A,		0x0000039E,		0x000003D5,		// -38.0,	-37.5,	-37.0,	-36.5,
	0x0000040F,		0x0000044C,		0x0000048D,		0x000004D2,		// -36.0,	-35.5,	-35.0,	-34.5,
	0x0000051C,		0x00000569,		0x000005BB,		0x00000612,		// -34.0,	-33.5,	-33.0,	-32.5,
	0x0000066E,		0x000006D0,		0x00000737,		0x000007A5,		// -32.0,	-31.5,	-31.0,	-30.5,
	0x00000818,		0x00000893,		0x00000915,		0x0000099F,		// -30.0,	-29.5,	-29.0,	-28.5,
	0x00000A31,		0x00000ACC,		0x00000B6F,		0x00000C1D,		// -28.0,	-27.5,	-27.0,	-26.5,
	0x00000CD5,		0x00000D97,		0x00000E65,		0x00000F40,		// -26.0,	-25.5,	-25.0,	-24.5,
	0x00001027,		0x0000111C,		0x00001220,		0x00001333,		// -24.0,	-23.5,	-23.0,	-22.5,
	0x00001456,		0x0000158A,		0x000016D1,		0x0000182B,		// -22.0,	-21.5,	-21.0,	-20.5,
	0x0000199A,		0x00001B1E,		0x00001CB9,		0x00001E6D,		// -20.0,	-19.5,	-19.0,	-18.5,
	0x0000203A,		0x00002223,		0x00002429,		0x0000264E,		// -18.0,	-17.5,	-17.0,	-16.5,
	0x00002893,		0x00002AFA,		0x00002D86,		0x00003039,		// -16.0,	-15.5,	-15.0,	-14.5,
	0x00003314,		0x0000361B,		0x00003950,		0x00003CB5,		// -14.0,	-13.5,	-13.0,	-12.5,
	0x0000404E,		0x0000441D,		0x00004827,		0x00004C6D,		// -12.0,	-11.5,	-11.0,	-10.5,
	0x000050F4,		0x000055C0,		0x00005AD5,		0x00006037,		// -10.0,	-9.5,	-9.0,	-8.5,
	0x000065EA,		0x00006BF4,		0x0000725A,		0x00007920,		// -8.0,	-7.5,	-7.0,	-6.5,
	0x0000804E,		0x000087EF,		0x00008FF6,		0x0000987D,		// -6.0,	-5.5,	-5.0,	-4.5,
	0x0000A186,		0x0000AB19,		0x0000B53C,		0x0000BFF9,		// -4.0,	-3.5,	-3.0,	-2.5,
	0x0000CB59,		0x0000D766,		0x0000E429,		0x0000F1AE,		// -2.0,	-1.5,	-1.0,	-0.5,
	0x00010000,		0x00010F2B,		0x00011F3D,		0x00013042,		// 0.0,		+0.5,	+1.0,	+1.5,
	0x00014249,		0x00015562,		0x0001699C,		0x00017F09,		// 2.0,		+2.5,	+3.0,	+3.5,
	0x000195BC,		0x0001ADC6,		0x0001C73D,		0x0001E237,		// 4.0,		+4.5,	+5.0,	+5.5,
	0x0001FECA,		0x00021D0E,		0x00023D1D,		0x00025F12,		// 6.0,		+6.5,	+7.0,	+7.5,
	0x0002830B,		0x0002A925,		0x0002D182,		0x0002FC42,		// 8.0,		+8.5,	+9.0,	+9.5,
	0x0003298B,		0x00035983,		0x00038C53,		0x0003C225,		// 10.0,	+10.5,	+11.0,	+11.5,
	0x0003FB28,		0x0004378B,		0x00047783,		0x0004BB44,		// 12.0,	+12.5,	+13.0,	+13.5,
	0x0005030A,		0x00054F10,		0x00059F98,		0x0005F4E5,		// 14.0,	+14.5,	+15.0,	+15.5,
	0x00064F40,		0x0006AEF6,		0x00071457,		0x00077FBB,		// 16.0,	+16.5,	+17.0,	+17.5,
	0x0007F17B														// +18.0 dB
};

// This is the coresponding dB values of the entries in the volumeTable arrary above.
static IOFixed	volumedBTable[] = {
	-70 << 16,														// Should really be -infinity
	-70 << 16,	-69 << 16 | 0x8000,	-69 << 16,	-68 << 16 | 0x8000,
	-68 << 16,	-67 << 16 | 0x8000,	-67 << 16,	-66 << 16 | 0x8000,
	-66 << 16,	-65 << 16 | 0x8000,	-65 << 16,	-64 << 16 | 0x8000,
	-64 << 16,	-63 << 16 | 0x8000,	-63 << 16,	-62 << 16 | 0x8000,
	-62 << 16,	-61 << 16 | 0x8000,	-61 << 16,	-60 << 16 | 0x8000,
	-60 << 16,	-59 << 16 | 0x8000,	-59 << 16,	-58 << 16 | 0x8000,
	-58 << 16,	-57 << 16 | 0x8000,	-57 << 16,	-56 << 16 | 0x8000,
	-56 << 16,	-55 << 16 | 0x8000,	-55 << 16,	-54 << 16 | 0x8000,
	-54 << 16,	-53 << 16 | 0x8000,	-53 << 16,	-52 << 16 | 0x8000,
	-52 << 16,	-51 << 16 | 0x8000,	-51 << 16,	-50 << 16 | 0x8000,
	-50 << 16,	-49 << 16 | 0x8000,	-49 << 16,	-48 << 16 | 0x8000,
	-48 << 16,	-47 << 16 | 0x8000,	-47 << 16,	-46 << 16 | 0x8000,
	-46 << 16,	-45 << 16 | 0x8000,	-45 << 16,	-44 << 16 | 0x8000,
	-44 << 16,	-43 << 16 | 0x8000,	-43 << 16,	-42 << 16 | 0x8000,
	-42 << 16,	-41 << 16 | 0x8000,	-41 << 16,	-40 << 16 | 0x8000,
	-40 << 16,	-39 << 16 | 0x8000,	-39 << 16,	-38 << 16 | 0x8000,
	-38 << 16,	-37 << 16 | 0x8000,	-37 << 16,	-36 << 16 | 0x8000,
	-36 << 16,	-35 << 16 | 0x8000,	-35 << 16,	-34 << 16 | 0x8000,
	-34 << 16,	-33 << 16 | 0x8000,	-33 << 16,	-32 << 16 | 0x8000,
	-32 << 16,	-31 << 16 | 0x8000,	-31 << 16,	-30 << 16 | 0x8000,
	-30 << 16,	-29 << 16 | 0x8000,	-29 << 16,	-28 << 16 | 0x8000,
	-28 << 16,	-27 << 16 | 0x8000,	-27 << 16,	-26 << 16 | 0x8000,
	-26 << 16,	-25 << 16 | 0x8000,	-25 << 16,	-24 << 16 | 0x8000,
	-24 << 16,	-23 << 16 | 0x8000,	-23 << 16,	-22 << 16 | 0x8000,
	-22 << 16,	-21 << 16 | 0x8000,	-21 << 16,	-20 << 16 | 0x8000,
	-20 << 16,	-19 << 16 | 0x8000,	-19 << 16,	-18 << 16 | 0x8000,
	-18 << 16,	-17 << 16 | 0x8000,	-17 << 16,	-16 << 16 | 0x8000,
	-16 << 16,	-15 << 16 | 0x8000,	-15 << 16,	-14 << 16 | 0x8000,
	-14 << 16,	-13 << 16 | 0x8000,	-13 << 16,	-12 << 16 | 0x8000,
	-12 << 16,	-11 << 16 | 0x8000,	-11 << 16,	-10 << 16 | 0x8000,
	-10 << 16,	-9 << 16 | 0x8000,	-9 << 16,	-8 << 16 | 0x8000,
	-8 << 16,	-7 << 16 | 0x8000,	-7 << 16,	-6 << 16 | 0x8000,
	-6 << 16,	-5 << 16 | 0x8000,	-5 << 16,	-4 << 16 | 0x8000,
	-4 << 16,	-3 << 16 | 0x8000,	-3 << 16,	-2 << 16 | 0x8000,
	-2 << 16,	-1 << 16 | 0x8000,	-1 << 16,	-0 << 16 | 0x8000,
	0 << 16,	+0 << 16 | 0x8000,	+1 << 16,	+1 << 16 | 0x8000,
	2 << 16,	+2 << 16 | 0x8000,	+3 << 16,	+3 << 16 | 0x8000,
	4 << 16,	+4 << 16 | 0x8000,	+5 << 16,	+5 << 16 | 0x8000,
	6 << 16,	+6 << 16 | 0x8000,	+7 << 16,	+7 << 16 | 0x8000,
	8 << 16,	+8 << 16 | 0x8000,	+9 << 16,	+9 << 16 | 0x8000,
	10 << 16,	+10 << 16 | 0x8000,	+11 << 16,	+11 << 16 | 0x8000,
	12 << 16,	+12 << 16 | 0x8000,	+13 << 16,	+13 << 16 | 0x8000,
	14 << 16,	+14 << 16 | 0x8000,	+15 << 16,	+15 << 16 | 0x8000,
	16 << 16,	+16 << 16 | 0x8000,	+17 << 16,	+17 << 16 | 0x8000,
	+18 << 16
};

#pragma mark -
#pragma mark ееееееее TAS3001C Registers ееееееее
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum TAS3001Registers {					//	Specification downloadable at <http://www.ti.com> or <http://www.ti.com/sc/docs/products/analog/tas3001.html>
										//	Bytes	Byte Description (bit encoding interleaved with register address where appropriate)
	kMainCtrlReg			=	0x01,
	kFL						=	7,		//	N.......	[bit address]	Load Mode:		0 = normal,		1 = Fast Load
	kSC						=	6,		//	.N......	[bit address]	SCLK frequency:	0 = 32 fs,		1 = 64 fs
	kE0						=	4,		//	..NN....	[bit address]	Output Serial Mode
	kF0						=	2,		//	....NN..	[bit address]	Input Serial Mode
	kW0						=	0,		//	......NN	[bit address]	Serial Port Word Length
	
	kNormalLoad				=	0,		//	use:	( kNormalLoad << kFL )
	kFastLoad				=	1,		//	use:	( kFastLoad << kFL )
	
	kSerialModeLeftJust		=	0,		//	use:	( kSerialModeLeftJust << kEO ) ╔OR╔ ( kSerialModeLeftJust << kFO )
	kSerialModeRightJust	=	1,		//	use:	( kSerialModeRightJust << kEO ) ╔OR╔ ( kSerialModeRightJust << kFO )
	kSerialModeI2S			=	2,		//	use:	( kSerialModeI2S << kEO ) ╔OR╔ ( kSerialModeI2S << kFO )
	
	kSerialWordLength16		=	0,		//	use:	( kSerialWordLength16 << kWO )
	kSerialWordLength18		=	1,		//	use:	( kSerialWordLength18 << kWO )
	kSerialWordLength20		=	2,		//	use:	( kSerialWordLength20 << kWO )
	
	k32fs					=	0,		//	use:	( k32fs << kSC )
	k64fs					=	1,		//	use:	( k64fs << kSC )
	
	kI2SMode				=	( kSerialModeI2S << kE0 ) | ( kSerialModeI2S << kF0 ),
	kLeftJustMode			=	( kSerialModeLeftJust << kE0 ) | ( kSerialModeLeftJust << kF0 ),
	kRightJustMode			=	( kSerialModeRightJust << kE0 ) | ( kSerialModeRightJust << kF0 ),
	
	kDynamicRangeCtrlReg	=	0x02,	//	2		DRC Byte 1 (7-0), DRC Byte 2 (7-0)
	kEN						=	0,		//	.......N	[bit address]	Enable:			0 = disable,	1 = enable
	kCR						=	6,		//	NN......	[bit address]	Compression Ratio
	kCompression3to1		=	3,		//	only valid compression ration is 3:1
										//	Byte[1] of dynamic range compressor is nibble map packed array
	
	kDrcDisable				=	0,		//	use:	( kDrcDisable << kEN )
	kDrcEnable				=	1,		//	use:	( kDrcEnable << kEN )
	kDefaultCompThld		=	0xA0,	//	default compression threshold (Larry Heyl provided this setting)
	
	kVolumeCtrlReg			=	0x04,	//	6		VL(23-16), VL(15-8), VL(7-0), VR(23-16), VR(15-8), VR(7-0)
	
	kTrebleCtrlReg			=	0x05,	//	1		T(7-0)
	
	kBassCtrlReg			=	0x06,	//	1		B(7-0)
	
	kMixer1CtrlReg			=	0x07,	//	3		S(23-16), S(15-8), S(7-0)
	
	kMixer2CtrlReg			=	0x08,	//	3		S(23-16), S(15-8), S(7-0)
	
	kLeftBiquad0CtrlReg		=	0x0A,	//	15		B0(23-16), B0(15-8), B0(7-0), 
										//			B1(23-16), B1(15-8), B1(7-0), 
										//			B2(23-16), B2(15-8), B2(7-0), 
										//			A1(23-16), A1(15-8), A1(7-0), 
										//			A2(23-16), A2(15-8), A2(7-0)
										
	kLeftBiquad1CtrlReg		=	0x0B,	//	15		(same format as kLeftBiquad0CtrlReg)
	kLeftBiquad2CtrlReg		=	0x0C,	//	15		(same format as kLeftBiquad0CtrlReg)
	kLeftBiquad3CtrlReg		=	0x0D,	//	15		(same format as kLeftBiquad0CtrlReg)
	kLeftBiquad4CtrlReg		=	0x0E,	//	15		(same format as kLeftBiquad0CtrlReg)
	kLeftBiquad5CtrlReg		=	0x0F,	//	15		(same format as kLeftBiquad0CtrlReg)
	
	kRightBiquad0CtrlReg	=	0x13,	//	15		(same format as kLeftBiquad0CtrlReg)
	kRightBiquad1CtrlReg	=	0x14,	//	15		(same format as kRightBiquad0CtrlReg)
	kRightBiquad2CtrlReg	=	0x15,	//	15		(same format as kRightBiquad0CtrlReg)
	kRightBiquad3CtrlReg	=	0x16,	//	15		(same format as kRightBiquad0CtrlReg)
	kRightBiquad4CtrlReg	=	0x17,	//	15		(same format as kRightBiquad0CtrlReg)
	kRightBiquad5CtrlReg	=	0x18	//	15		(same format as kRightBiquad0CtrlReg)
};

enum biquadInformation{
	kBiquadRefNum_0						=	 0,
	kBiquadRefNum_1						=	 1,
	kBiquadRefNum_2						=	 2,
	kBiquadRefNum_3						=	 3,
	kBiquadRefNum_4						=	 4,
	kBiquadRefNum_5						=	 5,
	kTumblerMaxBiquadRefNum				=	 5,
	kTumblerNumBiquads					=	 6,
	kTumblerCoefficientsPerBiquad		=	 5,
	kTumblerCoefficientBitWidth			=	24,
	kTumblerCoefficientIntegerBitWidth	=	 4,
	kTumblerCoefficientFractionBitWidth	=	20
};

#define	kEXPERIMENT			2
#if kEXPERIMENT == 1
#define	TAS_I2S_MODE		kLeftJustMode
#define	TAS_WORD_LENGTH		kSerialWordLength20
#elif kEXPERIMENT == 2
#define	TAS_I2S_MODE		kI2SMode
#define	TAS_WORD_LENGTH		kSerialWordLength20
#elif kEXPERIMENT == 3
#define	TAS_I2S_MODE		kLeftJustMode
#define	TAS_WORD_LENGTH		kSerialWordLength16
#elif kEXPERIMENT == 4
#define	TAS_I2S_MODE		kI2SMode
#define	TAS_WORD_LENGTH		kSerialWordLength16
#endif

#define	ASSERT_GPIO( x )					( 0 == x ? 0 : 1 )
#define	NEGATE_GPIO( x )					( 0 == x ? 1 : 0 )

enum TAS3001C_registerWidths{
	kMCRwidth				=	1,
	kDRCwidth				=	2,
	kVOLwidth				=	6,
	kTREwidth				=	1,
	kBASwidth				=	1,
	kMIXwidth				=	3,
	kBIQwidth				=	( 3 * 5 )
};

enum mixerType{
	kMixerNone,
	kMixerTAS3001C
};

enum TAS3001Constants {
	kMixMute				= 0x00000000,	//	4.N two's complement with 1 sign bit, 3 integer bits & N bits of fraction
	kMix0dB					= 0x00100000	//	4.N two's complement with 1 sign bit, 3 integer bits & N bits of fraction
};

enum {
	kStreamCountMono			= 1,
	kStreamCountStereo			= 2
};

enum GeneralHardwareAttributeConstants {
	kSampleRatesCount			=	2,
	kFrontLeftOFFSET			=	0,
	kFrontRightOFFSET			=	1,
	kTumblerMaxStreamCnt		=	kStreamCountStereo,	//	Two streams are kStreamFrontLeft and kStreamFrontRight (see SoundHardwarePriv.h)
	kTumblerMaxSndSystem		=	2,					//	Tumbler supports kSndHWSystemBuiltIn and kSndHWSystemTelephony
	k16BitsPerChannel			=	16,
	kTumblerInputChannelDepth	=	kStreamCountMono,
	kTumblerInputFrameSize		=	16,
	kTouchBiquad				=	1,
	kBiquadUntouched			=	0
};

enum {
	// Regular, actually addressable streams (i.e. data can be streamed to and/or from these).
	// As such, these streams can be used with get and set current stream map;
	// get and set relative volume min, max, and current; and 
	// get hardware stream map
	kStreamFrontLeft			= 'fntl',	// front left speaker
	kStreamFrontRight			= 'fntr',	// front right speaker
	kStreamSurroundLeft			= 'surl',	// surround left speaker
	kStreamSurroundRight		= 'surr',	// surround right speaker
	kStreamCenter				= 'cntr',	// center channel speaker
	kStreamLFE					= 'lfe ',	// low frequency effects speaker
	kStreamHeadphoneLeft		= 'hplf',	// left headphone
	kStreamHeadphoneRight		= 'hprt',	// right headphone
	kStreamLeftOfCenter			= 'loc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamRightOfCenter		= 'roc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamSurround				= 'sur ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[rear]
	kStreamSideLeft				= 'sidl',	//	see usb audio class spec. v1.0, section 3.7.2.3	[left wall]
	kStreamSideRight			= 'sidr',	//	see usb audio class spec. v1.0, section 3.7.2.3	[right wall]
	kStreamTop					= 'top ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[overhead]
	kStreamMono					= 'mono',	//	for usb devices with a spatial configuration of %0000000000000000

	// Virtual streams. These are separately addressable streams as a "regular" stream; however, they are mutually
	// exclusive with one or more "regular" streams because they are not actually separate on the hardware device.
	// These selectors may be used with get and set current stream map; the relative volume calls; and get the
	// hardware stream map. However, using these in set current stream map will return an error if any of the 
	// exclusive relationships are violated by the request. 
	kStreamVirtualHPLeft		= 'vhpl',	// virtual headphone left - exclusive with front left speaker
	kStreamVirtualHPRight		= 'vhpr',	// virtual headphone right 

	// Embeddeded streams. These are not separately addressable streams and may never be used in the current stream 
	// map calls. Their presence in the hardware stream map indicates that there is a port present that can be 
	// volume controlled and thus these may also be used with the relative volume calls.
	kStreamEmbeddedSubwoofer	= 'lfei',	// embedded subwoofer
	kStreamBitBucketLeft		= 'sbbl',	//	VirtualHAL stream bit bucket left channel
	kStreamBitBucketRight		= 'sbbr',	//	VirtualHAL stream bit bucket right channel
	kMAX_STREAM_COUNT			= 20,		//	total number of unique stream identifiers defined in this enumeration list
	
	//	The following enumerations are related to the stream descriptions provided above but are not unique.
	//	They are intented to include multiple selections of the individual stream descriptions and as such
	//	are not to be included within the tally for kMAX_STREAM_COUNT.
	
	kStreamStereo				= 'flfr'	//	implies both 'fntl' and 'fntr' streams
};

#define kSetBass				1
#define kSetTreble				0
#define kToneGainToRegIndex		0x38E

#define kDrcThresholdMin		-35.9375				/*	minimum DRC threshold dB (i.e. -36.000 dB + 0.375 dB)	*/
#define	kDrcThresholdMax		  /*0.0*/ 0				/*	maximum DRC threshold dB								*/

// For Mac OS X we can't use floats, so multiply by 1000 to make a normal interger
#define	kDrcThresholdStepSize	  /*0.375*/ 375			/*	dB per increment										*/
#define	kDrcUnityThresholdHW	 (15 << 4 )				/*	hardware value for 0.0 dB DRC threshold					*/
#define	kDrcRatioNumerator		  /*3.0*/ 3
#define	kDrcRationDenominator	  /*1.0*/ 1
#define	kDefaultMaximumVolume	  /*0.0*/ 0
#define	kTumblerVolumeStepSize	  /*0.5*/ 1
#define	kTumblerMinVolume		/*-70.0*/ -70
#define	kTumblerAbsMaxVolume	/*+18.0*/ 18
/*#define kTumblerVolToVolSteps	  1.82*/
#define	kTumblerMaxIntVolume	256
// 225 was picked over 10 because it allows the part to come fully to volume after mute so that we don't loose the start of a sound
#define	kAmpRecoveryMuteDuration	225					/* expressed in milliseconds	*/

#define kTexasInputSampleLatency	32
#define kTexasOutputSampleLatency	31

#define kHeadphoneAmpEntry			"headphone-mute"
#define kAmpEntry					"amp-mute"
#define kHWResetEntry				"audio-hw-reset"
#define kHeadphoneDetectInt			"headphone-detect"
#define	kKWHeadphoneDetectInt		"keywest-gpio15"
#define kDallasDetectInt			"extint-gpio16"
#define kKWDallasDetectInt			"keywest-gpio16"
#define kVideoPropertyEntry			"video"
#define kOneWireBus					"one-wire-bus"

#define kI2CDTEntry					"i2c"
#define kDigitalEQDTEntry			"deq"
#define kSoundEntry					"sound"

#define kNumInputs					"#-inputs"
#define kDeviceID					"device-id"
#define kSpeakerID					"speaker-id"
#define kCompatible					"compatible"
#define kI2CAddress					"i2c-address"
#define kAudioGPIO					"audio-gpio"
#define kAudioGPIOActiveState		"audio-gpio-active-state"
#define kIOInterruptControllers		"IOInterruptControllers"

enum UFixedPointGain{
	kMinSoftwareGain				=	0x00008000,
	kUnitySoftwareGain				=	0x00010000,
	kMaxSoftwareGain				=	0x00018000,
	kSoftwareGainMask				=	0x0001F000
};

enum TAS3001C_ResetFlags{
	kNO_FORCE_RESET_SETUP_TIME		=	0,
	kFORCE_RESET_SETUP_TIME			=	1
};

enum loadMode {
		kSetNormalLoadMode			=	0,
		kSetFastLoadMode			=	1
};

enum semaphores{
	kResetSemaphoreBit				=	0,							//	bit address:	1 = reset in progress
	kResetSemaphoreMask				=	( 1 << 0 )					//	bit address:	1 = reset in progress
};

enum writeMode{
	kUPDATE_SHADOW					=	0,
	kUPDATE_HW						=	1,
	kUPDATE_ALL						=	2
};

enum resetRetryCount{
	kRESET_MAX_RETRY_COUNT			=	5
};

enum eqPrefsVersion{
	kCurrentEQPrefsVersion			=	1
};

enum muteSelectors{
	kHEADPHONE_AMP					=	0,
	kSPEAKER_AMP					=	1
};

#define kHeadphoneBitPolarity		1
#define kHeadphoneBitAddr			0
#define kHeadphoneBitMask			(1 << kHeadphoneBitAddr)
#define kHeadphoneBitMatch			(kHeadphoneBitPolarity << kHeadphoneBitAddr)
#define kHeadphoneDetect			1

#define kSpeakerBit					1

#pragma mark -
#pragma mark ееееееее Structures ееееееее
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STRUCTURES

typedef Boolean	GpioActiveState;

typedef struct{
	UInt8		sMCR[kMCRwidth];
	UInt8		sDRC[kDRCwidth];
	UInt8		sVOL[kVOLwidth];
	UInt8		sTRE[kTREwidth];
	UInt8		sBAS[kBASwidth];
	UInt8		sMX1[kMIXwidth];
	UInt8		sMX2[kMIXwidth];
	UInt8		sLB0[kBIQwidth];
	UInt8		sLB1[kBIQwidth];
	UInt8		sLB2[kBIQwidth];
	UInt8		sLB3[kBIQwidth];
	UInt8		sLB4[kBIQwidth];
	UInt8		sLB5[kBIQwidth];
	UInt8		sRB0[kBIQwidth];
	UInt8		sRB1[kBIQwidth];
	UInt8		sRB2[kBIQwidth];
	UInt8		sRB3[kBIQwidth];
	UInt8		sRB4[kBIQwidth];
	UInt8		sRB5[kBIQwidth];
}TAS3001C_ShadowReg;
typedef TAS3001C_ShadowReg TAS3001C_ShadowReg;
typedef TAS3001C_ShadowReg *TAS3001C_ShadowRegPtr;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct HiLevelFilterCoefficients {
	float			filterFrequency;
	float			filterGain;
	float			filterQ;
	UInt32			filterType;	
};
typedef HiLevelFilterCoefficients * HiLevelFilterCoefficientsPtr;

struct FourDotTwenty
{
	unsigned char integerAndFraction1;
	unsigned char fraction2;
	unsigned char fraction3;
};
typedef struct FourDotTwenty FourDotTwenty, *FourDotTwentyPtr;

union EQFilterCoefficients {
		FourDotTwenty				coefficient[kNumberOfCoefficientsPerBiquad];	//	Coefficient[] is b0, b1, b2, a1 & a2 
//		HiLevelFilterCoefficients	hlFilter;
};
typedef EQFilterCoefficients *EQFilterCoefficientsPtr;

struct EQPrefsElement {
	/*double*/ UInt32		filterSampleRate;		
	/*double*/ UInt32		drcCompressionRatioNumerator;
	/*double*/ UInt32		drcCompressionRatioDenominator;
	/*double*/ SInt32		drcThreshold;
	/*double*/ SInt32		drcMaximumVolume;
	UInt32					drcEnable;
	UInt32					layoutID;				//	what cpu we're running on
	UInt32					deviceID;				//	i.e. internal spkr, external spkr, h.p.
	UInt32					speakerID;				//	what typeof external speaker is connected.
	UInt32					reserved;
	UInt32					filterCount;			//	number of biquad filters (total number : channels X ( biquads per channel )
	EQFilterCoefficients	filter[12];				//	an array of filter coefficient equal in length to filter count * sizeof(EQFilterCoefficients)
};
typedef EQPrefsElement *EQPrefsElementPtr;

#define	kCurrentEQPrefVersion	1
struct EQPrefs {
	UInt32					structVersionNumber;	//	identify what we're parsing
	UInt32					genreType;				//	'jazz', 'clas', etc...
	UInt32					eqCount;				//	number of eq[n] array elements
	UInt32					nameID;					//	resource id of STR identifying the filter genre
	EQPrefsElement			eq[0x00000013];					//	'n' sized based on number of devicID/speakerID/layoutID combinations...
};
typedef EQPrefs *EQPrefsPtr;

struct DRCInfo {
	/*double*/ UInt32		compressionRatioNumerator;
	/*double*/ UInt32		compressionRatioDenominator;
	/*double*/ SInt32		threshold;
	/*double*/ SInt32		maximumVolume;
	/*double*/ UInt32		maximumAvailableVolume;
	/*double*/ UInt32		minimumAvailableVolume;
	/*double*/ UInt32		maximumAvailableThreshold;
	/*double*/ UInt32		minimumAvailableThreshold;
	Boolean					enable;
};
typedef DRCInfo *DRCInfoPtr;

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

#endif	// __TEXAS_HW__
