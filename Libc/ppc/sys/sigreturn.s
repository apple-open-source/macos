/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/* Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	libc/m98k/sys/sigreturn.s
 *
 *	The sigreturn() system call.
 *
 *	The M98K sigcontext structure is more robust than older cousins.
 *	This routine is responsible for restoring register state
 *	based on the setting of sc_regs_saved before trapping into the
 *	kernel.  See <bsd/m98k/signal.h>
 *
 * HISTORY
 * 18-Nov-92  Ben Fathi (benf@next.com)
 *	Ported to m98k.
 *
 * 13-Jan-92  Peter King (king@next.com)
 *	Created.
 */

#import	"assym.h"
#import	"SYS.h"

/*
 * r3 = sigcontext pointer
 */

LEAF(_sigreturn)

	/* Now call the kernel routine to restore the rest */	

	SYSCALL_NONAME(sigreturn, 1)
	blr
END(_sigreturn)
