/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CRCCalc
 */

#include "CRCCalc.h"

// --------------------------------------------------------------------------------
//	* CRCCalc ()
// --------------------------------------------------------------------------------

CRCCalc::CRCCalc ( void )
{
} // CRCCalc

// --------------------------------------------------------------------------------
//	* ~CRCCalc ()
// --------------------------------------------------------------------------------

CRCCalc::~CRCCalc ( void )
{
} // ~CRCCalc

// --------------------------------------------------------------------------------
//	* UPDC16 ()
// --------------------------------------------------------------------------------

unsigned short CRCCalc::UPDC16 ( Byte *ptr, unsigned long count, unsigned short crc )
{
	while (count-- > 0)
	{
		crc = CRCTable[((crc >> 8) ^ *ptr++) & 0xFF] ^ (crc << 8);
	}
	return( crc );
}

// --------------------------------------------------------------------------------
//	* updcrc ()
// --------------------------------------------------------------------------------

unsigned short CRCCalc::updcrc ( register Byte b, register unsigned short crc )
{
	return CRCTable[((crc >> 8) ^ b) & 0xFF] ^ (crc << 8);
}

// --------------------------------------------------------------------------------
//	* UPDC32 ()
// --------------------------------------------------------------------------------

unsigned long CRCCalc::UPDC32 ( register Byte b, register unsigned long c )
{
	return (cr3tab[((int)c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFF));
}
