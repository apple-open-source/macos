/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * The common startup code.  This code is ifdef'ed with the 'C' preprocessor
 * macros CRT0, CRT1, GCRT and POSTSCRIPT.  It is used to create
 * the following files when compiled with the following macros defined:
 *
 *  File      Dedined Macros	   Purpose
 * crt1.o	CRT1		 startup for programs compiled -dynamic
 * gcrt1.o	CRT1, GCRT	 profiling startup, programs compiled -dynamic
 *
 * pscrt1.o	CRT1, POSTSCRIPT startup for postscript compiled -dynamic
 * crt0.o	CRT0		 startup for programs compiled -static 
 * gcrt0.o	CRT0, GCRT	 profiling startup for programs compiled -static
 * pscrt0.o	CRT0, POSTSCRIPT startup for postscript compiled -static
 * 
 * Starting in 5.0, Rhapsody, the first two files are to be placed in /lib the
 * others in /usr/local/lib.
 */

/*
 * Global data definitions (initialized data).
 */
int NXArgc = 0;
char **NXArgv = (char **)0;
char **environ = (char **)0;
char *__progname = "";
#if defined(CRT1)
/*
 * The following symbols are reference by System Framework symbolicly (instead
 * of through undefined references (to allow prebinding). To get strip(1) to
 * know these symbols are not to be stripped they need to have the
 * REFERENCED_DYNAMICALLY bit (0x10) set.  This would have been done automaticly
 * by ld(1) if these symbols were referenced through undefined symbols.
 * The catch_exception_raise symbol is special in that the Mach API specifically
 * requires that the library call into the user program for its implementation.
 * Therefore, we need to create a common definition and make sure the symbol
 * doesn't get stripped.
 */
asm(".desc _NXArgc, 0x10");
asm(".desc _NXArgv, 0x10");
asm(".desc _environ, 0x10");
asm(".desc __mh_execute_header, 0x10");
asm(".comm _catch_exception_raise, 4");
asm(".desc _catch_exception_raise, 0x10");
asm(".comm _catch_exception_raise_state, 4");
asm(".desc _catch_exception_raise_state, 0x10");
asm(".comm _catch_exception_raise_state_identity, 4");
asm(".desc _catch_exception_raise_state_identity, 0x10");
asm(".comm _do_mach_notify_dead_name, 4");
asm(".desc _do_mach_notify_dead_name, 0x10");
asm(".comm _do_seqnos_mach_notify_dead_name, 4");
asm(".desc _do_seqnos_mach_notify_dead_name, 0x10");
asm(".comm _do_mach_notify_no_senders, 4");
asm(".desc _do_mach_notify_no_senders, 0x10");
asm(".comm _do_seqnos_mach_notify_no_senders, 4");
asm(".desc _do_seqnos_mach_notify_no_senders, 0x10");
asm(".comm _do_mach_notify_port_deleted, 4");
asm(".desc _do_mach_notify_port_deleted, 0x10");
asm(".comm _do_seqnos_mach_notify_port_deleted, 4");
asm(".desc _do_seqnos_mach_notify_port_deleted, 0x10");
asm(".comm _do_mach_notify_send_once, 4");
asm(".desc _do_mach_notify_send_once, 0x10");
asm(".comm _do_seqnos_mach_notify_send_once, 4");
asm(".desc _do_seqnos_mach_notify_send_once, 0x10");
asm(".comm _clock_alarm_reply, 4");
asm(".desc _clock_alarm_reply, 0x10");
asm(".comm _receive_samples, 4");
asm(".desc _receive_samples, 0x10");
#endif /* CRT1 */

/*
 * Common data definitions.  If the routines in System Framework are not pulled
 * into the executable then the static linker will allocate these as common
 * symbols.  The code in here tests the value of these are non-zero to know if
 * the routines in System Framework got pulled in and should be called.  The
 * first two are pointers to functions.  The second two use just the symbol
 * itself.  In the later case we are using the symbol with two different 'C'
 * types.  To make it as clean as possible the 'C' type declared is that of the
 * external function.  The common symbol is declared with an asm() and the code
 * casts the function name to a pointer to an int and then indirects through
 * the pointer to see if the value is not zero to know the function got linked
 * in.
 */
int (*mach_init_routine)(void);
int (*_cthread_init_routine)(void);
asm(".comm __objcInit, 4");
asm(".comm __cplus_init, 4");
asm(".comm __carbon_init, 4");
extern void _objcInit(void);
extern void _cplus_init(void);
extern void _carbon_init(int argc, char **argv);

#define CARBON	1

/*
 * Prototypes for routines that are called.
 */
extern int main(
    int argc,
    char **argv,
    char **envp,
    char **apple);
extern void exit(
    int status) __attribute__ ((noreturn));
extern int atexit(
    void (*fcn)(void));

#ifdef GCRT
extern void moninit(
    void);
static void _mcleanup(
    void);
extern void monitor(
    char *lowpc,
    char *highpc,
    char *buf,
    int bufsiz,
    int nfunc);
#endif /* GCRT */

#ifdef  CRT1
extern void _dyld_init_check(
    void);
static void _call_mod_init_funcs(
    void);
__private_extern__
int _dyld_func_lookup(
    const char *dyld_func_name,
    unsigned long *address);

extern void __keymgr_dwarf2_register_sections (void);
#endif /* CRT1 */

extern int errno;

/*
 * _start() is called from the machine dependent assembly entry point "start:" .
 * It takes care of setting up the stack so 'C' routines can be called and
 * passes argc, argv and envp to here.
 */
__private_extern__
void
_start(
int argc,
char **argv,
char **envp)
{
    int i;
    char *p, **apple, *dummy_apple[1];
#if defined(__ppc__) || defined(__i386__)
    char **q;
#endif
#ifdef CRT1
    void (*term)(void);
#endif

#ifdef CRT1
	_dyld_init_check();
#endif

	NXArgc = argc;
	NXArgv = argv;
	environ = envp;

	if(mach_init_routine != 0)
	    (void) mach_init_routine();
	if(_cthread_init_routine != 0)
	    (*_cthread_init_routine)();

#ifdef CRT1
	__keymgr_dwarf2_register_sections ();
#endif

#ifdef CRT0
        if(*((int *)_cplus_init) != 0)
            _cplus_init();
#endif

#ifdef CRT1
	_call_mod_init_funcs();
#endif

#ifndef	POSTSCRIPT
        if(*((int *)_objcInit) != 0)
            _objcInit();
#endif

#ifdef CARBON
        if(*((int *)_carbon_init) != 0)
            _carbon_init(argc, argv);
#endif

#ifdef GCRT
	atexit(_mcleanup);
	moninit();
#endif

#ifdef CRT1
	/*
	 * If the dyld we are running with supports module termination routines
	 * for all types of images then register the function to call them with		 * atexit().
	 */
        _dyld_func_lookup("__dyld_mod_term_funcs", (unsigned long *)&term);
        if(term != 0)
	    atexit(term);
#endif

	errno = 0;

	if(argv[0] != 0){
	    p = 0;
	    for(i = 0; argv[0][i] != 0; i++){
		if(argv[0][i] == '/')
		    p = argv[0] + i; 
	    }
	    if(p != 0)
		__progname = p + 1;
	    else
		__progname = argv[0];
	}

	dummy_apple[0] = (char *)0;
	apple = dummy_apple;

#if defined(__ppc__) || defined(__i386__)
	/*
	 * Pickup the pointer to the array that contains as its first pointer a
         * pointer to the exec path (the actual first argument to exec(2) which
	 * most of the time is the same as argv[0]).  This array is placed by
	 * the kernel just after the trailing 0 of the envp[] array.  See the
	 * comments in start.s .
	 */
	for(q = envp; *q != (char *)0; q++)
	    ;
	/*
	 * This feature was added to MacOS X PR2.  So in case we are running
	 * with a kernel that does not set this we do a few checks on the
	 * address of the pointer and the value of the pointer.  These checks
	 * are temporary hacks and only work on PowerPC and i386 knowing the
	 * address of the top of the stack (0xc0000000) and the stack grows
	 * to lower addresses and that the bytes of the exec path should be
	 * on the stack above the pointer.
	 */
	if((unsigned long)(q + 1) < 0xc0000000 &&
	   (unsigned long)(q[1]) > (unsigned long)(q + 1) &&
	   (unsigned long)(q[1]) < 0xc0000000){
	    apple = q + 1;
	}
#endif

	exit(main(argc, argv, envp, apple));
}

#ifdef GCRT
/*
 * For profiling the routine _mcleanup gets registered with atexit so monitor(0)
 * gets called.
 */
static
void
_mcleanup(
void)
{
	monitor(0,0,0,0,0);
}
#endif /* GCRT */

#ifdef CRT1
/*
 * Cause the module initialization routines to be called for the modules linked
 * into the program that have been delayed waiting for the initialization in the
 * runtime startoff (like mach_init, etc).  This is used for C++ constructors
 * and destructors when -dynamic is used.
 */
static
void
_call_mod_init_funcs(void)
{
    void (*p)(void);

        _dyld_func_lookup("__dyld_make_delayed_module_initializer_calls",
                          (unsigned long *)&p);
        p();
}
#endif /* CRT1 */
