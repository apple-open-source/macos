/* $XConsortium: tocutil.h,v 2.7 95/01/09 16:52:53 swick Exp $ */
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
 * copyright notice and this permission notice appear in supporting documen-
 * tation, and that the name of Digital Equipment Corporation not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */
/* $XFree86: xc/programs/xmh/tocutil.h,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

#ifndef _tocutil_h
#define _tocutil_h

extern Toc	TUMalloc			(void);
extern int	TUScanFileOutOfDate		(Toc);
extern void	TUCheckSequenceMenu		(Toc);
extern void	TUScanFileForToc		(Toc);
extern int	TUGetMsgPosition		(Toc, Msg);
extern void	TUResetTocLabel			(Scrn);
extern void	TURedisplayToc			(Scrn);
extern void	TULoadSeqLists			(Toc);
extern void	TURefigureWhatsVisible		(Toc);
extern void	TULoadTocFile			(Toc);
extern void	TUSaveTocFile			(Toc);
extern void	TUEnsureScanIsValidAndOpen	(Toc, Boolean);
extern void	TURefigureTocPositions		(Toc);
extern void	TUGetFullFolderInfo		(Toc);
extern Msg	TUAppendToc			(Toc, char *);

#endif /* _tocutil_h */
