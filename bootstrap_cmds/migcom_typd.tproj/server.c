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

#include <assert.h>

#include <mach/message.h>
#include <write.h>
#include <utils.h>
#include <global.h>
#include <error.h>

static void
WriteIncludes(FILE *file)
{
    fprintf(file, "#define EXPORT_BOOLEAN\n");
    fprintf(file, "#include <mach/boolean.h>\n");
    fprintf(file, "#include <mach/kern_return.h>\n");
    fprintf(file, "#include <mach/message.h>\n");
    fprintf(file, "#include <mach/mig_errors.h>\n");
    fprintf(file, "#include <mach/mig_support.h>\n");
    if (IsKernelServer)
	fprintf(file, "#include <ipc/ipc_port.h>\n");
    fprintf(file, "\n");
}

static void
WriteGlobalDecls(FILE *file)
{
    if (RCSId != strNULL)
	WriteRCSDecl(file, strconcat(SubsystemName, "_server"), RCSId);

    /* Used for locations in the request message, *not* reply message.
       Reply message locations aren't dependent on IsKernelServer. */

    if (IsKernelServer)
    {
	fprintf(file, "#define msgh_request_port\tmsgh_remote_port\n");
	fprintf(file, "#define MACH_MSGH_BITS_REQUEST(bits)");
	fprintf(file, "\tMACH_MSGH_BITS_REMOTE(bits)\n");
	fprintf(file, "#define msgh_reply_port\t\tmsgh_local_port\n");
	fprintf(file, "#define MACH_MSGH_BITS_REPLY(bits)");
	fprintf(file, "\tMACH_MSGH_BITS_LOCAL(bits)\n");
    }
    else
    {
	fprintf(file, "#define msgh_request_port\tmsgh_local_port\n");
	fprintf(file, "#define MACH_MSGH_BITS_REQUEST(bits)");
	fprintf(file, "\tMACH_MSGH_BITS_LOCAL(bits)\n");
	fprintf(file, "#define msgh_reply_port\t\tmsgh_remote_port\n");
	fprintf(file, "#define MACH_MSGH_BITS_REPLY(bits)");
	fprintf(file, "\tMACH_MSGH_BITS_REMOTE(bits)\n");
    }
    fprintf(file, "\n");
}

static void
WriteProlog(FILE *file)
{
    fprintf(file, "/* Module %s */\n", SubsystemName);
    fprintf(file, "\n");
    
    WriteIncludes(file);
    WriteBogusDefines(file);
    WriteGlobalDecls(file);
}


static void
WriteSymTabEntries(FILE *file, const statement_t *stats)
{
    register const statement_t *stat;
    register u_int current = 0;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine) {
	    register	num = stat->stRoutine->rtNumber;
	    const char	*name = stat->stRoutine->rtName;

	    while (++current <= num)
		fprintf(file,"\t\t\t{ \"\", 0, 0 },\n");
	    fprintf(file, "\t{ \"%s\", %d, _X%s },\n",
	    	name,
		SubsystemBase + current - 1,
		name);
	}
    while (++current <= rtNumber)
	fprintf(file,"\t{ \"\", 0, 0 },\n");
}

static void
WriteArrayEntries(FILE *file, const statement_t *stats)
{
    register u_int current = 0;
    register const statement_t *stat;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine)
	{
	    register const routine_t *rt = stat->stRoutine;

	    while (current++ < rt->rtNumber)
		fprintf(file, "\t\t0,\n");
	    fprintf(file, "\t\t_X%s,\n", rt->rtName);
	}
    while (current++ < rtNumber)
	fprintf(file, "\t\t\t0,\n");
}

static void
WriteEpilog(FILE *file, const statement_t *stats)
{
    fprintf(file, "\n");

    /*
     * First, the symbol table
     */
     fprintf(file, "static mig_routine_t %s_routines[] = {\n", ServerDemux);

     WriteArrayEntries(file, stats);

     fprintf(file, "};\n");
     fprintf(file, "\n");

     /*
      * Then, the server routine
      */
    fprintf(file, "mig_external boolean_t %s\n", ServerDemux);
    fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");

    fprintf(file, "{\n");
    fprintf(file, "\tregister mach_msg_header_t *InP =  InHeadP;\n");

    fprintf(file, "\tregister mig_reply_header_t *OutP = (mig_reply_header_t *) OutHeadP;\n");

    fprintf(file, "\n");

    WriteStaticDecl(file, itRetCodeType,
		    itRetCodeType->itDeallocate, itRetCodeType->itLongForm,
		    !IsKernelServer, "RetCodeType");
    fprintf(file, "\n");

    fprintf(file, "\tregister mig_routine_t routine;\n");
    fprintf(file, "\n");

    fprintf(file, "\tOutP->Head.msgh_bits = ");
    fprintf(file, "MACH_MSGH_BITS(MACH_MSGH_BITS_REPLY(InP->msgh_bits), 0);\n");
    fprintf(file, "\tOutP->Head.msgh_size = sizeof *OutP;\n");
    fprintf(file, "\tOutP->Head.msgh_remote_port = InP->msgh_reply_port;\n");
    fprintf(file, "\tOutP->Head.msgh_local_port = MACH_PORT_NULL;\n");
    fprintf(file, "\tOutP->Head.msgh_seqno = 0;\n");
    fprintf(file, "\tOutP->Head.msgh_id = InP->msgh_id + 100;\n");
    fprintf(file, "\n");
    WritePackMsgType(file, itRetCodeType,
		     itRetCodeType->itDeallocate, itRetCodeType->itLongForm,
		     !IsKernelServer, "OutP->RetCodeType", "RetCodeType");
    fprintf(file, "\n");

    fprintf(file, "\tif ((InP->msgh_id > %d) || (InP->msgh_id < %d) ||\n",
	    SubsystemBase + rtNumber - 1, SubsystemBase);
    fprintf(file, "\t    ((routine = %s_routines[InP->msgh_id - %d]) == 0)) {\n",
	    ServerDemux, SubsystemBase);
    fprintf(file, "\t\tOutP->RetCode = MIG_BAD_ID;\n");
    fprintf(file, "\t\treturn FALSE;\n");
    fprintf(file, "\t}\n");

    /* Call appropriate routine */
    fprintf(file, "\t(*routine) (InP, &OutP->Head);\n");
    fprintf(file, "\treturn TRUE;\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");

    /*
     * Then, the <subsystem>_server_routine routine
     */
    fprintf(file, "mig_external mig_routine_t %s_routine\n", ServerDemux);
    fprintf(file, "\t(const mach_msg_header_t *InHeadP)\n");

    fprintf(file, "{\n");
    fprintf(file, "\tregister int msgh_id;\n");
    fprintf(file, "\n");
    fprintf(file, "\tmsgh_id = InHeadP->msgh_id - %d;\n", SubsystemBase);
    fprintf(file, "\n");
    fprintf(file, "\tif ((msgh_id > %d) || (msgh_id < 0))\n",
	    rtNumber - 1);
    fprintf(file, "\t\treturn 0;\n");
    fprintf(file, "\n");
    fprintf(file, "\treturn %s_routines[msgh_id];\n", ServerDemux);
    fprintf(file, "}\n");
    fprintf(file, "\n");

    /* symtab */

    if (GenSymTab) {
	fprintf(file,"\nmig_symtab_t _%sSymTab[] = {\n",SubsystemName);
	WriteSymTabEntries(file,stats);
	fprintf(file,"};\n");
	fprintf(file,"int _%sSymTabBase = %d;\n",SubsystemName,SubsystemBase);
	fprintf(file,"int _%sSymTabEnd = %d;\n",SubsystemName,SubsystemBase+rtNumber);
    }
}

/*
 *  Returns the return type of the server-side work function.
 *  Suitable for "extern %s serverfunc()".
 */
static const char *
ServerSideType(const routine_t *rt)
{
    if (rt->rtServerReturn == argNULL)
	return "void";
    else
	return rt->rtServerReturn->argType->itTransType;
}

static void
WriteLocalVarDecl(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    if (it->itInLine && it->itVarArray)
    {
	register const ipc_type_t *btype = it->itElement;

	fprintf(file, "\t%s %s[%d]", btype->itTransType,
		arg->argVarName, it->itNumber/btype->itNumber);
    }
    else
	fprintf(file, "\t%s %s", it->itTransType, arg->argVarName);
}

static void
WriteLocalPtrDecl(FILE *file, register const argument_t *arg)
{
    fprintf(file, "\t%s *%sP",
		FetchServerType(arg->argType->itElement),
		arg->argVarName);
}

static void
WriteServerArgDecl(FILE *file, const argument_t *arg)
{
    fprintf(file, "%s %s%s",
	    arg->argType->itTransType,
	    arg->argByReferenceServer ? "*" : "",
	    arg->argVarName);
}

/*
 *  Writes the local variable declarations which are always
 *  present:  InP, OutP, the server-side work function.
 */
static void
WriteVarDecls(FILE *file, const routine_t *rt)
{
    int i;
    boolean_t NeedMsghSize = FALSE;
    boolean_t NeedMsghSizeDelta = FALSE;

    fprintf(file, "\tregister Request *In0P = (Request *) InHeadP;\n");
    for (i = 1; i <= rt->rtMaxRequestPos; i++)
	fprintf(file, "\tregister Request *In%dP;\n", i);
    fprintf(file, "\tregister Reply *OutP = (Reply *) OutHeadP;\n");

    fprintf(file, "\tmig_external %s %s\n",
	    ServerSideType(rt), rt->rtServerName);
    fprintf(file, "\t\t(");
    WriteList(file, rt->rtArgs, WriteServerArgDecl, akbServerArg, ", ", "");
    fprintf(file, ");\n");
    fprintf(file, "\n");

    if (!rt->rtSimpleFixedReply)
	fprintf(file, "\tboolean_t msgh_simple;\n");
    else if (!rt->rtSimpleCheckRequest)
    {
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tboolean_t msgh_simple;\n");
	fprintf(file, "#endif\t/* TypeCheck */\n");
	fprintf(file, "\n");
    }

    /* if either request or reply is variable, we may need
       msgh_size_delta and msgh_size */

    if (rt->rtNumRequestVar > 0)
	NeedMsghSize = TRUE;
    if (rt->rtMaxRequestPos > 0)
	NeedMsghSizeDelta = TRUE;

    if (rt->rtNumReplyVar > 1)
	NeedMsghSize = TRUE;
    if (rt->rtMaxReplyPos > 0)
	NeedMsghSizeDelta = TRUE;

    if (NeedMsghSize)
	fprintf(file, "\tunsigned int msgh_size;\n");
    if (NeedMsghSizeDelta)
	fprintf(file, "\tunsigned int msgh_size_delta;\n");

    if (NeedMsghSize || NeedMsghSizeDelta)
	fprintf(file, "\n");
}

static void
WriteMsgError(FILE *file, const char *error_msg)
{
    fprintf(file, "\t\t{ OutP->RetCode = %s; return; }\n", error_msg);
}

static void
WriteReplyInit(FILE *file, const routine_t *rt)
{
    boolean_t printed_nl = FALSE;

    if (rt->rtSimpleFixedReply)
    {
	if (!rt->rtSimpleSendReply) /* complex reply message */
	{
	    printed_nl = TRUE;
	    fprintf(file, "\n");
	    fprintf(file,
		"\tOutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;\n");
	}
    }
    else
    {
	printed_nl = TRUE;
	fprintf(file, "\n");
	fprintf(file, "\tmsgh_simple = %s;\n", 
			  strbool(rt->rtSimpleSendReply));
    }

    if (rt->rtNumReplyVar == 0)
    {
	if (!printed_nl)
	    fprintf(file, "\n");
	fprintf(file, "\tOutP->Head.msgh_size = %d;\n", rt->rtReplySize);
    }
}

static void
WriteReplyHead(FILE *file, const routine_t *rt)
{
    if ((!rt->rtSimpleFixedReply) ||
	(rt->rtNumReplyVar > 1))
    {
	fprintf(file, "\n");
	if (rt->rtMaxReplyPos > 0)
	    fprintf(file, "\tOutP = (Reply *) OutHeadP;\n");
    }

    if (!rt->rtSimpleFixedReply)
    {
	fprintf(file, "\tif (!msgh_simple)\n");
	fprintf(file,
		"\t\tOutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;\n");
    }
    if (rt->rtNumReplyVar > 1)
	fprintf(file, "\tOutP->Head.msgh_size = msgh_size;\n");
}

static void
WriteCheckHead(FILE *file, const routine_t *rt)
{
    fprintf(file, "#if\tTypeCheck\n");
    if (rt->rtNumRequestVar > 0)
	fprintf(file, "\tmsgh_size = In0P->Head.msgh_size;\n");
    if (!rt->rtSimpleCheckRequest)
	fprintf(file, "\tmsgh_simple = !(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX);\n");

    if (rt->rtNumRequestVar > 0)
	fprintf(file, "\tif ((msgh_size < %d)",
		rt->rtRequestSize);
    else
	fprintf(file, "\tif ((In0P->Head.msgh_size != %d)",
		rt->rtRequestSize);

    if (rt->rtSimpleCheckRequest)
	fprintf(file, " ||\n\t    %s(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)",
		rt->rtSimpleReceiveRequest ? "" : "!");
    fprintf(file, ")\n");
    WriteMsgError(file, "MIG_BAD_ARGUMENTS");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");
}

static void
WriteTypeCheck(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;
    register const routine_t *rt = arg->argRoutine;

    fprintf(file, "#if\tTypeCheck\n");
    if (akCheck(arg->argKind, akbRequestQC))
	fprintf(file, "\tif (* (int *) &In%dP->%s != * (int *) &%sCheck)\n",
		arg->argRequestPos, arg->argTTName, arg->argVarName);
    else
    {
	fprintf(file, "\tif (");
	if (!it->itIndefinite) {
	    fprintf(file, "(In%dP->%s%s.msgt_inline != %s) ||\n\t    ",
		arg->argRequestPos, arg->argTTName,
		arg->argLongForm ? ".msgtl_header" : "",
		strbool(it->itInLine));
	}
	fprintf(file, "(In%dP->%s%s.msgt_longform != %s) ||\n",
		arg->argRequestPos, arg->argTTName,
		arg->argLongForm ? ".msgtl_header" : "",
		strbool(arg->argLongForm));
	if (it->itOutName == MACH_MSG_TYPE_POLYMORPHIC)
	{
	    if (!rt->rtSimpleCheckRequest)
		fprintf(file, "\t    (MACH_MSG_TYPE_PORT_ANY(In%dP->%s.msgt%s_name) && msgh_simple) ||\n",
			arg->argRequestPos, arg->argTTName,
			arg->argLongForm ? "l" : "");
	}
	else
	    fprintf(file, "\t    (In%dP->%s.msgt%s_name != %s) ||\n",
		    arg->argRequestPos, arg->argTTName,
		    arg->argLongForm ? "l" : "",
		    it->itOutNameStr);
	if (!it->itVarArray)
	    fprintf(file, "\t    (In%dP->%s.msgt%s_number != %d) ||\n",
		    arg->argRequestPos, arg->argTTName,
		    arg->argLongForm ? "l" : "",
		    it->itNumber);
	fprintf(file, "\t    (In%dP->%s.msgt%s_size != %d))\n",
		arg->argRequestPos, arg->argTTName,
		arg->argLongForm ? "l" : "",
		it->itSize);
    }
    WriteMsgError(file, "MIG_BAD_ARGUMENTS");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");
}

static void
WriteCheckArgSize(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *ptype = arg->argType;
    register const ipc_type_t *btype = ptype->itElement;
    const argument_t *count = arg->argCount;
    int multiplier = btype->itTypeSize / btype->itNumber;

    if (ptype->itIndefinite) {
	/*
	 * Check descriptor.  If out-of-line, use standard size.
	 */
	fprintf(file, "(In%dP->%s%s.msgt_inline) ? ",
		arg->argRequestPos,
		arg->argTTName,
		arg->argLongForm ? ".msgtl_header" : "");
    }

    if (btype->itTypeSize % 4 != 0)
	fprintf(file, "(");

    if (multiplier > 1)
	fprintf(file, "%d * ", multiplier);

    fprintf(file, "In%dP->%s", arg->argRequestPos, count->argMsgField);

    /* If the base type size of the data field isn`t a multiple of 4,
       we have to round up. */
    if (btype->itTypeSize % 4 != 0)
	fprintf(file, " + 3) & ~3");

    if (ptype->itIndefinite) {
	fprintf(file, " : sizeof(%s *)", FetchServerType(btype));
    }
}

static void
WriteCheckMsgSize(FILE *file, register const argument_t *arg)
{
    register const routine_t *rt = arg->argRoutine;

    /* If there aren't any more In args after this, then
       we can use the msgh_size_delta value directly in
       the TypeCheck conditional. */

    if (arg->argRequestPos == rt->rtMaxRequestPos)
    {
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tif (msgh_size != %d + (", rt->rtRequestSize);
	WriteCheckArgSize(file, arg);
	fprintf(file, "))\n");

	WriteMsgError(file, "MIG_BAD_ARGUMENTS");
	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    else
    {
	/* If there aren't any more variable-sized arguments after this,
	   then we must check for exact msg-size and we don't need to
	   update msgh_size. */

	boolean_t LastVarArg = arg->argRequestPos+1 == rt->rtNumRequestVar;

	/* calculate the actual size in bytes of the data field.  note
	   that this quantity must be a multiple of four.  hence, if
	   the base type size isn't a multiple of four, we have to
	   round up.  note also that btype->itNumber must
	   divide btype->itTypeSize (see itCalculateSizeInfo). */

	fprintf(file, "\tmsgh_size_delta = ");
	WriteCheckArgSize(file, arg);
	fprintf(file, ";\n");
	fprintf(file, "#if\tTypeCheck\n");

	/* Don't decrement msgh_size until we've checked that
	   it won't underflow. */

	if (LastVarArg)
	    fprintf(file, "\tif (msgh_size != %d + msgh_size_delta)\n",
		rt->rtRequestSize);
	else
	    fprintf(file, "\tif (msgh_size < %d + msgh_size_delta)\n",
		rt->rtRequestSize);
	WriteMsgError(file, "MIG_BAD_ARGUMENTS");

	if (!LastVarArg)
	    fprintf(file, "\tmsgh_size -= msgh_size_delta;\n");

	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    fprintf(file, "\n");
}

static const char *
InArgMsgField(register const argument_t *arg)
{
    static char buffer[100];

    /*
     *	Inside the kernel, the request and reply port fields
     *	really hold ipc_port_t values, not mach_port_t values.
     *	Hence we must cast the values.
     */

    if (IsKernelServer &&
	((akIdent(arg->argKind) == akeRequestPort) ||
	 (akIdent(arg->argKind) == akeReplyPort)))
	sprintf(buffer, "(ipc_port_t) In%dP->%s",
		arg->argRequestPos, arg->argMsgField);
    else
	sprintf(buffer, "In%dP->%s",
		arg->argRequestPos, arg->argMsgField);

    return buffer;
}

static void
WriteExtractArgValue(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    if (arg->argMultiplier > 1)
	WriteCopyType(file, it, "%s", "/* %s */ %s / %d",
		      arg->argVarName, InArgMsgField(arg), arg->argMultiplier);
    else if (it->itInTrans != strNULL)
	WriteCopyType(file, it, "%s", "/* %s */ %s(%s)",
		      arg->argVarName, it->itInTrans, InArgMsgField(arg));
    else
	WriteCopyType(file, it, "%s", "/* %s */ %s",
		      arg->argVarName, InArgMsgField(arg));
    fprintf(file, "\n");
}

static void
WriteInitializeCount(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *ptype = arg->argParent->argType;
    register const ipc_type_t *btype = ptype->itElement;

    /*
     *	Initialize 'count' argument for variable-length inline OUT parameter
     *	with maximum allowed number of elements.
     */

    fprintf(file, "\t%s = %d;\n", arg->argVarName,
	    ptype->itNumber/btype->itNumber);

    /*
     *	If the user passed in a count, then we use the minimum.
     *	We can't let the user completely override our maximum,
     *	or the user might convince the server to overwrite the buffer.
     */

    if (arg->argCInOut != argNULL) {
	const char *msgfield = InArgMsgField(arg->argCInOut);

	fprintf(file, "\tif (%s < %s)\n", msgfield, arg->argVarName);
	fprintf(file, "\t\t%s = %s;\n", arg->argVarName, msgfield);
    }

    fprintf(file, "\n");
}

static void
WriteInitializePtr(FILE *file, register const argument_t *arg)
{
    if (akCheck(arg->argKind, akbVarNeeded))
	fprintf(file, "\t%sP = %s;\n",
		arg->argVarName, arg->argVarName);
    else
	fprintf(file, "\t%sP = OutP->%s;\n",
		arg->argVarName, arg->argMsgField);
}

static void
WriteTypeCheckArg(FILE *file, register const argument_t *arg)
{
    if (akCheck(arg->argKind, akbRequest)) {
	WriteTypeCheck(file, arg);

	if (akCheck(arg->argKind, akbVariable))
	    WriteCheckMsgSize(file, arg);
    }
}

static void
WriteAdjustRequestMsgPtr(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *ptype = arg->argType;

    fprintf(file,
	"\tIn%dP = (Request *) ((char *) In%dP + msgh_size_delta - %d);\n\n",
	arg->argRequestPos+1, arg->argRequestPos,
	ptype->itTypeSize + ptype->itPadSize);
}

static void
WriteTypeCheckRequestArgs(FILE *file, register const routine_t *rt)
{
    register const argument_t *arg;
    register const argument_t *lastVarArg;

    lastVarArg = argNULL;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {

	/*
	 * Advance message pointer if the last request argument was
	 * variable-length and the request position will change.
	 */
	if (lastVarArg != argNULL &&
	    lastVarArg->argRequestPos < arg->argRequestPos)
	{
	    WriteAdjustRequestMsgPtr(file, lastVarArg);
	    lastVarArg = argNULL;
	}

	/*
	 * Type-check the argument.
	 */
	WriteTypeCheckArg(file, arg);

	/*
	 * Remember whether this was variable-length.
	 */
	if (akCheckAll(arg->argKind, akbVariable|akbRequest))
	    lastVarArg = arg;
    }
}

static void
WriteExtractArg(FILE *file, register const argument_t *arg)
{
    if (akCheckAll(arg->argKind, akbSendRcv|akbVarNeeded))
	WriteExtractArgValue(file, arg);

    if ((akIdent(arg->argKind) == akeCount) &&
	akCheck(arg->argKind, akbReturnSnd))
    {
	register ipc_type_t *ptype = arg->argParent->argType;

	if (ptype->itInLine && ptype->itVarArray)
	    WriteInitializeCount(file, arg);
    }

    if (akCheckAll(arg->argKind, akbReturnSnd|akbPointer))
	WriteInitializePtr(file, arg);
}

static void
WriteServerCallArg(FILE *file, register const argument_t *arg)
{
    const ipc_type_t *it = arg->argType;
    boolean_t NeedClose = FALSE;

    if (arg->argByReferenceServer)
	fprintf(file, "&");

    if ((it->itInTrans != strNULL) &&
	akCheck(arg->argKind, akbSendRcv) &&
	!akCheck(arg->argKind, akbVarNeeded))
    {
	fprintf(file, "%s(", it->itInTrans);
	NeedClose = TRUE;
    }

    if (akCheck(arg->argKind, akbPointer))
	fprintf(file, "%sP", arg->argVarName);
    else if (akCheck(arg->argKind, akbVarNeeded))
	fprintf(file, "%s", arg->argVarName);
    else if (akCheck(arg->argKind, akbSendRcv)) {
	if (akCheck(arg->argKind, akbIndefinite)) {
	    fprintf(file, "(In%dP->%s%s.msgt_inline) ",
		    arg->argRequestPos,
		    arg->argTTName,
		    arg->argLongForm ? ".msgtl_header" : "");
	    fprintf(file, "? %s ", InArgMsgField(arg));
	    fprintf(file, ": *((%s **)%s)",
			FetchServerType(arg->argType->itElement),
			InArgMsgField(arg));
	}
	else
	    fprintf(file, "%s", InArgMsgField(arg));
    }
    else
	fprintf(file, "OutP->%s", arg->argMsgField);

    if (NeedClose)
	fprintf(file, ")");

    if (!arg->argByReferenceServer && (arg->argMultiplier > 1))
	fprintf(file, " / %d", arg->argMultiplier);
}

static void
WriteDestroyArg(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    if (akCheck(arg->argKind, akbIndefinite)) {
	/*
	 * Deallocate only if out-of-line.
	 */
	argument_t *count = arg->argCount;
	ipc_type_t *btype = it->itElement;
	int	multiplier = btype->itTypeSize / btype->itNumber;

	fprintf(file, "\tif (!In%dP->%s%s.msgt_inline)\n",
		arg->argRequestPos,
		arg->argTTName,
		arg->argLongForm ? ".msgtl_header" : "");
	fprintf(file, "\t\tmig_deallocate(* (vm_offset_t *) %s, ",
		InArgMsgField(arg));
	if (multiplier > 1)
	    fprintf(file, "%d * ", multiplier);
	fprintf(file, " %s);\n", InArgMsgField(count));
    } else {
	if (akCheck(arg->argKind, akbVarNeeded))
	    fprintf(file, "\t%s(%s);\n", it->itDestructor, arg->argVarName);
	else
	    fprintf(file, "\t%s(%s);\n", it->itDestructor,
		InArgMsgField(arg));
    }
}

static void
WriteDestroyPortArg(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    /*
     *	If a translated port argument occurs in the body of a request
     *	message, and the message is successfully processed, then the
     *	port right should be deallocated.  However, the called function
     *	didn't see the port right; it saw the translation.  So we have
     *	to release the port right for it.
     */

    if ((it->itInTrans != strNULL) &&
	(it->itOutName == MACH_MSG_TYPE_PORT_SEND))
    {
	fprintf(file, "\n");
	fprintf(file, "\tif (IP_VALID(%s))\n", InArgMsgField(arg));
	fprintf(file, "\t\tipc_port_release_send(%s);\n", InArgMsgField(arg));
    }
}

/*
 * Check whether WriteDestroyPortArg would generate any code for arg.
 */
static boolean_t
CheckDestroyPortArg(register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    if ((it->itInTrans != strNULL) &&
	(it->itOutName == MACH_MSG_TYPE_PORT_SEND))
    {
	return TRUE;
    }
    return FALSE;
}

static void
WriteServerCall(FILE *file, const routine_t *rt)
{
    boolean_t NeedClose = FALSE;

    fprintf(file, "\t");
    if (rt->rtServerReturn != argNULL)
    {
	const argument_t *arg = rt->rtServerReturn;
	const ipc_type_t *it = arg->argType;

	fprintf(file, "OutP->%s = ", arg->argMsgField);
	if (it->itOutTrans != strNULL)
	{
	    fprintf(file, "%s(", it->itOutTrans);
	    NeedClose = TRUE;
	}
    }
    fprintf(file, "%s(", rt->rtServerName);
    WriteList(file, rt->rtArgs, WriteServerCallArg, akbServerArg, ", ", "");
    if (NeedClose)
	fprintf(file, ")");
    fprintf(file, ");\n");
}

static void
WriteGetReturnValue(FILE *file, register const routine_t *rt)
{
    if (rt->rtServerReturn != rt->rtRetCode)
	fprintf(file, "\tOutP->%s = KERN_SUCCESS;\n",
		rt->rtRetCode->argMsgField);
}

static void
WriteCheckReturnValue(FILE *file, register const routine_t *rt)
{
    if (rt->rtServerReturn == rt->rtRetCode)
    {
	fprintf(file, "\tif (OutP->%s != KERN_SUCCESS)\n",
		rt->rtRetCode->argMsgField);
	fprintf(file, "\t\treturn;\n");
    }
}

static void
WritePackArgType(FILE *file, register const argument_t *arg)
{
    fprintf(file, "\n");

    WritePackMsgType(file, arg->argType,
		     arg->argType->itIndefinite ? d_NO : arg->argDeallocate,
		     arg->argLongForm, !IsKernelServer,
		     "OutP->%s", "%s", arg->argTTName);
}

static void
WritePackArgValue(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    fprintf(file, "\n");

    if (it->itInLine && it->itVarArray) {

	if (it->itString) {
	    /*
	     *	Copy variable-size C string with mig_strncpy.
	     *	Save the string length (+ 1 for trailing 0)
	     *	in the argument`s count field.
	     */
	    fprintf(file,
		"\tOutP->%s = mig_strncpy(OutP->%s, %s, %d);\n",
		arg->argCount->argMsgField,
		arg->argMsgField,
		arg->argVarName,
		it->itNumber);
	}
	else {
	    register argument_t *count = arg->argCount;
	    register ipc_type_t *btype = it->itElement;

	    /* Note btype->itNumber == count->argMultiplier */

	    if (it->itIndefinite) {
		/*
		 * If we are packing argument, it must be from
		 * a local variable.
		 */
		fprintf(file, "\tif (%sP != %s) {\n",
			arg->argVarName,
			arg->argVarName);
		fprintf(file, "\t\tOutP->%s%s.msgt_inline = FALSE;\n",
			arg->argTTName,
			arg->argLongForm ? ".msgtl_header" : "");
		if (arg->argDeallocate == d_YES)
		    fprintf(file, "\t\tOutP->%s%s.msgt_deallocate = TRUE;\n",
			    arg->argTTName,
			    arg->argLongForm ? ".msgtl_header" : "");
		else if (arg->argDeallocate == d_MAYBE)
		    fprintf(file, "\t\tOutP->%s%s.msgt_deallocate = %s;\n",
			    arg->argTTName,
			    arg->argLongForm ? ".msgtl_header" : "",
			    arg->argDealloc->argVarName);
		fprintf(file, "\t\t*((%s **)OutP->%s) = %sP;\n",
			FetchServerType(btype),
			arg->argMsgField,
			arg->argVarName);
		if (!arg->argRoutine->rtSimpleFixedReply)
		    fprintf(file, "\t\tmsgh_simple = FALSE;\n");
		fprintf(file, "\t}\n\telse {\n\t");
	    }
	    fprintf(file, "\tmemcpy(OutP->%s, %s, ",
		    arg->argMsgField, arg->argVarName);
	    if (btype->itTypeSize > 1)
		fprintf(file, "%d * ",
			btype->itTypeSize);
	    fprintf(file, "%s);\n",
		count->argVarName);
	    if (it->itIndefinite)
		fprintf(file, "\t}\n");
	}
    }
    else if (arg->argMultiplier > 1)
	WriteCopyType(file, it, "OutP->%s", "/* %s */ %d * %s",
		      arg->argMsgField,
		      arg->argMultiplier,
		      arg->argVarName);
    else if (it->itOutTrans != strNULL)
	WriteCopyType(file, it, "OutP->%s", "/* %s */ %s(%s)",
		      arg->argMsgField, it->itOutTrans, arg->argVarName);
    else
	WriteCopyType(file, it, "OutP->%s", "/* %s */ %s",
		      arg->argMsgField, arg->argVarName);
}

static void
WriteCopyArgValue(FILE *file, register const argument_t *arg)
{
    fprintf(file, "\n");
    WriteCopyType(file, arg->argType, "/* %d */ OutP->%s", "In%dP->%s",
		  arg->argRequestPos, arg->argMsgField);
}

static void
WriteAdjustMsgSimple(FILE *file, register const argument_t *arg)
{
    /* akbVarNeeded must be on */

    if (!arg->argRoutine->rtSimpleFixedReply)
    {
	fprintf(file, "\n");
	fprintf(file, "\tif (MACH_MSG_TYPE_PORT_ANY(%s))\n", arg->argVarName);
	fprintf(file, "\t\tmsgh_simple = FALSE;\n");
    }
}

static void
WriteAdjustMsgCircular(FILE *file, register const argument_t *arg)
{
    fprintf(file, "\n");

    if (arg->argType->itOutName == MACH_MSG_TYPE_POLYMORPHIC)
	fprintf(file, "\tif (%s == MACH_MSG_TYPE_PORT_RECEIVE)\n",
		arg->argPoly->argVarName);

    /*
     *	The carried port right can be accessed in OutP->XXXX.  Normally
     *	the server function stuffs it directly there.  If it is InOut,
     *	then it has already been copied into the reply message.
     *	If the server function deposited it into a variable (perhaps
     *	because the reply message is variable-sized) then it has already
     *	been copied into the reply message.  Note we must use InHeadP
     *	(or In0P->Head) and OutHeadP to access the message headers,
     *	because of the variable-sized messages.
     */

    fprintf(file, "\tif (IP_VALID((ipc_port_t) InHeadP->msgh_reply_port) &&\n");
    fprintf(file, "\t    IP_VALID((ipc_port_t) OutP->%s) &&\n", arg->argMsgField);
    fprintf(file, "\t    ipc_port_check_circularity((ipc_port_t) OutP->%s, (ipc_port_t) InHeadP->msgh_reply_port))\n", arg->argMsgField);
    fprintf(file, "\t\tOutHeadP->msgh_bits |= MACH_MSGH_BITS_CIRCULAR;\n");
}

/*
 * Calculate the size of a variable-length message field.
 */
static void
WriteArgSize(FILE *file, register const argument_t *arg)
{
    register const ipc_type_t *ptype = arg->argType;
    register int bsize = ptype->itElement->itTypeSize;
    register const argument_t *count = arg->argCount;

    if (ptype->itIndefinite) {
	/*
	 * Check descriptor.  If out-of-line, use standard size.
	 */
	fprintf(file, "(OutP->%s%s.msgt_inline) ? ",
		arg->argTTName,
		arg->argLongForm ? ".msgtl_header" : "");
    }

    if (bsize % 4 != 0)
	fprintf(file, "(");

    if (bsize > 1)
	fprintf(file, "%d * ", bsize);
    if (ptype->itString)
	/* get count from descriptor in message */
	fprintf(file, "OutP->%s", count->argMsgField);
    else
	/* get count from argument */
	fprintf(file, "%s", count->argVarName);

    /*
     * If the base type size is not a multiple of sizeof(int) [4],
     * we have to round up.
     */
    if (bsize % 4 != 0)
	fprintf(file, " + 3) & ~3");

    if (ptype->itIndefinite) {
	fprintf(file, " : sizeof(%s *)",
		FetchServerType(ptype->itElement));
    }
}

/*
 * Adjust message size and advance reply pointer.
 * Called after packing a variable-length argument that
 * has more arguments following.
 */
static void
WriteAdjustMsgSize(FILE *file, register const argument_t *arg)
{
    register routine_t *rt = arg->argRoutine;
    register ipc_type_t *ptype = arg->argType;

    /* There are more Out arguments.  We need to adjust msgh_size
       and advance OutP, so we save the size of the current field
       in msgh_size_delta. */

    fprintf(file, "\tmsgh_size_delta = ");
    WriteArgSize(file, arg);
    fprintf(file, ";\n");

    if (rt->rtNumReplyVar == 1)
	/* We can still address the message header directly.  Fill
	   in the size field. */

	fprintf(file, "\tOutP->Head.msgh_size = %d + msgh_size_delta;\n",
			rt->rtReplySize);
    else
    if (arg->argReplyPos == 0)
	/* First variable-length argument.  The previous msgh_size value
	   is the minimum reply size. */

	fprintf(file, "\tmsgh_size = %d + msgh_size_delta;\n",
		rt->rtReplySize);
    else
	fprintf(file, "\tmsgh_size += msgh_size_delta;\n");

    fprintf(file,
	"\tOutP = (Reply *) ((char *) OutP + msgh_size_delta - %d);\n",
	ptype->itTypeSize + ptype->itPadSize);
}

/*
 * Calculate the size of the message.  Called after the
 * last argument has been packed.
 */
static void
WriteFinishMsgSize(FILE *file, register const argument_t *arg)
{
    /* No more Out arguments.  If this is the only variable Out
       argument, we can assign to msgh_size directly. */

    if (arg->argReplyPos == 0) {
	fprintf(file, "\tOutP->Head.msgh_size = %d + (",
			arg->argRoutine->rtReplySize);
	WriteArgSize(file, arg);
	fprintf(file, ");\n");
    }
    else {
	fprintf(file, "\tmsgh_size += ");
	WriteArgSize(file, arg);
	fprintf(file, ";\n");
    }
}

static void
WritePackArg(FILE *file, register const argument_t *arg)
{
    if (akCheck(arg->argKind, akbReplyInit))
	WritePackArgType(file, arg);

    if ((akIdent(arg->argKind) == akePoly) &&
	akCheck(arg->argKind, akbReturnSnd))
	WriteAdjustMsgSimple(file, arg);

    if (akCheckAll(arg->argKind, akbReturnSnd|akbVarNeeded))
	WritePackArgValue(file, arg);
    else if (akCheckAll(arg->argKind, akbReturnSnd|akbVariable)) {
	register const ipc_type_t *it = arg->argType;

	if (it->itString) {
	    /* Need to call strlen to calculate the size of the argument. */
	    fprintf(file, "\tOutP->%s = strlen(OutP->%s) + 1;\n",
		    arg->argCount->argMsgField, arg->argMsgField);
	} else if (it->itIndefinite) {
	    /*
	     * We know that array is in reply message.
	     */
	    fprintf(file, "\tif (%sP != OutP->%s) {\n",
			arg->argVarName,
			arg->argMsgField);
	    fprintf(file, "\t\tOutP->%s%s.msgt_inline = FALSE;\n",
		    arg->argTTName,
		    arg->argLongForm ? ".msgtl_header" : "");
	    if (arg->argDeallocate == d_YES)
		fprintf(file, "\t\tOutP->%s%s.msgt_deallocate = TRUE;\n",
			arg->argTTName,
			arg->argLongForm ? ".msgtl_header" : "");
	    else if (arg->argDeallocate == d_MAYBE)
		fprintf(file, "\t\tOutP->%s%s.msgt_deallocate = %s;\n",
			arg->argTTName,
			arg->argLongForm ? ".msgtl_header" : "",
			arg->argDealloc->argVarName);
	    fprintf(file, "\t\t*((%s **)OutP->%s) = %sP;\n",
			FetchServerType(it->itElement),
			arg->argMsgField,
			arg->argVarName);
	    if (!arg->argRoutine->rtSimpleFixedReply)
		fprintf(file, "\t\tmsgh_simple = FALSE;\n");
	    fprintf(file, "\t}\n");
	}
    }

    if (akCheck(arg->argKind, akbReplyCopy))
	WriteCopyArgValue(file, arg);

    /*
     *	If this is a KernelServer, and the reply message contains
     *	a receive right, we must check for the possibility of a
     *	port/message circularity.  If queueing the reply message
     *	would cause a circularity, we mark the reply message
     *	with the circular bit.
     */

    if (IsKernelServer &&
	akCheck(arg->argKind, akbReturnSnd) &&
	((arg->argType->itOutName == MACH_MSG_TYPE_PORT_RECEIVE) ||
	 (arg->argType->itOutName == MACH_MSG_TYPE_POLYMORPHIC)))
	WriteAdjustMsgCircular(file, arg);
}

/*
 * Handle reply arguments - fill in message types and copy arguments
 * that need to be copied.
 */
static void
WritePackReplyArgs(FILE *file, register const routine_t *rt)
{
    register const argument_t *arg;
    register const argument_t *lastVarArg;

    lastVarArg = argNULL;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {

	/*
	 * Adjust message size and advance message pointer if
	 * the last reply argument was variable-length and the
	 * request position will change.
	 */
	if (lastVarArg != argNULL &&
	    lastVarArg->argReplyPos < arg->argReplyPos)
	{
	    WriteAdjustMsgSize(file, lastVarArg);
	    lastVarArg = argNULL;
	}

	/*
	 * Copy the argument
	 */
	WritePackArg(file, arg);

	/*
	 * Remember whether this was variable-length.
	 */
	if (akCheckAll(arg->argKind, akbReturnSnd|akbVariable))
	    lastVarArg = arg;
    }

    /*
     * Finish the message size.
     */
    if (lastVarArg != argNULL)
	WriteFinishMsgSize(file, lastVarArg);
}

static void
WriteFieldDecl(FILE *file, const argument_t *arg)
{
    WriteFieldDeclPrim(file, arg, FetchServerType);
}

static void
WriteRoutine(FILE *file, register const routine_t *rt)
{
    fprintf(file, "\n");

    fprintf(file, "/* %s %s */\n", rtRoutineKindToStr(rt->rtKind), rt->rtName);
    fprintf(file, "mig_internal void _X%s\n", rt->rtName);
    fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");

    fprintf(file, "{\n");
    WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbRequest, "Request");
    WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbReply, "Reply");

    WriteVarDecls(file, rt);

    WriteList(file, rt->rtArgs, WriteCheckDecl, akbRequestQC, "\n", "\n");
    WriteList(file, rt->rtArgs,
	      IsKernelServer ? WriteTypeDeclOut : WriteTypeDeclIn,
	      akbReplyInit, "\n", "\n");

    WriteList(file, rt->rtArgs, WriteLocalVarDecl,
	      akbVarNeeded, ";\n", ";\n\n");
    WriteList(file, rt->rtArgs, WriteLocalPtrDecl,
	      akbPointer, ";\n", ";\n\n");

    WriteCheckHead(file, rt);

    WriteTypeCheckRequestArgs(file, rt);
    WriteList(file, rt->rtArgs, WriteExtractArg, akbNone, "", "");

    WriteServerCall(file, rt);
    WriteGetReturnValue(file, rt);

    WriteReverseList(file, rt->rtArgs, WriteDestroyArg, akbDestroy, "", "");

    /*
     * For one-way routines, it doesn`t make sense to check the return
     * code, because we return immediately afterwards.  However,
     * kernel servers may want to deallocate port arguments - and the
     * deallocation must not be done if the return code is not KERN_SUCCESS.
     */
    if (rt->rtOneWay || rt->rtNoReplyArgs)
    {
	if (IsKernelServer)
	{
	    if (rtCheckMaskFunction(rt->rtArgs, akbSendBody|akbSendRcv,
				CheckDestroyPortArg))
	    {
		WriteCheckReturnValue(file, rt);
	    }
	    WriteReverseList(file, rt->rtArgs, WriteDestroyPortArg,
			 akbSendBody|akbSendRcv, "", "");
	}
    }
    else
    {
	WriteCheckReturnValue(file, rt);

	if (IsKernelServer)
	    WriteReverseList(file, rt->rtArgs, WriteDestroyPortArg,
			 akbSendBody|akbSendRcv, "", "");

	WriteReplyInit(file, rt);
	WritePackReplyArgs(file, rt);
	WriteReplyHead(file, rt);
    }

    fprintf(file, "}\n");
}

void
WriteServer(FILE *file, const statement_t *stats)
{
    register const statement_t *stat;

    WriteProlog(file);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteRoutine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skSImport:
	    WriteImport(file, stat->stFileName);
	    break;
	  case skUImport:
	    break;
	  default:
	    fatal("WriteServer(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
    WriteEpilog(file, stats);
}
