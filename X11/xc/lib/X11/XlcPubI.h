/* $Xorg: XlcPubI.h,v 1.4 2000/12/12 12:44:05 coskrey Exp $ */
/*
 * Copyright 1992, 1993 by TOSHIBA Corp.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of TOSHIBA not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. TOSHIBA make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * TOSHIBA DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * TOSHIBA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author: Katsuhisa Yano	TOSHIBA Corp.
 *			   	mopi@osa.ilab.toshiba.co.jp
 */
/* $XFree86: xc/lib/X11/XlcPubI.h,v 3.9 2001/11/16 00:52:27 dawes Exp $ */

#ifndef _XLCPUBLICI_H_
#define _XLCPUBLICI_H_

#include "XlcPublic.h"

#define XLC_PUBLIC(lcd, x)	(((XLCdPublic) lcd->core)->pub.x)
#define XLC_PUBLIC_PART(lcd)	(&(((XLCdPublic) lcd->core)->pub))
#define XLC_PUBLIC_METHODS(lcd)	(&(((XLCdPublicMethods) lcd->methods)->pub))

/*
 * XLCd public methods
 */

typedef struct _XLCdPublicMethodsRec *XLCdPublicMethods;

typedef XLCd (*XlcPubCreateProc)(
    const char*		name,
    XLCdMethods		methods
);

typedef Bool (*XlcPubInitializeProc)(
    XLCd		lcd
);

typedef void (*XlcPubDestroyProc)(
    XLCd		lcd
);

typedef char* (*XlcPubGetValuesProc)(
    XLCd		lcd,
    XlcArgList		args,
    int			num_args
);

typedef void (*XlcPubGetResourceProc)(
    XLCd		lcd,
    const char*		category,
    const char*		_class,
    char***		value,
    int*		count
);

typedef struct _XLCdPublicMethodsPart {
    XLCdPublicMethods superclass;
    XlcPubCreateProc create;
    XlcPubInitializeProc initialize;
    XlcPubDestroyProc destroy;
    XlcPubGetValuesProc get_values;
    XlcPubGetResourceProc get_resource;
} XLCdPublicMethodsPart;

typedef struct _XLCdPublicMethodsRec {
    XLCdMethodsRec core;
    XLCdPublicMethodsPart pub;
} XLCdPublicMethodsRec;

/*
 * XLCd public data
 */

typedef struct _XLCdPublicPart {
    char *siname;			/* for _XlcMapOSLocaleName() */
    char *language;			/* language part of locale name */
    char *territory;			/* territory part of locale name */
    char *codeset;			/* codeset part of locale name */
    char *encoding_name;		/* encoding name */
    int mb_cur_max;			/* ANSI C MB_CUR_MAX */
    Bool is_state_depend;		/* state-depend encoding */
    const char *default_string;		/* for XDefaultString() */
    XPointer xlocale_db;
} XLCdPublicPart;

typedef struct _XLCdPublicRec {
    XLCdCoreRec core;	
    XLCdPublicPart pub;
} XLCdPublicRec, *XLCdPublic;

extern XLCdMethods _XlcPublicMethods;

_XFUNCPROTOBEGIN

extern XLCd _XlcCreateLC(
    const char*		name,
    XLCdMethods		methods
);

extern void _XlcDestroyLC(
    XLCd		lcd
);

/* Fills into a freshly created XlcCharSet the fields that can be inferred
   from the ESC sequence. These are side, char_size, set_size. */
extern Bool _XlcParseCharSet(
    XlcCharSet		charset
);

/* Creates a new XlcCharSet, given its name (including side suffix) and
   Compound Text ESC sequence (normally at most 4 bytes). */
extern XlcCharSet _XlcCreateDefaultCharSet(
    const char*		name,
    const char*		ct_sequence
);

extern XlcCharSet _XlcAddCT(
    const char*		name,
    const char*		ct_sequence
);

extern Bool _XlcInitCTInfo (void);

extern XrmMethods _XrmDefaultInitParseInfo(
    XLCd		lcd,
    XPointer*		state
);

extern int _XmbTextPropertyToTextList(
    XLCd		lcd,
    Display*		dpy,
    const XTextProperty* text_prop,
    char***		list_ret,
    int*		count_ret
);

extern int _XwcTextPropertyToTextList(
    XLCd		lcd,
    Display*		dpy,
    const XTextProperty* text_prop,
    wchar_t***		list_ret,
    int*		count_ret
);

extern int _Xutf8TextPropertyToTextList(
    XLCd		lcd,
    Display*		dpy,
    const XTextProperty* text_prop,
    char***		list_ret,
    int*		count_ret
);

extern int _XmbTextListToTextProperty(
#if NeedFunctionPrototypes
    XLCd		/* lcd */,
    Display*		/* dpy */,
    char**		/* list */,
    int			/* count */,
    XICCEncodingStyle	/* style */,
    XTextProperty*	/* text_prop */
#endif
);

extern int _XwcTextListToTextProperty(
#if NeedFunctionPrototypes
    XLCd		/* lcd */,
    Display*		/* dpy */,
    wchar_t**		/* list */,
    int			/* count */,
    XICCEncodingStyle	/* style */,
    XTextProperty*	/* text_prop */
#endif
);

extern int _Xutf8TextListToTextProperty(
#if NeedFunctionPrototypes
    XLCd		/* lcd */,
    Display*		/* dpy */,
    char**		/* list */,
    int			/* count */,
    XICCEncodingStyle	/* style */,
    XTextProperty*	/* text_prop */
#endif
);

extern void _XwcFreeStringList(
#if NeedFunctionPrototypes
    XLCd		/* lcd */,
    wchar_t**		/* list */
#endif
);

extern int _XlcResolveLocaleName(
    const char*		lc_name,
    XLCdPublicPart*	pub
);

extern int _XlcResolveI18NPath(
    char*		buf,
    int			buf_len
);

extern char *_XlcLocaleDirName(
#if NeedFunctionPrototypes
     char*             /* dir_name */,
     char*             /* lc_name */
#endif
);

extern XPointer _XlcCreateLocaleDataBase(
    XLCd		lcd
);

extern void _XlcDestroyLocaleDataBase(
    XLCd		lcd
);

extern void _XlcGetLocaleDataBase(
    XLCd		/* lcd */,
    const char*		/* category */,
    const char*		/* name */,
    char***		/* value */,
    int*		/* count */
);

_XFUNCPROTOEND

#endif  /* _XLCPUBLICI_H_ */
