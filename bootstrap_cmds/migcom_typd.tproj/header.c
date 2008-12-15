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

#include <write.h>
#include <utils.h>
#include <global.h>
#include <error.h>

static void
WriteIncludes(FILE *file)
{
    fprintf(file, "#include <mach/kern_return.h>\n");
    fprintf(file, "#include <mach/port.h>\n");
    fprintf(file, "#include <mach/message.h>\n");
    fprintf(file, "\n");
}

static void
WriteDefines(FILE *file)
{
}

static void
WriteMigExternal(FILE *file)
{
    fprintf(file, "#ifdef\tmig_external\n");
    fprintf(file, "mig_external\n");
    fprintf(file, "#else\n");
    fprintf(file, "extern\n");
    fprintf(file, "#endif\n");
}

static void
WriteProlog(FILE *file, const char *protect)
{
    if (protect != strNULL) {
	fprintf(file, "#ifndef\t_%s\n", protect);
	fprintf(file, "#define\t_%s\n", protect);
	fprintf(file, "\n");
    }

    fprintf(file, "/* Module %s */\n", SubsystemName);
    fprintf(file, "\n");

    WriteIncludes(file);
    WriteDefines(file);
}

static void
WriteEpilog(FILE *file, const char *protect)
{
    if (protect != strNULL) {
	fprintf(file, "\n");
	fprintf(file, "#endif\t/* not defined(_%s) */\n", protect);
    }
}

static void
WriteUserRoutine(FILE *file, const routine_t *rt)
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
    if (!rt->rtProcedure)
	fprintf(file, "return ");
    fprintf(file, "%s(", rt->rtUserName);
    WriteList(file, rt->rtArgs, WriteNameDecl, akbUserArg, ", ", "");
    fprintf(file, "); }\n");
    fprintf(file, "#else\n");
    fprintf(file, "(\n");
    WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ",\n", "\n");
    fprintf(file, ");\n");
    fprintf(file, "#endif\n");
}

void
WriteUserHeader(FILE *file, const statement_t *stats)
{
    register const statement_t *stat;
    const char *protect = strconcat(SubsystemName, "_user_");

    WriteProlog(file, protect);
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
WriteServerRoutine(FILE *file, const routine_t *rt)
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
    if (!rt->rtProcedure)
	fprintf(file, "return ");
    fprintf(file, "%s(", rt->rtServerName);
    WriteList(file, rt->rtArgs, WriteNameDecl, akbServerArg, ", ", "");
    fprintf(file, "); }\n");
    fprintf(file, "#else\n");
    fprintf(file, "(\n");
    WriteList(file, rt->rtArgs, WriteServerVarDecl,
	      akbServerArg, ",\n", "\n");
    fprintf(file, ");\n");
    fprintf(file, "#endif\n");
}

void
WriteServerHeader(FILE *file, const statement_t *stats)
{
    register const statement_t *stat;
    const char *protect = strconcat(SubsystemName, "_server_");

    WriteProlog(file, protect);
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
    WriteEpilog(file, protect);
}

static void
WriteInternalRedefine(FILE *file, register const routine_t *rt)
{
    fprintf(file, "#define %s %s_external\n", rt->rtUserName, rt->rtUserName);
}

void
WriteInternalHeader(FILE *file, const statement_t *stats)
{
    register const statement_t *stat;

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
