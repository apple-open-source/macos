/* $Xorg: XimTrans.h,v 1.3 2000/08/17 19:45:05 cpqbld Exp $ */
/******************************************************************

           Copyright 1992 by Sun Microsystems, Inc.
           Copyright 1992, 1993, 1994 by FUJITSU LIMITED

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Sun Microsystems, Inc.
and FUJITSU LIMITED not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.
Sun Microsystems, Inc. and FUJITSU LIMITED makes no representations about
the suitability of this software for any purpose.
It is provided "as is" without express or implied warranty.

Sun Microsystems Inc. AND FUJITSU LIMITED DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL Sun Microsystems, Inc. AND FUJITSU LIMITED
BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

  Author: Hideki Hiura (hhiura@Sun.COM) Sun Microsystems, Inc.
          Takashi Fujiwara     FUJITSU LIMITED 
                               fujiwara@a80.tech.yk.fujitsu.co.jp

******************************************************************/

#ifndef _XIMTRANS_H
#define _XIMTRANS_H

typedef struct _TransIntrCallbackRec	*TransIntrCallbackPtr;

typedef struct _TransIntrCallbackRec {
    Bool			(*func)(
#if NeedNestedPrototypes
					Xim, INT16, XPointer, XPointer
#endif
					);
    XPointer			 call_data;
    TransIntrCallbackPtr	 next;
} TransIntrCallbackRec ;

typedef struct {
    TransIntrCallbackPtr	 intr_cb;
    struct _XtransConnInfo 	*trans_conn; /* transport connection object */
    int				 fd;
    char			*address;
    Window			 window;
    Bool			 is_putback;
} TransSpecRec;


/*
 * Prototypes
 */

extern Bool _XimTransIntrCallback(
#if NeedFunctionPrototypes
    Xim		 im,
    Bool	 (*callback)(
#if NeedNestedPrototypes
			     Xim, INT16, XPointer, XPointer
#endif
			     ),
    XPointer	 call_data
#endif
);

extern void _XimFreeTransIntrCallback(
#if NeedFunctionPrototypes
    Xim		 im
#endif
);

extern Bool _XimTransIntrCallbackCheck(
#if NeedFunctionPrototypes
    Xim		 im,
    INT16	 len,
    XPointer	 data
#endif
);

extern Bool _XimTransFilterWaitEvent(
#if NeedFunctionPrototypes
    Display	*d,
    Window	 w,
    XEvent	*ev,
    XPointer	 arg
#endif
);

extern void _XimTransInternalConnection(
#if NeedFunctionPrototypes
    Display	*d,
    int		 fd,
    XPointer	 arg
#endif
);

extern Bool _XimTransWrite(
#if NeedFunctionPrototypes
    Xim		 im,
    INT16	 len,
    XPointer	 data
#endif
);

extern Bool _XimTransRead(
#if NeedFunctionPrototypes
    Xim		 im,
    XPointer	 recv_buf,
    int		 buf_len,
    int		*ret_len
#endif
);

extern void _XimTransFlush(
#if NeedFunctionPrototypes
    Xim		 im
#endif
);

#endif /* _XIMTRANS__H */
