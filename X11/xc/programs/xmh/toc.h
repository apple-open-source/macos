/*
 * $XConsortium: toc.h,v 2.13 91/07/17 12:28:29 converse Exp $
 *
 *
 *		       COPYRIGHT 1987, 1989
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
/* $XFree86: xc/programs/xmh/toc.h,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

#ifndef _toc_h
#define _toc_h

extern void	TocInit			(void);
extern Toc	TocCreate		(char *);
extern Toc	TocCreateFolder		(char *);
extern int	TocHasMail		(Toc);
extern void	TocCheckForNewMail	(Boolean);
extern Boolean	TocTestAndSetDeletePending(Toc);
extern void	TocClearDeletePending	(Toc);
extern void	TocDeleteFolder		(Toc);
extern void	TocSetScrn		(Toc, Scrn);

extern void	TocRemoveMsg		(Toc, Msg);
extern void	TocRecheckValidity	(Toc);
extern void	TocSetCurMsg		(Toc, Msg);
extern Msg	TocGetCurMsg		(Toc);
extern Msg	TocMsgAfter		(Toc, Msg);
extern Msg	TocMsgBefore		(Toc, Msg);
extern void	TocForceRescan		(Toc);

extern void	TocReloadSeqLists	(Toc);
extern int	TocHasSequences		(Toc);
extern void	TocChangeViewedSeq	(Toc, Sequence);
extern Sequence	TocViewedSequence	(Toc);
extern Sequence	TocGetSeqNamed		(Toc, char *);
extern void	TocSetSelectedSequence	(Toc, Sequence);
extern Sequence	TocSelectedSequence	(Toc);

extern MsgList	TocCurMsgList		(Toc);
extern void	TocUnsetSelection	(Toc);
extern Msg	TocMakeNewMsg		(Toc);
extern void	TocStopUpdate		(Toc);
extern void	TocStartUpdate		(Toc);
extern void	TocSetCacheValid	(Toc);

extern char *	TocMakeFolderName	(Toc);
extern char *	TocName			(Toc);
extern Toc	TocGetNamed		(char*);

extern int	TocConfirmCataclysm(Toc, XtCallbackList, XtCallbackList);
extern void	TocCommitChanges	(Widget, XtPointer, XtPointer);
extern int	TocCanIncorporate	(Toc);
extern int	TocIncorporate		(Toc);
extern void	TocMsgChanged		(Toc, Msg);
extern Msg	TocMsgFromId		(Toc, int);

#endif /* _toc_h */
