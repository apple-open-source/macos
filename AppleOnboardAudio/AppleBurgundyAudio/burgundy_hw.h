/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1998-1999 Apple Computer, Inc.  All rights reserved.
 *
 * Burgundy Hardware Registers
 *
 */

#define kSoundCtlReg				0x00
#define kSoundCtlReg_InSubFrame_Mask		0x0000000F	/*All of the input subframe bits*/
#define kSoundCtlReg_InSubFrame_Bit		0x00000001	/*All of the input subframe bits*/
#define kSoundCtlReg_InSubFrame0		0x00000001
#define kSoundCtlReg_InSubFrame1		0x00000002
#define kSoundCtlReg_InSubFrame2		0x00000004
#define kSoundCtlReg_InSubFrame3		0x00000008
#define kSoundCtlReg_OutSubFrame_Mask		0x000000F0
#define kSoundCtlReg_OutSubFrame_Bit		0x00000010
#define kSoundCtlReg_OutSubFrame0		0x00000010
#define kSoundCtlReg_OutSubFrame1		0x00000020
#define kSoundCtlReg_OutSubFrame2		0x00000040
#define kSoundCtlReg_OutSubFrame3		0x00000080
#define kSoundCtlReg_Rate_Mask			0x00000700
#define kSoundCtlReg_Rate_Bit			0x00000100
#define kSoundCtlReg_Rate_44100			0x00000000
#define kSoundCtlReg_Error			0x00000800
#define kSoundCtlReg_PortChange			0x00001000
#define kSoundCtlReg_ErrorInt			0x00002000
#define kSoundCtlReg_StatusSubFrame_Mask	0x00018000
#define kSoundCtlReg_StatusSubFrame_Bit		0x00008000
#define kSoundCtlReg_StatusSubFrame0		0x00000000
#define kSoundCtlReg_StatusSubFrame1		0x00008000
#define kSoundCtlReg_StatusSubFrame2		0x00010000
#define kSoundCtlReg_StatusSubFrame3		0x00018000

/*
 *
 *    Codec Control Fields Bit Format:	vuwr Pppp Aaaa nn qq Ddddddddd
 *					23			     0
 *
 *    Ddddddddd	- Data byte (to Codec)
 *    qq       	- Current byte in register transaction
 *    nn        - Last byte in register transaction
 *    Aaaa	- Codec register address - reg #
 *    Pppp      - Codec register address - page #
 *    r         - Reset transaction pointers
 *    w		- 0=read 1=write
 *    u		- unused
 *    v		- Valid (v is set by the hardware automatically)
 */
#define kCodecCtlReg				0x10
#define kCodecCtlReg_Data_Mask			0x000000FF
#define kCodecCtlReg_Data_BitAddress		0
#define kCodecCtlReg_Data_Bit			( 1 << kCodecCtlReg_Data_BitAddress )
#define kCodecCtlReg_CurrentByte_Mask		0x00000300
#define kCodecCtlReg_CurrentByte_BitAddress	8
#define kCodecCtlReg_CurrentByte_Bit		( 1 << kCodecCtlReg_CurrentByte_BitAddress )
#define kCodecCtlReg_LastByte_Mask		0x00000C00
#define kCodecCtlReg_LastByte_BitAddress	10
#define kCodecCtlReg_LastByte_Bit		( 1 << kCodecCtlReg_LastByte_BitAddress )
#define kCodecCtlReg_Addr_Mask			0x000FF000
#define kCodecCtlReg_Addr_BitAddress		12
#define kCodecCtlReg_Addr_Bit			( 1 << kCodecCtlReg_Addr_BitAddress )
#define kCodecCtlReg_Reset			0x00100000
#define kCodecCtlReg_Write			0x00200000
#define kCodecCtlReg_Busy			0x01000000

 
/*
 *
 *    Codec Status Fields Bit Format:	FC00 IIII BB LL Dddddddd Pppp
 *					23		            0
 *    Pppp	- Input sense lines
 *    Dddddddd  - Data byte (from Codec)
 *    LL	- Byte location being read (0-3)
 *    BB        - Byte counter (0-3) Increments when new byte is presented.
 *    IIII	- Indicator code
 *    C		- Codec ready
 *    F		- First valid byte
 */
#define kCodecStatusReg				0x20
#define kCodecStatusReg_Sense_Mask		0x0000000F
#define kCodecStatusReg_Sense_BitAddress	0
#define kCodecStatusReg_Sense_Bit		( 1 << kCodecStatusReg_Sense_BitAddress )
#define kCodecStatusReg_Sense_Mic               0x00000002
#define kCodecStatusReg_Sense_Headphones        0x00000004
#define kCodecStatusReg_Sense_Headphones2       0x00000008
#define kCodecStatusReg_Data_Mask		0x00000FF0
#define	kCodecStatusReg_Data_BitAddress		4
#define kCodecStatusReg_Data_Bit		( 1 << kCodecStatusReg_Data_BitAddress )
#define kCodecStatusReg_CurrentByte_Mask	0x00003000
#define	kCodecStatusReg_CurrentByte_BitAddress	12
#define kCodecStatusReg_CurrentByte_Bit		( 1 << kCodecStatusReg_CurrentByte_BitAddress )
#define kCodecStatusReg_ByteCounter_Mask	0x0000C000
#define kCodecStatusReg_ByteCounter_BitAddress	14
#define kCodecStatusReg_ByteCounter_Bit		( 1 << kCodecStatusReg_ByteCounter_BitAddress )
#define kCodecStatusReg_Indicator_Mask		0x000F0000
#define kCodecStatusReg_Indicator_BitAddress	16
#define kCodecStatusReg_Indicator_Bit		( 1 << kCodecStatusReg_Indicator_BitAddress )
#define kCodecStatusReg_Ready			0x00400000
#define kCodecStatusReg_FirstByte		0x00800000


#define kCodec_Indicator_ToneControl		0x01
#define kCodec_Indicator_Overflow0		0x02
#define kCodec_Indicator_Overflow1		0x03
#define kCodec_Indicator_Overflow2		0x04
#define kCodec_Indicator_InputLineChg		0x06
#define kCodec_Indicator_Threshold0		0x0B
#define kCodec_Indicator_Threshold1		0x0C
#define kCodec_Indicator_Threshold2		0x0D
#define kCodec_Indicator_Threshold3		0x0E
#define kCodec_Indicator_TwilightCmp		0x0F


/*
 * The I/O routines use the following convention to pass Codec register addresses:
 *
 *    Codec Register Address:	00LL Pppp Aaaa 0000 00BB
 *
 *    LL 	- Length of register
 *    Pppp	- Codec register address - page #
 *    Aaaa 	- Codec register address - reg #
 *    BB	- Offset to 1st byte of register.
 */

#define BURGUNDY_LENGTH_1			0x00010000
#define BURGUNDY_LENGTH_2			0x00020000
#define BURGUNDY_LENGTH_3			0x00030000
#define BURGUNDY_LENGTH_4			0x00040000

#define kInitReg				(0x0000 | BURGUNDY_LENGTH_1)
#define kInitReg_Recalibrate			0x01
#define kInitReg_Twilight			0x02

#define kRevisionReg				(0x0100 | BURGUNDY_LENGTH_1)
#define kVersionReg				(0x0101 | BURGUNDY_LENGTH_1)
#define kVendorReg				(0x0102 | BURGUNDY_LENGTH_1)
#define kIdReg					(0x0103 | BURGUNDY_LENGTH_1)

#define kGlobalStatusReg			(0x0200 | BURGUNDY_LENGTH_2)
#define kGlobalStatusReg_ClippingStatus		0x0001
#define kGlobalStatusReg_OverflowStatus0	0x0002
#define kGlobalStatusReg_OverflowStatus1	0x0004
#define kGlobalStatusReg_SOSStatus1		0x0008
#define kGlobalStatusReg_ParallelInStatus	0x0020
#define kGlobalStatusReg_Threshold0		0x0400
#define kGlobalStatusReg_Threshold1		0x0800
#define kGlobalStatusReg_Threshold2		0x1000
#define kGlobalStatusReg_Threshold3		0x2000

#define kReturnZeroReg				(0x0F00 | BURGUNDY_LENGTH_1)


/*
 * This register controls a fixed gain preamp (+24dB) on input ports 5-7
 */
#define kInputPreampReg				(0x1000 | BURGUNDY_LENGTH_1)
#define kInputPreampReg_Port5L			0x04
#define kInputPreampReg_Port5R			0x08
#define kInputPreampReg_Port6L			0x10
#define kInputPreampReg_Port6R			0x20
#define kInputPreampReg_Port7L			0x40
#define kInputPreampReg_Port7R			0x80

/*
 * Mux01 and Mux2 determine which analog inputs will be presented to A/D Converters 0-2
 */
#define kMux01Reg				(0x1100 | BURGUNDY_LENGTH_1)
#define kMux01Reg_Mux0L_Mask			0x03
#define kMux01Reg_Mux0L_SelectPort1L		0x00
#define kMux01Reg_Mux0L_SelectPort2L		0x01
#define kMux01Reg_Mux0L_SelectPort3L		0x02
#define kMux01Reg_Mux0R_Mask			0x0C
#define kMux01Reg_Mux0R_SelectPort1R		0x00
#define kMux01Reg_Mux0R_SelectPort2R		0x04
#define kMux01Reg_Mux0R_SelectPort3R		0x08
#define kMux01Reg_Mux1L_Mask			0x30
#define kMux01Reg_Mux1L_SelectPort1L		0x00
#define kMux01Reg_Mux1L_SelectPort2L		0x10
#define kMux01Reg_Mux1L_SelectPort3L		0x20
#define kMux01Reg_Mux1R_Mask			0xC0
#define kMux01Reg_Mux1R_SelectPort1R		0x00
#define kMux01Reg_Mux1R_SelectPort2R		0x40
#define kMux01Reg_Mux1R_SelectPort3R		0x80

#define kMux2Reg				(0x1200 | BURGUNDY_LENGTH_1)
#define kMux2Reg_Mux2L_Mask			0x03
#define kMux2Reg_Mux2L_SelectPort5L		0x00
#define kMux2Reg_Mux2L_SelectPort6L		0x01
#define kMux2Reg_Mux2L_SelectPort7L		0x02
#define kMux2Reg_Mux2R_Mask			0x0C
#define kMux2Reg_Mux2R_SelectPort5R		0x00
#define kMux2Reg_Mux2R_SelectPort6R		0x04
#define kMux2Reg_Mux2R_SelectPort7R		0x08

/*
 * VGA0-3 control an analog amp at the input to each A/D Converter
 */
#define kVGA0Reg				(0x1300 | BURGUNDY_LENGTH_1)
#define kVGA0Reg_VGA0L_GainMask			0x0F
#define kVGA0Reg_VGA0L_GainBit			0x01
#define kVGA0Reg_VGA0R_GainMask			0xF0
#define kVGA0Reg_VGA0R_GainBit			0x10

#define kVGA1Reg				(0x1400 | BURGUNDY_LENGTH_1)
#define kVGA1Reg_VGA1L_GainMask			0x0F
#define kVGA1Reg_VGA1L_GainBit			0x01
#define kVGA1Reg_VGA1R_GainMask			0xF0
#define kVGA1Reg_VGA1R_GainBit			0x10

#define kVGA2Reg				(0x1500 | BURGUNDY_LENGTH_1)
#define kVGA2Reg_VGA2L_GainMask			0x0F
#define kVGA2Reg_VGA2L_GainBit			0x01
#define kVGA2Reg_VGA2R_GainMask			0xF0
#define kVGA2Reg_VGA2R_GainBit			0x10

#define kVGA3Reg				(0x1600 | BURGUNDY_LENGTH_1)
#define kVGA0Reg_VGA3M_GainMask			0x0F
#define kVGA0Reg_VGA3M_GainBit			0x01

#define kDigInputPortReg			(0x1700 | BURGUNDY_LENGTH_1)


/*
 * This register provides the status of various input sense lines
 */
#define kInputStateReg				(0x1800 | BURGUNDY_LENGTH_1)
#define kInputStateReg_InputIndicator		0x01
#define kInputStateReg_Input1Active		0x02
#define kInputStateReg_Input2Active		0x04
#define kInputStateReg_Input3Active		0x08
#define kInputStateReg_Sense0Active		0x10
#define kInputStateReg_Sense1Active		0x20
#define kInputStateReg_Sense2Active		0x40
#define kInputStateReg_Sense3Active		0x80

/*
 * This register provides the status of the A/D Converters
 */
#define kAD12StatusReg				(0x1900 | BURGUNDY_LENGTH_2)
#define kAD12StatusReg_AD0L_OverRange		0x0001
#define kAD12StatusReg_AD0R_OverRange		0x0002
#define kAD12StatusReg_AD1L_OverRange		0x0004
#define kAD12StatusReg_AD1R_OverRange		0x0008
#define kAD12StatusReg_AD2L_OverRange		0x0010
#define kAD12StatusReg_AD2R_OverRange		0x0020
#define kAD12StatusReg_AD3M_OverRange		0x0040
#define kAD12StatusReg_AD0L_OverRangeIndicator	0x0100
#define kAD12StatusReg_AD0R_OverRangeIndicator	0x0200
#define kAD12StatusReg_AD1L_OverRangeIndicator	0x0400
#define kAD12StatusReg_AD1R_OverRangeIndicator	0x0800
#define kAD12StatusReg_AD2L_OverRangeIndicator	0x1000
#define kAD12StatusReg_AD2R_OverRangeIndicator	0x2000
#define kAD12StatusReg_AD3M_OverRangeIndicator	0x4000


/*
 * These registers control digital scalars for the various input sources.
 * Sources 0-4 are derived from the A/D Converter outputs. Sources A-H
 * are either data from the host system or digital outputs from the chip
 * which are being internally wrapped back.
 */
#define kGAS0LReg				(0x2000 | BURGUNDY_LENGTH_1)
#define kPAS0LReg				(0x2001 | BURGUNDY_LENGTH_1)
#define kGAS0RReg				(0x2002 | BURGUNDY_LENGTH_1)
#define kPAS0RReg				(0x2003 | BURGUNDY_LENGTH_1)

#define kGAS_Default_Gain			0xDF

#define kGAS1LReg				(0x2100 | BURGUNDY_LENGTH_1)
#define kPAS1LReg				(0x2101 | BURGUNDY_LENGTH_1)
#define kGAS1RReg				(0x2102 | BURGUNDY_LENGTH_1)
#define kPAS1RReg				(0x2103 | BURGUNDY_LENGTH_1)

#define kGAS2LReg				(0x2200 | BURGUNDY_LENGTH_1)
#define kPAS2LReg				(0x2201 | BURGUNDY_LENGTH_1)
#define kGAS2RReg				(0x2202 | BURGUNDY_LENGTH_1)
#define kPAS2RReg				(0x2203 | BURGUNDY_LENGTH_1)

#define kGAS3LReg				(0x2300 | BURGUNDY_LENGTH_1)
#define kGAS3RReg				(0x2301 | BURGUNDY_LENGTH_1)
#define kGAS4LReg				(0x2302 | BURGUNDY_LENGTH_1)
#define kGAS4RReg				(0x2303 | BURGUNDY_LENGTH_1)

#define kGASALReg				(0x2500 | BURGUNDY_LENGTH_1)
#define kGASARReg				(0x2501 | BURGUNDY_LENGTH_1)
#define kGASBLReg				(0x2502 | BURGUNDY_LENGTH_1)
#define kGASBRReg				(0x2503 | BURGUNDY_LENGTH_1)

#define kGASCLReg				(0x2600 | BURGUNDY_LENGTH_1)
#define kGASCRReg				(0x2601 | BURGUNDY_LENGTH_1)
#define kGASDLReg				(0x2602 | BURGUNDY_LENGTH_1)
#define kGASDRReg				(0x2603 | BURGUNDY_LENGTH_1)

#define kGASELReg				(0x2700 | BURGUNDY_LENGTH_1)
#define kGASERReg				(0x2701 | BURGUNDY_LENGTH_1)
#define kGASFLReg				(0x2702 | BURGUNDY_LENGTH_1)
#define kGASFRReg				(0x2703 | BURGUNDY_LENGTH_1)

#define kGASGLReg				(0x2800 | BURGUNDY_LENGTH_1)
#define kGASGRReg				(0x2801 | BURGUNDY_LENGTH_1)
#define kGASHLReg				(0x2802 | BURGUNDY_LENGTH_1)
#define kGASHRReg				(0x2803 | BURGUNDY_LENGTH_1)

/*
 * These registers control the inputs of four digital mixers.
 *
 * Each mixer may be provided any combination of the input sources mentioned
 * listed above.
 */
#define kMX0Reg					(0x2900 | BURGUNDY_LENGTH_4)
#define kMX0Reg_Select_IS0L			0x00000001
#define kMX0Reg_Select_IS1L			0x00000002
#define kMX0Reg_Select_IS2L			0x00000004
#define kMX0Reg_Select_IS3L			0x00000008
#define kMX0Reg_Select_IS4L			0x00000010
#define kMX0Reg_Select_ISAL			0x00000100
#define kMX0Reg_Select_ISBL			0x00000200
#define kMX0Reg_Select_ISCL			0x00000400
#define kMX0Reg_Select_ISDL			0x00000800
#define kMX0Reg_Select_ISEL			0x00001000
#define kMX0Reg_Select_ISFL			0x00002000
#define kMX0Reg_Select_ISGL			0x00004000
#define kMX0Reg_Select_ISHL			0x00008000
#define kMX0Reg_Select_IS0R			0x00010000
#define kMX0Reg_Select_IS1R			0x00020000
#define kMX0Reg_Select_IS2R			0x00040000
#define kMX0Reg_Select_IS3R			0x00080000
#define kMX0Reg_Select_IS4R			0x00100000
#define kMX0Reg_Select_ISAR			0x01000000
#define kMX0Reg_Select_ISBR			0x02000000
#define kMX0Reg_Select_ISCR			0x04000000
#define kMX0Reg_Select_ISDR			0x08000000
#define kMX0Reg_Select_ISER			0x10000000
#define kMX0Reg_Select_ISFR			0x20000000
#define kMX0Reg_Select_ISGR			0x40000000
#define kMX0Reg_Select_ISHR			0x80000000

#define kMX1Reg					(0x2A00 | BURGUNDY_LENGTH_4)
#define kMX1Reg_Select_IS0L			0x00000001
#define kMX1Reg_Select_IS1L			0x00000002
#define kMX1Reg_Select_IS2L			0x00000004
#define kMX1Reg_Select_IS3L			0x00000008
#define kMX1Reg_Select_IS4L			0x00000010
#define kMX1Reg_Select_ISAL			0x00000100
#define kMX1Reg_Select_ISBL			0x00000200
#define kMX1Reg_Select_ISCL			0x00000400
#define kMX1Reg_Select_ISDL			0x00000800
#define kMX1Reg_Select_ISEL			0x00001000
#define kMX1Reg_Select_ISFL			0x00002000
#define kMX1Reg_Select_ISGL			0x00004000
#define kMX1Reg_Select_ISHL			0x00008000
#define kMX1Reg_Select_IS0R			0x00010000
#define kMX1Reg_Select_IS1R			0x00020000
#define kMX1Reg_Select_IS2R			0x00040000
#define kMX1Reg_Select_IS3R			0x00080000
#define kMX1Reg_Select_IS4R			0x00100000
#define kMX1Reg_Select_ISAR			0x01000000
#define kMX1Reg_Select_ISBR			0x02000000
#define kMX1Reg_Select_ISCR			0x04000000
#define kMX1Reg_Select_ISDR			0x08000000
#define kMX1Reg_Select_ISER			0x10000000
#define kMX1Reg_Select_ISFR			0x20000000
#define kMX1Reg_Select_ISGR			0x40000000
#define kMX1Reg_Select_ISHR			0x80000000

#define kMX2Reg					(0x2B00 | BURGUNDY_LENGTH_4)
#define kMX2Reg_Select_IS0L			0x00000001
#define kMX2Reg_Select_IS1L			0x00000002
#define kMX2Reg_Select_IS2L			0x00000004
#define kMX2Reg_Select_IS3L			0x00000008
#define kMX2Reg_Select_IS4L			0x00000010
#define kMX2Reg_Select_ISAL			0x00000100
#define kMX2Reg_Select_ISBL			0x00000200
#define kMX2Reg_Select_ISCL			0x00000400
#define kMX2Reg_Select_ISDL			0x00000800
#define kMX2Reg_Select_ISEL			0x00001000
#define kMX2Reg_Select_ISFL			0x00002000
#define kMX2Reg_Select_ISGL			0x00004000
#define kMX2Reg_Select_ISHL			0x00008000
#define kMX2Reg_Select_IS0R			0x00010000
#define kMX2Reg_Select_IS1R			0x00020000
#define kMX2Reg_Select_IS2R			0x00040000
#define kMX2Reg_Select_IS3R			0x00080000
#define kMX2Reg_Select_IS4R			0x00100000
#define kMX2Reg_Select_ISAR			0x01000000
#define kMX2Reg_Select_ISBR			0x02000000
#define kMX2Reg_Select_ISCR			0x04000000
#define kMX2Reg_Select_ISDR			0x08000000
#define kMX2Reg_Select_ISER			0x10000000
#define kMX2Reg_Select_ISFR			0x20000000
#define kMX2Reg_Select_ISGR			0x40000000
#define kMX2Reg_Select_ISHR			0x80000000

#define kMX3Reg					(0x2C00 | BURGUNDY_LENGTH_4)
#define kMX3Reg_Select_IS0L			0x00000001
#define kMX3Reg_Select_IS1L			0x00000002
#define kMX3Reg_Select_IS2L			0x00000004
#define kMX3Reg_Select_IS3L			0x00000008
#define kMX3Reg_Select_IS4L			0x00000010
#define kMX3Reg_Select_ISAL			0x00000100
#define kMX3Reg_Select_ISBL			0x00000200
#define kMX3Reg_Select_ISCL			0x00000400
#define kMX3Reg_Select_ISDL			0x00000800
#define kMX3Reg_Select_ISEL			0x00001000
#define kMX3Reg_Select_ISFL			0x00002000
#define kMX3Reg_Select_ISGL			0x00004000
#define kMX3Reg_Select_ISHL			0x00008000
#define kMX3Reg_Select_IS0R			0x00010000
#define kMX3Reg_Select_IS1R			0x00020000
#define kMX3Reg_Select_IS2R			0x00040000
#define kMX3Reg_Select_IS3R			0x00080000
#define kMX3Reg_Select_IS4R			0x00100000
#define kMX3Reg_Select_ISAR			0x01000000
#define kMX3Reg_Select_ISBR			0x02000000
#define kMX3Reg_Select_ISCR			0x04000000
#define kMX3Reg_Select_ISDR			0x08000000
#define kMX3Reg_Select_ISER			0x10000000
#define kMX3Reg_Select_ISFR			0x20000000
#define kMX3Reg_Select_ISGR			0x40000000
#define kMX3Reg_Select_ISHR			0x80000000

/*
 * This register controls a digital scalar at the output of each mixer.
 */
#define kMXEQ0LReg				(0x2D00 | BURGUNDY_LENGTH_1)
#define kMXEQ0RReg				(0x2D01 | BURGUNDY_LENGTH_1)
#define kMXEQ1LReg				(0x2D02 | BURGUNDY_LENGTH_1)
#define kMXEQ1RReg				(0x2D03 | BURGUNDY_LENGTH_1)

#define kMXEQ2LReg				(0x2E00 | BURGUNDY_LENGTH_1)
#define kMXEQ2RReg				(0x2E01 | BURGUNDY_LENGTH_1)
#define kMXEQ3LReg				(0x2E02 | BURGUNDY_LENGTH_1)
#define kMXEQ3RReg				(0x2E03 | BURGUNDY_LENGTH_1)

#define kMXEQ_Default_Gain			0xDF

/*
 * This register controls a digital demultiplexer which routes
 * the mixer 0-3 output to one 12 output sources.
 *
 * Output sources 0-2 can eventually be converted to analog. The
 * remaining output sources remain digital and may be either
 * sent to the host or wrapped back as digital input sources.
 */
#define kOSReg					(0x2F00 | BURGUNDY_LENGTH_4)
#define kOSReg_OS0_SelectMask			0x00000003
#define kOSReg_OS0_SelectBit			0x00000001
#define kOSReg_OS0_Select_MXO0			0x00000000
#define kOSReg_OS0_Select_MXO1			0x00000001
#define kOSReg_OS0_Select_MXO2			0x00000002
#define kOSReg_OS0_Select_MXO3			0x00000003

#define kOSReg_OS1_SelectMask			0x0000000C
#define kOSReg_OS1_SelectBit			0x00000004
#define kOSReg_OS1_Select_MXO0			0x00000000
#define kOSReg_OS1_Select_MXO1			0x00000004
#define kOSReg_OS1_Select_MXO2			0x00000008
#define kOSReg_OS1_Select_MXO3			0x0000000C

#define kOSReg_OS2_SelectMask			0x00000030
#define kOSReg_OS2_SelectBit			0x00000010
#define kOSReg_OS2_Select_MXO0			0x00000000
#define kOSReg_OS2_Select_MXO1			0x00000010
#define kOSReg_OS2_Select_MXO2			0x00000020
#define kOSReg_OS2_Select_MXO3			0x00000030

#define kOSReg_OS3_SelectMask			0x000000C0
#define kOSReg_OS3_SelectBit			0x00000040
#define kOSReg_OS3_Select_MXO0			0x00000000
#define kOSReg_OS3_Select_MXO1			0x00000040
#define kOSReg_OS3_Select_MXO2			0x00000080
#define kOSReg_OS3_Select_MXO3			0x000000C0

#define kOSReg_OSA_SelectMask			0x00030000
#define kOSReg_OSA_SelectBit			0x00010000
#define kOSReg_OSA_Select_MXO0			0x00000000
#define kOSReg_OSA_Select_MXO1			0x00010000
#define kOSReg_OSA_Select_MXO2			0x00020000
#define kOSReg_OSA_Select_MXO3			0x00030000

#define kOSReg_OSB_SelectMask			0x000C0000
#define kOSReg_OSB_SelectBit			0x00040000
#define kOSReg_OSB_Select_MXO0			0x00000000
#define kOSReg_OSB_Select_MXO1			0x00040000
#define kOSReg_OSB_Select_MXO2			0x00080000
#define kOSReg_OSB_Select_MXO3			0x000C0000

#define kOSReg_OSC_SelectMask			0x00100000
#define kOSReg_OSC_SelectBit			0x00300000
#define kOSReg_OSC_Select_MXO0			0x00000000
#define kOSReg_OSC_Select_MXO1			0x00100000
#define kOSReg_OSC_Select_MXO2			0x00200000
#define kOSReg_OSC_Select_MXO3			0x00300000

#define kOSReg_OSD_SelectMask			0x00C00000
#define kOSReg_OSD_SelectBit			0x00400000
#define kOSReg_OSD_Select_MXO0			0x00000000
#define kOSReg_OSD_Select_MXO1			0x00400000
#define kOSReg_OSD_Select_MXO2			0x00800000
#define kOSReg_OSD_Select_MXO3			0x00C00000

#define kOSReg_OSE_SelectMask			0x03000000
#define kOSReg_OSE_SelectBit			0x01000000
#define kOSReg_OSE_Select_MXO0			0x00000000
#define kOSReg_OSE_Select_MXO1			0x01000000
#define kOSReg_OSE_Select_MXO2			0x02000000
#define kOSReg_OSE_Select_MXO3			0x03000000

#define kOSReg_OSF_SelectMask			0x0C000000
#define kOSReg_OSF_SelectBit			0x04000000
#define kOSReg_OSF_Select_MXO0			0x00000000
#define kOSReg_OSF_Select_MXO1			0x04000000
#define kOSReg_OSF_Select_MXO2			0x08000000
#define kOSReg_OSF_Select_MXO3			0x0C000000

#define kOSReg_OSG_SelectMask			0x30000000
#define kOSReg_OSG_SelectBit			0x10000000
#define kOSReg_OSG_Select_MXO0			0x00000000
#define kOSReg_OSG_Select_MXO1			0x10000000
#define kOSReg_OSG_Select_MXO2			0x20000000
#define kOSReg_OSG_Select_MXO3			0x30000000

#define kOSReg_OSH_SelectMask			0xC0000000
#define kOSReg_OSH_SelectBit			0x40000000
#define kOSReg_OSH_Select_MXO0			0x00000000
#define kOSReg_OSH_Select_MXO1			0x40000000
#define kOSReg_OSH_Select_MXO2			0x80000000
#define kOSReg_OSH_Select_MXO3			0xC0000000

/*
 * This register controls a digital scalar for Output sources 0-3
 */
#define kGAP0LReg				(0x3000 | BURGUNDY_LENGTH_1)
#define kGAP0RReg				(0x3001 | BURGUNDY_LENGTH_1)
#define kGAP1LReg				(0x3002 | BURGUNDY_LENGTH_1)
#define kGAP1RReg				(0x3003 | BURGUNDY_LENGTH_1)

#define kGAP2LReg				(0x3100 | BURGUNDY_LENGTH_1)
#define kGAP2RReg				(0x3101 | BURGUNDY_LENGTH_1)
#define kGAP3LReg				(0x3102 | BURGUNDY_LENGTH_1)
#define kGAP3RReg				(0x3103 | BURGUNDY_LENGTH_1)

#define kGAP_Default_Gain			0xDF

/*
 * These registers access the values of four peak-level meters. Inputs to the
 * digital mixers MX0-3 or the mixer outputs may be routed these meters.
 */
#define kPeakLvl0Reg				(0x3300 | BURGUNDY_LENGTH_2)
#define kPeakLvl1Reg				(0x3302 | BURGUNDY_LENGTH_2)

#define kPeakLvl3Reg				(0x3400 | BURGUNDY_LENGTH_2)
#define kPeakLvl4Reg				(0x3402 | BURGUNDY_LENGTH_2)

/*
 * These registers select the digital inputs to be monitored by each level meter.
 */
#define kPeakLvl0SourceReg			(0x3500 | BURGUNDY_LENGTH_1)
#define kPeakLvl0SourceReg_SelectMask		0x3F
#define kPeakLvl0SourceReg_Select_AS0L		0x00
#define kPeakLvl0SourceReg_Select_AS0R		0x01
#define kPeakLvl0SourceReg_Select_AS1L		0x02
#define kPeakLvl0SourceReg_Select_AS1R		0x03
#define kPeakLvl0SourceReg_Select_AS2L		0x04
#define kPeakLvl0SourceReg_Select_AS2R		0x05
#define kPeakLvl0SourceReg_Select_AS3L		0x06
#define kPeakLvl0SourceReg_Select_AS3R		0x07
#define kPeakLvl0SourceReg_Select_AS4L		0x08
#define kPeakLvl0SourceReg_Select_AS4R		0x09
#define kPeakLvl0SourceReg_Select_IS0L		0x10
#define kPeakLvl0SourceReg_Select_IS0R		0x11
#define kPeakLvl0SourceReg_Select_IS1L		0x12
#define kPeakLvl0SourceReg_Select_IS1R		0x13
#define kPeakLvl0SourceReg_Select_IS2L		0x14
#define kPeakLvl0SourceReg_Select_IS2R		0x15
#define kPeakLvl0SourceReg_Select_IS3L		0x16
#define kPeakLvl0SourceReg_Select_IS3R		0x17
#define kPeakLvl0SourceReg_Select_IS4L		0x18
#define kPeakLvl0SourceReg_Select_IS5R		0x19
#define kPeakLvl0SourceReg_Select_ISAL		0x20
#define kPeakLvl0SourceReg_Select_ISAR		0x21
#define kPeakLvl0SourceReg_Select_ISBL		0x22
#define kPeakLvl0SourceReg_Select_ISBR		0x23
#define kPeakLvl0SourceReg_Select_ISCL		0x24
#define kPeakLvl0SourceReg_Select_ISCR		0x25
#define kPeakLvl0SourceReg_Select_ISDL		0x26
#define kPeakLvl0SourceReg_Select_ISDR		0x27
#define kPeakLvl0SourceReg_Select_ISEL		0x28
#define kPeakLvl0SourceReg_Select_ISER		0x29
#define kPeakLvl0SourceReg_Select_ISFL		0x2A
#define kPeakLvl0SourceReg_Select_ISFR		0x2B
#define kPeakLvl0SourceReg_Select_ISGL		0x2C
#define kPeakLvl0SourceReg_Select_ISGR		0x2D
#define kPeakLvl0SourceReg_Select_ISHL		0x2E
#define kPeakLvl0SourceReg_Select_ISHR		0x2F
#define kPeakLvl0SourceReg_Select_MXO0L		0x30
#define kPeakLvl0SourceReg_Select_MXO0R		0x31
#define kPeakLvl0SourceReg_Select_MXO1L		0x32
#define kPeakLvl0SourceReg_Select_MXO1R		0x33
#define kPeakLvl0SourceReg_Select_MXO2L		0x34
#define kPeakLvl0SourceReg_Select_MXO2R		0x35
#define kPeakLvl0SourceReg_Select_MXO3L		0x36
#define kPeakLvl0SourceReg_Select_MXO3R		0x37
#define kPeakLvl0SourceReg_Zero			0x40

#define kPeakLvl1SourceReg			(0x3501 | BURGUNDY_LENGTH_1)
#define kPeakLvl1SourceReg_SelectMask		0x3F
#define kPeakLvl1SourceReg_Select_AS0L		0x00
#define kPeakLvl1SourceReg_Select_AS0R		0x01
#define kPeakLvl1SourceReg_Select_AS1L		0x02
#define kPeakLvl1SourceReg_Select_AS1R		0x03
#define kPeakLvl1SourceReg_Select_AS2L		0x04
#define kPeakLvl1SourceReg_Select_AS2R		0x05
#define kPeakLvl1SourceReg_Select_AS3L		0x06
#define kPeakLvl1SourceReg_Select_AS3R		0x07
#define kPeakLvl1SourceReg_Select_AS4L		0x08
#define kPeakLvl1SourceReg_Select_AS4R		0x09
#define kPeakLvl1SourceReg_Select_IS0L		0x10
#define kPeakLvl1SourceReg_Select_IS0R		0x11
#define kPeakLvl1SourceReg_Select_IS1L		0x12
#define kPeakLvl1SourceReg_Select_IS1R		0x13
#define kPeakLvl1SourceReg_Select_IS2L		0x14
#define kPeakLvl1SourceReg_Select_IS2R		0x15
#define kPeakLvl1SourceReg_Select_IS3L		0x16
#define kPeakLvl1SourceReg_Select_IS3R		0x17
#define kPeakLvl1SourceReg_Select_IS4L		0x18
#define kPeakLvl1SourceReg_Select_IS5R		0x19
#define kPeakLvl1SourceReg_Select_ISAL		0x20
#define kPeakLvl1SourceReg_Select_ISAR		0x21
#define kPeakLvl1SourceReg_Select_ISBL		0x22
#define kPeakLvl1SourceReg_Select_ISBR		0x23
#define kPeakLvl1SourceReg_Select_ISCL		0x24
#define kPeakLvl1SourceReg_Select_ISCR		0x25
#define kPeakLvl1SourceReg_Select_ISDL		0x26
#define kPeakLvl1SourceReg_Select_ISDR		0x27
#define kPeakLvl1SourceReg_Select_ISEL		0x28
#define kPeakLvl1SourceReg_Select_ISER		0x29
#define kPeakLvl1SourceReg_Select_ISFL		0x2A
#define kPeakLvl1SourceReg_Select_ISFR		0x2B
#define kPeakLvl1SourceReg_Select_ISGL		0x2C
#define kPeakLvl1SourceReg_Select_ISGR		0x2D
#define kPeakLvl1SourceReg_Select_ISHL		0x2E
#define kPeakLvl1SourceReg_Select_ISHR		0x2F
#define kPeakLvl1SourceReg_Select_MXO0L		0x30
#define kPeakLvl1SourceReg_Select_MXO0R		0x31
#define kPeakLvl1SourceReg_Select_MXO1L		0x32
#define kPeakLvl1SourceReg_Select_MXO1R		0x33
#define kPeakLvl1SourceReg_Select_MXO2L		0x34
#define kPeakLvl1SourceReg_Select_MXO2R		0x35
#define kPeakLvl1SourceReg_Select_MXO3L		0x36
#define kPeakLvl1SourceReg_Select_MXO3R		0x37
#define kPeakLvl1SourceReg_Zero			0x40

#define kPeakLvl2SourceReg			(0x3502 | BURGUNDY_LENGTH_1)
#define kPeakLvl2SourceReg_SelectMask		0x3F
#define kPeakLvl2SourceReg_Select_AS0L		0x00
#define kPeakLvl2SourceReg_Select_AS0R		0x01
#define kPeakLvl2SourceReg_Select_AS1L		0x02
#define kPeakLvl2SourceReg_Select_AS1R		0x03
#define kPeakLvl2SourceReg_Select_AS2L		0x04
#define kPeakLvl2SourceReg_Select_AS2R		0x05
#define kPeakLvl2SourceReg_Select_AS3L		0x06
#define kPeakLvl2SourceReg_Select_AS3R		0x07
#define kPeakLvl2SourceReg_Select_AS4L		0x08
#define kPeakLvl2SourceReg_Select_AS4R		0x09
#define kPeakLvl2SourceReg_Select_IS0L		0x10
#define kPeakLvl2SourceReg_Select_IS0R		0x11
#define kPeakLvl2SourceReg_Select_IS1L		0x12
#define kPeakLvl2SourceReg_Select_IS1R		0x13
#define kPeakLvl2SourceReg_Select_IS2L		0x14
#define kPeakLvl2SourceReg_Select_IS2R		0x15
#define kPeakLvl2SourceReg_Select_IS3L		0x16
#define kPeakLvl2SourceReg_Select_IS3R		0x17
#define kPeakLvl2SourceReg_Select_IS4L		0x18
#define kPeakLvl2SourceReg_Select_IS5R		0x19
#define kPeakLvl2SourceReg_Select_ISAL		0x20
#define kPeakLvl2SourceReg_Select_ISAR		0x21
#define kPeakLvl2SourceReg_Select_ISBL		0x22
#define kPeakLvl2SourceReg_Select_ISBR		0x23
#define kPeakLvl2SourceReg_Select_ISCL		0x24
#define kPeakLvl2SourceReg_Select_ISCR		0x25
#define kPeakLvl2SourceReg_Select_ISDL		0x26
#define kPeakLvl2SourceReg_Select_ISDR		0x27
#define kPeakLvl2SourceReg_Select_ISEL		0x28
#define kPeakLvl2SourceReg_Select_ISER		0x29
#define kPeakLvl2SourceReg_Select_ISFL		0x2A
#define kPeakLvl2SourceReg_Select_ISFR		0x2B
#define kPeakLvl2SourceReg_Select_ISGL		0x2C
#define kPeakLvl2SourceReg_Select_ISGR		0x2D
#define kPeakLvl2SourceReg_Select_ISHL		0x2E
#define kPeakLvl2SourceReg_Select_ISHR		0x2F
#define kPeakLvl2SourceReg_Select_MXO0L		0x30
#define kPeakLvl2SourceReg_Select_MXO0R		0x31
#define kPeakLvl2SourceReg_Select_MXO1L		0x32
#define kPeakLvl2SourceReg_Select_MXO1R		0x33
#define kPeakLvl2SourceReg_Select_MXO2L		0x34
#define kPeakLvl2SourceReg_Select_MXO2R		0x35
#define kPeakLvl2SourceReg_Select_MXO3L		0x36
#define kPeakLvl2SourceReg_Select_MXO3R		0x37
#define kPeakLvl2SourceReg_Zero			0x40

#define kPeakLvl3SourceReg			(0x3503 | BURGUNDY_LENGTH_1)
#define kPeakLvl3SourceReg_SelectMask		0x3F
#define kPeakLvl3SourceReg_Select_AS0L		0x00
#define kPeakLvl3SourceReg_Select_AS0R		0x01
#define kPeakLvl3SourceReg_Select_AS1L		0x02
#define kPeakLvl3SourceReg_Select_AS1R		0x03
#define kPeakLvl3SourceReg_Select_AS2L		0x04
#define kPeakLvl3SourceReg_Select_AS2R		0x05
#define kPeakLvl3SourceReg_Select_AS3L		0x06
#define kPeakLvl3SourceReg_Select_AS3R		0x07
#define kPeakLvl3SourceReg_Select_AS4L		0x08
#define kPeakLvl3SourceReg_Select_AS4R		0x09
#define kPeakLvl3SourceReg_Select_IS0L		0x10
#define kPeakLvl3SourceReg_Select_IS0R		0x11
#define kPeakLvl3SourceReg_Select_IS1L		0x12
#define kPeakLvl3SourceReg_Select_IS1R		0x13
#define kPeakLvl3SourceReg_Select_IS2L		0x14
#define kPeakLvl3SourceReg_Select_IS2R		0x15
#define kPeakLvl3SourceReg_Select_IS3L		0x16
#define kPeakLvl3SourceReg_Select_IS3R		0x17
#define kPeakLvl3SourceReg_Select_IS4L		0x18
#define kPeakLvl3SourceReg_Select_IS5R		0x19
#define kPeakLvl3SourceReg_Select_ISAL		0x20
#define kPeakLvl3SourceReg_Select_ISAR		0x21
#define kPeakLvl3SourceReg_Select_ISBL		0x22
#define kPeakLvl3SourceReg_Select_ISBR		0x23
#define kPeakLvl3SourceReg_Select_ISCL		0x24
#define kPeakLvl3SourceReg_Select_ISCR		0x25
#define kPeakLvl3SourceReg_Select_ISDL		0x26
#define kPeakLvl3SourceReg_Select_ISDR		0x27
#define kPeakLvl3SourceReg_Select_ISEL		0x28
#define kPeakLvl3SourceReg_Select_ISER		0x29
#define kPeakLvl3SourceReg_Select_ISFL		0x2A
#define kPeakLvl3SourceReg_Select_ISFR		0x2B
#define kPeakLvl3SourceReg_Select_ISGL		0x2C
#define kPeakLvl3SourceReg_Select_ISGR		0x2D
#define kPeakLvl3SourceReg_Select_ISHL		0x2E
#define kPeakLvl3SourceReg_Select_ISHR		0x2F
#define kPeakLvl3SourceReg_Select_MXO0L		0x30
#define kPeakLvl3SourceReg_Select_MXO0R		0x31
#define kPeakLvl3SourceReg_Select_MXO1L		0x32
#define kPeakLvl3SourceReg_Select_MXO1R		0x33
#define kPeakLvl3SourceReg_Select_MXO2L		0x34
#define kPeakLvl3SourceReg_Select_MXO2R		0x35
#define kPeakLvl3SourceReg_Select_MXO3L		0x36
#define kPeakLvl3SourceReg_Select_MXO3R		0x37
#define kPeakLvl3SourceReg_Zero			0x40

/*
 * This register holds the threshold value at which the level meters will
 * indicate an event.
 */
#define kPeakLvlThresholdReg			(0x3600 | BURGUNDY_LENGTH_2)
#define kPeakLvlThresholdReg_Default		0x7FFF

/*
 * These registers indicate overflows in the digital scalars prior to the mixers.
 * See GAS0-4.
 */
#define kISOverflowReg				(0x3700 | BURGUNDY_LENGTH_1)
#define kISOverflowReg_IS0			0x01
#define kISOverflowReg_IS1			0x02
#define kISOverflowReg_IS2			0x04
#define kISOverflowReg_IS3			0x08
#define kISOverflowReg_IS4			0x10
#define kISOverflowReg_Indicator		0x80

/*
 * This register monitors overflows in digital mixers 0-3.
 */
#define kMXOverflowReg				(0x3701 | BURGUNDY_LENGTH_2)
#define kMXOverflowReg_MX0L			0x0001
#define kMXOverflowReg_MX0R			0x0002
#define kMXOverflowReg_MX1L			0x0004
#define kMXOverflowReg_MX1R			0x0008
#define kMXOverflowReg_MX2L			0x0010
#define kMXOverflowReg_MX2R			0x0020
#define kMXOverflowReg_MX3L			0x0040
#define kMXOverflowReg_MX3R			0x0080
#define kMXOverflowReg_MX0L_Indicator		0x0100
#define kMXOverflowReg_MX0R_Indicator		0x0200
#define kMXOverflowReg_MX1L_Indicator		0x0400
#define kMXOverflowReg_MX1R_Indicator		0x0800
#define kMXOverflowReg_MX2L_Indicator		0x1000
#define kMXOverflowReg_MX2R_Indicator		0x2000
#define kMXOverflowReg_MX3L_Indicator		0x4000
#define kMXOverflowReg_MX3R_Indicator		0x8000

/*
 * This register controls a digital tone filter connected to the output
 * of mixer 0.
 *
 * The registers are programmed with 3-byte coefficients for the filters.
 */
#define kSOSS0B0Reg				(0x4000 | BURGUNDY_LENGTH_3)
#define kSOSS0B1Reg				(0x4100 | BURGUNDY_LENGTH_3)
#define kSOSS0B2Reg				(0x4200 | BURGUNDY_LENGTH_3)
#define kSOSS0A1Reg				(0x4300 | BURGUNDY_LENGTH_3)
#define kSOSS0A2Reg				(0x4400 | BURGUNDY_LENGTH_3)

#define kSOSS1B0Reg				(0x4500 | BURGUNDY_LENGTH_3)
#define kSOSS1B1Reg				(0x4600 | BURGUNDY_LENGTH_3)
#define kSOSS1B2Reg				(0x4700 | BURGUNDY_LENGTH_3)
#define kSOSS1A1Reg				(0x4800 | BURGUNDY_LENGTH_3)
#define kSOSS1A2Reg				(0x4900 | BURGUNDY_LENGTH_3)

#define kSOSS2B0Reg				(0x4A00 | BURGUNDY_LENGTH_3)
#define kSOSS2B1Reg				(0x4B00 | BURGUNDY_LENGTH_3)
#define kSOSS2B2Reg				(0x4C00 | BURGUNDY_LENGTH_3)
#define kSOSS2A1Reg				(0x4D00 | BURGUNDY_LENGTH_3)
#define kSOSS2A2Reg				(0x4E00 | BURGUNDY_LENGTH_3)

#define kSOSS3B0Reg				(0x5000 | BURGUNDY_LENGTH_3)
#define kSOSS3B1Reg				(0x5100 | BURGUNDY_LENGTH_3)
#define kSOSS3A1Reg				(0x5200 | BURGUNDY_LENGTH_3)
#define kSOSS3A2Reg				(0x5300 | BURGUNDY_LENGTH_3)

#define kSOSControlReg				(0x5500 | BURGUNDY_LENGTH_1)
#define kSOSControlReg_Mode0			0x00
#define kSOSControlReg_Mode1			0x01

#define kSOSOverflowReg				(0x5600 | BURGUNDY_LENGTH_1)
#define kSOSOverflowReg_S0			0x01
#define kSOSOverflowReg_S1			0x02
#define kSOSOverflowReg_S2			0x04
#define kSOSOverflowReg_S3			0x08
#define kSOSOverflowReg_Indicator		0x10

/*
 * This register controls the muting the of analog outputs
 */
#define kOutputMuteReg	 			(0x6000 | BURGUNDY_LENGTH_1)
#define kOutputMuteReg_Port13M			0x01
#define kOutputMuteReg_Port14L			0x02
#define kOutputMuteReg_Port14R			0x04
#define kOutputMuteReg_Port15L			0x08
#define kOutputMuteReg_Port15R			0x10
#define kOutputMuteReg_Port16L			0x20
#define kOutputMuteReg_Port16R			0x40
#define kOutputMuteReg_Port17M			0x80

/*
 * These registers control attenuators at each analog output
 */
#define kOutputLvlPort13Reg			(0x6100 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort13Reg_Mask		0x0F
#define kOutputLvlPort13Reg_Bit			0x01

#define kOutputLvl_Default			0x00

#define kOutputLvlPort14Reg			(0x6200 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort14Reg_LeftMask		0x0F
#define kOutputLvlPort14Reg_LeftBit		0x01
#define kOutputLvlPort14Reg_RightMask		0xF0
#define kOutputLvlPort14Reg_RightBit		0x10

#define kOutputLvlPort15Reg			(0x6300 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort15Reg_LeftMask		0x0F
#define kOutputLvlPort15Reg_LeftBit		0x01
#define kOutputLvlPort15Reg_RightMask		0xF0
#define kOutputLvlPort15Reg_RightBit		0x10

#define kOutputLvlPort16Reg			(0x6400 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort16Reg_LeftMask		0x0F
#define kOutputLvlPort16Reg_LeftBit		0x01
#define kOutputLvlPort16Reg_RightMask		0xF0
#define kOutputLvlPort16Reg_RightBit		0x10

#define kOutputLvlPort17Reg			(0x6500 | BURGUNDY_LENGTH_1)
#define kOutputLvlPort17Reg_Mask		0x0F
#define kOutputLvlPort17Reg_Bit			0x01

/*
 * This register controls discharge current to reduce noise if the chip
 * is powered down.
 */
#define kOutputSettleTimeReg			(0x6600 | BURGUNDY_LENGTH_2)
#define kOutputSettleTimeReg_Port13_Mask	0x0003
#define kOutputSettleTimeReg_Port13_Bit		0x0001
#define kOutputSettleTimeReg_Port14_Mask	0x000C
#define kOutputSettleTimeReg_Port14_Bit		0x0004
#define kOutputSettleTimeReg_Port15_Mask	0x0030
#define kOutputSettleTimeReg_Port15_Bit		0x0010
#define kOutputSettleTimeReg_Port16_Mask	0x00C0
#define kOutputSettleTimeReg_Port16_Bit		0x0040
#define kOutputSettleTimeReg_Port17_Mask	0x0300
#define kOutputSettleTimeReg_Port17_Bit		0x0100


#define kOutputCtl0Reg				(0x6700 | BURGUNDY_LENGTH_1)
#define kOutputCtl0Reg_OutCtl0_High		0x10
#define kOutputCtl0Reg_OutCtl0_Tristate		0x20
#define kOutputCtl0Reg_OutCtl1_High		0x40
#define kOutputCtl0Reg_OutCtl1_Tristate		0x80

#define kOutputCtl2Reg				(0x6800 | BURGUNDY_LENGTH_1)
#define kOutputCtl2Reg_OutCtl2_High		0x01
#define kOutputCtl2Reg_OutCtl2_Tristate		0x02
#define kOutputCtl2Reg_OutCtl3_High		0x04
#define kOutputCtl2Reg_OutCtl3_Tristate		0x08
#define kOutputCtl2Reg_OutCtl4_High		0x10
#define kOutputCtl2Reg_OutCtl4_Tristate		0x20

#define kDOutConfigReg				(0x6900 | BURGUNDY_LENGTH_1)
#define kDOutConfigReg_Zero			0x80
#define kDOutConfigReg_Enable			0x80

#define kMClkReg				(0x7000 | BURGUNDY_LENGTH_1)
#define kMClkReg_Tristate			0x00
#define kMClkReg_Div1				0x01
#define kMClkReg_Div2				0x02
#define kMClkReg_Div4				0x03


/*
 * These registers control the digital input sources A-C to the chip.
 * A source may be derived from data comming from the host (SF0-3) or
 * from an internally generated digital output source (OSA-D)
 */
#define kSDInReg				(0x7800 | BURGUNDY_LENGTH_1)
#define kSDInReg_OSA_To_SF0			0x01
#define kSDInReg_ASA_Mask			0x02
#define kSDInReg_ASA_From_SF0			0x00
#define kSDInReg_ASA_From_OSA			0x02
#define kSDInReg_OSB_To_SF1			0x04
#define kSDInReg_ASB_From_SF1			0x00
#define kSDInReg_ASB_From_OSB			0x08
#define kSDInReg_OSC_To_SF2			0x10
#define kSDInReg_ASC_From_SF2			0x00
#define kSDInReg_ASC_From_OSC			0x20
#define kSDInReg_OSD_To_SF3			0x40
#define kSDInReg_ASD_From_SF3			0x00
#define kSDInReg_ASD_From_OSD			0x80

#define kSDInReg2				(0x7900 | BURGUNDY_LENGTH_1)


/*
 * This register controls digital output sources (OSE-H) from the chip.
 * These sources may be routed to the host (SF0-3) or may be internally
 * wrapped back to input sources (ASE-H).
 */
#define kSDOutReg				(0x7A00 | BURGUNDY_LENGTH_1)
#define kSDOutReg_OSE_To_SF0			0x01
#define kSDOutReg_ASE_Mask			0x02
#define kSDOutReg_ASE_From_SF0			0x00
#define kSDOutReg_ASE_From_OSE			0x02
#define kSDOutReg_OSF_To_SF1			0x04
#define kSDOutReg_ASF_From_SF1			0x00
#define kSDOutReg_ASF_From_OSF			0x08
#define kSDOutReg_OSG_To_SF2			0x10
#define kSDOutReg_ASG_From_SF2			0x00
#define kSDOutReg_ASG_From_OSG			0x20
#define kSDOutReg_OSH_To_SF3			0x40
#define kSDOutReg_ASH_From_SF3			0x00
#define kSDOutReg_ASH_From_OSH			0x80

#define kThresholdMaskReg			(0x7A00 | BURGUNDY_LENGTH_1)
#define kThresholdMaskReg_Threshold0		0x01
#define kThresholdMaskReg_Threshold1		0x02
#define kThresholdMaskReg_Threshold2		0x04
#define kThresholdMaskReg_Threshold3		0x08

/* From OS 9 Burgundy file*/
enum {
    kBurgundyRecalibrateReg	=	0x00000000,		//	register to force a recalibration of Burgandy
    kBurgundyRecalibrate	=	0x00000002,		//	recalibrate
    kBurgundyTwilight		=	0x00000004,		//	1 = initiate twilight & quietly shut down burgandy
    kRecalibrateDelay		=	2200,			//	number of milliseconds to delay after setting recalibrate
    kRecalibratePollDelay	=	100,	//	number of milliseconds to delay after setting recalibrate
    kTwilightDelay		=	330,			//	330 msec is from table 4.12 Power Management in Burgundy Spec +10% margin
	
    kBurgundyIDReg			=	0x00000001,		//	register for identifying silicon
    kBurgundySiliconRevision=	0x000000FF,		//	Silicon Revison Field
    kBurgundySiliconVersion	=	0x0000FF00,		//	Silicon Version Field
    kBurgundySiliconVendor	=	0x00FF0000,		//	Silicon Vendor Field
    kBurgundyIDField		=	0xFF000000,		//	Burgandy ID Field
    kBurgundyManfCrystal	=   0x00010000,		//	vendor ID for Crystal
    kBurgundyManfTI			=   0x00020000,		//	vendor ID for TI
    kBurgundyID				=	0x01000000,		//	Burgandy ID
	
    kBurgundyGlobalStatus	=	0x00000002,		//	Global Status Register
    kBurgundyAdcClippingStatus=	0x00000001,		//	1 indicates A/D Clipping Status Register 0x19 has a status bit set
    kBurgundyOverflowStatus0=	0x00000002,		//	1 indicates overflow status register 0x37 byte 00 has a status bit set
    kBurgundyOverflowStatus1=	0x00000004,		//	1 indicates overflow status register 0x37 byte 01 has a status bit set
    kBurgundySOSStatus		=	0x00000008,		//	1 indicates SOS status register 0x56 has a status bit set
    kBurgundyParallel_inStatus=	0x00000020,		//	1 indicates a change on parallel_in (cleared by reading x018)
    kBurgundyPkLvlMeter0ThldStatus	=	0x00000400,		//	1 indicates Peak Level Meter 0 ³ Threshold Level Status
    kBurgundyPkLvlMeter1ThldStatus	=	0x00000800,		//	1 indicates Peak Level Meter 1 ³ Threshold Level Status
    kBurgundyPkLvlMeter2ThldStatus	=	0x00001000,		//	1 indicates Peak Level Meter 2 ³ Threshold Level Status
    kBurgundyPkLvlMeter3ThldStatus	=	0x00002000,		//	1 indicates Peak Level Meter 3 ³ Threshold Level Status

    kBurgundyDataStatusAllZero=	0x0000000F,		//	When accessed this register causes the 'Register Read Data / Status Field
														//	to return zeros until a subsequent command requests a write or read.
													
	kBurgundyAnalogInputPortConfig	=	0x00000010,		//	Analog Input Port Configuration Register
	kBurgundyInPreampGainPort5L=	0x00000004,		//	input pre-amp gain port_5_left:  0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort5R=	0x00000008,		//	input pre-amp gain port_5_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort5=	0x0000000C,		//	input pre-amp gain port_5_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort6L=	0x00000010,		//	input pre-amp gain port_6_left:  0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort6R=	0x00000020,		//	input pre-amp gain port_6_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort6=	0x00000030,		//	input pre-amp gain port_6_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort7L=	0x00000040,		//	input pre-amp gain port_7_left:  0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort7R=	0x00000080,		//	input pre-amp gain port_6_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGainPort7=	0x000000C0,		//	input pre-amp gain port_6_right: 0 = 0dB, 1 = 24dB
	kBurgundyInPreampGain0dB=	0x00000000,		//	input pre-amp gain for all channels is 0.0 dB
	kBurgundyInPreampGain24dB=	0x000000FC,		//	input pre-amp gain for all channels is 24.0 dB
	
	kBurgundyAMux0_AMux1	=	0x00000011,		//	A_Mux_0 Configuration and A_Mux_1 Configuration
	kBurgundyAMux_0SelPort_1=	0x00000000,		//	selects Port_1 within A_Mux_0 Assign field
	kBurgundyAMux_0SelPort_2=	0x00000005,		//	selects Port_2 within A_Mux_0 Assign field
	kBurgundyAMux_0SelPort_3=	0x0000000A,		//	selects Port_3 within A_Mux_0 Assign field
	kBurgundyAMux_0LSelPort_1L=	0x00000000,		//	selects Port_1L within A_Mux_0L Assign field
	kBurgundyAMux_0LSelPort_2L=	0x00000001,		//	selects Port_2L within A_Mux_0L Assign field
	kBurgundyAMux_0LSelPort_3L=	0x00000002,		//	selects Port_3L within A_Mux_0L Assign field
	kBurgundyAMux_0LAssignField=	0x00000003,		//	A_Mux_0 Assign field
	kBurgundyAMux_0RSelPort_1R=	0x00000000,		//	selects Port_1R within A_Mux_0R Assign field
	kBurgundyAMux_0RSelPort_2R=	0x00000004,		//	selects Port_2R within A_Mux_0R Assign field
	kBurgundyAMux_0RSelPort_3R=	0x00000008,		//	selects Port_3R within A_Mux_0R Assign field
	kBurgundyAMux_0RAssignField=	0x0000000C,		//	A_Mux_1 Assign field
	kBurgundyAMux_1SelPort_3=	0x00000000,		//	selects Port_3 within A_Mux_1 Assign field
	kBurgundyAMux_1SelPort_4=	0x00000050,		//	selects Port_4 within A_Mux_1 Assign field
	kBurgundyAMux_1SelPort_5=	0x000000A0,		//	selects Port_5 within A_Mux_1 Assign field
	kBurgundyAMux_1LSelPort_3L=	0x00000000,		//	selects Port_3L within A_Mux_1L Assign field
	kBurgundyAMux_1LSelPort_4L=	0x00000010,		//	selects Port_4L within A_Mux_1L Assign field
	kBurgundyAMux_1LSelPort_5L=	0x00000020,		//	selects Port_5L within A_Mux_1L Assign field
	kBurgundyAMux_1LAssignField=	0x00000030,		//	A_Mux_1 Assign field
	kBurgundyAMux_1RSelPort_3R=	0x00000000,		//	selects Port_3R within A_Mux_1R Assign field
	kBurgundyAMux_1RSelPort_4R=	0x00000040,		//	selects Port_4R within A_Mux_1R Assign field
	kBurgundyAMux_1RSelPort_5R=	0x00000080,		//	selects Port_5R within A_Mux_1R Assign field
	kBurgundyAMux_1RAssignField=	0x000000C0,		//	A_Mux_1 Assign field
	
	kBurgundyAMux_2			=	0x00000012,		//	A_Mux_2 Configuration
	kBurgundyAMux_2SelPort_5=	0x00000000,		//	selects Port_5 within A_Mux_2 Assign field
	kBurgundyAMux_2SelPort_6=	0x00000005,		//	selects Port_6 within A_Mux_2 Assign field
	kBurgundyAMux_2SelPort_7=	0x0000000A,		//	selects Port_7 within A_Mux_2 Assign field
	kBurgundyAMux_2LSelPort_5L=	0x00000000,		//	selects Port_5L within A_Mux_2L Assign field
	kBurgundyAMux_2LSelPort_6L=	0x00000001,		//	selects Port_6L within A_Mux_2L Assign field
	kBurgundyAMux_2LSelPort_7L=	0x00000002,		//	selects Port_7L within A_Mux_2L Assign field
	kBurgundyAMux_2LAssignField=	0x00000003,		//	A_Mux_0 Assign field
	kBurgundyAMux_2RSelPort_5R=	0x00000000,		//	selects Port_5R within A_Mux_2R Assign field
	kBurgundyAMux_2RSelPort_6R=	0x00000004,		//	selects Port_6R within A_Mux_2R Assign field
	kBurgundyAMux_2RSelPort_7R=	0x00000008,		//	selects Port_7R within A_Mux_2R Assign field
	kBurgundyAMux_2RAssignField=	0x0000000C,		//	A_Mux_1 Assign field
	
	kBurgundyVGA_0GainZeroCrossDet	=	0x00000013,		//	VGA_0 Gain Zero Crossing Detection
	kBurgundyVGA_0LAssign	=	0x0000000F,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_0RAssign	=	0x000000F0,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_0RbitShift	=	0x00000004,		//	bit shift to right field
	
	kBurgundyVGA_1GainZeroCrossDet	=	0x00000014,		//	VGA_1 Gain Zero Crossing Detection
	kBurgundyVGA_1LAssign	=	0x0000000F,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_1RAssign	=	0x000000F0,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_1RbitShift	=	0x00000004,		//	bit shift to right field
	
	kBurgundyVGA_2GainZeroCrossDet	=	0x00000015,		//	VGA_2 Gain Zero Crossing Detection
	kBurgundyVGA_2LAssign	=	0x0000000F,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_2RAssign	=	0x000000F0,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	kBurgundyVGA_2RbitShift	=	0x00000004,		//	bit shift to right field
	
	kBurgundyVGA_3GainZeroCrossDet	=	0x00000016,		//	VGA_3 Gain Zero Crossing Detection
	kBurgundyVGA_3MAssign	=	0x0000000F,		//	%0000 = minimum Gain (0 dB), %1111 = Variable Gain Max Gain (+22.5 dB)
	
	kBurgundyPort9Config	=	0x00000017,		//	Port 9 Configuration
	
	kBurgundyParallelInputState=	0x00000018,		//	Parallel Input logic state monitor register
	kBurgundyParallelMask	=	0x00000001,		//	0 = parallel_in transition will not set kBurgundyParallel_inStatus,
													//	1 = parallel_in transition will set kBurgundyParallel_inStatus.
	kBurgundyParallel_in_1state=	0x00000002,		//	0 = parallel_in_1 is low, 1 = parallel_in_1 is high.
	kBurgundyParallel_in_2state=	0x00000004,		//	0 = parallel_in_2 is low, 1 = parallel_in_2 is high.
	kBurgundyParallel_in_3state=	0x00000008,		//	0 = parallel_in_3 is low, 1 = parallel_in_3 is high.
	kBurgundyIn_sense_0		=	0x00000010,		//	0 = In_sense_0 is low, 1 = In_sense_0 is high.
	kBurgundyIn_sense_1		=	0x00000020,		//	0 = In_sense_1 is low, 1 = In_sense_1 is high.
	kBurgundyIn_sense_2		=	0x00000040,		//	0 = In_sense_2 is low, 1 = In_sense_2 is high.
	kBurgundyIn_sense_3		=	0x00000080,		//	0 = In_sense_3 is low, 1 = In_sense_3 is high.
	
	kBurgundyAdc1Adc2Status	=	0x00000019,		//	A/D 1, 2 Status (these bits remain set until read)
	kBurgundyAD_0L_OverRange=	0x00000001,		//	1 = A/D_0L over range
	kBurgundyAD_0R_OverRange=	0x00000002,		//	1 = A/D_0R over range
	kBurgundyAD_1L_OverRange=	0x00000004,		//	1 = A/D_1L over range
	kBurgundyAD_1R_OverRange=	0x00000008,		//	1 = A/D_1R over range
	kBurgundyAD_2L_OverRange=	0x00000010,		//	1 = A/D_2L over range
	kBurgundyAD_2R_OverRange=	0x00000020,		//	1 = A/D_2R over range
	kBurgundyAD_3M_OverRange=	0x00000040,		//	1 = A/D_3M over range
	
	kBurgundyMaskAD_0L_OverRange	=	0x00000100,		//	Mask A/D_0L over range if 0.
	kBurgundyMaskAD_0R_OverRange	=	0x00000200,		//	Mask A/D_0R over range if 0.
	kBurgundyMaskAD_1L_OverRange	=	0x00000400,		//	Mask A/D_1L over range if 0.
	kBurgundyMaskAD_1R_OverRange	=	0x00000800,		//	Mask A/D_1R over range if 0.
	kBurgundyMaskAD_2L_OverRange	=	0x00001000,		//	Mask A/D_2L over range if 0.
	kBurgundyMaskAD_2R_OverRange	=	0x00002000,		//	Mask A/D_2R over range if 0.
	kBurgundyMaskAD_3M_OverRange	=	0x00004000,		//	Mask A/D_3M over range if 0.
	kBurgundyMaskAD_OverRange=	0x00007F00,		//	Mask A/D over range if 0.
	
	kBurgundyPort0GainAndPan=	0x00000020,		//	GAS_0L, PAS_0L, GAS_0R, PAS_0R assignment
	kBurgundyGAS_0LShift	=	0,				//	shift to access GAS_0L (bits 7-0)
	kBurgundyPAS_0LShift	=	8,				//	shift to access PAS_0L (bits 15-8)
	kBurgundyGAS_0RShift	=	16,				//	shift to access GAS_0R (bits 23-16)
	kBurgundyPAS_0RShift	=	24,				//	shift to access PAS_0R (bits 31-24)
	kNO_PAN_MASK			=	0x00FF00FF,		//	
	kNO_GAIN_MASK			=	0xFF00FF00,		//
	
	kBurgundyPort1GainAndPan=	0x00000021,		//	GAS_1L, PAS_1L, GAS_1R, PAS_1R assignment
	kBurgundyGAS_1LShift	=	0,				//	shift to access GAS_1L (bits 7-0)
	kBurgundyPAS_1LShift	=	8,				//	shift to access PAS_1L (bits 15-8)
	kBurgundyGAS_1RShift	=	16,				//	shift to access GAS_1R (bits 23-16)
	kBurgundyPAS_1RShift	=	24,				//	shift to access PAS_1R (bits 31-24)
	
	kBurgundyPort2GainAndPan=	0x00000022,		//	GAS_2L, PAS_2L, GAS_2R, PAS_2R assignment
	kBurgundyGAS_2LShift	=	0,				//	shift to access GAS_2L (bits 7-0)
	kBurgundyPAS_2LShift	=	8,				//	shift to access PAS_2L (bits 15-8)
	kBurgundyGAS_2RShift	=	16,				//	shift to access GAS_2R (bits 23-16)
	kBurgundyPAS_2RShift	=	24,				//	shift to access PAS_2R (bits 31-24)
	
	kBurgundyPort3_4Gain	=	0x00000023,		//	GAS_3L, GAS_3R, GAS_4L, GAS_4R assignment
	kBurgundyGAS_3LShift	=	0,				//	shift to access GAS_3L (bits 7-0)
	kBurgundyGAS_3RShift	=	8,				//	shift to access PAS_3R (bits 15-8)
	kBurgundyGAS_4LShift	=	16,				//	shift to access GAS_4L (bits 23-16)
	kBurgundyGAS_4RShift	=	24,				//	shift to access PAS_4R (bits 31-24)
	kBurgundyGAS_3MASK		=	0x0000FFFF,		//	mask to retain telephony input gain
	kBurgundyGAS_4MASK		=	0xFFFF0000,		//	mask to retain digital input gain
	
	kBurgundyGAS_A_GAS_B	=	0x00000025,		//	Weighting value for AS_AL (bits 7-0)
													//	Weighting value for AS_AR (bits 15-8)
													//	Weighting value for AS_BL (bits 23-9)
													//	Weighting value for AS_BR (bits 31-24)
	
	kBurgundyGAS_C_GAS_D	=	0x00000026,		//	Weighting value for AS_CL (bits 7-0)
													//	Weighting value for AS_CR (bits 15-8)
													//	Weighting value for AS_DL (bits 23-9)
													//	Weighting value for AS_DR (bits 31-24)
	
	kBurgundyGAS_E_GAS_F	=	0x00000027,		//	Weighting value for AS_EL (bits 7-0)
													//	Weighting value for AS_ER (bits 15-8)
													//	Weighting value for AS_FL (bits 23-9)
													//	Weighting value for AS_FR (bits 31-24)
	
	kBurgundyGAS_G_GAS_H	=	0x00000028,		//	Weighting value for AS_GL (bits 7-0)
													//	Weighting value for AS_GR (bits 15-8)
													//	Weighting value for AS_HL (bits 23-9)
													//	Weighting value for AS_HR (bits 31-24)
	
	kBurgundyW_0L			=	0x00000001,		//	W_0_0L
	kBurgundyW_1L			=	0x00000002,		//	W_1_0L
	kBurgundyW_2L			=	0x00000004,		//	W_2_0L
	kBurgundyW_3L			=	0x00000008,		//	W_3_0L
	kBurgundyW_4L			=	0x00000010,		//	W_4_0L
	kBurgundyW_AL			=	0x00000100,		//	W_A_0L
	kBurgundyW_BL			=	0x00000200,		//	W_B_0L
	kBurgundyW_CL			=	0x00000400,		//	W_C_0L
	kBurgundyW_DL			=	0x00000800,		//	W_D_0L
	kBurgundyW_EL			=	0x00001000,		//	W_E_0L
	kBurgundyW_FL			=	0x00002000,		//	W_F_0L
	kBurgundyW_GL			=	0x00004000,		//	W_G_0L
	kBurgundyW_HL			=	0x00008000,		//	W_H_0L
	kBurgundyW_0R			=	0x00010000,		//	W_0_0L
	kBurgundyW_1R			=	0x00020000,		//	W_1_0L
	kBurgundyW_2R			=	0x00040000,		//	W_2_0L
	kBurgundyW_3R			=	0x00080000,		//	W_3_0L
	kBurgundyW_4R			=	0x00100000,		//	W_4_0L
	kBurgundyW_AR			=	0x01000000,		//	W_A_0L
	kBurgundyW_BR			=	0x02000000,		//	W_B_0L
	kBurgundyW_CR			=	0x04000000,		//	W_C_0L
	kBurgundyW_DR			=	0x08000000,		//	W_D_0L
	kBurgundyW_ER			=	0x10000000,		//	W_E_0L
	kBurgundyW_FR			=	0x20000000,		//	W_F_0L
	kBurgundyW_GR			=	0x40000000,		//	W_G_0L
	kBurgundyW_HR			=	0x80000000,		//	W_H_0L
	kBurgundyW_ASInputMask	=   0x001F001F,		//  Mask for inputs coming thru AS block

	kBurgundyMixer0MixSelect=	0x00000029,		//	Assign MX_0L, MX_0R sum terms.
	kBurgundyMixer1MixSelect=	0x0000002A,		//	Assign MX_1L, MX_1R sum terms.
	kBurgundyMixer2MixSelect=	0x0000002B,		//	Assign MX_2L, MX_2R sum terms.
	kBurgundyMixer3MixSelect=	0x0000002C,		//	Assign MX_3L, MX_3R sum terms.
	
	kBurgundyW_NONE			=	0x00000000,		//	W_0_nL	connect mixer 'n' to NO mux
	kBurgundyW_0_nL			=	0x00000001,		//	W_0_nL	connect mixer 'n' left to mux 0 left
	kBurgundyW_1_nL			=	0x00000002,		//	W_1_nL	connect mixer 'n' left to mux 1 left
	kBurgundyW_2_nL			=	0x00000004,		//	W_2_nL	connect mixer 'n' left to mux 2 left
	kBurgundyW_3_nL			=	0x00000008,		//	W_3_nL	connect mixer 'n' left to mux 3 left
	kBurgundyW_4_nL			=	0x00000010,		//	W_4_nL	connect mixer 'n' left to mux 4 left
	kBurgundyW_A_nL			=	0x00000100,		//	W_A_nL	connect mixer 'n' left to mux A left
	kBurgundyW_B_nL			=	0x00000200,		//	W_B_nL	connect mixer 'n' left to mux B left
	kBurgundyW_C_nL			=	0x00000400,		//	W_C_nL	connect mixer 'n' left to mux C left
	kBurgundyW_D_nL			=	0x00000800,		//	W_D_nL	connect mixer 'n' left to mux D left
	kBurgundyW_E_nL			=	0x00001000,		//	W_E_nL	connect mixer 'n' left to mux E left
	kBurgundyW_F_nL			=	0x00002000,		//	W_F_nL	connect mixer 'n' left to mux F left
	kBurgundyW_G_nL			=	0x00004000,		//	W_G_nL	connect mixer 'n' left to mux G left
	kBurgundyW_H_nL			=	0x00008000,		//	W_H_nL	connect mixer 'n' left to mux H left
	kBurgundyW_0_nR			=	0x00010000,		//	W_0_nL	connect mixer 'n' right to mux 0 right
	kBurgundyW_1_nR			=	0x00020000,		//	W_1_nL	connect mixer 'n' right to mux 1 right
	kBurgundyW_2_nR			=	0x00040000,		//	W_2_nL	connect mixer 'n' right to mux 2 right
	kBurgundyW_3_nR			=	0x00080000,		//	W_3_nL	connect mixer 'n' right to mux 3 right
	kBurgundyW_4_nR			=	0x00100000,		//	W_4_nL	connect mixer 'n' right to mux 4 right
	kBurgundyW_A_nR			=	0x01000000,		//	W_A_nL	connect mixer 'n' right to mux A right
	kBurgundyW_B_nR			=	0x02000000,		//	W_B_nL	connect mixer 'n' right to mux B right
	kBurgundyW_C_nR			=	0x04000000,		//	W_C_nL	connect mixer 'n' right to mux C right
	kBurgundyW_D_nR			=	0x08000000,		//	W_D_nL	connect mixer 'n' right to mux D right
	kBurgundyW_E_nR			=	0x10000000,		//	W_E_nL	connect mixer 'n' right to mux E right
	kBurgundyW_F_nR			=	0x20000000,		//	W_F_nL	connect mixer 'n' right to mux F right
	kBurgundyW_G_nR			=	0x40000000,		//	W_G_nL	connect mixer 'n' right to mux G right
	kBurgundyW_H_nR			=	0x80000000,		//	W_H_nL	connect mixer 'n' right to mux H right
	kBurgundyW_0_n			=	0x00010001,		//	W_0_n	connect mixer 'n' to mux 0 (Ports 1, 2 & 3)
	kBurgundyW_1_n			=	0x00020002,		//	W_1_n	connect mixer 'n' to mux 1 (Ports 4 & 5)
	kBurgundyW_2_n			=	0x00040004,		//	W_2_n	connect mixer 'n' to mux 2 (Ports 6 & 7)
	kBurgundyW_3_n			=	0x00080008,		//	W_3_n	connect mixer 'n' to mux 3 (Port 8: Modem)
	kBurgundyW_4_n			=	0x00100010,		//	W_4_n	connect mixer 'n' to mux 4 (Port 9: PWM)
	kBurgundyW_A_n			=	0x01000100,		//	W_A_n	connect mixer 'n' to digital stream A
	kBurgundyW_B_n			=	0x02000200,		//	W_B_n	connect mixer 'n' to digital stream B
	kBurgundyW_C_n			=	0x04000400,		//	W_C_n	connect mixer 'n' to digital stream C
	kBurgundyW_D_n			=	0x08000800,		//	W_D_n	connect mixer 'n' to digital stream D
	kBurgundyW_E_n			=	0x10001000,		//	W_E_n	connect mixer 'n' to digital stream E
	kBurgundyW_F_n			=	0x20002000,		//	W_F_n	connect mixer 'n' to digital stream F
	kBurgundyW_G_n			=	0x40004000,		//	W_G_n	connect mixer 'n' to digital stream G
	kBurgundyW_H_n			=	0x80008000,		//	W_H_n	connect mixer 'n' to digital stream H
	
	kBurgundyMixer0_1Normalization	=	0x0000002D,		//	Mixer 0,1 Assign MXEQ_0L, MXEQ_0R, MXEQ_1L, MXEQ_1R
	kBurgundyMXEQ_0L		=	0x000000FF,		//	mask for MXEQ_0L weighting value for digital mixer 0L output sum
	kBurgundyMXEQ_0R		=	0x0000FF00,		//	mask for MXEQ_0R weighting value for digital mixer 0R output sum
	kBurgundyMXEQ_1L		=	0x00FF0000,		//	mask for MXEQ_1L weighting value for digital mixer 1L output sum
	kBurgundyMXEQ_1R		=	0xFF000000,		//	mask for MXEQ_1R weighting value for digital mixer 1R output sum
	
	kBurgundyMixer2_3Normalization	=	0x0000002E,		//	Mixer 2,3 Assign MXEQ_2L, MXEQ_2R, MXEQ_3L, MXEQ_3R
	kBurgundyMXEQ_2L		=	0x000000FF,		//	mask for MXEQ_2L weighting value for digital mixer 2L output sum
	kBurgundyMXEQ_2R		=	0x0000FF00,		//	mask for MXEQ_2R weighting value for digital mixer 2R output sum
	kBurgundyMXEQ_3L		=	0x00FF0000,		//	mask for MXEQ_3L weighting value for digital mixer 3L output sum
	kBurgundyMXEQ_3R		=	0xFF000000,		//	mask for MXEQ_3R weighting value for digital mixer 3R output sum
	
	kBurgundyOutSampleSystemSel=	0x0000002F,		//	Assign OS_0 select
	kBurgundyOS_0select		=	0x00000003,		//	mask for output sample 0 select
	kBurgundyOS_0_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_0_MXO_1		=	0x00000001,		//	select MXO_1
	kBurgundyOS_0_MXO_2		=	0x00000002,		//	select MXO_2
	kBurgundyOS_0_MXO_3		=	0x00000003,		//	select MXO_3
	
	kBurgundyOS_1select		=	0x0000000C,		//	mask for output sample 1 select
	kBurgundyOS_1_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_1_MXO_1		=	0x00000004,		//	select MXO_1
	kBurgundyOS_1_MXO_2		=	0x00000008,		//	select MXO_2
	kBurgundyOS_1_MXO_3		=	0x0000000C,		//	select MXO_3
	
	kBurgundyOS_2select		=	0x00000030,		//	mask for output sample 2 select
	kBurgundyOS_2_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_2_MXO_1		=	0x00000010,		//	select MXO_1
	kBurgundyOS_2_MXO_2		=	0x00000020,		//	select MXO_2
	kBurgundyOS_2_MXO_3		=	0x00000030,		//	select MXO_3
	
	kBurgundyOS_3select		=	0x000000C0,		//	mask for output sample 3 select
	kBurgundyOS_3_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_3_MXO_1		=	0x00000040,		//	select MXO_1
	kBurgundyOS_3_MXO_2		=	0x00000080,		//	select MXO_2
	kBurgundyOS_3_MXO_3		=	0x000000C0,		//	select MXO_3
	
	kBurgundyOS_Aselect		=	0x00030000,		//	mask for output sample A select
	kBurgundyOS_A_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_A_MXO_1		=	0x00010000,		//	select MXO_1
	kBurgundyOS_A_MXO_2		=	0x00020000,		//	select MXO_2
	kBurgundyOS_A_MXO_3		=	0x00030000,		//	select MXO_3
	
	kBurgundyOS_Bselect		=	0x000C0000,		//	mask for output sample B select
	kBurgundyOS_B_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_B_MXO_1		=	0x00040000,		//	select MXO_1
	kBurgundyOS_B_MXO_2		=	0x00080000,		//	select MXO_2
	kBurgundyOS_B_MXO_3		=	0x000C0000,		//	select MXO_3
	
	kBurgundyOS_Cselect		=	0x00300000,		//	mask for output sample C select
	kBurgundyOS_C_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_C_MXO_1		=	0x00100000,		//	select MXO_1
	kBurgundyOS_C_MXO_2		=	0x00200000,		//	select MXO_2
	kBurgundyOS_C_MXO_3		=	0x00300000,		//	select MXO_3
	
	kBurgundyOS_Dselect		=	0x00C00000,		//	mask for output sample D select
	kBurgundyOS_D_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_D_MXO_1		=	0x00400000,		//	select MXO_1
	kBurgundyOS_D_MXO_2		=	0x00800000,		//	select MXO_2
	kBurgundyOS_D_MXO_3		=	0x00C00000,		//	select MXO_3
	
	kBurgundyOS_Eselect		=	0x03000000,		//	mask for output sample E select
	kBurgundyOS_E_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_E_MXO_1		=	0x01000000,		//	select MXO_1
	kBurgundyOS_E_MXO_2		=	0x02000000,		//	select MXO_2
	kBurgundyOS_E_MXO_3		=	0x03000000,		//	select MXO_3
	
	kBurgundyOS_Fselect		=	0x0C000000,		//	mask for output sample F select
	kBurgundyOS_F_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_F_MXO_1		=	0x04000000,		//	select MXO_1
	kBurgundyOS_F_MXO_2		=	0x08000000,		//	select MXO_2
	kBurgundyOS_F_MXO_3		=	0x0C000000,		//	select MXO_3
	
	kBurgundyOS_Gselect		=	0x30000000,		//	mask for output sample G select
	kBurgundyOS_G_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_G_MXO_1		=	0x10000000,		//	select MXO_1
	kBurgundyOS_G_MXO_2		=	0x20000000,		//	select MXO_2
	kBurgundyOS_G_MXO_3		=	0x30000000,		//	select MXO_3
	
	kBurgundyOS_Hselect		=	0xC0000000,		//	mask for output sample H select
	kBurgundyOS_H_MXO_0		=	0x00000000,		//	select MXO_0
	kBurgundyOS_H_MXO_1		=	0x40000000,		//	select MXO_1
	kBurgundyOS_H_MXO_2		=	0x80000000,		//	select MXO_2
	kBurgundyOS_H_MXO_3		=	0xC0000000,		//	select MXO_3
	
	kBurgundyOutSamplSystemGain0_1	=	0x00000030,		//	output sample system gain (Gain from OS_mL to AP_mL, OS_mR to AP_mR)
	kBurgundyGAP_0L			=	0x000000FF,		//	GAP_0L field
	kBurgundyGAP_0R			=	0x0000FF00,		//	GAP_0R field
	kBurgundyGAP_1L			=	0x00FF0000,		//	GAP_1L field
	kBurgundyGAP_1R			=	0xFF000000,		//	GAP_1R field
	
	kBurgundyOutSamplSystemGain2_3	=	0x00000031,		//	output sample system gain (Gain from OS_mL to AP_mL, OS_mR to AP_mR)
	kBurgundyGAP_2L			=	0x000000FF,		//	GAP_2L field
	kBurgundyGAP_2R			=	0x0000FF00,		//	GAP_2R field
	kBurgundyGAP_3L			=	0x00FF0000,		//	GAP_3L field
	kBurgundyGAP_3R			=	0xFF000000,		//	GAP_3R field
	
	kBurgundyPkLvlMeter0and1=	0x00000033,		//	Peak Level Meter, entry is 16 MSBs of 18 bit source using Truncation
	kBurgundyPkLvlMeter0	=	0x0000FFFF,		//	Meter 0 Output Level
	kBurgundyPkLvlMeter1	=	0xFFFF0000,		//	Meter 1 Output Level
	
	kBurgundyPkLvlMeter2and3=	0x00000034,		//	Peak Level Meter, entry is 16 MSBs of 18 bit source using Truncation
	kBurgundyPkLvlMeter2	=	0x0000FFFF,		//	Meter 2 Output Level
	kBurgundyPkLvlMeter3	=	0xFFFF0000,		//	Meter 3 Output Level
	
	kBurgundyPkLvlMeter0and1SrcSel	=	0x00000035,		//	Peak Level Meter_0 and Peak Level Meter_1 Source selection
	kBurgundyPkLvlMeter0SrcSel=	0,				//	bit position for peak level meter_0 source selection (8 bits)
	kBurgundyPkLvlMeter1SrcSel=	8,				//	bit position for peak level meter_1 source selection (8 bits)
	kBurgundyPkLvlMeter2SrcSel=	16,				//	bit position for peak level meter_0 source selection (8 bits)
	kBurgundyPkLvlMeter3SrcSel=	24,				//	bit position for peak level meter_1 source selection (8 bits)
	kBurgundySelChPkLvlMeter_0L=	0x00000000,		//	bit 0 = 0 for left channel
	kBurgundySelChPkLvlMeter_0R=	0x00000001,		//	bit 0 = 1 for right channel
	kBurgundySelPkLevelMeter0Src	=	0x0000003E,		//	select peak level meter 0 input left/right field (per bit 0)
	kBurgundySelAudioSourceAS_0X	=	0x00000000,		//	selects audio source AS_0X
	kBurgundySelAudioSourceAS_1X	=	0x00000002,		//	selects audio source AS_1X
	kBurgundySelAudioSourceAS_2X	=	0x00000004,		//	selects audio source AS_2X
	kBurgundySelAudioSourceAS_3X	=	0x00000006,		//	selects audio source AS_3X
	kBurgundySelAudioSourceAS_4X	=	0x00000008,		//	selects audio source AS_4X
	kBurgundySelIntSystemIS_0X=	0x00000010,		//	selects internal system IS_0X
	kBurgundySelIntSystemIS_1X=	0x00000012,		//	selects internal system IS_1X
	kBurgundySelIntSystemIS_2X=	0x00000014,		//	selects internal system IS_2X
	kBurgundySelIntSystemIS_3X=	0x00000016,		//	selects internal system IS_3X
	kBurgundySelIntSystemIS_4X=	0x00000018,		//	selects internal system IS_4X
	kBurgundySelIntSystemIS_AX=	0x00000020,		//	selects internal system IS_AX
	kBurgundySelIntSystemIS_BX=	0x00000022,		//	selects internal system IS_BX
	kBurgundySelIntSystemIS_CX=	0x00000024,		//	selects internal system IS_CX
	kBurgundySelIntSystemIS_DX=	0x00000026,		//	selects internal system IS_DX
	kBurgundySelIntSystemIS_EX=	0x00000028,		//	selects internal system IS_EX
	kBurgundySelIntSystemIS_FX=	0x0000002A,		//	selects internal system IS_FX
	kBurgundySelIntSystemIS_GX=	0x0000002C,		//	selects internal system IS_GX
	kBurgundySelIntSystemIS_HX=	0x0000002E,		//	selects internal system IS_HX
	kBurgundySelMixOutMXO_0X=	0x00000030,		//	selects mixer output MX0_0X
	kBurgundySelMixOutMXO_1X=	0x00000031,		//	selects mixer output MX0_1X
	kBurgundySelMixOutMXO_2X=	0x00000032,		//	selects mixer output MX0_2X
	kBurgundySelMixOutMXO_3X=	0x00000033,		//	selects mixer output MX0_3X
	kBurgundyPkLvlMeterOperates=	0x00000040,		//	0 = hold peak level meter at zero, 1 = peak level meter_0 operates
	
	kBurgundyPkLvlMeterThreshold	=	0x00000036,		//	Peak Level Meter Threshold Register
	kBurgundyPkLvlThreshold	=	0x0000FFFF,		//	threshold level.  2's complement positive short (i.e. 0x0000 ² entry ² 0x7FFF)
	
	kBurgundyOverflowStatus	=	0x00000037,		//	overflow status (read only registers)
	kBurgundyInteranSystemStatus	=	0x0000001F,		//	internal system status field
	kBurgundyIS_0overflow	=	0x00000001,		//	1 = IS_0L or IS_0R has an overflow status
	kBurgundyIS_1overflow	=	0x00000002,		//	1 = IS_1L or IS_1R has an overflow status
	kBurgundyIS_2overflow	=	0x00000004,		//	1 = IS_2L or IS_2R has an overflow status
	kBurgundyIS_3overflow	=	0x00000008,		//	1 = IS_3L or IS_3R has an overflow status
	kBurgundyIS_4overflow	=	0x00000010,		//	1 = IS_4L or IS_4R has an overflow status
	kBurgundyIS_nMask		=	0x00000080,		//	0 = mask IS_0 through IS_4
	kBurgundyMixer0Loverflow=	0x00000100,		//	1 = digital mixer 0 left has an output gain overflow
	kBurgundyMixer0Roverflow=	0x00000200,		//	1 = digital mixer 0 right has an output gain overflow
	kBurgundyMixer1Loverflow=	0x00000400,		//	1 = digital mixer 1 left has an output gain overflow
	kBurgundyMixer1Roverflow=	0x00000800,		//	1 = digital mixer 1 right has an output gain overflow
	kBurgundyMixer2Loverflow=	0x00001000,		//	1 = digital mixer 2 left has an output gain overflow
	kBurgundyMixer2Roverflow=	0x00002000,		//	1 = digital mixer 2 right has an output gain overflow
	kBurgundyMixer3Loverflow=	0x00004000,		//	1 = digital mixer 3 left has an output gain overflow
	kBurgundyMixer3Roverflow=	0x00008000,		//	1 = digital mixer 3 right has an output gain overflow
	kBurgundyMixer0LoverflowMask	=	0x00010000,		//	0 = mask digital mixer 0 left overflow status
	kBurgundyMixer0RoverflowMask	=	0x00020000,		//	0 = mask digital mixer 0 right overflow status
	kBurgundyMixer1LoverflowMask	=	0x00040000,		//	0 = mask digital mixer 1 left overflow status
	kBurgundyMixer1RoverflowMask	=	0x00080000,		//	0 = mask digital mixer 1 right overflow status
	kBurgundyMixer2LoverflowMask	=	0x00100000,		//	0 = mask digital mixer 2 left overflow status
	kBurgundyMixer2RoverflowMask	=	0x00200000,		//	0 = mask digital mixer 2 right overflow status
	kBurgundyMixer3LoverflowMask	=	0x00400000,		//	0 = mask digital mixer 3 left overflow status
	kBurgundyMixer3RoverflowMask	=	0x00800000,		//	0 = mask digital mixer 3 right overflow status
	kBurgundyIS_MixerOvMask	=	0x00FF0000,		//	0 = mask IS & digital mixer overflow status
	
	kBurgundy_SOS0_b0		=	0x00000040,		//	SOS S0 coefficient b0 (bits 23-00)
	kBurgundy_SOS0_b1		=	0x00000041,		//	SOS S0 coefficient b1 (bits 23-00)
	kBurgundy_SOS0_b2		=	0x00000042,		//	SOS S0 coefficient b2 (bits 23-00)
	kBurgundy_SOS0_a1		=	0x00000043,		//	SOS S0 coefficient a1 (bits 23-00)
	kBurgundy_SOS0_a2		=	0x00000044,		//	SOS S0 coefficient a2 (bits 23-00)

	kBurgundy_SOS1_b0		=	0x00000045,		//	SOS S1 coefficient b0 (bits 23-00)
	kBurgundy_SOS1_b1		=	0x00000046,		//	SOS S1 coefficient b1 (bits 23-00)
	kBurgundy_SOS1_b2		=	0x00000047,		//	SOS S1 coefficient b2 (bits 23-00)
	kBurgundy_SOS1_a1		=	0x00000048,		//	SOS S1 coefficient a1 (bits 23-00)
	kBurgundy_SOS1_a2		=	0x00000049,		//	SOS S1 coefficient a2 (bits 23-00)

	kBurgundy_SOS2_b0		=	0x0000004A,		//	SOS S2 coefficient b0 (bits 23-00)
	kBurgundy_SOS2_b1		=	0x0000004B,		//	SOS S2 coefficient b1 (bits 23-00)
	kBurgundy_SOS2_b2		=	0x0000004C,		//	SOS S2 coefficient b2 (bits 23-00)
	kBurgundy_SOS2_a1		=	0x0000004D,		//	SOS S2 coefficient a1 (bits 23-00)
	kBurgundy_SOS2_a2		=	0x0000004E,		//	SOS S2 coefficient a2 (bits 23-00)

	kBurgundy_SOS3_b0		=	0x0000004F,		//	SOS S3 coefficient b0 (bits 23-00)
	
	kBurgundy_SOS3_b1		=	0x00000050,		//	SOS S3 coefficient b1 (bits 23-00)
	kBurgundy_SOS3_b2		=	0x00000051,		//	SOS S3 coefficient b2 (bits 23-00)
	kBurgundy_SOS3_a1		=	0x00000052,		//	SOS S3 coefficient a1 (bits 23-00)
	kBurgundy_SOS3_a2		=	0x00000053,		//	SOS S3 coefficient a2 (bits 23-00)
	
	kBurgundyToneModeControlReg=	0x00000055,		//	Tone Mode Control Register
	kBurgundyIsToneControl	=	0x00000000,		//	0 = Tone Control
	kBurgundyIsSRSControl	=	0x00000001,		//	1 = Alternate Control
	
	kBurgundySOSoverflowStatus=	0x00000056,		//	SOS Overflow status
	kBurgundySOS_S0_overflow=	0x00000001,		//	1 = SOS S0 overflow
	kBurgundySOS_S1_overflow=	0x00000002,		//	1 = SOS S1 overflow
	kBurgundySOS_S2_overflow=	0x00000004,		//	1 = SOS S2 overflow
	kBurgundySOS_S3_overflow=	0x00000008,		//	1 = SOS S3 overflow
	kBurgundySOS_SnMaskOverflow=	0x00000010,		//	0 = mask SOS Sn overflow
	
	kBurgundyAnalogOutputMute=	0x00000060,		//	Analog Output Step Attenuator Mute
	kBurgundyPort13MonoMute	=	0,				//	bit number Port 13 Mono Mute MS_13M:  0 = mute, 1 = unmute
	kBurgundyPort14LMute	=	1,				//	bit number Port 14 Left Mute MS_13M:  0 = mute, 1 = unmute
	kBurgundyPort14RMute	=	2,				//	bit number Port 14 Right Mute MS_13M: 0 = mute, 1 = unmute
	kBurgundyPort15LMute	=	3,				//	bit number Port 15 Left Mute MS_13M:  0 = mute, 1 = unmute
	kBurgundyPort15RMute	=	4,				//	bit number Port 15 Right Mute MS_13M: 0 = mute, 1 = unmute
	kBurgundyPort16LMute	=	5,				//	bit number Port 16 Left Mute MS_13M:  0 = mute, 1 = unmute
	kBurgundyPort16RMute	=	6,				//	bit number Port 16 Right Mute MS_13M: 0 = mute, 1 = unmute
	kBurgundyPort17MonoMute	=	7,				//	bit number Port 17 Mono Mute MS_13M:  0 = mute, 1 = unmute
	kBurgundyMuteAll		=	0x00000000,		//	mute all ports
	kBurgundyMuteOnState		= 0,
        kBurgundyMuteOffState		= 1,
                
	kBurgundyPort13OutputLvl=	0x00000061,		//	Port 13 output level (0 to - 22.5 dB)
	kBurgundyPort14LeftRightOut=	0x00000062,		//	Port 14 left and right output level (0 to - 22.5 dB)
	kBurgundyPort15LeftRightOut=	0x00000063,		//	Port 15 left and right output level (0 to - 22.5 dB)
	kBurgundyPort16LeftRightOut=	0x00000064,		//	Port 16 left and right output level (0 to - 22.5 dB)
	kBurgundyPort17OutputLvl=	0x00000065,		//	Port 17 output level (0 to - 22.5 dB)
	kBurgundyOutputLvlMask	=	0x0000000F,		//	mask to limit port output level range to 0 through -22.5 dB
	kBurgundyLeftOutputLvlMask=	0x0000000F,		//	mask to limit left port output level range to 0 through -22.5 dB
	kBurgundyRightOutputLvlMask=	0x000000F0,		//	mask to limit right port output level range to 0 through -22.5 dB
	
	kBurgundySettlingTimeReg=	0x00000066,		// Settling time settings for output ports
	kBurgundyPort1SettlingShift 	= 	0x00000000,		// output port 13
	kBurgundyPort2SettlingShift 	= 	0x00000002,		// output port 14
	kBurgundyPort3SettlingShift 	= 	0x00000004,		// output port 15
	kBurgundyPort4SettlingShift 	= 	0x00000006,		// output port 16
	kBurgundyPort5SettlingShift 	= 	0x00000008,		// output port 17
	kBurgundySettlingMask	=	0x00000003,		//		mask for settling time selection
	kBurgundyDefaultSettling=	0x00000000,		//		register default value == 22 µF
	kBurgundySmallSettling	=	0x00000001,		//		01 = 100 µF ² C ² 220 µF
	kBurgundyMediumSettling	=	0x00000002,		//		10 = 220 µF ² C ² 470 µF
	kBurgundyLargeSettling	=	0x00000003,		//		11 = 470 µF
	kBurgundySettlingFieldWidth=	2,				//	
	BurgundyMaxSettlingPort	=	5,				//	number of outputs that discharge rate can be set
	
	kBurgundyOutCtrl_0_1	=	0x00000067,		//	Out_Ctrl0, Out_Ctrl1 Configuration
	kBurgundyOut_Ctrl_0State=	0x00000010,		//	Out_Ctrl_0: 0 = low, 1 = high
	kBurgundyOut_Ctrl_0Tristate=	0x00000020,		//	Out_Ctrl_0 tristate: 1 = tristate
	kBurgundyOut_Ctrl_1State=	0x00000040,		//	Out_Ctrl_1: 0 = low, 1 = high
	kBurgundyOut_Ctrl_1Tristate=	0x00000080,		//	Out_Ctrl_1 tristate: 1 = tristate
	
	kBurgundyOutCtrl_2_3_4	=	0x00000068,		//	Out_Ctrl2, Out_Ctrl3, Out_Ctrl4 Configuration
	kBurgundyOut_Ctrl_2State=	0x00000001,		//	Out_Ctrl_2: 0 = low, 1 = high
	kBurgundyOut_Ctrl_2Tristate=	0x00000002,		//	Out_Ctrl_2 tristate: 1 = tristate
	kBurgundyOut_Ctrl_3State=	0x00000004,		//	Out_Ctrl_3: 0 = low, 1 = high
	kBurgundyOut_Ctrl_3Tristate=	0x00000008,		//	Out_Ctrl_3 tristate: 1 = tristate
	kBurgundyOut_Ctrl_4State=	0x00000010,		//	Out_Ctrl_4: 0 = low, 1 = high
	kBurgundyOut_Ctrl_4Tristate=	0x00000020,		//	Out_Ctrl_4 tristate: 1 = tristate
	
	kBurgundyDOut_12Config	=	0x00000069,		//	DOut_12 Configuration
	kBurgundyDOutMute		=	0x00000080,		//		0 = mute digital output (all zeros), 1 = select digital output
	
	kBurgundyMclk_0FrequencySel=	0x00000070,		//	Mclk_0 Frequency Output Select
	kBurgundyMclk_0HiZ		=	0x00000000,		//		00: clock out = High Z Output
	kBurgundyMclk_0_Mclk_inDiv1=	0x00000001,		//		01: clock out = Mclk_in / 1
	kBurgundyMclk_0_Mclk_inDiv2=	0x00000002,		//		10: clock out = Mclk_in / 2
	kBurgundyMclk_0_Mclk_inDiv4=	0x00000003,		//		11: clock out = Mclk_in / 4
	
	kBurgundyOS_X_AS_X_AtoDConfig	=	0x00000078,		//	OS_X Output Driver Enable, SD_in Line (Host Port) AS_X Input Selection
	kBurgundyOS_AOutputEnable=	0x00000001,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_A to subframe 0 SD_in Line
	kBurgundyAS_AInputSelect=	0x00000002,		//		0 = read AS_A from subframe 0 SD_in line, 1 = read AS_A from 
	kBurgundyOS_BOutputEnable=	0x00000004,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_B to subframe 1 SD_in Line
	kBurgundyAS_BInputSelect=	0x00000008,		//		0 = read AS_B from subframe 1 SD_in line, 1 = read AS_B from 
	kBurgundyOS_COutputEnable=	0x00000010,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_C to subframe 2 SD_in Line
	kBurgundyAS_CInputSelect=	0x00000020,		//		0 = read AS_C from subframe 2 SD_in line, 1 = read AS_C from 
	kBurgundyOS_DOutputEnable=	0x00000040,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_D to subframe 3 SD_in Line
	kBurgundyAS_DInputSelect=	0x00000080,		//		0 = read AS_D from subframe 3 SD_in line, 1 = read AS_D from 
	
	kBurgundyOS_X_AS_X_EtoHConfig	=	0x00000079,		//	OS_X Output Driver Enable, SD_in Line (Host Port) AS_X Input Selection
	kBurgundyOS_EOutputEnable=	0x00000001,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_E to subframe 0 SD_in Line
	kBurgundyAS_EInputSelect=	0x00000002,		//		0 = read AS_E from subframe 0 SD_in line, 1 = read AS_E from 
	kBurgundyOS_FOutputEnable=	0x00000004,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_F to subframe 1 SD_in Line
	kBurgundyAS_FInputSelect=	0x00000008,		//		0 = read AS_F from subframe 1 SD_in line, 1 = read AS_F from 
	kBurgundyOS_GOutputEnable=	0x00000010,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_G to subframe 2 SD_in Line
	kBurgundyAS_GInputSelect=	0x00000020,		//		0 = read AS_G from subframe 2 SD_in line, 1 = read AS_G from 
	kBurgundyOS_HOutputEnable=	0x00000040,		//		0 = trisate subframe 0 SD_in line, 1 = Write OS_H to subframe 3 SD_in Line
	kBurgundyAS_HInputSelect=	0x00000080,		//		0 = read AS_H from subframe 3 SD_in line, 1 = read AS_H from 
	
	kBurgundyThresholdMaskReg=	0x0000007A,		//	Mask bits for threshold detectors
	kBurgundyMASKLevelMeter_0=	0x00000001,		//		MASK Level Meter_0: 0 = exceeds threshold condition, 1 = report condition
	kBurgundyMASKLevelMeter_1=	0x00000002,		//		MASK Level Meter_1: 0 = exceeds threshold condition, 1 = report condition
	kBurgundyMASKLevelMeter_2=	0x00000004,		//		MASK Level Meter_2: 0 = exceeds threshold condition, 1 = report condition
	kBurgundyMASKLevelMeter_3=	0x00000008,		//		MASK Level Meter_3: 0 = exceeds threshold condition, 1 = report condition

	kMaxSndHWRegisters	= 	0x0000007B,		//	Number valid register address in Burgundy (highest address +1)
	
	kVGA_0				=	0,				//	reference constant for input block #0
	kVGA_1				=	1,				//	reference constant for input block #1
	kVGA_2				=	2,				//	reference constant for input block #2
	kVGA_3				=	3,				//	reference constant for input block #3
	
	kBurgundyPhysInputPortIllegal	= -1,				//	physical input #
	kBurgundyPhysInputPortNone= 0,				//	physical input #
	kBurgundyPhysInputPort1	= 1,				//	physical input #
	kBurgundyPhysInputPort2	= 2,				//	physical input #
	kBurgundyPhysInputPort3	= 3,				//	physical input #
	kBurgundyPhysInputPort4	= 4,				//	physical input #
	kBurgundyPhysInputPort5	= 5,				//	physical input #
	kBurgundyPhysInputPort6	= 6,				//	physical input #
	kBurgundyPhysInputPort7	= 7,				//	physical input #
	kBurgundyPhysInputPort8	= 8,				//	physical input #
	kBurgundyPhysInputPort9	= 9,				//	physical input #
	
	kBurgundyPhysOutputPortIllegal	= -1,				//
	kBurgundyPhysOutputPortNone= 0,				//
	kBurgundyPhysOutputPort13= 1,				//	reference constant for physical output block #13
	kBurgundyPhysOutputPort14= 2,				//	reference constant for physical output block #14
	kBurgundyPhysOutputPort15= 3,				//	reference constant for physical output block #15
	kBurgundyPhysOutputPort16= 4,				//	reference constant for physical output block #16
	kBurgundyPhysOutputPort17= 5,				//	reference constant for physical output block #17
	
	kBurgundyRegSizeInvalid	=	-1,				//	register transaction validation size
	kBurgundyRegSize_1_Byte	=	0,				//	register transaction validation size
	kBurgundyRegSize_2_Byte	=	1,				//	register transaction validation size
	kBurgundyRegSize_3_Byte	=	2,				//	register transaction validation size
	kBurgundyRegSize_4_Byte	=	3,				//	register transaction validation size

	kByteShift			=	8,
	kWordShift			=	16,
	kByteMask			=	0x000000FF,

	kBurgundyRegAddrField_Wr=	21,				//	field "A" bit address:	1 = write, 0 = read
	kBurgundyRegAddrField_Reset=	20,				//	field "A" bit address:	1 = reset state machine
	kBurgundyRegAddrField_reg=	12,				//	field "A" bit address:	register address 0 - 7
	kBurgundyCmdRegWrData_nn=	10,				//	field "B" bit address:	last byte to write
	kBurgundyCmdRegWrData_qq=	8,				//	field "B" bit address:	current byte to write
	kBurgundyCmdRegWrData_data=	0,				//	field "B" bit address:	data to write
	
	kBurgundyFirstValid		=	23,				//	field "A" bit report field:	1 = first byte
	kBurgundyReportField_CodecRdy	=	22,				//	field "A" bit report field:	1 = CODEC ready
	kBurgundyReportField_Indicator	=	16,				//	field "A" bit report field:	indicator 0 - 3
	kBurgundyBB				=	14,				//	field "A" bit report field:	
	kBurgundyLL				=	12,				//	field "A" bit report field:	
	kBurgundyData			=	4,				//	field "B" bit read data / status:	data 0 - 7
	kBurgundyConfig			=	0,				//	field "B" bit read data / status:	configuration pins 0 - 3
	
	kBurgundyMaxSndSystem	=	2,				//	maximum number of sound systems
	kBurgundySystem_None	=	0,				//	
	kBurgundySystem_Acoustic=	1,				//	
	kBurgundySystem_Telephony=	2,				//
	kBurgundyMaxInput		=	9,				//	maximum input port number

	kBurgundyMinNormalHardwareGain	=	0,				// 0.5 setting for the software API
	kBurgundyMaxNormalHardwareGain	=	15,				// 1.5 setting for the software API
	kBurgundyPopMaxNormalHardwareGain	=	12,				// 1.5 setting for the software API
	kMinSoundColorizationGain	=	-12,
	kUnitySoundColorizationGain	=	0,
	kMaxSoundColorizationGain	=	12,
	kMinSoftwareGain	=	0x00008000,		// 0.5 setting for the software API
	kMaxSoftwareGain	=	0x00018000,		// 1.5 setting for the software API
	kUnitySoftwareGain	=	0x00010000,		// 1.0 setting for the software API
	kSoftwareGainMask	=	0x0001F000,
	kBurgundyHWdBStepSize	=	0x00018000,
	
	kBurgundyMinVolume		=	0x00008000,
	kBurgundyMaxVolume		=	0x00018000,
	kInvalidGainMask		= 	0xFFFE0000,		// mask to check gain settings against
	kBurgundyInvalidVolumeMask=	0xFF00FF00,
	
	kBurgundy0_375dBGain	=	0x00006000,		//	analog input gain of  0.375 dB
	kBurgundy0_376dBGain	=	0x0000606F,		//	analog input gain of   0.376 dB
	kBurgundy1_5dBGain		=	0x00018000,		//	analog input gain of  1.500 dB
	kBurgundy6_0dBGain		=	0x00060000,		//	analog input gain of  6.000 dB
	kBurgundy12dBGain		=	0x000C0000,		//	analog input gain of 12.000 dB
	kBurgundy18_0dBGain		=	0x00120000,		//	analog input gain of 18.000 dB
	kBurgundy22_5dBGain		=	0x00168000,		//	analog input gain of 22.500 dB
	kBurgundy24dBGain		=	0x00180000,		//	analog input gain of 24.000 dB
	kBurgundy34_5dBGain		=	0x001E0000,		//	analog input gain of 34.500 dB
	kBurgundy46_5dBGain		=	0x002E8000,		//	analog input gain of 46.500 dB
	kBurgundy58_5dBGain		=	0x00360000,		//	analog input gain of  58.500 dB
	kBurgundyNeg24dBGain	=	0xFFE80000,		//	analog input gain of -24.000 dB
	kBurgundyNeg84dBGain	=	0xFFAC0000		//	analog input gain of -84.000 dB
};

enum{
	kBurgundyPortType_NONE	=	0,				//	no input = no gain
	kBurgundyPortType_D		=	1,				//	input port: 0 - 12 dB Digital GAIN only
	kBurgundyPortType_AD	=	2,				//	input port: 0 - 22.5 dB Analog Gain, 0 - 12 dB Digital Gain
	kBurgundyPortType_AAD	=	3				//	input port: 0 - 22.5 dB Analog Gain, 24.0 - 46.5 dB Analog Gain, 0 - 12 dB Digital Gain
};

enum{
	kBurgundyMuxNone		=	0,				//	no mux selected
	kBurgundyMux0			=	1,				//	mux 0: inputs 1, 2 & 3
	kBurgundyMux1			=	2,				//	mux 1: inputs 3, 4 & 5
	kBurgundyMux2			=	3,				//	mux 2: inputs 5, 6 & 7
	kBurgundyMux3			=	4,				//	mux 3: input 8 (no hardware - logical ref)(Telephony)
	kBurgundyMux4			=	5,				//	mux 4: input 9 (no hardware - logical ref)
	kBurgundyMaxMux			=	6				//	number of multiplexer
};

enum{
	kBurgundyStart,										//	state machine position in register read
	kBurgundySendCmd,									//	state machine position in register read
	kBurgundyCmdDone,									//	state machine position in register read
	kBurgundySdOutStart,								//	state machine position in register write
	kBurgundyGetData,									//	state machine position in register read
	kBurgundySdOutDone									//	state machine position in register read
};

enum{
	kBurgundyIC_normal		=	0x0,			//	BURGUNDY INDICATOR CODE: normal, no indicators pending
	kBurgundyIC_ToneCtrlStatus=	0x1,			//	BURGUNDY INDICATOR CODE: Tone Control Status Indicator
	kBurgundyIC_Overflow0	=	0x2,			//	BURGUNDY INDICATOR CODE: Overflow Status Detector 0
	kBurgundyIC_Overflow1	=	0x3,			//	BURGUNDY INDICATOR CODE: Overflow Status Detector 1
	kBurgundyIC_Overflow2	=	0x4,			//	BURGUNDY INDICATOR CODE: Overflow Status Detector 2
	kBurgundyIC_ParallelInChg=	0x6,			//	BURGUNDY INDICATOR CODE: Parallel Input Line Changed
	kBurgundyIC_PeakLvlMeter0=	0xB,			//	BURGUNDY INDICATOR CODE: Peak Level Meter 0 ³ Threshold Level
	kBurgundyIC_PeakLvlMeter1=	0xC,			//	BURGUNDY INDICATOR CODE: Peak Level Meter 1 ³ Threshold Level
	kBurgundyIC_PeakLvlMeter2=	0xD,			//	BURGUNDY INDICATOR CODE: Peak Level Meter 2 ³ Threshold Level
	kBurgundyIC_PeakLvlMeter3=	0xE,			//	BURGUNDY INDICATOR CODE: Peak Level Meter 3 ³ Threshold Level
	kBurgundyIC_Twilight	=	0xF,			//	BURGUNDY INDICATOR CODE: Twilight Completed
	kBurgundyIC_MAX_COUNT	=	16				//	maximum number of indicator codes
};


enum{
	SRS_feedthrough		= 0x0002892C,
	SRS_centerMUTE		= 0x00000000,
	SRS_center16dB		= 0x000404DE,
	SRS_center12dB		= 0x00026BF4,
	SRS_SOS2_b0			= 0x000FF20A,			//	srs "Space" function at 0 dB / 44.1 kHz
	SRS_SOS2_b1			= 0x00F00DF5,			//	srs "Space" function at 0 dB / 44.1 kHz
	SRS_SOS2_b2			= 0x00000000,		//	srs perspective function
	SRS_SOS2_a1			= 0x000FE415,		//	srs perspective function
	SRS_SOS2_a2			= 0x00000000,		//	srs perspective function
	SRS_SOS3_b0			= 0x0013202C,		//	srs perspective function
	SRS_SOS3_b1			= 0x00E0CB9C,		//	srs perspective function
	SRS_SOS3_b2			= 0x000CD3AA,		//	srs perspective function
	SRS_SOS3_a1			= 0x0014E71E,		//	srs perspective function
	SRS_SOS3_a2			= 0x00FACC4C		//	srs perspective function
};

enum{
        kBurgundyInSenseMask			=	0x0000000F,		// mask for status bits
	kBurgundyInSense0			=	0x00000008,		// in sense bit 0
	kBurgundyInSense1			=	0x00000004,		// in sense bit 1
	kBurgundyInSense2			=	0x00000002,		// in sense bit 2
	kBurgundyInSense3			=	0x00000001,		// in sense bit 3
};