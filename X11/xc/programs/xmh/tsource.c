/* $XConsortium: tsource.c,v 2.24 91/10/21 14:32:36 eswu Exp $ */

/*
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
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT RIGHTS,
 * APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN ADDITION TO THAT
 * SET FORTH ABOVE.
 *
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting documentation,
 * and that the name of Digital Equipment Corporation not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 */
/* $XFree86: xc/programs/xmh/tsource.c,v 1.4 2002/04/07 03:57:46 tsi Exp $ */

/* File: tsource.c -- the code for a toc source */

#include "xmh.h"
#include "tocintrnl.h"
#include <X11/Xatom.h>
#include "tsourceP.h"

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

#define Offset(field) XtOffsetOf(TocSourceRec, toc_source.field)

static XtResource resources[] = {
    {XtNtoc, XtCToc, XtRPointer, sizeof(caddr_t), 
       Offset(toc), XtRPointer, NULL},
};

#undef Offset

static void Initialize(Widget, Widget, ArgList, Cardinal *num_args);
static XawTextPosition Read(Widget, XawTextPosition, XawTextBlock *, int);
static XawTextPosition Scan(Widget, XawTextPosition, XawTextScanType, XawTextScanDirection, int, Bool);
static XawTextPosition Search(Widget, XawTextPosition, XawTextScanDirection, XawTextBlock *);
static int Replace(Widget, XawTextPosition, XawTextPosition, XawTextBlock *);

#define SuperClass		(&textSrcClassRec)
TocSourceClassRec tocSourceClassRec = {
  {
/* core_class fields */	
    /* superclass	  	*/	(WidgetClass) SuperClass,
    /* class_name	  	*/	"TocSrc",
    /* widget_size	  	*/	sizeof(TocSourceRec),
    /* class_initialize   	*/	NULL,
    /* class_part_initialize	*/	NULL,
    /* class_inited       	*/	FALSE,
    /* initialize	  	*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize		  	*/	NULL,
    /* actions		  	*/	NULL,
    /* num_actions	  	*/	0,
    /* resources	  	*/	resources,
    /* num_resources	  	*/	XtNumber(resources),
    /* xrm_class	  	*/	NULLQUARK,
    /* compress_motion	  	*/	FALSE,
    /* compress_exposure  	*/	FALSE,
    /* compress_enterleave	*/	FALSE,
    /* visible_interest	  	*/	FALSE,
    /* destroy		  	*/	NULL,
    /* resize		  	*/	NULL,
    /* expose		  	*/	NULL,
    /* set_values	  	*/	NULL,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	NULL,
    /* get_values_hook		*/	NULL,
    /* accept_focus	 	*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private   	*/	NULL,
    /* tm_table		   	*/	NULL,
    /* query_geometry		*/	NULL,
    /* display_accelerator	*/	NULL,
    /* extension		*/	NULL
  },
/* textSrc_class fields */
  {
    /* Read                     */      Read,
    /* Replace                  */      Replace,
    /* Scan                     */      Scan,
    /* Search                   */      Search,
    /* SetSelection             */      XtInheritSetSelection,
    /* ConvertSelection         */      XtInheritConvertSelection
  },
/* toc_source_class fields */
  {
    /* keeping the compiler happy */	0
  }
};

WidgetClass tocSourceWidgetClass = (WidgetClass)&tocSourceClassRec;

/************************************************************
 *
 * Class specific methods.
 *
 ************************************************************/

Msg MsgFromPosition(
    Toc toc,
    XawTextPosition position,
    XawTextScanDirection dir)
{
    Msg msg;
    int     h, l, m;
    if (position > toc->lastPos) position = toc->lastPos;
    if (dir == XawsdLeft) position--;
    l = 0;
    h = toc->nummsgs - 1;
    while (l < h - 1) {
	m = (l + h) / 2;
	if (toc->msgs[m]->position > position)
	    h = m;
	else
	    l = m;
    }
    msg = toc->msgs[h];
    if (msg->position > position)
	msg = toc->msgs[h = l];
    while (!msg->visible)
	msg = toc->msgs[h++];
    if (position < msg->position || position > msg->position + msg->length)
	Punt("Error in MsgFromPosition!");
    return msg;
}


static XawTextPosition
CoerceToLegalPosition(Toc toc, XawTextPosition position)
{
    return (position < 0) ? 0 :
		 ((position > toc->lastPos) ? toc->lastPos : position);
}

static XawTextPosition
Read(
    Widget w,
    XawTextPosition position,
    XawTextBlock *block,
    int length)
{
    TocSourceWidget source = (TocSourceWidget) w;
    Toc toc = source->toc_source.toc;
    Msg msg;
    int count;

    if (position < toc->lastPos) {
        block->firstPos = position;
	msg = MsgFromPosition(toc, position, XawsdRight);
	block->ptr = msg->buf + (position - msg->position);
	count = msg->length - (position - msg->position);
	block->length = (count < length) ? count : length;
	position += block->length;
    }
    else {
        block->firstPos = 0;
	block->length = 0;
	block->ptr = "";
    }
    block->format = FMT8BIT;
    return position;
}

/* Right now, we can only replace a piece with another piece of the same size,
   and it can't cross between lines. */

static int 
Replace(
    Widget w,
    XawTextPosition startPos,
    XawTextPosition endPos,
    XawTextBlock *block)
{
    TocSourceWidget source = (TocSourceWidget) w;
    Toc toc = source->toc_source.toc;
    Msg msg;
    int i;

    if (block->length != endPos - startPos)
	return XawEditError;
    msg = MsgFromPosition(toc, startPos, XawsdRight);
    for (i = 0; i < block->length; i++)
	msg->buf[startPos - msg->position + i] = block->ptr[i];
/*    for (i=0 ; i<toc->numwidgets ; i++)
	XawTextInvalidate(toc->widgets[i], startPos, endPos);
*
* CDP 9/1/89 
*/
    return XawEditDone;
}


#define Look(ti, c)\
{									\
    if ((dir == XawsdLeft && ti <= 0) ||				\
	    (dir == XawsdRight && ti >= toc->lastPos))		\
	c = 0;								\
    else {								\
	if (ti + doff < msg->position ||				\
		ti + doff >= msg->position + msg->length)		\
	    msg = MsgFromPosition(toc, ti, dir);			\
	c = msg->buf[ti + doff - msg->position];			\
    }									\
}



static XawTextPosition 
Scan(
    Widget w,
    XawTextPosition position,
    XawTextScanType sType,
    XawTextScanDirection dir,
    int count,
    Bool include)
{
    TocSourceWidget source = (TocSourceWidget) w;
    Toc toc = source->toc_source.toc;
    XawTextPosition textindex;
    Msg msg;
    char    c;
    int     ddir, doff, i, whiteSpace = 0;
    ddir = (dir == XawsdRight) ? 1 : -1;
    doff = (dir == XawsdRight) ? 0 : -1;

    if (toc->lastPos == 0) return 0;
    textindex = position;
    if (textindex + doff < 0) return 0;
    if (dir == XawsdRight && textindex >= toc->lastPos) return toc->lastPos;
    msg = MsgFromPosition(toc, textindex, dir);
    switch (sType) {
	case XawstPositions:
	    if (!include && count > 0)
		count--;
	    textindex = CoerceToLegalPosition(toc, textindex + count * ddir);
	    break;
	case XawstWhiteSpace:
	    for (i = 0; i < count; i++) {
		whiteSpace = -1;
		while (textindex >= 0 && textindex <= toc->lastPos) {
		    Look(textindex, c);
		    if ((c == ' ') || (c == '\t') || (c == '\n')) {
			if (whiteSpace < 0) whiteSpace = textindex;
		    } else if (whiteSpace >= 0)
			break;
		    textindex += ddir;
		}
	    }
	    if (!include) {
		if (whiteSpace < 0 && dir == XawsdRight)
		    whiteSpace = toc->lastPos;
		textindex = whiteSpace;
	    }
	    textindex = CoerceToLegalPosition(toc, textindex);
	    break;
	case XawstEOL:
	case XawstParagraph:
	    for (i = 0; i < count; i++) {
		while (textindex >= 0 && textindex <= toc->lastPos) {
		    Look(textindex, c);
		    if (c == '\n')
			break;
		    textindex += ddir;
		}
		if (i < count - 1)
		    textindex += ddir;
	    }
	    if (include)
		textindex += ddir;
	    textindex = CoerceToLegalPosition(toc, textindex);
	    break;
	case XawstAll:
	    if (dir == XawsdLeft)
		textindex = 0;
	    else
		textindex = toc->lastPos;
	    break;
	default:
	    break;
    }
    return textindex;
}

/*ARGSUSED*/
static XawTextPosition Search(
    Widget			w,
    XawTextPosition		position,
    XawTextScanDirection	direction,
    XawTextBlock		*block)
{
    /* TocSourceWidget source = (TocSourceWidget) w;
     * Toc toc = source->toc_source.toc;
     * not implemented
     */
    return XawTextSearchError;
}

/* Public definitions. */

/* ARGSUSED*/
static void Initialize(
    Widget request,
    Widget new,
    ArgList args,
    Cardinal *num_args)
{
    Toc toc;
    TocSourceWidget source = (TocSourceWidget) new;

    source->text_source.edit_mode = XawtextRead; /* force read only. */

    toc = source->toc_source.toc;

    toc->hasselection = FALSE;
    toc->left = toc->right = 0;
}

void
TSourceInvalid(Toc toc, XawTextPosition position, int length)
{
  XawTextInvalidate(XtParent(toc->source), position, 
		    (XawTextPosition) position+length-1);
}
