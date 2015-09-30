/*
 * Copyright (c) 2005-2007,2011,2014 Apple Inc. All Rights Reserved.
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


/*
 * libDERUtils.c - support routines for libDER tests & examples 
 *
 */

#include <libDERUtils/libDERUtils.h>
#include <stdio.h>

const char *DERReturnString(
	DERReturn		drtn)
{
	static char unknown[128];
	
	switch(drtn) {
		case DR_Success: return "DR_Success";
		case DR_EndOfSequence: return "DR_EndOfSequence";
		case DR_UnexpectedTag: return "DR_UnexpectedTag";
		case DR_DecodeError: return "DR_DecodeError";
		case DR_Unimplemented: return "DR_Unimplemented";
		case DR_IncompleteSeq: return "DR_IncompleteSeq";
		default:
			sprintf(unknown, "Unknown error (%d)", (int)drtn);
			return unknown;
	}
}
	
void DERPerror(
	const char *op,
	DERReturn drtn)
{
	fprintf(stderr, "*** %s: %s\n", op, DERReturnString(drtn));
}

