/*
 * Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifdef __DYNAMIC__

#include <TargetConditionals.h>	// for TARGET_OS_EMBEDDED

#include <_libkernel_init.h>
#include <dlfcn.h>

struct ProgramVars; /* forward reference */

// system library initialisers
extern void bootstrap_init(void);		// from liblaunch.dylib
extern void mach_init(void);			// from libsystem_mach.dylib
extern void pthread_init(void);			// from libc.a
extern void __libc_init(const struct ProgramVars *vars, void (*atfork_prepare)(void), void (*atfork_parent)(void), void (*atfork_child)(void), const char *apple[]);	// from libc.a
extern void __keymgr_initializer(void);		// from libkeymgr.a
extern void _dyld_initializer(void);		// from libdyld.a
extern void libdispatch_init(void);		// from libdispatch.a
extern void _libxpc_initializer(void);		// from libxpc

// system library atfork handlers
extern void _cthread_fork_prepare();
extern void _cthread_fork_parent();
extern void _cthread_fork_child();
extern void _cthread_fork_child_postinit();

extern void _mach_fork_child();
extern void _cproc_fork_child();
extern void _libc_fork_child();
extern void _notify_fork_child();
extern void _dyld_fork_child();
extern void xpc_atfork_prepare();
extern void xpc_atfork_parent();
extern void xpc_atfork_child();

// advance decls for below;
void libSystem_atfork_prepare();
void libSystem_atfork_parent();
void libSystem_atfork_child();

// from mig_support.c in libc
mach_port_t _mig_get_reply_port();
void _mig_set_reply_port(mach_port_t);

void cthread_set_errno_self(int);
int* __error(void);

/*
 * libsyscall_initializer() initializes all of libSystem.dylib <rdar://problem/4892197>
 */
static __attribute__((constructor)) 
void libSystem_initializer(int argc, const char* argv[], const char* envp[], const char* apple[], const struct ProgramVars* vars)
{
	_libkernel_functions_t libkernel_funcs = {
		.get_reply_port = _mig_get_reply_port,
		.set_reply_port = _mig_set_reply_port,
		.get_errno = __error,
		.set_errno = cthread_set_errno_self,
		.dlsym = dlsym,
	};

	_libkernel_init(libkernel_funcs);

	bootstrap_init();
	mach_init();
	pthread_init();
	__libc_init(vars, libSystem_atfork_prepare, libSystem_atfork_parent, libSystem_atfork_child, apple);
	__keymgr_initializer();
	_dyld_initializer();
	libdispatch_init();
#if !TARGET_OS_EMBEDDED || __IPHONE_OS_VERSION_MAX_ALLOWED >= 50000 // __IPHONE_5_0
	_libxpc_initializer();
#endif
}

/*
 * libSystem_atfork_{prepare,parent,child}() are called by libc when we fork, then we deal with running fork handlers
 * for everyone else.
 */
void libSystem_atfork_prepare()
{
#if !TARGET_OS_EMBEDDED || __IPHONE_OS_VERSION_MAX_ALLOWED >= 50000 // __IPHONE_5_0
	xpc_atfork_prepare();
#endif
	_cthread_fork_prepare();
}

void libSystem_atfork_parent()
{
	_cthread_fork_parent();
#if !TARGET_OS_EMBEDDED || __IPHONE_OS_VERSION_MAX_ALLOWED >= 50000 // __IPHONE_5_0
	xpc_atfork_parent();
#endif
}

void libSystem_atfork_child()
{
	_dyld_fork_child();
	_cthread_fork_child();
	
	bootstrap_init();
	_mach_fork_child();
	_cproc_fork_child();
	_libc_fork_child();
	_notify_fork_child();
#if !TARGET_OS_EMBEDDED || __IPHONE_OS_VERSION_MAX_ALLOWED >= 50000 // __IPHONE_5_0
	xpc_atfork_child();
#endif

	_cthread_fork_child_postinit();
}

/*  
 *  Old crt1.o glue used to call through mach_init_routine which was used to initialize libSystem.
 *  LibSystem now auto-initializes but mach_init_routine is left for binary compatibility.
 */
static void mach_init_old() {}
void (*mach_init_routine)(void) = &mach_init_old;

/*
 *	This __crashreporter_info__ symbol is for all non-dylib parts of libSystem.
 */
const char *__crashreporter_info__;
asm (".desc __crashreporter_info__, 0x10");

#endif /* __DYNAMIC__ */
