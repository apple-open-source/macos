/*
 * XDPSuserpath.c
 *
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dpstk/XDPSuserpath.c,v 1.2 2000/06/07 22:03:01 tsi Exp $ */

#include <DPS/dpsXclient.h>
#include <DPS/dpsops.h>
#include <DPS/dpsXuserpath.h>

typedef struct _t_NumStrHeader {
    unsigned char type;
    unsigned char representation;
    unsigned short length;
} NumStrHeader;

void PSDoUserPath(coords, numCoords, numType, ops, numOps, bbox, action)
    DPSPointer coords;
    int numCoords;
    DPSNumberFormat numType;
    DPSUserPathOp *ops;
    int numOps;
    DPSPointer bbox;
    DPSUserPathAction action;
{
    DPSDoUserPath(DPSGetCurrentContext(), coords, numCoords, numType, ops,
		  numOps, bbox, action);
}

void DPSDoUserPath(ctxt, coords, numCoords, numType, ops, numOps, bbox, action)
    DPSContext ctxt;
    DPSPointer coords;
    int numCoords;
    DPSNumberFormat numType;
    DPSUserPathOp *ops;
    int numOps;
    DPSPointer bbox;
    DPSUserPathAction action;
{
    typedef struct {
	unsigned char tokenType;
	unsigned char topLevelCount;
	unsigned short nBytes;
	DPSBinObjGeneric obj0;
	DPSBinObjGeneric obj1;
	DPSBinObjGeneric obj2;
	DPSBinObjGeneric obj3;
    } _dpsQ;
    static _dpsQ _dpsF = {
	DPS_DEF_TOKENTYPE, 0, 36, /* will fill in topLevelCount later */
	{DPS_LITERAL|DPS_ARRAY, 0, 2, 16},
	{DPS_EXEC|DPS_NAME, 0, DPSSYSNAME, 0},    
	{DPS_LITERAL|DPS_STRING, 0, 0, 32}, /* param nums */
	{DPS_LITERAL|DPS_STRING, 0, 0, 32}, /* param ops */
    }; /* _dpsQ */
    register DPSBinObjRec *_dpsP = (DPSBinObjRec *)&_dpsF.obj0;
    register int _dps_offset = 32;
    int needBBox, hasUCache, numberSize;
    DPSUserPathOp setbboxOp;
    NumStrHeader nsHeader;

    if (numType >= dps_short && numType < dps_float) numberSize = 2;
    else numberSize = 4;

    hasUCache = (*ops == dps_ucache);

    if (hasUCache) {
	needBBox = (numOps > 1 && ops[1] != dps_setbbox);
    } else needBBox = (*ops != dps_setbbox);

    if (needBBox) {
	numOps += 1;
	setbboxOp = dps_setbbox;
    }
    
    numCoords += 4; /* Account for bbox */

    nsHeader.type = 149; /* Homogeneous Number Array */
    nsHeader.representation = numType;
    nsHeader.length = numCoords;
  
    /* If we're using the send operation, we modify the sequence so that
       it never gets to the action.  This leaves a hole in the sequence,
       but that's ok. */

    if (action == dps_send) _dpsF.topLevelCount = 1;
    else _dpsF.topLevelCount = 2;

    _dpsP[1].val.nameVal = action;
    _dpsP[2].length = (sizeof(NumStrHeader) + numCoords * numberSize);
    _dpsP[3].length = numOps;
    _dpsP[3].val.stringVal = _dps_offset;
    _dps_offset += numOps;
    _dpsP[2].val.stringVal = _dps_offset;
    _dps_offset += _dpsP[2].length;
    _dpsF.nBytes = _dps_offset+4;

    if (needBBox) numOps -= 1;

    numCoords -= 4; /* Unaccount for bbox */

    DPSBinObjSeqWrite(ctxt, (char *) &_dpsF, 36);
    if (needBBox) {
	if (hasUCache) {
	    DPSWriteStringChars(ctxt, (char *) ops, 1);
	    ops++; numOps--;
	}
	DPSWriteStringChars(ctxt, (char *) &setbboxOp, 1);
    }
    DPSWriteStringChars(ctxt, (char *) ops, numOps);
    DPSWriteStringChars(ctxt, (char *) &nsHeader, sizeof(NumStrHeader));
    DPSWriteStringChars(ctxt, (char *) bbox, 4 * numberSize);
    DPSWriteStringChars(ctxt, (char *) coords, numCoords * numberSize);
}

Bool PSHitUserPath(x, y, radius,
		   coords, numCoords, numType, ops, numOps, bbox, action)
    double x, y, radius;
    DPSPointer coords;
    int numCoords;
    DPSNumberFormat numType;
    DPSUserPathOp *ops;
    int numOps;
    DPSPointer bbox;
    DPSUserPathAction action;
{
    return DPSHitUserPath(DPSGetCurrentContext(), x, y, radius,
			  coords, numCoords, numType, ops,
			  numOps, bbox, action);
}

Bool DPSHitUserPath(ctxt, x, y, radius,
		       coords, numCoords, numType, ops, numOps, bbox, action)
    DPSContext ctxt;
    double x, y, radius;
    DPSPointer coords;
    int numCoords;
    DPSNumberFormat numType;
    DPSUserPathOp *ops;
    int numOps;
    DPSPointer bbox;
    DPSUserPathAction action;
{
    float aCoords[5];
    DPSUserPathOp aOps[1];
    float aBbox[4];
    int result;

    if (radius != 0.0) {
	aCoords[0] = x;
	aCoords[1] = y;
	aCoords[2] = radius;
	aCoords[3] = 0.0;
	aCoords[4] = 360.0;
	aOps[0] = dps_arc;
	aBbox[0] = x - radius;
	aBbox[1] = y - radius;
	aBbox[2] = x + radius;
	aBbox[3] = y + radius;

	switch (action) {
	    case dps_infill:
	    case dps_ineofill:
	    case dps_instroke:
		DPSDoUserPath(ctxt, (DPSPointer) aCoords, 5, dps_float,
			      aOps, 1, (DPSPointer) aBbox, action);
		break;
	    case dps_inufill:
	    case dps_inueofill:
	    case dps_inustroke:
		DPSDoUserPath(ctxt, (DPSPointer) aCoords, 5, dps_float,
			      aOps, 1, (DPSPointer) aBbox, dps_send);
		DPSDoUserPath(ctxt, coords, numCoords, numType, ops,
			      numOps, bbox, action);
		break;
	    default:
		return False;
	}
	DPSgetboolean(ctxt, &result);

    } else {
	switch (action) {
	    case dps_infill:
	        DPSinfill(ctxt, x, y, &result);
		break;
	    case dps_ineofill:
		DPSineofill(ctxt, x, y, &result);
		break;
	    case dps_instroke:
		DPSinstroke(ctxt, x, y, &result);
		break;
	    case dps_inufill:
	    case dps_inueofill:
	    case dps_inustroke:
		DPSsendfloat(ctxt, x);
		DPSsendfloat(ctxt, y);
		DPSDoUserPath(ctxt, coords, numCoords, numType, ops,
			      numOps, bbox, action);
		DPSgetboolean(ctxt, &result);
		break;
	    default:
		return False;
	    }
    }
    return result;
}

