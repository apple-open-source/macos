/* $XConsortium: bboxint.h,v 2.10 89/09/15 16:10:22 converse Exp $ 
 *
 *			  COPYRIGHT 1987
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */

/* Includes for modules implementing buttonbox stuff. */

#ifndef _bboxint_h
#define _bboxint_h

typedef struct _ButtonRec {
    Widget	widget;		/* Widget containing this button. */
    ButtonBox	buttonbox;	/* Button box containing this button. */
    char	*name;		/* Name of the button. */
    Widget	menu;		/* Menu widget, for menu buttons only */
} ButtonRec;

typedef struct _XmhButtonBoxRec {
    Widget	outer;		/* Widget containing scollbars & inner */
    Widget	inner;		/* Widget containing the buttons. */
    Scrn	scrn;		/* Scrn containing this button box. */
    int		numbuttons;	/* How many buttons in this box. */
    Button	*button;	/* Array of pointers to buttons. */
} ButtonBoxRec;

#endif /* _bboxint_h */
