/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 
/*!
 *  @header nbputilities
 */
 
#include "nbputilities.h"

unsigned short gCompareTable[] = {

        /* 0 */	0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00,
        /* 1 */	0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700, 0x1800, 0x1900, 0x1A00, 0x1B00, 0x1C00, 0x1D00, 0x1E00, 0x1F00,
        /* 2 */	0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x2A00, 0x2B00, 0x2C00, 0x2D00, 0x2E00, 0x2F00,
        /* 3 */	0x3000, 0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3A00, 0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00,
        /* 4 */	0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700, 0x4800, 0x4900, 0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00,
        /* 5 */	0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5A00, 0x5B00, 0x5C00, 0x5D00, 0x5E00, 0x5F00,

        // 0x60 maps to 'a'
        // range 0x61 to 0x7a ('a' to 'z') map to upper case

        /* 6 */	0x4180, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700, 0x4800, 0x4900, 0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00,
        /* 7 */	0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5A00, 0x7B00, 0x7C00, 0x7D00, 0x7E00, 0x7F00,

        // range 0x80 to 0xd8 gets mapped...

        /* 8 */	0x4108, 0x410C, 0x4310, 0x4502, 0x4E0A, 0x4F08, 0x5508, 0x4182, 0x4104, 0x4186, 0x4108, 0x410A, 0x410C, 0x4310, 0x4502, 0x4584,
        /* 9 */	0x4586, 0x4588, 0x4982, 0x4984, 0x4986, 0x4988, 0x4E0A, 0x4F82, 0x4F84, 0x4F86, 0x4F08, 0x4F0A, 0x5582, 0x5584, 0x5586, 0x5508,
        /* A */	0xA000, 0xA100, 0xA200, 0xA300, 0xA400, 0xA500, 0xA600, 0x5382, 0xA800, 0xA900, 0xAA00, 0xAB00, 0xAC00, 0xAD00, 0x4114, 0x4F0E,
        /* B */	0xB000, 0xB100, 0xB200, 0xB300, 0xB400, 0xB500, 0xB600, 0xB700, 0xB800, 0xB900, 0xBA00, 0x4192, 0x4F92, 0xBD00, 0x4114, 0x4F0E,
        /* C */	0xC000, 0xC100, 0xC200, 0xC300, 0xC400, 0xC500, 0xC600, 0x2206, 0x2208, 0xC900, 0x2000, 0x4104, 0x410A, 0x4F0A, 0x4F14, 0x4F14,
        /* D */	0xD000, 0xD100, 0x2202, 0x2204, 0x2702, 0x2704, 0xD600, 0xD700, 0x5988, 0xD900, 0xDA00, 0xDB00, 0xDC00, 0xDD00, 0xDE00, 0xDF00,

        /* E */	0xE000, 0xE100, 0xE200, 0xE300, 0xE400, 0xE500, 0xE600, 0xE700, 0xE800, 0xE900, 0xEA00, 0xEB00, 0xEC00, 0xED00, 0xEE00, 0xEF00,
        /* F */	0xF000, 0xF100, 0xF200, 0xF300, 0xF400, 0xF500, 0xF600, 0xF700, 0xF800, 0xF900, 0xFA00, 0xFB00, 0xFC00, 0xFD00, 0xFE00, 0xFF00,

        };


int myFastRelString ( const unsigned char* str1, int length, const unsigned char* str2, int length2 )
{
	UInt16*			compareTable;
	SInt32	 		bestGuess;

	if (length == length2)
			bestGuess = 0;
	else if (length < length2)
			bestGuess = -1;
	else
	{
			bestGuess = 1;
			length = length2;
	}

	compareTable = (UInt16*) gCompareTable;

	while (length--)
	{
			UInt8	aChar, bChar;

			aChar = *(str1++);
			bChar = *(str2++);

			if (aChar != bChar)		//	If they don't match exacly, do case conversion
			{
					UInt16	aSortWord, bSortWord;

					aSortWord = compareTable[aChar];
					bSortWord = compareTable[bChar];

					if (aSortWord > bSortWord)
							return 1;

					if (aSortWord < bSortWord)
							return -1;
			}

			//	If characters match exactly, then go on to next character immediately without
			//	doing any extra work.
	}

	//	if you got to here, then return bestGuess
	return bestGuess;
}


int my_strcmp (const void *str1, const void *str2)
{
	return (myFastRelString ((unsigned char*)str1, strlen ((char *)str1), (unsigned char*)str2, strlen ((char *)str2) ));
}


int my_strcmp2 (const void *entry1, const void *entry2)
{
	struct NBPNameAndAddress *lEntry1 = (struct NBPNameAndAddress *)entry1;
	struct NBPNameAndAddress *lEntry2 = (struct NBPNameAndAddress *)entry2;

	return (myFastRelString ((unsigned char*) lEntry1->name, strlen (lEntry1->name), (unsigned char*) lEntry2->name, strlen (lEntry2->name) ));
}


int GetATStackState()
{
    int state = 0, error = 0;
    
    state = checkATStack();

    DBGLOG("returned from checkATStack with state = %d\n", state);

	switch( state )
	{
		case NOTLOADED:
		case LOADED:
			error = kNBPAppleTalkOff;
			break;
		
		default:
			if ( state != RUNNING )
				error = kNBPInternalError;			// unknown error condition
	}

	return error;
}
