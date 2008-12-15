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
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 *  ABSTRACT:
 *   Provides the routine used by parser.c to generate
 *   routine structures for each routine statement.
 *   The parser generates a threaded list of statements
 *   of which the most interesting are the various kinds
 *   routine statments. The routine structure is defined
 *   in routine.h which includes it name, kind of routine
 *   and other information,
 *   a pointer to an argument list which contains the name
 *   and type information for each argument, and a list
 *   of distinguished arguments, eg.  Request and Reply
 *   ports, waittime, retcode etc.
 */

#include <mach/message.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <global.h>
#include <routine.h>
#include <cross64.h>

u_int rtNumber = 0;

routine_t *
rtAlloc(void)
{
    register routine_t *new;

    new = (routine_t *) calloc(1, sizeof *new);
    if (new == rtNULL)
	fatal("rtAlloc(): %s", unix_error_string(errno));
    new->rtNumber = rtNumber++;
    new->rtName = strNULL;
    new->rtErrorName = strNULL;
    new->rtUserName = strNULL;
    new->rtServerName = strNULL;

    return new;
}

void
rtSkip(void)
{
    rtNumber++;
}

argument_t *
argAlloc(void)
{
    static const argument_t prototype =
    {
	strNULL,		/* identifier_t argName */
	argNULL,		/* argument_t *argNext */
	akNone,			/* arg_kind_t argKind */
	itNULL,			/* ipc_type_t *argType */
	strNULL,		/* string_t argVarName */
	strNULL,		/* string_t argMsgField */
	strNULL,		/* string_t argTTName */
	strNULL,		/* string_t argPadName */
	flNone,			/* ipc_flags_t argFlags */
	d_NO,			/* dealloc_t argDeallocate */
	FALSE,			/* boolean_t argLongForm */
	FALSE,			/* boolean_t argServerCopy */
	FALSE,			/* boolean_t argCountInOut */
	rtNULL,			/* routine_t *argRoutine */
	argNULL,		/* argument_t *argCount */
	argNULL,		/* argument_t *argCInOut */
	argNULL,		/* argument_t *argPoly */
	argNULL,		/* argument_t *argDealloc */
	argNULL,		/* argument_t *argSCopy */
	argNULL,		/* argument_t *argParent */
	1,			/* int argMultiplier */
	0,			/* int argRequestPos */
	0,			/* int argReplyPos */
	FALSE,			/* boolean_t argByReferenceUser */
	FALSE			/* boolean_t argByReferenceServer */
    };
    register argument_t *new;

    new = malloc(sizeof *new);
    if (new == argNULL)
	fatal("argAlloc(): %s", unix_error_string(errno));
    *new = prototype;
    return new;
}

routine_t *
rtMakeRoutine(identifier_t name, argument_t *args)
{
    register routine_t *rt = rtAlloc();

    rt->rtName = name;
    rt->rtKind = rkRoutine;
    rt->rtArgs = args;

    return rt;
}

routine_t *
rtMakeSimpleRoutine(identifier_t name, argument_t *args)
{
    register routine_t *rt = rtAlloc();

    rt->rtName = name;
    rt->rtKind = rkSimpleRoutine;
    rt->rtArgs = args;

    return rt;
}

routine_t *
rtMakeProcedure(identifier_t name, argument_t *args)
{
    register routine_t *rt = rtAlloc();

    rt->rtName = name;
    rt->rtKind = rkProcedure;
    rt->rtArgs = args;

    warn("Procedure %s: obsolete routine kind", name);

    return rt;
}

routine_t *
rtMakeSimpleProcedure(identifier_t name, argument_t *args)
{
    register routine_t *rt = rtAlloc();

    rt->rtName = name;
    rt->rtKind = rkSimpleProcedure;
    rt->rtArgs = args;

    warn("SimpleProcedure %s: obsolete routine kind", name);

    return rt;
}

routine_t *
rtMakeFunction(identifier_t name, argument_t *args, ipc_type_t *type)
{
    register routine_t *rt = rtAlloc();
    register argument_t *ret = argAlloc();

    ret->argName = name;
    ret->argKind = akReturn;
    ret->argType = type;
    ret->argNext = args;

    rt->rtName = name;
    rt->rtKind = rkFunction;
    rt->rtArgs = ret;

    warn("Function %s: obsolete routine kind", name);

    return rt;
}

const char *
rtRoutineKindToStr(routine_kind_t rk)
{
    switch (rk)
    {
      case rkRoutine:
	return "Routine";
      case rkSimpleRoutine:
	return "SimpleRoutine";
      case rkProcedure:
	return "Procedure";
      case rkSimpleProcedure:
	return "SimpleProcedure";
      case rkFunction:
	return "Function";
      default:
	fatal("rtRoutineKindToStr(%d): not a routine_kind_t", rk);
	/*NOTREACHED*/
    }
}

static void
rtPrintArg(register const argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    if (!akCheck(arg->argKind, akbUserArg|akbServerArg) ||
	(akIdent(arg->argKind) == akeCount) ||
	(akIdent(arg->argKind) == akePoly))
	return;

    printf("\n\t");

    switch (akIdent(arg->argKind))
    {
      case akeRequestPort:
	printf("RequestPort");
	break;
      case akeReplyPort:
	printf("ReplyPort");
	break;
      case akeWaitTime:
	printf("WaitTime");
	break;
      case akeMsgOption:
	printf("MsgOption");
	break;
      case akeMsgSeqno:
	printf("MsgSeqno\t");
	break;
      default:
	if (akCheck(arg->argKind, akbRequest))
	    if (akCheck(arg->argKind, akbSend))
		printf("In");
	    else
		printf("(In)");
	if (akCheck(arg->argKind, akbReply))
	    if (akCheck(arg->argKind, akbReturn))
		printf("Out");
	    else
		printf("(Out)");
	printf("\t");
    }

    printf("\t%s: %s", arg->argName, it->itName);

    if (arg->argDeallocate != it->itDeallocate)
	if (arg->argDeallocate == d_YES)
	    printf(", Dealloc");
	else if (arg->argDeallocate == d_MAYBE)
	    printf(", Dealloc[]");
	else
	    printf(", NotDealloc");

    if (arg->argLongForm != it->itLongForm)
	if (arg->argLongForm)
	    printf(", IsLong");
	else
	    printf(", IsNotLong");

    if (arg->argServerCopy)
	printf(", ServerCopy");

    if (arg->argCountInOut)
	printf(", CountInOut");
}

void
rtPrintRoutine(register const routine_t *rt)
{
    register const argument_t *arg;

    printf("%s (%d) %s(", rtRoutineKindToStr(rt->rtKind),
	   rt->rtNumber, rt->rtName);

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
	rtPrintArg(arg);

    if (rt->rtKind == rkFunction)
	printf("): %s\n", rt->rtReturn->argType->itName);
    else
	printf(")\n");

    printf("\n");
}

/*
 * Determines appropriate value of msg-simple for the message,
 * and whether this value can vary at runtime.  (If it can vary,
 * then the simple value is optimistically returned as TRUE.)
 * Uses itInName values, so useful when sending messages.
 */

static void
rtCheckSimpleIn(const argument_t *args, u_int mask, boolean_t *fixed,
		boolean_t *simple)
{
    register const argument_t *arg;
    boolean_t MayBeComplex = FALSE;
    boolean_t MustBeComplex = FALSE;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheck(arg->argKind, mask))
	{
	    register const ipc_type_t *it = arg->argType;

	    if (it->itInName == MACH_MSG_TYPE_POLYMORPHIC)
		MayBeComplex = TRUE;

	    if (it->itIndefinite)
		MayBeComplex = TRUE;

	    if (MACH_MSG_TYPE_PORT_ANY(it->itInName) ||
		!it->itInLine)
		MustBeComplex = TRUE;
	}

    *fixed = MustBeComplex || !MayBeComplex;
    *simple = !MustBeComplex;
}

/*
 * Determines appropriate value of msg-simple for the message,
 * and whether this value can vary at runtime.  (If it can vary,
 * then the simple value is optimistically returned as TRUE.)
 * Uses itOutName values, so useful when receiving messages
 * (and sending reply messages in KernelServer interfaces).
 */

static void
rtCheckSimpleOut(const argument_t *args, u_int mask, boolean_t *fixed,
		 boolean_t *simple)
{
    register const argument_t *arg;
    boolean_t MayBeComplex = FALSE;
    boolean_t MustBeComplex = FALSE;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheck(arg->argKind, mask))
	{
	    register const ipc_type_t *it = arg->argType;

	    if (it->itOutName == MACH_MSG_TYPE_POLYMORPHIC)
		MayBeComplex = TRUE;

	    if (it->itIndefinite)
		MayBeComplex = TRUE;

	    if (MACH_MSG_TYPE_PORT_ANY(it->itOutName) ||
		!it->itInLine)
		MustBeComplex = TRUE;
	}

    *fixed = MustBeComplex || !MayBeComplex;
    *simple = !MustBeComplex;
}

static u_int
rtFindSize(const argument_t *args, u_int mask)
{
    register const argument_t *arg;
    u_int size = sizeof_mach_msg_header_t;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheck(arg->argKind, mask))
	{
	    register ipc_type_t *it = arg->argType;

	    if (arg->argLongForm) {
		/* might need proper alignment on 64bit archies */
		size = (size + word_size-1) & ~(word_size-1);
		size += sizeof_mach_msg_type_long_t;
	    } else {
		register bs = (it->itSize / 8); /* in bytes */
		size += (bs > sizeof_mach_msg_type_t) ? bs : sizeof_mach_msg_type_t;
	    }

	    size += it->itMinTypeSize;
	}

    return size;
}

boolean_t
rtCheckMask(const argument_t *args, u_int mask)
{
    register const argument_t *arg;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheckAll(arg->argKind, mask))
	    return TRUE;
    return FALSE;
}

boolean_t
rtCheckMaskFunction(const argument_t *args, u_int mask,
		    boolean_t (*func)(const argument_t *))
{
    register const argument_t *arg;

    for (arg = args; arg != argNULL; arg = arg->argNext)
	if (akCheckAll(arg->argKind, mask))
	    if ((*func)(arg))
		return TRUE;
    return FALSE;
}

/* arg->argType may be NULL in this function */

static void
rtDefaultArgKind(const routine_t *rt, argument_t *arg)
{
    if ((arg->argKind == akNone) &&
	(rt->rtRequestPort == argNULL))
	arg->argKind = akRequestPort;

    if (arg->argKind == akNone)
	arg->argKind = akIn;
}

/*
 * Initializes arg->argDeallocate, arg->argLongForm,
 * arg->argServerCopy, arg->argCountInOut from arg->argFlags.
 */

static void
rtProcessArgFlags(register argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;

    arg->argFlags = itCheckFlags(arg->argFlags, arg->argName);

    if (((IsKernelServer && akCheck(arg->argKind, akbReturn)) ||
	 (IsKernelUser && akCheck(arg->argKind, akbSend))) &&
	(arg->argFlags & flDealloc) &&
	(it->itDeallocate == d_NO)) {
	/*
	 *	For a KernelServer interface and an Out argument,
	 *	or a KernelUser interface and an In argument,
	 *	we avoid a possible spurious warning about the deallocate bit.
	 *	For compatibility with Mach 2.5, the deallocate bit
	 *	may need to be enabled on some inline arguments.
	 */

	arg->argDeallocate = d_YES;
    } else
	arg->argDeallocate = itCheckDeallocate(it, arg->argFlags,
					       it->itDeallocate, arg->argName);

    arg->argLongForm = itCheckIsLong(it, arg->argFlags,
				     it->itLongForm, arg->argName);

    if (arg->argFlags & flServerCopy) {
	if (it->itIndefinite && akCheck(arg->argKind, akbSend))
	    arg->argServerCopy = TRUE;
	else
	    warn("%s: ServerCopy on argument is meaningless", arg->argName);
    }

    if (arg->argFlags & flCountInOut) {
	if (it->itVarArray && it->itInLine &&
	    akCheck(arg->argKind, akbReply))
	    arg->argCountInOut = TRUE;
	else
	    warn("%s: CountInOut on argument is meaningless", arg->argName);
    }
}

static void
rtAugmentArgKind(argument_t *arg)
{
    register ipc_type_t *it = arg->argType;

    /* akbVariable means variable-sized inline. */

    if (it->itVarArray && it->itInLine)
    {
	if (akCheckAll(arg->argKind, akbRequest|akbReply))
	    error("%s: Inline variable-sized arguments can't be InOut",
		  arg->argName);
	arg->argKind = akAddFeature(arg->argKind, akbVariable);

	/* akbIndefinite means inline or out-of-line */

	if (it->itIndefinite)
	    arg->argKind = akAddFeature(arg->argKind, akbIndefinite);
    }

    /*
     *	Kernel servers can't do quick-checking of request arguments
     *	which are out-of-line or ports, because the deallocate bit isn't
     *	predictable.  This is because the deallocate bit is preserved
     *	at message copyin time and normalized during message copyout.
     *	This accomodates old IPC programs which expect the deallocate
     *	bit to be preserved.
     */

    if (akCheck(arg->argKind, akbRequest) &&
	!arg->argLongForm &&
	(it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	!it->itVarArray &&
	!(IsKernelServer && (!it->itInLine ||
			     MACH_MSG_TYPE_PORT_ANY(it->itOutName))))
	arg->argKind = akAddFeature(arg->argKind, akbRequestQC);

    if (akCheck(arg->argKind, akbReply) &&
	!arg->argLongForm &&
	(it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	!it->itVarArray)
	arg->argKind = akAddFeature(arg->argKind, akbReplyQC);

    /*
     * Need to use a local variable in the following cases:
     *	1) There is a translate-out function & the argument is being
     *	   returned.  We need to translate it before it hits the message.
     *	2) There is a translate-in function & the argument is
     *	   sent and returned.  We need a local variable for its address.
     *	3) There is a destructor function, which will be used
     *	   (SendRcv and not ReturnSnd), and there is a translate-in
     *	   function whose value must be saved for the destructor.
     *	4) This is a count arg, getting returned.  The count can't get
     *	   stored directly into the msg-type, because the msg-type won't
     *	   get initialized until later, and that would trash the count.
     *	5) This is a poly arg, getting returned.  The name can't get
     *	   stored directly into the msg-type, because the msg-type won't
     *	   get initialized until later, and that would trash the name.
     *  6) This is a dealloc arg, being returned.  The name can't be
     *	   stored directly into the msg_type, because the msg-type
     *	   field is a bit-field.
     */

    if (((it->itOutTrans != strNULL) &&
	 akCheck(arg->argKind, akbReturnSnd)) ||
	((it->itInTrans != strNULL) &&
	 akCheckAll(arg->argKind, akbSendRcv|akbReturnSnd)) ||
	((it->itDestructor != strNULL) &&
	 akCheck(arg->argKind, akbSendRcv) &&
	 !akCheck(arg->argKind, akbReturnSnd) &&
	 (it->itInTrans != strNULL)) ||
	((akIdent(arg->argKind) == akeCount) &&
	 akCheck(arg->argKind, akbReturnSnd)) ||
	((akIdent(arg->argKind) == akePoly) &&
	 akCheck(arg->argKind, akbReturnSnd)) ||
	((akIdent(arg->argKind) == akeDealloc) &&
	 akCheck(arg->argKind, akbReturnSnd)))
    {
	arg->argKind = akRemFeature(arg->argKind, akbReplyCopy);
	arg->argKind = akAddFeature(arg->argKind, akbVarNeeded);
    }

    /*
     * If the argument is a variable-length array that can be passed in-line
     * or out-of-line, and is being returned, the server procedure
     * is passed a pointer to the buffer, which it can change.
     */
    if (it->itIndefinite &&
	akCheck(arg->argKind, akbReturnSnd))
    {
	arg->argKind = akAddFeature(arg->argKind, akbPointer);
    }
}

/* arg->argType may be NULL in this function */

static void
rtCheckRoutineArg(routine_t *rt, argument_t *arg)
{
    switch (akIdent(arg->argKind))
    {
      case akeRequestPort:
	if (rt->rtRequestPort != argNULL)
	    warn("multiple RequestPort args in %s; %s won't be used",
		 rt->rtName, rt->rtRequestPort->argName);
	rt->rtRequestPort = arg;
	break;

      case akeReplyPort:
	if (rt->rtReplyPort != argNULL)
	    warn("multiple ReplyPort args in %s; %s won't be used",
		 rt->rtName, rt->rtReplyPort->argName);
	rt->rtReplyPort = arg;
	break;

      case akeWaitTime:
	if (rt->rtWaitTime != argNULL)
	    warn("multiple WaitTime args in %s; %s won't be used",
		 rt->rtName, rt->rtWaitTime->argName);
	rt->rtWaitTime = arg;
	break;

      case akeMsgOption:
	if (rt->rtMsgOption != argNULL)
	    warn("multiple MsgOption args in %s; %s won't be used",
		 rt->rtName, rt->rtMsgOption->argName);
	rt->rtMsgOption = arg;
	break;

      case akeMsgSeqno:
	if (rt->rtMsgSeqno != argNULL)
	    warn("multiple MsgSeqno args in %s; %s won't be used",
		 rt->rtName, rt->rtMsgSeqno->argName);
	rt->rtMsgSeqno = arg;
	break;

      case akeReturn:
	if (rt->rtReturn != argNULL)
	    warn("multiple Return args in %s; %s won't be used",
		 rt->rtName, rt->rtReturn->argName);
	rt->rtReturn = arg;
	break;

      default:
	break;
    }
}

/* arg->argType may be NULL in this function */

static void
rtSetArgDefaults(routine_t *rt, register argument_t *arg)
{
    arg->argRoutine = rt;
    if (arg->argVarName == strNULL)
	arg->argVarName = arg->argName;
    if (arg->argMsgField == strNULL)
	switch(akIdent(arg->argKind))
	{
	  case akeRequestPort:
	    arg->argMsgField = "Head.msgh_request_port";
	    break;
	  case akeReplyPort:
	    arg->argMsgField = "Head.msgh_reply_port";
	    break;
	  case akeMsgSeqno:
	    arg->argMsgField = "Head.msgh_seqno";
	    break;
	  default:
	    arg->argMsgField = arg->argName;
	    break;
	}
    if (arg->argTTName == strNULL)
	arg->argTTName = strconcat(arg->argName, "Type");
    if (arg->argPadName == strNULL)
	arg->argPadName = strconcat(arg->argName, "Pad");

    /*
     *	The poly args for the request and reply ports have special defaults,
     *	because their msg-type-name values aren't stored in normal fields.
     */

    if ((rt->rtRequestPort != argNULL) &&
	(rt->rtRequestPort->argPoly == arg) &&
	(arg->argType != itNULL)) {
	arg->argMsgField = "Head.msgh_bits";
	arg->argType->itInTrans = "MACH_MSGH_BITS_REQUEST";
    }

    if ((rt->rtReplyPort != argNULL) &&
	(rt->rtReplyPort->argPoly == arg) &&
	(arg->argType != itNULL)) {
	arg->argMsgField = "Head.msgh_bits";
	arg->argType->itInTrans = "MACH_MSGH_BITS_REPLY";
    }
}

static void
rtAddCountArg(register argument_t *arg)
{
    register argument_t *count;

    count = argAlloc();
    count->argName = strconcat(arg->argName, "Cnt");
    count->argType = itMakeCountType();
    count->argParent = arg;
    count->argMultiplier = arg->argType->itElement->itNumber;
    count->argNext = arg->argNext;
    arg->argNext = count;
    arg->argCount = count;

    if (arg->argType->itString) {
	/* C String gets no Count argument on either side.
	   There is no explicit field in the message -
	   the count is passed as part of the descriptor. */
	count->argKind = akeCount;
	count->argVarName = (char *)0;
    } else
	count->argKind = akAddFeature(akCount,
				  akCheck(arg->argKind, akbSendReturnBits));

    if (arg->argLongForm)
	count->argMsgField = strconcat(arg->argTTName,
				       ".msgtl_number");
    else
	count->argMsgField = strconcat(arg->argTTName, ".msgt_number");
}

static void
rtAddCountInOutArg(register argument_t *arg)
{
    register argument_t *count;

    /*
     *	The user sees a single count variable.  However, to get the
     *	count passed from user to server for variable-sized inline OUT
     *	arrays, we need two count arguments internally.  This is
     *	because the count value lives in different message fields (and
     *	is scaled differently) in the request and reply messages.
     *
     *	The two variables have the same name to simplify code generation.
     *
     *	This variable has a null argParent field because it has akbRequest.
     *	For example, see rtCheckVariable.
     */

    count = argAlloc();
    count->argName = strconcat(arg->argName, "Cnt");
    count->argType = itMakeCountType();
    count->argParent = argNULL;
    count->argNext = arg->argNext;
    arg->argNext = count;
    (count->argCInOut = arg->argCount)->argCInOut = count;
    count->argKind = akCountInOut;
}

static void
rtAddPolyArg(register argument_t *arg)
{
    register const ipc_type_t *it = arg->argType;
    register argument_t *poly;
    arg_kind_t akbsend, akbreturn;

    poly = argAlloc();
    poly->argName = strconcat(arg->argName, "Poly");
    poly->argType = itMakePolyType();
    poly->argParent = arg;
    poly->argNext = arg->argNext;
    arg->argNext = poly;
    arg->argPoly = poly;

    /*
     * akbsend is bits added if the arg is In;
     * akbreturn is bits added if the arg is Out.
     * The mysterious business with KernelServer subsystems:
     * when packing Out arguments, they use OutNames instead
     * of InNames, and the OutName determines if they are poly-in
     * as well as poly-out.
     */

    akbsend = akbSend|akbSendBody;
    akbreturn = akbReturn|akbReturnBody;

    if (it->itInName == MACH_MSG_TYPE_POLYMORPHIC)
    {
	akbsend |= akbUserArg|akbSendSnd;
	if (!IsKernelServer)
	    akbreturn |= akbServerArg|akbReturnSnd;
    }
    if (it->itOutName == MACH_MSG_TYPE_POLYMORPHIC)
    {
	akbsend |= akbServerArg|akbSendRcv;
	akbreturn |= akbUserArg|akbReturnRcv;
	if (IsKernelServer)
	    akbreturn |= akbServerArg|akbReturnSnd;
    }

    poly->argKind = akPoly;
    if (akCheck(arg->argKind, akbSend))
	poly->argKind = akAddFeature(poly->argKind,
				     akCheck(arg->argKind, akbsend));
    if (akCheck(arg->argKind, akbReturn))
	poly->argKind = akAddFeature(poly->argKind,
				     akCheck(arg->argKind, akbreturn));

    if (arg->argLongForm)
	poly->argMsgField = strconcat(arg->argTTName,
				      ".msgtl_name");
    else
	poly->argMsgField = strconcat(arg->argTTName, ".msgt_name");
}

static void
rtAddDeallocArg(register argument_t *arg)
{
    register argument_t *dealloc;

    dealloc = argAlloc();
    dealloc->argName = strconcat(arg->argName, "Dealloc");
    dealloc->argType = itMakeDeallocType();
    dealloc->argParent = arg;
    dealloc->argNext = arg->argNext;
    arg->argNext = dealloc;
    arg->argDealloc = dealloc;

    /*
     *	For Indefinite types, we leave out akbSendSnd and akbReturnSnd
     *	so that the normal argument-packing is bypassed.  The special code
     *	generated for the Indefinite argument handles the deallocate bit.
     *	(It can only be enabled if the data is actually out-of-line.)
     */

    dealloc->argKind = akeDealloc;
    if (akCheck(arg->argKind, akbSend))
	dealloc->argKind = akAddFeature(dealloc->argKind,
		akCheck(arg->argKind,
			akbUserArg|akbSend|akbSendBody|
			(arg->argType->itIndefinite ? 0 : akbSendSnd)));
    if (akCheck(arg->argKind, akbReturn)) {
	dealloc->argKind = akAddFeature(dealloc->argKind,
		akCheck(arg->argKind,
			akbServerArg|akbReturn|akbReturnBody|
			(arg->argType->itIndefinite ? 0 : akbReturnSnd)));

	/*
	 *  Without akbReturnSnd, rtAugmentArgKind will not add
	 *  akbVarNeeded and rtAddByReference will not set
	 *  argByReferenceServer.  So we do it here.
	 */

	if (arg->argType->itIndefinite) {
	    dealloc->argKind = akAddFeature(dealloc->argKind, akbVarNeeded);
	    dealloc->argByReferenceServer = TRUE;
	}
    }

    if (arg->argLongForm)
	dealloc->argMsgField = strconcat(arg->argTTName,
					 ".msgtl_header.msgt_deallocate");
    else
	dealloc->argMsgField = strconcat(arg->argTTName, ".msgt_deallocate");

}

static void
rtAddSCopyArg(register argument_t *arg)
{
    register argument_t *scopy;

    scopy = argAlloc();
    scopy->argName = strconcat(arg->argName, "SCopy");
    scopy->argType = itMakeDeallocType();
    scopy->argParent = arg;
    scopy->argNext = arg->argNext;
    arg->argNext = scopy;
    arg->argSCopy = scopy;

    scopy->argKind = akServerCopy;

    if (arg->argLongForm)
	scopy->argMsgField = strconcat(arg->argTTName,
					".msgtl_header.msgt_inline");
    else
	scopy->argMsgField = strconcat(arg->argTTName, ".msgt_inline");
}

static void
rtCheckRoutineArgs(routine_t *rt)
{
    register argument_t *arg;

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
    {
	register const ipc_type_t *it = arg->argType;

	rtDefaultArgKind(rt, arg);
	rtCheckRoutineArg(rt, arg);

	/* need to set argTTName before adding implicit args */
	rtSetArgDefaults(rt, arg);

	/* the arg may not have a type (if there was some error in parsing it),
	   in which case we don't want to do these steps. */

	if (it != itNULL)
	{
	    /* need to set argLongForm before adding implicit args */
	    rtProcessArgFlags(arg);
	    rtAugmentArgKind(arg);

	    /* args added here will get processed in later iterations */
	    /* order of args is 'arg poly countinout count dealloc scopy' */

	    if (arg->argServerCopy)
		rtAddSCopyArg(arg);
	    if (arg->argDeallocate == d_MAYBE)
		rtAddDeallocArg(arg);
	    if (it->itVarArray)
		rtAddCountArg(arg);
	    if (arg->argCountInOut)
		rtAddCountInOutArg(arg);
	    if ((it->itInName == MACH_MSG_TYPE_POLYMORPHIC) ||
		(it->itOutName == MACH_MSG_TYPE_POLYMORPHIC))
		rtAddPolyArg(arg);
	}
    }
}

static void
rtCheckArgTypes(routine_t *rt)
{
    if (rt->rtRequestPort == argNULL)
	error("%s %s doesn't have a server port argument",
	      rtRoutineKindToStr(rt->rtKind), rt->rtName);

    if ((rt->rtKind == rkFunction) &&
	(rt->rtReturn == argNULL))
	error("Function %s doesn't have a return arg", rt->rtName);

    if ((rt->rtKind != rkFunction) &&
	(rt->rtReturn != argNULL))
	error("non-function %s has a return arg", rt->rtName);

    if ((rt->rtReturn == argNULL) && !rt->rtProcedure)
	rt->rtReturn = rt->rtRetCode;

    rt->rtServerReturn = rt->rtReturn;

    if ((rt->rtReturn != argNULL) &&
	(rt->rtReturn->argType != itNULL))
	itCheckReturnType(rt->rtReturn->argName,
			  rt->rtReturn->argType);

    if ((rt->rtRequestPort != argNULL) &&
	(rt->rtRequestPort->argType != itNULL))
	itCheckRequestPortType(rt->rtRequestPort->argName,
			       rt->rtRequestPort->argType);

    if ((rt->rtReplyPort != argNULL) &&
	(rt->rtReplyPort->argType != itNULL))
	itCheckReplyPortType(rt->rtReplyPort->argName,
			     rt->rtReplyPort->argType);

    if ((rt->rtWaitTime != argNULL) &&
	(rt->rtWaitTime->argType != itNULL))
	itCheckIntType(rt->rtWaitTime->argName,
		       rt->rtWaitTime->argType);

    if ((rt->rtMsgOption != argNULL) &&
	(rt->rtMsgOption->argType != itNULL))
	itCheckIntType(rt->rtMsgOption->argName,
		       rt->rtMsgOption->argType);

    if ((rt->rtMsgSeqno != argNULL) &&
	(rt->rtMsgSeqno->argType != itNULL))
	itCheckNaturalType(rt->rtMsgSeqno->argName,
		       rt->rtMsgSeqno->argType);
}

/*
 * Check for arguments which are missing seemingly needed functions.
 * We make this check here instead of in itCheckDecl, because here
 * we can take into account what kind of argument the type is
 * being used with.
 *
 * These are warnings, not hard errors, because mig will generate
 * reasonable code in any case.  The generated code will work fine
 * if the ServerType and TransType are really the same, even though
 * they have different names.
 */

static void
rtCheckArgTrans(const routine_t *rt)
{
    register const argument_t *arg;

    /* the arg may not have a type (if there was some error in parsing it) */

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext)
    {
	register const ipc_type_t *it = arg->argType;

	if ((it != itNULL) &&
	    !streql(it->itServerType, it->itTransType))
	{
	    if (akCheck(arg->argKind, akbSendRcv) &&
		(it->itInTrans == strNULL))
		warn("%s: argument has no in-translation function",
		     arg->argName);

	    if (akCheck(arg->argKind, akbReturnSnd) &&
		(it->itOutTrans == strNULL))
		warn("%s: argument has no out-translation function",
		     arg->argName);
	}
    }
}

/*
 * Adds an implicit return-code argument.  It exists in the reply message,
 * where it is the first piece of data.  Even if there is no reply
 * message (rtOneWay is true), we generate the argument because
 * the server-side stub needs a dummy reply msg to return error codes
 * back to the server loop.
 */

static void
rtAddRetCode(routine_t *rt)
{
    register argument_t *arg = argAlloc();

    arg->argName = "RetCode";
    arg->argType = itRetCodeType;
    arg->argKind = akRetCode;
    rt->rtRetCode = arg;

    /* add at beginning, so return-code is first in the reply message  */
    arg->argNext = rt->rtArgs;
    rt->rtArgs = arg;
}

/*
 *  Adds a dummy WaitTime argument to the function.
 *  This argument doesn't show up in any C argument lists;
 *  it implements the global WaitTime statement.
 */

static void
rtAddWaitTime(routine_t *rt, identifier_t name)
{
    register argument_t *arg = argAlloc();
    argument_t **loc;

    arg->argName = "dummy WaitTime arg";
    arg->argVarName = name;
    arg->argType = itWaitTimeType;
    arg->argKind = akeWaitTime;
    rt->rtWaitTime = arg;

    /* add wait-time after msg-option, if possible */

    if (rt->rtMsgOption != argNULL)
	loc = &rt->rtMsgOption->argNext;
    else
	loc = &rt->rtArgs;

    arg->argNext = *loc;
    *loc = arg;

    rtSetArgDefaults(rt, arg);
}

/*
 *  Adds a dummy MsgOption argument to the function.
 *  This argument doesn't show up in any C argument lists;
 *  it implements the global MsgOption statement.
 */

static void
rtAddMsgOption(routine_t *rt, identifier_t name)
{
    register argument_t *arg = argAlloc();
    argument_t **loc;

    arg->argName = "dummy MsgOption arg";
    arg->argVarName = name;
    arg->argType = itMsgOptionType;
    arg->argKind = akeMsgOption;
    rt->rtMsgOption = arg;

    /* add msg-option after msg-seqno */

    if (rt->rtMsgSeqno != argNULL)
	loc = &rt->rtMsgSeqno->argNext;
    else
	loc = &rt->rtArgs;

    arg->argNext = *loc;
    *loc = arg;

    rtSetArgDefaults(rt, arg);
}

/*
 *  Adds a dummy reply port argument to the function.
 */

static void
rtAddDummyReplyPort(routine_t *rt, ipc_type_t *type)
{
    register argument_t *arg = argAlloc();
    argument_t **loc;

    arg->argName = "dummy ReplyPort arg";
    arg->argVarName = "dummy ReplyPort arg";
    arg->argType = type;
    arg->argKind = akeReplyPort;
    rt->rtReplyPort = arg;

    /* add the reply port after the request port */

    if (rt->rtRequestPort != argNULL)
	loc = &rt->rtRequestPort->argNext;
    else
	loc = &rt->rtArgs;

    arg->argNext = *loc;
    *loc = arg;

    rtSetArgDefaults(rt, arg);
}

/*
 * Initializes argRequestPos, argReplyPos, rtMaxRequestPos, rtMaxReplyPos,
 * rtNumRequestVar, rtNumReplyVar, and adds akbVarNeeded to those arguments
 * that need it because of variable-sized inline considerations.
 *
 * argRequestPos and argReplyPos get -1 if the value shouldn't be used.
 */
static void
rtCheckVariable(register routine_t *rt)
{
    register argument_t *arg;
    int NumRequestVar = 0;
    int NumReplyVar = 0;
    int MaxRequestPos = 0;
    int MaxReplyPos = 0;

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {
	register argument_t *parent = arg->argParent;

	if (parent == argNULL) {
	    if (akCheck(arg->argKind, akbRequest|akbSend)) {
		arg->argRequestPos = NumRequestVar;
		MaxRequestPos = NumRequestVar;
		if (akCheck(arg->argKind, akbVariable))
		    NumRequestVar++;
	    } else
		arg->argRequestPos = -1;

	    if (akCheck(arg->argKind, akbReply|akbReturn)) {
		arg->argReplyPos = NumReplyVar;
		MaxReplyPos = NumReplyVar;
		if (akCheck(arg->argKind, akbVariable))
		    NumReplyVar++;
	    } else
		arg->argReplyPos = -1;
	} else {
	    arg->argRequestPos = parent->argRequestPos;
	    arg->argReplyPos = parent->argReplyPos;
	}

	/* Out variables that follow a variable-sized field
	   need VarNeeded or ReplyCopy; they can't be stored
	   directly into the reply message. */

	if (akCheck(arg->argKind, akbReturnSnd) &&
	    !akCheck(arg->argKind, akbReplyCopy|akbVarNeeded) &&
	    (arg->argReplyPos > 0))
	    arg->argKind = akAddFeature(arg->argKind, akbVarNeeded);
    }

    rt->rtNumRequestVar = NumRequestVar;
    rt->rtNumReplyVar = NumReplyVar;
    rt->rtMaxRequestPos = MaxRequestPos;
    rt->rtMaxReplyPos = MaxReplyPos;
}

/*
 * Adds akbDestroy where needed.
 */

static void
rtCheckDestroy(register routine_t *rt)
{
    register argument_t *arg;

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {
	register const ipc_type_t *it = arg->argType;

	if(akCheck(arg->argKind, akbSendRcv) &&
	   !akCheck(arg->argKind, akbReturnSnd)) {
	   if ((it->itDestructor != strNULL) ||
	       (akCheck(arg->argKind, akbIndefinite) && !arg->argServerCopy))
		arg->argKind = akAddFeature(arg->argKind, akbDestroy);
	}
    }
}

/*
 * Sets ByReferenceUser and ByReferenceServer.
 */

static void
rtAddByReference(register routine_t *rt)
{
    register argument_t *arg;

    for (arg = rt->rtArgs; arg != argNULL; arg = arg->argNext) {
	register const ipc_type_t *it = arg->argType;

	if (akCheck(arg->argKind, akbReturnRcv) &&
	    (it->itStruct || it->itIndefinite)) {
	    arg->argByReferenceUser = TRUE;

	    /*
	     *	A CountInOut arg itself is not akbReturnRcv,
	     *	so we need to set argByReferenceUser specially.
	     */

	    if (arg->argCInOut != argNULL)
		arg->argCInOut->argByReferenceUser = TRUE;
	}

	if (akCheck(arg->argKind, akbReturnSnd) &&
	    (it->itStruct || it->itIndefinite))
	    arg->argByReferenceServer = TRUE;
    }
}

void
rtCheckRoutine(register routine_t *rt)
{
    /* Initialize random fields. */

    rt->rtErrorName = ErrorProc;
    rt->rtOneWay = ((rt->rtKind == rkSimpleProcedure) ||
		    (rt->rtKind == rkSimpleRoutine));
    rt->rtProcedure = ((rt->rtKind == rkProcedure) ||
		       (rt->rtKind == rkSimpleProcedure));
    rt->rtUseError = rt->rtProcedure || (rt->rtKind == rkFunction);
    rt->rtServerName = strconcat(ServerPrefix, rt->rtName);
    rt->rtUserName = strconcat(UserPrefix, rt->rtName);

    /* Add implicit arguments. */

    rtAddRetCode(rt);

    /* Check out the arguments and their types.  Add count, poly
       implicit args.  Any arguments added after rtCheckRoutineArgs
       should have rtSetArgDefaults called on them. */

    rtCheckRoutineArgs(rt);

    /* Add dummy WaitTime and MsgOption arguments, if the routine
       doesn't have its own args and the user specified global values. */

    if (rt->rtReplyPort == argNULL)
	if (rt->rtOneWay)
	    rtAddDummyReplyPort(rt, itZeroReplyPortType);
	else
	    rtAddDummyReplyPort(rt, itRealReplyPortType);

    if (rt->rtMsgOption == argNULL)
	if (MsgOption == strNULL)
	    rtAddMsgOption(rt, "MACH_MSG_OPTION_NONE");
	else
	    rtAddMsgOption(rt, MsgOption);

    if ((rt->rtWaitTime == argNULL) &&
	(WaitTime != strNULL))
	rtAddWaitTime(rt, WaitTime);

    /* Now that all the arguments are in place, do more checking. */

    rtCheckArgTypes(rt);
    rtCheckArgTrans(rt);

    if (rt->rtOneWay && rtCheckMask(rt->rtArgs, akbReturn))
	error("%s %s has OUT argument",
	      rtRoutineKindToStr(rt->rtKind), rt->rtName);

    /* If there were any errors, don't bother calculating more info
       that is only used in code generation anyway.  Therefore,
       the following functions don't have to worry about null types. */

    if (errors > 0)
	return;

    rtCheckSimpleIn(rt->rtArgs, akbRequest,
		    &rt->rtSimpleFixedRequest,
		    &rt->rtSimpleSendRequest);
    rtCheckSimpleOut(rt->rtArgs, akbRequest,
		     &rt->rtSimpleCheckRequest,
		     &rt->rtSimpleReceiveRequest);
    rt->rtRequestSize = rtFindSize(rt->rtArgs, akbRequest);

    if (IsKernelServer)
	rtCheckSimpleOut(rt->rtArgs, akbReply,
			 &rt->rtSimpleFixedReply,
			 &rt->rtSimpleSendReply);
    else
	rtCheckSimpleIn(rt->rtArgs, akbReply,
			&rt->rtSimpleFixedReply,
			&rt->rtSimpleSendReply);
    rtCheckSimpleOut(rt->rtArgs, akbReply,
		     &rt->rtSimpleCheckReply,
		     &rt->rtSimpleReceiveReply);
    rt->rtReplySize = rtFindSize(rt->rtArgs, akbReply);

    rtCheckVariable(rt);
    rtCheckDestroy(rt);
    rtAddByReference(rt);

    if (rt->rtKind == rkFunction)
	rt->rtNoReplyArgs = FALSE;
    else
	rt->rtNoReplyArgs = !rtCheckMask(rt->rtArgs, akbReturnSnd);
}
