/* $XConsortium: tocintrnl.h,v 2.18 91/07/14 18:53:37 converse Exp $ */
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
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */

/* $XFree86: xc/programs/xmh/tocintrnl.h,v 1.2 2003/05/27 22:27:08 tsi Exp $ */

/* Includes for modules implementing toc stuff. */

#ifndef _tocinternal_h
#define _tocinternal_h

#include "xmh.h"
#include <X11/IntrinsicP.h>	/* %%% */
#include "tsource.h"

typedef enum {
    unknown, valid, invalid
} ValidType;

typedef struct _MsgRec {
    Toc		toc;		/* Which toc this message is in. */
    Toc		desttoc;	/* Folder to copy or move to (NULL if none) */
    Scrn	*scrn;		/* Scrns showing this message (if any) */
    Widget      source;		/* Source (if any) containing this msg. */
    XawTextPosition position;	/* Position in the scanfile for this msg. */
    XawTextPosition startPos;	/* Where to start the insertion point. */
    char	*buf;		/* The scanline for this message. */
    int		msgid;		/* Message id for this message. */
    short	length;		/* #/chars for this msg's entry in scanfile */
    unsigned char num_scrns;	/* How many scrns are currently showing msg */
    FateType	fate:2;		/* What will be done to this message */
    unsigned	changed:1;	/* True iff this entry needs to be saved */
    unsigned	visible:1;	/* Whether we should show this message */
    unsigned	temporary:1;	/* Whether we should delete this message when
				   it is no longer visible */
    unsigned	reapable:1;	/* True iff we don't need to keep this
				   composition around */
    unsigned	unused:2;
} MsgRec;

typedef struct _TocRec {
   Scrn		*scrn;		/* Scrns containing this table of contents. */
   Cardinal	num_scrns;	/* How many scrns are currently showing toc. */
   char 	*foldername;	/* Folder name for this toc */
   char		*path;		/* Full path to folder's directory. */
   char		*scanfile;	/* Full path to file containing scan. */
   Msg		curmsg;		/* Current msgid for this toc. */
   int		nummsgs;	/* How many info entries we currently have. */
   Msg		*msgs;		/* Array of pointers to info about each msg. */
   int		numsequences;	/* #/sequences defined for this folder. */
   Sequence	*seqlist;	/* Array of pointers to sequences. */
   Sequence 	viewedseq;	/* Seq currently shown (NULL == all msgs) */
   Sequence	selectseq;	/* The most recently selected sequence */
   Widget       source;		/* Source for the file containing info. */
   Boolean	hasselection;	/* Whether we own the selection. */
   XawTextPosition left, right;	/* Left and right extents of selection. */
   int		length;		/* #/chars in the scanfile. */
   int		origlength;	/* Original #/chars in the scanfile. */
   int		lastPos;	/* Last legal position */
   ValidType	validity;	/* Whether the scan file for this toc is */
				/* up to date. */
   Boolean	needsrepaint;	/* TRUE if we should repaint this toc. */
   Boolean	needscachesave;	/* TRUE if the cache needs saving. */
   Boolean	needslabelupdate;/* TRUE if the toclabel needs repainting. */
   Boolean	stopupdate;	/* Zero if refreshing; nonzero if refreshing is
				   currently inhibited. */
   Boolean	haschanged;	/* Whether it's changed in the process of */
				/* the current commit. */
   Boolean	delete_pending;	/* Is a delete folder operation pending? */
   Boolean	force_reset;	/* temporary bug work-around for sequences */
   char		*incfile;	/* Which file to incorporate from (if any). */
   int		mailpending;	/* True if we're currently displaying
				   mail pending true for this folder */
   long		lastreaddate;	/* Last time we read or wrote the cache. */
   Stack	sequence_stack;	/* Stack of sequence names. */
} TocRec;

#endif /* _tocinternal_h */
