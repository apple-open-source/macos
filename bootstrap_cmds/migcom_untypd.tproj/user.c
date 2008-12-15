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
 * 29-Sep-92  John Loverso (loverso) at Open Software Foundation
 *	Changes to deallocate memory on various error codes
 *
 * 92/03/03  16:25:33  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  12:32:55  jeffreyh]
 * 
 * 92/01/14  16:46:59  rpd
 * 	Changed CountInOut code generation, to send the minimum
 * 	of the reply msg buffer size and the user's buffer size.
 * 	[92/01/13            rpd]
 * 
 * 	Fixed WriteExtractArgValue/itIndefinite, in the case when
 * 	the data is in-line but doesn't fit.
 * 	Fixed Indefinite code generation, to allow short type descriptors.
 * 	Added deallocate bit handling to Indefinite code generation.
 * 	[92/01/08            rpd]
 * 
 * 92/01/03  20:30:38  dbg
 * 	Redo handling of OUT arrays that are passed in-line or
 * 	out-of-line.  Treat more like out-of-line arrays:
 * 	user allocates buffer and pointer
 * 	fills in pointer with buffer address
 * 	passes pointer to stub
 * 	stub copies data to *pointer, or changes pointer
 * 	User can always use *pointer.
 * 
 * 	Change argByReferenceUser to a field in argument_t.
 * 	[91/09/04            dbg]
 * 
 * 91/08/28  11:17:34  jsb
 * 	Added MIG_SERVER_DIED.
 * 	[91/08/21            rpd]
 * 	Removed Camelot and TrapRoutine support.
 * 	Changed MsgKind to MsgSeqno.
 * 	[91/08/12            rpd]
 * 
 * 91/07/31  18:11:31  dbg
 * 	Allow indefinite-length variable arrays.  They may be copied
 * 	either in-line or out-of-line, depending on size.
 * 
 * 	Copy variable-length C Strings with mig_strncpy, to combine
 * 	'strcpy' and 'strlen' operations.
 * 
 * 	New method for advancing request message pointer past
 * 	variable-length arguments.  We no longer have to know the order
 * 	of variable-length arguments and their count arguments.
 * 
 * 	Remove redundant assignments (to msgh_simple, msgh_size) in
 * 	generated code.
 * 	[91/07/17            dbg]
 * 
 * 91/06/26  14:39:44  rpd
 * 	Removed the dummy user initialization function,
 * 	which was kept for backwards-compatibility.
 * 	[91/06/26            rpd]
 * 
 * 91/06/25  10:32:22  rpd
 * 	Cast request and reply ports to mach_port_t in KernelUser stubs.
 * 	[91/05/27            rpd]
 * 
 * 	Changed HeaderFileName to UserHeaderFileName.
 * 	Changed WriteVarDecl to WriteUserVarDecl.
 * 	[91/05/23            rpd]
 * 
 * 91/02/05  17:56:20  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:56:28  mrt]
 * 
 * 90/06/19  23:01:20  rpd
 * 	Added UserFilePrefix support.
 * 	[90/06/03            rpd]
 * 
 * 90/06/02  15:06:03  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:14:40  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 * 21-Feb-89  David Golub (dbg) at Carnegie-Mellon University
 *	Get name for header file from HeaderFileName, since it can
 *	change.
 *
 *  8-Feb-89  David Golub (dbg) at Carnegie-Mellon University
 *	Added WriteUserIndividual to put each user-side routine in its
 *	own file.
 *
 *  8-Jul-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Declared routines to be mig_external instead of extern,
 *	where mig_external is conditionally defined in <subsystem>.h.
 *	The Avalon folks want to define mig_external to be static
 *	in their compilations because they inlcude the User.c code in
 *	their programs.
 *
 * 23-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed the include of camelot_types.h to cam/camelot_types.h
 *
 * 19-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added comments for each routine. Called WriteMsgError
 *	for MIG_ARRAY_TOO_LARGE errors.
 *
 * 19-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Change variable-length inline array declarations to use
 *	maximum size specified to Mig.  Make message variable
 *	length if the last item in the message is variable-length
 *	and inline.  Use argMultiplier field to convert between
 *	argument and IPC element counts.
 *
 * 19-Jan-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	In WriteInitRoutine changed reference from reply_port; to reply_port++;
 *	for lint code.
 *
 * 17-Jan-88  David Detlefs (dld) at Carnegie-Mellon University
 *	Modified to produce C++ compatible code via #ifdefs.
 *	All changes have to do with argument declarations.
 *
 * 16-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	Handle variable-length inline arrays.
 *
 * 22-Oct-87  Mary Thompson (mrt) at Carnegie-Mellon University
 * 	Added a reference to rep_port in the InitRoutine
 *	with an ifdef lint conditional.
 *
 * 22-Sep-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Fixed check for TransId to be a not equal test
 *	rather than an equal test.
 *
 *  2-Sep-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed WriteCheckIdentity to check TransId instead
 *	of msgh_id for a returned camelot reply
 *
 * 24-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added a  LINTLIBRARY  line to keep lint
 *	from complaining about routines that are not used.
 *
 * 21-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added Flag parameter to WritePackMsgType.
 *
 * 12-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Made various camelot changes: include of camelot_types.h
 *	Check for death_pill before correct msg-id.
 *
 * 10-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Renamed get_reply_port and dealloc_reply_port to
 *	mig_get_reply_port and mig_dealloc_reply_port.
 *	Fixed WriteRequestHead to handle MsgType parameter.
 *
 *  3-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Fixed to generate code that is the same for multi-threaded and
 *	single threaded use. Gets reply port from library routine
 *	get_reply_port and deallocates with routine
 *	dealloc_reply_port. Removed all routines in mig interface code
 *	to keep track of the reply port. The init routine still exists
 *	but does nothing.
 * 
 * 29-Jul_87  Mary Thompson (mrt) at Carnegie-Mellon University
 * 	Fixed call to WriteVarDecl to use correspond to
 *	the changes that were made in that routine.
 *
 * 16-Jul-87  Robert Sansom (rds) at Carnegie Mellon University
 *	Added write of MsgType to WriteSetMsgTypeRoutine.
 *
 *  8-Jun-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Removed #include of sys/types.h from WriteIncludes.
 *	Changed the KERNEL include from ../h to sys/
 *	Removed extern from WriteUser to make hi-c happy
 *
 * 28-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#include <assert.h>

#include "type.h"
#include <mach/message.h>
#include "write.h"
#include "error.h"
#include "utils.h"
#include "global.h"

char *MessAllocRoutine = "mig_user_allocate";
char *MessFreeRoutine = "mig_user_deallocate";

char stRetCode[] = "ReturnValue";
char stRetNone[] = "";

void WriteLogDefines();
void WriteIdentificationString();

static void
WriteKPD_Iterator(file, in, overwrite, varying, arg, bracket)
    FILE *file;
    boolean_t in, overwrite, varying;
    argument_t *arg;
    boolean_t bracket;
{
    register ipc_type_t *it = arg->argType;
    char string[MAX_STR_LEN];

    fprintf(file, "\t{\n");
    fprintf(file, "\t    register\t%s\t*ptr;\n", it->itUserKPDType);
    fprintf(file, "\t    register\ti");
    if (varying && !in)
	fprintf(file, ", j");
    fprintf(file, ";\n\n");

    if (in)
	sprintf(string, "InP");
    else if (overwrite)
	sprintf(string, "InOvTemplate");
    else
	sprintf(string, "Out%dP", arg->argRequestPos);

    fprintf(file, "\t    ptr = &%s->%s[0];\n", string, arg->argMsgField);

    if (varying) {
	register argument_t *count = arg->argCount;
	register char *cref = count->argByReferenceUser ? "*" : "";

	if (in || overwrite) {
	    fprintf(file, "\t    if (%s%s > %d)\n", cref, count->argVarName,
		it->itKPD_Number);
	    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_ARRAY_TOO_LARGE");
            fprintf(file, "\t    for (i = 0; i < %s%s; ptr++, i++) %s\n",
		cref, count->argVarName, (bracket) ? "{" : "");
	} else {
	    fprintf(file, "\t    j = min(Out%dP->%s, %s%s);\n", count->argReplyPos, 
		count->argVarName, cref, count->argVarName);
	    fprintf(file, "\t    for (i = 0; i < j; ptr++, i++) %s\n",
		(bracket) ? "{" : "");
	}
    } else
        fprintf(file, "\t    for (i = 0; i < %d; ptr++, i++) %s\n", it->itKPD_Number,
	    (bracket) ? "{" : "");
}

/*************************************************************
 *	Writes the standard includes. The subsystem specific
 *	includes  are in <SubsystemName>.h and writen by
 *	header:WriteHeader. Called by WriteProlog.
 *************************************************************/
static void
WriteIncludes(file)
    FILE *file;
{
    if (IsKernelServer)
    {
	/*
	 *	We want to get the user-side definitions of types
	 *	like task_t, ipc_space_t, etc. in mach/mach_types.h.
	 */

	fprintf(file, "#undef\tMACH_KERNEL\n");

	if (InternalHeaderFileName != strNULL)
	{
	    register char *cp;

	    /* Strip any leading path from InternalHeaderFileName. */
	    cp = strrchr(InternalHeaderFileName, '/');
	    if (cp == 0)
		cp = InternalHeaderFileName;
	    else
		cp++;	/* skip '/' */
	    fprintf(file, "#include \"%s\"\n", cp);
	}
    }

    if (UserHeaderFileName != strNULL)
    {
	register char *cp;

	/* Strip any leading path from UserHeaderFileName. */
	cp = strrchr(UserHeaderFileName, '/');
	if (cp == 0)
	    cp = UserHeaderFileName;
	else
	    cp++;	/* skip '/' */
	fprintf(file, "#include \"%s\"\n", cp);
    } else {
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
	if (IsKernelUser) {
	    fprintf(file, "#include <ipc/ipc_port.h>\n");
	    fprintf(file, "#include <kern/ipc_mig.h>\n");
	} else
	    fprintf(file, "#include <mach/port.h>\n");
	fprintf(file, "\n");
    }

    if (UseEventLogger) {
	if (IsKernelUser) {
	    fprintf(file, "#ifdef\tMACH_KERNEL\n");
	    fprintf(file, "#include <mig_debug.h>\n");
	    fprintf(file, "#endif\t/* MACH_KERNEL */\n");
        }
	fprintf(file, "#if  MIG_DEBUG\n"); 
	fprintf(file, "#include <mach/mig_log.h>\n");
	fprintf(file, "#endif /* MIG_DEBUG */\n"); 
    }
    fprintf(file, "/* LINTLIBRARY */\n");
    fprintf(file, "\n");
    if (!BeAnsiC) {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "#else\t/* %s */\n", NewCDecl);
        fprintf(file, "extern mach_port_t mig_get_reply_port();\n");
        fprintf(file, "extern void mig_dealloc_reply_port();\n");
	fprintf(file, "extern char *%s();\n", MessAllocRoutine);
	fprintf(file, "extern void %s();\n", MessFreeRoutine);
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }
    fprintf(file, "\n");
}

static void
WriteGlobalDecls(file)
    FILE *file;
{
    if (RCSId != strNULL)
	WriteRCSDecl(file, strconcat(SubsystemName, "_user"), RCSId);

    fprintf(file, "#define msgh_request_port\tmsgh_remote_port\n");
    fprintf(file, "#define msgh_reply_port\t\tmsgh_local_port\n");
    fprintf(file, "\n");
    if (UseEventLogger)
	WriteLogDefines(file, "MACH_MSG_LOG_USER");
    fprintf(file, "\n");
}

/*************************************************************
 *	Writes the standard #includes, #defines, and
 *	RCS declaration. Called by WriteUser.
 *************************************************************/
static void
WriteProlog(file)
    FILE *file;
{
    WriteIdentificationString(file);
    WriteIncludes(file);
    WriteBogusDefines(file);
    WriteGlobalDecls(file);
}

/*ARGSUSED*/
static void
WriteEpilog(file)
    FILE *file;
{
}

static string_t
WriteHeaderPortType(arg)
    argument_t *arg;
{
    if (arg->argType->itInName == MACH_MSG_TYPE_POLYMORPHIC)
	return arg->argPoly->argVarName;
    else
	return arg->argType->itInNameStr;
}

static void
WriteRequestHead(file, rt)
    FILE *file;
    routine_t *rt;
{
    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) 
	fprintf(file, "ready_to_send:\n");

    if (rt->rtMaxRequestPos > 0)
	if (rt->rtOverwrite)
	    fprintf(file, "\tInP = &MessRequest;\n");
	else
	    fprintf(file, "\tInP = &Mess%sIn;\n",
		    (rt->rtMessOnStack ? "." : "->"));

    fprintf(file, "\tInP->Head.msgh_bits =");
    if (rt->rtRetCArg == argNULL && !rt->rtSimpleRequest)
        fprintf(file, " MACH_MSGH_BITS_COMPLEX|");
    fprintf(file, "\n");
    fprintf(file, "\t\tMACH_MSGH_BITS(%s, %s);\n",
    WriteHeaderPortType(rt->rtRequestPort),
    WriteHeaderPortType(rt->rtReplyPort));
    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) {
        fprintf(file, "\tif (!%s)\n", rt->rtRetCArg->argVarName); 
	fprintf(file, "\t\tInP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;\n");
    }


    fprintf(file, "\t/* msgh_size passed as argument */\n");

    /*
     *	KernelUser stubs need to cast the request and reply ports
     *	from ipc_port_t to mach_port_t.
     */

    if (IsKernelUser)
	fprintf(file, "\tInP->%s = (mach_port_t) %s;\n",
		rt->rtRequestPort->argMsgField,
		rt->rtRequestPort->argVarName);
    else
	fprintf(file, "\tInP->%s = %s;\n",
		rt->rtRequestPort->argMsgField,
		rt->rtRequestPort->argVarName);

    if (akCheck(rt->rtReplyPort->argKind, akbUserArg)) {
	if (IsKernelUser)
	    fprintf(file, "\tInP->%s = (mach_port_t) %s;\n",
		    rt->rtReplyPort->argMsgField,
		    rt->rtReplyPort->argVarName);
	else
	    fprintf(file, "\tInP->%s = %s;\n",
		    rt->rtReplyPort->argMsgField,
		    rt->rtReplyPort->argVarName);
    } else if (rt->rtOneWay || IsKernelUser)
	fprintf(file, "\tInP->%s = MACH_PORT_NULL;\n",
		rt->rtReplyPort->argMsgField);
    else
	fprintf(file, "\tInP->%s = mig_get_reply_port();\n",
		rt->rtReplyPort->argMsgField);

    fprintf(file, "\tInP->Head.msgh_id = %d;\n",
	    rt->rtNumber + SubsystemBase);
}

/*************************************************************
 *  Writes declarations for the message types, variables
 *  and return  variable if needed. Called by WriteRoutine.
 *************************************************************/
static void
WriteVarDecls(file, rt)
    FILE *file;
    routine_t *rt;
{
    register i;

    if (rt->rtOverwrite) {
	fprintf(file, "\tRequest MessRequest;\n");
	fprintf(file, "\tregister Request *InP = &MessRequest;\n\n");

	fprintf(file, "\tunion {\n");
	fprintf(file, "\t\tOverwriteTemplate In;\n");
	    fprintf(file, "\t\tReply Out;\n");
	fprintf(file, "\t} MessReply;\n");

	fprintf(file, "\tregister OverwriteTemplate *InOvTemplate = &MessReply.In;\n");
	fprintf(file, "\tregister Reply *Out0P = &MessReply.Out;\n");
	for (i = 1; i <= rt->rtMaxReplyPos; i++)
	    fprintf(file, "\tregister Reply *Out%dP;\n", i);
    } else {
        if (rt->rtMessOnStack)
	    fprintf(file, "\tunion {\n");
	else
	    fprintf(file, "\tunion %sMessU {\n", rt->rtName);
	fprintf(file, "\t\tRequest In;\n");
	if (!rt->rtOneWay)
	    fprintf(file, "\t\tReply Out;\n");
        if (rt->rtMessOnStack)
	    fprintf(file, "\t} Mess;\n");
        else
	    fprintf(file, "\t} *Mess = (union %sMessU *) %s(sizeof(*Mess));\n",
		rt->rtName, MessAllocRoutine);
	fprintf(file, "\n");

	fprintf(file, "\tregister Request *InP = &Mess%sIn;\n",
	    (rt->rtMessOnStack ? "." : "->"));
	if (!rt->rtOneWay) {
	    fprintf(file, "\tregister Reply *Out0P = &Mess%sOut;\n",
		(rt->rtMessOnStack ? "." : "->"));
	    for (i = 1; i <= rt->rtMaxReplyPos; i++)
		fprintf(file, "\tregister Reply *Out%dP;\n", i);
        }
    }
    
    fprintf(file, "\n");

    if (!rt->rtOneWay || (! rt->rtMessOnStack))
	fprintf(file, "\tmach_msg_return_t msg_result;\n");

    if (!rt->rtOneWay &&
	     !(rt->rtSimpleReply)) {
	fprintf(file, "\tboolean_t msgh_simple;\n");
    }

    if (rt->rtNumRequestVar > 0 || (!rt->rtOneWay && !rt->rtNoReplyArgs))
	fprintf(file, "\tunsigned int msgh_size;\n");

    /* if either request or reply is variable, we need msgh_size_delta */
    if ((rt->rtMaxRequestPos > 0) ||
	(rt->rtMaxReplyPos > 0))
	fprintf(file, "\tunsigned int msgh_size_delta;\n");

    if (rt->rtUserImpl) {
	fprintf(file, "\tmach_msg_format_0_trailer_t *TrailerP;\n");
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tunsigned int trailer_size;\n");
	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    fprintf(file, "\n");
}

static void
WriteReturn(file, rt, before, value, after)
    FILE *file;
    routine_t *rt;
    char *value;
{
    if (rt->rtMessOnStack && value != stRetCode)
    {
	/* get the easy case (no braces needed) out of the way */
	fprintf(file, "%sreturn%s%s;%s",
	    before, (*value ? " " : ""), value, after);
	return;
    }

    /* get the easy case (no braces needed) out of the way */
    if (rt->rtMessOnStack)
    {
	if (value == stRetNone)
	    fprintf(file, "%sreturn;%s",
		before, after);
	else if (value == stRetCode)
	    fprintf(file, "%sreturn Out0P->RetCode;%s",
		before, after);
	else
	    fprintf(file, "%sreturn %s;%s",
		before, value, after);
	return;
    }

    fprintf(file, "%s{ ", before);

    if (value == stRetCode)
	fprintf(file, "%s ReturnValue = Out0P->RetCode; ", ReturnTypeStr(rt));

    if (! rt->rtMessOnStack)
	fprintf(file, "%s((char *) Mess, sizeof(*Mess)); ", MessFreeRoutine);

    if (value == stRetNone)
	fprintf(file, "return; }%s", after);
    else
	fprintf(file, "return %s; }%s", value, after);
}

static void
WriteRetCodeArg(file, rt)
    FILE *file;
    register routine_t *rt;
{
    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) {
	register argument_t *arg = rt->rtRetCArg;

	fprintf(file, "\tif (%s) {\n", arg->argVarName);
	fprintf(file, "\t\t((mig_reply_error_t *)InP)->RetCode = %s;\n", arg->argVarName);
	fprintf(file, "\t\t((mig_reply_error_t *)InP)->NDR = NDR_record;\n");
	fprintf(file, "\t\tgoto ready_to_send;\n");
	fprintf(file, "\t}\n\n");
    }
}

/*************************************************************
 *   Writes the send call when there is to be no subsequent
 *   receive. Called by WriteRoutine SimpleRoutines
 *************************************************************/
static void
WriteMsgSend(file, rt)
    FILE *file;
    routine_t *rt;
{
    char *SendSize = "";
    char string[MAX_STR_LEN];
    char *MsgResult = "";

    if (rt->rtNumRequestVar == 0)
	SendSize = "sizeof(Request)";
    else
	SendSize = "msgh_size";

    if (!rt->rtMessOnStack)
	MsgResult = "msg_result =";
    else
	MsgResult = "return";
        
    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) {
	sprintf(string, "(%s) ? sizeof(mig_reply_error_t) : ", 
	    rt->rtRetCArg->argVarName);
	SendSize = strconcat(string, SendSize);
    }

    if (IsKernelUser)
    {
	fprintf(file, "\t%s mach_msg_send_from_kernel(", MsgResult);
	fprintf(file, "&InP->Head, %s);\n", SendSize);
    }
    else
    {
	fprintf(file, "\t%s mach_msg_overwrite(&InP->Head, MACH_SEND_MSG|%s, %s, 0,",
		MsgResult,
		rt->rtMsgOption->argVarName,
		SendSize);
	fprintf(file, " MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,");
	fprintf(file, " (mach_msg_header_t *) 0, 0);\n");
    }
    if (! rt->rtMessOnStack)
	WriteReturn(file, rt, "\t\t", "msg_result", "\n");
}

/*************************************************************
 *  Writes to code to check for error returns from receive.
 *  Called by WriteMsgSendReceive and WriteMsgRPC
 *************************************************************/
static void
WriteMsgCheckReceive(file, rt, success)
    FILE *file;
    routine_t *rt;
    char *success;
{
    fprintf(file, "\tif (msg_result != %s) {\n", success);
    if (!akCheck(rt->rtReplyPort->argKind, akbUserArg) && !IsKernelUser)
    {
	/* If we aren't using a user-supplied reply port, then
	   deallocate the reply port when it is invalid or
	   for TIMED_OUT errors. */
#ifdef DeallocOnAnyError
	fprintf(file, "\t\tmig_dealloc_reply_port();\n");
#else
	fprintf(file, "\t\tswitch (msg_result) {\n");
	fprintf(file, "\t\tcase MACH_SEND_INVALID_REPLY:\n");
	fprintf(file, "\t\tcase MACH_RCV_INVALID_NAME:\n");
	fprintf(file, "\t\tcase MACH_RCV_IN_SET:\n");
	fprintf(file, "\t\tcase MACH_RCV_PORT_DIED:\n");
	fprintf(file, "\t\tcase MACH_RCV_PORT_CHANGED:\n");
	fprintf(file, "\t\tcase MACH_SEND_INVALID_MEMORY:\n");
	fprintf(file, "\t\tcase MACH_SEND_INVALID_RIGHT:\n");
	fprintf(file, "\t\tcase MACH_SEND_INVALID_TYPE:\n");
	fprintf(file, "\t\tcase MACH_SEND_MSG_TOO_SMALL:\n");
	if (rt->rtWaitTime != argNULL)
	    fprintf(file, "\t\tcase MACH_RCV_TIMED_OUT:\n");
	fprintf(file, "\t\t\tmig_dealloc_reply_port();\n");
	fprintf(file, "\t\t\t/*break;*/\n\t\t}\n");
#endif
    }
    WriteReturnMsgError(file, rt, TRUE, argNULL, "msg_result");
    fprintf(file, "\t}\n");
}

/*************************************************************
 *  Writes the send and receive calls and code to check
 *  for errors. Normally the rpc code is generated instead
 *  although, the subsytem can be compiled with the -R option
 *  which will cause this code to be generated. Called by
 *  WriteRoutine if UseMsgRPC option is false.
 *************************************************************/
static void
WriteMsgSendReceive(file, rt)
    FILE *file;
    routine_t *rt;
{
    char *SendSize = "";
    char string[MAX_STR_LEN];

    if (rt->rtNumRequestVar == 0)
	SendSize = "sizeof(Request)";
    else
	SendSize = "msgh_size";

    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) {
	sprintf(string, "(%s) ? sizeof(mig_reply_error_t) : ", 
	    rt->rtRetCArg->argVarName);
	SendSize = strconcat(string, SendSize);
    }

    /* IsKernelUser to be done! */
    fprintf(file, "\tmsg_result = mach_msg_overwrite(&InP->Head, MACH_SEND_MSG|%s, %s, 0, ",
	    rt->rtMsgOption->argVarName,
	    SendSize);
    fprintf(file, " MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,");
    fprintf(file, " (mach_msg_header_t *) 0, 0);\n");

    fprintf(file, "\tif (msg_result != MACH_MSG_SUCCESS)\n");
    WriteReturnMsgError(file, rt, TRUE, argNULL, "msg_result");
    fprintf(file, "\n");

    fprintf(file, "\tmsg_result = mach_msg_overwrite(&Out0P->Head, MACH_RCV_MSG|%s%s%s, 0, sizeof(Reply), InP->Head.msgh_local_port, %s, MACH_PORT_NULL, ",
	    rt->rtMsgOption->argVarName,
	    rt->rtUserImpl != 0 ? "|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)" : "",
	    rt->rtWaitTime != argNULL ? "|MACH_RCV_TIMEOUT" : "",
	    rt->rtWaitTime != argNULL ? rt->rtWaitTime->argVarName : "MACH_MSG_TIMEOUT_NONE");
    fprintf(file, " (mach_msg_header_t *) 0, 0);\n");
    WriteMsgCheckReceive(file, rt, "MACH_MSG_SUCCESS");
    fprintf(file, "\n");
}

/*************************************************************
 *  Writes the rpc call and the code to check for errors.
 *  This is the default code to be generated. Called by WriteRoutine
 *  for all routine types except SimpleRoutine.
 *************************************************************/
static void
WriteMsgRPC(file, rt)
    FILE *file;
    routine_t *rt;
{
    char *SendSize = "";
    char string[MAX_STR_LEN];

    if (rt->rtNumRequestVar == 0)
	SendSize = "sizeof(Request)";
    else
	SendSize = "msgh_size";

    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) {
	sprintf(string, "(%s) ? sizeof(mig_reply_error_t) : ", 
	    rt->rtRetCArg->argVarName);
	SendSize = strconcat(string, SendSize);
    }

    if (IsKernelUser)
	fprintf(file, "\tmsg_result = mach_msg_rpc_from_kernel(&InP->Head, %s, sizeof(Reply));\n", SendSize);
    else {
	fprintf(file, "\tmsg_result = mach_msg_overwrite(&InP->Head, MACH_SEND_MSG|MACH_RCV_MSG|%s%s%s%s, %s, sizeof(Reply), InP->Head.msgh_reply_port, %s, MACH_PORT_NULL, ",
	    rt->rtMsgOption->argVarName,
	    rt->rtUserImpl != 0 ? "|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)" : "",
	    rt->rtOverwrite ? "|MACH_RCV_OVERWRITE" : "",
	    rt->rtWaitTime != argNULL ? "|MACH_RCV_TIMEOUT" : "",
	    SendSize,
	    rt->rtWaitTime != argNULL? rt->rtWaitTime->argVarName : "MACH_MSG_TIMEOUT_NONE");
	if (rt->rtOverwrite)
      	    fprintf(file, " &InOvTemplate->Head, sizeof(OverwriteTemplate));\n");
	else
	    fprintf(file, " (mach_msg_header_t *) 0, 0);\n");
    }
    WriteMsgCheckReceive(file, rt, "MACH_MSG_SUCCESS");
    fprintf(file, "\n");
}

/*
 * argKPD_Pack discipline for Port types.
 */
static void
WriteKPD_port(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *subindex = "";
    char *recast = "";
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];
    register char *ref = arg->argByReferenceUser ? "*" : "";
    ipc_type_t *real_it;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, TRUE, FALSE, it->itVarArray, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	subindex = "[i]";
	real_it = it->itElement;
    } else {
	(void)sprintf(firststring, "InP->%s", arg->argMsgField);
	(void)sprintf(string, "InP->%s.", arg->argMsgField);
	real_it = it;
    }

    if (IsKernelUser && streql(real_it->itUserType, "ipc_port_t"))
	recast = "(mach_port_t)";
    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\t%s = %s;\n", firststring,
	arg->argTTName);
    /* ref is required also in the Request part, because of inout parameters */
    fprintf(file, "\t%sname = %s%s%s%s;\n", string,
	recast, ref, arg->argVarName, subindex);
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbSendSnd)) {
	register argument_t *poly = arg->argPoly;

	fprintf(file, "\t%sdisposition = %s%s;\n", string, 
	    poly->argByReferenceUser ? "*" : "", poly->argVarName);
    }
    fprintf(file, "#else\t/* UseStaticTemplates */\n");
    fprintf(file, "\t%sname = %s%s%s%s;\n", string,
	recast, ref, arg->argVarName, subindex);
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbSendSnd)) {
        register argument_t *poly = arg->argPoly;

        fprintf(file, "\t%sdisposition = %s%s;\n", string,
            poly->argByReferenceUser ? "*" : "", poly->argVarName);
    } else 
	fprintf(file, "\t%sdisposition = %s;\n", string,
	    it->itInNameStr);
    fprintf(file, "\t%stype = MACH_MSG_PORT_DESCRIPTOR;\n", string);
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
    if (IS_MULTIPLE_KPD(it))  {
	fprintf(file, "\t    }\n");
	if (it->itVarArray) {
	    fprintf(file, "\t    for (i = %s; i < %d; ptr++, i++) {\n", 
		arg->argCount->argVarName, it->itKPD_Number);
	    /* fill the rest of the statically allocated KPD entries with MACH_PORT_NULL */
    	    fprintf(file, "#if\tUseStaticTemplates\n");
    	    fprintf(file, "\t%s = %s;\n", firststring,
		arg->argTTName);
	    fprintf(file, "#else\t/* UseStaticTemplates */\n");
    	    fprintf(file, "\t%sname = MACH_PORT_NULL;\n", string);
    	    fprintf(file, "\t%stype = MACH_MSG_PORT_DESCRIPTOR;\n", string);
    	    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
	    fprintf(file, "\t    }\n");
	}
	fprintf(file, "\t}\n");
    }
    fprintf(file, "\n");
}

static void
WriteKPD_ool_varsize(file, arg, who, where, iscomplex)
    FILE *file;
    register argument_t *arg;
    char *who, *where;
    boolean_t iscomplex;
{
    register ipc_type_t *it = arg->argType;
    register argument_t *count;
    register char *cref;

    if (iscomplex) {
	it = it->itElement;
	count = arg->argSubCount;
    } else 
	count = arg->argCount;
    cref = count->argByReferenceUser ? "*" : "";

    /* size has to be expressed in bytes! */
    if (count->argMultiplier > 1 || it->itSize > 8)
        fprintf(file, "\t%s->%s = %s%s%s * %d;\n", who, where,
	   cref, count->argVarName, 
	   (iscomplex)? "[i]" : "", 
	   count->argMultiplier * it->itSize / 8);	
    else
	fprintf(file, "\t%s->%s = %s%s%s;\n", who, where,
	   cref, count->argVarName,
	   (iscomplex)? "[i]" : "");
}

/*
 * argKPD_Pack discipline for out-of-line types.
 */
static void
WriteKPD_ool(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    register char *ref = arg->argByReferenceUser ? "*" : "";
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];
    boolean_t VarArray;
    u_int howmany, howbig;
    char *subindex;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, TRUE, FALSE, it->itVarArray, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	howmany = it->itElement->itNumber;
	howbig = it->itElement->itSize;
	subindex = "[i]";
    } else {
	(void)sprintf(firststring, "InP->%s", arg->argMsgField);
	(void)sprintf(string, "InP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	howmany = it->itNumber;
	howbig = it->itSize;
	subindex = "";
    }

    fprintf(file, "#if\tUseStaticTemplates\n");

    fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
    fprintf(file, "\t%saddress = (void *)(%s%s%s);\n", 
	string, ref, arg->argVarName, subindex);
    if (VarArray)
	if (IS_MULTIPLE_KPD(it)) 
	    WriteKPD_ool_varsize(file, arg, "\tptr", "size", TRUE);
	else 
	    WriteKPD_ool_varsize(file, arg, "InP", 
		strconcat(arg->argMsgField, ".size"), FALSE);
    if (arg->argDeallocate == d_MAYBE)
	fprintf(file, "\t%sdeallocate =  %s;\n", 
	    string, arg->argDealloc->argVarName); 

    fprintf(file, "#else\t/* UseStaticTemplates */\n");

    fprintf(file, "\t%saddress = (void *)(%s%s%s);\n", 
	string, ref, arg->argVarName, subindex);
    if (VarArray)
	if (IS_MULTIPLE_KPD(it)) 
	    WriteKPD_ool_varsize(file, arg, "\tptr", "size", TRUE);
	else 
	    WriteKPD_ool_varsize(file, arg, "InP", 
		strconcat(arg->argMsgField, ".size"), FALSE);
    else
        fprintf(file, "\t%ssize = %d;\n", string, 
	    (howmany * howbig + 7)/8);
    if (arg->argDeallocate == d_MAYBE)
	fprintf(file, "\t%sdeallocate =  %s;\n", string,
	    arg->argDealloc->argVarName); 
    else
	fprintf(file, "\t%sdeallocate =  %s;\n", string,
	    (arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    fprintf(file, "\t%scopy = %s;\n", string,
	(arg->argFlags & flPhysicalCopy) ? "MACH_MSG_PHYSICAL_COPY" : "MACH_MSG_VIRTUAL_COPY");
#ifdef ALIGNMENT
    fprintf(file, "\t%salignment = MACH_MSG_ALIGN_%d;\n", string,
	(it->itElement->itSize < 8) ? 1 : it->itElement->itSize / 8);
#endif
    fprintf(file, "\t%stype = MACH_MSG_OOL_DESCRIPTOR;\n", string);

    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
    if (IS_MULTIPLE_KPD(it)) {
        fprintf(file, "\t    }\n");
        if (it->itVarArray) {
	    fprintf(file, "\t    for (i = %s; i < %d; ptr++, i++) {\n", 
   	        arg->argCount->argVarName, it->itKPD_Number);
	    /* fill the rest of the statically allocated KPD entries with size NULL */
    	    fprintf(file, "#if\tUseStaticTemplates\n");
	    fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
            if (!VarArray)
    	        fprintf(file, "\t%ssize = 0;\n", string); 
	    /* otherwise the size in the template would be != 0! */
    	    fprintf(file, "#else\t/* UseStaticTemplates */\n");
    	    fprintf(file, "\t%ssize = 0;\n", string);
    	    fprintf(file, "\t%stype = MACH_MSG_OOL_DESCRIPTOR;\n", string);
    	    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
	    fprintf(file, "\t    }\n");
        }
        fprintf(file, "\t}\n");
    }
    fprintf(file, "\n");
}

/*
 * argKPD_Pack discipline for out-of-line Port types.
 */
static void
WriteKPD_oolport(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    register char *ref = arg->argByReferenceUser ? "*" : "";
    register argument_t *count;
    boolean_t VarArray;
    string_t howstr;
    u_int howmany;
    char *subindex;
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];

    if (IS_MULTIPLE_KPD(it)) {
        WriteKPD_Iterator(file, TRUE, FALSE, it->itVarArray, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	howmany = it->itElement->itNumber;
	howstr = it->itElement->itInNameStr;
	count = arg->argSubCount;
	subindex = "[i]";
    } else {
	(void)sprintf(firststring, "InP->%s", arg->argMsgField);
	(void)sprintf(string, "InP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	howmany = it->itNumber;
	howstr = it->itInNameStr;
	count = arg->argCount;
	subindex = "";
    }

    fprintf(file, "#if\tUseStaticTemplates\n");

    fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
    fprintf(file, "\t%saddress = (void *)(%s%s%s);\n", string,
        ref, arg->argVarName, subindex);
    if (VarArray)
        fprintf(file, "\t%scount = %s%s%s;\n", string, 
            count->argByReferenceUser ? "*" : "",
            count->argVarName, subindex);
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbSendSnd)) {
        register argument_t *poly = arg->argPoly;
        register char *pref = poly->argByReferenceUser ? "*" : "";

        fprintf(file, "\t%sdisposition = %s%s;\n", string,
            pref, poly->argVarName);
    }
    if (arg->argDeallocate == d_MAYBE)
        fprintf(file, "\t%sdeallocate =  %s;\n", string,
            arg->argDealloc->argVarName);

    fprintf(file, "#else\t/* UseStaticTemplates */\n");

    fprintf(file, "\t%saddress = (void *)(%s%s%s);\n", 
        string, ref, arg->argVarName, subindex);
    if (VarArray)
        fprintf(file, "\t%scount = %s%s%s;\n", string,
            count->argByReferenceUser ? "*" : "",
            count->argVarName, subindex);
    else
        fprintf(file, "\t%scount = %d;\n", string,
            howmany);
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbSendSnd)) {
        register argument_t *poly = arg->argPoly;
        register char *pref = poly->argByReferenceUser ? "*" : "";

        fprintf(file, "\t%sdisposition = %s%s;\n", 
            string, pref, poly->argVarName);
    } else
        fprintf(file, "\t%sdisposition = %s;\n", 
            string, howstr);
    if (arg->argDeallocate == d_MAYBE)
        fprintf(file, "\t%sdeallocate =  %s;\n", string,
            arg->argDealloc->argVarName);
    else
        fprintf(file, "\t%sdeallocate =  %s;\n", string,
            (arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    fprintf(file, "\t%stype = MACH_MSG_OOL_PORTS_DESCRIPTOR;\n", string);
 
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
    fprintf(file, "\n");

    if (IS_MULTIPLE_KPD(it)) {
        fprintf(file, "\t    }\n");
        if (it->itVarArray) {
            fprintf(file, "\t    for (i = %s; i < %d; ptr++, i++) {\n",
                arg->argCount->argVarName, it->itKPD_Number);
            /* fill the rest of the statically allocated KPD entries with size NULL */
            fprintf(file, "#if\tUseStaticTemplates\n");
            fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
            if (!VarArray)
                fprintf(file, "\t%scount = 0;\n", string);
            /* otherwise the size in the template would be != 0! */
            fprintf(file, "#else\t/* UseStaticTemplates */\n");
            fprintf(file, "\t%scount = 0;\n", string);
            fprintf(file, "\t%stype = MACH_MSG_OOL_PORTS_DESCRIPTOR;\n", string);
            fprintf(file, "#endif\t/* UseStaticTemplates */\n");
            fprintf(file, "\t    }\n");
        }
        fprintf(file, "\t}\n");
    }
    fprintf(file, "\n");
}

static void
WriteOverwriteTemplate(file, rt)
    FILE *file;
    routine_t *rt;
{
    register argument_t *arg;
    char string[MAX_STR_LEN];
    char *subindex = "";
    boolean_t finish = FALSE;

    fprintf(file, "\t/* Initialize the template for overwrite */\n");
    fprintf(file, "\tInOvTemplate->msgh_body.msgh_descriptor_count = %d;\n",
	rt->rtOverwriteKPDs);
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	register ipc_type_t *it = arg->argType;
	char *ref = arg->argByReferenceUser ? "*" : "";
	argument_t *count;
	char *cref;
	boolean_t VarIndex;
	u_int howmany, howbig;

	if (akCheck(arg->argKind, akbOverwrite)) {
	    if (arg->argFlags & flOverwrite) {
		if (IS_MULTIPLE_KPD(it)) {
		    WriteKPD_Iterator(file, FALSE, TRUE, it->itVarArray, arg, TRUE);
		    if (it->itVarArray)
			finish = TRUE;
		    sprintf(string, "\tptr->");
		    subindex = "[i]";
		    count = arg->argSubCount;
		    VarIndex = it->itElement->itVarArray;
		    howmany = it->itElement->itNumber;
		    howbig = it->itElement->itSize;
	 	} else {
		    sprintf(string, "InOvTemplate->%s.", arg->argMsgField);
		    subindex = "";
		    count = arg->argCount;
		    VarIndex = it->itVarArray;
		    howmany = it->itNumber;
		    howbig = it->itSize;
		}

		fprintf(file, "\t%saddress = (void *) %s%s%s;\n", string, 
		    ref, arg->argVarName, subindex);	

		if (it->itPortType) {
		    fprintf(file, "\t%scount = ", string);
		    if (VarIndex) {
			cref = count->argByReferenceUser ? "*" : "";
	 		fprintf(file, "%s%s%s;\n", cref, count->argVarName, subindex);
		    } else
			fprintf(file, "%d;\n", howmany);
	        } else {
		    fprintf(file, "\t%ssize = ", string);
		    if (VarIndex) {
			cref = count->argByReferenceUser ? "*" : "";
			if (count->argMultiplier > 1 || howbig > 8)
	 		    fprintf(file, "%s%s%s * %d;\n", cref, count->argVarName, subindex,
				count->argMultiplier * howbig / 8);
		        else
	 		    fprintf(file, "%s%s%s;\n", cref, count->argVarName, subindex);
		    } else
			fprintf(file, "\t%ssize = %d;\n", string, 
			        (howmany * howbig + 7)/8);
		}
		fprintf(file, "\t%scopy = MACH_MSG_OVERWRITE;\n", string);
		fprintf(file, "\t%stype = MACH_MSG_OOL_%sDESCRIPTOR;\n", string,
		    (it->itPortType) ? "PORTS_" : "");
		if (IS_MULTIPLE_KPD(it))
		    fprintf(file, "\t    }\n");
		if (finish) {
		    fprintf(file, "\t    for (i = %s%s; i < %d; ptr++, i++) {\n", 
			(arg->argCount->argByReferenceUser) ? "*" : "", 
			arg->argCount->argVarName, it->itKPD_Number);
		    fprintf(file, "\t\tptr->copy = MACH_MSG_ALLOCATE;\n");
		    fprintf(file, "\t\tptr->type = MACH_MSG_OOL_%sDESCRIPTOR;\n",
			(it->itPortType) ? "PORTS_" : "");
		    fprintf(file, "\t    }\n");
		}
		if (IS_MULTIPLE_KPD(it))
		    fprintf(file, "\t}\n");
	    } else {
	 	/* just a placeholder */	
		if (IS_MULTIPLE_KPD(it)) {
		    WriteKPD_Iterator(file, FALSE, TRUE, FALSE, arg, TRUE);
		    fprintf(file, "\t\tptr->copy = MACH_MSG_ALLOCATE;\n");
		    fprintf(file, "\t\tptr->type = MACH_MSG_OOL_%sDESCRIPTOR;\n",
			(it->itPortType) ? "PORTS_" : "");
		    fprintf(file, "\t    }\n\t}\n");
	 	} else {
		    fprintf(file, "\tInOvTemplate->%s.copy = MACH_MSG_ALLOCATE;\n",
		        arg->argMsgField);
	            /* not sure whether this is needed */
	            fprintf(file, "\tInOvTemplate->%s.type = MACH_MSG_OOL_%sDESCRIPTOR;\n", 
		        arg->argMsgField, (it->itPortType) ? "PORTS_" : "");
		}
	    }
	}
    }
    fprintf(file, "\n");
}

/*************************************************************
 *  Writes code to copy an argument into the request message.  
 *  Called by WriteRoutine for each argument that is to placed
 *  in the request message.
 *************************************************************/

static void
WritePackArgValueNormal(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    register char *ref = arg->argByReferenceUser ? "*" : "";

    if (IS_VARIABLE_SIZED_UNTYPED(it) || it->itNoOptArray) {
	if (it->itString) {
	    /*
	     *	Copy variable-size C string with mig_strncpy.
	     *	Save the string length (+ 1 for trailing 0)
	     *	in the argument`s count field.
	     */
	    fprintf(file,
		"\tInP->%s = mig_strncpy(InP->%s, %s, %d);\n",
		arg->argCount->argMsgField,
		arg->argMsgField,
		arg->argVarName,
		it->itNumber);
	} else if (it->itNoOptArray)
	    fprintf(file, "\t(void)memcpy((char *) InP->%s, (const char *) %s%s, %d);\n",
		arg->argMsgField, ref, arg->argVarName, it->itTypeSize);
	else {

	    /*
	     *	Copy in variable-size inline array with (void)memcpy,
	     *	after checking that number of elements doesn`t
	     *	exceed declared maximum.
	     */
	    register argument_t *count = arg->argCount;
	    register char *countRef = count->argByReferenceUser ? "*" : "";
	    register ipc_type_t *btype = it->itElement;

	    /* Note btype->itNumber == count->argMultiplier */

	    if (akIdent(arg->argKind) != akeSubCount) {
		/* we skip the SubCount case, as we have already taken care of */
	        fprintf(file, "\tif (%s%s > %d) {\n",
		    countRef, count->argVarName,
		    it->itNumber/btype->itNumber);
	        WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_ARRAY_TOO_LARGE");
	        fprintf(file, "\t}\n");
	    }

	    fprintf(file, "\t(void)memcpy((char *) InP->%s, (const char *) %s%s, ",
		arg->argMsgField, ref, arg->argVarName);
	    if (btype->itTypeSize > 1)
		    fprintf(file, "%d * ", btype->itTypeSize);
	    fprintf(file, "%s%s);\n", countRef, count->argVarName);
	}
    }
    else
	WriteCopyType(file, it, "InP->%s", "/* %s */ %s%s",
		      arg->argMsgField, ref, arg->argVarName);
    fprintf(file, "\n");
}

/*
 * Calculate the size of a variable-length message field.
 */
static void
WriteArgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argType;
    register int bsize = ptype->itElement->itTypeSize;
    register argument_t *count = arg->argCount;

    if (bsize > 1)
	fprintf(file, "%d * ", bsize);
    if (ptype->itString)
	/* get count from descriptor in message */
	fprintf(file, "InP->%s", count->argMsgField);
    else
	/* get count from argument */
	fprintf(file, "%s%s",
		count->argByReferenceUser ? "*" : "",
		count->argVarName);

    /*
     * If the base type size is not a multiple of sizeof(int) [4],
     * we have to round up.
     */
    if (bsize % 4 != 0)
	fprintf(file, " + 3 & ~3");
}

/*
 * Adjust message size and advance request pointer.
 * Called after packing a variable-length argument that
 * has more arguments following.
 */
static void
WriteAdjustMsgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argType;

    /* There are more In arguments.  We need to adjust msgh_size
       and advance InP, so we save the size of the current field
       in msgh_size_delta. */

    fprintf(file, "\tmsgh_size_delta = ");
    WriteArgSize(file, arg);
    fprintf(file, ";\n");

    if (arg->argRequestPos == 0)
	/* First variable-length argument.  The previous msgh_size value
	   is the minimum request size. */

	fprintf(file, "\tmsgh_size = %d + msgh_size_delta;\n",
		arg->argRoutine->rtRequestMinSize);
    else
	fprintf(file, "\tmsgh_size += msgh_size_delta;\n");

    fprintf(file,
	"\tInP = (Request *) ((char *) InP + msgh_size_delta - %d);\n\n",
	ptype->itTypeSize + ptype->itPadSize);
}

/*
 * Calculate the size of the message.  Called after the
 * last argument has been packed.
 */
static void
WriteFinishMsgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    /* No more In arguments.  If this is the only variable In
       argument, the previous msgh_size value is the minimum
       request size. */

    if (arg->argRequestPos == 0) {
	fprintf(file, "\tmsgh_size = %d + (",
			arg->argRoutine->rtRequestMinSize);
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
WriteInitializeCount(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argCInOut->argParent->argType;
    register ipc_type_t *btype = ptype->itElement;

    fprintf(file, "\tif (%s%s < %d)\n",
	    arg->argByReferenceUser ? "*" : "",
	    arg->argVarName,
	    ptype->itNumber/btype->itNumber);
    fprintf(file, "\t\tInP->%s = %s%s;\n",
	    arg->argMsgField,
	    arg->argByReferenceUser ? "*" : "",
	    arg->argVarName);
    fprintf(file, "\telse\n");
    fprintf(file, "\t\tInP->%s = %d;\n",
	    arg->argMsgField, ptype->itNumber/btype->itNumber);
    fprintf(file, "\n");
}

/*
 * Generate code to fill in all of the request arguments and their
 * message types.
 */
static void
WriteRequestArgs(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register argument_t *lastVarArg;

    /*
     * 1. The Kernel Processed Data
     */
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  
	if (akCheckAll(arg->argKind, akbSendSnd|akbSendKPD))
	     (*arg->argKPD_Pack)(file, arg);

    /*
     * 2. The Data Stream
     */
    lastVarArg = argNULL;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	/*
	 * Adjust message size and advance message pointer if
	 * the last request argument was variable-length and the
	 * request position will change.
	 */
	if (lastVarArg != argNULL &&
	    lastVarArg->argRequestPos < arg->argRequestPos)
	{
	    WriteAdjustMsgSize(file, lastVarArg);
	    lastVarArg = argNULL;
	}

	if ((akIdent(arg->argKind) == akeCountInOut) &&
	    akCheck(arg->argKind, akbSendSnd))
		WriteInitializeCount(file, arg);
	else if (akCheckAll(arg->argKind, akbSendSnd|akbSendBody))
	    WritePackArgValueNormal(file, arg);
	/*
	 * Remember whether this was variable-length.
	 */
	if (akCheckAll(arg->argKind, akbSendSnd|akbSendBody|akbVariable))
	    lastVarArg = arg;
    }
    /*
     * Finish the message size.
     */
    if (lastVarArg != argNULL)
	WriteFinishMsgSize(file, lastVarArg);
}

/*************************************************************
 *  Writes code to check that the return msgh_id is correct and that
 *  the size of the return message is correct. Called by
 *  WriteRoutine.
 *************************************************************/
static void
WriteCheckIdentity(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "\tif (Out0P->Head.msgh_id != %d) {\n",
	    rt->rtNumber + SubsystemBase + 100);
    fprintf(file, "\t    if (Out0P->Head.msgh_id == MACH_NOTIFY_SEND_ONCE)\n");
    WriteReturnMsgError(file, rt, TRUE, argNULL, "MIG_SERVER_DIED");
    fprintf(file, "\t    else\n");
    WriteReturnMsgError(file, rt, TRUE, argNULL, "MIG_REPLY_MISMATCH");
    fprintf(file, "\t}\n");
    fprintf(file, "\n");
    if (!rt->rtSimpleReply)
	fprintf(file, "\tmsgh_simple = !(Out0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX);\n");
    fprintf(file, "#if\tTypeCheck\n");

    if (!rt->rtNoReplyArgs)	    
	fprintf(file, "\tmsgh_size = Out0P->Head.msgh_size;\n\n");

    if (rt->rtSimpleReply)
    {
	/* Expecting a simple message.  We can factor out the check for
	   a simple message, since the error reply message is also simple.
	   */
  	fprintf(file,
	    "\tif ((Out0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||\n");
	if (rt->rtNoReplyArgs)
	    fprintf(file, "\t    (Out0P->Head.msgh_size != %d))\n",
			rt->rtReplyMinSize);
	else {
	    /*
	     * We have an error iff:
	     * 1) the message size is not the one expected AND
	     * 2) the message size is also different from sizeof(mig_reply_error_t)
	     *    or the RetCode == KERN_SUCCESS
	     */
	    if (rt->rtNumReplyVar > 0)
	        fprintf(file, "\t    ((msgh_size > %d || msgh_size < %d) &&\n",
		    rt->rtReplyMaxSize, rt->rtReplyMinSize);
	    else
	        fprintf(file, "\t    ((msgh_size != %d) &&\n",
		    rt->rtReplyMinSize);
	    fprintf(file, "\t     (msgh_size != sizeof(mig_reply_error_t) ||\n");
	    fprintf(file, "\t      Out0P->RetCode == KERN_SUCCESS)))\n");
	}
    }
    else {
	/* Expecting a complex message. */

	fprintf(file, "\tif ((msgh_simple || Out0P->msgh_body.msgh_descriptor_count != %d ||\n",
		rt->rtReplyKPDs);
	if (rt->rtNumReplyVar > 0)
	    fprintf(file, "\t    msgh_size < %d || msgh_size > %d) &&\n", 
		    rt->rtReplyMinSize, rt->rtReplyMaxSize);
	else
	    fprintf(file, "\t    msgh_size != %d) &&\n",
		    rt->rtReplyMinSize);
	fprintf(file, "\t    (!msgh_simple || msgh_size != sizeof(mig_reply_error_t) ||\n");
	fprintf(file, "\t    ((mig_reply_error_t *)Out0P)->RetCode == KERN_SUCCESS))\n");	
    }
    WriteReturnMsgError(file, rt, TRUE, argNULL, "MIG_TYPE_ERROR");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");
}

/*************************************************************
 *  Write code to generate error handling code if the RetCode
 *  argument of a Routine is not KERN_SUCCESS.
 *************************************************************/
static void
WriteRetCodeCheck(file, rt)
    FILE *file;
    routine_t *rt;
{
    if (rt->rtSimpleReply) {
	fprintf(file, "\tif (Out0P->RetCode != KERN_SUCCESS)\n");
	WriteReturnMsgError(file, rt, TRUE, argNULL, "Out0P->RetCode");
    } else {
	fprintf(file, "\tif (msgh_simple &&\n");
	fprintf(file, "\t\t((mig_reply_error_t *)Out0P)->RetCode != KERN_SUCCESS)\n");
	WriteReturnMsgError(file, rt, TRUE, argNULL, "((mig_reply_error_t *)Out0P)->RetCode");
    }
    fprintf(file, "\n");
}

/*
 * argKPD_TypeCheck discipline for Port types.
 */
static void
WriteTCheckKPD_port(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *tab = "";
    char string[MAX_STR_LEN];
    boolean_t close = FALSE;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, FALSE, arg, TRUE);
	(void)sprintf(string, "ptr->");
	tab = "\t";
	close = TRUE;
    } else 
	(void)sprintf(string, "Out%dP->%s.", arg->argReplyPos, arg->argMsgField);
    fprintf(file, "\t%sif (%stype != MACH_MSG_PORT_DESCRIPTOR",
	tab, string);
    if (arg->argPoly == argNULL && !it->itVarArray)
	/* we can't check disposition when poly or VarArray,
	   (because some of the entries could be empty) */
	fprintf(file, " ||\n\t%s    %sdisposition != %s",
	    tab, string, it->itOutNameStr);
    fprintf(file, ")\n");
    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_TYPE_ERROR");
    if (close)
	fprintf(file, "\t    }\n\t}\n");
}

/*
 * argKPD_TypeCheck discipline for out-of-line types.
 */
static void
WriteTCheckKPD_ool(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *tab, string[MAX_STR_LEN];
    boolean_t test;
    u_int howmany, howbig;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, FALSE, arg, TRUE);
	tab = "\t";
	sprintf(string, "ptr->");
	howmany = it->itElement->itNumber;
	howbig = it->itElement->itSize;
	test = !it->itVarArray && !it->itElement->itVarArray;
    } else {
	tab = "";
	sprintf(string, "Out%dP->%s.", arg->argReplyPos, arg->argMsgField);
	howmany = it->itNumber;
	howbig = it->itSize;
	test = !it->itVarArray;
    }

    fprintf(file, "\t%sif (%stype != MACH_MSG_OOL_DESCRIPTOR", tab, string);
    if (test)
	/* if VarArray we may use no-op; if itElement->itVarArray size might change */
	fprintf(file, " ||\n\t%s    %ssize != %d", tab, string,
	    (howmany * howbig + 7)/8);
    fprintf(file, ")\n");
    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_TYPE_ERROR");
    if (IS_MULTIPLE_KPD(it))
	fprintf(file, "\t    }\n\t}\n");
}

/*
 * argKPD_TypeCheck discipline for out-of-line Port types.
 */
static void
WriteTCheckKPD_oolport(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *tab, string[MAX_STR_LEN];
    boolean_t test;
    u_int howmany;
    char *howstr;

    if (IS_MULTIPLE_KPD(it)) {
        WriteKPD_Iterator(file, FALSE, FALSE, FALSE, arg, TRUE);
	tab = "\t";
	sprintf(string, "ptr->");
	howmany = it->itElement->itNumber;
	test = !it->itVarArray && !it->itElement->itVarArray;
	howstr = it->itElement->itOutNameStr;
    } else {
	tab = "";
	sprintf(string, "Out%dP->%s.", arg->argReplyPos, arg->argMsgField);
	howmany = it->itNumber;
	test = !it->itVarArray;
	howstr = it->itOutNameStr;
    }

    fprintf(file, "\t%sif (%stype != MACH_MSG_OOL_PORTS_DESCRIPTOR", tab, string);
    if (test)
        /* if VarArray we may use no-op; if itElement->itVarArray size might change */
        fprintf(file, " ||\n\t%s    %scount != %d", tab, string, howmany);
    if (arg->argPoly == argNULL)
        fprintf(file, " ||\n\t%s    %sdisposition != %s", tab, string,
            howstr);
    fprintf(file, ")\n");
    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_TYPE_ERROR");
    if (IS_MULTIPLE_KPD(it))
	fprintf(file, "\t    }\n\t}\n");
}

/*************************************************************
 *  Writes code to check that the type of each of the arguments
 *  in the reply message is what is expected. Called by 
 *  WriteRoutine for each out && typed argument in the reply message.
 *************************************************************/
static void
WriteTypeCheck(file, arg)
    FILE *file;
    register argument_t *arg;
{
    fprintf(file, "#if\tTypeCheck\n");
    (*arg->argKPD_TypeCheck)(file, arg);
    fprintf(file, "#endif\t/* TypeCheck */\n");
}

static void
WriteCheckArgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argType;
    register ipc_type_t *btype = ptype->itElement;
    argument_t *count = arg->argCount;
    int multiplier = btype->itTypeSize;

    if (multiplier > 1)
	fprintf(file, "%d * ", multiplier);

    fprintf(file, "Out%dP->%s", count->argReplyPos, count->argMsgField);

    /* If the base type size of the data field isn`t a multiple of 4,
       we have to round up. */
    if (btype->itTypeSize % 4 != 0)
	fprintf(file, " + 3 & ~3");
}

static void
WriteCheckMsgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register routine_t *rt = arg->argRoutine;

    /* If there aren't any more Out args after this, then
       we can use the msgh_size_delta value directly in
       the TypeCheck conditional. */

    if (arg->argReplyPos == rt->rtMaxReplyPos)
    {
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tif (msgh_size != %d + (",
		rt->rtReplyMinSize);
	WriteCheckArgSize(file, arg);
	fprintf(file, "))\n");

	WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_TYPE_ERROR");
	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    else
    {
	/* If there aren't any more variable-sized arguments after this,
	   then we must check for exact msg-size and we don't need
	   to update msgh_size. */

	boolean_t LastVarArg = arg->argReplyPos+1 == rt->rtNumReplyVar;

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
		rt->rtReplyMinSize);
	else
	    fprintf(file, "\tif (msgh_size < %d + msgh_size_delta)\n",
		rt->rtReplyMinSize);
	WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_TYPE_ERROR");

	if (!LastVarArg)
	    fprintf(file, "\tmsgh_size -= msgh_size_delta;\n");

	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    fprintf(file, "\n");
}

void
WriteAdjustReplyMsgPtr(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argType;

    fprintf(file,
	"\tOut%dP = (Reply *) ((char *) Out%dP + msgh_size_delta - %d);\n\n",
	arg->argReplyPos +1, arg->argReplyPos, ptype->itTypeSize + ptype->itPadSize);
}

/*
 * argKPD_Extract discipline for Port types.
 */
static void
WriteExtractKPD_port(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t	*it = arg->argType;
    register char *ref = arg->argByReferenceUser ? "*" : "";
    register char *subindex;
    register char *recast = "";
    ipc_type_t *real_it;

    real_it = (IS_MULTIPLE_KPD(it)) ? it->itElement : it;
    if (IsKernelUser && streql(real_it->itUserType, "ipc_port_t"))
	recast = "(ipc_port_t)";
    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, it->itVarArray, arg, FALSE);

	fprintf(file, "\t\t%s[i] = %sptr->name;\n", 
	    arg->argVarName, recast);
	if (it->itVarArray) {
	    register argument_t *count = arg->argCount;
	    register char *cref = count->argByReferenceUser ? "*" : "";

	    fprintf(file, "\t    if (Out%dP->%s > %s%s)\n",  count->argReplyPos, 
		count->argVarName, cref, count->argVarName);
	    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_ARRAY_TOO_LARGE");
	}
	fprintf(file, "\t}\n");
	subindex = "[0]";
    } else {
	fprintf(file, "\t%s%s = %sOut%dP->%s.name;\n",
	    ref, arg->argVarName, recast, arg->argReplyPos, arg->argMsgField);
	subindex = "";
    }

    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbReturnRcv)) {
	register argument_t *poly = arg->argPoly;
	register char *pref = poly->argByReferenceUser ? "*" : "";

	fprintf(file, "\t%s%s = Out%dP->%s%s.disposition;\n",
	    pref, poly->argVarName, arg->argReplyPos, arg->argMsgField, subindex);
    }
}

/*
 * argKPD_Extract discipline for out-of-line types.
 */
static void
WriteExtractKPD_ool(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register char *ref = arg->argByReferenceUser ? "*" : "";
    register ipc_type_t	*it = arg->argType;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, it->itVarArray, arg, FALSE);
	fprintf(file, "\t\t%s[i] = ptr->address;\n", 
	    arg->argVarName);
	fprintf(file, "\t}\n");
    } else
	fprintf(file, "\t%s%s = (%s)(Out%dP->%s.address);\n",
	    ref, arg->argVarName, arg->argType->itUserType, 
	    arg->argReplyPos, arg->argMsgField);
    /*
     *  In case of variable sized arrays,
     *  the count field will be retrieved from the untyped
     *  section of the message
     */
}

/*
 * argKPD_Extract discipline for out-of-line Port types.
 */
static void
WriteExtractKPD_oolport(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register char *ref = arg->argByReferenceUser ? "*" : "";
    register ipc_type_t *it = arg->argType;
    char *subindex;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, it->itVarArray, arg, FALSE);
	fprintf(file, "\t\t%s[i] = ptr->address;\n",
	    arg->argVarName);
	fprintf(file, "\t}\n");
	subindex = "[0]";
    } else {
	fprintf(file, "\t%s%s = (%s)(Out%dP->%s.address);\n",
	    ref, arg->argVarName, arg->argType->itUserType, arg->argReplyPos, arg->argMsgField);
	subindex = "";
    }
    /*
     *  In case of variable sized arrays,
     *  the count field will be retrieved from the untyped
     *  section of the message
     */
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbReturnRcv)) {
        register argument_t *poly = arg->argPoly;
        register char *pref = poly->argByReferenceUser ? "*" : "";

        fprintf(file, "\t%s%s = Out%dP->%s%s.disposition;\n",
            pref, poly->argVarName, arg->argReplyPos, arg->argMsgField, subindex);
    }
}

/*************************************************************
 *  Write code to copy an argument from the reply message
 *  to the parameter. Called by WriteRoutine for each argument
 *  in the reply message.
 *************************************************************/

static void
WriteExtractArgValueNormal(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t	*argType = arg->argType;
    register char *ref = arg->argByReferenceUser ? "*" : "";
    char who[20];

    if (akCheck(arg->argKind, akbUserImplicit)) 
	sprintf(who, "TrailerP");
    else
	sprintf(who, "Out%dP", arg->argReplyPos);
   
    if (IS_VARIABLE_SIZED_UNTYPED(argType) || argType->itNoOptArray) {
	if (argType->itString) {
	    /*
	     *	Copy out variable-size C string with mig_strncpy.
	     */
	    fprintf(file, "\t(void) mig_strncpy(%s%s, %s->%s, %d);\n",
		ref,
		arg->argVarName,
		who,
		arg->argMsgField,
		argType->itNumber);
	}
	else if (argType->itNoOptArray)
	    fprintf(file, "\t(void)memcpy((char *) %s%s, (const char *) %s->%s, %d);\n",
	  	ref, arg->argVarName, who, arg->argMsgField, argType->itTypeSize);
	else {

	    /*
	     *	Copy out variable-size inline array with (void)memcpy,
	     *	after checking that number of elements doesn`t
	     *	exceed user`s maximum.
	     */
	    register argument_t *count = arg->argCount;
	    register char *countRef = count->argByReferenceUser ? "*" : "";
	    register ipc_type_t *btype = argType->itElement;

	    /* Note count->argMultiplier == btype->itNumber */
	    /* Note II: trailer logic isn't supported in this case */

	    fprintf(file, "\tif (Out%dP->%s", count->argReplyPos, count->argMsgField);
	    fprintf(file, " > %s%s) {\n",
		countRef, count->argVarName);

	    /*
	     * If number of elements is too many for user receiving area,
	     * fill user`s area as much as possible.  Return the correct
	     * number of elements.
	     */
	    fprintf(file, "\t\t(void)memcpy((char *) %s%s, (const char *) Out%dP->%s, ",
		ref, arg->argVarName, arg->argReplyPos, arg->argMsgField);
	    if (btype->itTypeSize > 1)
		fprintf(file, "%d * ", btype->itTypeSize);
	    fprintf(file, "%s%s);\n",
		countRef, count->argVarName);

	    fprintf(file, "\t\t%s%s = Out%dP->%s;\n",
		     countRef, count->argVarName, count->argReplyPos, count->argMsgField);
	    fprintf(file, "");
	    WriteReturnMsgError(file, arg->argRoutine, TRUE, arg, "MIG_ARRAY_TOO_LARGE");

	    fprintf(file, "\t}\n");

	    fprintf(file, "\t(void)memcpy((char *) %s%s, (const char *) Out%dP->%s, ",
		ref, arg->argVarName, arg->argReplyPos, arg->argMsgField);
	    if (btype->itTypeSize > 1)
		fprintf(file, "%d * ",
			btype->itTypeSize);
	    fprintf(file, "Out%dP->%s);\n",
		count->argReplyPos, count->argMsgField);
	}
    }
    else
	WriteCopyType(file, argType,
		      "%s%s", "/* %s%s */ %s->%s",
		      ref, arg->argVarName, who, arg->argMsgField);
    fprintf(file, "\n");
}

static void
WriteReplyArgs(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register argument_t *lastVarArg;

    lastVarArg = argNULL;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {
	/*
	 * Advance message pointer if the last reply argument was
	 * variable-length and the reply position will change.
	 */
	if (lastVarArg != argNULL && 
	    lastVarArg->argReplyPos < arg->argReplyPos) {
		WriteAdjustReplyMsgPtr(file, lastVarArg);
		lastVarArg = argNULL;
	}

	if (akCheckAll(arg->argKind, akbReturnRcv|akbReturnBody)) {
	    if (akCheck(arg->argKind, akbVariable)) {
	        WriteCheckMsgSize(file, arg);
	        lastVarArg = arg;
	    }
	    WriteExtractArgValueNormal(file, arg);
	    continue;
	}
	if (akCheckAll(arg->argKind, akbReturnRcv|akbReturnKPD)) {
	    /*
	     * KPDs have argReplyPos 0, therefore they escape the above logic
	     */
	    if ((arg->argCount != argNULL || arg->argSubCount != argNULL)
		&& lastVarArg) {
		    WriteAdjustReplyMsgPtr(file, lastVarArg);
		    lastVarArg = argNULL;
	    }
	    (*arg->argKPD_Extract)(file, arg);
	    continue; 
	} 
	if (akCheck(arg->argKind, akbUserImplicit)) { 
	    WriteCheckTrailerSize(file, TRUE, arg);
	    WriteExtractArgValueNormal(file, arg);
        }
    }
}

/*************************************************************
 *  Writes code to return the return value. Called by WriteRoutine
 *  for routines and functions.
 *************************************************************/
static void
WriteReturnValue(file, rt)
    FILE *file;
    routine_t *rt;
{
    /* If returning RetCode, we have already checked that it is KERN_SUCCESS */
    WriteReturn(file, rt, "\t", "KERN_SUCCESS", "\n");
}

/*************************************************************
 *  Writes the elements of the message type declaration: the
 *  msg_type structure, the argument itself and any padding 
 *  that is required to make the argument a multiple of 4 bytes.
 *  Called by WriteRoutine for all the arguments in the request
 *  message first and then the reply message.
 *************************************************************/
static void
WriteFieldDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    if (akCheck(arg->argKind, akbSendKPD) ||
	akCheck(arg->argKind, akbReturnKPD))
	    WriteFieldDeclPrim(file, arg, FetchUserKPDType);
    else
	WriteFieldDeclPrim(file, arg, FetchUserType);
}

/* Fill in the string with an expression that refers to the size
 * of the specified array:
 */
static void
GetArraySize(arg, size)
    register argument_t *arg;
    char *size;
{
    register ipc_type_t *it = arg->argType;

    if (it->itVarArray) {
	if (arg->argCount->argByReferenceUser) {
	    sprintf(size, "*%s", arg->argCount->argVarName);
	} else
	    sprintf(size, "%s", arg->argCount->argVarName);
    } else {
	sprintf(size, "%d", (it->itNumber * it->itSize + 7) / 8);
    }
}


static void
WriteRPCPortDisposition(file, arg)
    FILE *file;
    register argument_t *arg;
{

    /*
     * According to the MIG specification, the port disposition could be different
     * on input and output. If we stay with this then a new bitfield will have
     * to be added. Right now the port disposition is the same for in and out cases.
     */
    switch(arg->argType->itInName)
    {

	case  MACH_MSG_TYPE_MOVE_RECEIVE:
	fprintf(file, " | MACH_RPC_MOVE_RECEIVE");
	break;

	case  MACH_MSG_TYPE_MOVE_SEND:
	fprintf(file, " | MACH_RPC_MOVE_SEND");
	break;

	case  MACH_MSG_TYPE_MOVE_SEND_ONCE:
	fprintf(file, " | MACH_RPC_MOVE_SEND_ONCE");
	break;

	case  MACH_MSG_TYPE_COPY_SEND:
	fprintf(file, " | MACH_RPC_COPY_SEND");
	break;

	case  MACH_MSG_TYPE_MAKE_SEND:
	fprintf(file, " | MACH_RPC_MAKE_SEND");
	break;

	case  MACH_MSG_TYPE_MAKE_SEND_ONCE:
	fprintf(file, " | MACH_RPC_MAKE_SEND_ONCE");
	break;
    }
}

static void
WriteRPCArgDescriptor(file, arg, offset)
    FILE *file;
    register argument_t *arg;
    int offset;
{
    fprintf(file, "            {\n                0 ");
    if (RPCPort(arg))
    {
	fprintf(file, "| MACH_RPC_PORT ");
	if (arg->argType->itNumber > 1)
	    fprintf(file, "| MACH_RPC_ARRAY ");
	if (arg->argType->itVarArray)
	    fprintf(file, "| MACH_RPC_VARIABLE ");
	WriteRPCPortDisposition(file, arg);
    }
    else if (RPCPortArray(arg))
    {
	fprintf(file, "| MACH_RPC_PORT_ARRAY ");
	if (arg->argType->itVarArray)
	    fprintf(file, "| MACH_RPC_VARIABLE ");
	WriteRPCPortDisposition(file, arg);
    }
    else if (RPCFixedArray(arg))
	fprintf(file, "| MACH_RPC_ARRAY_FIXED ");
    else if (RPCVariableArray(arg))
	fprintf(file, "| MACH_RPC_ARRAY_VARIABLE ");
    if (argIsIn(arg))
	fprintf(file, " | MACH_RPC_IN ");
    if (argIsOut(arg))
	fprintf(file, " | MACH_RPC_OUT ");
    if ((! arg->argType->itInLine) && (! arg->argType->itMigInLine))
	fprintf(file, " | MACH_RPC_POINTER ");
    if (arg->argFlags & flDealloc)
	fprintf(file, " | MACH_RPC_DEALLOCATE ");
    if (arg->argFlags & flPhysicalCopy)
	fprintf(file, " | MACH_RPC_PHYSICAL_COPY ");
    fprintf(file, ",\n");
    fprintf(file, "                %d,\n", (arg->argType->itSize / 8));
    fprintf(file, "                %d,\n", arg->argType->itNumber);
    fprintf(file, "                %d,\n            },\n", offset);
}

void
WriteRPCRoutineDescriptor(file, rt, arg_count, descr_count,
			  work_routine, stub_routine, sig_array)
    FILE *file;
    register routine_t *rt;
    int arg_count, descr_count;
    string_t work_routine, stub_routine, sig_array;
{
    register argument_t *arg;

    fprintf(file, "          { (mig_impl_routine_t) %s,\n\
            (mig_stub_routine_t) %s, ",
	    work_routine, stub_routine);
    fprintf(file, "%d, %d, %s }", arg_count, descr_count, sig_array);
}

void
WriteRPCRoutineArgDescriptor(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    int offset = 0;
    int size = 0;

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
    {
	boolean_t compound = arg->argType->itStruct && arg->argType->itInLine;

	if (RPCPort(arg) || RPCPortArray(arg) ||
	    RPCFixedArray(arg) || RPCVariableArray(arg))
	{
	    WriteRPCArgDescriptor(file, arg, offset);
	    size = 4;
	}
	if (! size)
	{
	    if (compound)
		size = arg->argType->itNumber * (arg->argType->itSize / 8);
	    else
		size = (arg->argType->itSize / 8);
	}
	if (akCheck(arg->argKind, akbServerArg))
	    offset += size;
	size = 0;
    }
}


static void
WriteRPCSignature(file, rt)
    FILE *file;
    register routine_t *rt;
{
    int arg_count = 0;
    int descr_count = 0;

    fprintf(file, "    kern_return_t rtn;\n");
    descr_count = rtCountArgDescriptors(rt->rtArgs, &arg_count);
    fprintf(file, "    static struct\n    {\n");
    fprintf(file, "        struct routine_descriptor rd;\n");
    fprintf(file, "        struct routine_arg_descriptor rad[%d];\n", descr_count);
    fprintf(file, "    } sig =\n    {\n");
    WriteRPCRoutineDescriptor(file, rt, arg_count, descr_count,
			      "0", "0", "sig.rad");
    fprintf(file, ",\n");
    fprintf(file, "        {\n");
    WriteRPCRoutineArgDescriptor(file, rt);
    fprintf(file, "\n        }\n");
    fprintf(file, "\n    };\n\n");
}

static void
WriteRPCCall(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register int i;

    i = 0;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
    {
	if (akIdent(arg->argKind) == akeRequestPort)
	{
	    fprintf(file, "    rtn = (MACH_RPC(&sig, sizeof(sig), %d, %s,\n",
		    rt->rtNumber + SubsystemBase, arg->argVarName);
	    fprintf(file, "                   (%s", arg->argVarName);
	}
	else if (akCheck(arg->argKind, akbServerArg))
	{
	    if (i && (i++ % 6 == 0))
		fprintf(file, ",\n                    ");
	    else
		fprintf(file, ", ");
	    fprintf(file, "%s", arg->argVarName);
	}
    }
    fprintf(file, ")));\n");
    fprintf(file, "\n");
    fprintf(file, "    if (rtn != KERN_NO_ACCESS) return rtn;\n\n");
    fprintf(file, "/* The following message rpc code is generated for the network case */\n\n");
}

static int
CheckRPCCall(rt)
    register routine_t *rt;
{
    register argument_t *arg;
    register int i;

    i = 0;
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
    {
	if (akCheck(arg->argKind, akbUserArg) &&
	    ((arg->argType->itOutName == -1) || (arg->argType->itInName == -1)))
	{
	    return FALSE;
	}
	if (arg->argFlags & flMaybeDealloc)
	{
	    return FALSE;
	}
    }
    return TRUE;
}

static void
WriteRPCRoutine(file, rt)
    FILE *file;
    register routine_t *rt;
{
    if (CheckRPCCall(rt))
    {
	WriteRPCSignature(file, rt);
	WriteRPCCall(file, rt);
    }
}

/********************** End UserRPCTrap Routines*************************/

/* Process an IN/INOUT arg before the short-circuited RPC */
static void
WriteShortCircInArgBefore(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char size[128];

    fprintf(file, "\n\t/* IN %s: */\n", arg->argVarName);

    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD)) {
	switch (arg->argKPD_Type) {
	case MACH_MSG_PORT_DESCRIPTOR:
	    break;
	case MACH_MSG_OOL_DESCRIPTOR:
	    /* Arg is an out-of-line array: */
	    if (!(arg->argFlags & flDealloc) &&
		    (!(arg->argFlags & flAuto) || !(arg->argFlags & flConst))) {
		/* Need to map a copy of the array: */
		GetArraySize(arg, size);
		fprintf(file, "\t(void)vm_read(mach_task_self(),\n");
		fprintf(file, "\t\t      (vm_address_t) %s%s, %s, (vm_address_t *) &_%sTemp_, &_MIG_Ignore_Count_);\n",
					(arg->argByReferenceUser ? "*" : ""),
					arg->argVarName, size, arg->argVarName);
		/* Point argument at the copy: */
		fprintf(file, "\t*(char **)&%s%s = _%sTemp_;\n",
					(arg->argByReferenceUser ? "*" : ""),
					arg->argVarName,
					arg->argVarName);
	    } else if ((arg->argFlags & flDealloc) &&
			    ((arg->argFlags & flAuto) || it->itMigInLine)) {
		/* Point the temp var at the original argument: */
		fprintf(file, "\t_%sTemp_ = (char *) %s%s;\n",
					arg->argVarName,
					(arg->argByReferenceUser ? "*" : ""),
					arg->argVarName);
	    }
	    break;
	case MACH_MSG_OOL_PORTS_DESCRIPTOR:
	    break;
	default:
	    printf("MiG internal error: type of kernel processed data unknown\n");
	    exit(1);
	}   /* end of switch */
    } else if (it->itNumber > 1) {
	if (it->itStruct) {
	    /* Arg is a struct -- nothing to do. */
	} else {
	    /* Arg is a C string or an in-line array: */
	    if (!argIsOut(arg) && !(arg->argFlags & flConst)) {
		/* Have to copy it into a temp.  Use a stack var, if this would
		 * not overflow the -maxonstack specification:
		 */
		if (it->itTypeSize <= sizeof(char *) ||
			arg->argRoutine->rtMessOnStack ||
			arg->argRoutine->rtTempBytesOnStack +
				     it->itTypeSize <= MaxMessSizeOnStack) {
		    fprintf(file, "\t{   char _%sTemp_[%d];\n",
			    arg->argVarName, it->itTypeSize);
		    arg->argRoutine->rtTempBytesOnStack += it->itTypeSize;
		    arg->argTempOnStack = TRUE;
		} else {
		    fprintf(file, "\t{   _%sTemp_ = (char *) %s(%d);\n",
			    arg->argVarName, MessAllocRoutine, it->itTypeSize);
		    arg->argTempOnStack = FALSE;
		}
		WriteCopyArg(file, arg, "_%sTemp_", "/* %s */ (char *) %s",
				  arg->argVarName, arg->argVarName);
		/* Point argument at temp: */
		fprintf(file, "\t    *(char **)&%s%s = _%sTemp_;\n",
					   (arg->argByReferenceUser ? "*" : ""),
					   arg->argVarName,
					   arg->argVarName);
		fprintf(file, "\t}\n");
	    }
	}
    }
}


/* Process an INOUT/OUT arg before the short-circuited RPC */
static void
WriteShortCircOutArgBefore(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    fprintf(file, "\n\t/* OUT %s: */\n", arg->argVarName);

    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD)) {
	switch (arg->argKPD_Type) {
	case MACH_MSG_PORT_DESCRIPTOR:
	    break;
	case MACH_MSG_OOL_DESCRIPTOR:
	    /* Arg is an out-of-line array: */
	    if (!argIsIn(arg) && (arg->argFlags & flOverwrite)) {
		/* Point the temp var at the original argument: */
		fprintf(file, "\t    _%sTemp_ = (char *) %s%s;\n",
					arg->argVarName,
					(arg->argByReferenceUser ? "*" : ""),
					arg->argVarName);
	    }
	    break;
	case MACH_MSG_OOL_PORTS_DESCRIPTOR:
	    break;
	default:
	    printf("MiG internal error: type of kernel processed data unknown\n");
	    exit(1);
	}   /* end of switch */
    } else if (it->itNumber > 1) {
	/* Arg is an in-line array: */
    }
}
 


/* Process an IN arg after the short-circuited RPC */
static void
WriteShortCircInArgAfter(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char size[128];

    fprintf(file, "\n\t/* IN %s: */\n", arg->argVarName);

    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD)) {
	switch (arg->argKPD_Type) {
	case MACH_MSG_PORT_DESCRIPTOR:
	    break;
	case MACH_MSG_OOL_DESCRIPTOR:
	    /* Arg is an out-of-line array: */
	    GetArraySize(arg, size);
	    if ((!(arg->argFlags & flAuto) && it->itMigInLine) ||
		((arg->argFlags & flAuto) &&
				((arg->argFlags & flDealloc) ||
				 !(arg->argFlags & flConst))
		)) {
		/* Need to dealloc the temporary: */
		fprintf(file, "\t(void)vm_deallocate(mach_task_self(),");
		fprintf(file, " (vm_address_t *) _%sTemp_, %s);\n",
					    arg->argVarName, size);
	    }
	    break;
	case MACH_MSG_OOL_PORTS_DESCRIPTOR:
	    break;
	default:
	    printf("MiG internal error: type of kernel processed data unknown\n");
	    exit(1);
	}   /* end of switch */
    } else if (it->itNumber > 1) {
	if (it->itStruct) {
	    /* Arg is a struct -- nothing to do. */
	} else {
	    /* Arg is a C string or an in-line array: */
	    if (!argIsOut(arg) && !(arg->argFlags & flConst)) {
		/* A temp needs to be deallocated, if not on stack: */
		if (!arg->argTempOnStack) {
		    fprintf(file, "\t%s(_%sTemp_, %d);\n",
			    MessFreeRoutine, arg->argVarName, it->itTypeSize);
		}
	    }
	}
    }
}

static void
WriteShortCircOutArgAfter(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char size[128];

    fprintf(file, "\n\t/* OUT %s: */\n", arg->argVarName);

    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD)) {
	switch (arg->argKPD_Type) {
	case MACH_MSG_PORT_DESCRIPTOR:
	    break;
	case MACH_MSG_OOL_DESCRIPTOR:
	    /* Arg is an out-of-line array: */

	    /* Calculate size of array: */
	    GetArraySize(arg, size);
	    if (!(arg->argFlags & flDealloc) || (arg->argFlags & flOverwrite)) {
		/* Copy argument to vm_allocated Temp: */
		fprintf(file, "\t(void)vm_read(mach_task_self(),\n");
		fprintf(file, "\t\t      (vm_address_t) %s%s, %s, (vm_address_t *) &_%sTemp_, &_MIG_Ignore_Count_);\n",
				    (arg->argByReferenceUser ? "*" : ""),
				    arg->argVarName, size, arg->argVarName);
		if (!argIsIn(arg) && (arg->argFlags & flDealloc) &&
					(arg->argFlags & flOverwrite)) {
		    /* Deallocate argument returned by server */
		    fprintf(file, "\t(void)vm_deallocate(mach_task_self(),");
		    fprintf(file, " (vm_address_t *) %s%s, %s);\n",
				    (arg->argByReferenceUser ? "*" : ""),
				    arg->argVarName, size);
		}
		/* Point argument at new temporary: */
		fprintf(file, "\t*(char **)&%s%s = _%sTemp_;\n",
					(arg->argByReferenceUser ? "*" : ""),
					arg->argVarName,
					arg->argVarName);
	    }
	    break;
	case MACH_MSG_OOL_PORTS_DESCRIPTOR:
	    break;
	default:
	    printf("MiG internal error: type of kernel processed data unknown\n");
	    exit(1);
	}   /* end of switch */
    } else if (it->itNumber != 1) {
	/* Arg is an in-line array: */
    }
}


static void
WriteShortCircRPC(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register int server_argc, i;
    boolean_t ShortCircOkay = TRUE;
    boolean_t first_OOL_arg = TRUE;

    fprintf(file, "    if (0 /* Should be: !(%s & 0x3) XXX */) {\n",
						rt->rtRequestPort->argVarName);

    if (rt->rtOneWay) {
	/* Do not short-circuit simple routines: */
	ShortCircOkay = FALSE;
    } else {
	/* Scan for any types we can't yet handle.  If found, give up on short-
	 * circuiting and fall back to mach_msg:
	 */
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	    if (arg->argFlags & flMaybeDealloc) {
		ShortCircOkay = FALSE;
		break;
	    }
	    /* Can't yet handle ports: */
	    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD) &&
		    (arg->argKPD_Type == MACH_MSG_PORT_DESCRIPTOR ||
		     arg->argKPD_Type == MACH_MSG_OOL_PORTS_DESCRIPTOR)) {
		ShortCircOkay = FALSE;
		break;
	    }
	}
    }

    if (ShortCircOkay) {

	fprintf(file,
	  "      rpc_subsystem_t subsystem = ((rpc_port_t)%s)->rp_subsystem;\n",
					rt->rtRequestPort->argVarName);
	fprintf(file, "\n");
	fprintf(file, "      if (subsystem && subsystem->start == %d) {\n",
		       SubsystemBase);
	fprintf(file, "\tkern_return_t rtn;\n");
	fprintf(file, "\n");

	/* Declare temp vars for out-of-line array args, and for all array
	 * args, if -maxonstack has forced us to allocate in-line arrays
	 * off the stack:
	 */
	rt->rtTempBytesOnStack = 0;
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	    arg->argTempOnStack = FALSE;
	    if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD) &&
			    arg->argKPD_Type == MACH_MSG_OOL_DESCRIPTOR) {
		if (first_OOL_arg) {
			/* Need a garbage temporary to hold the datacount
			 * returned by vm_read, which we always ignore:
			 */
			fprintf(file,
			    "\tmach_msg_type_number_t _MIG_Ignore_Count_;\n");
			first_OOL_arg = FALSE;
		}
	    } else if (!rt->rtMessOnStack &&
		    arg->argType->itNumber > 1 && !arg->argType->itStruct) {
	    } else
		continue;
	    fprintf(file, "\tchar *_%sTemp_;\n", arg->argVarName);
	    rt->rtTempBytesOnStack += sizeof(char *);
	}

	/* Process the IN arguments, in order: */

	fprintf(file, "\t/* Pre-Process the IN arguments: */\n");
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {
	    if (argIsIn(arg))
		WriteShortCircInArgBefore(file, arg);
	    if (argIsOut(arg))
		WriteShortCircOutArgBefore(file, arg);
	}
	fprintf(file, "\n");

	/* Count the number of server args: */
	server_argc = 0;
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
	    if (akCheck(arg->argKind, akbServerArg))
		    server_argc++;

	/* Call RPC_SIMPLE to switch to server stack and function: */
	i = 0;
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	    if (akIdent(arg->argKind) == akeRequestPort) {
		    fprintf(file, "\trtn = RPC_SIMPLE(%s, %d, %d, (",
			    arg->argVarName, rt->rtNumber + SubsystemBase,
			    server_argc);
		    fprintf(file, "%s", arg->argVarName);
	    } else if (akCheck(arg->argKind, akbServerArg)) {
		    if (i++ % 6 == 0)
			fprintf(file, ",\n\t\t");
		    else
			fprintf(file, ", ");
		    fprintf(file, "%s", arg->argVarName);
	    }
	}
	fprintf(file, "));\n");
	fprintf(file, "\n");

	/* Process the IN and OUT arguments, in order: */
	fprintf(file, "\t/* Post-Process the IN and OUT arguments: */\n");
	for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  {
	    if (argIsIn(arg))
		WriteShortCircInArgAfter(file, arg);
	    if (argIsOut(arg))
		WriteShortCircOutArgAfter(file, arg);
	}
	fprintf(file, "\n");

	fprintf(file, "\treturn rtn;\n");
	fprintf(file, "      }\n");
    }

    /* In latest design, the following is not necessary, because in
     * kernel-loaded tasks, the Mach port name is the same as the handle
     * used by the RPC mechanism, namely a pointer to the ipc_port, and
     * in user-mode tasks, the Mach port name gets renamed to be a pointer
     * to the user-mode rpc_port_t struct.
     */
#if	0
	if (IsKernelUser)
	    fprintf(file, "      %s = (ipc_port_t)%s->rp_receiver_name;\n",
					    rt->rtRequestPort->argVarName,
					    rt->rtRequestPort->argVarName);
	else
	    fprintf(file, "      %s = ((rpc_port_t)%s)->rp_receiver_name;\n",
					    rt->rtRequestPort->argVarName,
					    rt->rtRequestPort->argVarName);
#endif

    fprintf(file, "    }\n");
}

static void
WriteStubDecl(file, rt)
    FILE *file;
    register routine_t *rt;
{
    fprintf(file, "\n");
    fprintf(file, "/* %s %s */\n", rtRoutineKindToStr(rt->rtKind), rt->rtName);
    fprintf(file, "mig_external %s %s\n", ReturnTypeStr(rt), rt->rtUserName);
    if (BeAnsiC) {
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ",\n", "\n");
        fprintf(file, ")\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "(\n");
        WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ",\n", "\n");
        fprintf(file, ")\n");
        fprintf(file, "#else\n");
        fprintf(file, "\t(");
        WriteList(file, rt->rtArgs, WriteNameDecl, akbUserArg, ", ", "");
        fprintf(file, ")\n");
        WriteList(file, rt->rtArgs, WriteUserVarDecl, akbUserArg, ";\n", ";\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }
    fprintf(file, "{\n");
}

static void
InitKPD_Disciplines(args)
    argument_t *args;
{
    argument_t *arg;
    extern void KPD_noop();
    extern void KPD_error();
    extern void WriteTemplateKPD_port();
    extern void WriteTemplateKPD_ool();
    extern void WriteTemplateKPD_oolport();

    /*
     * WriteKPD_port,  WriteExtractKPD_port, 
     * WriteKPD_ool,  WriteExtractKPD_ool, 
     * WriteKPD_oolport,  WriteExtractKPD_oolport
     * are local to this module (which is the reason why this initialization
     * takes place here rather than in utils.c). 
     * Common routines for user and server will be established SOON, and
     * all of them (including the initialization) will be transfert to 
     * utils.c
     * All the KPD disciplines are defaulted to be KPD_error().
     * Note that akbSendKPD and akbReturnKPd are not exclusive,
     * because of inout type of parameters.
     */ 
    for (arg = args; arg != argNULL; arg = arg->argNext)
        if (akCheck(arg->argKind, akbSendKPD|akbReturnKPD)) 
	    switch (arg->argKPD_Type) {
	    case MACH_MSG_PORT_DESCRIPTOR:
		arg->argKPD_Init = KPD_noop;
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_port;
		    arg->argKPD_Pack = WriteKPD_port;
		}
		if akCheck(arg->argKind, akbReturnKPD) {
		    arg->argKPD_Extract = WriteExtractKPD_port;
		    arg->argKPD_TypeCheck = WriteTCheckKPD_port;
		} 
		break;
	    case MACH_MSG_OOL_DESCRIPTOR:
		arg->argKPD_Init = KPD_noop;
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_ool;
		    arg->argKPD_Pack = WriteKPD_ool;
		}
		if akCheck(arg->argKind, akbReturnKPD) {
	 	    arg->argKPD_TypeCheck = WriteTCheckKPD_ool;
		    arg->argKPD_Extract = WriteExtractKPD_ool;
		} 
		break;
	    case MACH_MSG_OOL_PORTS_DESCRIPTOR:
		arg->argKPD_Init = KPD_noop;
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_oolport;
		    arg->argKPD_Pack = WriteKPD_oolport;
		}
		if akCheck(arg->argKind, akbReturnKPD) {
	 	    arg->argKPD_TypeCheck = WriteTCheckKPD_oolport;
		    arg->argKPD_Extract = WriteExtractKPD_oolport;
		}
		break;
	    default:
		printf("MiG internal error: type of kernel processed data unknown\n");
		exit(1);
	    }   /* end of switch */
}

/*************************************************************
 *  Writes all the code comprising a routine body. Called by
 *  WriteUser for each routine.
 *************************************************************/
static void
WriteRoutine(file, rt)
    FILE *file;
    register routine_t *rt;
{
    /* initialize the disciplines for the handling of KPDs */

    InitKPD_Disciplines(rt->rtArgs);

    /* write the stub's declaration */

    WriteStubDecl(file, rt);

    /* Use the RPC trap for user-user and user-kernel RPC */
    if (UseRPCTrap)
	WriteRPCRoutine(file, rt);

    /* write the code for doing a short-circuited RPC: */
    if (ShortCircuit)
	WriteShortCircRPC(file, rt);

    fprintf(file, "    {\n");

    /* typedef of structure for Request and Reply messages */
    WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbRequest, 
	"Request", rt->rtSimpleRequest, FALSE, TRUE, FALSE);
    if (!rt->rtOneWay)
	WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbReply, 
	"Reply", rt->rtSimpleReply, rt->rtUserImpl, TRUE, FALSE);
    if (rt->rtOverwrite)
	WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbReply|akbOverwrite,
	"OverwriteTemplate", FALSE, FALSE, TRUE, TRUE);
    /*
     * Define a Minimal Reply structure to be used in case of errors
     */
    fprintf(file, "\t/*\n");
    fprintf(file, "\t * typedef struct {\n");
    fprintf(file, "\t * \tmach_msg_header_t Head;\n");
    fprintf(file, "\t * \tNDR_record_t NDR;\n");
    fprintf(file, "\t * \tkern_return_t RetCode;\n");
    fprintf(file, "\t * } mig_reply_error_t;\n");
    fprintf(file, "\t */\n");
    fprintf(file, "\n");


    /* declarations for local vars: Union of Request and Reply messages,
       InP, OutP and return value */

    WriteVarDecls(file, rt);

    /* declarations and initializations of the mach_msg_type_descriptor_t variables
       for each argument that is a Kernel Processed Data */

    WriteList(file, rt->rtArgs, WriteTemplateDeclIn, akbRequest | akbSendKPD, "\n", "\n");
 
    WriteRetCodeArg(file, rt);

    /* fill in the fields that are non related to parameters */

    if (!rt->rtSimpleRequest)
	fprintf(file, "\tInP->msgh_body.msgh_descriptor_count = %d;\n",
	    rt->rtRequestKPDs);

    /* fill in all the request message types and then arguments */

    WriteRequestArgs(file, rt);

    /* fill in request message head */

    WriteRequestHead(file, rt);
    fprintf(file, "\n");

    /* Write the send/receive or rpc call */

    if (UseEventLogger)
	WriteLogMsg(file, rt, LOG_USER, LOG_REQUEST);

    if (rt->rtOneWay)
	WriteMsgSend(file, rt);
    else
    {
	if (UseMsgRPC) {
	    /* overwrite mode meaningful only when UseMsgRPC is enabled */
	    if (rt->rtOverwrite) 
		WriteOverwriteTemplate(file, rt);
	    WriteMsgRPC(file, rt);
	} else
	    WriteMsgSendReceive(file, rt);

	/* Check the values that are returned in the reply message */
	    
	fprintf(file, "\n");
	fprintf(file, "\t/* look at the NDR record and type convert if needed */\n");
	fprintf(file, "\t\t/* NOT IMPLEMENTED YET */\n");
	fprintf(file, "\n");

	WriteCheckIdentity(file, rt);
        if (rt->rtUserImpl)
	    WriteCheckTrailerHead(file, TRUE);	

	/* If the reply message has no Out parameters or return values
	   other than the return code, we can type-check it and
	   return it directly. */

	if (rt->rtNoReplyArgs) 
	    WriteReturn(file, rt, "\t", stRetCode, "\n");
	else {
	    if (UseEventLogger)
		WriteLogMsg(file, rt, LOG_USER, LOG_REPLY);

	    WriteRetCodeCheck(file, rt);

	    /* Type Checking for the Out parameters which are typed */
	    WriteList(file, rt->rtArgs, WriteTypeCheck, akbReturnKPD, "\n", "\n");

	    WriteReplyArgs(file, rt);

	    /* return the return value, if any */

	    WriteReturnValue(file, rt);
	}
    }
    fprintf(file, "    }\n");
    fprintf(file, "}\n");
}

static void
WriteRPCClientFunctions(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    char *fname;

    fprintf(file, "#ifdef AUTOTEST\n");
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine)
	{
	    fname = stat->stRoutine->rtName;
	    fprintf(file, "extern void client_%s(mach_port_t, int, char **);\n", fname);
	}
    fprintf(file, "function_table_entry %s_client_functions[] =\n", SubsystemName);
    fprintf(file, "{\n");
    for (stat = stats; stat != stNULL; stat = stat->stNext)
        if (stat->stKind == skRoutine)
	{
	    fname = stat->stRoutine->rtName;
	    fprintf(file, "    { \"%s\", client_%s },\n", fname, fname);
	}
    fprintf(file, "    { (char *) 0, (function_ptr_t) 0 }\n");
    fprintf(file, "};\n");
    fprintf(file, "#endif /* AUTOTEST */\n");
}

/*************************************************************
 *  Writes out the xxxUser.c file. Called by mig.c
 *************************************************************/
void
WriteUser(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;

    WriteProlog(file);
    if (TestRPCTrap)
	WriteRPCClientFunctions(file, stats);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteRoutine(file, stat->stRoutine);
	    break;
	  case skImport:
	  case skUImport:
	  case skSImport:
	    break;
	  default:
	    fatal("WriteUser(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
    WriteEpilog(file);
}

/*************************************************************
 *  Writes out individual .c user files for each routine.  Called by mig.c
 *************************************************************/
void
WriteUserIndividual(stats)
    statement_t *stats;
{
    register statement_t *stat;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    {
		FILE *file;
		register char *filename;

		filename = strconcat(UserFilePrefix,
				     strconcat(stat->stRoutine->rtName, ".c"));
		file = fopen(filename, "w");
		if (file == NULL)
		    fatal("fopen(%s): %s", filename,
			  strerror(errno));
		WriteProlog(file);
		WriteRoutine(file, stat->stRoutine);
		WriteEpilog(file);
		fclose(file);
		strfree(filename);
	    }
	    break;
	  case skImport:
	  case skUImport:
	  case skSImport:
	    break;
	  default:
	    fatal("WriteUserIndividual(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}
}
