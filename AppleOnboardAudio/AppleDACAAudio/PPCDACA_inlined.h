/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * INLINEd functions for the DAC 3550A audio Controller these
 * methods are all private, so I like them better in this separate
 * header rather than in the main header where the interface
 * definition lays.
 *
 * HISTORY
 *
 */

#ifndef _PPCDACA_INLINED_H
#define _PPCDACA_INLINED_H

#include "PPCDACA.h"

// In debug mode we may wish to step trough the INLINEd methods, so:
#ifdef DEBUGMODE
#define INLINE
#else
#define INLINE	inline
#endif

// Generic INLINEd methods to access to registers:
// ===============================================
INLINE UInt32
PPCDACA::ReadWordLittleEndian(void *address, UInt32 offset)
{
#if 0
    UInt32 *realAddress = (UInt32*)(address) + offset;
    UInt32 value = *realAddress;
    UInt32 newValue =
        ((value & 0x000000FF) << 16) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0xFF000000) >> 16);

    return (newValue);
#else
    return OSReadLittleInt32(address, offset);
#endif
}

INLINE void
PPCDACA::WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value)
{
#if 0
    UInt32 *realAddress = (UInt32*)(address) + offset;
    UInt32 newValue =
        ((value & 0x000000FF) << 16) |
        ((value & 0x0000FF00) << 8) |
        ((value & 0x00FF0000) >> 8) |
        ((value & 0xFF000000) >> 16);

    *realAddress = newValue;
#else
    OSWriteLittleInt32(address, offset, value);
#endif    
}

// INLINEd methods to access to all the I2S registers:
// ===================================================
INLINE UInt32
PPCDACA::I2SGetIntCtlReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SIntCtlOffset);
}

INLINE void
PPCDACA::I2SSetIntCtlReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SIntCtlOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetSerialFormatReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SSerialFormatOffset);
}

INLINE void
PPCDACA::I2SSetSerialFormatReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SSerialFormatOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetCodecMsgOutReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SCodecMsgOutOffset);
}

INLINE void
PPCDACA::I2SSetCodecMsgOutReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SCodecMsgOutOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetCodecMsgInReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SCodecMsgInOffset);
}

INLINE void
PPCDACA::I2SSetCodecMsgInReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SCodecMsgInOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetFrameCountReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SFrameCountOffset);
}

INLINE void
PPCDACA::I2SSetFrameCountReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SFrameCountOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetFrameMatchReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset);
}

INLINE void
PPCDACA::I2SSetFrameMatchReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetDataWordSizesReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SDataWordSizesOffset);
}

INLINE void
PPCDACA::I2SSetDataWordSizesReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SDataWordSizesOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetPeakLevelSelReg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SPeakLevelSelOffset);
}

INLINE void
PPCDACA::I2SSetPeakLevelSelReg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SPeakLevelSelOffset, value);
}

INLINE UInt32
PPCDACA::I2SGetPeakLevelIn0Reg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SPeakLevelIn0Offset);
}

INLINE void
PPCDACA::I2SSetPeakLevelIn0Reg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SPeakLevelIn0Offset, value);
}

INLINE UInt32
PPCDACA::I2SGetPeakLevelIn1Reg()
{
    return ReadWordLittleEndian(soundConfigSpace, kI2SPeakLevelIn1Offset);
}

INLINE void
PPCDACA::I2SSetPeakLevelIn1Reg(UInt32 value)
{
    WriteWordLittleEndian(soundConfigSpace, kI2SPeakLevelIn1Offset, value);
}

INLINE UInt32
PPCDACA::I2SCounterReg()
{
    return ((UInt32)(soundConfigSpace) + kI2SFrameCountOffset);
}

// Access to Keylargo registers:
INLINE void
PPCDACA::KLSetRegister(void *klRegister, UInt32 value)
{
    UInt32 *reg = (UInt32*)klRegister;
    *reg = value;
}

INLINE UInt32
PPCDACA::KLGetRegister(void *klRegister)
{
    UInt32 *reg = (UInt32*)klRegister;
    return (*reg);
}


#endif // _PPCDACA_INLINED_H
