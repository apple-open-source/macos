/*
 * dpsint.h	-- internal definitions to dpsclient.c
 *
 * (c) Copyright 1989-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */

#ifndef DPSINT_H
#define DPSINT_H

#include <DPS/dpsXclient.h>

#include "publictypes.h"
#include "dpsassert.h"

#include "dpsprivate.h"
#include "dpsXint.h"
#include "dpsdict.h"

typedef struct _t_ContextBufferRec {
  struct _t_ContextBufferRec *next;
  } ContextBufferRec, *ContextBuffer;

typedef struct {
  ContextBuffer gcontextBuffers;
  integer gqueuedBuffers;
  DPSProcs gctxProcs, gtextCtxProcs, gdummyCtxProcs;
  DPSSpaceProcs gspaceProcs;
  DPSPrivSpace gspaces;
  char **guserNames;
  integer guserNamesLength;
  PSWDict guserNameDict;
  boolean gclientStarted;
  DPSContext gdummyCtx;
  integer ginitFailed, gglobLastNameIndex;
  DPSPrivSpace gTextSpace;
} GlobalsRec, *Globals;

extern Globals DPSglobals;

#define contextBuffers DPSglobals->gcontextBuffers
#define queuedBuffers DPSglobals->gqueuedBuffers
#define ctxProcs DPSglobals->gctxProcs
#define textCtxProcs DPSglobals->gtextCtxProcs
#define dummyCtxProcs DPSglobals->gdummyCtxProcs
#define spaceProcs DPSglobals->gspaceProcs
#define spaces DPSglobals->gspaces
#define userNames DPSglobals->guserNames
#define userNamesLength DPSglobals->guserNamesLength
#define userNameDict DPSglobals->guserNameDict
#define clientStarted DPSglobals->gclientStarted
#define dummyCtx DPSglobals->gdummyCtx
#define initFailed DPSglobals->ginitFailed
#define globLastNameIndex DPSglobals->gglobLastNameIndex
#define textSpace DPSglobals->gTextSpace

extern boolean DPSCheckShared(DPSPrivContext ctxt);
extern boolean DPSKnownContext(DPSContext ctxt);
extern boolean DPSKnownSpace(DPSSpace space);
extern boolean DPSPrivateCheckWait(DPSContext ctxt);
extern void DPSCheckInitClientGlobals(void);
extern void DPSPrivateDestroyContext(DPSContext ctxt);
extern void DPSPrivateDestroySpace(DPSSpace space);
extern void DPSSafeSetLastNameIndex(DPSContext ctxt);
extern void DPSclientPrintProc(DPSContext ctxt, char *buf, unsigned nch);
extern void DPSinnerProcWriteData(DPSContext ctxt, char *buf, unsigned int count);

extern void DPSDefaultPrivateHandler(
  DPSContext ctxt,
  DPSErrorCode errorCode,
  long unsigned int arg1,
  long unsigned int arg2,
  char *prefix,
  char *suffix);

extern char *DPScalloc(integer e, integer n);

extern DPSContext DPSCreateContext(
  char *wh,
  DPSTextProc textProc,
  DPSErrorProc errorProc,
  DPSSpace space);

extern void DPSHandleBogusError(DPSContext ctxt, char *prefix, char *suffix);
extern void DPSInitCommonContextProcs(DPSProcs p);
extern void DPSInitCommonSpaceProcs(DPSSpaceProcs p);
extern void DPSInitCommonTextContextProcs(DPSProcs p);
extern void DPSInitPrivateContextFields(DPSPrivContext c, DPSPrivSpace s);
extern void DPSInitPrivateContextProcs(DPSProcs p);
extern void DPSInitPrivateSpaceFields(DPSPrivSpace s);
extern void DPSInitPrivateTextContextFields(DPSPrivContext c, DPSPrivSpace s);
extern void DPSServicePostScript(boolean (*returnControl)(void));

#endif /* DPSINT_H */
