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

#include <mach/message.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <global.h>
#include <type.h>
#include <cross64.h>

ipc_type_t *itRetCodeType;	/* used for return codes */
ipc_type_t *itDummyType;	/* used for camelot dummy args */
ipc_type_t *itRequestPortType;	/* used for default Request port arg */
ipc_type_t *itZeroReplyPortType;/* used for dummy Reply port arg */
ipc_type_t *itRealReplyPortType;/* used for default Reply port arg */
ipc_type_t *itWaitTimeType;	/* used for dummy WaitTime args */
ipc_type_t *itMsgOptionType;	/* used for dummy MsgOption args */

static ipc_type_t *list = itNULL;

/*
 *  Searches for a named type.  We use a simple
 *  self-organizing linked list.
 */
ipc_type_t *
itLookUp(identifier_t name)
{
    register ipc_type_t *it, **last;

    for (it = *(last = &list); it != itNULL; it = *(last = &it->itNext))
	if (streql(name, it->itName))
	{
	    /* move this type to the front of the list */
	    *last = it->itNext;
	    it->itNext = list;
	    list = it;

	    return it;
	}

    return itNULL;
}

/*
 *  Enters a new name-type association into
 *  our self-organizing linked list.
 */
void
itInsert(identifier_t name, ipc_type_t *it)
{
    it->itName = name;
    it->itNext = list;
    list = it;
}

static ipc_type_t *
itAlloc(void)
{
    static const ipc_type_t prototype =
    {
	strNULL,		/* identifier_t itName */
	0,			/* ipc_type_t *itNext */
	0,			/* u_int itTypeSize */
	0,			/* u_int itPadSize */
	0,			/* u_int itMinTypeSize */
	0,			/* u_int itInName */
	0,			/* u_int itOutName */
	0,			/* u_int itSize */
	1,			/* u_int itNumber */
	TRUE,			/* boolean_t itInLine */
	FALSE,			/* boolean_t itLongForm */
	d_NO,			/* dealloc_t itDeallocate */
	strNULL,		/* string_t itInNameStr */
	strNULL,		/* string_t itOutNameStr */
	flNone,			/* ipc_flags_t itFlags */
	TRUE,			/* boolean_t itStruct */
	FALSE,			/* boolean_t itString */
	FALSE,			/* boolean_t itVarArray */
	FALSE,			/* boolean_t itIndefinite */
	itNULL,			/* ipc_type_t *itElement */
	strNULL,		/* identifier_t itUserType */
	strNULL,		/* identifier_t itServerType */
	strNULL,		/* identifier_t itTransType */
	strNULL,		/* identifier_t itInTrans */
	strNULL,		/* identifier_t itOutTrans */
	strNULL,		/* identifier_t itDestructor */
    };
    register ipc_type_t *new;

    new = malloc(sizeof *new);
    if (new == itNULL)
	fatal("itAlloc(): %s", unix_error_string(errno));
    *new = prototype;
    return new;
}

/*
 * Convert an IPC type-name into a string.
 */
static string_t
itNameToString(u_int name)
{
    char buffer[100];

    (void) sprintf(buffer, "%u", name);
    return strmake(buffer);
}

/*
 * Calculate itTypeSize, itPadSize, itMinTypeSize.
 * Every type needs this info; it is recalculated
 * when itInLine, itNumber, or itSize changes.
 */
static void
itCalculateSizeInfo(register ipc_type_t *it)
{
    if (it->itInLine)
    {
	u_int bytes = (it->itNumber * it->itSize + 7) / 8;
	u_int padding = (4 - bytes%4)%4;

	it->itTypeSize = bytes;
	it->itPadSize = padding;
	if (it->itVarArray)
	    it->itMinTypeSize = 0;
	else
	    it->itMinTypeSize = bytes + padding;
    }
    else
    {
	/* out-of-line, so use size of pointers */
	u_int bytes = sizeof_pointer;

	it->itTypeSize = bytes;
	it->itPadSize = 0;
	it->itMinTypeSize = bytes;
    }

    /* Unfortunately, these warning messages can't give a type name;
       we haven't seen a name yet (it might stay anonymous.) */

    if ((it->itNumber * it->itSize) % 8 != 0)
	warn("size of C types must be multiples of 8 bits");

    if ((it->itTypeSize == 0) && !it->itVarArray)
	warn("sizeof(%s) == 0", it->itName);
}

/*
 * Fill in default values for some fields used in code generation:
 *	itInNameStr, itOutNameStr, itUserType, itServerType, itTransType
 * Every argument's type should have these values filled in.
 */
static void
itCalculateNameInfo(register ipc_type_t *it)
{
    if (it->itInNameStr == strNULL)
	it->itInNameStr = strmake(itNameToString(it->itInName));
    if (it->itOutNameStr == strNULL)
	it->itOutNameStr = strmake(itNameToString(it->itOutName));

    if (it->itUserType == strNULL)
	it->itUserType = it->itName;
    if (it->itServerType == strNULL)
	it->itServerType = it->itName;

    /*
     *	KernelServer and KernelUser interfaces get special treatment here.
     *	On the kernel side of the interface, ports are really internal
     *	port pointers (ipc_port_t), not port names (mach_port_t).
     *	At this point, we don't know if the argument is in or out,
     *	so we don't know if we should look at itInName or itOutName.
     *	Looking at both should be OK.
     *
     *	This is definitely a hack, but I think it is cleaner than
     *	mucking with type declarations throughout the kernel .def files,
     *	hand-conditionalizing on KERNEL_SERVER and KERNEL_USER.
     */

    if (IsKernelServer &&
	streql(it->itServerType, "mach_port_t") &&
	(((it->itInName == MACH_MSG_TYPE_POLYMORPHIC) &&
	  (it->itOutName == MACH_MSG_TYPE_POLYMORPHIC)) ||
	 MACH_MSG_TYPE_PORT_ANY(it->itInName) ||
	 MACH_MSG_TYPE_PORT_ANY(it->itOutName)))
	it->itServerType = "ipc_port_t";

    if (IsKernelUser &&
	streql(it->itUserType, "mach_port_t") &&
	(((it->itInName == MACH_MSG_TYPE_POLYMORPHIC) &&
	  (it->itOutName == MACH_MSG_TYPE_POLYMORPHIC)) ||
	 MACH_MSG_TYPE_PORT_ANY(it->itInName) ||
	 MACH_MSG_TYPE_PORT_ANY(it->itOutName)))
	it->itUserType = "ipc_port_t";

    if (it->itTransType == strNULL)
	it->itTransType = it->itServerType;
}

ipc_flags_t
itCheckFlags(ipc_flags_t flags, identifier_t name)
{
    /* only one of flLong and flNotLong */

    if ((flags&(flLong|flNotLong)) == (flLong|flNotLong)) {
	warn("%s: IsLong and IsNotLong cancel out", name);
	flags &= ~(flLong|flNotLong);
    }

    /* only one of flDealloc, flNotDealloc, flMaybeDealloc */

    if (flags & flMaybeDealloc) {
	if (flags & (flDealloc|flNotDealloc)) {
	    warn("%s: Dealloc and NotDealloc ignored with Dealloc[]", name);
	    flags &= ~(flDealloc|flNotDealloc);
	}
    }

    if ((flags&(flDealloc|flNotDealloc)) == (flDealloc|flNotDealloc)) {
	warn("%s: Dealloc and NotDealloc cancel out", name);
	flags &= ~(flDealloc|flNotDealloc);
    }

    return flags;
}

dealloc_t
itCheckDeallocate(const ipc_type_t *it, ipc_flags_t flags, dealloc_t dfault,
		  identifier_t name)
{
    dealloc_t dealloc = dfault;

    if (flags & flMaybeDealloc)
	dealloc = d_MAYBE;
    if (flags & flDealloc)
	dealloc = d_YES;
    if (flags & flNotDealloc)
	dealloc = d_NO;

    if (dealloc == dfault) {
	if (flags & flMaybeDealloc)
	    warn("%s: Dealloc[] is redundant", name);
	if (flags & flDealloc)
	    warn("%s: Dealloc is redundant", name);
	if (flags & flNotDealloc)
	    warn("%s: NotDealloc is redundant", name);
    }

    if (flags & (flMaybeDealloc|flDealloc|flNotDealloc)) {
	/* only give semantic warnings if the user specified something */

	if (dealloc != d_NO) {
	    if (it->itInLine && !it->itIndefinite)
		warn("%s: Dealloc will cause an IPC error", name);
	}
    }

    return dealloc;
}

static enum uselong { NotLong, CanBeLong, ShouldBeLong, MustBeLong, TooLong }
itUseLong(register const ipc_type_t *it)
{
    enum uselong uselong = NotLong;

    if ((it->itInName == MACH_MSG_TYPE_POLYMORPHIC) ||
	(it->itOutName == MACH_MSG_TYPE_POLYMORPHIC) ||
	it->itVarArray)
	uselong = CanBeLong;

    if ((it->itVarArray && !it->itInLine) || it->itIndefinite)
	uselong = ShouldBeLong;

    if (((it->itInName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itInName >= (1<<8))) ||
	((it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itOutName >= (1<<8))) ||
	(it->itSize >= (1<<8)) ||
	(it->itNumber >= (1<<12)))
	uselong = MustBeLong;

    if (((it->itInName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itInName >= (1<<16))) ||
	((it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itOutName >= (1<<16))) ||
	(it->itSize >= (1<<16)))
	uselong = TooLong;

    return uselong;
}

boolean_t
itCheckIsLong(const ipc_type_t *it, ipc_flags_t flags, boolean_t dfault,
	      identifier_t name)
{
    boolean_t islong = dfault;

    if (flags & flLong)
	islong = TRUE;
    if (flags & flNotLong)
	islong = FALSE;

    if (islong == dfault) {
	if (flags & flLong)
	    warn("%s: IsLong is redundant", name);
	if (flags & flNotLong)
	    warn("%s: IsNotLong is redundant", name);
    }

    if (flags & (flLong|flNotLong)) {
	enum uselong uselong = itUseLong(it);

	/* only give semantic warnings if the user specified something */

	if (islong && ((int)uselong <= (int)NotLong))
	    warn("%s: doesn't need IsLong", name);
	if (!islong && ((int)uselong >= (int)MustBeLong))
	    warn("%s: too big for IsNotLong", name);
    }

    return islong;
}

/******************************************************
 *  Checks for non-implemented types, conflicting type
 *  flags and whether the long or short form of msg type
 *  descriptor is appropriate. Called after each type statement
 *  is parsed.
 ******************************************************/
static void
itCheckDecl(identifier_t name, register ipc_type_t *it)
{
    enum uselong uselong;

    it->itName = name;

    itCalculateNameInfo(it);

    /* do a bit of error checking, mostly necessary because of
       limitations in Mig */

    if (it->itVarArray) {
	if ((it->itInTrans != strNULL) || (it->itOutTrans != strNULL))
	    error("%s: can't translate variable-sized arrays", name);

	if (it->itDestructor != strNULL)
	    error("%s: can't destroy variable-sized array", name);
    }

    if (it->itVarArray && it->itInLine) {
	if ((it->itElement->itUserType == strNULL) ||
	    (it->itElement->itServerType == strNULL))
	    error("%s: variable-sized in-line arrays need a named base type",
		  name);
    }

    /* process the IPC flag specification */

    it->itFlags = itCheckFlags(it->itFlags, name);

    it->itDeallocate = itCheckDeallocate(it, it->itFlags, d_NO, name);

    uselong = itUseLong(it);
    if (uselong == TooLong)
	warn("%s: too big for mach_msg_type_long_t", name);
    it->itLongForm = itCheckIsLong(it, it->itFlags,
				   (int)uselong >= (int)ShouldBeLong, name);
}

/*
 *  Pretty-prints translation/destruction/type information.
 */
static void
itPrintTrans(const ipc_type_t *it)
{
    if (!streql(it->itName, it->itUserType))
	printf("\tCUserType:\t%s\n", it->itUserType);

    if (!streql(it->itName, it->itServerType))
	printf("\tCServerType:\t%s\n", it->itServerType);

    if (it->itInTrans != strNULL)
       printf("\tInTran:\t\t%s %s(%s)\n",
	      it->itTransType, it->itInTrans, it->itServerType);

    if (it->itOutTrans != strNULL)
       printf("\tOutTran:\t%s %s(%s)\n",
	      it->itServerType, it->itOutTrans, it->itTransType);

    if (it->itDestructor != strNULL)
	printf("\tDestructor:\t%s(%s)\n", it->itDestructor, it->itTransType);
}

/*
 *  Pretty-prints type declarations.
 */
static void
itPrintDecl(identifier_t name, const ipc_type_t *it)
{
    printf("Type %s = ", name);
    if (!it->itInLine)
	printf("^ ");
    if (it->itVarArray)
	if (it->itNumber == 0 || it->itIndefinite)
	    printf("array [] of ");
	else
	    printf("array [*:%d] of ", it->itNumber);
    else if (it->itStruct && ((it->itNumber != 1) ||
			      (it->itInName == MACH_MSG_TYPE_STRING_C)))
	printf("struct [%d] of ", it->itNumber);
    else if (it->itNumber != 1)
	printf("array [%d] of ", it->itNumber);

    if (streql(it->itInNameStr, it->itOutNameStr))
	printf("(%s,", it->itInNameStr);
    else
	printf("(%s|%s", it->itInNameStr, it->itOutNameStr);

    printf(" %d%s%s)\n", it->itSize,	it->itLongForm ? ", IsLong" : "",
	   it->itDeallocate == d_YES ? ", Dealloc"
				     :it->itDeallocate == d_MAYBE ?", Dealloc[]"
								  : "");

    itPrintTrans(it);

    printf("\n");
}

/*
 *  Handles named type-specs, which can occur in type
 *  declarations or in argument lists.  For example,
 *	type foo = type-spec;	// itInsert will get called later
 *	routine foo(arg : bar = type-spec);	// itInsert won't get called
 */
void
itTypeDecl(identifier_t name, ipc_type_t *it)
{
    itCheckDecl(name, it);

    if (BeVerbose)
	itPrintDecl(name, it);
}

/*
 *  Handles declarations like
 *	type new = name;
 *	type new = inname|outname;
 */
ipc_type_t *
itShortDecl(u_int inname, const_string_t instr, u_int outname,
	    const_string_t outstr, u_int defsize)
{
    if (defsize == 0)
	error("must use full IPC type decl");

    return itLongDecl(inname, instr, outname, outstr, defsize, defsize, flNone);
}

/*
 *  Handles declarations like
 *	type new = (name, size, flags...)
 *	type new = (inname|outname, size, flags...)
 */
ipc_type_t *
itLongDecl(u_int inname, const_string_t instr, u_int outname,
	   const_string_t outstr, u_int defsize, u_int size, ipc_flags_t flags)
{
    register ipc_type_t *it;

    if ((defsize != 0) && (defsize != size))
	warn("IPC type decl has strange size (%u instead of %u)",
	     size, defsize);

    it = itAlloc();
    it->itInName = inname;
    it->itInNameStr = instr;
    it->itOutName = outname;
    it->itOutNameStr = outstr;
    it->itSize = size;
    if (inname == MACH_MSG_TYPE_STRING_C)
    {
	it->itStruct = FALSE;
	it->itString = TRUE;
    }
    it->itFlags = flags;

    itCalculateSizeInfo(it);
    return it;
}

static ipc_type_t *
itCopyType(const ipc_type_t *old)
{
    register ipc_type_t *new = itAlloc();

    *new = *old;
    new->itName = strNULL;
    new->itNext = itNULL;
    new->itElement = (ipc_type_t *) old;

    /* size info still valid */
    return new;
}

/*
 * A call to itCopyType is almost always followed with itResetType.
 * The exception is itPrevDecl.  Also called before adding any new
 * translation/destruction/type info (see parser.y).
 *
 *	type new = old;	// new doesn't get old's info
 *	type new = array[*:10] of old;
 *			// new doesn't get old's info, but new->itElement does
 *	type new = array[*:10] of struct[3] of old;
 *			// new and new->itElement don't get old's info
 */

ipc_type_t *
itResetType(ipc_type_t *old)
{
    /* reset all special translation/destruction/type info */

    old->itInTrans = strNULL;
    old->itOutTrans = strNULL;
    old->itDestructor = strNULL;
    old->itUserType = strNULL;
    old->itServerType = strNULL;
    old->itTransType = strNULL;
    return old;
}

/*
 *  Handles the declaration
 *	type new = old;
 */
ipc_type_t *
itPrevDecl(identifier_t name)
{
    register ipc_type_t *old;

    old = itLookUp(name);
    if (old == itNULL)
    {
	error("type '%s' not defined", name);
	return itAlloc();
    }
    else
	return itCopyType(old);
}

/*
 *  Handles the declarations
 *	type new = array[] of old;	// number is 0
 *	type new = array[*] of old;	// number is 0
 *	type new = array[*:number] of old;
 */
ipc_type_t *
itVarArrayDecl(u_int number, register const ipc_type_t *old)
{
    register ipc_type_t *it = itResetType(itCopyType(old));

    if (!it->itInLine || it->itVarArray)
	error("IPC type decl is too complicated");
    if (number != 0)
	it->itNumber *= number;
    else {
	/*
	 * Send at most 2048 bytes inline.
	 */
	u_int	bytes;

	bytes = (it->itNumber * it->itSize + 7) / 8;
	it->itNumber = (2048 / bytes) * it->itNumber;
	it->itIndefinite = TRUE;
    }
    it->itVarArray = TRUE;
    it->itStruct = FALSE;
    it->itString = FALSE;

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = array[number] of old;
 */
ipc_type_t *
itArrayDecl(u_int number, const ipc_type_t *old)
{
    register ipc_type_t *it = itResetType(itCopyType(old));

    if (!it->itInLine || it->itVarArray)
	error("IPC type decl is too complicated");
    it->itNumber *= number;
    it->itStruct = FALSE;
    it->itString = FALSE;

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = ^ old;
 */
ipc_type_t *
itPtrDecl(ipc_type_t *it)
{
    if (!it->itInLine ||
	(it->itVarArray && !it->itIndefinite && (it->itNumber > 0)))
	error("IPC type decl is too complicated");
    it->itNumber = 0;
    it->itIndefinite = FALSE;
    it->itInLine = FALSE;
    it->itStruct = TRUE;
    it->itString = FALSE;

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = struct[number] of old;
 */
ipc_type_t *
itStructDecl(u_int number, const ipc_type_t *old)
{
    register ipc_type_t *it = itResetType(itCopyType(old));

    if (!it->itInLine || it->itVarArray)
	error("IPC type decl is too complicated");
    it->itNumber *= number;
    it->itStruct = TRUE;
    it->itString = FALSE;

    itCalculateSizeInfo(it);
    return it;
}

/*
 * Treat 'c_string[n]' as
 * 'array[n] of (MSG_TYPE_STRING_C, 8)'
 */
ipc_type_t *
itCStringDecl(u_int count, boolean_t varying)
{
    register ipc_type_t *it;
    register ipc_type_t *itElement;

    itElement = itShortDecl(MACH_MSG_TYPE_STRING_C,
			    "MACH_MSG_TYPE_STRING_C",
			    MACH_MSG_TYPE_STRING_C,
			    "MACH_MSG_TYPE_STRING_C",
			    8);
    itCheckDecl("char", itElement);

    it = itResetType(itCopyType(itElement));
    it->itNumber = count;
    it->itVarArray = varying;
    it->itStruct = FALSE;
    it->itString = TRUE;

    itCalculateSizeInfo(it);
    return it;
}

ipc_type_t *
itMakeCountType(void)
{
    ipc_type_t *it = itAlloc();

    it->itName = "mach_msg_type_number_t";
    it->itInName = word_size_name;
    it->itInNameStr = word_size_name_string;
    it->itOutName = word_size_name;
    it->itOutNameStr = word_size_name_string;
    it->itSize = word_size_in_bits;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

extern ipc_type_t *
itMakeIntType()
{
    ipc_type_t *it = itAlloc();

    it->itName = "int";
    it->itInName = MACH_MSG_TYPE_INTEGER_32;
    it->itInNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itOutName = MACH_MSG_TYPE_INTEGER_32;
    it->itOutNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itSize = 32;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

ipc_type_t *
itMakePolyType(void)
{
    ipc_type_t *it = itAlloc();

    it->itName = "mach_msg_type_name_t";
    it->itInName = word_size_name;
    it->itInNameStr = word_size_name_string;
    it->itOutName = word_size_name;
    it->itOutNameStr = word_size_name_string;
    it->itSize = word_size_in_bits;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

ipc_type_t *
itMakeDeallocType(void)
{
    ipc_type_t *it = itAlloc();

    it->itName = "boolean_t";
    it->itInName = MACH_MSG_TYPE_BOOLEAN;
    it->itInNameStr = "MACH_MSG_TYPE_BOOLEAN";
    it->itOutName = MACH_MSG_TYPE_BOOLEAN;
    it->itOutNameStr = "MACH_MSG_TYPE_BOOLEAN";
    it->itSize = 32;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

/*
 *  Initializes the pre-defined types.
 */
void
init_type(void)
{
    itRetCodeType = itAlloc();
    itRetCodeType->itName = "kern_return_t";
    itRetCodeType->itInName = MACH_MSG_TYPE_INTEGER_32;
    itRetCodeType->itInNameStr = "MACH_MSG_TYPE_INTEGER_32";
    itRetCodeType->itOutName = MACH_MSG_TYPE_INTEGER_32;
    itRetCodeType->itOutNameStr = "MACH_MSG_TYPE_INTEGER_32";
    itRetCodeType->itSize = 32;
    itCalculateSizeInfo(itRetCodeType);
    itCalculateNameInfo(itRetCodeType);

    itDummyType = itAlloc();
    itDummyType->itName = "char *";
    itDummyType->itInName = MACH_MSG_TYPE_UNSTRUCTURED;
    itDummyType->itInNameStr = "MACH_MSG_TYPE_UNSTRUCTURED";
    itDummyType->itOutName = MACH_MSG_TYPE_UNSTRUCTURED;
    itDummyType->itOutNameStr = "MACH_MSG_TYPE_UNSTRUCTURED";
    itDummyType->itSize = word_size_in_bits;
    itCalculateSizeInfo(itDummyType);
    itCalculateNameInfo(itDummyType);

    itRequestPortType = itAlloc();
    itRequestPortType->itName = "mach_port_t";
    itRequestPortType->itInName = MACH_MSG_TYPE_COPY_SEND;
    itRequestPortType->itInNameStr = "MACH_MSG_TYPE_COPY_SEND";
    itRequestPortType->itOutName = MACH_MSG_TYPE_PORT_SEND;
    itRequestPortType->itOutNameStr = "MACH_MSG_TYPE_PORT_SEND";
    itRequestPortType->itSize = word_size_in_bits;
    itCalculateSizeInfo(itRequestPortType);
    itCalculateNameInfo(itRequestPortType);

    itZeroReplyPortType = itAlloc();
    itZeroReplyPortType->itName = "mach_port_t";
    itZeroReplyPortType->itInName = 0;
    itZeroReplyPortType->itInNameStr = "0";
    itZeroReplyPortType->itOutName = 0;
    itZeroReplyPortType->itOutNameStr = "0";
    itZeroReplyPortType->itSize = word_size_in_bits;
    itCalculateSizeInfo(itZeroReplyPortType);
    itCalculateNameInfo(itZeroReplyPortType);

    itRealReplyPortType = itAlloc();
    itRealReplyPortType->itName = "mach_port_t";
    itRealReplyPortType->itInName = MACH_MSG_TYPE_MAKE_SEND_ONCE;
    itRealReplyPortType->itInNameStr = "MACH_MSG_TYPE_MAKE_SEND_ONCE";
    itRealReplyPortType->itOutName = MACH_MSG_TYPE_PORT_SEND_ONCE;
    itRealReplyPortType->itOutNameStr = "MACH_MSG_TYPE_PORT_SEND_ONCE";
    itRealReplyPortType->itSize = word_size_in_bits;
    itCalculateSizeInfo(itRealReplyPortType);
    itCalculateNameInfo(itRealReplyPortType);

    itWaitTimeType = itMakeIntType();
    itMsgOptionType = itMakeIntType();
}

/******************************************************
 *  Make sure return values of functions are assignable.
 ******************************************************/
void
itCheckReturnType(identifier_t name, const ipc_type_t *it)
{
    if (!it->itStruct)
	error("type of %s is too complicated", name);
    if ((it->itInName == MACH_MSG_TYPE_POLYMORPHIC) ||
	(it->itOutName == MACH_MSG_TYPE_POLYMORPHIC))
	error("type of %s can't be polymorphic", name);
}


/******************************************************
 *  Called by routine.c to check that request ports are
 *  simple and correct ports with send rights.
 ******************************************************/
void
itCheckRequestPortType(identifier_t name, const ipc_type_t *it)
{
    if (((it->itOutName != MACH_MSG_TYPE_PORT_SEND) &&
	 (it->itOutName != MACH_MSG_TYPE_PORT_SEND_ONCE) &&
	 (it->itOutName != MACH_MSG_TYPE_POLYMORPHIC)) ||
	(it->itNumber != 1) ||
	(it->itSize != word_size_in_bits) ||
	!it->itInLine ||
	it->itDeallocate != d_NO ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper request port", name);
}


/******************************************************
 *  Called by routine.c to check that reply ports are
 *  simple and correct ports with send rights.
 ******************************************************/
void
itCheckReplyPortType(identifier_t name, const ipc_type_t *it)
{
    if (((it->itOutName != MACH_MSG_TYPE_PORT_SEND) &&
	 (it->itOutName != MACH_MSG_TYPE_PORT_SEND_ONCE) &&
	 (it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itOutName != 0)) ||
	(it->itNumber != 1) ||
	(it->itSize != word_size_in_bits) ||
	!it->itInLine ||
	it->itDeallocate != d_NO ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper reply port", name);
}


/******************************************************
 *  Used by routine.c to check that WaitTime is a
 *  simple bit 32 integer.
 ******************************************************/
void
itCheckIntType(identifier_t name, const ipc_type_t *it)
{
    if ((it->itInName != MACH_MSG_TYPE_INTEGER_32) ||
	(it->itOutName != MACH_MSG_TYPE_INTEGER_32) ||
	(it->itNumber != 1) ||
	(it->itSize != 32) ||
	!it->itInLine ||
	it->itDeallocate != d_NO ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper integer", name);
}
void
itCheckNaturalType(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    if ((it->itInName != word_size_name) ||
	(it->itOutName != word_size_name) ||
	(it->itNumber != 1) ||
	(it->itSize != word_size_in_bits) ||
	!it->itInLine ||
	it->itDeallocate != d_NO ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s should have been a %s", name, word_size_name_string);
}
