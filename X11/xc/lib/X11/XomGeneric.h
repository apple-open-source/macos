/* $Xorg: XomGeneric.h,v 1.3 2000/08/17 19:45:08 cpqbld Exp $ */
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
/*
 * Copyright 1995 by FUJITSU LIMITED
 * This is source code modified by FUJITSU LIMITED under the Joint
 * Development Agreement for the CDE/Motif PST.
 *
 * Modifier: Takanori Tateno   FUJITSU LIMITED
 *
 */
/* $XFree86: xc/lib/X11/XomGeneric.h,v 1.4 2001/01/17 19:41:50 dawes Exp $ */

#ifndef _XOMGENERIC_H_
#define _XOMGENERIC_H_

#include "XlcPublic.h"

#define XOM_GENERIC(om)		(&((XOMGeneric) om)->gen)
#define XOC_GENERIC(font_set)	(&((XOCGeneric) font_set)->gen)

/* For VW/UDC */
typedef struct _CodeRangeRec {
    unsigned long       start;
    unsigned long       end;
    unsigned long       dmy1;
    unsigned long       dmy2;
} CodeRangeRec, *CodeRange;

typedef struct _VRotateRec {
    char        *charset_name;  /* Charset name                         */
    XlcSide     side;           /* Encoding side                        */
    int         num_cr;
    CodeRange   code_range;
    char        *xlfd_name;
    XFontStruct *font;
} VRotateRec, *VRotate;

typedef enum {
    XOMMultiByte,
    XOMWideChar,
    XOMUtf8String
} XOMTextType;

typedef struct _FontDataRec {
    char 	*name;
    XlcSide 	side;
    /* For VW/UDC */
    int       	scopes_num;
    FontScope 	scopes;
    char      	*xlfd_name;
    XFontStruct *font;
} FontDataRec, *FontData;

#define VROTATE_NONE   0
#define VROTATE_PART   1
#define VROTATE_ALL    2

typedef struct _OMDataRec {
    int 	charset_count;
    XlcCharSet 	*charset_list;
    int 	font_data_count;
    FontData 	font_data;
    /* For VW/UDC */
    int substitute_num;
    FontData substitute;
    /* Vertical Writing */
    int         vmap_num;
    FontData    vmap;
    int		vrotate_type;
    int         vrotate_num;
    CodeRange   vrotate;
} OMDataRec, *OMData;

typedef struct _XOMGenericPart {
    int 	data_num;
    OMData 	data;
    Bool 	on_demand_loading;
    char 	*object_name;
} XOMGenericPart;

typedef struct _XOMGenericRec {
    XOMMethods 		methods;
    XOMCoreRec 		core;
    XOMGenericPart 	gen;
} XOMGenericRec, *XOMGeneric;

/*
 * XOC dependent data
 */

typedef struct _FontSetRec {
    int 		id;
    int 		charset_count;
    XlcCharSet 		*charset_list;
    int 		font_data_count;
    FontData 		font_data;
    char 		*font_name;
    XFontStruct 	*info;
    XFontStruct 	*font;
    XlcSide 		side;
    Bool 		is_xchar2b;
    /* For VW/UDC */
    int 		substitute_num;
    FontData 		substitute;
    /* Vertical Writing */
    int		vpart_initialize;
    int         vmap_num;
    FontData    vmap;
    int         vrotate_num;
    VRotate     vrotate;
} FontSetRec, *FontSet;

typedef struct _XOCGenericPart {
    XlcConv 		mbs_to_cs;
    XlcConv 		wcs_to_cs;
    XlcConv 		utf8_to_cs;
    int 		font_set_num;
    FontSet 		font_set;
} XOCGenericPart;

typedef struct _XOCGenericRec {
    XOCMethods 		methods;
    XOCCoreRec 		core;	
    XOCGenericPart 	gen;
} XOCGenericRec, *XOCGeneric;

_XFUNCPROTOBEGIN

extern XOM _XomGenericOpenOM(
#if NeedFunctionPrototypes
    XLCd		/* lcd */,
    Display*		/* dpy */,
    XrmDatabase		/* rdb */,
    _Xconst char*	/* res_name */,
    _Xconst char*	/* res_class */
#endif
);

extern XlcConv _XomInitConverter(
#if NeedFunctionPrototypes
    XOC			/* oc */,
    XOMTextType		/* type */
#endif
);

extern int _XomConvert(
#if NeedFunctionPrototypes
    XOC			/* oc */,
    XlcConv		/* conv */,
    XPointer*		/* from */,
    int*		/* from_left */,
    XPointer*		/* to */,
    int*		/* to_left */,
    XPointer*		/* args */,
    int			/* num_args */
#endif
);

_XFUNCPROTOEND

#endif  /* _XOMGENERIC_H_ */
