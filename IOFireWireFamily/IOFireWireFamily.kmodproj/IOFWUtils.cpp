/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 3 June 99 wgulland created.
 *
 * Useful stuff called from several different FireWire objects.
 */
 
// public
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOFWUtils.h>

// system
#import <IOKit/assert.h>
#import <IOKit/IOLib.h>

////////////////////////////////////////////////////////////////////////////////
//
// FWUpdateCRC16
//
//   This proc updates a crc with the next quad.
//

UInt16 FWUpdateCRC16(UInt16 crc16, UInt32 quad)
{
	UInt32 host_quad = OSSwapBigToHostInt32( quad );
    SInt32 shift;
    UInt32 sum;
    UInt32 crc = crc16;
    for (shift = 28; shift >= 0; shift -= 4) {
        sum = ((crc >> 12) ^ (host_quad >> shift)) & 0x0F;
        crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ (sum);
    }
    return (crc & 0xFFFF);
}
////////////////////////////////////////////////////////////////////////////////
//
// FWComputeCRC16
//
//   This proc computes a CRC 16 check.
//

UInt16	FWComputeCRC16(const UInt32 *pQuads, UInt32 numQuads)
{
    SInt32	shift;
    UInt32	sum;
    UInt32	crc16;
    UInt32	quadNum;
    UInt32	quad;

    // Compute CRC 16 over all quads.
    crc16 = 0;
    for (quadNum = 0; quadNum < numQuads; quadNum++) {
        quad = OSSwapBigToHostInt32(*pQuads++);
        for (shift = 28; shift >= 0; shift -= 4) {
            sum = ((crc16 >> 12) ^ (quad >> shift)) & 0x0F;
            crc16 = (crc16 << 4) ^ (sum << 12) ^ (sum << 5) ^ (sum);
        }
    }

    return (crc16 & 0xFFFF);
}

UInt32  AddFWCycleTimeToFWCycleTime( UInt32 cycleTime1, UInt32 cycleTime2 )
{
    UInt32    secondCount,
              cycleCount,
              cycleOffset;
    UInt32    cycleTime;

    // Add cycle offsets.
    cycleOffset = (cycleTime1 & 0x0FFF) + (cycleTime2 & 0x0FFF);

    // Add cycle counts.
    cycleCount = (cycleTime1 & 0x01FFF000) + (cycleTime2 & 0x01FFF000);

    // Add any carry over from cycle offset to cycle count.
    if (cycleOffset > 3071)
    {
        cycleCount += 0x1000;
        cycleOffset -= 3072;
    }

    // Add secondCounts.
    secondCount = (cycleTime1 & 0xFE000000) + (cycleTime2 & 0xFE000000);

    // Add any carry over from cycle count to secondCount.
    if (cycleCount > (7999 << 12))
    {
        secondCount += 0x02000000;
        cycleCount -= (8000 << 12);
    }

    // Put everything together into cycle time.
    cycleTime = secondCount | cycleCount | cycleOffset;

    return (cycleTime);
}

UInt32 SubtractFWCycleTimeFromFWCycleTime( UInt32 cycleTime1, UInt32 cycleTime2)
{
    SInt32 secondCount,
           cycleCount,
           cycleOffset;
    UInt32 cycleTime;

    // Subtract cycle offsets.
    cycleOffset = (cycleTime1 & 0x0FFF) - (cycleTime2 & 0x0FFF);

    // Subtract cycle counts.
    cycleCount = (cycleTime1 & 0x01FFF000) - (cycleTime2 & 0x01FFF000);

    // Subtract any borrow over from cycle offset to cycle count.

    if (cycleOffset < 0)
    {
        cycleCount -= 0x1000;
        cycleOffset += 3072;
    }

    // Subtract secondCounts.
    secondCount = (cycleTime1 & 0xFE000000) - (cycleTime2 & 0xFE000000);

    // Subtract any borrow over from cycle count to secondCount.
    if (cycleCount < 0)
    {
        secondCount -= 0x02000000;
        cycleCount += (8000 << 12);
    }

    // Put everything together into cycle time.
    cycleTime = secondCount | cycleCount | cycleOffset;

    return (cycleTime);
}

// findOffsetInRanges:
// takes a pointer and a list of ranges, and finds the offset of the pointer into 
// the range array
bool
findOffsetInRanges ( mach_vm_address_t address, unsigned rangeCount, IOAddressRange ranges[], IOByteCount & outOffset )
{
	UInt32			index			= 0 ;
	IOByteCount		distanceInRange ;
	bool			found			= false ;

	outOffset = 0 ;
	while ( ! found && index < rangeCount )
	{
		distanceInRange = address - ranges[index].address ;
		if ( found = ( distanceInRange < ranges[ index ].length ) )
			outOffset += distanceInRange ;
		else
			outOffset += ranges[ index ].length ;
		
		++index ;
	}
	
	return found ;
}

void
IOFWGetAbsoluteTime( AbsoluteTime * result )
{
	*((uint64_t*)result) = mach_absolute_time();
}
