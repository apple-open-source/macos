#ifdef __ppc__
#if __GNUC__ > 2  || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
/*
 * The syntax of the asm() is different in the older compilers like the 2.7
 * compiler in MacOS X Server so this is not turned on for that compiler.
 */
static
inline
int
strcmp(
const char *in_s1,
const char *in_s2)
{
    int result, temp;
    register const char *s1 = in_s1 - 1;
    register const char *s2 = in_s2 - 1;

	asm("1:lbzu %0,1(%1)\n"
	    "\tcmpwi cr1,%0,0\n"
	    "\tlbzu %3,1(%2)\n"
	    "\tsubf. %0,%3,%0\n"
	    "\tbeq- cr1,2f\n"
	    "\tbeq+ 1b\n2:"
	    /* outputs: */  : "=&r" (result), "+b" (s1), "+b" (s2), "=r" (temp)
	    /* inputs: */   : "1" (s1) , "2" (s2)
	    /* clobbers: */ : "cr0", "cr1", "memory");
	return(result);
}

/*
"=&r" (result)     means: 'result' is written on (the '='), it's any GP
                   register (the 'r'), and it must not be the same as
                   any of the input registers (the '&').
"+b" (s1)          means: 's1' is read from and written to (the '+'),
                   and it must be a base GP register (i.e., not R0.)
"=r" (temp)        means: 'temp' is any GP reg and it's only written to.

"memory"           in the 'clobbers' section means that gcc will make
                   sure that anything that should be in memory IS there
                   before calling this routine.
*/
#endif /* __GNUC__ > 2  || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95) */
#endif /* __ppc__ */
