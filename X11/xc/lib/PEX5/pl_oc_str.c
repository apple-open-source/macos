/* $Xorg: pl_oc_str.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */

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
#include "pl_oc_util.h"


void
PEXExecuteStructure (display, resource_id, req_type, structure)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXStructure	structure;

{
    register pexExecuteStructure	*req;

    BEGIN_SIMPLE_OC (ExecuteStructure, resource_id, req_type, req);
    req->id = structure;
    END_SIMPLE_OC (ExecuteStructure, resource_id, req_type, req);
}


void
PEXLabel (display, resource_id, req_type, label)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT long		label;

{
    register pexLabel	*req;

    BEGIN_SIMPLE_OC (Label, resource_id, req_type, req);
    req->label = label;
    END_SIMPLE_OC (Label, resource_id, req_type, req);
}


void
PEXNoop (display, resource_id, req_type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;

{
    register pexNoop	*req;

    BEGIN_SIMPLE_OC (Noop, resource_id, req_type, req);
    /* no data */
    END_SIMPLE_OC (Noop, resource_id, req_type, req);
}


void
PEXApplicationData (display, resource_id, req_type, length, data)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		length;
INPUT char		*data;

{
    register pexApplicationData	*req;
    char			*pBuf;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (length);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexApplicationData), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    BEGIN_OC_HEADER (ApplicationData, dataLength, pBuf, req);

    req->numElements = length;

    END_OC_HEADER (ApplicationData, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_CARD8_PAD (length, data);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXGSE (display, resource_id, req_type, id, length, data)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT long		id;
INPUT int		length;
INPUT char		*data;

{
    register pexGSE	*req;
    char		*pBuf;
    int			dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (length);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexGSE), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the GSE request header data. 
     */

    BEGIN_OC_HEADER (GSE, dataLength, pBuf, req);

    req->id = id;
    req->numElements = length;

    END_OC_HEADER (GSE, pBuf, req);


    /*
     * Copy the GSE data to the oc.
     */

    OC_LISTOF_CARD8_PAD (length, data);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}
