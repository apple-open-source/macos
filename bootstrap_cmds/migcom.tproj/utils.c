/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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

#include <mach/message.h>
#include <stdarg.h>
#include <stdlib.h>
#include "routine.h"
#include "write.h"
#include "global.h"
#include "utils.h"
#include "error.h"

extern char *MessFreeRoutine;

void
WriteIdentificationString(file)
    FILE *file;
{
    extern char * GenerationDate;
    extern char * MigGenerationDate;
    extern char * MigMoreData;

    fprintf(file, "/*\n");
    fprintf(file, " * IDENTIFICATION:\n");
    fprintf(file, " * stub generated %s", GenerationDate);
    fprintf(file, " * with a MiG generated %s by %s\n", MigGenerationDate, MigMoreData);
    fprintf(file, " * OPTIONS: \n");
    if (IsKernelUser)
	fprintf(file, " *\tKernelUser\n");
    if (IsKernelServer)
	fprintf(file, " *\tKernelServer\n");
    if (!UseMsgRPC)
	fprintf(file, " *\t-R (no RPC calls)\n");
    fprintf(file, " */\n");
}

void
WriteMigExternal(file)
    FILE *file;
{
    fprintf(file, "#ifdef\tmig_external\n");
    fprintf(file, "mig_external\n");
    fprintf(file, "#else\n");
    fprintf(file, "extern\n");
    fprintf(file, "#endif\t/* mig_external */\n");
}

void
WriteMigInternal(file)
    FILE *file;
{
    fprintf(file, "#ifdef\tmig_internal\n");
    fprintf(file, "mig_internal\n");
    fprintf(file, "#else\n");
    fprintf(file, "static\n");
    fprintf(file, "#endif\t/* mig_internal */\n");
}

void
WriteImport(file, filename)
    FILE *file;
    string_t filename;
{
    fprintf(file, "#include %s\n", filename);
}

void 
WriteImplImports(file, stats, isuser)
    FILE *file;
    statement_t *stats;
    boolean_t isuser;
{
    register statement_t *stat;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skImport:
          case skIImport:
	    WriteImport(file, stat->stFileName);
	    break;
	  case skSImport:
	    if (!isuser)
	        WriteImport(file, stat->stFileName);
	    break;
	  case skUImport:
	    if (isuser)
	        WriteImport(file, stat->stFileName);
	    break;
	  case skRoutine:
	  case skDImport:
	    break;
	  default:
	    fatal("WriteImplImport(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
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

static void
WriteOneApplDefault(file, word1, word2, word3)
    FILE *file;
    char *word1;
    char *word2;
    char *word3;
{
    char buf[50];

    sprintf(buf, "__%s%s%s", word1, word2, word3);
    fprintf(file, "#ifndef\t%s\n", buf);
    fprintf(file, "#define\t%s(_NUM_, _NAME_)\n", buf);
    fprintf(file, "#endif\t/* %s */\n", buf);
    fprintf(file, "\n");
}
    
void
WriteApplDefaults(file, dir)
    FILE *file;
    char *dir;
{
    WriteOneApplDefault(file, "Declare", dir, "Rpc");
    WriteOneApplDefault(file, "Before", dir, "Rpc");
    WriteOneApplDefault(file, "After", dir, "Rpc");
    WriteOneApplDefault(file, "Declare", dir, "Simple");
    WriteOneApplDefault(file, "Before", dir, "Simple");
    WriteOneApplDefault(file, "After", dir, "Simple");
}

void
WriteApplMacro(file, dir, when, rt)
    FILE *file;
    char *dir;
    char *when;
    routine_t *rt;
{
    char *what = (rt->rtOneWay) ? "Simple" : "Rpc";

    fprintf(file, "\t__%s%s%s(%d, \"%s\")\n", 
	    when, dir, what, SubsystemBase + rt->rtNumber, rt->rtName);
}


void
WriteBogusDefines(file)
    FILE *file;
{
    fprintf(file, "#ifndef\tmig_internal\n");
    fprintf(file, "#define\tmig_internal\tstatic __inline__\n");
    fprintf(file, "#endif\t/* mig_internal */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tmig_external\n");
    fprintf(file, "#define mig_external\n");
    fprintf(file, "#endif\t/* mig_external */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tTypeCheck\n");
    fprintf(file, "#define\tTypeCheck 0\n");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tLimitCheck\n");
    fprintf(file, "#define\tLimitCheck 0\n");
    fprintf(file, "#endif\t/* LimitCheck */\n");
    fprintf(file, "\n");

    fprintf(file, "#ifndef\tmin\n");
    fprintf(file, "#define\tmin(a,b)  ( ((a) < (b))? (a): (b) )\n");
    fprintf(file, "#endif\t/* min */\n");
    fprintf(file, "\n");

	fprintf(file, "#if !defined(_WALIGN_)\n");
	fprintf(file, "#define _WALIGN_(x) (((x) + %d) & ~%d)\n",
			(int)(itWordAlign - 1), (int)(itWordAlign - 1));
	fprintf(file, "#endif /* !defined(_WALIGN_) */\n");
	fprintf(file, "\n");

	fprintf(file, "#if !defined(_WALIGNSZ_)\n");
    fprintf(file, "#define _WALIGNSZ_(x) _WALIGN_(sizeof(x))\n");
	fprintf(file, "#endif /* !defined(_WALIGNSZ_) */\n");
	fprintf(file, "\n");

    fprintf(file, "#ifndef\tUseStaticTemplates\n");
    if (BeAnsiC) {
        fprintf(file, "#define\tUseStaticTemplates\t0\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "#define\tUseStaticTemplates\t0\n");
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
    boolean_t pointer = (arg->argByReferenceUser ||arg->argType->itNativePointer);
    char *ref = (pointer) ? "*" : "";
    char *cnst = ((arg->argFlags & flConst) &&
		  (IS_VARIABLE_SIZED_UNTYPED(arg->argType) ||
		   arg->argType->itNoOptArray || arg->argType->itString)) ?
		"const " : "";

    fprintf(file, "\t%s%s %s%s", cnst, arg->argType->itUserType, ref, arg->argVarName);
}

void
WriteServerVarDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    char *ref = (arg->argByReferenceServer ||
		 arg->argType->itNativePointer) ? "*" : "";
    char *cnst = ((arg->argFlags & flConst) &&
		  (IS_VARIABLE_SIZED_UNTYPED(arg->argType) ||
		   arg->argType->itNoOptArray || arg->argType->itString)) ?
		"const " : "";

    fprintf(file, "\t%s%s %s%s", cnst, arg->argType->itTransType, ref, arg->argVarName);
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
FetchServerType(it)
    ipc_type_t *it;
{
    return it->itServerType;
}

char *
FetchKPDType(it)
    ipc_type_t *it;
{
    return it->itKPDType;
}

void
WriteTrailerDecl(file, trailer)
    FILE *file;
    boolean_t trailer;
{
    if (trailer)
	fprintf(file, "\t\tmach_msg_max_trailer_t trailer;\n");
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
    else if (IS_OPTIONAL_NATIVE(it)) {
        fprintf(file, "\t\tboolean_t __Present__%s;\n",  arg->argMsgField);
        fprintf(file, "\t\tunion {\n");
	fprintf(file, "\t\t    %s __Real__%s;\n",
		(*tfunc)(it), arg->argMsgField);
	fprintf(file, "\t\t    char __Phony__%s[_WALIGNSZ_(%s)];\n",
		arg->argMsgField, (*tfunc)(it));
	fprintf(file, "\t\t} %s;", arg->argMsgField);
    } 
    else  {
	/* either simple KPD or simple in-line */
	fprintf(file, "\t\t%s %s;", (*tfunc)(it), arg->argMsgField);
    }

    /* Kernel Processed Data has always PadSize = 0 */
    if (it->itPadSize != 0)
	fprintf(file, "\n\t\tchar %s[%d];", arg->argPadName, it->itPadSize);
}

void
WriteKPDFieldDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    if (akCheck(arg->argKind, akbSendKPD) ||
	akCheck(arg->argKind, akbReturnKPD))
    	WriteFieldDeclPrim(file, arg, FetchKPDType);
    else
	WriteFieldDeclPrim(file, arg, FetchServerType);
}

void
WriteStructDecl(file, args, func, mask, name, simple, trailer, trailer_t, template_only)
    FILE *file;
    argument_t *args;
    void (*func)();
    u_int mask;
    char *name;
    boolean_t simple, trailer;
    boolean_t trailer_t, template_only;
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
    if (!template_only) {
	if (mask == akbRequest) 
	    WriteList(file, args, func, mask | akbSendBody, "\n", "\n");

	else
	    WriteList(file, args, func, mask | akbReturnBody, "\n", "\n");
	if (trailer)
	    WriteTrailerDecl(file, trailer_t);    
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
    fprintf(file, "\tconst static %s %s = {\n", it->itKPDType, arg->argTTName);

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
    fprintf(file, "\tconst static %s %s = {\n", it->itKPDType, arg->argTTName);

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
    fprintf(file, "\tconst static %s %s = {\n", it->itKPDType, arg->argTTName);

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

void
WriteReplyTypes(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;

    fprintf(file, "/* typedefs for all replies */\n\n");
    fprintf(file, "#ifndef __Reply__%s_subsystem__defined\n", SubsystemName);
    fprintf(file, "#define __Reply__%s_subsystem__defined\n", SubsystemName);
    for (stat = stats; stat != stNULL; stat = stat->stNext) {
        if (stat->stKind == skRoutine) {
            register routine_t *rt;
            char str[MAX_STR_LEN];
	    
	    rt = stat->stRoutine;
	    sprintf(str, "__Reply__%s_t", rt->rtName);
	    WriteStructDecl(file, rt->rtArgs, WriteKPDFieldDecl, akbReply, 
			    str, rt->rtSimpleReply, FALSE, FALSE, FALSE);
	}
    }
    fprintf(file, "#endif /* !__Reply__%s_subsystem__defined */\n", SubsystemName);
    fprintf(file, "\n");
}

void
WriteRequestTypes(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;

    fprintf(file, "/* typedefs for all requests */\n\n");
    fprintf(file, "#ifndef __Request__%s_subsystem__defined\n", SubsystemName);
    fprintf(file, "#define __Request__%s_subsystem__defined\n", SubsystemName);
    for (stat = stats; stat != stNULL; stat = stat->stNext) {
        if (stat->stKind == skRoutine) {
            register routine_t *rt;
            char str[MAX_STR_LEN];
	    
	    rt = stat->stRoutine;
	    sprintf(str, "__Request__%s_t", rt->rtName);
	    WriteStructDecl(file, rt->rtArgs, WriteKPDFieldDecl, akbRequest, 
			    str, rt->rtSimpleRequest, FALSE, FALSE, FALSE);
	}
    }
    fprintf(file, "#endif /* !__Request__%s_subsystem__defined */\n", SubsystemName);
    fprintf(file, "\n");
}

void
WriteNDRConvertArgDecl(file, arg, convert, dir)
     FILE *file;
     argument_t *arg;
     char *convert, *dir;
{
    argument_t *count = arg->argCount;
    argument_t *parent = arg->argParent;
    char *carg = (count) ? ", c" : "";
    routine_t *rt = arg->argRoutine;
    ipc_type_t *ptype = arg->argType;
    ipc_type_t *btype;
    int multi, array;
    char domain[MAX_STR_LEN];

    fprintf(file, "#ifndef __NDR_convert__%s__%s__%s_t__%s__defined\n#",
	    convert, dir, rt->rtName, arg->argMsgField);

    for (btype = ptype, multi = (!parent) ? arg->argMultiplier : 1, array = 0;
		 btype;
		 ptype = btype, array += ptype->itVarArray, btype = btype->itElement) {
		char *bttype; 

		if (btype->itNumber < ptype->itNumber && !ptype->itVarArray && !parent) {
			multi *= ptype->itNumber / btype->itNumber;
			if (!btype->itString)
				continue;
		} else if (array && ptype->itVarArray)
			continue;
		if (btype != ptype)
			fprintf(file, "#el");

		bttype = (multi > 1 && btype->itString) ? "string" : FetchServerType(btype);
		sprintf(domain, "__%s", SubsystemName);
		do {
			fprintf(file, "if\tdefined(__NDR_convert__%s%s__%s__defined)\n",
					convert, domain, bttype);
			fprintf(file, "#define\t__NDR_convert__%s__%s__%s_t__%s__defined\n",
					convert, dir, rt->rtName, arg->argMsgField);
			fprintf(file, "#define\t__NDR_convert__%s__%s__%s_t__%s(a, f%s) \\\n\t",
					convert, dir, rt->rtName, arg->argMsgField, carg);
			if (multi > 1) {
				if (array)
					if (btype->itString)
						fprintf(file, "__NDR_convert__2DARRAY((%s *)(a), f, %d, c, ", bttype, multi);
					else
						fprintf(file, "__NDR_convert__ARRAY((%s *)(a), f, %d * (c), ", bttype, multi);
				else
					if (!btype->itString)
						fprintf(file, "__NDR_convert__ARRAY((%s *)(a), f, %d, ", bttype, multi);
			} else if (array)
				fprintf(file, "__NDR_convert__ARRAY((%s *)(a), f, c, ", bttype);
			fprintf(file, "__NDR_convert__%s%s__%s", convert, domain, bttype);
			if (multi > 1) {
				if (!array && btype->itString)
					fprintf(file, "(a, f, %d", multi);
			} else if (!array)
				fprintf(file, "((%s *)(a), f%s", bttype, carg);
			fprintf(file, ")\n");
		} while (strcmp(domain, "") && (domain[0] = '\0', fprintf(file, "#el")));
    }
    fprintf(file, "#endif /* defined(__NDR_convert__*__defined) */\n");
    fprintf(file, "#endif /* __NDR_convert__%s__%s__%s_t__%s__defined */\n\n",
	    convert, dir, rt->rtName, arg->argMsgField);
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
   	if (! rtMessOnStack(rt))
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

    fprintf(file, "\tTrailerP = (mach_msg_max_trailer_t *)((vm_offset_t)%s +\n", who);
    fprintf(file, "\t\tround_msg(%s->Head.msgh_size));\n", who);
    fprintf(file, "\tif (TrailerP->msgh_trailer_type != MACH_MSG_TRAILER_FORMAT_0)\n");
    if (isuser)
		fprintf(file, "\t\t{ return MIG_TRAILER_ERROR ; }\n");
    else
      fprintf(file, "\t\t{ MIG_RETURN_ERROR(%s, MIG_TRAILER_ERROR); }\n", who);
    
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
	if (isuser)
		fprintf(file, "\t\t{ return MIG_TRAILER_ERROR ; }\n");
	else
		fprintf(file, "\t\t{ MIG_RETURN_ERROR(OutP, MIG_TRAILER_ERROR); }\n");
	fprintf(file, "\ttrailer_size -= sizeof(mach_port_seqno_t);\n");
    } else if (akIdent(arg->argKind) == akeSecToken) {
		fprintf(file, "\tif (trailer_size < sizeof(security_token_t))\n");
		if (isuser)
			fprintf(file, "\t\t{ return MIG_TRAILER_ERROR ; }\n");
		else
			fprintf(file, "\t\t{ MIG_RETURN_ERROR(OutP, MIG_TRAILER_ERROR); }\n");
		fprintf(file, "\ttrailer_size -= sizeof(security_token_t);\n");
    } else if (akIdent(arg->argKind) == akeAuditToken) {
		fprintf(file, "\tif (trailer_size < sizeof(audit_token_t))\n");
		if (isuser)
			fprintf(file, "\t\t{ return MIG_TRAILER_ERROR ; }\n");
		else
			fprintf(file, "\t\t{ MIG_RETURN_ERROR(OutP, MIG_TRAILER_ERROR); }\n");
		fprintf(file, "\ttrailer_size -= sizeof(audit_token_t);\n");
    }
    fprintf(file, "#endif\t/* TypeCheck */\n");
}

