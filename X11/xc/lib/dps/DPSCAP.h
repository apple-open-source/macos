/*
 * DPSCAP.h -- CAP constants, analogous to X.h
 *  		 
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
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

#ifndef _DPSCAP_h
#define _DPSCAP_h

/* DPSCAPConnSetup functional hint flags */

#define DPSCAPNONEFLAG		0x0000

/* Values for DPSCAPNotify */

#define DPSCAPNOTE_GRAB		0
#define DPSCAPNOTE_UNGRAB	1
#define DPSCAPNOTE_FREEGC	2
#define DPSCAPNOTE_SYNC		3
#define DPSCAPNOTE_PAUSE	4

/* Minor opcodes for CAP errors */

#define DPSCAPDEADLOCK		0

/* Pnames for ClientMessage type atoms */


#define DPSCAP_TYPE_PSOUTPUT	 "_ADOBE_DPSCAP_TYPE_PSOUTPUT"
#define DPSCAP_TYPE_PSOUTPUT_LEN "_ADOBE_DPSCAP_TYPE_PSOUTPUT_WITH_LEN"
#define DPSCAP_TYPE_PSSTATUS	 "_ADOBE_DPSCAP_TYPE_PSSTATUS"
#define DPSCAP_TYPE_NOOP	 "_ADOBE_DPSCAP_TYPE_NOOP"
#define DPSCAP_TYPE_SYNC	 "_ADOBE_DPSCAP_TYPE_SYNC"
#define DPSCAP_TYPE_XERROR	 "_ADOBE_DPSCAP_TYPE_XERROR"
#define DPSCAP_TYPE_PSREADY	 "_ADOBE_DPSCAP_TYPE_PSREADY"
#define DPSCAP_TYPE_RESUME       "_ADOBE_DPSCAP_TYPE_RESUME"

#define DPSCAP_ATOMS		8

/* Translate argument names for X_CAPSetArg, see dpsNXargs.h */

#define DPSCAP_ARG_SMALLFONTS	1
#define DPSCAP_ARG_PIXMEM	2

#endif /* _DPSCAP_h */
