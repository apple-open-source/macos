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

#ifndef	_ROUTINE_H
#define	_ROUTINE_H

#define	EXPORT_BOOLEAN
#include <mach/boolean.h>
#include <sys/types.h>
#include <type.h>

/* base kind arg */
#define akeNone		(0)
#define akeNormal	(1)	/* a normal, user-defined argument */
#define akeRequestPort	(2)	/* pointed at by rtRequestPort */
#define akeWaitTime	(3)	/* pointed at by rtWaitTime */
#define akeReplyPort	(4)	/* pointed at by rtReplyPort */
#define akeMsgOption	(5)	/* pointed at by rtMsgOption */
#define akeMsgSeqno	(6)	/* pointed at by rtMsgSeqno */
#define akeRetCode	(7)	/* pointed at by rtRetCode/rtReturn */
#define akeReturn	(8)	/* pointed at by rtReturn */
#define akeCount	(9)	/* a count arg for argParent */
#define akePoly		(10)	/* a poly arg for argParent */
#define	akeDealloc	(11)	/* a deallocate arg for argParent */
#define	akeServerCopy	(12)	/* a server-copy arg for argParent */
#define akeCountInOut	(13)	/* a count-in-out arg */

#define	akeBITS		(0x0000003f)
#define	akbRequest	(0x00000040)	/* has a msg_type in request */
#define	akbReply	(0x00000080)	/* has a msg_type in reply */
#define	akbUserArg	(0x00000100)	/* an arg on user-side */
#define	akbServerArg	(0x00000200)	/* an arg on server-side  */
#define akbSend		(0x00000400)	/* value carried in request */
#define akbSendBody	(0x00000800)	/* value carried in request body */
#define akbSendSnd	(0x00001000)	/* value stuffed into request */
#define akbSendRcv	(0x00002000)	/* value grabbed from request */
#define akbReturn	(0x00004000)	/* value carried in reply */
#define akbReturnBody	(0x00008000)	/* value carried in reply body */
#define akbReturnSnd	(0x00010000)	/* value stuffed into reply */
#define akbReturnRcv	(0x00020000)	/* value grabbed from reply */
#define akbReplyInit	(0x00040000)	/* reply msg-type must be init'ed */
#define akbRequestQC	(0x00080000)	/* msg_type can be checked quickly */
#define akbReplyQC	(0x00100000)	/* msg_type can be checked quickly */
#define akbReplyCopy	(0x00200000)	/* copy reply value from request */
#define akbVarNeeded	(0x00400000)	/* may need local var in server */
#define akbDestroy	(0x00800000)	/* call destructor function */
#define akbVariable	(0x01000000)	/* variable size inline data */
#define	akbIndefinite	(0x02000000)	/* variable size, inline or out */
#define	akbPointer	(0x04000000)	/* server gets a pointer to the
					   real buffer */
/* be careful, there aren't many bits left */

typedef u_int  arg_kind_t;

/*
 * akbRequest means msg_type/data fields are allocated in the request
 * msg.  akbReply means msg_type/data fields are allocated in the
 * reply msg.  These bits (with akbReplyInit, akbRequestQC, akbReplyQC)
 * control msg structure declarations packing, and checking of
 * mach_msg_type_t fields.
 *
 * akbUserArg means this argument is an argument to the user-side stub.
 * akbServerArg means this argument is an argument to
 * the server procedure called by the server-side stub.
 *
 * The akbSend* and akbReturn* bits control packing/extracting values
 * in the request and reply messages.
 *
 * akbSend means the argument's value is carried in the request msg.
 * akbSendBody implies akbSend; the value is carried in the msg body.
 * akbSendSnd implies akbSend; the value is stuffed into the request.
 * akbSendRcv implies akbSend; the value is pulled out of the request.
 *
 * akbReturn, akbReturnBody, akbReturnSnd, akbReturnRcv are defined
 * similarly but apply to the reply message.
 *
 * User-side code generation (header.c, user.c) and associated code
 * should use akbSendSnd and akbReturnRcv, but not akbSendRcv and
 * akbReturnSnd.  Server-side code generation (server.c) is reversed.
 * Code generation should use the more specific akb{Send,Return}{Snd,Rcv}
 * bits when possible, instead of akb{Send,Return}.
 *
 * Note that akRetCode and akReturn lack any Return bits, although
 * there is a value in the msg.  These guys are packed/unpacked
 * with special code, unlike other arguments.
 *
 * akbReplyInit implies akbReply.  It means the server-side stub
 * should initialize the argument's msg_type field in the reply msg.
 * Some special arguments (RetCode, Dummy, Tid) have their msg_type
 * fields in the reply message initialized by the server demux
 * function; these arguments have akbReply but not akbReplyInit.
 *
 * akbRequestQC implies akbRequest.  If it's on, then the
 * mach_msg_type_t value in the request message can be checked quickly
 * (by casting to an int and checking with a single comparison).
 * akbReplyQC has the analogous meaning with respect to akbReply.
 *
 * akbVariable means the argument has variable-sized inline data.
 * It isn't currently used for code generation, but routine.c
 * does use it internally.  It is added in rtAugmentArgKind.
 *
 * akbReplyCopy and akbVarNeeded help control code generation in the
 * server-side stub.  The preferred method of handling data in the
 * server-side stub avoids copying into/out-of local variables.  In
 * arguments get passed directly to the server proc from the request msg.
 * Out arguments get stuffed directly into the reply msg by the server proc.
 * For InOut arguments, the server proc gets the address of the data in
 * the request msg, and the resulting data gets copied to the reply msg.
 * Some arguments need a local variable in the server-side stub.  The
 * code extracts the data from the request msg into the variable, and
 * stuff the reply msg from the variable.
 *
 * akbReplyCopy implies akbReply.  It means the data should get copied
 * from the request msg to the reply msg after the server proc is called.
 * It is only used by akInOut.  akTid doesn't need it because the tid
 * data in the reply msg is initialized in the server demux function.
 *
 * akbVarNeeded means the argument needs a local variable in the
 * server-side stub.  It is added in rtAugmentArgKind and
 * rtCheckVariable.  An argument shouldn't have all three of
 * akbReturnSnd, akbVarNeeded and akbReplyCopy, because this indicates
 * the reply msg should be stuffed both ways.
 *
 * akbDestroy helps control code generation in the server-side stub.
 * It means this argument has a destructor function which should be called.
 *
 * Header file generation (header.c) uses:
 *	akbUserArg
 *
 * User stub generation (user.c) uses:
 *	akbUserArg, akbRequest, akbReply, akbSendSnd,
 *	akbSendBody, akbReturnRcv, akbReplyQC
 *
 * Server stub generation (server.c) uses:
 *	akbServerArg, akbRequest, akbReply, akbSendRcv, akbReturnSnd,
 *	akbReplyInit, akbReplyCopy, akbVarNeeded, akbSendBody, akbRequestQC
 *
 *
 * During code generation, the routine, argument, and type data structures
 * are read-only.  The code generation functions' output is their only
 * side-effect.
 *
 *
 * Style note:
 * Code can use logical operators (|, &, ~) on akb values.
 * ak values should be manipulated with the ak functions.
 */

/* various useful combinations */

#define akbNone		(0)
#define akbAll		(~akbNone)
#define akbAllBits	(~akeBITS)

#define akbSendBits	(akbSend|akbSendBody|akbSendSnd|akbSendRcv)
#define akbReturnBits	(akbReturn|akbReturnBody|akbReturnSnd|akbReturnRcv)
#define akbSendReturnBits	(akbSendBits|akbReturnBits)

#define akNone		akeNone

#define akIn		akAddFeature(akeNormal,				\
	akbUserArg|akbServerArg|akbRequest|akbSendBits)

#define akOut		akAddFeature(akeNormal,				\
	akbUserArg|akbServerArg|akbReply|akbReturnBits|akbReplyInit)

#define akInOut		akAddFeature(akeNormal,				\
	akbUserArg|akbServerArg|akbRequest|akbReply|			\
	akbSendBits|akbReturnBits|akbReplyInit|akbReplyCopy)

#define akRequestPort	akAddFeature(akeRequestPort,			\
	akbUserArg|akbServerArg|akbSend|akbSendSnd|akbSendRcv)

#define akWaitTime	akAddFeature(akeWaitTime, akbUserArg)

#define akMsgOption	akAddFeature(akeMsgOption, akbUserArg)

#define akMsgSeqno	akAddFeature(akeMsgSeqno,			\
	akbServerArg|akbSend|akbSendRcv)

#define akReplyPort	akAddFeature(akeReplyPort,			\
	akbUserArg|akbServerArg|akbSend|akbSendSnd|akbSendRcv)

#define akUReplyPort	akAddFeature(akeReplyPort,			\
	akbUserArg|akbSend|akbSendSnd|akbSendRcv)

#define akSReplyPort	akAddFeature(akeReplyPort,			\
	akbServerArg|akbSend|akbSendSnd|akbSendRcv)

#define akRetCode	akAddFeature(akeRetCode, akbReply)

#define akReturn	akAddFeature(akeReturn,				\
	akbReply|akbReplyInit)

#define akCount		akAddFeature(akeCount,				\
	akbUserArg|akbServerArg)

#define akPoly		akePoly

#define	akDealloc	akAddFeature(akeDealloc, akbUserArg)

#define	akServerCopy	akAddFeature(akeServerCopy, akbServerArg|akbSendRcv)

#define akCountInOut	akAddFeature(akeCountInOut, akbRequest|akbSendBits)

#define	akCheck(ak, bits)	((ak) & (bits))
#define akCheckAll(ak, bits)	(akCheck(ak, bits) == (bits))
#define akAddFeature(ak, bits)	((ak)|(bits))
#define akRemFeature(ak, bits)	((ak)&~(bits))
#define akIdent(ak)		((ak) & akeBITS)

/*
 * The arguments to a routine/function are linked in left-to-right order.
 * argName is used for error messages and pretty-printing,
 * not code generation.  Code generation shouldn't make any assumptions
 * about the order of arguments, esp. count and poly arguments.
 * (Unfortunately, code generation for inline variable-sized arguments
 * does make such assumptions.)
 *
 * argVarName is the name used in generated code for function arguments
 * and local variable names.  argMsgField is the name used in generated
 * code for the field in msgs where the argument's value lives.
 * argTTName is the name used in generated code for msg-type fields and
 * static variables used to initialize those fields.  argPadName is the
 * name used in generated code for a padding field in msgs.
 *
 * argFlags can be used to override the deallocate and longform bits
 * in the argument's type.  rtProcessArgFlags sets argDeallocate and
 * argLongForm from it and the type.  Code generation shouldn't use
 * argFlags.
 *
 * argCount, argPoly, and argDealloc get to the implicit count, poly,
 * and dealloc arguments associated with the argument; they should be
 * used instead of argNext.  In these implicit arguments, argParent is
 * a pointer to the "real" arg.
 *
 * In count arguments, argMultiplier is a scaling factor applied to
 * the count arg's value to get msg-type-number.  It is equal to
 *	argParent->argType->itElement->itNumber
 */

typedef struct argument
{
    /* if argKind == akReturn, then argName is name of the function */
    identifier_t argName;
    struct argument *argNext;

    arg_kind_t argKind;
    ipc_type_t *argType;

    const_string_t argVarName;	/* local variable and argument names */
    const_string_t argMsgField;	/* message field's name */
    const_string_t argTTName;	/* name for msg_type fields, static vars */
    const_string_t argPadName;	/* name for pad field in msg */

    ipc_flags_t argFlags;
    dealloc_t argDeallocate;	/* overrides argType->itDeallocate */
    boolean_t argLongForm;	/* overrides argType->itLongForm */
    boolean_t argServerCopy;
    boolean_t argCountInOut;

    struct routine *argRoutine;	/* routine we are part of */

    struct argument *argCount;	/* our count arg, if present */
    struct argument *argCInOut;	/* our CountInOut arg, if present */
    struct argument *argPoly;	/* our poly arg, if present */
    struct argument *argDealloc;/* our dealloc arg, if present */
    struct argument *argSCopy;	/* our serverCopy arg, if present */
    struct argument *argParent;	/* in a count or poly arg, the base arg */
    int argMultiplier;		/* for Count argument: parent is a multiple
				   of a basic IPC type.  Argument must be
				   multiplied by Multiplier to get IPC
				   number-of-elements. */

    /* how variable/inline args precede this one, in request and reply */
    int argRequestPos;
    int argReplyPos;
    /* whether argument is by reference, on user and server side */
    boolean_t	argByReferenceUser;
    boolean_t	argByReferenceServer;
} argument_t;

/*
 * The various routine kinds' peculiarities are abstracted by rtCheckRoutine
 * into attributes like rtOneWay, rtProcedure, etc.  These are what
 * code generation should use.  It is Bad Form for code generation to
 * test rtKind.
 */

typedef enum
{
    rkRoutine,
    rkSimpleRoutine,
    rkSimpleProcedure,
    rkProcedure,
    rkFunction,
} routine_kind_t;

typedef struct routine
{
    identifier_t rtName;
    routine_kind_t rtKind;
    argument_t *rtArgs;
    u_int rtNumber;		/* used for making msg ids */

    identifier_t rtUserName;	/* user-visible name (UserPrefix + Name) */
    identifier_t rtServerName;	/* server-side name (ServerPrefix + Name) */

    /* rtErrorName is only used for Procs, SimpleProcs, & Functions */
    identifier_t rtErrorName;	/* error-handler name */

    boolean_t rtOneWay;		/* SimpleProcedure or SimpleRoutine */
    boolean_t rtProcedure;	/* Procedure or SimpleProcedure */
    boolean_t rtUseError;	/* Procedure or Function */

    boolean_t rtSimpleFixedRequest;	/* fixed msg-simple value in request */
    boolean_t rtSimpleSendRequest;	/* in any case, initial value */
    boolean_t rtSimpleCheckRequest;	/* check msg-simple in request */
    boolean_t rtSimpleReceiveRequest;	/* if so, the expected value */

    boolean_t rtSimpleFixedReply;	/* fixed msg-simple value in reply */
    boolean_t rtSimpleSendReply;	/* in any case, initial value */
    boolean_t rtSimpleCheckReply;	/* check msg-simple in reply */
    boolean_t rtSimpleReceiveReply;	/* if so, the expected value */

    u_int rtRequestSize;	/* minimal size of a legal request msg */
    u_int rtReplySize;		/* minimal size of a legal reply msg */

    int rtNumRequestVar;	/* number of variable/inline args in request */
    int rtNumReplyVar;		/* number of variable/inline args in reply */

    int rtMaxRequestPos;	/* maximum of argRequestPos */
    int rtMaxReplyPos;		/* maximum of argReplyPos */

    boolean_t rtNoReplyArgs;	/* if so, no reply message arguments beyond
				   what the server dispatch routine inserts */

    /* distinguished arguments */
    argument_t *rtRequestPort;	/* always non-NULL, defaults to first arg */
    argument_t *rtReplyPort;	/* always non-NULL, defaults to Mig-supplied */
    argument_t *rtReturn;	/* non-NULL unless rtProcedure */
    argument_t *rtServerReturn;	/* NULL or rtReturn  */
    argument_t *rtRetCode;	/* always non-NULL */
    argument_t *rtWaitTime;	/* if non-NULL, will use MACH_RCV_TIMEOUT */
    argument_t *rtMsgOption;	/* always non-NULL, defaults to NONE */
    argument_t *rtMsgSeqno;	/* if non-NULL, server gets passed seqno */
} routine_t;

#define rtNULL		((routine_t *) 0)
#define argNULL		((argument_t *) 0)

extern u_int rtNumber;
/* rt->rtNumber will be initialized */
extern routine_t *rtAlloc(void);
/* skip a number */
extern void rtSkip(void);

extern argument_t *argAlloc(void);

extern boolean_t rtCheckMask(const argument_t *args, u_int mask);

extern boolean_t rtCheckMaskFunction(const argument_t *args, u_int mask,
				     boolean_t (*func)(const argument_t *arg));

extern routine_t *rtMakeRoutine(identifier_t name, argument_t *args);
extern routine_t *rtMakeSimpleRoutine(identifier_t name, argument_t *args);
extern routine_t *rtMakeProcedure(identifier_t name, argument_t *args);
extern routine_t *rtMakeSimpleProcedure(identifier_t name, argument_t *args);
extern routine_t *rtMakeFunction(identifier_t name, argument_t *args,
				 ipc_type_t *type);

extern void rtPrintRoutine(const routine_t *rt);
extern void rtCheckRoutine(routine_t *rt);

extern const char *rtRoutineKindToStr(routine_kind_t rk);

#endif	/* _ROUTINE_H */
