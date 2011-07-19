/*
 * $XTermId: proto.h,v 1.10 2003/10/27 01:07:57 tom Exp $
 * ----------------------------------------------------------------------------
 * this file is part of xterm
 *
 * Copyright 1996-2008,2003 by Thomas E. Dickey
 * 
 *                         All Rights Reserved
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 * ----------------------------------------------------------------------------
 */

#ifndef included_proto_h
#define included_proto_h

#define PROTO_XT_ACTIONS_ARGS \
	(Widget w, XEvent *event, String *params, Cardinal *num_params)

#define PROTO_XT_CALLBACK_ARGS \
	(Widget gw, XtPointer closure, XtPointer data)

#define PROTO_XT_CVT_SELECT_ARGS \
	(Widget w, Atom *selection, Atom *target, Atom *type, XtPointer *value, unsigned long *length, int *format)

#define PROTO_XT_EV_HANDLER_ARGS \
	(Widget w, XtPointer closure, XEvent *event, Boolean *cont)

#define PROTO_XT_SEL_CB_ARGS \
	(Widget w, XtPointer client_data, Atom *selection, Atom *type, XtPointer value, unsigned long *length, int *format)

#ifdef SIGNALRETURNSINT
#define SIGNAL_T int
#define SIGNAL_RETURN return 0
#else
#define SIGNAL_T void
#define SIGNAL_RETURN return
#endif

#endif/*included_proto_h*/
