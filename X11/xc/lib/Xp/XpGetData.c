/* $Xorg: XpGetData.c,v 1.4 2000/08/17 19:46:07 cpqbld Exp $ */
/******************************************************************************
 ******************************************************************************
 **
 ** (c) Copyright 1996 Hewlett-Packard Company
 ** (c) Copyright 1996 International Business Machines Corp.
 ** (c) Copyright 1996 Sun Microsystems, Inc.
 ** (c) Copyright 1996 Novell, Inc.
 ** (c) Copyright 1996 Digital Equipment Corp.
 ** (c) Copyright 1996 Fujitsu Limited
 ** (c) Copyright 1996 Hitachi, Ltd.
 ** 
 ** Permission is hereby granted, free of charge, to any person obtaining a copy
 ** of this software and associated documentation files (the "Software"), to deal
 ** in the Software without restriction, including without limitation the rights
 ** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ** copies of the Software, and to permit persons to whom the Software is
 ** furnished to do so, subject to the following conditions:
 **
 ** The above copyright notice and this permission notice shall be included in
 ** all copies or substantial portions of the Software.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 ** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 ** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **
 ** Except as contained in this notice, the names of the copyright holders shall
 ** not be used in advertising or otherwise to promote the sale, use or other
 ** dealings in this Software without prior written authorization from said
 ** copyright holders.
 **
 ******************************************************************************
 *****************************************************************************/
/* $XFree86: xc/lib/Xp/XpGetData.c,v 1.5 2002/10/16 00:37:32 dawes Exp $ */

#define NEED_REPLIES

#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#define MAX_XP_BUFFER_SIZE 32768

/*
 * At the tail of every async struct of ours is attached the
 * following "dev private" information needed by our async handler.
 */
typedef struct {
    XPContext       context;
    XPSaveProc      save_proc;
    XPFinishProc    finish_proc;
    XPointer        client_data;
    _XAsyncHandler  *async;
    unsigned long   seq;	/* sequence # that will trigger handler */
} _XpState;


/*
 * The following is the handler for async replies from
 * XpGetDocumentData().
 */
static Bool
_XpGetDocDataHandler(dpy, rep, buf, len, adata)
    register Display *dpy;
    register xReply *rep;
    char *buf;
    int len;
    XPointer adata;
{
    register _XpState            *state;
    xPrintGetDocumentDataReply   replbuf;
    xPrintGetDocumentDataReply   *repl;
    int dataLen;
    char *data;

    state = (_XpState *)adata;

    /*
     * Bypass this handler if the reply is NOT the one we're looking for.
     */
    if (dpy->last_request_read != state->seq) {
        return False;
    }

    /*
     * When an error occurs, call the finish_proc and then de-queue
     * this event handler.  Once an error occurs, all bets are off.
     * The error XPGetDocError is *not* the most descriptive, so the
     * caller will be encouraged to dig around for the corresponding
     * generated error.
     *
     * Note - Do not confuse the "generated" errors here with
     * XPGetDocSecondConsumer which is returned in a protocol reply.
     */
    if (rep->generic.type == X_Error) {
	(*state->finish_proc)(  (Display *) dpy,
				(XPContext) state->context,
				XPGetDocError,
				(XPointer) state->client_data );
        DeqAsyncHandler(dpy, state->async);
        Xfree(state->async);
        return False;
    }

    repl = (xPrintGetDocumentDataReply *)
        _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len, 0, False);

    if (repl->dataLen) {
	/*
	 * Call save_proc in cases where there was document data
	 */

	dataLen = repl->length << 2;				/*with pad len*/

	data = (char *) _XAllocTemp( dpy, dataLen );

	_XGetAsyncData( dpy, (char *) data, buf, len,
			SIZEOF(xPrintGetDocumentDataReply), dataLen, 0);

	(*state->save_proc)( (Display *) dpy,
			     (XPContext) state->context,
			     (unsigned char *) data,
			     (unsigned int) repl->dataLen,	/* actual len */
			     (XPointer) state->client_data );

	_XFreeTemp( dpy, (char *) data, dataLen );
    }

    if (repl->finishedFlag) {
	/*
	 * Call finish_proc
	 */
	 (*state->finish_proc)( (Display *) dpy,
				(XPContext) state->context,
				(XPGetDocStatus) repl->statusCode,
				(XPointer) state->client_data );
	/*
	 * De-queue this async handler - we're done.
	 */
	DeqAsyncHandler( dpy, state->async );
	Xfree(state->async);
    }

    return True;	/* the reply WAS consumed by this handler */
}

/******************************************************************************
 *
 * XpGetDocumentData()
 *
 * ...registers callbacks to be triggered when async protocol replies
 * come back in response to the origial request.
 *
 * Returned Status indicate whether the callbacks will be used
 * (finish_proc and possibly save_proc), or whether they will
 * never be used.
 *
 */
Status
XpGetDocumentData (
    Display       *dpy,
    XPContext     context,
    XPSaveProc    save_proc,
    XPFinishProc  finish_proc,
    XPointer      client_data
)
{
    xPrintGetDocumentDataReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);
    _XAsyncHandler *async;
    _XpState *async_state;


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return(0); /* No such extension */

    async = (_XAsyncHandler *)Xmalloc(sizeof(_XAsyncHandler) +
                                      sizeof(_XpState));
    if (!async)
	return(0); /* malloc error */
    async_state = (_XpState *)(async + 1);

    LockDisplay (dpy);

    GetReq(PrintGetDocumentData,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetDocumentData;
    req->printContext = context;
    req->maxBufferSize = MAX_XP_BUFFER_SIZE;	/* use as a hint to X server */

    async_state->context     = context;
    async_state->save_proc   = save_proc;
    async_state->finish_proc = finish_proc;
    async_state->client_data = client_data;
    async_state->seq         = dpy->request;
    async_state->async       = async;

    async->next = dpy->async_handlers;
    async->handler = _XpGetDocDataHandler;
    async->data = (XPointer)async_state;

    dpy->async_handlers = async;

    UnlockDisplay(dpy);
    SyncHandle();

    return(1);		/* success at registering a handler */
}

