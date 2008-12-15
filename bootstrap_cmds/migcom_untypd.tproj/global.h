/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1995, 1994, 1993, 1992, 1991, 1990  
 * Open Software Foundation, Inc. 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation, and that the name of ("OSF") or Open Software 
 * Foundation not be used in advertising or publicity pertaining to 
 * distribution of the software without specific, written prior permission. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL OSF BE LIABLE FOR ANY 
 * SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES 
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN 
 * ACTION OF CONTRACT, NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING 
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE 
 */
/*
 * OSF Research Institute MK6.1 (unencumbered) 1/31/1995
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * 91/08/28  11:16:56  jsb
 * 	Replaced ServerProcName with ServerDemux.
 * 	[91/08/13            rpd]
 * 
 * 	Removed Camelot and TrapRoutine support.
 * 	Changed MsgKind to MsgSeqno.
 * 	[91/08/12            rpd]
 * 
 * 91/06/26  14:39:32  rpd
 * 	Removed InitRoutineName.
 * 	[91/06/26            rpd]
 * 
 * 91/06/25  10:31:12  rpd
 * 	Added ServerHeaderFileName.
 * 	[91/05/22            rpd]
 * 
 * 91/02/05  17:54:29  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:54:12  mrt]
 * 
 * 90/06/19  23:00:54  rpd
 * 	Added UserFilePrefix.
 * 	[90/06/03            rpd]
 * 
 * 90/06/02  15:04:42  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:10:53  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 * 17-Sep-87  Bennet Yee (bsy) at Carnegie-Mellon University
 *	Added GenSymTab
 *
 * 16-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added CamelotPrefix
 *
 * 28-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#ifndef	_GLOBAL_H
#define	_GLOBAL_H

#include "type.h"

extern boolean_t BeQuiet;	/* no warning messages */
extern boolean_t BeVerbose;	/* summarize types, routines */
extern boolean_t BeDebug;	/* enters in the debug mode */
extern boolean_t UseMsgRPC;
extern boolean_t GenSymTab;
extern boolean_t UseEventLogger;
extern boolean_t BeAnsiC;
extern boolean_t ShortCircuit;
extern boolean_t UseRPCTrap;
extern boolean_t TestRPCTrap;

extern boolean_t IsKernelUser;
extern boolean_t IsKernelServer;

extern string_t RCSId;

extern string_t SubsystemName;
extern u_int SubsystemBase;

extern string_t MsgOption;
extern string_t WaitTime;
extern string_t ErrorProc;
extern string_t ServerPrefix;
extern string_t UserPrefix;
extern string_t ServerDemux;
extern string_t ServerImpl;
extern string_t ServerSubsys;
extern int MaxMessSizeOnStack;

extern int yylineno;
extern string_t yyinname;

extern void init_global();

extern string_t UserFilePrefix;
extern string_t UserHeaderFileName;
extern string_t ServerHeaderFileName;
extern string_t InternalHeaderFileName;
extern string_t DefinesHeaderFileName;
extern string_t UserFileName;
extern string_t ServerFileName;

extern void more_global();

extern char NewCDecl[];
extern char LintLib[];

#endif	/* _GLOBAL_H */
