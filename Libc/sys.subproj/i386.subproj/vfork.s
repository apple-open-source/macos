/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
#include "SYS.h"

#if 0
LEAF(_vfork, 0) 
	CALL_EXTERN(__cthread_fork_prepare)
#if defined(__DYNAMIC__)
// Just like __cthread_fork_prepare we need to prevent threads on the child's
// side from doing a mach call in the dynamic linker until __dyld_fork_child
// is run (see below).  So we call __dyld_fork_prepare which takes out the dyld
// lock to prevent all other threads but this one from entering dyld.
.cstring
LC1:
	.ascii "__dyld_fork_prepare\0"
.text
	subl	$4,%esp		// allocate space for the address parameter
	leal	0(%esp),%eax	// get the address of the allocated space
	pushl	%eax		// push the address of the allocated space
	call	1f
1:	popl	%eax
	leal	LC1-1b(%eax),%eax
	pushl 	%eax		// push the name of the function to look up
	call 	__dyld_func_lookup
	addl	$8,%esp		// remove parameters to __dyld_func_lookup
	movl	0(%esp),%eax	// move the value returned in address parameter
	addl	$4,%esp		// deallocate the space for the address param
	call	*%eax		// call __dyld_fork_prepare indirectly
#endif

	movl 	$SYS_vfork,%eax; 	// code for vfork -> eax
	UNIX_SYSCALL_TRAP; 		// do the system call
	jnc	L1			// jump if CF==0

#if defined(__DYNAMIC__)
// __dyld_fork_parent() is called by the parent process after a vfork syscall.
// This releases the dyld lock acquired by __dyld_fork_prepare().  In this case
// we just use it to clean up after a vfork error so the parent process can 
// dyld after vfork() errors without deadlocking.
.cstring
LC2:
	.ascii "__dyld_fork_parent\0"
.text
	pushl	%eax		// save the return value (errno)
	subl	$4,%esp		// allocate space for the address parameter
	leal	0(%esp),%eax	// get the address of the allocated space
	pushl	%eax		// push the address of the allocated space
	call	1f
1:	popl	%eax
	leal	LC2-1b(%eax),%eax
	pushl 	%eax		// push the name of the function to look up
	call 	__dyld_func_lookup
	addl	$8,%esp		// remove parameters to __dyld_func_lookup
	movl	0(%esp),%eax	// move the value returned in address parameter
	addl	$4,%esp		// deallocate the space for the address param
	call	*%eax		// call __dyld_fork_parent indirectly
	popl	%eax		// restore the return value (errno)
#endif
	CALL_EXTERN(cerror)
	CALL_EXTERN(__cthread_fork_parent)
	movl	$-1,%eax
	ret
	
L1:
	orl	%edx,%edx	// CF=OF=0,  ZF set if zero result	
	jz	L2		// parent, since r1 == 0 in parent, 1 in child
	
	//child here...
#if defined(__DYNAMIC__)
// Here on the child side of the vfork we need to tell the dynamic linker that
// we have vforked.  To do this we call __dyld_fork_child in the dyanmic
// linker.  But since we can't dynamicly bind anything until this is done we
// do this by using the private extern __dyld_func_lookup() function to get the
// address of __dyld_fork_child (the 'C' code equivlent):
//
//	_dyld_func_lookup("__dyld_fork_child", &address);
//	address();
//
.cstring
LC0:
	.ascii "__dyld_fork_child\0"

.text
	subl	$4,%esp		// allocate space for the address parameter
	leal	0(%esp),%eax	// get the address of the allocated space
	pushl	%eax		// push the address of the allocated space
	call	1f
1:	popl	%eax
	leal	LC0-1b(%eax),%eax
	pushl 	%eax		// push the name of the function to look up
	call 	__dyld_func_lookup
	addl	$8,%esp		// remove parameters to __dyld_func_lookup
	movl	0(%esp),%eax	// move the value returned in address parameter
	addl	$4,%esp		// deallocate the space for the address param
	call	*%eax		// call __dyld_fork_child indirectly
#endif
	CALL_EXTERN(_fork_mach_init)
	CALL_EXTERN(__cthread_fork_child)
#if	defined(__DYNAMIC__)
.cstring
LC10:
	.ascii "__dyld_fork_child_final\0"

.text
	subl	$4,%esp		// allocate space for the address parameter
	leal	0(%esp),%eax	// get the address of the allocated space
	pushl	%eax		// push the address of the allocated space
	call	1f
1:	popl	%eax
	leal	LC10-1b(%eax),%eax
	pushl 	%eax		// push the name of the function to look up
	call 	__dyld_func_lookup
	addl	$8,%esp		// remove parameters to __dyld_func_lookup
	movl	0(%esp),%eax	// move the value returned in address parameter
	addl	$4,%esp		// deallocate the space for the address param
	call	*%eax		// call __dyld_fork_child_final indirectly
#endif
	xorl	%eax,%eax	// zero eax
	ret

	//parent here...
L2:
	push	%eax		// save pid
#if	defined(__DYNAMIC__)
// __dyld_fork_parent() is called by the parent process after a vfork syscall.
// This releases the dyld lock acquired by __dyld_fork_prepare().
	subl	$4,%esp		// allocate space for the address parameter
	leal	0(%esp),%eax	// get the address of the allocated space
	pushl	%eax		// push the address of the allocated space
	call	1f
1:	popl	%eax
	leal	LC2-1b(%eax),%eax
	pushl 	%eax		// push the name of the function to look up
	call 	__dyld_func_lookup
	addl	$8,%esp		// remove parameters to __dyld_func_lookup
	movl	0(%esp),%eax	// move the value returned in address parameter
	addl	$4,%esp		// deallocate the space for the address param
	call	*%eax		// call __dyld_fork_parent indirectly
#endif
	CALL_EXTERN_AGAIN(__cthread_fork_parent)
	pop	%eax
	ret		
#else

LEAF(_vfork, 0)
        popl    %ecx
        movl    $SYS_vfork,%eax;      // code for vfork -> eax
        UNIX_SYSCALL_TRAP;              // do the system call
        jnb     L1                      // jump if CF==0
        pushl   %ecx
        BRANCH_EXTERN(cerror)

L1:
        orl     %edx,%edx       // CF=OF=0,  ZF set if zero result
        jz      L2              // parent, since r1 == 0 in parent, 1 in child
        xorl    %eax,%eax       // zero eax
        jmp     *%ecx

L2:
        jmp     *%ecx

#endif

