/*
 * halt() function causes a breakpoint trap.  This is used when available and
 * is called before exit() to cause a crash that maybe the debugger or the
 * crashcatcher app will catch.  This allows the programmer to see the stack
 * trace that caused the problem.
 */
	.text
	.align 2
	.globl _halt
_halt:

#ifdef __ppc__
	trap
	blr
#endif /* __ppc__ */

#ifdef __i386__
	int3
#endif /* __i386__ */

#ifdef m68k
	rts
#endif

#ifdef hppa
	bv,n	0(%r2)
#endif

#ifdef sparc
	retl
#endif
