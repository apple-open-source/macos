/* $Xorg: unsquish.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1987, 1998  The Open Group

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

*/
/*
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* handles unsquishing events */

#include "X.h"
#define NEED_REPLIES
#define NEED_EVENTS
#include "Xproto.h"
#include "misc.h"
#define	_XLBX_SERVER_
#include "lbxstr.h"
#include "proxyopts.h"

#include	<stdio.h>

static char lbxevsize[] = {
    sz_xError,
    sz_xReply,
    lbxsz_KeyButtonEvent,
    lbxsz_KeyButtonEvent,
    lbxsz_KeyButtonEvent,
    lbxsz_KeyButtonEvent,
    lbxsz_KeyButtonEvent,   
    lbxsz_EnterLeaveEvent,
    lbxsz_EnterLeaveEvent,
    lbxsz_FocusEvent,
    lbxsz_FocusEvent,
    lbxsz_KeymapEvent,
    lbxsz_ExposeEvent,
    lbxsz_GfxExposeEvent,
    lbxsz_NoExposeEvent,
    lbxsz_VisibilityEvent,
    lbxsz_CreateNotifyEvent,
    lbxsz_DestroyNotifyEvent,
    lbxsz_UnmapNotifyEvent,
    lbxsz_MapNotifyEvent,
    lbxsz_MapRequestEvent,
    lbxsz_ReparentEvent,
    lbxsz_ConfigureNotifyEvent,
    lbxsz_ConfigureRequestEvent,
    lbxsz_GravityEvent,
    lbxsz_ResizeRequestEvent,
    lbxsz_CirculateEvent,
    lbxsz_CirculateEvent,
    lbxsz_PropertyEvent,
    lbxsz_SelectionClearEvent,
    lbxsz_SelectionRequestEvent,
    lbxsz_SelectionNotifyEvent,
    lbxsz_ColormapEvent,
    lbxsz_ClientMessageEvent,
    lbxsz_MappingNotifyEvent
};

int
EventLength(ev, squish)
    xEvent     *ev;
    Bool       squish;
{
    if (!squish || ev->u.u.type >= LASTEvent)
	return sz_xEvent;
    return lbxevsize[ev->u.u.type];
}

Bool
UnsquishEvent(rep, ev, lenp)
    xReply     *rep;
    xEvent     *ev;
    int        *lenp;
{
    if (*lenp < sz_xEvent) {
	memcpy((char *)ev, (char *)rep, *lenp);
	*lenp = sz_xEvent;
	return TRUE;
    }
    return FALSE;
}
