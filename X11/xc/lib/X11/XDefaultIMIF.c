/*
Copyright 1985, 1986, 1987, 1991, 1998  The Open Group

Portions Copyright 2000 Sun Microsystems, Inc. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions: The above copyright notice and this
permission notice shall be included in all copies or substantial
portions of the Software.


THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP OR SUN MICROSYSTEMS, INC. BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE EVEN IF
ADVISED IN ADVANCE OF THE POSSIBILITY OF SUCH DAMAGES.


Except as contained in this notice, the names of The Open Group and/or
Sun Microsystems, Inc. shall not be used in advertising or otherwise to
promote the sale, use or other dealings in this Software without prior
written authorization from The Open Group and/or Sun Microsystems,
Inc., as applicable.


X Window System is a trademark of The Open Group

OSF/1, OSF/Motif and Motif are registered trademarks, and OSF, the OSF
logo, LBX, X Window System, and Xinerama are trademarks of the Open
Group. All other trademarks and registered trademarks mentioned herein
are the property of their respective owners. No right, title or
interest in or to any trademark, service mark, logo or trade name of
Sun Microsystems, Inc. or its licensors is granted.

*/
/* $XFree86: xc/lib/X11/XDefaultIMIF.c,v 1.2 2001/11/19 15:33:38 tsi Exp $ */

#include <stdio.h>
#define NEED_EVENTS
#include "Xlibint.h"
#include "Xlcint.h"
#include "XlcGeneric.h"

#ifndef MAXINT
#define MAXINT          (~((unsigned int)1 << (8 * sizeof(int)) - 1))
#endif /* !MAXINT */

typedef struct _StaticXIM *StaticXIM;

typedef struct _XIMStaticXIMRec {
    /* for CT => MB,WC converter */
    XlcConv		 ctom_conv;
    XlcConv		 ctow_conv;
} XIMStaticXIMRec;

typedef enum {
    CREATE_IC = 1,
    SET_ICVAL = 2,
    GET_ICVAL = 3
} XICOp_t;

typedef struct _StaticXIM {
    XIMMethods		methods;
    XIMCoreRec		core;
    XIMStaticXIMRec	*private;
} StaticXIMRec;

static Status _CloseIM(
#if NeedFunctionPrototypes
	XIM
#endif
);

static char *_SetIMValues(
#if NeedFunctionPrototypes
	XIM, XIMArg *
#endif
);

static char *_GetIMValues(
#if NeedFunctionPrototypes
	XIM, XIMArg*
#endif
);

static XIC _CreateIC(
#if NeedFunctionPrototypes
	XIM, XIMArg*
#endif
);

static _Xconst XIMMethodsRec local_im_methods = {
    _CloseIM,		/* close */
    _SetIMValues,	/* set_values */
    _GetIMValues, 	/* get_values */
    _CreateIC,		/* create_ic */
    NULL,		/* ctstombs */
    NULL		/* ctstowcs */
};

static void _DestroyIC(
#if NeedFunctionPrototypes
		       XIC
#endif
);
static void _SetFocus(
#if NeedFunctionPrototypes
		      XIC
#endif
);
static void _UnsetFocus(
#if NeedFunctionPrototypes
			XIC
#endif
);
static char* _SetICValues(
#if NeedFunctionPrototypes
			 XIC, XIMArg *
#endif
);
static char* _GetICValues(
#if NeedFunctionPrototypes
			 XIC, XIMArg *
#endif
);
static char *_MbReset(
#if NeedFunctionPrototypes
		      XIC
#endif
);
static wchar_t *_WcReset(
#if NeedFunctionPrototypes
			 XIC
#endif
);
static int _MbLookupString(
#if NeedFunctionPrototypes
	XIC, XKeyEvent *, char *, int, KeySym *, Status *
#endif
);
static int _WcLookupString(
#if NeedFunctionPrototypes
	XIC, XKeyEvent *, wchar_t *, int, KeySym *, Status *
#endif
);

static _Xconst XICMethodsRec local_ic_methods = {
    _DestroyIC, 	/* destroy */
    _SetFocus,		/* set_focus */
    _UnsetFocus,	/* unset_focus */
    _SetICValues,	/* set_values */
    _GetICValues,	/* get_values */
    _MbReset,		/* mb_reset */
    _WcReset,		/* wc_reset */
    NULL,		/* utf8_reset */		/* ??? */
    _MbLookupString,	/* mb_lookup_string */
    _WcLookupString,	/* wc_lookup_string */
    NULL		/* utf8_lookup_string */	/* ??? */
};

XIM
_XDefaultOpenIM(lcd, dpy, rdb, res_name, res_class)
XLCd		 lcd;
Display		*dpy;
XrmDatabase	 rdb;
char		*res_name, *res_class;
{
    StaticXIM im;
    XIMStaticXIMRec *local_impart;
    XlcConv ctom_conv, ctow_conv;
    int i;
    char *mod;
    char buf[BUFSIZ];

    if (!(ctom_conv = _XlcOpenConverter(lcd,
			XlcNCompoundText, lcd, XlcNMultiByte))) {
	return((XIM)NULL);
    }
	
    if (!(ctow_conv = _XlcOpenConverter(lcd,
			XlcNCompoundText, lcd, XlcNWideChar))) {
	return((XIM)NULL);
    }

    if ((im = (StaticXIM)Xmalloc(sizeof(StaticXIMRec))) == (StaticXIM)NULL) {
	return((XIM)NULL);
    }
    if ((local_impart = (XIMStaticXIMRec*)Xmalloc(sizeof(XIMStaticXIMRec)))
	== (XIMStaticXIMRec *)NULL) {
	Xfree(im);
	return((XIM)NULL);
    }
    memset(im, 0, sizeof(StaticXIMRec));
    memset(local_impart, 0, sizeof(XIMStaticXIMRec));

    buf[0] = '\0';
    i = 0;
    if ((lcd->core->modifiers) && (*lcd->core->modifiers)) {
#define	MODIFIER "@im="
	mod = strstr(lcd->core->modifiers, MODIFIER);
	if (mod) {
	    mod += strlen(MODIFIER);
	    while (*mod && *mod != '@' && i < BUFSIZ - 1) {
		buf[i++] = *mod++;
	    }
	    buf[i] = '\0';
	}
    }
#undef MODIFIER
    if ((im->core.im_name = Xmalloc(i+1)) == NULL)
	goto Error2;
    strcpy(im->core.im_name, buf);

    im->private = local_impart;
    im->methods        = (XIMMethods)&local_im_methods;
    im->core.lcd       = lcd;
    im->core.ic_chain  = (XIC)NULL;
    im->core.display   = dpy;
    im->core.rdb       = rdb;
    im->core.res_name  = NULL;
    im->core.res_class = NULL;

    local_impart->ctom_conv = ctom_conv;
    local_impart->ctow_conv = ctow_conv;

    if ((res_name != NULL) && (*res_name != '\0')){
	im->core.res_name  = (char *)Xmalloc(strlen(res_name)+1);
	strcpy(im->core.res_name,res_name);
    }
    if ((res_class != NULL) && (*res_class != '\0')){
	im->core.res_class = (char *)Xmalloc(strlen(res_class)+1);
	strcpy(im->core.res_class,res_class);
    }

    return (XIM)im;
Error2 :
    Xfree(im->private);
    Xfree(im->core.im_name);
    Xfree(im);
    _XlcCloseConverter(ctom_conv);
    _XlcCloseConverter(ctow_conv);
    return(NULL);
}

static Status
_CloseIM(xim)
XIM xim;
{
    StaticXIM im = (StaticXIM)xim;
    _XlcCloseConverter(im->private->ctom_conv);
    _XlcCloseConverter(im->private->ctow_conv);
    XFree(im->private);
    XFree(im->core.im_name);
    if (im->core.res_name) XFree(im->core.res_name);
    if (im->core.res_class) XFree(im->core.res_class);
    return 1; /*bugID 4163122*/
}

static char *
_SetIMValues(xim, arg)
XIM xim;
XIMArg *arg;
{
    return(arg->name);		/* evil */
}

static char *
_GetIMValues(xim, values)
XIM xim;
XIMArg *values;
{
    XIMArg *p;
    XIMStyles *styles;

    for (p = values; p->name != NULL; p++) {
	if (strcmp(p->name, XNQueryInputStyle) == 0) {
	    styles = (XIMStyles *)Xmalloc(sizeof(XIMStyles));
	    *(XIMStyles **)p->value = styles;
	    styles->count_styles = 1;
	    styles->supported_styles =
		(XIMStyle*)Xmalloc(styles->count_styles * sizeof(XIMStyle));
	    styles->supported_styles[0] = (XIMPreeditNone | XIMStatusNone);
	} else {
	    break;
	}
    }
    return (p->name);
}

static char*
_SetICValueData(ic, values, mode)
XIC ic;
XIMArg *values;
XICOp_t mode;
{
    XIMArg *p;
    char *return_name = NULL;

    for (p = values; p != NULL && p->name != NULL; p++) {
	if(strcmp(p->name, XNInputStyle) == 0) {
	    if (mode == CREATE_IC)
		ic->core.input_style = (XIMStyle)p->value;
	} else if (strcmp(p->name, XNClientWindow) == 0) {
	    ic->core.client_window = (Window)p->value ;
	} else if (strcmp(p->name, XNFocusWindow) == 0) {
	    ic->core.focus_window = (Window)p->value ;
	} else if (strcmp(p->name, XNPreeditAttributes) == 0
		   || strcmp(p->name, XNStatusAttributes) == 0) {
            return_name = _SetICValueData(ic, (XIMArg*)p->value, mode);
            if (return_name) break;
        } else {
            return_name = p->name;
            break;
        }
    }
    return(return_name);
}

static char*
_GetICValueData(ic, values, mode)
XIC ic;
XIMArg *values;
XICOp_t mode;
{
    XIMArg *p;
    char *return_name = NULL;

    for (p = values; p->name != NULL; p++) {
	if(strcmp(p->name, XNInputStyle) == 0) {
	    *((XIMStyle *)(p->value)) = ic->core.input_style;
	} else if (strcmp(p->name, XNClientWindow) == 0) {
	    *((Window *)(p->value)) = ic->core.client_window;
	} else if (strcmp(p->name, XNFocusWindow) == 0) {
	    *((Window *)(p->value)) = ic->core.focus_window;
	} else if (strcmp(p->name, XNFilterEvents) == 0) {
	    *((unsigned long *)(p->value))= ic->core.filter_events;
	} else if (strcmp(p->name, XNPreeditAttributes) == 0
		   || strcmp(p->name, XNStatusAttributes) == 0) {
	    return_name = _GetICValueData(ic, (XIMArg*)p->value, mode);
	    if (return_name) break;
	} else {
	    return_name = p->name;
	    break;
	}
    }
    return(return_name);
}

static XIC
_CreateIC(im, arg)
XIM im;
XIMArg *arg;
{
    XIC ic;

    if ((ic = (XIC)Xmalloc(sizeof(XICRec))) == (XIC)NULL) {
	return ((XIC)NULL);
    }
    memset(ic, 0, sizeof(XICRec));

    ic->methods = (XICMethods)&local_ic_methods;
    ic->core.im = im;
    ic->core.filter_events = KeyPressMask;

    if (_SetICValueData(ic, arg, CREATE_IC) != NULL)
	goto err_return;
    if (!(ic->core.input_style))
	goto err_return;

    return (XIC)ic;
err_return:
    XFree(ic);
    return ((XIC)NULL);
}

static  void
_DestroyIC(ic)
XIC ic;
{
/*BugId4255571. This Xfree() should be removed because XDestroyIC() still need ic after invoking _DestroyIC() and there is a XFree(ic) at the end of XDestroyIC() already.
   if(ic)
   	XFree(ic); */
}

static void
_SetFocus(ic)
XIC ic;
{
}

static void
_UnsetFocus(ic)
XIC ic;
{
}

static char*
_SetICValues(ic, args)
XIC ic;
XIMArg *args;
{
    char *ret = NULL;
    if (!ic) {
        return (args->name);
    }
    ret = _SetICValueData(ic, args, SET_ICVAL);
    return(ret);
}

static char*
_GetICValues(ic, args)
XIC ic;
XIMArg *args;
{
    char *ret = NULL;
    if (!ic) {
        return (args->name);
    }
    ret = _GetICValueData(ic, args, GET_ICVAL);
    return(ret);
}

static char *
_MbReset(xic)
XIC xic;
{
    return(NULL);
}

static wchar_t *
_WcReset(xic)
XIC xic;
{
    return(NULL);
}

static int
_MbLookupString(xic, ev, buffer, bytes, keysym, status)
XIC xic;
XKeyEvent *ev;
char * buffer;
int bytes;
KeySym *keysym;
Status *status;
{
    XComposeStatus NotSupportedYet ;
    int length;
    
    length = XLookupString(ev, buffer, bytes, keysym, &NotSupportedYet);

    if (keysym && *keysym == NoSymbol){
	*status = XLookupNone;
    } else if (length > 0) {
	*status = XLookupBoth;
    } else {
	*status = XLookupKeySym;
    }
    return(length);
}

static int
_WcLookupString(xic, ev, buffer, wlen, keysym, status)
XIC xic;
XKeyEvent *ev;
wchar_t * buffer;
int wlen;
KeySym *keysym;
Status *status;
{
    XComposeStatus NotSupportedYet ;
    int length;
    /* In single-byte, mb_len = wc_len */
    char *mb_buf = (char *)Xmalloc(wlen);

    length = XLookupString(ev, mb_buf, wlen, keysym, &NotSupportedYet);

    if (keysym && *keysym == NoSymbol){
	*status = XLookupNone;
    } else if (length > 0) {
	*status = XLookupBoth;
    } else {
	*status = XLookupKeySym;
    }
    mbstowcs(buffer, mb_buf, length);
    XFree(mb_buf);
    return(length);
}
