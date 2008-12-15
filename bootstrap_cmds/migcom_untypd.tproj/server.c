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
 * 92/03/03  16:25:17  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  12:32:30  jeffreyh]
 * 
 * 92/01/14  16:46:39  rpd
 * 	Modified WriteInitializeCount, WriteExtractArg
 * 	for the revised CountInOut implementation.
 * 	Fixed Indefinite code generation, to allow short type descriptors.
 * 	Added deallocate bit handling to Indefinite code generation.
 * 	[92/01/08            rpd]
 * 
 * 92/01/03  20:30:09  dbg
 * 	Generate <subsystem>_server_routine to return unpacking function
 * 	pointer.
 * 	[91/11/11            dbg]
 * 
 * 	For inline variable-length arrays that are Out parameters, allow
 * 	passing the user's count argument to the server as an InOut
 * 	parameter.
 * 	[91/11/11            dbg]
 * 
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
 * 91/08/28  11:17:21  jsb
 * 	Replaced ServerProcName with ServerDemux.
 * 	[91/08/13            rpd]
 * 
 * 	Removed Camelot and TrapRoutine support.
 * 	Changed MsgKind to MsgSeqno.
 * 	[91/08/12            rpd]
 * 
 * 91/07/31  18:10:51  dbg
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
 * 91/06/25  10:31:51  rpd
 * 	Cast request and reply ports to ipc_port_t in KernelServer stubs.
 * 	[91/05/27            rpd]
 * 
 * 91/02/05  17:55:37  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:55:30  mrt]
 * 
 * 90/06/02  15:05:29  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:13:12  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 * 18-Oct-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Set the local port in the server reply message to
 *	MACH_PORT_NULL for greater efficiency and to make Camelot
 *	happy.
 *
 * 18-Apr-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed call to WriteLocalVarDecl in WriteMsgVarDecl
 *	to write out the parameters for the C++ code to a call
 *	a new routine WriteServerVarDecl which includes the *
 *	for reference variable, but uses the transType if it
 *	exists.
 *
 * 27-Feb-88  Richard Draves (rpd) at Carnegie-Mellon University
 *	Changed reply message initialization for camelot interfaces.
 *	Now we assume camelot interfaces are all camelotroutines and
 *	always initialize the dummy field & tid field.  This fixes
 *	the wrapper-server-call bug in distributed transactions.
 *
 * 23-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed the include of camelot_types.h to cam/camelot_types.h
 *
 * 19-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Fixed WriteDestroyArg to not call the destructor
 *	function on any in/out args.
 *
 *  4-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Fixed dld's code to write out parameter list to
 *	use WriteLocalVarDecl to get transType or ServType if
 *	they exist.
 *
 * 19-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Change variable-length inline array declarations to use
 *	maximum size specified to Mig.  Make message variable
 *	length if the last item in the message is variable-length
 *	and inline.  Use argMultipler field to convert between
 *	argument and IPC element counts.
 *
 * 18-Jan-88  David Detlefs (dld) at Carnegie-Mellon University
 *	Modified to produce C++ compatible code via #ifdefs.
 *	All changes have to do with argument declarations.
 *
 *  2-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Added destructor function for IN arguments to server.
 *
 * 18-Nov-87  Jeffrey Eppinger (jle) at Carnegie-Mellon University
 *	Changed to typedef "novalue" as "void" if we're using hc.
 *
 * 17-Sep-87  Bennet Yee (bsy) at Carnegie-Mellon University
 *	Added _<system>SymTab{Base|End} for use with security
 *	dispatch routine.  It is neccessary for the authorization
 *	system to know the operations by symbolic names.
 *	It is harmless to user code as it only means an extra
 *	array if it is accidentally turned on.
 *
 * 24-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Corrected the setting of retcode for CamelotRoutines.
 *
 * 21-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added deallocflag to call to WritePackArgType.
 *
 * 14-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Moved type declarations and assignments for DummyType 
 *	and tidType to server demux routine. Automatically
 *	include camelot_types.h and msg_types.h for interfaces 
 *	containing camelotRoutines.
 *
 *  8-Jun-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Removed #include of sys/types.h and strings.h from WriteIncludes.
 *	Changed the KERNEL include from ../h to sys/
 *	Removed extern from WriteServer to make hi-c happy
 *
 * 28-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#include <assert.h>

#include "type.h"
#include <mach/message.h>
#include "write.h"
#include "utils.h"
#include "global.h"
#include "error.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif  /* max */

void WriteLogDefines();
void WriteIdentificationString();

static void
WriteKPD_Iterator(file, in, varying, arg, bracket)
    FILE *file;
    boolean_t in, varying;
    argument_t *arg;
    boolean_t bracket;
{
    register ipc_type_t *it = arg->argType;
    char string[MAX_STR_LEN];

    fprintf(file, "\t{\n");
    fprintf(file, "\t    register\t%s\t*ptr;\n", it->itServerKPDType);
    fprintf(file, "\t    register\ti");
    if (varying && !in)
	fprintf(file, ", j");
    fprintf(file, ";\n\n");

    if (in)
	sprintf(string, "In%dP", arg->argRequestPos);
    else
	sprintf(string, "OutP");

    fprintf(file, "\t    ptr = &%s->%s[0];\n", string, arg->argMsgField);

    if (varying) {
	register argument_t *count = arg->argCount;

	if (in) 
            fprintf(file, "\t    for (i = 0; i < In%dP->%s; ptr++, i++) %s\n",
		count->argRequestPos, count->argMsgField,
		(bracket) ? "{" : "");
	else {
	    fprintf(file, "\t    j = min(%d, ", it->itKPD_Number);
	    if (akCheck(count->argKind, akbVarNeeded))
		fprintf(file, "%s);\n", count->argName);
	    else
		fprintf(file, "%s->%s);\n", string, count->argMsgField);
            fprintf(file, "\t    for (i = 0; i < j; ptr++, i++) %s\n",
		(bracket) ? "{" : "");
	}
    } else
        fprintf(file, "\t    for (i = 0; i < %d; ptr++, i++) %s\n", it->itKPD_Number,
	    (bracket) ? "{" : "");
}

static void
WriteIncludes(file)
    FILE *file;
{
    if (ServerHeaderFileName != strNULL)
    {
        register char *cp;

        /* Strip any leading path from ServerHeaderFileName. */
        cp = strrchr(ServerHeaderFileName, '/');
        if (cp == 0)
            cp = ServerHeaderFileName;
        else
            cp++;       /* skip '/' */
        fprintf(file, "#include \"%s\"\n", cp); 
    } else {
	fprintf(file, "#include <string.h>\n");
	fprintf(file, "#include <mach/ndr.h>\n");
	fprintf(file, "#include <mach/boolean.h>\n");
	fprintf(file, "#include <mach/kern_return.h>\n");
	fprintf(file, "#include <mach/message.h>\n");
	fprintf(file, "#include <mach/mig_errors.h>\n");
	fprintf(file, "#ifndef\tNO_DIRECT_RPC\n");
	fprintf(file, "#include <mach/rpc.h>\n");
	fprintf(file, "#endif\t/* NO_DIRECT_RPC */\n");
	if (IsKernelServer)
	    fprintf(file, "#include <ipc/ipc_port.h>\n");
	else
	    fprintf(file, "#include <mach/port.h>\n");
    }
    if (UseEventLogger) {
	if (IsKernelServer) {
	    fprintf(file, "#ifdef\tMACH_KERNEL\n");
	    fprintf(file, "#include <mig_debug.h>\n");
	    fprintf(file, "#endif\t/* MACH_KERNEL */\n");
        }
	fprintf(file, "#if  MIG_DEBUG\n"); 
	fprintf(file, "#include <mach/mig_log.h>\n");
	fprintf(file, "#endif /* MIG_DEBUG */\n"); 
    }
    fprintf(file, "\n");
}

static void
WriteGlobalDecls(file)
    FILE *file;
{
    if (BeAnsiC) {
        fprintf(file, "#define novalue void\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "#define novalue void\n");
        fprintf(file, "#else\n");
        fprintf(file, "#define novalue int\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }
    fprintf(file, "\n");

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
    if (UseEventLogger)
	WriteLogDefines(file, "MACH_MSG_LOG_SERVER");
    fprintf(file, "#define MIG_RETURN_ERROR(X, code)\t{\\\n");
    fprintf(file, "\t\t\t\t((mig_reply_error_t *)X)->RetCode = code;\\\n");
    fprintf(file, "\t\t\t\t((mig_reply_error_t *)X)->NDR = NDR_record;\\\n");
    fprintf(file, "\t\t\t\treturn;\\\n");
    fprintf(file, "\t\t\t\t}\n");
    fprintf(file, "\n");
}

static void
WriteForwardDeclarations(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;

    fprintf(file, "/* Forward Declarations */\n\n");
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine) {
	    fprintf(file, "\nmig_internal novalue _X%s\n", 
		    stat->stRoutine->rtName);
	    fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);\n");
	}
    fprintf(file, "\n");
}

static void
WriteProlog(file)
    FILE *file;
{
    WriteIdentificationString(file);
    fprintf(file, "\n");
    fprintf(file, "/* Module %s */\n", SubsystemName);
    fprintf(file, "\n");
    
    WriteIncludes(file);
    WriteBogusDefines(file);
    WriteGlobalDecls(file);
}


static void
WriteSymTabEntries(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    register u_int current = 0;

    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine) {
	    register	num = stat->stRoutine->rtNumber;
	    char	*name = stat->stRoutine->rtName;
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
WriteRoutineEntries(file, stats)
    FILE *file;
    statement_t *stats;
{
    register u_int current = 0;
    register statement_t *stat;
    char *sig_array, *rt_name;
    int arg_count, descr_count;
    int offset = 0;

    fprintf(file, "\t{\n");
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine)
	{
	    register routine_t *rt = stat->stRoutine;

	    sig_array = (char *) malloc(strlen(ServerSubsys)+50);
	    rt_name = (char *) malloc(strlen(rt->rtName)+5);
	    while (current++ < rt->rtNumber)
		fprintf(file, "\t\t{0, 0, 0, 0, 0, 0},\n");
	    sprintf(sig_array, "&%s.arg_descriptor[%d]", ServerSubsys, offset);
	    sprintf(rt_name, "_X%s", rt->rtName);
	    descr_count = rtCountArgDescriptors(rt->rtArgs, &arg_count);
	    offset += descr_count;
	    WriteRPCRoutineDescriptor(file, rt,
				      arg_count, descr_count,
				      stat->stRoutine->rtServerName,
				      rt_name, sig_array);
	    fprintf(file, ",\n");
	    free(sig_array);
	    free(rt_name);
	}
    while (current++ < rtNumber)
	fprintf(file, "\t\t{0, 0, 0, 0, 0},\n");

    fprintf(file, "\t},\n\n");
}

static void
WriteArgDescriptorEntries(file, stats)
    FILE *file;
    statement_t *stats;
{
    register u_int current = 0;
    register statement_t *stat;

    fprintf(file, "\t{\n");
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	if (stat->stKind == skRoutine)
	{
	    register routine_t *rt = stat->stRoutine;

	    /* For each arg of the routine, write an arg descriptor:
	     */
	    WriteRPCRoutineArgDescriptor(file, rt);
	}
    fprintf(file, "\t},\n\n");
}


/*
 * Write out the description of this subsystem, for use in direct RPC
 */
static void
WriteSubsystem(file, stats, maxsize)
    FILE *file;
    statement_t *stats;
    unsigned int maxsize;
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
    fprintf(file, "\n/* Description of this subsystem, for use in direct RPC */\n");
    if (ServerHeaderFileName == strNULL) {
	fprintf(file, "struct %s {\n", ServerSubsys);
	fprintf(file, "\tstruct subsystem *\tsubsystem;\t/* Reserved for system use */\n");
	fprintf(file, "\tmach_msg_id_t\tstart;\t/* Min routine number */\n");
	fprintf(file, "\tmach_msg_id_t\tend;\t/* Max routine number + 1 */\n");
	fprintf(file, "\tunsigned int\tmaxsize;\t/* Max msg size */\n");
	fprintf(file, "\tvm_address_t\tbase_addr;\t/* Base ddress */\n");
	fprintf(file, "\tstruct routine_descriptor\t/*Array of routine descriptors */\n");
	fprintf(file, "\t\troutine[%d];\n", rtNumber);
	fprintf(file, "\tstruct routine_arg_descriptor\t/*Array of arg descriptors */\n");
	fprintf(file, "\t\targ_descriptor[%d];\n", descr_count);
	fprintf(file, "} %s = {\n", ServerSubsys);
    } else {
	fprintf(file, "struct %s %s = {\n", ServerSubsys, ServerSubsys);
    }
    fprintf(file, "\t0,\n");
    fprintf(file, "\t%d,\n", SubsystemBase);
    fprintf(file, "\t%d,\n", SubsystemBase + rtNumber);
    fprintf(file, "\t%d,\n", maxsize);
    fprintf(file, "\t(vm_address_t)&%s,\n", ServerSubsys);

    WriteRoutineEntries(file, stats);
    WriteArgDescriptorEntries(file, stats);

    fprintf(file, "};\n\n");
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
WriteDispatcher(file, stats, maxsize, doSubsystem)
    FILE *file;
    statement_t *stats;
    unsigned int maxsize;
    boolean_t doSubsystem;
{
    fprintf(file, "\n");

    /*
     * First, the RPC subsystem structure:
     */
    if (doSubsystem)
	WriteSubsystem(file, stats, maxsize);
    else {
	/*
	 * First, the symbol table
	 */
	fprintf(file, "static mig_routine_t %s_routines[] = {\n", ServerDemux);

     	WriteArrayEntries(file, stats);

     	fprintf(file, "};\n");
     	fprintf(file, "\n");
    }

     /*
      * Then, the server routine
      */
    fprintf(file, "mig_external boolean_t %s\n", ServerDemux);
    if (BeAnsiC) {
        fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");
        fprintf(file, "#else\n");
        fprintf(file, "\t(InHeadP, OutHeadP)\n");
        fprintf(file, "\tmach_msg_header_t *InHeadP, *OutHeadP;\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }
    
    fprintf(file, "{\n");
    fprintf(file, "\t/*\n");
    fprintf(file, "\t * typedef struct {\n");
    fprintf(file, "\t * \tmach_msg_header_t Head;\n");
    fprintf(file, "\t * \tNDR_record_t NDR;\n");
    fprintf(file, "\t * \tkern_return_t RetCode;\n");
    fprintf(file, "\t * } mig_reply_error_t;\n");
    fprintf(file, "\t */\n");
    fprintf(file, "\n");
    
    fprintf(file, "\tregister mig_routine_t routine;\n");
    fprintf(file, "\n");
    
    fprintf(file, "\tOutHeadP->msgh_bits = ");
    fprintf(file, "MACH_MSGH_BITS(MACH_MSGH_BITS_REPLY(InHeadP->msgh_bits), 0);\n");
    fprintf(file, "\tOutHeadP->msgh_remote_port = InHeadP->msgh_reply_port;\n");
    fprintf(file, "\t/* Minimal size: routine() will update it if different */\n");
    fprintf(file, "\tOutHeadP->msgh_size = sizeof(mig_reply_error_t);\n");
    fprintf(file, "\tOutHeadP->msgh_local_port = MACH_PORT_NULL;\n");
    fprintf(file, "\tOutHeadP->msgh_id = InHeadP->msgh_id + 100;\n");
    fprintf(file, "\n");
    
    fprintf(file, "\tif ((InHeadP->msgh_id > %d) || (InHeadP->msgh_id < %d) ||\n",
    	SubsystemBase + rtNumber - 1, SubsystemBase);
    if (doSubsystem)
    	fprintf(file, "\t    ((routine = %s.routine[InHeadP->msgh_id - %d].stub_routine) == 0)) {\n",
    	ServerSubsys, SubsystemBase);
    else
    	fprintf(file, "\t    ((routine = %s_routines[InHeadP->msgh_id - %d]) == 0)) {\n",
	    ServerDemux, SubsystemBase);
    fprintf(file, "\t\t((mig_reply_error_t *)OutHeadP)->NDR = NDR_record;\n");
    fprintf(file, "\t\t((mig_reply_error_t *)OutHeadP)->RetCode = MIG_BAD_ID;\n");
    if (UseEventLogger) {
        fprintf(file, "#if  MIG_DEBUG\n");
        fprintf(file, "\t\tLOG_ERRORS(MACH_MSG_LOG_SERVER, MACH_MSG_ERROR_UNKNOWN_ID,\n");
        fprintf(file, "\t\t\t&InHeadP->msgh_id, __FILE__, __LINE__);\n");
        fprintf(file, "#endif /* MIG_DEBUG */\n");
    }
    fprintf(file, "\t\treturn FALSE;\n");
    fprintf(file, "\t}\n");
    
    /* Call appropriate routine */
    fprintf(file, "\t(*routine) (InHeadP, OutHeadP);\n");
    fprintf(file, "\treturn TRUE;\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    
    /*
     * Then, the <subsystem>_server_routine routine
     */
    fprintf(file, "mig_external mig_routine_t %s_routine\n", ServerDemux);
    if (BeAnsiC) {
        fprintf(file, "\t(mach_msg_header_t *InHeadP)\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "\t(mach_msg_header_t *InHeadP)\n");
        fprintf(file, "#else\n");
        fprintf(file, "\t(InHeadP)\n");
        fprintf(file, "\tmach_msg_header_t *InHeadP;\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }
    
    fprintf(file, "{\n");
    fprintf(file, "\tregister int msgh_id;\n");
    fprintf(file, "\n");
    fprintf(file, "\tmsgh_id = InHeadP->msgh_id - %d;\n", SubsystemBase);
    fprintf(file, "\n");
    fprintf(file, "\tif ((msgh_id > %d) || (msgh_id < 0))\n",
    	rtNumber - 1);
    fprintf(file, "\t\treturn 0;\n");
    fprintf(file, "\n");
    if (doSubsystem)
    	fprintf(file, "\treturn %s.routine[msgh_id].stub_routine;\n", ServerSubsys);
    else
    	fprintf(file, "\treturn %s_routines[msgh_id];\n", ServerDemux);
    fprintf(file, "}\n");

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
static char *
ServerSideType(rt)
    routine_t *rt;
{
    return rt->rtRetCode->argType->itTransType;
}

static void
WriteRetCode(file, ret)
    FILE *file;
    register argument_t *ret;
{
    register ipc_type_t *it = ret->argType;

    if (akCheck(ret->argKind, akbVarNeeded)) {
	fprintf(file, "\t%s %s;\n", it->itTransType, ret->argVarName);
    }
}

static void
WriteLocalVarDecl(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    register ipc_type_t *btype = it->itElement;

    if (IS_VARIABLE_SIZED_UNTYPED(it))
	fprintf(file, "\t%s %s[%d]", btype->itTransType,
		arg->argVarName, it->itNumber/btype->itNumber);
    else if (IS_MULTIPLE_KPD(it)) {
	if (btype->itTransType != strNULL)
	    fprintf(file, "\t%s %s[%d]", btype->itTransType,
		arg->argVarName, it->itKPD_Number);
	else
	    /* arrays of ool or oolport */
	    fprintf(file, "\tvoid *%s[%d]", 
		arg->argVarName, it->itKPD_Number);
    } else
	fprintf(file, "\t%s %s", it->itTransType, arg->argVarName);
}

static void
WriteServerArgDecl(file, arg)
    FILE *file;
    argument_t *arg;
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
WriteVarDecls(file, rt)
    FILE *file;
    routine_t *rt;
{
    int i;
    boolean_t NeedMsghSize = FALSE;
    boolean_t NeedMsghSizeDelta = FALSE;

    fprintf(file, "\tregister Request *In0P = (Request *) InHeadP;\n");
    for (i = 1; i <= rt->rtMaxRequestPos; i++)
	fprintf(file, "\tregister Request *In%dP;\n", i);
    fprintf(file, "\tregister Reply *OutP = (Reply *) OutHeadP;\n");

    /* if either request or reply is variable, we may need
       msgh_size_delta and msgh_size */

    if (rt->rtNumRequestVar > 0 || rt->rtNumReplyVar > 1)
	NeedMsghSize = TRUE;
    if (rt->rtMaxRequestPos > 0 || rt->rtMaxReplyPos > 0)
	NeedMsghSizeDelta = TRUE;

    if (NeedMsghSize)
	fprintf(file, "\tunsigned int msgh_size;\n");
    if (NeedMsghSizeDelta)
	fprintf(file, "\tunsigned int msgh_size_delta;\n");
    if (rt->rtServerImpl) {
	fprintf(file, "\tmach_msg_format_0_trailer_t *TrailerP;\n");
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tunsigned int trailer_size;\n");
        fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    
    if (NeedMsghSize || NeedMsghSizeDelta)
	fprintf(file, "\n");
}

static void
WriteReplyInit(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "\n");
    if 	(rt->rtNumReplyVar > 1 || rt->rtMaxReplyPos) 
	/* WritheAdjustMsgSize() has been executed at least once! */
	fprintf(file, "\tOutP = (Reply *) OutHeadP;\n");

    if (!rt->rtSimpleReply)  /* complex reply message */
	fprintf(file,
		"\tOutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;\n");

    if (rt->rtNumReplyVar == 0)
	fprintf(file, "\tOutP->Head.msgh_size = %d;\n", rt->rtReplyMinSize);
    else if (rt->rtNumReplyVar > 1)
	fprintf(file, "\tOutP->Head.msgh_size = msgh_size;\n");
    /* the case rt->rtNumReplyVar = 1 is taken care of in WriteAdjustMsgSize() */
}

static void
WriteRetCArgCheckError(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "\tif (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&\n");
    fprintf(file, "\t    (In0P->Head.msgh_size == sizeof(mig_reply_error_t)))\n");
    fprintf(file, "\t{\n");
}

static void
WriteRetCArgFinishError(file, rt)
    FILE *file;
    routine_t *rt;
{
    argument_t *retcode = rt->rtRetCArg;

    fprintf(file, "\treturn;\n");
    fprintf(file, "\t}\n");
    retcode->argMsgField = "KERN_SUCCESS";
}

static void
WriteCheckHead(file, rt)
    FILE *file;
    routine_t *rt;
{
    fprintf(file, "#if\tTypeCheck\n");
    if (rt->rtNumRequestVar > 0)
	fprintf(file, "\tmsgh_size = In0P->Head.msgh_size;\n");

    if (rt->rtSimpleRequest) {
	/* Expecting a simple message. */
	fprintf(file,
	    "\tif ((In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||\n");
	if (rt->rtNumRequestVar > 0)
	    fprintf(file, "\t    (msgh_size < %d) ||  (msgh_size > %d))\n",
		rt->rtRequestMinSize, rt->rtRequestMaxSize);
	else 
	    fprintf(file, "\t    (In0P->Head.msgh_size != %d))\n",
			rt->rtRequestMinSize);
    } else {
	/* Expecting a complex message. */

	fprintf(file, "\tif (");
	if (rt->rtRetCArg != argNULL)
	    fprintf(file, "(");
	fprintf(file, "!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||\n");
	fprintf(file, "\t    (In0P->msgh_body.msgh_descriptor_count != %d) ||\n",
		rt->rtRequestKPDs);
	if (rt->rtNumRequestVar > 0)
	    fprintf(file, "\t    (msgh_size < %d) ||  (msgh_size > %d)",
		rt->rtRequestMinSize, rt->rtRequestMaxSize);
	else
	    fprintf(file, "\t    (In0P->Head.msgh_size != %d)",
		rt->rtRequestMinSize);
	if (rt->rtRetCArg == argNULL) 
	    fprintf(file, ")\n");
	else {
	    fprintf(file, ") &&\n");
	    fprintf(file, "\t    ((In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||\n");
	    fprintf(file, "\t    In0P->Head.msgh_size != sizeof(mig_reply_error_t) ||\n");
      	    fprintf(file, "\t    ((mig_reply_error_t *)In0P)->RetCode == KERN_SUCCESS))\n");	
        }
    }
    WriteReturnMsgError(file, rt, FALSE, argNULL, "MIG_BAD_ARGUMENTS");
    fprintf(file, "#endif\t/* TypeCheck */\n");
    fprintf(file, "\n");
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

    fprintf(file, "In%dP->%s", count->argRequestPos, count->argMsgField); /* XXX */

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

    /* If there aren't any more In args after this, then
       we can use the msgh_size_delta value directly in
       the TypeCheck conditional. */

    if (arg->argRequestPos == rt->rtMaxRequestPos)
    {
	fprintf(file, "#if\tTypeCheck\n");
	fprintf(file, "\tif (msgh_size != %d + (",
		rt->rtRequestMinSize);
	WriteCheckArgSize(file, arg);
	fprintf(file, "))\n");

	WriteReturnMsgError(file, rt, FALSE, arg, "MIG_BAD_ARGUMENTS");
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
		rt->rtRequestMinSize);
	else
	    fprintf(file, "\tif (msgh_size < %d + msgh_size_delta)\n",
		rt->rtRequestMinSize);
	WriteReturnMsgError(file, rt, FALSE, arg, "MIG_BAD_ARGUMENTS");

	if (!LastVarArg)
	    fprintf(file, "\tmsgh_size -= msgh_size_delta;\n");

	fprintf(file, "#endif\t/* TypeCheck */\n");
    }
    fprintf(file, "\n");
}

static char *
InArgMsgField(arg)
    register argument_t *arg;
{
    static char buffer[MAX_STR_LEN];
    char who[20] = {0};

    /*
     *	Inside the kernel, the request and reply port fields
     *	really hold ipc_port_t values, not mach_port_t values.
     *	Hence we must cast the values.
     */

    if (!(arg->argFlags & flRetCode))
	if (akCheck(arg->argKind, akbServerImplicit)) 
	    sprintf(who, "TrailerP->");
	else
	    sprintf(who, "In%dP->", arg->argRequestPos);

    if (IsKernelServer &&
	((akIdent(arg->argKind) == akeRequestPort) ||
	 (akIdent(arg->argKind) == akeReplyPort)))
	sprintf(buffer, "(ipc_port_t) %s%s", who,
		(arg->argSuffix != strNULL) ? arg->argSuffix : arg->argMsgField);
    else
	sprintf(buffer, "%s%s", who,
		(arg->argSuffix != strNULL) ? arg->argSuffix : arg->argMsgField);

    return buffer;
}

static void
WriteExtractArgValue(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    string_t recast;

    if (IsKernelServer && it->itPortType && streql(it->itServerType, "ipc_port_t")
	&& akIdent(arg->argKind) != akeRequestPort 
	&& akIdent(arg->argKind) != akeReplyPort)
	    recast = "(ipc_port_t)";
    else
	recast = "";
    if (it->itInTrans != strNULL)
	WriteCopyType(file, it, "%s", "/* %s */ %s(%s%s)",
	    arg->argVarName, it->itInTrans, recast, InArgMsgField(arg));
    else
	WriteCopyType(file, it, "%s", "/* %s */ %s%s",
	    arg->argVarName, recast, InArgMsgField(arg));

    fprintf(file, "\n");
}

/*
 * argKPD_Extract discipline for Port types.
 */
static void
WriteExtractKPD_port(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *recast = "";

    WriteKPD_Iterator(file, TRUE, it->itVarArray, arg, FALSE);
    /* translation function do not apply to complex types */
    if (IsKernelServer)
	recast = "(ipc_port_t)";
    fprintf(file, "\t\t%s[i] = %sptr->name;\n", arg->argVarName, recast);
    fprintf(file, "\t}\n");
}

/*
 * argKPD_Extract discipline for out-of-line types.
 */
static void
WriteExtractKPD_ool(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    WriteKPD_Iterator(file, TRUE, it->itVarArray, arg, FALSE);
    fprintf(file, "\t\t%s[i] = ptr->address;\n", arg->argVarName);
    fprintf(file, "\t}\n");
}

/*
 * argKPD_Extract discipline for out-of-line Port types.
 */
static void
WriteExtractKPD_oolport(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    WriteKPD_Iterator(file, TRUE, it->itVarArray, arg, FALSE);
    fprintf(file, "\t\t%s[i] = ptr->address;\n", arg->argVarName);
    fprintf(file, "\t}\n");
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbSendRcv)) {
        register argument_t *poly = arg->argPoly;
        register char *pref = poly->argByReferenceServer ? "*" : "";

        fprintf(file, "\t%s%s = In%dP->%s[0].disposition;\n",
            pref, poly->argVarName, arg->argRequestPos, arg->argMsgField);
    }
}

static void
WriteInitializeCount(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argParent->argType;
    register ipc_type_t *btype = ptype->itElement;
    identifier_t	newstr;

    /*
     *	Initialize 'count' argument for variable-length inline OUT parameter
     *	with maximum allowed number of elements.
     */

    if (akCheck(arg->argKind, akbVarNeeded))
	newstr = arg->argMsgField;
    else
	newstr = (identifier_t)strconcat("OutP->", arg->argMsgField);

    fprintf(file, "\t%s = ", newstr);
    if (IS_MULTIPLE_KPD(ptype))
	fprintf(file, "%d;\n", ptype->itKPD_Number);
    else
	fprintf(file, "%d;\n", ptype->itNumber/btype->itNumber);

    /*
     *	If the user passed in a count, then we use the minimum.
     *	We can't let the user completely override our maximum,
     *	or the user might convince the server to overwrite the buffer.
     */

    if (arg->argCInOut != argNULL) {
	char *msgfield = InArgMsgField(arg->argCInOut);

	fprintf(file, "\tif (%s < %s)\n", msgfield, newstr);
	fprintf(file, "\t\t%s = %s;\n", newstr, msgfield);
    }

    fprintf(file, "\n");
}

static void
WriteAdjustRequestMsgPtr(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *ptype = arg->argType;

    fprintf(file,
	"\tIn%dP = (Request *) ((char *) In%dP + msgh_size_delta - %d);\n\n",
	arg->argRequestPos+1, arg->argRequestPos,
	ptype->itTypeSize + ptype->itPadSize);
}

static void
WriteRequestArgs(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register argument_t *lastVarArg;

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
	 * check parameters and advance the pointers
	 */
	if (akCheckAll(arg->argKind, akbSendRcv|akbSendBody)) {
	    if (akCheck(arg->argKind, akbVariable)) {
	 	WriteCheckMsgSize(file, arg);
		lastVarArg = arg;
	    }
	    continue;
	}
	if (akCheckAll(arg->argKind, akbSendRcv|akbSendKPD)) {
	    /*
	     * KPDs have argRequestPos 0, therefore they escape the above logic
	     */
	    if (arg->argCount != argNULL && lastVarArg) {
		WriteAdjustRequestMsgPtr(file, lastVarArg);
		lastVarArg = argNULL;
	    }
	    continue;
	}
	if (akCheck(arg->argKind, akbServerImplicit)) 
	    WriteCheckTrailerSize(file, FALSE, arg);
    }
}

static void
WriteExtractArg(file, arg)
    FILE *file;
    register argument_t *arg;
{
    if (akCheckAll(arg->argKind, akbSendRcv|akbVarNeeded))
	if (akCheck(arg->argKind, akbSendKPD))
	    (*arg->argKPD_Extract)(file, arg);
	else
	    WriteExtractArgValue(file, arg);

    if ((akIdent(arg->argKind) == akeCount) &&
	akCheck(arg->argKind, akbReturnSnd)) {

	register ipc_type_t *ptype = arg->argParent->argType;
	/*
	 * the count will be initialized to 0 in the case of
	 * unbounded arrays (MigInLine = TRUE): this is because 
	 * the old interface used to pass to the target procedure
	 * the maximum in-line size (it was 2048 bytes)
	 */
	if (IS_VARIABLE_SIZED_UNTYPED(ptype) ||
	    IS_MIG_INLINE_EMUL(ptype) ||
	    (IS_MULTIPLE_KPD(ptype) && ptype->itVarArray))
		WriteInitializeCount(file, arg);
    }
}

static void
WriteServerCallArg(file, arg)
    FILE *file;
    register argument_t *arg;
{
    ipc_type_t *it = arg->argType;
    boolean_t NeedClose = FALSE;
    string_t  at = (arg->argByReferenceServer) ? "&" : "";
    string_t  star = (arg->argByReferenceServer) ? " *" : "";
    string_t  msgfield = 
	(arg->argSuffix != strNULL) ? arg->argSuffix : arg->argMsgField;

    if ((it->itInTrans != strNULL) &&
	akCheck(arg->argKind, akbSendRcv) &&
	!akCheck(arg->argKind, akbVarNeeded))
    {
	fprintf(file, "%s%s(", at, it->itInTrans);
	NeedClose = TRUE;
    }

    if (akCheckAll(arg->argKind, akbVarNeeded|akbServerArg))
	fprintf(file, "%s%s", at, arg->argVarName);
    else if (akCheckAll(arg->argKind, akbSendRcv|akbSendKPD)) { 
	if (!it->itInLine)
	    /* recast the void *, although it is not necessary */
	    fprintf(file, "(%s%s)%s(%s)", it->itTransType, 
		star, at, InArgMsgField(arg));
	else if (IsKernelServer && streql(it->itServerType, "ipc_port_t"))
	    /* recast the port to the kernel internal form value */
	    fprintf(file, "(ipc_port_t%s)%s(%s)", 
		star, at, InArgMsgField(arg));
	else
	    fprintf(file, "%s%s", at, InArgMsgField(arg));
    } else if (akCheck(arg->argKind, akbSendRcv))
	fprintf(file, "%s%s", at, InArgMsgField(arg));
    else if (akCheckAll(arg->argKind, akbReturnSnd|akbReturnKPD)) {
        if (!it->itInLine)
	    /* recast the void *, although it is not necessary */
	    fprintf(file, "(%s%s)%s(OutP->%s)", 
		it->itTransType, star, at, msgfield);
	else if (IsKernelServer && streql(it->itServerType, "ipc_port_t"))
	    /* recast the port to the kernel internal form value */
	    fprintf(file, "(ipc_port_t%s)%s(OutP->%s)", star, at, msgfield);
	else
	    fprintf(file, "%sOutP->%s", at, msgfield);
    } else  if (akCheck(arg->argKind, akbReturnSnd))
	fprintf(file, "%sOutP->%s", at, msgfield);

    if (NeedClose)
	fprintf(file, ")");
}

/*
 * Shrunk version of WriteServerCallArg, to implement the RetCode functionality:
 * we have received a mig_reply_error_t, therefore we want to call the target
 * routine with all 0s except for the error code (and the implicit data).
 * We know that we are a SimpleRoutine.
 */
static void
WriteConditionalCallArg(file, arg)
    FILE *file;
    register argument_t *arg;
{
    ipc_type_t *it = arg->argType;
    boolean_t NeedClose = FALSE;
    string_t  msgfield = 
	(arg->argSuffix != strNULL) ? arg->argSuffix : arg->argMsgField;

    if ((it->itInTrans != strNULL) &&
	akCheck(arg->argKind, akbSendRcv) &&
	!akCheck(arg->argKind, akbVarNeeded))
    {
	fprintf(file, "%s(", it->itInTrans);
	NeedClose = TRUE;
    }

    if (akCheck(arg->argKind, akbSendRcv)) {
	if (akIdent(arg->argKind) == akeRequestPort ||
	    akCheck(arg->argKind, akbServerImplicit)) 
	        fprintf(file, "%s", InArgMsgField(arg));
	else if (akIdent(arg->argKind) == akeRetCode)
	    fprintf(file, "((mig_reply_error_t *)In0P)->RetCode");
        else
	    fprintf(file, "(%s)(0)", it->itTransType);
    }

    if (NeedClose)
	fprintf(file, ")");
}

static void
WriteDestroyArg(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    /*
     * Deallocate IN/INOUT out-of-line args if specified by "auto" flag.
     *
     * We also have to deallocate in the cases where the target routine
     * is given a itInLine semantic whereas the underlying transmission
     * was out-of-line
     */
    if ((argIsIn(arg) && akCheck(arg->argKind, akbSendKPD|akbReturnKPD) &&
			 arg->argKPD_Type == MACH_MSG_OOL_DESCRIPTOR &&
			 (arg->argFlags & flAuto))
		||
	IS_MIG_INLINE_EMUL(it)
       ) {
	/*
	 * Deallocate only if out-of-line.
	 */
	argument_t *count = arg->argCount;
	ipc_type_t *btype = it->itElement;
	int	multiplier = btype->itSize / (8 * btype->itNumber);

        if (IsKernelServer)
            fprintf(file, "\tvm_map_copy_discard(%s);\n", InArgMsgField(arg));
        else {
            fprintf(file, "\tmig_deallocate((vm_offset_t) %s, ",
                InArgMsgField(arg));
	    if (it->itVarArray) {
		if (multiplier > 1)
		    fprintf(file, "%d * ", multiplier);
		fprintf(file, "%s);\n", InArgMsgField(count));
	    } else
		fprintf(file, "%d);\n", (it->itNumber * it->itSize + 7) / 8);
        }
	fprintf(file, "\t%s = (vm_offset_t) 0;\n", 
	    InArgMsgField(arg));
	fprintf(file, "\tIn%dP->%s.size = (mach_msg_size_t) 0;\n",
	    arg->argRequestPos, arg->argMsgField); 
    } else {
	if (akCheck(arg->argKind, akbVarNeeded))
	    fprintf(file, "\t%s(%s);\n", it->itDestructor, arg->argVarName);
	else
	    fprintf(file, "\t%s(%s);\n", it->itDestructor,
		InArgMsgField(arg));
    }
}

static void
WriteDestroyPortArg(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    /*
     *	If a translated port argument occurs in the body of a request
     *	message, and the message is successfully processed, then the
     *	port right should be deallocated.  However, the called function
     *	didn't see the port right; it saw the translation.  So we have
     *	to release the port right for it.
     *
     *  The test over it->itInTrans will exclude any complex type 
     *  made out of ports
     */
    if ((it->itInTrans != strNULL) &&
	(it->itOutName == MACH_MSG_TYPE_PORT_SEND)) {
	    fprintf(file, "\n");
	    fprintf(file, "\tif (IP_VALID((ipc_port_t)%s))\n", InArgMsgField(arg));
	    fprintf(file, "\t\tipc_port_release_send((ipc_port_t)%s);\n", InArgMsgField(arg));
    }
}

/*
 * Check whether WriteDestroyPortArg would generate any code for arg.
 */
boolean_t
CheckDestroyPortArg(arg)
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    if ((it->itInTrans != strNULL) &&
	(it->itOutName == MACH_MSG_TYPE_PORT_SEND))
    {
	return TRUE;
    }
    return FALSE;
}

static void
WriteServerCall(file, rt, func)
    FILE *file;
    routine_t *rt;
    void (*func)();
{
    argument_t *arg = rt->rtRetCode;
    ipc_type_t *it = arg->argType;
    boolean_t NeedClose = FALSE;

    fprintf(file, "\t");
    if (akCheck(arg->argKind, akbVarNeeded))
	fprintf(file, "%s = ", arg->argMsgField);
    else
	fprintf(file, "OutP->%s = ", arg->argMsgField);
    if (it->itOutTrans != strNULL) {
	fprintf(file, "%s(", it->itOutTrans);
	NeedClose = TRUE;
    }
    fprintf(file, "%s(", rt->rtServerName);
    WriteList(file, rt->rtArgs, func, akbServerArg, ", ", "");
    if (NeedClose)
	fprintf(file, ")");
    fprintf(file, ");\n");
}

static void
WriteCheckReturnValue(file, rt)
    FILE *file;
    register routine_t *rt;
{
    argument_t *arg = rt->rtRetCode;
    char string[MAX_STR_LEN];

    if (akCheck(arg->argKind, akbVarNeeded))
	sprintf(string, "%s", arg->argMsgField);
    else 
	sprintf(string, "OutP->%s", arg->argMsgField);
    fprintf(file, "\tif (%s != KERN_SUCCESS) {\n", string);
    fprintf(file, "\t\tMIG_RETURN_ERROR(OutP, %s);\n", string);
    fprintf(file, "\t}\n");
}

/*
 * WriteInitKPD_port, WriteInitKPD_ool, WriteInitKPD_oolport 
 * initializes the OutP KPD fields (this job cannot be done once
 * the target routine has been called, otherwise informations
 * would be lost)
 */
/*
 * argKPD_Init discipline for Port types.
 */
static void
WriteInitKPD_port(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char *subindex = "";
    boolean_t close = FALSE;
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	subindex = "[i]";
	close = TRUE;
    } else {
	(void)sprintf(firststring, "OutP->%s", arg->argMsgField);
	(void)sprintf(string, "OutP->%s.", arg->argMsgField);
    }

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\t%s = %s;\n", firststring,
	arg->argTTName);
    fprintf(file, "#else\t/* UseStaticTemplates */\n");
    if (IS_MULTIPLE_KPD(it) && it->itVarArray)
	fprintf(file, "\t%sname = MACH_PORT_NULL;\n", string);
    if (arg->argPoly == argNULL)
	fprintf(file, "\t%sdisposition = %s;\n", string,
	    IsKernelServer ? it->itOutNameStr : it->itInNameStr); 
    fprintf(file, "\t%stype = MACH_MSG_PORT_DESCRIPTOR;\n", string);
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");
    if (close) 
	fprintf(file, "\t    }\n\t}\n");
    fprintf(file, "\n");
}

/*
 * argKPD_Init discipline for out-of-line types.
 */
static void
WriteInitKPD_ool(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];
    boolean_t VarArray;
    u_int howmany, howbig;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, FALSE, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	howmany = it->itElement->itNumber;
	howbig = it->itElement->itSize;
    } else {
	(void)sprintf(firststring, "OutP->%s", arg->argMsgField);
	(void)sprintf(string, "OutP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	howmany = it->itNumber;
	howbig = it->itSize;
    }

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
    fprintf(file, "#else\t/* UseStaticTemplates */\n");
    if (!VarArray)
        fprintf(file, "\t%ssize = %d;\n", string, 
	    (howmany * howbig + 7)/8);
    if (arg->argDeallocate != d_MAYBE)
	fprintf(file, "\t%sdeallocate =  %s;\n", string,
	    (arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    fprintf(file, "\t%scopy = %s;\n", string, 
	(arg->argFlags & flPhysicalCopy) ? "MACH_MSG_PHYSICAL_COPY" : "MACH_MSG_VIRTUAL_COPY");
#ifdef ALIGNMENT
    fprintf(file, "\t%salignment = MACH_MSG_ALIGN_%d;\n", string, arg->argMsgField,
	(howbig < 8) ? 1 : howbig / 8);
#endif
    fprintf(file, "\t%stype = MACH_MSG_OOL_DESCRIPTOR;\n", string);
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");

    if (IS_MULTIPLE_KPD(it))
	fprintf(file, "\t    }\n\t}\n");
    fprintf(file, "\n");
}

/*
 * argKPD_Init discipline for out-of-line Port types.
 */
static void
WriteInitKPD_oolport(file, arg) 
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;
    boolean_t VarArray;
    ipc_type_t *howit;
    u_int howmany;
    char firststring[MAX_STR_LEN];
    char string[MAX_STR_LEN];

    if (IS_MULTIPLE_KPD(it)) {
        WriteKPD_Iterator(file, FALSE, FALSE, arg, TRUE);
	(void)sprintf(firststring, "\t*ptr");
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	howmany = it->itElement->itNumber;
	howit = it->itElement;
    } else {
	(void)sprintf(firststring, "OutP->%s", arg->argMsgField);
	(void)sprintf(string, "OutP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	howmany = it->itNumber;
	howit = it;
    }

    fprintf(file, "#if\tUseStaticTemplates\n");
    fprintf(file, "\t%s = %s;\n", firststring, arg->argTTName);
    fprintf(file, "#else\t/* UseStaticTemplates */\n");

    if (!VarArray)
        fprintf(file, "\t%scount = %d;\n", string,
            howmany);
    if (arg->argPoly == argNULL) 
    	fprintf(file, "\t%sdisposition = %s;\n", string,
	    IsKernelServer ? howit->itOutNameStr : howit->itInNameStr); 
    if (arg->argDeallocate != d_MAYBE)
        fprintf(file, "\t%sdeallocate =  %s;\n", string,
            (arg->argDeallocate == d_YES) ? "TRUE" : "FALSE");
    fprintf(file, "\t%stype = MACH_MSG_OOL_PORTS_DESCRIPTOR;\n", string);
    fprintf(file, "#endif\t/* UseStaticTemplates */\n");

    if (IS_MULTIPLE_KPD(it))
	fprintf(file, "\t    }\n\t}\n");
    fprintf(file, "\n");
}

static void
WriteInitKPDValue(file, arg)
    FILE *file;
    register argument_t *arg;
{
    (*arg->argKPD_Init)(file, arg);
}

static void
WriteAdjustMsgCircular(file, arg)
    FILE *file;
    register argument_t *arg;
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
     *	been copied into the reply message.  
     * 
     *  The old MiG does not check for circularity in the case of
     *  array of ports. So do I ...
     */

    fprintf(file, "\t  if (IP_VALID((ipc_port_t) In0P->Head.msgh_reply_port) &&\n");
    fprintf(file, "\t    IP_VALID((ipc_port_t) OutP->%s.name) &&\n", arg->argMsgField);
    fprintf(file, "\t    ipc_port_check_circularity((ipc_port_t) OutP->%s.name, (ipc_port_t) In0P->Head.msgh_reply_port))\n", arg->argMsgField);
    fprintf(file, "\t\tOutP->Head.msgh_bits |= MACH_MSGH_BITS_CIRCULAR;\n");
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
    boolean_t close = FALSE;
    char string[MAX_STR_LEN];
    ipc_type_t *real_it;
	     
    if (akCheck(arg->argKind, akbVarNeeded)) {
	if (IS_MULTIPLE_KPD(it)) {
	    WriteKPD_Iterator(file, FALSE, it->itVarArray, arg, TRUE);
	    (void)sprintf(string, "\tptr->");
	    subindex = "[i]";
	    close = TRUE;
	    real_it = it->itElement;
	} else {
	    (void)sprintf(string, "OutP->%s.", arg->argMsgField);
	    real_it = it;
	}
	if (IsKernelServer && streql(real_it->itTransType, "ipc_port_t"))
            recast = "(mach_port_t)";

	if (it->itOutTrans != strNULL && !close)
	    fprintf(file, "\t%sname = (mach_port_t)%s(%s);\n", string,  
		it->itOutTrans, arg->argVarName);
	else
	    fprintf(file, "\t%sname = %s%s%s;\n", string,
		recast, arg->argVarName, subindex);
	if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbReturnSnd)) {
	    register argument_t *poly = arg->argPoly;

	    if  (akCheck(arg->argPoly->argKind, akbVarNeeded)) 
		fprintf(file, "\t%sdisposition = %s;\n", string, 
		    poly->argVarName);
	    else if (close) 
		fprintf(file, "\t%sdisposition = OutP->%s;\n", string,
		    poly->argSuffix);
	}
	if (close) 
	    fprintf(file, "\t    }\n\t}\n");
	fprintf(file, "\n");
    } else  if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbReturnSnd|akbVarNeeded))
	fprintf(file, "\tOutP->%s.disposition = %s;\n", arg->argMsgField,
	    arg->argPoly->argVarName);
    /*
     *	If this is a KernelServer, and the reply message contains
     *	a receive right, we must check for the possibility of a
     *	port/message circularity.  If queueing the reply message
     *	would cause a circularity, we mark the reply message
     *	with the circular bit.
     */
    if (IsKernelServer && !(IS_MULTIPLE_KPD(it)) &&
	((arg->argType->itOutName == MACH_MSG_TYPE_PORT_RECEIVE) ||
	(arg->argType->itOutName == MACH_MSG_TYPE_POLYMORPHIC)))
	    WriteAdjustMsgCircular(file, arg);
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
    char string[MAX_STR_LEN];
    boolean_t VarArray;
    argument_t *count;
    u_int howbig;
    char *subindex;

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, it->itVarArray, arg, TRUE);
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	count = arg->argSubCount;
	howbig = it->itElement->itSize;
	subindex = "[i]";
    } else {
	(void)sprintf(string, "OutP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	count = arg->argCount;
	howbig = it->itSize;
	subindex = "";
    }

    if (akCheck(arg->argKind, akbVarNeeded))
	fprintf(file, "\t%saddress = (void *)%s%s;\n", string, 
            arg->argMsgField, subindex);
    if (arg->argDealloc != argNULL)
	if (akCheck(arg->argDealloc->argKind, akbVarNeeded) || IS_MULTIPLE_KPD(it))
            fprintf(file, "\t%sdeallocate = %s;\n", string,
                 arg->argDealloc->argVarName);
    if (VarArray) {
        fprintf(file, "\t%ssize = ", string);
	if (akCheck(count->argKind, akbVarNeeded))
	    fprintf(file, "%s%s", count->argName, subindex);
	else
	    fprintf(file, "OutP->%s%s", count->argMsgField, subindex);
	
        if (howbig > 8)
            fprintf(file, " * %d;\n", 
		count->argMultiplier * howbig / 8);
        else
            fprintf(file, ";\n");
    } 

    if (IS_MULTIPLE_KPD(it)) {
        fprintf(file, "\t    }\n");
        if (it->itVarArray && !it->itElement->itVarArray) {
            fprintf(file, "\t    for (i = j; i < %d; ptr++, i++)\n", it->itKPD_Number);
	    /* since subordinate arrays aren't variable, they are initialized from template:
	       here we must no-op 'em */
            fprintf(file, "\t\tptr->size = 0;\n");
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
    boolean_t VarArray;
    argument_t *count;
    char *subindex, string[MAX_STR_LEN];

    if (IS_MULTIPLE_KPD(it)) {
	WriteKPD_Iterator(file, FALSE, it->itVarArray, arg, TRUE);
	(void)sprintf(string, "\tptr->");
	VarArray = it->itElement->itVarArray;
	count = arg->argSubCount;
	subindex = "[i]";
    } else {
	(void)sprintf(string, "OutP->%s.", arg->argMsgField);
	VarArray = it->itVarArray;
	count = arg->argCount;
	subindex = "";
    }

    if (akCheck(arg->argKind, akbVarNeeded))
        fprintf(file, "\t%saddress = (void *)%s%s;\n", string,
            arg->argMsgField, subindex);
    if (arg->argDealloc != argNULL)
	if (akCheck(arg->argDealloc->argKind, akbVarNeeded) || IS_MULTIPLE_KPD(it))
            fprintf(file, "\t%sdeallocate = %s;\n", string,
                arg->argDealloc->argVarName);
    if (VarArray) {
        fprintf(file, "\t%scount = ", string);
        if (akCheck(count->argKind, akbVarNeeded))
            fprintf(file, "%s%s;\n", count->argName, subindex);
        else
            fprintf(file, "OutP->%s%s;\n", count->argMsgField, subindex);
    }
    if (arg->argPoly != argNULL && akCheckAll(arg->argPoly->argKind, akbReturnSnd)) 
	if (akCheck(arg->argPoly->argKind, akbVarNeeded) || IS_MULTIPLE_KPD(it))
            fprintf(file, "\t%sdisposition = %s;\n", string,
                arg->argPoly->argVarName);
    if (IS_MULTIPLE_KPD(it)) {
	fprintf(file, "\t    }\n");
	if (it->itVarArray && !it->itElement->itVarArray) {
	    fprintf(file, "\t    for (i = j; i < %d; ptr++, i++)\n", it->itKPD_Number);
            /* since subordinate arrays aren't variable, they are initialized from template:
                here we must no-op 'em */
            fprintf(file, "\t%scount = 0;\n", string);
	}
	fprintf(file, "\t}\n");
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
	WriteKPD_Iterator(file, TRUE, FALSE, arg, TRUE);
	(void)sprintf(string, "ptr->");
	tab = "\t";
	close = TRUE;
    } else 
	(void)sprintf(string, "In%dP->%s.", arg->argRequestPos, arg->argMsgField);

    fprintf(file, "\t%sif (%stype != MACH_MSG_PORT_DESCRIPTOR",
	tab, string);
    if (arg->argPoly == argNULL && !it->itVarArray)
	/* we can't check disposition when poly or VarArray,
	   (because some of the entries could be empty) */
	fprintf(file, " ||\n\t%s    %sdisposition != %s",
	    tab, string, it->itOutNameStr);
    fprintf(file, ")\n");
    WriteReturnMsgError(file, arg->argRoutine, FALSE, arg, "MIG_TYPE_ERROR");
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
	WriteKPD_Iterator(file, TRUE, FALSE, arg, TRUE);
	tab = "\t";
	sprintf(string, "ptr->");
	howmany = it->itElement->itNumber;
	howbig = it->itElement->itSize;
	test = !it->itVarArray && !it->itElement->itVarArray;
    } else {
	tab = "";
	sprintf(string, "In%dP->%s.", arg->argRequestPos, arg->argMsgField);
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
    WriteReturnMsgError(file, arg->argRoutine, FALSE, arg, "MIG_TYPE_ERROR");

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
	WriteKPD_Iterator(file, TRUE, FALSE, arg, TRUE);
	tab = "\t";
	sprintf(string, "ptr->");
	howmany = it->itElement->itNumber;
	test = !it->itVarArray && !it->itElement->itVarArray;
	howstr = it->itElement->itOutNameStr;
    } else {
	tab = "";
	sprintf(string, "In%dP->%s.", arg->argRequestPos, arg->argMsgField);
	howmany = it->itNumber;
	test = !it->itVarArray;
	howstr = it->itOutNameStr;
    }

    fprintf(file, "\t%sif (%stype != MACH_MSG_OOL_PORTS_DESCRIPTOR", tab, string);
    if (test)
        /* if VarArray we may use no-op; if itElement->itVarArray size might change */
        fprintf(file, " ||\n\t%s    %scount != %d", tab, string,
            howmany);
    if (arg->argPoly == argNULL)
        fprintf(file, " ||\n\t%s    %sdisposition != %s", tab, string,
            howstr);
    fprintf(file, ")\n");
    WriteReturnMsgError(file, arg->argRoutine, FALSE, arg, "MIG_TYPE_ERROR");

    if (IS_MULTIPLE_KPD(it)) 
	fprintf(file, "\t    }\n\t}\n");
}

/*************************************************************
 *  Writes code to check that the type of each of the arguments
 *  in the reply message is what is expected. Called by 
 *  WriteRoutine for each in && typed argument in the request message.
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
WritePackArgValueNormal(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    if (IS_VARIABLE_SIZED_UNTYPED(it) || it->itNoOptArray) {
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
	} else if (it->itNoOptArray)
	    fprintf(file, "\t(void)memcpy((char *) OutP->%s, (const char *) %s, %d);\n",
		arg->argMsgField, arg->argVarName, it->itTypeSize);
	else {
	    register argument_t *count = arg->argCount;
	    register ipc_type_t *btype = it->itElement;
	    identifier_t newstr;

	    /* Note btype->itNumber == count->argMultiplier */

	    fprintf(file, "\t(void)memcpy((char *) OutP->%s, (const char *) %s, ",
		arg->argMsgField, arg->argVarName);
	    if (btype->itTypeSize > 1)
	        fprintf(file, "%d * ", btype->itTypeSize);
	    /* count is a akbVarNeeded if arg is akbVarNeeded */
	    if (akCheck(count->argKind, akbVarNeeded))
	        newstr = count->argVarName;
	    else
	        newstr = (identifier_t)strconcat("OutP->", count->argMsgField);
	    fprintf(file, "%s);\n", newstr);
	}
    }
    else if (it->itOutTrans != strNULL)
	WriteCopyType(file, it, "OutP->%s", "/* %s */ %s(%s)",
		      arg->argMsgField, it->itOutTrans, arg->argVarName);
    else
	WriteCopyType(file, it, "OutP->%s", "/* %s */ %s",
		      arg->argMsgField, arg->argVarName);
}

static void
WritePackArgValueVariable(file, arg)
    FILE *file;
    register argument_t *arg;
{
    register ipc_type_t *it = arg->argType;

    /*
     * only itString are treated here so far
     */
    if (it->itString) 
	/* Need to call strlen to calculate the size of the argument. */
	fprintf(file, "\tOutP->%s = strlen(OutP->%s) + 1;\n",
	    arg->argCount->argMsgField, arg->argMsgField);
}

static void
WriteCopyArgValue(file, arg)
    FILE *file;
    argument_t *arg;
{
    fprintf(file, "\n");
    WriteCopyType(file, arg->argType, "/* %d */ OutP->%s", "In%dP->%s",
	arg->argRequestPos, (arg->argSuffix != strNULL) ? arg->argSuffix : arg->argMsgField);
}

static void
WriteInitArgValue(file, arg)
    FILE *file;
    argument_t *arg;
{
    fprintf(file, "\n");
    fprintf(file, "\tOutP->%s = %s;\n\n", arg->argMsgField, arg->argVarName);
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
    if (ptype->itString || !akCheck(count->argKind, akbVarNeeded))
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
	fprintf(file, " + 3 & ~3");
}

/*
 * Adjust message size and advance reply pointer.
 * Called after packing a variable-length argument that
 * has more arguments following.
 */
static void
WriteAdjustMsgSize(file, arg)
    FILE *file;
    register argument_t *arg;
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
			rt->rtReplyMinSize);
    else
    if (arg->argReplyPos == 0)
	/* First variable-length argument.  The previous msgh_size value
	   is the minimum reply size. */

	fprintf(file, "\tmsgh_size = %d + msgh_size_delta;\n",
		rt->rtReplyMinSize);
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
WriteFinishMsgSize(file, arg)
    FILE *file;
    register argument_t *arg;
{
    /* No more Out arguments.  If this is the only variable Out
       argument, we can assign to msgh_size directly. */

    if (arg->argReplyPos == 0) {
	fprintf(file, "\tOutP->Head.msgh_size = %d + (",
			arg->argRoutine->rtReplyMinSize);
	WriteArgSize(file, arg);
	fprintf(file, ");\n");
    }
    else {
	fprintf(file, "\tmsgh_size += ");
	WriteArgSize(file, arg);
	fprintf(file, ";\n");
    }
}

/*
 * Handle reply arguments - fill in message types and copy arguments
 * that need to be copied.
 */
static void
WriteReplyArgs(file, rt)
    FILE *file;
    register routine_t *rt;
{
    register argument_t *arg;
    register argument_t *lastVarArg;

    /*
     * 1. The Kernel Processed Data
     */
    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)  
	if (akCheckAll(arg->argKind, akbReturnSnd|akbReturnKPD))
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
	    lastVarArg->argReplyPos < arg->argReplyPos) {
		WriteAdjustMsgSize(file, lastVarArg);
		lastVarArg = argNULL;
	}

	if (akCheckAll(arg->argKind, akbReturnSnd|akbReturnBody|akbVarNeeded)) 
            WritePackArgValueNormal(file, arg);
	else if (akCheckAll(arg->argKind, akbReturnSnd|akbReturnBody|akbVariable))  
            WritePackArgValueVariable(file, arg);

        if (akCheck(arg->argKind, akbReplyCopy))
            WriteCopyArgValue(file, arg);
        if (akCheck(arg->argKind, akbReplyInit))
            WriteInitArgValue(file, arg);
	/*
	 * Remember whether this was variable-length.
	 */
	if (akCheckAll(arg->argKind, akbReturnSnd|akbReturnBody|akbVariable)) 
	    lastVarArg = arg;
    }
    /*
     * Finish the message size.
     */
    if (lastVarArg != argNULL)
	WriteFinishMsgSize(file, lastVarArg);
}

static void
WriteFieldDecl(file, arg)
    FILE *file;
    argument_t *arg;
{
    if (akCheck(arg->argKind, akbSendKPD) ||
	akCheck(arg->argKind, akbReturnKPD))
	    WriteFieldDeclPrim(file, arg, FetchServerKPDType);
    else
	WriteFieldDeclPrim(file, arg, FetchServerType);
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
     * WriteInitKPD_port, WriteKPD_port,  WriteExtractKPD_port, 
     * WriteInitKPD_ool, WriteKPD_ool,  WriteExtractKPD_ool, 
     * WriteInitKPD_oolport, WriteKPD_oolport,  WriteExtractKPD_oolport
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
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Extract = 
			(IS_MULTIPLE_KPD(arg->argType)) ? WriteExtractKPD_port : WriteExtractArgValue;
		    arg->argKPD_TypeCheck = WriteTCheckKPD_port;
		} 
		if akCheck(arg->argKind, akbReturnKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_port;
		    arg->argKPD_Init = WriteInitKPD_port;
		    arg->argKPD_Pack = WriteKPD_port;
		}
		break;
	    case MACH_MSG_OOL_DESCRIPTOR:
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Extract = 
			(IS_MULTIPLE_KPD(arg->argType)) ? WriteExtractKPD_ool : WriteExtractArgValue;
		    arg->argKPD_TypeCheck = WriteTCheckKPD_ool;
		}
		if akCheck(arg->argKind, akbReturnKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_ool;
		    arg->argKPD_Init = WriteInitKPD_ool;
		    arg->argKPD_Pack = WriteKPD_ool;
		}
		break;
	    case MACH_MSG_OOL_PORTS_DESCRIPTOR:
		if akCheck(arg->argKind, akbSendKPD) {
		    arg->argKPD_Extract = 
			(IS_MULTIPLE_KPD(arg->argType)) ? WriteExtractKPD_oolport : WriteExtractArgValue;
		    arg->argKPD_TypeCheck = WriteTCheckKPD_oolport;
		}
		if akCheck(arg->argKind, akbReturnKPD) {
		    arg->argKPD_Template = WriteTemplateKPD_oolport;
		    arg->argKPD_Init = WriteInitKPD_oolport;
		    arg->argKPD_Pack = WriteKPD_oolport;
		}
		break;
	    default:
		printf("MiG internal error: type of kernel processed data unknown\n");
		exit(1);
	    }   /* end of switch */
}

static void
WriteRoutine(file, rt)
    FILE *file;
    register routine_t *rt;
{
    /*  Declare the server work function: */
    if (ServerHeaderFileName == strNULL)
	WriteServerRoutine(file, rt);

    /* initialize the disciplines for the handling of KPDs */
    InitKPD_Disciplines(rt->rtArgs);

    fprintf(file, "\n");

    fprintf(file, "/* %s %s */\n", rtRoutineKindToStr(rt->rtKind), rt->rtName);
    fprintf(file, "mig_internal novalue _X%s\n", rt->rtName);
    if (BeAnsiC) {
        fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");
    } else {
        fprintf(file, "#if\t%s\n", NewCDecl);
        fprintf(file, "\t(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)\n");
        fprintf(file, "#else\n");
        fprintf(file, "\t(InHeadP, OutHeadP)\n");
        fprintf(file, "\tmach_msg_header_t *InHeadP, *OutHeadP;\n");
        fprintf(file, "#endif\t/* %s */\n", NewCDecl);
    }

    fprintf(file, "{\n");
    WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbRequest, "Request",
	rt->rtSimpleRequest, rt->rtServerImpl, FALSE, FALSE);
    WriteStructDecl(file, rt->rtArgs, WriteFieldDecl, akbReply, "Reply",
	rt->rtSimpleReply, FALSE, FALSE, FALSE);
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

    WriteVarDecls(file, rt);

    WriteList(file, rt->rtArgs,
	IsKernelServer ? WriteTemplateDeclOut : WriteTemplateDeclIn,
	akbReturnKPD, "\n", "\n");

    WriteRetCode(file, rt->rtRetCode);
    WriteList(file, rt->rtArgs, WriteLocalVarDecl,
	akbVarNeeded | akbServerArg, ";\n", ";\n\n");

    if (rt->rtRetCArg != argNULL && !rt->rtSimpleRequest) { 
	WriteRetCArgCheckError(file, rt);
	if (rt->rtServerImpl)
	    WriteCheckTrailerHead(file, rt, FALSE, "In0P");
        WriteServerCall(file, rt, WriteConditionalCallArg);
	WriteRetCArgFinishError(file, rt);
    }

    WriteCheckHead(file, rt);
    if (rt->rtServerImpl)
        WriteCheckTrailerHead(file, rt, FALSE, "In0P");

    WriteList(file, rt->rtArgs, WriteTypeCheck, akbSendKPD, "\n", "\n");

    WriteRequestArgs(file, rt);

    /*
     * Initialize the KPD records in the Reply structure with the 
     * templates. We do this beforehand because the call to the procedure
     * will overwrite some of the values (after the call it would be impossible
     * to initialize the KPD records from the static Templates, because we
     * would lose data).
     */
    WriteList(file, rt->rtArgs, WriteInitKPDValue, akbReturnKPD, "\n", "\n");

    WriteList(file, rt->rtArgs, WriteExtractArg, akbNone, "", "");

    if (UseEventLogger)
	WriteLogMsg(file, rt, LOG_SERVER, LOG_REQUEST);

    WriteServerCall(file, rt, WriteServerCallArg);

    WriteReverseList(file, rt->rtArgs, WriteDestroyArg, akbDestroy, "", "");

    /*
     * For one-way routines, it doesn`t make sense to check the return
     * code, because we return immediately afterwards.  However,
     * kernel servers may want to deallocate port arguments - and the
     * deallocation must not be done if the return code is not KERN_SUCCESS.
     */
    if (rt->rtOneWay || rt->rtNoReplyArgs) {
	if (IsKernelServer) {
	    if (rtCheckMaskFunction(rt->rtArgs, akbSendKPD,
				CheckDestroyPortArg)) {
		WriteCheckReturnValue(file, rt);
	    }
	    WriteReverseList(file, rt->rtArgs, WriteDestroyPortArg,
			 akbSendKPD, "", "");
	} 
	/* although we have an empty reply, we still have to make sure that
	   some fields such as NDR get properly initialized */
	if (!rt->rtOneWay)
	    WriteList(file, rt->rtArgs, WriteInitArgValue, akbReplyInit, "\n", "\n");
    } else {
	WriteCheckReturnValue(file, rt);

	if (IsKernelServer)
	    WriteReverseList(file, rt->rtArgs, WriteDestroyPortArg,
			 akbSendKPD, "", "");

	WriteReplyArgs(file, rt);
	WriteReplyInit(file, rt);
	if (!rt->rtSimpleReply)
	    fprintf(file, "\tOutP->msgh_body.msgh_descriptor_count = %d;\n",
		rt->rtReplyKPDs);
    }
    if (UseEventLogger)
	WriteLogMsg(file, rt, LOG_SERVER, LOG_REPLY);

    fprintf(file, "}\n");
}

void
WriteServer(file, stats)
    FILE *file;
    statement_t *stats;
{
    register statement_t *stat;
    register size = 0;

    WriteProlog(file);
    if (BeAnsiC)
	WriteForwardDeclarations(file, stats);
    for (stat = stats; stat != stNULL; stat = stat->stNext)
	switch (stat->stKind)
	{
	  case skRoutine:
	    WriteRoutine(file, stat->stRoutine);
	    size = max(size, stat->stRoutine->rtReplyMaxSize);
	    break;
	  case skImport:
	  case skSImport:
	    if (ServerHeaderFileName == strNULL)
		WriteImport(file, stat->stFileName);
	    break;
	  case skUImport:
	    break;
	  default:
	    fatal("WriteServer(): bad statement_kind_t (%d)",
		  (int) stat->stKind);
	}

    fprintf(file, "#ifdef\tNO_DIRECT_RPC\n");
    WriteDispatcher(file, stats, size, FALSE);
    fprintf(file, "#else\t/* NO_DIRECT_RPC */\n");
    WriteDispatcher(file, stats, size, TRUE);
    fprintf(file, "#endif\t/* NO_DIRECT_RPC */\n");
}
