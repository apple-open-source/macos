/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

        // Globals



/****************************************************************************************************/
//
//		Function:	AllocateEventLog
//
//		Inputs:		size - amount of memory to allocate
//
//		Outputs:	
//
//		Desc:		Allocates the event log buffer
//
/****************************************************************************************************/

void AllocateEventLog(UInt32 size)
{

    evLogFlag = 0;            					// assume insufficient memory
    evLogBuf = (UInt8*)IOMalloc(size);
    if (!evLogBuf)
    {
        kprintf("AppleRS232Serial evLog allocation failed ");
        return;
    }

    bzero(evLogBuf, size);
    evLogBufp = evLogBuf;
    evLogBufe = evLogBufp + kEvLogSize - 0x20; 			// ??? overran buffer?
    evLogFlag = 0xFEEDBEEF;					// continuous wraparound
//	evLogFlag = 'step';					// stop at each ELG
//	evLogFlag = 0x0333;					// any nonzero - don't wrap - stop logging at buffer end

    IOLog("AllocateEventLog - buffer=%8x", (unsigned int)evLogBuf);

    return;
	
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//		Function:	EvLog
//
//		Inputs:		a - anything
//				b - anything
//				ascii - 4 charater tag
//				str - any info string			
//
//		Outputs:	
//
//		Desc:		Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

void EvLog(UInt32 a, UInt32 b, UInt32 ascii, char* str)
{
    register UInt32	*lp;
    mach_timespec_t	time;

    if (evLogFlag == 0)
        return;

    IOGetTime(&time);

    lp = (UInt32*)evLogBufp;
    evLogBufp += 0x10;

    if (evLogBufp >= evLogBufe)       		// handle buffer wrap around if any
    {    
        evLogBufp  = evLogBuf;
        if (evLogFlag != 0xFEEDBEEF)   			 // make 0xFEEDBEEF a symbolic ???
            evLogFlag = 0;               		 // stop tracing if wrap undesired
    }

        // compose interrupt level with 3 byte time stamp:

    *lp++ = (intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp = ascii;

    if(evLogFlag == 'step')
    {	
        static char	code[ 5 ] = {0,0,0,0,0};
        *(UInt32*)&code = ascii;
        IOLog( "%8x AppleRS232Serial: %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
    }

    return;
	
}/* end EvLog */

#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	Asciify
//
//		Inputs:		i - the nibble
//
//		Outputs:	return byte - ascii byte
//
//		Desc:		Converts to ascii. 
//
/****************************************************************************************************/
 
UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if ( i < 10 )
        return( '0' + i );
    else return( 55  + i );
	
}/* end Asciify */

/****************************************************************************************************/
//
//		Function:	SerialLogData
//
//		Inputs:		Dir - direction
//				Count - number of bytes
//				buf - the data
//
//		Outputs:	
//
//		Desc:		Puts the data in the log. 
//
/****************************************************************************************************/

void SerialLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8	wlen, i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];

    for ( i=0; i<=buflen; i++ )
    {
        LocBuf[i] = 0x20;
    }
    LocBuf[i] = 0x00;
	
    if ( Dir == kSerialIn )
    {
        IOLog( "AppleRS232Serial: SerialLogData - Read Complete, size = %8x\n", Count );
    } else {
        if ( Dir == kSerialOut )
        {
            IOLog( "AppleRS232Serial: SerialLogData - Write, size = %8x\n", Count );
        } else {
            if ( Dir == kSerialOther )
            {
                IOLog( "AppleRS232Serial: SerialLogData - Other, size = %8x\n", Count );
            }
        }			
    }

    if ( Count > dumplen )
    {
        wlen = dumplen;
    } else {
        wlen = Count;
    }
	
    if ( wlen > 0 )
    {
        Aspnt = Asciistart;
        Hxpnt = 0;
        for ( i=1; i<=wlen; i++ )
        {
            wchr = buf[i-1];
            LocBuf[Hxpnt++] = Asciify( wchr >> 4 );
            LocBuf[Hxpnt++] = Asciify( wchr );
            if (( wchr < 0x20) || (wchr > 0x7F )) 		// Non printable characters
            {
                LocBuf[Aspnt++] = 0x2E;				// Replace with a period
            } else {
                LocBuf[Aspnt++] = wchr;
            }
        }
        LocBuf[(wlen + Asciistart) + 1] = 0x00;
        IOLog( LocBuf );
        IOLog( "\n" );
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
    } else {
        IOLog( "AppleRS232Serial: SerialLogData - No data, Count=0\n" );
    }
	
}/* end SerialLogData */
