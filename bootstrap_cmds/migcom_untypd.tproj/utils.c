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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * 92/03/03  16:25:39  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  12:33:02  jeffreyh]
 * 
 * 92/01/14  16:47:08  rpd
 * 	Modified WriteTypeDeclIn and WriteTypeDeclOut to disable
 * 	the deallocate flag on Indefinite arguments.
 * 	[92/01/09            rpd]
 * 
 * 92/01/03  20:30:51  dbg
 * 	Change argByReferenceUser and argByReferenceServer to fields in
 * 	argument_t.
 * 	[91/08/29            dbg]
 * 
 * 91/07/31  18:11:45  dbg
 * 	Accept new dealloc_t argument type in WriteStaticDecl,
 * 	WritePackMsgType.
 * 
 * 	Don't need to zero last character of C string.  Mig_strncpy does
 * 	the proper work.
 * 
 * 	Add SkipVFPrintf, so that WriteCopyType doesn't print fields in
 * 	comments.
 * 	[91/07/17            dbg]
 * 
 * 91/06/25  10:32:36  rpd
 * 	Changed WriteVarDecl to WriteUserVarDecl.
 * 	Added WriteServerVarDecl.
 * 	[91/05/23            rpd]
 * 
 * 91/02/05  17:56:28  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:56:39  mrt]
 * 
 * 90/06/02  15:06:11  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:14:54  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 * 21-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added deallocflag to the WritePackMsg routines.
 *
 * 29-Jul-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed WriteVarDecl to not automatically write
 *	semi-colons between items, so that it can be
 *	used to write C++ argument lists.
 *
 * 27-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#include "type.h"
#include <mach/message.h>
#include <stdarg.h>
#include "routine.h"
#include "write.h"
#include "global.h"
#include "utils.h"

extern char *MessFreeRoutine;

void
WriteIdentificationString(file)
    FILE *file;
{
    extern char * GenerationDate;
    extern char migcom_untypd_VERS_STRING[];
    extern boolean_t IsKernelUser, IsKernelServer, UseMsgRPC;

    fprintf(file, "/*\n");
    fprintf(file, " * IDENTIFICATION:\n");
    fprintf(file, " * stub generated %s", GenerationDate);
    fprintf(file, " * OPTIONS: \n");
    if (IsKernelUser)
	fprintf(file, " *\tKernelUser\n");
    if (IsKernelServer)
	fprintf(file, " *\tKernelServer\n");
    if (!UseMsgRPC)
	fprintf(file, " *\t-R (no RPC calls)\n");
    fprintf(file, " */\n");
    fprintf(file, "\nstatic %s char mig_vers[] = \"%.*s\";\n", (BeAnsiC) ? "const" : "", strlen(migcom_untypd_VERS_STRING) - 1, migcom_untypd_VERS_STRING);
}

void
WriteImport(file, filename)
    FILE *file;
    string_t filename;
{
    fprintf(file, "#include %s\n", filename);
}

void
WriteRCSDecl(file, name, rcs)
    FILE *file;
    identifier_t name;
    string_t rcs;
{
    fprintf(file, "#ifndef\tlint\n");
    fprintf(file, "#if\tUseExternRCSId\n");
    fprintf(file, "%s char %s_rcsid[] = %s;\n", (BeAnsiC) ? "const" : "", name, rcs);
    fprintf(file, "#else\t/* UseExternRCSId */\n");
    fprintf(file, "static %s char rcsid[] = %s;\n", (BeAnsiC) ? "const" : "", rcs);
    fprintf(file, "#endif\t/* UseExternRCSId */\n");
    fprintf(file, "#endif\t/* lint */\n");
    fprintf(file, "\n");
}

void
WriteBogusDefines(file)
    FILE *file;
{
    fprintf(file, "#ifndef\tmig_internal\n");
    fprintf(file, "#define\tmig_internal\tstatic\n");
    fprintf(file, "#endif\t/* mig_internal */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tmig_external\n");
    fprintf(file, "#define mig_external\n");
    fprintf(file, "#endif\t/* mig_external */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tTypeCheck\n");
    fprintf(file, "#define\tTypeCheck 1\n");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tmin\n");
    fprintf(file, "#define\tmin(a,b)  ( ((a) < (b))? (a): (b) )\n");
    fprintf(file, "#endif\t/* min */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tUseStaticTemplates\n");
    if (BeAnsiC) {
        fprintf(file, "#define\tUseStaticTemplates\t1\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "#define\tUseStaticTemplates\t1\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }    
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
    fprintf(file, "\n");
}

void
WriteList(file, args, func, mask, between, after)
    FILE *file;
    argument_t *args;
    void (*func)();
    u_int mask;
    char *between, *after;
{
    register argument_t *arg;
    register boolean_t sawone = FALSE;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheckAll(arg->argKind, mask))
	{
	    if (sawone)
		fprintf(file, "%s", between);
	    sawone = TRUE;

	    (*func)(file, arg);
	}

    if (sawone)
	fprintf(file, "%s", after);
}

static boolean_t
WriteReverseListPrim(file, arg, func, mask, between)
    FILE *file;
    register argument_t *arg;
    void (*func)();
    u_int mask;
    char *between;
{
    boolean_t sawone = FALSE;

    if (arg != argNULL)
    {
	sawone = WriteReverseListPrim(file, arg->argNext, func, mask, between);

	if (akCheckAll(arg->argKind, mask))
	{
	    if (sawone)
		fprintf(file, "%s", between);
	    sawone = TRUE;

	    (*func)(file, arg);
	}
    }

    return sawone;
}

void
WriteReverseList(file, args, func, mask, between, after)
    FILE *file;
    argument_t *args;
    void (*func)();
    u_int mask;
    char *between, *after;
{
    boolean_t sawone;

    sawone = WriteReverseListPrim(file, args, func, mask, between);

    if (sawone)
	fprintf(file, "%s", after);
}

void
WriteNameDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    fprintf(file, "%s", arg->argVarName);
}

void
WriteUserVarDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    char *ref = arg->argByReferenceUser ? "*" : "";

    fprintf(file, "\t%s %s%s", arg->argType->itUserType, ref, arg->argVarName);
}

void
WriteServerVarDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    char *ref = arg->argByReferenceServer ? "*" : "";
  
    fprintf(file, "\t%s %s%s",
	    arg->argType->itTransType, ref, arg->argVarName);
}

char *
ReturnTypeStr(rt)
    routine_t *rt;
{
    return rt->rtRetCode->argType->itUserType;
}

char *
FetchUserType(it)
    ipc_type_t *it;
{
    return it->itUserType;
}

char *
FetchUserKPDType(it)
    ipc_type_t *it;
{
    return it->itUserKPDType;
}

char *
FetchServerType(it)
    ipc_type_t *it;
{
    return it->itServerType;
}

char *
FetchServerKPDType(it)
    ipc_type_t *it;
{
    /* do we really need to differentiate User and Server ?? */
    return it->itServerKPDType;
}

void
WriteTrailerDecl(file, trailer)
    FILE *file;
    boolean_t trailer;
{
    if (trailer)
	fprintf(file, "\t\tmach_msg_format_0_trailer_t trailer;\n");
    else
	fprintf(file, "\t\tmach_msg_trailer_t trailer;\n");
}

void
WriteFieldDeclPrim(file, arg, tfunc)
    FILE *file;
    argument_t *arg;
    char *(*tfunc)();
{
    register ipc_type_t *it = arg->argType;

    if (IS_VARIABLE_SIZED_UNTYPED(it) || it->itNoOptArray) {
	register argument_t *count = arg->argCount;
	register ipc_type_t *btype = it->itElement;

	/*
	 *	Build our own declaration for a varying array:
	 *	use the element type and maximum size specified.
	 *	Note arg->argCount->argMultiplier == btype->itNumber.
	 */
	/*
	 * NDR encoded VarStrings requires the offset field.
	 * Since it is not used, it wasn't worthwhile to create an extra 
	 * parameter
	 */
	if (it->itString)
	    fprintf(file, "\t\t%s %sOffset; /* MiG doesn't use it */\n", 
		(*tfunc)(count->argType), arg->argName);

	if (!(arg->argFlags & flSameCount) && !it->itNoOptArray)
	        /* in these cases we would have a count, which we don't want */
		fprintf(file, "\t\t%s %s;\n", (*tfunc)(count->argType), 
		    count->argMsgField);
	fprintf(file, "\t\t%s %s[%d];",
			(*tfunc)(btype),
			arg->argMsgField,
			it->itNumber/btype->itNumber);
    }
    else if (IS_MULTIPLE_KPD(it))  
	fprintf(file, "\t\t%s %s[%d];", (*tfunc)(it), arg->argMsgField,
			it->itKPD_Number);
    else  {
	/* either simple KPD or simple in-line */
	fprintf(file, "\t\t%s %s;", (*tfunc)(it), arg->argMsgField);
    }

    /* Kernel Processed Data has always PadSize = 0 */
    if (it->itPadSize != 0)
	fprintf(file, "\n\t\tchar %s[%d];", arg->argPadName, it->itPadSize);
}


void
WriteStructDecl(file, args, func, mask, name, simple, trailer, isuser, template_only)
    FILE *file;
    argument_t *args;
    void (*func)();
    u_int mask;
    char *name;
    boolean_t simple, trailer;
    boolean_t isuser, template_only;
{
    fprintf(file, "\ttypedef struct {\n");
    fprintf(file, "\t\tmach_msg_header_t Head;\n");
    if (simple == FALSE) {
	fprintf(file, "\t\t/* start of the kernel processed data */\n");
	fprintf(file, "\t\tmach_msg_body_t msgh_body;\n");
	if (mask == akbRequest) 
    	    WriteList(file, args, func, mask | akbSendKPD, "\n", "\n");
	else 
    	    WriteList(file, args, func, mask | akbReturnKPD, "\n", "\n");
	fprintf(file, "\t\t/* end of the kernel processed data */\n");
    }
    if (!template_only) 
	if (mask == akbRequest) {
	    WriteList(file, args, func, mask | akbSendBody, "\n", "\n");
	    if (!isuser)
		WriteTrailerDecl(file, trailer);
	} else {
	    WriteList(file, args, func, mask | akbReturnBody, "\n", "\n");
	    if (isuser)
		WriteTrailerDecl(file, trailer);
	}
    fprintf(file, "\t} %s;\n", name);
    fprintf(file, "\n");
}

void
WriteTemplateDeclIn(file, arg)
    FILE *file;
    register argument_t *arg;
{
    (*arg->argKPD_Template)(file, arg, TRUE);
}

void
WriteTemplateDeclOut(file, arg)
    FILE *file;
    register argument_t *arg;
{
    (*arg->argKPD_Template)(file, arg, FALSE);
}

void
WriteTemplateKPD_port(file, arg, in)
    FILE *file;
    argument_t *arg;
    boolean_t in;
{
    register ipc_type_t *it = arg->argType;

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\tstatic %s %s = {\n", it->itUserKPDType, arg->argTTName);

    fprintf(file, "\t\t/* name = */\t\tMACH_PORT_NULL,\n");
    fprintf(file, "\t\t/* pad1 = */\t\t0,\n");
    fprintf(file, "\t\t/* pad2 = */\t\t0,\n");
    fprintf(file, "\t\t/* disp = */\t\t%s,\n",
	in ? it->itInNameStr: it->itOutNameStr);
    fprintf(file, "\t\t/* type = */\t\tMACH_MSG_PORT_DESCRIPTOR,\n");

    fprintf(file, "\t};\n");
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
}

void
WriteTemplateKPD_ool(file, arg, in)
    FILE *file;
    argument_t *arg;
    boolean_t in;
{
    register ipc_type_t *it = arg->argType;

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\tstatic %s %s = {\n", it->itUserKPDType, arg->argTTName);

    if (IS_MULTIPLE_KPD(it))
	it = it->itElement;

    fprintf(file, "\t\t/* addr = */\t\t(void *)0,\n");
    if (it->itVarArray)
	fprintf(file, "\t\t/* size = */\t\t0,\n");
    else
	fprintf(file, "\t\t/* size = */\t\t%d,\n",
	    (it->itNumber * it->itSize + 7)/8);
    fprintf(file, "\t\t/* deal = */\t\t%s,\n",
	(arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    /* the d_MAYBE case will be fixed runtime */
    fprintf(file, "\t\t/* copy = */\t\t%s,\n",
	(arg->argFlags & flPhysicalCopy) ? "MACH_MSG_PHYSICAL_COPY" : "MACH_MSG_VIRTUAL_COPY");
    /* the PHYSICAL COPY flag has not been established yet */
    fprintf(file, "\t\t/* pad2 = */\t\t0,\n");
    fprintf(file, "\t\t/* type = */\t\tMACH_MSG_OOL_DESCRIPTOR,\n");

    fprintf(file, "\t};\n");
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
}

void
WriteTemplateKPD_oolport(file, arg, in)
    FILE *file;
    argument_t *arg;
    boolean_t in;
{
    register ipc_type_t *it = arg->argType;

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\tstatic %s %s = {\n", it->itUserKPDType, arg->argTTName);

    if (IS_MULTIPLE_KPD(it))
	it = it->itElement;

    fprintf(file, "\t\t/* addr = */\t\t(void *)0,\n");
    if (!it->itVarArray)
	fprintf(file, "\t\t/* coun = */\t\t%d,\n",
	    it->itNumber);
    else
	fprintf(file, "\t\t/* coun = */\t\t0,\n");
    fprintf(file, "\t\t/* deal = */\t\t%s,\n",
        (arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    fprintf(file, "\t\t/* copy is meaningful only in overwrite mode */\n");
    fprintf(file, "\t\t/* copy = */\t\tMACH_MSG_PHYSICAL_COPY,\n");
    fprintf(file, "\t\t/* disp = */\t\t%s,\n",
	in ? it->itInNameStr: it->itOutNameStr);
    fprintf(file, "\t\t/* type = */\t\tMACH_MSG_OOL_PORTS_DESCRIPTOR,\n");

    fprintf(file, "\t};\n");
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
}

/*
 * Like vfprintf, but omits a leading comment in the format string
 * and skips the items that would be printed by it.  Only %s, %d,
 * and %f are recognized.
 */
void
SkipVFPrintf(file, fmt, pvar)
    FILE *file;
    register char *fmt;
    va_list pvar;
{
    if (*fmt == 0)
	return;	/* degenerate case */

    if (fmt[0] == '/' && fmt[1] == '*') {
	/* Format string begins with C comment.  Scan format
	   string until end-comment delimiter, skipping the
	   items in pvar that the enclosed format items would
	   print. */

	register int c;

	fmt += 2;
	for (;;) {
	    c = *fmt++;
	    if (c == 0)
		return;	/* nothing to format */
	    if (c == '*') {
		if (*fmt == '/') {
		    break;
		}
	    }
	    else if (c == '%') {
		/* Field to skip */
		c = *fmt++;
		switch (c) {
		    case 's':
			(void) va_arg(pvar, char *);
			break;
		    case 'd':
			(void) va_arg(pvar, int);
			break;
		    case 'f':
			(void) va_arg(pvar, double);
			break;
		    case '\0':
			return; /* error - fmt ends with '%' */
		    default:
			break;
		}
	    }
	}
	/* End of comment.  To be pretty, skip
	   the space that follows. */
	fmt++;
	if (*fmt == ' ')
	    fmt++;
    }

    /* Now format the string. */
    (void) vfprintf(file, fmt, pvar);
}

static void
vWriteCopyType(FILE *file, ipc_type_t *it, char *left, char *right, va_list pvar)
{
    if (it->itStruct)
    {
	fprintf(file, "\t");
	(void) SkipVFPrintf(file, left, pvar);
	fprintf(file, " = ");
	(void) SkipVFPrintf(file, right, pvar);
	fprintf(file, ";\n");
    }
    else if (it->itString)
    {
	fprintf(file, "\t(void) mig_strncpy(");
	(void) SkipVFPrintf(file, left, pvar);
	fprintf(file, ", ");
	(void) SkipVFPrintf(file, right, pvar);
	fprintf(file, ", %d);\n", it->itTypeSize);
    }
    else
    {
	fprintf(file, "\t{   typedef struct { char data[%d]; } *sp;\n",
		it->itTypeSize);
	fprintf(file, "\t    * (sp) ");
	(void) SkipVFPrintf(file, left, pvar);
	fprintf(file, " = * (sp) ");
	(void) SkipVFPrintf(file, right, pvar);
	fprintf(file, ";\n\t}\n");
    }
}


/*ARGSUSED*/
/*VARARGS4*/
void
WriteCopyType(FILE *file, ipc_type_t *it, char *left, char *right, ...)
{
    va_list pvar;
    va_start(pvar, right);

    vWriteCopyType(file, it, left, right, pvar);

    va_end(pvar);
}


/*ARGSUSED*/
/*VARARGS4*/
void
WriteCopyArg(FILE *file, argument_t *arg, char *left, char *right, ...)
{
    va_list pvar;
    va_start(pvar, right);

    {
	ipc_type_t *it = arg->argType;
	if (it->itVarArray && !it->itString) {
	    fprintf(file, "\t    (void)memcpy(");
	    (void) SkipVFPrintf(file, left, pvar);
	    fprintf(file, ", ");
	    (void) SkipVFPrintf(file, right, pvar);
	    fprintf(file, ", %s);\n", arg->argCount->argVarName);
	} else
	    vWriteCopyType(file, it, left, right, pvar);
    }

    va_end(pvar);
}


/*
 * Global KPD disciplines 
 */
void
KPD_error(file, arg)
    FILE *file;
    argument_t *arg;
{
    printf("MiG internal error: argument is %s\n", arg->argVarName);
    exit(1);
}

void
KPD_noop(file, arg)
    FILE *file;
    argument_t *arg;
{
}

static void
WriteStringDynArgs(args, mask, InPOutP, str_oolports, str_ool)
    argument_t *args;
    u_int	mask;
    string_t 	InPOutP;
    string_t 	*str_oolports, *str_ool;
{
    argument_t *arg;
    char loc[100], sub[20];
    string_t tmp_str1 = ""; 
    string_t tmp_str2 = "";
    int cnt, multiplier = 1;
    boolean_t test, complex = FALSE;

    for (arg = args; arg != argNULL; arg = arg->argNext) {
	ipc_type_t *it = arg->argType;

	if (IS_MULTIPLE_KPD(it)) {
	    test = it->itVarArray || it->itElement->itVarArray;
	    if (test) {
		multiplier = it->itKPD_Number;
	        it = it->itElement;
	        complex = TRUE;
	    }
	} else
	    test = it->itVarArray;

	cnt = multiplier;
	while (cnt) {
	    if (complex)
		sprintf(sub, "[%d]", multiplier - cnt);
	    if (akCheck(arg->argKind, mask) && 
		it->itPortType && !it->itInLine && test) {
		    sprintf(loc, " + %s->%s%s.count", InPOutP, arg->argMsgField,
		        complex ? sub : "");
		    tmp_str1 = strconcat(tmp_str1, loc);
	    }
	    if (akCheck(arg->argKind, mask) && 
		!it->itInLine && !it->itPortType && test) {
	 	    sprintf(loc, " + %s->%s%s.size", InPOutP, arg->argMsgField,
		        complex ? sub : "");
		    tmp_str2 = strconcat(tmp_str2, loc);
	    }
	    cnt--;
	}
    }
    *str_oolports = tmp_str1;
    *str_ool = tmp_str2;  
}

/*
 * Utilities for Logging Events that happen at the stub level
 */
void
WriteLogMsg(file, rt, where, what)
    FILE *file;
    routine_t *rt;
    int where, what;
{
    string_t ptr_str;
    string_t StringOolPorts = strNULL;
    string_t StringOOL = strNULL;
    u_int ports, oolports, ool;
    string_t event;

    fprintf(file, "\n#if  MIG_DEBUG\n");
    if (where == LOG_USER)
	fprintf(file, "\tLOG_TRACE(MACH_MSG_LOG_USER,\n");
    else
	fprintf(file, "\tLOG_TRACE(MACH_MSG_LOG_SERVER,\n");
    if (where == LOG_USER && what == LOG_REQUEST) {
	ptr_str = "InP";
	event = "MACH_MSG_REQUEST_BEING_SENT";
    } else if (where == LOG_USER && what == LOG_REPLY) {
	ptr_str = "Out0P";
	event = "MACH_MSG_REPLY_BEING_RCVD";
    } else if (where == LOG_SERVER && what == LOG_REQUEST) {
	ptr_str = "In0P";
	event = "MACH_MSG_REQUEST_BEING_RCVD";
    } else {
	ptr_str = "OutP";
	event = "MACH_MSG_REPLY_BEING_SENT";
    }
    WriteStringDynArgs(rt->rtArgs, 
	(what == LOG_REQUEST) ? akbSendKPD : akbReturnKPD, 
	ptr_str, &StringOolPorts, &StringOOL);
    fprintf(file, "\t\t%s,\n", event);
    fprintf(file, "\t\t%s->Head.msgh_id,\n", ptr_str);
    if (where == LOG_USER && what == LOG_REQUEST) {
	if (rt->rtNumRequestVar)
	    fprintf(file, "\t\tmsgh_size,\n");
	else
	    fprintf(file, "\t\tsizeof(Request),\n");
    } else 
	fprintf(file, "\t\t%s->Head.msgh_size,\n", ptr_str);
    if ((what == LOG_REQUEST && rt->rtSimpleRequest == FALSE) ||
	(what == LOG_REPLY && rt->rtSimpleReply == FALSE))
	    fprintf(file, "\t\t%s->msgh_body.msgh_descriptor_count,\n", ptr_str);
    else
	    fprintf(file, "\t\t0, /* Kernel Proc. Data entries */\n");
    if (what == LOG_REQUEST) {
	fprintf(file, "\t\t0, /* RetCode */\n");
	ports = rt->rtCountPortsIn;
        oolports = rt->rtCountOolPortsIn;
	ool = rt->rtCountOolIn;
    } else {
	if (akCheck(rt->rtRetCode->argKind, akbReply))
	    fprintf(file, "\t\t%s->RetCode,\n", ptr_str);
	else
	    fprintf(file, "\t\t0, /* RetCode */\n");
	ports = rt->rtCountPortsOut;
        oolports = rt->rtCountOolPortsOut;
	ool = rt->rtCountOolOut;
    }
    fprintf(file, "\t\t/* Ports */\n");
    fprintf(file, "\t\t%d,\n", ports);
    fprintf(file, "\t\t/* Out-of-Line Ports */\n");
    fprintf(file, "\t\t%d", oolports);
    if (StringOolPorts != strNULL)
	fprintf(file, "%s,\n", StringOolPorts);
    else
	fprintf(file, ",\n");
    fprintf(file, "\t\t/* Out-of-Line Bytes */\n");
    fprintf(file, "\t\t%d", ool);
    if (StringOOL != strNULL)
	fprintf(file, "%s,\n", StringOOL);
    else
	fprintf(file, ",\n");
    fprintf(file, "\t\t__FILE__, __LINE__);\n");
    fprintf(file, "#endif /* MIG_DEBUG */\n\n");
}

void
WriteLogDefines(file, who)
    FILE *file;
    string_t who;
{
    fprintf(file, "#if  MIG_DEBUG\n");
    fprintf(file, "#define LOG_W_E(X)\tLOG_ERRORS(%s, \\\n", who);
    fprintf(file, "\t\t\tMACH_MSG_ERROR_WHILE_PARSING, (void *)(X), __FILE__, __LINE__)\n");
    fprintf(file, "#else  /* MIG_DEBUG */\n");
    fprintf(file, "#define LOG_W_E(X)\n");
    fprintf(file, "#endif /* MIG_DEBUG */\n");
    fprintf(file, "\n");
}

/* common utility to report errors */
void
WriteReturnMsgError(file, rt, isuser, arg, error)
    FILE *file;
    routine_t *rt;
    boolean_t isuser;
    argument_t *arg;
    string_t error;
{
    char space[MAX_STR_LEN];
    string_t string = &space[0];

    if (UseEventLogger && arg != argNULL) 
	sprintf(string, "LOG_W_E(\"%s\"); ", arg->argVarName);
    else
	string = "";

    fprintf(file, "\t\t{ ");

    if (isuser) {
   	if (! rt->rtMessOnStack)
		fprintf(file, "%s((char *) Mess, sizeof(*Mess)); ", MessFreeRoutine);

        fprintf(file, "%sreturn %s; }\n", string, error);
    }
    else
        fprintf(file, "%sMIG_RETURN_ERROR(OutP, %s); }\n", string, error);
}

/* executed iff elements are defined */
void
WriteCheckTrailerHead(file, rt, isuser)
    FILE *file;
    routine_t *rt;
    boolean_t isuser;
{
    string_t who = (isuser) ? "Out0P" : "In0P";

    fprintf(file, "\tTrailerP = (mach_msg_format_0_trailer_t *)((vm_offset_t)%s +\n", who);
    fprintf(file, "\t\tround_msg(%s->Head.msgh_size));\n", who);
    fprintf(file, "\tif (TrailerP->msgh_trailer_type != MACH_MSG_TRAILER_FORMAT_0)\n");

    WriteReturnMsgError(file, rt, isuser, argNULL, "MIG_TRAILER_ERROR");
    
    fprintf(file, "#if\tTypeCheck\n");
    fprintf(file, "\ttrailer_size = TrailerP->msgh_trailer_size -\n");
    fprintf(file, "\t\tsizeof(mach_msg_trailer_type_t) - sizeof(mach_msg_trailer_size_t);\n");
    fprintf(file, "#endif\t/* TypeCheck */\n");
}

/* executed iff elements are defined */
void
WriteCheckTrailerSize(file, isuser, arg)
    FILE *file;
    boolean_t isuser;
    register argument_t *arg;
{
    fprintf(file, "#if\tTypeCheck\n");
    if (akIdent(arg->argKind) == akeMsgSeqno) {
	fprintf(file, "\tif (trailer_size < sizeof(mach_port_seqno_t))\n");
	WriteReturnMsgError(file, arg->argRoutine, isuser, arg, "MIG_TRAILER_ERROR");
	fprintf(file, "\ttrailer_size -= sizeof(mach_port_seqno_t);\n");
    } else if (akIdent(arg->argKind) == akeSecToken) {
	fprintf(file, "\tif (trailer_size < sizeof(security_token_t))\n");
	WriteReturnMsgError(file, arg->argRoutine, isuser, arg, "MIG_TRAILER_ERROR");
	fprintf(file, "\ttrailer_size -= sizeof(security_token_t);\n");
    }
    fprintf(file, "#endif\t/* TypeCheck */\n");
}
