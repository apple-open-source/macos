/* $Xorg: pl_startup.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_global.h"
#include <stdio.h>


int
PEXInitialize (display, info_return, length, error_string)

INPUT  Display		*display;
OUTPUT PEXExtensionInfo	**info_return;
INPUT  int		length;
OUTPUT char		*error_string;

{
    pexGetExtensionInfoReq	*req;
    char			*pBuf;
    pexGetExtensionInfoReply	rep;
    PEXExtensionInfo		*extInfo;
    XExtCodes			*pExtCodes;
    PEXDisplayInfo		*pexDisplayInfo;
    int				enumType, match, i;
    PEXEnumTypeDesc		*enumReturn;
    unsigned long		*numReturn;
    char			*string;

    extern Status		_PEXConvertMaxHitsEvent();
    Bool			_PEXConvertOCError();
    void			_PEXPrintOCError();
    int				_PEXCloseDisplay();


    /*
     * Lock the display.
     */

    LockDisplay (display);


    /*
     * If PEXInitialize() has already been successfully called on this
     * display, just return the extension information.
     */

    PEXGetDisplayInfo (display, pexDisplayInfo);

    if (pexDisplayInfo)
    {
	*info_return = pexDisplayInfo->extInfo;

        UnlockDisplay (display);
	PEXSyncHandle (display);

	return (0);
    }


    /*
     * Initialize the PEX extension on this display.
     */

    *info_return = NULL;

    if ((pExtCodes = XInitExtension (display, "X3D-PEX")) == NULL)
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "BadExtension",
	    "Could not initialize the PEX extension on the specified display",
            error_string, length);

	return (PEXBadExtension);
    }


    /*
     * For each display initialized by PEXlib, some additional data must
     * be maintained (such as extension codes and float format).  A linked
     * list of records is maintained, one for each open display, with the
     * most recently referenced display always at the beginning.
     */

    pexDisplayInfo = (PEXDisplayInfo *)	Xmalloc (sizeof (PEXDisplayInfo));

    if (!pexDisplayInfo)
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "BadLocalAlloc",
	    "Could not allocate memory for PEXlib internal usage",
            error_string, length);

	return (PEXBadLocalAlloc);
    }

    PEXAddDisplayInfo (display, pexDisplayInfo);

    pexDisplayInfo->extCodes = pExtCodes;
    pexDisplayInfo->extOpcode = pExtCodes->major_opcode;
    pexDisplayInfo->lastResID = 0;
    pexDisplayInfo->lastReqType = -1;
    pexDisplayInfo->lastReqNum = -1;


    /*
     * Check if the PEX server on this display supports the client's native
     * floating point format.  If not, choose a server supported format
     * (hopefully, the server's native floating point format is in the
     * first entry returned by PEXGetEnumTypeInfo).  In places specified by
     * the PEXlib spec, PEXlib will convert between the client's native
     * floating point format and the server supported format.
     */

    enumType = PEXETFloatFormat;

    if (PEXGetEnumTypeInfo (display, DefaultRootWindow (display), 1,
	&enumType, PEXETIndex, &numReturn, &enumReturn) == 0)
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "GetEnumFailure",
	    "Implicit call to PEXGetEnumTypeInfo by PEXInitialize failed",
            error_string, length);

	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadFloatConversion);
    }

    pexDisplayInfo->fpSupport = enumReturn;
    pexDisplayInfo->fpCount = *numReturn;

    for (i = match = 0; i < *numReturn; i++)
	if (enumReturn[i].index == NATIVE_FP_FORMAT)
	{
	    match = 1;
	    break;
	}

    if (enumReturn == NULL || *numReturn == 0)
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "NoFloats",
	    "No floating point formats supported by server",
            error_string, length);

	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadFloatConversion);
    }
    else if (match)
    {
	pexDisplayInfo->fpFormat = NATIVE_FP_FORMAT;
	pexDisplayInfo->fpConvert = 0;
    }
    else
    {
	pexDisplayInfo->fpFormat = enumReturn[0].index;
	pexDisplayInfo->fpConvert = 1;
    }

    Xfree ((char *) numReturn);


    /*
     * Tell Xlib how to convert an Output Command error from
     * wire to client format.
     */

    XESetWireToError (display, pExtCodes->first_error + BadPEXOutputCommand,
	_PEXConvertOCError);


    /*
     * Tell Xlib how to print the OC error.
     */

    XESetPrintErrorValues (display, pExtCodes->extension, _PEXPrintOCError);


    /*
     * Tell Xlib how to convert a MaxHitReachedEvent from
     * wire to client format.
     */

    XESetWireToEvent (display, pExtCodes->first_event + PEXMaxHitsReached,
	_PEXConvertMaxHitsEvent);


    /*
     * Tell Xlib which PEXlib function to call when the display is closed.
     */

    XESetCloseDisplay (display, pExtCodes->extension, _PEXCloseDisplay);


    /*
     * Get information about the PEX server extension.
     */

    PEXGetReq (GetExtensionInfo, pBuf);

    BEGIN_REQUEST_HEADER (GetExtensionInfo, pBuf, req);

    PEXStoreReqHead (GetExtensionInfo, req);
    req->clientProtocolMajor = PEX_PROTO_MAJOR;
    req->clientProtocolMinor = PEX_PROTO_MINOR;

    END_REQUEST_HEADER (GetExtensionInfo, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "GetInfoFailure",
	    "Could not get PEX extension information",
            error_string, length);

	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadExtension);
    }


    /*
     * Get the vendor name string and null terminate it.
     */

    if (!(string = (char *) Xmalloc ((unsigned) (rep.lengthName + 1))))
    {
        UnlockDisplay (display);
	PEXSyncHandle (display);

	XGetErrorDatabaseText (display, "PEXlibMessage", "BadLocalAlloc",
	    "Could not allocate memory for PEXlib internal usage",
            error_string, length);

	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadLocalAlloc);
    }

    _XReadPad (display, string, (long) (rep.lengthName));
    string[rep.lengthName] = '\0';


    /*
     * We can unlock the display now.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);


    /*
     * Store the extension info.
     */

    extInfo = *info_return = pexDisplayInfo->extInfo = (PEXExtensionInfo *)
	Xmalloc (sizeof (PEXExtensionInfo));

    if (!extInfo)
    {
	XGetErrorDatabaseText (display, "PEXlibMessage", "BadLocalAlloc",
	    "Could not allocate memory for PEXlib internal usage",
            error_string, length);

	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadLocalAlloc);
    }

    extInfo->major_version = rep.majorVersion;
    extInfo->minor_version = rep.minorVersion;
    extInfo->release = rep.release;
    extInfo->subset_info = rep.subsetInfo;
    extInfo->vendor_name = string;
    extInfo->major_opcode = pExtCodes->major_opcode;
    extInfo->first_event = pExtCodes->first_event;
    extInfo->first_error = pExtCodes->first_error;

    if (rep.majorVersion == PEX_PROTO_MAJOR)
	return (0);
    else
    {
	char str[PEXErrorStringLength];

	XGetErrorDatabaseText (display, "PEXlibMessage", "BadProtoVersion",
	    "Client speaks PEX %d.%d; Server speaks PEX %d.%d",
            str, PEXErrorStringLength);

	sprintf (error_string, str,
            PEX_PROTO_MAJOR, PEX_PROTO_MINOR,
	    rep.majorVersion, rep.minorVersion);
  
	PEXRemoveDisplayInfo (display, pexDisplayInfo);

	return (PEXBadProtocolVersion);
    }
}


PEXExtensionInfo *
PEXGetExtensionInfo (display)

INPUT Display	*display;

{
    PEXDisplayInfo 	*pexDisplayInfo;

    
    PEXGetDisplayInfo (display, pexDisplayInfo);
    return (pexDisplayInfo ? pexDisplayInfo->extInfo : NULL);
}


int
PEXGetProtocolFloatFormat (display)

INPUT Display	*display;

{
    PEXDisplayInfo 	*pexDisplayInfo;

    
    PEXGetDisplayInfo (display, pexDisplayInfo);
    return (pexDisplayInfo ? pexDisplayInfo->fpFormat : 0);
}


/*
 * PEXGetEnumTypeInfo is broken in the PEX spec.  For 5.1, PEXlib will
 * be compatible with the PEX SI.  In 6.0, the encoding should be fixed.
 */

Status
PEXGetEnumTypeInfo (display, drawable, numEnumTypes, enumTypes,
    itemMask, numReturn, enumInfoReturn)

INPUT Display			*display;
INPUT Drawable			drawable;
INPUT unsigned long		numEnumTypes;
INPUT int			*enumTypes;
INPUT unsigned long		itemMask;
OUTPUT unsigned long		**numReturn;
OUTPUT PEXEnumTypeDesc		**enumInfoReturn;

{
    register pexGetEnumTypeInfoReq	*req;
    char				*pBuf;
    pexGetEnumTypeInfoReply		rep;
    char				*pstartrep;
    PEXEnumTypeDesc			*penum;
    unsigned long			*pcounts;
    char				*pstring;
    int					numDescs;
    int					totDescs;
    int					*srcEnumType;
    unsigned int			length;
    int					size;
    int					i, j;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    size = numEnumTypes * SIZEOF (CARD16);
    PEXGetReqExtra (GetEnumTypeInfo, size, pBuf);

    BEGIN_REQUEST_HEADER (GetEnumTypeInfo, pBuf, req);

    PEXStoreReqExtraHead (GetEnumTypeInfo, size, req);
    req->drawable = drawable;
    req->itemMask = itemMask;
    req->numEnums = numEnumTypes;

    END_REQUEST_HEADER (GetEnumTypeInfo, pBuf, req);

    for (i = 0, srcEnumType = enumTypes; i < numEnumTypes; i++)
    {
	STORE_CARD16 (*srcEnumType, pBuf);
	srcEnumType++;
    }

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*numReturn = NULL;
	*enumInfoReturn = NULL;
	return (0);			/* return an error */
    }


    /*
     * Error if fewer than numEnumTypes lists returned.
     */

    if (rep.numLists < numEnumTypes) 
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*numReturn = NULL;
	*enumInfoReturn = NULL;
	return (0);			/* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pstartrep, rep.length << 2);


    /*
     * Count total number of enums returned.
     */

    for (i = 0, totDescs = 0, pBuf = pstartrep; i < rep.numLists; i++)
    { 
	EXTRACT_CARD32 (pBuf, numDescs);
	totDescs += numDescs; 

	if (i == rep.numLists - 1)
	    break;

	if (itemMask == PEXETIndex)
	{
	    pBuf += PADDED_BYTES (numDescs * SIZEOF (INT16));
	}
	else if (itemMask == PEXETMnemonic)
	{
	    for (j = 0; j < numDescs; j++)
	    {
		GET_CARD16 (pBuf, length);
	        pBuf += PADDED_BYTES (SIZEOF (CARD16) + length);
	    }
	}
	else if (itemMask == PEXETAll)
	{
	    for (j = 0; j < numDescs; j++)
	    {
	        pBuf += SIZEOF (INT16);
	        EXTRACT_CARD16 (pBuf, length);
	        pBuf += PADDED_BYTES (length);
	    }
	}
    }


    /*
     * Allocate storage for enum data to be returned to the client.
     */

    if (itemMask == PEXETCounts)
	*enumInfoReturn = NULL;
    else
    {
	*enumInfoReturn = penum = (PEXEnumTypeDesc *)
	    Xmalloc ((unsigned) (totDescs * sizeof (PEXEnumTypeDesc)));
    }


    /*
     * Allocate storage to return the counts to the client.
     */

    *numReturn = pcounts = (unsigned long *)
       Xmalloc ((unsigned) (numEnumTypes * sizeof (unsigned long)));


    /*
     * Retrieve the lists of enum info.
     */

    for (i = 0, pBuf = pstartrep; i < rep.numLists; i++, pcounts++)
    {
	EXTRACT_CARD32 (pBuf, numDescs);
        *pcounts = numDescs;

	if (itemMask == PEXETIndex)
	{
	    for (j = 0; j < numDescs; j++, penum++)
	    {
		penum->descriptor = NULL;
		EXTRACT_INT16 (pBuf, penum->index);
	    }

	    if (numDescs & 1)
	        pBuf += SIZEOF (INT16);
	}
	else if (itemMask == PEXETMnemonic)
	{
	    for (j = 0; j < numDescs; j++, penum++)
	    {
		penum->index = 0;
		EXTRACT_CARD16 (pBuf, length);

		penum->descriptor = pstring =
		    (char *) Xmalloc ((unsigned) (length + 1));
		memcpy (pstring, pBuf, length);
		pstring[length] = '\0';       /* null terminate */

		pBuf += (PADDED_BYTES (SIZEOF (CARD16) + length) - 
		    SIZEOF (CARD16));
	    }
	}
	else if (itemMask == PEXETAll)
	{
	    for (j = 0; j < numDescs; j++, penum++)
	    {
		EXTRACT_INT16 (pBuf, penum->index);
		EXTRACT_CARD16 (pBuf, length);

		penum->descriptor = pstring =
		    (char *) Xmalloc ((unsigned) (length + 1));
		memcpy (pstring, pBuf, length);
		pstring[length] = '\0';       /* null terminate */

		pBuf += PADDED_BYTES (length);
	    }
	}
    }

    FINISH_WITH_SCRATCH (display, pstartrep, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetImpDepConstants (display, drawable, numNames, names, constantsReturn)

INPUT Display			*display;
INPUT Drawable			drawable;
INPUT unsigned long		numNames;
INPUT unsigned short		*names;
OUTPUT PEXImpDepConstant	**constantsReturn;

{
    register pexGetImpDepConstantsReq	*req;
    char				*pBuf;
    pexGetImpDepConstantsReply		rep;
    long				size;
    CARD32				*c32list;
    float				*flist;
    int					i;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    size = numNames * SIZEOF (CARD16);
    PEXGetReqExtra (GetImpDepConstants, size, pBuf);

    BEGIN_REQUEST_HEADER (GetImpDepConstants, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (GetImpDepConstants, fpFormat, size, req);
    req->drawable = drawable;
    req->numNames = numNames;

    END_REQUEST_HEADER (GetImpDepConstants, pBuf, req);

    STORE_LISTOF_CARD16 (numNames, names, pBuf);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*constantsReturn = NULL;
        return (0);            /* return an error */
    }


    /*
     * Allocate a buffer for the client.
     */

    *constantsReturn = (PEXImpDepConstant *) Xmalloc (
	(unsigned) (numNames * sizeof (PEXImpDepConstant)));


    /*
     * Read the values into the buffer.
     */

    c32list = (CARD32 *) (*constantsReturn);
    flist = (float *) (*constantsReturn);

    if (!fpConvert)
    {
	XREAD_LISTOF_CARD32 (display, numNames, c32list);
    }
    else
    {
	for (i = 0; i < numNames; i++)
	{
	    switch (names[i])
	    {
	    case PEXIDDitheringSupported:
	    case PEXIDMaxEdgeWidth:
	    case PEXIDMaxLineWidth:
	    case PEXIDMaxMarkerSize:
	    case PEXIDMaxModelClipPlanes:
	    case PEXIDMaxNameSetNames:
	    case PEXIDMaxNonAmbientLights:
	    case PEXIDMaxNURBOrder:
	    case PEXIDMaxTrimCurveOrder:
	    case PEXIDMinEdgeWidth:
	    case PEXIDMinLineWidth:
	    case PEXIDMinMarkerSize:
	    case PEXIDNominalEdgeWidth:
	    case PEXIDNominalLineWidth:
	    case PEXIDNominalMarkerSize:
	    case PEXIDNumSupportedEdgeWidths:
	    case PEXIDNumSupportedLineWidths:
	    case PEXIDNumSupportedMarkerSizes:
	    case PEXIDBestColorApprox:
	    case PEXIDTransparencySupported:
	    case PEXIDDoubleBufferingSupported:
	    case PEXIDMaxHitsEventSupported:

		_XRead (display, (char *) &c32list[i], SIZEOF (CARD32));
		break;

	    case PEXIDChromaticityRedU:
	    case PEXIDChromaticityRedV:
	    case PEXIDLuminanceRed:
	    case PEXIDChromaticityGreenU:
	    case PEXIDChromaticityGreenV:
	    case PEXIDLuminanceGreen:
	    case PEXIDChromaticityBlueU:
	    case PEXIDChromaticityBlueV:
	    case PEXIDLuminanceBlue:
	    case PEXIDChromaticityWhiteU:
	    case PEXIDChromaticityWhiteV:
	    case PEXIDLuminanceWhite:
		
	    {
		char temp[4];
		_XRead (display, temp, SIZEOF (float));
		FP_CONVERT_NTOH_BUFF  (temp, flist[i], fpFormat);
	    }
	    }
	}
    }


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXMatchRenderingTargets (display, drawable, depth, type, visual,
    maxTargets, numTargets, targets)

INPUT Display			*display;
INPUT Drawable			drawable;
INPUT int			depth;
INPUT int			type;
INPUT Visual			*visual;
INPUT unsigned long  	        maxTargets;
OUTPUT unsigned long		*numTargets;
OUTPUT PEXRenderingTarget	**targets;

{
    register pexMatchRenderingTargetsReq	*req;
    char					*pBuf;
    pexMatchRenderingTargetsReply       	rep;
    PEXRenderingTarget				*info;
    pexRendererTarget				*matchRec;
    int						i;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (MatchRenderingTargets, pBuf);

    BEGIN_REQUEST_HEADER (MatchRenderingTargets, pBuf, req);

    PEXStoreReqHead (MatchRenderingTargets, req);
    req->drawable = drawable;
    req->depth = depth;
    req->type = type;
    req->visualID = visual ? visual->visualid : 0;
    req->maxTriplets = maxTargets;

    END_REQUEST_HEADER (MatchRenderingTargets, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numTargets = 0;
	*targets = NULL;
        return (0);               /* return an error */
    }

    *numTargets = rep.length / 2;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);


    /*
     * The total size of pexRendererTarget is 8 bytes, so it's safe
     * to have matchRec point to pBuf on 64 bit machines.
     */

    matchRec = (pexRendererTarget *) pBuf;


    /*
     * Allocate a buffer for the target list to pass back to the client.
     */

    *targets = info = (PEXRenderingTarget *) Xmalloc (
	(unsigned) (*numTargets * sizeof (PEXRenderingTarget)));

    for (i = 0; i < *numTargets; i++)
    {
	info->depth = matchRec->depth;
	info->type = matchRec->type;
	info->visual = _XVIDtoVisual (display, matchRec->visualID);
	info++;
	matchRec++;
    }

    FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


/*
 * Routine called to convert an Output Command error from wire format
 * to client format.  The callback is set up in PEXInitialize.
 */

Bool
_PEXConvertOCError (display, client_error, wire_error)

INPUT Display		*display;
OUTPUT XErrorEvent	*client_error;
INPUT xError		*wire_error;

{
    PEXOCErrorEvent		*client = (PEXOCErrorEvent *) client_error;
    pexOutputCommandError	*wire = (pexOutputCommandError *) wire_error;


    /*
     * PEXOCErrorEvent = XErrorEvent + oc_op_code + ocs_processed_count
     * Xlib will convert all of the XErrorEvent fields.  We must
     * convert the op code and count.
     */

    client->op_code = wire->opcode;
    client->count = wire->numCommands;

    return (True);
}


/*
 * Routine called when an Output Command error is printed.
 * The callback is set up in PEXInitialize.
 */

void
_PEXPrintOCError (display, error, fp)

INPUT Display		*display;
INPUT XErrorEvent	*error;
#if NeedFunctionPrototypes
INPUT void		*fp;
#else
INPUT FILE		*fp;
#endif
{
    /*
     * Xlib bug - extension codes should be passed to this function,
     * but they're not.  We must get them ourselves.
     */

    PEXDisplayInfo 	*pexDisplayInfo;
    char		opcode_message[PEXErrorStringLength];
    char		oc_count_message[PEXErrorStringLength];


    PEXGetDisplayInfo (display, pexDisplayInfo);

    if (error->error_code ==
	pexDisplayInfo->extCodes->first_error + BadPEXOutputCommand)
    {
	PEXOCErrorEvent	*oc_error = (PEXOCErrorEvent *) error;

	XGetErrorDatabaseText (display,
	    "PEXlibMessage", "OCErrorOpCode",
	    "Opcode of failed output command : %d\n",
            opcode_message, PEXErrorStringLength);

	XGetErrorDatabaseText (display,
	    "PEXlibMessage", "OCErrorCount",
	    "Number of output commands processed before error : %d\n",
            oc_count_message, PEXErrorStringLength);

	fprintf (fp, "  ");
	fprintf (fp, opcode_message, oc_error->op_code);

	fprintf (fp, "  ");
	fprintf (fp, oc_count_message, oc_error->count);
    }
}


/*
 * Routine called when a display is closed via XCloseDisplay.
 * The callback is set up in PEXInitialize.
 */

int
_PEXCloseDisplay (display, codes)

INPUT Display	*display;
INPUT XExtCodes	*codes;

{
    PEXDisplayInfo	*pexDisplayInfo;


    /*
     * Free the extension codes and other info attached to this display.
     */

    PEXRemoveDisplayInfo (display, pexDisplayInfo);

    if (pexDisplayInfo == NULL)
	return (0);

    Xfree ((char *) (pexDisplayInfo->extInfo->vendor_name));
    Xfree ((char *) (pexDisplayInfo->extInfo));
    Xfree ((char *) (pexDisplayInfo->fpSupport));
    Xfree ((char *) pexDisplayInfo);


    /*
     * Free the pick path cache (if it's not in use)
     */

    if (PEXPickCache && !PEXPickCacheInUse)
	Xfree ((char *) PEXPickCache);

    return (1);
}
