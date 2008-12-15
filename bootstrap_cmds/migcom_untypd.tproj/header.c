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
 * 91/08/28  11:16:58  jsb
 * 	Removed TrapRoutine support.
 * 	[91/08/12            rpd]
 * 
 * 91/06/26  14:39:37  rpd
 * 	Removed the user initialization function and InitRoutineName.
 * 	Fixed to use different symbolic constants to protect
 * 	the user and server header files against multiple inclusion.
 * 	[91/06/26            rpd]
 * 
 * 91/06/25  10:31:21  rpd
 * 	Restored prototype generation.
 * 	Changed WriteHeader to WriteUserHeader.
 * 	Added WriteServerHeader.
 * 	[91/05/23            rpd]
 * 
 * 91/02/05  17:54:33  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:54:19  mrt]
 * 
 * 90/12/20  17:04:47  jeffreyh
 * 	Commented out code for prototype generation. This is a temporary solution
 * 	to the longer term problem of the need 
 * 	for the generation of both a client and a server header file 
 * 	that have correct prototypes for strict ansi c and c++. The
 * 	prototypes generated before anly were for the client and broke kernel
 * 	files that included them if compiled under standard gcc
 * 	[90/12/07            jeffreyh]
 * 
 * 90/06/02  15:04:46  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:11:06  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 *  8-Jul-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Conditionally defined mig_external to be extern and then defined
 *	all functions  with the storage class mig_external.
 *	Mig_external can be changed
 *	when the generated code is compiled.
 *
 * 18-Jan-88  David Detlefs (dld) at Carnegie-Mellon University
 *	Modified to produce C++ compatible code via #ifdefs.
 *	All changes have to do with argument declarations.
 *
 *  3-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Revision to make multi-threaded use work. Removed definitions for
 * 	alloc_reply_port and init_msg_type as these routines are 
 * 	no longer generated.
 *
 * 30-Jul-87  Mary Thompson (mrt) at Carnegie-Mellon University
 * 	Made changes to generate conditional code for C++ parameter lists
 *
 * 29-Jul-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed WriteRoutine to produce conditional argument
 *	lists for C++
 *
 *  8-Jun-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed the KERNEL include from ../h to sys/
 *	Removed extern from WriteHeader to make hi-c happy
 *
 * 28-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#include "write.h"
#include "utils.h"
#include "global.h"
#include "strdefs.h"
#include "error.h"
#include <stdlib.h>

static void
WriteIncludes(file, isuser)
    FILE *file;
    boolean_t	isuser;
{
    fprintf(file, "#include <string.h>\n");
    fprintf(file, "#include <mach/ndr.h>\n");
    fprintf(file, "#include <mach/boolean.h>\n");
    fprintf(file, "#include <mach/kern_return.h>\n");
    fprintf(file, "#include <mach/notify.h>\n");
    fprintf(file, "#include <mach/mach_types.h>\n");
    fprintf(file, "#include <mach/message.h>\n");
    fprintf(file, "#include <mach/mig_errors.h>\n");
    if (ShortCircuit)
	    fprintf(file, "#include <mach/rpc.h>\n");
    if (isuser && IsKernelUser) {
	    fprintf(file, "#include <ipc/ipc_port.h>\n");
	    fprintf(file, "#include <kern/ipc_mig.h>\n");
    } else if (!isuser && IsKernelServer)
	    fprintf(file, "#include <ipc/ipc_port.h>\n");
    else
	fprintf(file, "#include <mach/port.h>\n");
    fprintf(file, "\n");
}

static void
WriteDefines(file)
    FILE *file;
{
}

static void
WriteMigExternal(file)
    FILE *file;
{
    fprintf(file, "#ifdef\tmig_external\n");
    fprintf(file, "mig_external\n");
    fprintf(file, "#else\n");
    fprintf(file, "extern\n");
    fprintf(file, "#endif\t/* mig_external */\n");
}

static void
WriteProlog(file, protect, more, isuser)
    FILE *file;
    char *protect;
    boolean_t more, isuser;
{
    if (protect != strNULL) {
	fprintf(file, "#ifndef\t_%s\n", protect);
	fprintf(file, "#define\t_%s\n", protect);
	fprintf(file, "\n");
    }

    fprintf(file, "/* Module %s */\n", SubsystemName);
    fprintf(file, "\n");

    if (more) {
	WriteIncludes(file, isuser);
	WriteDefines(file);
    }
}

static void
WriteEpilog(file, protect)
    FILE *file;
    char *protect;
{
    if (protect != strNULL) {
	fprintf(file, "\n");
	fprintf(file, "#endif\t /* _%s */\n", protect);
    }
}

static void
WriteUserRoutine(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "\n");
    fprintf(file, "/* %s %s */\n", rtRoutineKindToStr(rt->rtKind), rt->rtName);
    WriteMigExternal(file);
    fprintf(file, "%s %s\n", ReturnTypeStr(rt), rt->rtUserName);
    fprintf(file, "#if\t%s\n", LintLib);
    fprintf(file, "    (");
    WriteList(file, rt->rtArgs, WriteNameDecl, akbUserArg, ", " , "");
    fprintf(file, ")\n");
    WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ";\n", ";\n");
    fprintf(file, "{ ");
    fprintf(file, "return ");
    fprintf(file, "%s(", rt->rtUserName);
    WriteList(file, rt->rtArgs, WriteNameDecl, akbUserArg, ", ", "");
    fprintf(file, "); }\n");
    fprintf(file, "#else\n");
    if (BeAnsiC) {
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ",\n", "\n");
        fprintf(file, ");\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ",\n", "\n");
        fprintf(file, ");\n");
        fprintf(file, "#else\n");

        fprintf(file, "    ();\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl); 
    }
    fprintf(file, "#endif\t/* %s */\n",  LintLib);
}

void
WriteUserHeader(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    char *protect = strconcat(SubsystemName, "_user_");

    WriteProlog(file, protect, TRUE, TRUE);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteUserRoutine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skUImport:
	    WriteImport(file, stat->stFileName);
	    break;
	  case skSImport:
	    break;
	  default:
	    fatal("WriteHeader(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
    WriteEpilog(file, protect);
}

static void
WriteDefinesRoutine(file, rt)
    FILE *file;
    routine_t *rt;
{
    register char *up = (char *)malloc(strlen(rt->rtName)+1);

    up = toupperstr(strcpy(up, rt->rtName));
    fprintf(file, "#define\tMACH_ID_%s\t\t%d\t/* %s() */\n", 
	up, rt->rtNumber + SubsystemBase, rt->rtName);
    if (rt->rtKind == rkRoutine)
	fprintf(file, "#define\tMACH_ID_%s_REPLY\t\t%d\t/* %s() */\n", 
	    up, rt->rtNumber + SubsystemBase + 100, rt->rtName);
    fprintf(file, "\n");
}

void
WriteServerRoutine(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "\n");
    fprintf(file, "/* %s %s */\n", rtRoutineKindToStr(rt->rtKind), rt->rtName);
    WriteMigExternal(file);
    fprintf(file, "%s %s\n", ReturnTypeStr(rt), rt->rtServerName);
    fprintf(file, "#if\t%s\n", LintLib);
    fprintf(file, "    (");
    WriteList(file, rt->rtArgs, WriteNameDecl, akbServerArg, ", " , "");
    fprintf(file, ")\n");
    WriteList(file, rt->rtArgs, WriteServerVarDecl,
	      akbServerArg, ";\n", ";\n");
    fprintf(file, "{ ");
    fprintf(file, "return ");
    fprintf(file, "%s(", rt->rtServerName);
    WriteList(file, rt->rtArgs, WriteNameDecl, akbServerArg, ", ", "");
    fprintf(file, "); }\n");
    fprintf(file, "#else\n");
    if (BeAnsiC) {
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteServerVarDecl,
	      akbServerArg, ",\n", "\n");
        fprintf(file, ");\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteServerVarDecl,
	      akbServerArg, ",\n", "\n");
        fprintf(file, ");\n");
        fprintf(file, "#else\n");

        fprintf(file, "    ();\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl); 
    }
    fprintf(file, "#endif\t/* %s */\n",  LintLib);
}

static void
WriteDispatcher(file)
    FILE *file;
{
    register statement_t *stat;
    int descr_count = 0;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine)
	{
	    register routine_t *rt = stat->stRoutine;
	    descr_count += rtCountArgDescriptors(rt->rtArgs, (int *) 0);
	}
    fprintf(file, "\n");
    fprintf(file, "extern boolean_t %s(\n", ServerDemux);
    fprintf(file, "\t\tmach_msg_header_t *InHeadP,\n");
    fprintf(file, "\t\tmach_msg_header_t *OutHeadP);\n\n");

    fprintf(file, "extern mig_routine_t %s_routine(\n", ServerDemux);
    fprintf(file, "\t\tmach_msg_header_t *InHeadP);\n\n");

    fprintf(file, "\n/* Description of this subsystem, for use in direct RPC */\n");
    fprintf(file, "extern struct %s {\n", ServerSubsys);
    fprintf(file, "\tstruct subsystem *\tsubsystem;\t/* Reserved for system use */\n");
    fprintf(file, "\tmach_msg_id_t\tstart;\t/* Min routine number */\n");
    fprintf(file, "\tmach_msg_id_t\tend;\t/* Max routine number + 1 */\n");
    fprintf(file, "\tunsigned int\tmaxsize;\t/* Max msg size */\n");
    fprintf(file, "\tvm_address_t\tbase_addr;\t/* Base ddress */\n");
    fprintf(file, "\tstruct routine_descriptor\t/*Array of routine descriptors */\n");
    fprintf(file, "\t\troutine[%d];\n", rtNumber);
    fprintf(file, "\tstruct routine_arg_descriptor\t/*Array of arg descriptors */\n");
    fprintf(file, "\t\targ_descriptor[%d];\n", descr_count);
    fprintf(file, "} %s;\n", ServerSubsys);
    fprintf(file, "\n");
}

void
WriteServerHeader(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    char *protect = strconcat(SubsystemName, "_server_");

    WriteProlog(file, protect, TRUE, FALSE);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteServerRoutine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skSImport:
	    WriteImport(file, stat->stFileName);
	    break;
	  case skUImport:
	    break;
	  default:
	    fatal("WriteServerHeader(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
    WriteDispatcher(file);
    WriteEpilog(file, protect);
}

static void
WriteInternalRedefine(file, rt)
    FILE *file;
    register routine_t *rt;
{
    fprintf(file, "#define %s %s_external\n",
	    rt->rtUserName, rt->rtUserName);
}

void
WriteInternalHeader(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteInternalRedefine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skUImport:
	  case skSImport:
	    break;
	  default:
	    fatal("WriteInternalHeader(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
}

void
WriteDefinesHeader(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    char *protect = strconcat(SubsystemName, "_defines");

    WriteProlog(file, protect, FALSE, FALSE);
    fprintf(file, "\n/*\tDefines related to the Subsystem %s\t*/\n\n", SubsystemName);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteDefinesRoutine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skSImport:
	  case skUImport:
	    break;
	  default:
	    fatal("WriteDefinesHeader(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
    WriteEpilog(file, protect);
}
