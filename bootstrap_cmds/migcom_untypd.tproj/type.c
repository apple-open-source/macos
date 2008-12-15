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
 * 92/03/03  16:25:23  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  12:32:39  jeffreyh]
 * 
 * 92/01/14  16:46:47  rpd
 * 	Changed Indefinite types from MustBeLong to ShouldBeLong.
 * 	Added itCheckFlags, itCheckDeallocate, itCheckIsLong.
 * 	Removed itServerCopy.
 * 	[92/01/09            rpd]
 * 
 * 91/08/28  11:17:27  jsb
 * 	Removed itMsgKindType.
 * 	[91/08/12            rpd]
 * 
 * 91/07/31  18:11:12  dbg
 * 	Indefinite-length inline arrays.
 * 
 * 	Change itDeallocate to an enumerated type, to allow for
 * 	user-specified deallocate flag.
 * 
 * 	Add itCStringDecl.
 * 	[91/07/17            dbg]
 * 
 * 91/06/25  10:32:09  rpd
 * 	Changed itCalculateNameInfo to change type names from mach_port_t
 * 	to ipc_port_t for KernelServer and KernelUser interfaces.
 * 	[91/05/28            rpd]
 * 
 * 91/02/05  17:56:02  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:56:12  mrt]
 * 
 * 90/06/02  15:05:54  rpd
 * 	Created for new IPC.
 * 	[90/03/26  21:14:07  rpd]
 * 
 * 07-Apr-89  Richard Draves (rpd) at Carnegie-Mellon University
 *	Extensive revamping.  Added polymorphic arguments.
 *	Allow multiple variable-sized inline arguments in messages.
 *
 * 17-Aug-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Removed translation of MSG_TYPE_INVALID as that type
 *	is no longer defined by the kernel.
 * 
 * 19-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed itPrintTrans and itPrintDecl to reflect new translation syntax.
 *	Changed itCheckDecl to set itServerType to itType if is is strNULL.
 *
 *  4-Feb-88  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added a check to itCheckDecl to make sure that in-line
 *	variable length arrays have a non-zero maximum length.
 *
 * 22-Dec-87  David Golub (dbg) at Carnegie-Mellon University
 *	Removed warning message for translation.
 *
 * 16-Nov-87  David Golub (dbg) at Carnegie-Mellon University
 *	Changed itVarArrayDecl to take a 'max' parameter for maximum
 *	number of elements, and to make type not be 'struct'.
 *	Added itDestructor.
 *
 * 18-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added initialization of itPortType
 *
 * 14-Aug-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Added initialization for itTidType
 *
 * 15-Jun-87  David Black (dlb) at Carnegie-Mellon University
 *	Fixed prototype for itAlloc; was missing itServerType field.
 *
 * 10-Jun-87  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Removed extern from procedure definitions to make hi-c happy
 *	Changed the c type names of itDummyType from caddr_t to
 *	char * and of itCountType from u_int to unsigned int to
 *	eliminate the need to import sys/types.h into the mig generated
 *	code.
 *
 * 28-May-87  Richard Draves (rpd) at Carnegie-Mellon University
 *	Created.
 */

#include "type.h"
#include <mach/message.h>
#include <mach/ndr.h>
#include "routine.h"
#include "error.h"
#include "alloc.h"
#include "global.h"
#include <stdio.h>

ipc_type_t *itRetCodeType;	/* used for return codes */
ipc_type_t *itNdrCodeType;	/* used for NDR format labels */
ipc_type_t *itDummyType;	/* used for camelot dummy args */
ipc_type_t *itTidType;		/* used for camelot tids */
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
itLookUp(name)
    identifier_t name;
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
itInsert(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    it->itName = name;
    it->itNext = list;
    list = it;
}

static ipc_type_t *
itAlloc()
{
    static ipc_type_t prototype =
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
	0,			/* u_int itKPD_Number */
	TRUE,			/* boolean_t itInLine */
	FALSE,			/* boolean_t itMigInLine */
	FALSE,			/* boolean_t itPortType */
	strNULL,		/* string_t itInNameStr */
	strNULL,		/* string_t itOutNameStr */
	TRUE,			/* boolean_t itStruct */
	FALSE,			/* boolean_t itString */
	FALSE,			/* boolean_t itVarArray */
	FALSE,			/* boolean_t itNoOptArray */
	itNULL,			/* ipc_type_t *itElement */
	strNULL,		/* identifier_t itUserType */
	strNULL,		/* identifier_t itServerType */
	strNULL,		/* identifier_t itTransType */
	strNULL,		/* identifier_t itUserKPDType */
	strNULL,		/* identifier_t itServerKPDType */
	strNULL,		/* identifier_t itInTrans */
	strNULL,		/* identifier_t itOutTrans */
	strNULL,		/* identifier_t itDestructor */
    };
    register ipc_type_t *new;

    new = (ipc_type_t *) malloc(sizeof *new);
    if (new == itNULL)
	fatal("itAlloc(): %s", strerror(errno));
    *new = prototype;
    return new;
}

/*
 * Convert an IPC type-name into a string.
 */
static char *
itNameToString(name)
    u_int name;
{
    char buffer[100];

    (void) sprintf(buffer, "%u", name);
    return strmake(buffer);
}

/*
 * Calculate itTypeSize, itPadSize, itMinTypeSize
 * Every type needs this info; it is recalculated
 * when itInLine, itNumber, or itSize changes.
 */
static void
itCalculateSizeInfo(it)
    register ipc_type_t *it;
{
    if (!IS_KERN_PROC_DATA(it))
    {
	u_int bytes = (it->itNumber * it->itSize + 7) / 8;
	u_int padding = (4 - bytes%4)%4;

	it->itTypeSize = bytes;
	it->itPadSize = padding;
	if (IS_VARIABLE_SIZED_UNTYPED(it)) {
	    /*
   	     * for these arrays, the argCount is not a akbRequest|akbReply,
	     * therefore we need to account here for the space of the count
	     * (itMinTypeSize is used only in rtFindSize)
	     */
	    it->itMinTypeSize = 4;
	    /* 
	     * NDR encoded VarString carry the extra offset 4-bytes fields
	     * for MIG, it should be always 0;
	     */
	    if (it->itString)
		it->itMinTypeSize += 4;
	} else
	    it->itMinTypeSize = bytes + padding;
    } else {
	/*
	 * 1) ports 2) OOL 3) ports OOL 
         * all have the same size = sizeof(mach_msg_descriptor_t)
	 */
	u_int bytes;
	if (IS_MULTIPLE_KPD(it))
	    bytes = it->itKPD_Number * sizeof(mach_msg_descriptor_t);
	else 
	    bytes = sizeof(mach_msg_descriptor_t);

	it->itTypeSize = bytes;
	it->itPadSize = 0;
	it->itMinTypeSize = bytes;
    }

    /* Unfortunately, these warning messages can't give a type name;
       we haven't seen a name yet (it might stay anonymous.) */

    if ((it->itTypeSize == 0) && !it->itVarArray)
	warn("sizeof(%s) == 0");
}

/*
 * Fill in default values for some fields used in code generation:
 *	itInNameStr, itOutNameStr, itUserType, itServerType, itTransType
 * Every argument's type should have these values filled in.
 */
static void
itCalculateNameInfo(it)
    register ipc_type_t *it;
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

/******************************************************
 *  Checks for non-implemented types, conflicting type
 *  flags and whether the long or short form of msg type
 *  descriptor is appropriate. Called after each type statement
 *  is parsed.
 ******************************************************/
static void
itCheckDecl(name, it)
    identifier_t name;
    register ipc_type_t *it;
{
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
}

/*
 *  Pretty-prints translation/destruction/type information.
 */
static void
itPrintTrans(it)
    register ipc_type_t *it;
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
itPrintDecl(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    printf("Type %s = ", name);
    if (!it->itInLine)
	printf("^ ");
    if (it->itVarArray)
	if (it->itNumber == 0 || it->itMigInLine)
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

    printf(" %d)\n", it->itSize);

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
itTypeDecl(name, it)
    identifier_t name;
    ipc_type_t *it;
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
itShortDecl(inname, instr, outname, outstr, defsize)
    u_int inname;
    string_t instr;
    u_int outname;
    string_t outstr;
    u_int defsize;
{
    register ipc_type_t *it;

    if (defsize == 0)
	error("must use full IPC type decl");

    it = itAlloc();
    it->itInName = inname;
    it->itInNameStr = instr;
    it->itOutName = outname;
    it->itOutNameStr = outstr;
    it->itSize = defsize;
    if (inname == MACH_MSG_TYPE_STRING_C)
    {
	it->itStruct = FALSE;
	it->itString = TRUE;
    }
    /*
     * I check only inname, because outname
     * has to be a port as well (polymorphic types
     * are now restricted to port rights)
     */
    if (MACH_MSG_TYPE_PORT_ANY(inname) ||
	inname == MACH_MSG_TYPE_POLYMORPHIC) {
	    it->itPortType = TRUE;
	    it->itKPD_Number = 1;
    }

    itCalculateSizeInfo(it);
    return it;
}

static ipc_type_t *
itCopyType(old)
    ipc_type_t *old;
{
    register ipc_type_t *new = itAlloc();

    *new = *old;
    new->itName = strNULL;
    new->itNext = itNULL;
    new->itElement = old;

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
itResetType(old)
    ipc_type_t *old;
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
itPrevDecl(name)
    identifier_t name;
{
    register ipc_type_t *old;

    old = itLookUp(name);
    if (old == itNULL) {
	error("type '%s' not defined", name);
	return itAlloc();
    }
    else
	return itCopyType(old);
}

/*
 *  Handles the declarations
 *	type new = array[] of old;	// number is oo
 *	type new = array[*] of old;	// number is oo
 *	type new = array[*:number] of old;
 */
ipc_type_t *
itVarArrayDecl(number, old)
    u_int number;
    register ipc_type_t *old;
{
    register ipc_type_t *it = itResetType(itCopyType(old)); 

    if (!it->itInLine) {
	/* already an initialized KPD */
	if (it->itKPD_Number != 1 || !number)
	    error("IPC type decl is too complicated for Kernel Processed Data");
	it->itKPD_Number *= number;
	it->itNumber = 1;
	it->itInLine = FALSE;
	it->itStruct = FALSE;  
    } else if (it->itVarArray)
	error("IPC type decl is too complicated");
    else if (number) {
	it->itNumber *= number;
	/* 
	 * Bounded [Scalar, Port] VarArray: in-line!
	 */
	it->itInLine = TRUE;
	it->itStruct = FALSE;  
        if (it->itPortType) 
	    it->itKPD_Number *= number;
    } else {
	it->itNumber = 0;
	/* 
	 * UnBounded [Scalar, Port] VarArray: always in-line
	 * interface and out-of-line mechanism!
	 */
	it->itMigInLine = TRUE;
	it->itInLine = FALSE;
	it->itStruct = TRUE;  
	it->itKPD_Number = 1;
    }
	
    it->itVarArray = TRUE;
    it->itString = FALSE;

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = array[number] of old;
 */
ipc_type_t *
itArrayDecl(number, old)
    u_int number;
    ipc_type_t *old;
{
    register ipc_type_t *it = itResetType(itCopyType(old)); 

    if (!it->itInLine) {
	/* already an initialized KPD */
	if (it->itKPD_Number != 1)
	    error("IPC type decl is too complicated for Kernel Processed Data");
	it->itKPD_Number *= number;
	it->itNumber = 1;
	it->itStruct = FALSE;
	it->itString = FALSE;
	it->itVarArray = FALSE;
    } else if (it->itVarArray)
	error("IPC type decl is too complicated");
    else {
	it->itNumber *= number;
	it->itStruct = FALSE;
	it->itString = FALSE;
	if (it->itPortType) 
	    it->itKPD_Number *= number;
    }

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = ^ old;
 */
ipc_type_t *
itPtrDecl(it)
    ipc_type_t *it;
{
    if (!it->itInLine && !it->itMigInLine)
	error("IPC type decl is already defined to be Out-Of-Line");
    it->itInLine = FALSE;
    it->itStruct = TRUE;
    it->itString = FALSE;
    it->itMigInLine = FALSE;
    it->itKPD_Number = 1;

    itCalculateSizeInfo(it);
    return it;
}

/*
 *  Handles the declaration
 *	type new = struct[number] of old;
 */
ipc_type_t *
itStructDecl(number, old)
    u_int number;
    ipc_type_t *old;
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
itCStringDecl(count, varying)
    int	count;
    boolean_t varying;
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

extern ipc_type_t *
itMakeSubCountType(count, varying, name)
    int	count;
    boolean_t varying;
    string_t  name;
{
    register ipc_type_t *it;
    register ipc_type_t *itElement;

    itElement = itShortDecl(MACH_MSG_TYPE_INTEGER_32,
			    "MACH_MSG_TYPE_INTEGER_32",
			    MACH_MSG_TYPE_INTEGER_32,
			    "MACH_MSG_TYPE_INTEGER_32",
			    32);
    itCheckDecl("mach_msg_type_number_t", itElement);

    it = itResetType(itCopyType(itElement));
    it->itNumber = count;
    /* 
     * I cannot consider it as a Fixed array, otherwise MiG will try
     * to follow the path for efficient copy of arrays
     */
    it->itVarArray = FALSE;
    it->itStruct = FALSE;
    it->itString = FALSE;
    it->itInLine = TRUE;
    it->itName = "mach_msg_type_number_t *";
    if (varying)
	it->itVarArray = TRUE;
    else
	/* to skip the optimized copy of fixed array: in fact we need to
	 * reference each element and we also miss a user type for it */
	it->itNoOptArray = TRUE;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

extern ipc_type_t *
itMakeCountType()
{
    ipc_type_t *it = itAlloc();

    it->itName = "mach_msg_type_number_t";
    it->itInName = MACH_MSG_TYPE_INTEGER_32;
    it->itInNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itOutName = MACH_MSG_TYPE_INTEGER_32;
    it->itOutNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itSize = 32;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

extern ipc_type_t *
itMakePolyType()
{
    ipc_type_t *it = itAlloc();

    it->itName = "mach_msg_type_name_t";
    it->itInName = MACH_MSG_TYPE_INTEGER_32;
    it->itInNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itOutName = MACH_MSG_TYPE_INTEGER_32;
    it->itOutNameStr = "MACH_MSG_TYPE_INTEGER_32";
    it->itSize = 32;

    itCalculateSizeInfo(it);
    itCalculateNameInfo(it);
    return it;
}

extern ipc_type_t *
itMakeDeallocType()
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
init_type()
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

    itNdrCodeType = itAlloc();
    itNdrCodeType->itName = "NDR_record_t";
    itNdrCodeType->itInName = 0;
    itNdrCodeType->itInNameStr = "NDR_record_t";
    itNdrCodeType->itOutName = 0;
    itNdrCodeType->itOutNameStr = "NDR_record_t";
    itNdrCodeType->itSize = sizeof(NDR_record_t) * 8;
    itCalculateSizeInfo(itNdrCodeType);
    itCalculateNameInfo(itNdrCodeType);

    itDummyType = itAlloc();
    itDummyType->itName = "char *";
    itDummyType->itInName = MACH_MSG_TYPE_UNSTRUCTURED;
    itDummyType->itInNameStr = "MACH_MSG_TYPE_UNSTRUCTURED";
    itDummyType->itOutName = MACH_MSG_TYPE_UNSTRUCTURED;
    itDummyType->itOutNameStr = "MACH_MSG_TYPE_UNSTRUCTURED";
    itDummyType->itSize = 32;
    itCalculateSizeInfo(itDummyType);
    itCalculateNameInfo(itDummyType);

    itTidType = itAlloc();
    itTidType->itName = "tid_t";
    itTidType->itInName = MACH_MSG_TYPE_INTEGER_32;
    itTidType->itInNameStr = "MACH_MSG_TYPE_INTEGER_32";
    itTidType->itOutName = MACH_MSG_TYPE_INTEGER_32;
    itTidType->itOutNameStr = "MACH_MSG_TYPE_INTEGER_32";
    itTidType->itSize = 32;
    itTidType->itNumber = 6;
    itCalculateSizeInfo(itTidType);
    itCalculateNameInfo(itTidType);

    itRequestPortType = itAlloc();
    itRequestPortType->itName = "mach_port_t";
    itRequestPortType->itInName = MACH_MSG_TYPE_COPY_SEND;
    itRequestPortType->itInNameStr = "MACH_MSG_TYPE_COPY_SEND";
    itRequestPortType->itOutName = MACH_MSG_TYPE_PORT_SEND;
    itRequestPortType->itOutNameStr = "MACH_MSG_TYPE_PORT_SEND";
    itRequestPortType->itSize = 32;
    itCalculateSizeInfo(itRequestPortType);
    itCalculateNameInfo(itRequestPortType);

    itZeroReplyPortType = itAlloc();
    itZeroReplyPortType->itName = "mach_port_t";
    itZeroReplyPortType->itInName = 0;
    itZeroReplyPortType->itInNameStr = "0";
    itZeroReplyPortType->itOutName = 0;
    itZeroReplyPortType->itOutNameStr = "0";
    itZeroReplyPortType->itSize = 32;
    itCalculateSizeInfo(itZeroReplyPortType);
    itCalculateNameInfo(itZeroReplyPortType);

    itRealReplyPortType = itAlloc();
    itRealReplyPortType->itName = "mach_port_t";
    itRealReplyPortType->itInName = MACH_MSG_TYPE_MAKE_SEND_ONCE;
    itRealReplyPortType->itInNameStr = "MACH_MSG_TYPE_MAKE_SEND_ONCE";
    itRealReplyPortType->itOutName = MACH_MSG_TYPE_PORT_SEND_ONCE;
    itRealReplyPortType->itOutNameStr = "MACH_MSG_TYPE_PORT_SEND_ONCE";
    itRealReplyPortType->itSize = 32;
    itCalculateSizeInfo(itRealReplyPortType);
    itCalculateNameInfo(itRealReplyPortType);

    itWaitTimeType = itMakeCountType();
    itMsgOptionType = itMakeCountType();
}

/******************************************************
 *  Make sure return values of functions are assignable.
 ******************************************************/
void
itCheckReturnType(name, it)
    identifier_t name;
    ipc_type_t *it;
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
itCheckRequestPortType(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    if (((it->itOutName != MACH_MSG_TYPE_PORT_SEND) &&
	 (it->itOutName != MACH_MSG_TYPE_PORT_SEND_ONCE) &&
	 (it->itOutName != MACH_MSG_TYPE_POLYMORPHIC)) ||
	(it->itNumber != 1) ||
	(it->itSize != 32) ||
	!it->itInLine ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper request port", name);
}


/******************************************************
 *  Called by routine.c to check that reply ports are
 *  simple and correct ports with send rights.
 ******************************************************/
void
itCheckReplyPortType(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    if (((it->itOutName != MACH_MSG_TYPE_PORT_SEND) &&
	 (it->itOutName != MACH_MSG_TYPE_PORT_SEND_ONCE) &&
	 (it->itOutName != MACH_MSG_TYPE_POLYMORPHIC) &&
	 (it->itOutName != 0)) ||
	(it->itNumber != 1) ||
	(it->itSize != 32) ||
	!it->itInLine ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper reply port", name);
}


/******************************************************
 *  Used by routine.c to check that WaitTime is a
 *  simple bit 32 integer.
 ******************************************************/
void
itCheckIntType(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    if ((it->itInName != MACH_MSG_TYPE_INTEGER_32) ||
	(it->itOutName != MACH_MSG_TYPE_INTEGER_32) ||
	(it->itNumber != 1) ||
	(it->itSize != 32) ||
	!it->itInLine ||
	!it->itStruct ||
	it->itVarArray)
	error("argument %s isn't a proper integer", name);
}

void
itCheckSecTokenType(name, it)
    identifier_t name;
    ipc_type_t *it;
{
    if (it->itMigInLine || it->itNoOptArray || it->itString || 
        it->itTypeSize != 8 || !it->itInLine || !it->itStruct || 
	it->itVarArray || it->itPortType)
	    error("argument %s isn't a proper SecToken", name);
}
