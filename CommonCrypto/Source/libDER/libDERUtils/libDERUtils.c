/* Copyright (c) 2005-2007 Apple Inc. All Rights Reserved. */

/*
 * libDERUtils.c - support routines for libDER tests & examples 
 *
 * Created Nov. 7 2005 by dmitch
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

