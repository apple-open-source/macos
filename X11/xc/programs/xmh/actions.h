/*
 * $XConsortium: actions.h,v 1.10 94/04/17 20:24:00 converse Exp $
 *
Copyright (c) 1989  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 *
 */
/* $XFree86: xc/programs/xmh/actions.h,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

#define XMH_ACTION_ARGS Widget, XEvent *, String *, Cardinal *

	/* from compfuncs.c */

extern void	XmhResetCompose (XMH_ACTION_ARGS);
extern void	XmhSend (XMH_ACTION_ARGS);
extern void	XmhSave (XMH_ACTION_ARGS);

	/* from folder.c */

extern void	XmhClose (XMH_ACTION_ARGS);
extern void	XmhComposeMessage (XMH_ACTION_ARGS);
extern void 	XmhOpenFolder (XMH_ACTION_ARGS);
extern void	XmhOpenFolderInNewWindow (XMH_ACTION_ARGS);
extern void	XmhCreateFolder (XMH_ACTION_ARGS);
extern void	XmhDeleteFolder (XMH_ACTION_ARGS);
extern void	XmhPopupFolderMenu (XMH_ACTION_ARGS);
extern void	XmhSetCurrentFolder (XMH_ACTION_ARGS);
extern void	XmhLeaveFolderButton (XMH_ACTION_ARGS);
extern void 	XmhPushFolder (XMH_ACTION_ARGS);
extern void	XmhPopFolder (XMH_ACTION_ARGS);
extern void	XmhWMProtocols (XMH_ACTION_ARGS);

	/* from msg.c */

extern void	XmhInsert (XMH_ACTION_ARGS);

	/* from popup.c */

extern void	XmhPromptOkayAction (XMH_ACTION_ARGS);

	/* from toc.c */

extern void	XmhPushSequence (XMH_ACTION_ARGS);
extern void	XmhPopSequence (XMH_ACTION_ARGS);
extern void	XmhReloadSeqLists (XMH_ACTION_ARGS);

	/* from tocfuncs.c */

extern void	XmhCheckForNewMail (XMH_ACTION_ARGS);
extern void	XmhIncorporateNewMail (XMH_ACTION_ARGS);
extern void	XmhCommitChanges (XMH_ACTION_ARGS);
extern void	XmhPackFolder (XMH_ACTION_ARGS);
extern void	XmhSortFolder (XMH_ACTION_ARGS);
extern void	XmhForceRescan (XMH_ACTION_ARGS);
extern void	XmhViewNextMessage (XMH_ACTION_ARGS);
extern void	XmhViewPreviousMessage (XMH_ACTION_ARGS);
extern void	XmhMarkDelete (XMH_ACTION_ARGS);
extern void	XmhMarkMove (XMH_ACTION_ARGS);
extern void	XmhMarkCopy (XMH_ACTION_ARGS);
extern void	XmhUnmark (XMH_ACTION_ARGS);
extern void	XmhViewInNewWindow (XMH_ACTION_ARGS);
extern void	XmhReply (XMH_ACTION_ARGS);
extern void	XmhForward (XMH_ACTION_ARGS);
extern void	XmhUseAsComposition (XMH_ACTION_ARGS);
extern void	XmhPrint (XMH_ACTION_ARGS);
extern void	XmhShellCommand (XMH_ACTION_ARGS);
extern void	XmhPickMessages (XMH_ACTION_ARGS);
extern void	XmhOpenSequence (XMH_ACTION_ARGS);
extern void	XmhAddToSequence (XMH_ACTION_ARGS);
extern void	XmhRemoveFromSequence (XMH_ACTION_ARGS);
extern void	XmhDeleteSequence (XMH_ACTION_ARGS);

	/* from viewfuncs.c */

extern void	XmhCloseView (XMH_ACTION_ARGS);
extern void	XmhViewReply (XMH_ACTION_ARGS);
extern void	XmhViewForward (XMH_ACTION_ARGS);
extern void	XmhViewUseAsComposition (XMH_ACTION_ARGS);
extern void	XmhEditView (XMH_ACTION_ARGS);
extern void	XmhSaveView (XMH_ACTION_ARGS);
extern void	XmhPrintView (XMH_ACTION_ARGS);
extern void	XmhViewMarkDelete (XMH_ACTION_ARGS);
