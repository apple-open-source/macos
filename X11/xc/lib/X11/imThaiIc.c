/* $Xorg: imThaiIc.c,v 1.3 2000/08/17 19:45:15 cpqbld Exp $ */
/******************************************************************

          Copyright 1992, 1993, 1994 by FUJITSU LIMITED
          Copyright 1993 by Digital Equipment Corporation

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of FUJITSU LIMITED and
Digital Equipment Corporation not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.  FUJITSU LIMITED and Digital Equipment Corporation
makes no representations about the suitability of this software for
any purpose.  It is provided "as is" without express or implied
warranty.

FUJITSU LIMITED AND DIGITAL EQUIPMENT CORPORATION DISCLAIM ALL 
WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL 
FUJITSU LIMITED AND DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR 
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES 
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER 
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF 
THIS SOFTWARE.

  Author:    Takashi Fujiwara     FUJITSU LIMITED 
                               	  fujiwara@a80.tech.yk.fujitsu.co.jp
  Modifier:  Franky Ling          Digital Equipment Corporation
	                          frankyling@hgrd01.enet.dec.com

******************************************************************/
/* $XFree86: xc/lib/X11/imThaiIc.c,v 1.4 2001/01/17 19:41:52 dawes Exp $ */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include "Xlibint.h"
#include "Xlcint.h"
#include "Ximint.h"

Private void
_XimThaiUnSetFocus(xic)
    XIC	 xic;
{
    Xic  ic = (Xic)xic;
    ((Xim)ic->core.im)->private.local.current_ic = (XIC)NULL;

    if (ic->core.focus_window)
	_XUnregisterFilter(ic->core.im->core.display, ic->core.focus_window,
			_XimThaiFilter, (XPointer)ic);
    return;
}

Private void
_XimThaiDestroyIC(xic)
    XIC	 xic;
{
    Xic	 ic = (Xic)xic;
    if(((Xim)ic->core.im)->private.local.current_ic == (XIC)ic) {
	_XimThaiUnSetFocus(xic);
    }
    if(ic->private.local.ic_resources) {
	Xfree(ic->private.local.ic_resources);
	ic->private.local.ic_resources = NULL;
    }

    Xfree(ic->private.local.context->mb);
    Xfree(ic->private.local.context->wc);
    Xfree(ic->private.local.context->utf8);
    Xfree(ic->private.local.context);
    Xfree(ic->private.local.composed->mb);
    Xfree(ic->private.local.composed->wc);
    Xfree(ic->private.local.composed->utf8);
    Xfree(ic->private.local.composed);
    return;
}

Private void
_XimThaiSetFocus(xic)
    XIC	 xic;
{
    Xic	 ic = (Xic)xic;
    XIC	 current_ic = ((Xim)ic->core.im)->private.local.current_ic;

    if (current_ic == (XIC)ic)
	return;

    if (current_ic != (XIC)NULL) {
	_XimThaiUnSetFocus(current_ic);
    }
    ((Xim)ic->core.im)->private.local.current_ic = (XIC)ic;

    if (ic->core.focus_window)
	_XRegisterFilterByType(ic->core.im->core.display, ic->core.focus_window,
			KeyPress, KeyPress, _XimThaiFilter, (XPointer)ic);
    return;
}

Private void
_XimThaiReset(xic)
    XIC	 xic;
{
    Xic	 ic = (Xic)xic;
    ic->private.local.thai.comp_state = 0;
    ic->private.local.thai.keysym = 0;
    ic->private.local.composed->mb[0] = '\0';
    ic->private.local.composed->wc[0] = 0;
    ic->private.local.composed->utf8[0] = '\0';
}

Private char *
_XimThaiMbReset(xic)
    XIC	 xic;
{
    _XimThaiReset(xic);
    return (char *)NULL;
}

Private wchar_t *
_XimThaiWcReset(xic)
    XIC	 xic;
{
    _XimThaiReset(xic);
    return (wchar_t *)NULL;
}

Private XICMethodsRec Thai_ic_methods = {
    _XimThaiDestroyIC, 	/* destroy */
    _XimThaiSetFocus,  	/* set_focus */
    _XimThaiUnSetFocus,	/* unset_focus */
    _XimLocalSetICValues,	/* set_values */
    _XimLocalGetICValues,	/* get_values */
    _XimThaiMbReset,		/* mb_reset */
    _XimThaiWcReset,		/* wc_reset */
    _XimThaiMbReset,		/* utf8_reset */
    _XimLocalMbLookupString,	/* mb_lookup_string */
    _XimLocalWcLookupString,	/* wc_lookup_string */
    _XimLocalUtf8LookupString	/* utf8_lookup_string */
};

XIC
_XimThaiCreateIC(im, values)
    XIM			 im;
    XIMArg		*values;
{
    Xic			 ic;
    XimDefICValues	 ic_values;
    XIMResourceList	 res;
    unsigned int	 num;
    int			 len;

    if((ic = (Xic)Xmalloc(sizeof(XicRec))) == (Xic)NULL) {
	return ((XIC)NULL);
    }
    bzero((char *)ic,      sizeof(XicRec));

    ic->methods = &Thai_ic_methods;
    ic->core.im = im;
    ic->core.filter_events = KeyPressMask;
    if ((ic->private.local.context = (DefTree *)Xmalloc(sizeof(DefTree)))
		== (DefTree *)NULL)
	goto Set_Error;
    if ((ic->private.local.context->mb = (char *)Xmalloc(10))
		== (char *)NULL)
	goto Set_Error;
    if ((ic->private.local.context->wc = (wchar_t *)Xmalloc(10*sizeof(wchar_t)))
		== (wchar_t *)NULL)
	goto Set_Error;
    if ((ic->private.local.context->utf8 = (char *)Xmalloc(10))
		== (char *)NULL)
	goto Set_Error;
    if ((ic->private.local.composed = (DefTree *)Xmalloc(sizeof(DefTree)))
	    == (DefTree *)NULL)
	goto Set_Error;
    if ((ic->private.local.composed->mb = (char *)Xmalloc(10))
		== (char *)NULL)
	goto Set_Error;
    if ((ic->private.local.composed->wc = (wchar_t *)Xmalloc(10*sizeof(wchar_t)))
		== (wchar_t *)NULL)
	goto Set_Error;
    if ((ic->private.local.composed->utf8 = (char *)Xmalloc(10))
		== (char *)NULL)
	goto Set_Error;

    ic->private.local.thai.comp_state = 0;
    ic->private.local.thai.keysym = 0;
    ic->private.local.thai.input_mode = 0;

    num = im->core.ic_num_resources;
    len = sizeof(XIMResource) * num;
    if((res = (XIMResourceList)Xmalloc(len)) == (XIMResourceList)NULL) {
	goto Set_Error;
    }
    (void)memcpy((char *)res, (char *)im->core.ic_resources, len);
    ic->private.local.ic_resources     = res;
    ic->private.local.ic_num_resources = num;

    bzero((char *)&ic_values, sizeof(XimDefICValues));
    if(_XimCheckLocalInputStyle(ic, (XPointer)&ic_values, values,
				 im->core.styles, res, num) == False) {
	goto Set_Error;
    }

    _XimSetICMode(res, num, ic_values.input_style);

    if(_XimSetICValueData(ic, (XPointer)&ic_values,
			ic->private.local.ic_resources,
			ic->private.local.ic_num_resources,
			values, XIM_CREATEIC, True)) {
	goto Set_Error;
    }
    if(_XimSetICDefaults(ic, (XPointer)&ic_values,
				XIM_SETICDEFAULTS, res, num) == False) {
	goto Set_Error;
    }
    ic_values.filter_events = KeyPressMask;
    _XimSetCurrentICValues(ic, &ic_values);

    return ((XIC)ic);

Set_Error :
    if (ic->private.local.ic_resources) {
	Xfree(ic->private.local.ic_resources);
    }
    Xfree(ic);
    return((XIC)NULL);
}
