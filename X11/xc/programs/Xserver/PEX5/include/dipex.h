/* $Xorg: dipex.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/


/*	Pex server-private include file
 */

#ifndef DIPEX_H
#define DIPEX_H 1

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include "PEX.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "pexSwap.h"
#include "gcstruct.h"
#include "dixstruct.h"
#include "ddpex.h"
#include "misc.h"
#include "pixmap.h"


#define PEXCONTEXTTABLE 1
#define PEXID(client, type)	((((client)->index)<<CLIENTOFFSET) + \
							(SERVER_BIT) + (type))

#ifndef LOCAL_FLAG
#define LOCAL_FLAG extern
#endif

LOCAL_FLAG ErrorCode PexReqCode;
LOCAL_FLAG ErrorCode PexErrorBase;
LOCAL_FLAG ErrorCode PexEventBase;

/* resources */
extern Bool AddResource();

#define ADDRESOURCE(id,what,stuff) \
    if (!(AddResource((id),(what),(pointer)(stuff)))) { \
	PEX_ERR_EXIT(BadAlloc,0,cntxtPtr); \
    }

/* resource class */
LOCAL_FLAG unsigned long PEXClass;

/* resource types */
LOCAL_FLAG unsigned long PEXContextType;
LOCAL_FLAG unsigned long PEXLutType;
LOCAL_FLAG unsigned long PEXPipeType;
LOCAL_FLAG unsigned long PEXRendType;
LOCAL_FLAG unsigned long PEXStructType;
LOCAL_FLAG unsigned long PEXNameType;
LOCAL_FLAG unsigned long PEXSearchType;
LOCAL_FLAG unsigned long PEXWksType;
LOCAL_FLAG unsigned long PEXPickType;
LOCAL_FLAG unsigned long PEXFontType;

/* additional type so that we get Drawable-destroy notification */
LOCAL_FLAG unsigned long PEXWksDrawableType;

/* default font */
LOCAL_FLAG diFontHandle defaultPEXFont;


typedef ErrorCode (*RequestFunction)();
typedef void (*ReplyFunction)();

/* OCFunction is typedef'd in pexSwap.h */

/* This structure holds all client dependent information.  */
typedef struct {
    ClientPtr	    client;
    pexReq	    *current_req;
    RequestFunction *pexRequest;
    ReplyFunction   *pexSwapReply;	/* only non-0 if conversion needed */
    OCFunction	    *pexSwapRequestOC;	/* only non-0 if conversion needed */
    OCFunction	    *pexSwapReplyOC;	/* only non-0 if conversion needed */
    pexSwap	    *swap;
} pexContext;


#define VALID_OC(n) \
    if ((PEXOCAll >= (n) ) || ((n) > PEXMaxOC)) { \
	err = BadValue; \
	PEX_ERR_EXIT(err, (n), cntxtPtr); }


#define OC_RANGE(ELTYPE) ((PEXOCAll < (ELTYPE) ) && ( PEXMaxOC >=(ELTYPE)))

/* these arrays are in dipexExt.c */

#ifndef _DIPEXEXT_
LOCAL_FLAG unsigned long add_pad_of[];

LOCAL_FLAG unsigned int ColourSpecSizes[];
#endif


#define PADDED(PB) (PB + add_pad_of[(PB & 3)]);
#define LWORDS(PB) (((PB) + add_pad_of[((*((int *)&(PB))) & 3)])/4);

#define WritePEXReplyToClient(CONTEXT, PEXREQUEST, DATA_SIZE, DATA)  { \
    int num_bytes = (int)(DATA_SIZE); \
    (DATA)->type = X_Reply; \
    (DATA)->sequenceNumber = (CONTEXT)->client->sequence; \
    if ((CONTEXT)->pexSwapReply) \
	(CONTEXT)->pexSwapReply[(CONTEXT)->current_req->opcode]\
		((CONTEXT), (PEXREQUEST), (DATA)); \
    (void) WriteToClient ((CONTEXT)->client, num_bytes, (char *)(DATA)); }


#define WritePEXBufferReply(TYPE)  \
    WritePEXReplyToClient(  cntxtPtr, strmPtr, \
			    sizeof(TYPE) + *((int *)&(pPEXBuffer->dataSize)), \
			    reply)


#define SETUP_INQ(TYPE) \
    pPEXBuffer->pBuf = (unsigned char *)(pPEXBuffer->pHead+sizeof(TYPE)); \
    pPEXBuffer->dataSize = 0


#define SETUP_VAR_REPLY(TYPE) \
	TYPE *reply = (TYPE *)(pPEXBuffer->pHead); \
	reply->length = LWORDS(pPEXBuffer->dataSize)



/* some dipex types */

typedef struct {
    ddWKSResource   dd_data;
    Drawable	    did;
} dipexPhigsWks;


/*
 * NOTE: The following structure has it's fields layed out in an
 *       order dependent way.  It is created in the DI as a dipexFont,
 *       and is sometime referenced in the DD as a ddFontResource.
 */

typedef struct {
    ddFontResource  ddFont;
    long	    refcnt;
} dipexFont;


/*
    Some data structures so that we can keep track of Drawables,
    in case they get destroyed (about which destruction we want
    notification).
 */
typedef struct _dipexWksDrawableLink {
    pexPhigsWks			    wksid;
    dipexPhigsWks		    *wks;
    struct _dipexWksDrawableLink    *next;
} dipexWksDrawableLink;

typedef struct {
    Drawable		    id;
    DrawablePtr		    x_drawable;
    dipexWksDrawableLink    *wks_list;
} dipexWksDrawable;


#define GetId(PTR) (((PTR)) ? (PTR)->id : 0)


#undef LOCAL_FLAG

#endif /* SERVER_PEX_H */
