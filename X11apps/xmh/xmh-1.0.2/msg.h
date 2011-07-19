/* $XConsortium: msg.h,v 2.7 89/07/20 21:12:59 converse Exp $ */
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
/* $XFree86$ */

#ifndef _msg_h
#define _msg_h

extern char *MsgFileName(Msg);
extern int MsgSaveChanges(Msg);
extern int MsgSetScrn(Msg, Scrn, XtCallbackList, XtCallbackList);
extern void MsgSetScrnForComp(Msg, Scrn);
extern void MsgSetScrnForce(Msg, Scrn);
extern void MsgSetFate(Msg, FateType, Toc);
extern FateType MsgGetFate(Msg, Toc *);
extern void MsgSetTemporary(Msg);
extern void MsgSetPermanent(Msg);
extern int MsgGetId(Msg);
extern char *MsgGetScanLine(Msg);
extern Toc MsgGetToc(Msg);
extern void MsgSetReapable(Msg);
extern void MsgClearReapable(Msg);
extern int MsgGetReapable(Msg);
extern void MsgSetEditable(Msg);
extern void MsgClearEditable(Msg);
extern int MsgGetEditable(Msg);
extern int MsgChanged(Msg);
extern void MsgSetCallOnChange(Msg, void (*)(XMH_CB_ARGS), XtPointer);
extern void MsgSend(Msg);
extern void MsgLoadComposition(Msg);
extern void MsgLoadReply(Msg, Msg, String *, Cardinal);
extern void MsgLoadForward(Scrn, Msg, MsgList, String *, Cardinal);
extern void MsgLoadCopy(Msg, Msg);
extern void MsgCheckPoint(Msg);
extern void MsgFree(Msg);

#endif /* _msg_h */
