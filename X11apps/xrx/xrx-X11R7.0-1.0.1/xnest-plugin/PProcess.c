/* $Xorg: PProcess.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#include "RxPlugin.h"
#include "XUrls.h"

/***********************************************************************
 * Process the given RxParams and make the RxReturnParams
 ***********************************************************************/
int
RxpProcessParams(PluginInstance* This, RxParams *in, RxReturnParams *out)
{
    /* init return struture */
    memset(out, 0, sizeof(RxReturnParams));
    out->x_ui_lbx = RxUndef;
    out->x_print_lbx = RxUndef;

    out->action = in->action;
    if (in->embedded != RxUndef)
	out->embedded = RxTrue; /* we do support embbeding! */
    else
	out->embedded = RxUndef;

    out->width = in->width;
    out->height = in->height;	

    if (in->ui[0] == XUI) {	/* X display needed */
        out->ui = GetXUrl(RxpXnestDisplay(This->display_num), NULL, in->action);

	if (in->x_ui_lbx != RxUndef)
	    out->x_ui_lbx = RxFalse; /* we do not support LBX */
	else
	    out->x_ui_lbx = RxUndef;
    }

    if (in->print[0] == XPrint)	{ /* XPrint server needed */
        out->print = NULL;

	if (in->x_print_lbx != RxUndef)
	    out->x_print_lbx = RxFalse; /* we do not support LBX */
	else
	    out->x_print_lbx = RxUndef;
    }
    return 0;
}
