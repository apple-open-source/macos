/* $Xorg: imLcFlt.c,v 1.3 2000/08/17 19:45:13 cpqbld Exp $ */
/******************************************************************

              Copyright 1992 by Fuji Xerox Co., Ltd.
              Copyright 1992, 1994 by FUJITSU LIMITED

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Fuji Xerox, 
FUJITSU LIMITED not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission. Fuji Xerox, FUJITSU LIMITED make no representations
about the suitability of this software for any purpose.
It is provided "as is" without express or implied warranty.

FUJI XEROX, FUJITSU LIMITED DISCLAIM ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL FUJI XEROX,
FUJITSU LIMITED BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

  Author   : Kazunori Nishihara	Fuji Xerox
  Modifier : Takashi Fujiwara   FUJITSU LIMITED 
                                fujiwara@a80.tech.yk.fujitsu.co.jp

******************************************************************/

#define NEED_EVENTS
#include "Xlibint.h"
#include <X11/keysym.h>
#include "Xlcint.h"
#include "Ximint.h"

Bool
_XimLocalFilter(d, w, ev, client_data)
    Display	*d;
    Window	 w;
    XEvent	*ev;
    XPointer	 client_data;
{
    Xic		 ic = (Xic)client_data;
    KeySym	 keysym;
    static char	 buf[256];
    DefTree	*p;

    if(   (ev->type != KeyPress)
       || (ev->xkey.keycode == 0)
       || (((Xim)ic->core.im)->private.local.top == (DefTree *)NULL) )
	return(False);

    XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &keysym, NULL);

    if(IsModifierKey(keysym))
	return (False);

    for(p = ic->private.local.context; p; p = p->next) {
	if(((ev->xkey.state & p->modifier_mask) == p->modifier) &&
	    (keysym == p->keysym)) {
	    break;
	}
    }

    if(p) { /* Matched */
	if(p->succession) { /* Intermediate */
	    ic->private.local.context = p->succession;
	    return(True);
	} else { /* Terminate (reached to leaf) */
	    ic->private.local.composed = p;
	    /* return back to client KeyPressEvent keycode == 0 */
	    ev->xkey.keycode = 0;
	    XPutBackEvent(d, ev);
	    /* initialize internal state for next key sequence */
	    ic->private.local.context = ((Xim)ic->core.im)->private.local.top;
	    return(True);
	}
    } else { /* Unmatched */
	if(ic->private.local.context == ((Xim)ic->core.im)->private.local.top) {
	    return(False);
	}
	/* Error (Sequence Unmatch occured) */
	/* initialize internal state for next key sequence */
	ic->private.local.context = ((Xim)ic->core.im)->private.local.top;
	return(True);
    }
}
