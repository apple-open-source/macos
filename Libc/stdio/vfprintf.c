/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Actual printf innards.
 *
 * This code is large and complicated...
 */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "local.h"
#include "fvwrite.h"

/* Define FLOATING_POINT to get floating point. */
#define	FLOATING_POINT
union arg {
	int			intarg;
	unsigned int		uintarg;
	long			longarg;
	unsigned long		ulongarg;
	quad_t			quadarg;
	u_quad_t		uquadarg;
	void			*pvoidarg;
	char			*pchararg;
	short			*pshortarg;
	int			*pintarg;
	long			*plongarg;
	quad_t			*pquadarg;
#ifdef FLOATING_POINT
	double			doublearg;
	long double		longdoublearg;
#endif
#ifdef ALTIVEC
	unsigned char		vuchararg[16];
	signed char		vchararg[16];
	unsigned short		vushortarg[8];
	signed short		vshortarg[8];
	unsigned int		vuintarg[4];
	signed int		vintarg[4];
	float			vfloatarg[4];
#endif
};

static int	__sprint __P((FILE *, struct __suio *));
static int	__sbprintf __P((FILE *, const char *, va_list));
static char *	__ultoa __P((u_long, char *, int, int, char *));
static char *	__uqtoa __P((u_quad_t, char *, int, int, char *));
static void	__find_arguments __P((const char *, va_list, union arg **));
static void	__grow_type_table __P((int, unsigned char **, int *));

        /*
         * Get the argument indexed by nextarg.   If the argument table is
         * built, use it to get the argument.  If its not, get the next
         * argument (and arguments must be gotten sequentially).
         */
#define GETARG(type) \
        ((argtable != NULL) ? *((type*)(&argtable[nextarg++])) : \
            (nextarg++, va_arg(ap, type)))

#ifdef ALTIVEC
static void getvec(union arg *dst, const union arg *argtable, int nextarg, va_list ap)
{
	vector unsigned char tmp;

	tmp = GETARG(vector unsigned char);
	memcpy( dst, &tmp, 16 );
}

#endif

/*
 * Flush out all the vectors defined by the given uio,
 * then reset it so that it can be reused.
 */
static int
__sprint(fp, uio)
	FILE *fp;
	register struct __suio *uio;
{
	register int err;

	if (uio->uio_resid == 0) {
		uio->uio_iovcnt = 0;
		return (0);
	}
	err = __sfvwrite(fp, uio);
	uio->uio_resid = 0;
	uio->uio_iovcnt = 0;
	return (err);
}

/*
 * Helper function for `fprintf to unbuffered unix file': creates a
 * temporary buffer.  We only work on write-only files; this avoids
 * worries about ungetc buffers and so forth.
 */
static int
__sbprintf(fp, fmt, ap)
	register FILE *fp;
	const char *fmt;
	va_list ap;
{
	int ret;
	FILE fake;
	unsigned char buf[BUFSIZ];

	/* copy the important variables */
	fake._flags = fp->_flags & ~__SNBF;
	fake._file = fp->_file;
	fake._cookie = fp->_cookie;
	fake._write = fp->_write;

	/* set up the buffer */
	fake._bf._base = fake._p = buf;
	fake._bf._size = fake._w = sizeof(buf);
	fake._lbfsize = 0;	/* not actually used, but Just In Case */

	/* do the work, then copy any error status */
	ret = vfprintf(&fake, fmt, ap);
	if (ret >= 0 && fflush(&fake))
		ret = EOF;
	if (fake._flags & __SERR)
		fp->_flags |= __SERR;
	return (ret);
}

/*
 * Macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')

/*
 * Convert an unsigned long to ASCII for printf purposes, returning
 * a pointer to the first character of the string representation.
 * Octal numbers can be forced to have a leading zero; hex numbers
 * use the given digits.
 */
static char *
__ultoa(val, endp, base, octzero, xdigs)
	register u_long val;
	char *endp;
	int base, octzero;
	char *xdigs;
{
	register char *cp = endp;
	register long sval;

	/*
	 * Handle the three cases separately, in the hope of getting
	 * better/faster code.
	 */
	switch (base) {
	case 10:
		if (val < 10) {	/* many numbers are 1 digit */
			*--cp = to_char(val);
			return (cp);
		}
		/*
		 * On many machines, unsigned arithmetic is harder than
		 * signed arithmetic, so we do at most one unsigned mod and
		 * divide; this is sufficient to reduce the range of
		 * the incoming value to where signed arithmetic works.
		 */
		if (val > LONG_MAX) {
			*--cp = to_char(val % 10);
			sval = val / 10;
		} else
			sval = val;
		do {
			*--cp = to_char(sval % 10);
			sval /= 10;
		} while (sval != 0);
		break;

	case 8:
		do {
			*--cp = to_char(val & 7);
			val >>= 3;
		} while (val);
		if (octzero && *cp != '0')
			*--cp = '0';
		break;

	case 16:
		do {
			*--cp = xdigs[val & 15];
			val >>= 4;
		} while (val);
		break;

	default:			/* oops */
		abort();
	}
	return (cp);
}

/* Identical to __ultoa, but for quads. */
static char *
__uqtoa(val, endp, base, octzero, xdigs)
	register u_quad_t val;
	char *endp;
	int base, octzero;
	char *xdigs;
{
	register char *cp = endp;
	register quad_t sval;

	/* quick test for small values; __ultoa is typically much faster */
	/* (perhaps instead we should run until small, then call __ultoa?) */
	if (val <= ULONG_MAX)
		return (__ultoa((u_long)val, endp, base, octzero, xdigs));
	switch (base) {
	case 10:
		if (val < 10) {
			*--cp = to_char(val % 10);
			return (cp);
		}
		if (val > QUAD_MAX) {
			*--cp = to_char(val % 10);
			sval = val / 10;
		} else
			sval = val;
		do {
			*--cp = to_char(sval % 10);
			sval /= 10;
		} while (sval != 0);
		break;

	case 8:
		do {
			*--cp = to_char(val & 7);
			val >>= 3;
		} while (val);
		if (octzero && *cp != '0')
			*--cp = '0';
		break;

	case 16:
		do {
			*--cp = xdigs[val & 15];
			val >>= 4;
		} while (val);
		break;

	default:
		abort();
	}
	return (cp);
}

#ifdef FLOATING_POINT
#include <math.h>
#include "floatio.h"

#define	BUF		(MAXEXP+MAXFRACT+1)	/* + decimal point */
#define	DEFPREC		6

static char *cvt __P((double, int, int, char *, int *, int, int *, char **));
static int exponent __P((char *, int, int));

#if defined(__APPLE__)
/*
 * We don't want to be dependent on any libm symbols so use the versions in Libc
 */

#undef isinf
#undef isnan

extern int isnan(double);
extern int isinf(double);

#endif /* __APPLE __ */

#else /* no FLOATING_POINT */

#define	BUF		68

#endif /* FLOATING_POINT */

#define STATIC_ARG_TBL_SIZE 8           /* Size of static argument table. */

/*
 * Flags used during conversion.
 */
#define	ALT		0x001		/* alternate form */
#define	HEXPREFIX	0x002		/* add 0x or 0X prefix */
#define	LADJUST		0x004		/* left adjustment */
#define	LONGDBL		0x008		/* long double */
#define	LONGINT		0x010		/* long integer */
#define	QUADINT		0x020		/* quad integer */
#define	SHORTINT	0x040		/* short integer */
#define	ZEROPAD		0x080		/* zero (as opposed to blank) pad */
#define FPT		0x100		/* Floating point number */
#define VECTOR		0x200		/* Altivec vector */
int
vfprintf(fp, fmt0, ap)
	FILE *fp;
	const char *fmt0;
	va_list ap;
{
	register char *fmt;	/* format string */
	register int ch;	/* character from fmt */
	register int n, n2;	/* handy integer (short term usage) */
	register char *cp;	/* handy char pointer (short term usage) */
	register struct __siov *iovp;/* for PRINT macro */
	register int flags;	/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format (%.3d), or -1 */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */
#ifdef FLOATING_POINT
	char softsign;		/* temporary negative sign for floats */
	double _double = 0;	/* double precision arguments %[eEfgG] */
	int expt;		/* integer value of exponent */
	int expsize = 0;	/* character count for expstr */
	int ndig;		/* actual number of digits returned by cvt */
	char expstr[7];		/* buffer for exponent string */
	char *dtoaresult;	/* buffer allocated by dtoa */
#endif
#ifdef ALTIVEC
	union arg vval;		/* Vector argument. */
	char *pct;		/* Pointer to '%' at beginning of specifier. */
	char vsep;		/* Vector separator character. */
#endif
	u_long	ulval = 0;	/* integer arguments %[diouxX] */
	u_quad_t uqval = 0;	/* %q integers */
	int base;		/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec, sign, etc */
	int size;		/* size of converted field or string */
	int prsize;             /* max size of printed field */
	char *xdigs = NULL;	/* digits for [xX] conversion */
#define NIOV 8
	struct __suio uio;	/* output information: summary */
	struct __siov iov[NIOV];/* ... and individual io vectors */
	char buf[BUF];		/* space for %c, %[diouxX], %[eEfgG] */
	char ox[2];		/* space for 0x hex-prefix */
        union arg *argtable;        /* args, built due to positional arg */
        union arg statargtable [STATIC_ARG_TBL_SIZE];
        int nextarg;            /* 1-based argument index */
        va_list orgap;          /* original argument pointer */

	/*
	 * Choose PADSIZE to trade efficiency vs. size.  If larger printf
	 * fields occur frequently, increase PADSIZE and make the initialisers
	 * below longer.
	 */
#define	PADSIZE	16		/* pad chunk size */
	static char blanks[PADSIZE] =
	 {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
	static char zeroes[PADSIZE] =
	 {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};

	/*
	 * BEWARE, these `goto error' on error, and PAD uses `n'.
	 */
#define	PRINT(ptr, len) { \
	iovp->iov_base = (ptr); \
	iovp->iov_len = (len); \
	uio.uio_resid += (len); \
	iovp++; \
	if (++uio.uio_iovcnt >= NIOV) { \
		if (__sprint(fp, &uio)) \
			goto error; \
		iovp = iov; \
	} \
}
#define	PAD(howmany, with) { \
	if ((n = (howmany)) > 0) { \
		while (n > PADSIZE) { \
			PRINT(with, PADSIZE); \
			n -= PADSIZE; \
		} \
		PRINT(with, n); \
	} \
}
#define	FLUSH() { \
	if (uio.uio_resid && __sprint(fp, &uio)) \
		goto error; \
	uio.uio_iovcnt = 0; \
	iovp = iov; \
}


	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */
#define	SARG() \
	(flags&LONGINT ? GETARG(long) : \
	    flags&SHORTINT ? (long)(short)GETARG(int) : \
	    (long)GETARG(int))
#define	UARG() \
	(flags&LONGINT ? GETARG(u_long) : \
	    flags&SHORTINT ? (u_long)(u_short)GETARG(int) : \
	    (u_long)GETARG(u_int))

        /*
         * Get * arguments, including the form *nn$.  Preserve the nextarg
         * that the argument can be gotten once the type is determined.
         */
#define GETASTER(val) \
        n2 = 0; \
        cp = fmt; \
        while (is_digit(*cp)) { \
                n2 = 10 * n2 + to_digit(*cp); \
                cp++; \
        } \
        if (*cp == '$') { \
            	int hold = nextarg; \
                if (argtable == NULL) { \
                        argtable = statargtable; \
                        __find_arguments (fmt0, orgap, &argtable); \
                } \
                nextarg = n2; \
                val = GETARG (int); \
                nextarg = hold; \
                fmt = ++cp; \
        } else { \
		val = GETARG (int); \
        }
#ifdef FLOATING_POINT
	dtoaresult = NULL;
#endif
	/* FLOCKFILE(fp); */
	/* sorry, fprintf(read_only_file, "") returns EOF, not 0 */
	if (cantwrite(fp)) {
		/* FUNLOCKFILE(fp); */
		return (EOF);
	}

	/* optimise fprintf(stderr) (and other unbuffered Unix files) */
	if ((fp->_flags & (__SNBF|__SWR|__SRW)) == (__SNBF|__SWR) &&
	    fp->_file >= 0) {
		/* FUNLOCKFILE(fp); */
		return (__sbprintf(fp, fmt0, ap));
	}

	fmt = (char *)fmt0;
        argtable = NULL;
        nextarg = 1;
        orgap = ap;
	uio.uio_iov = iovp = iov;
	uio.uio_resid = 0;
	uio.uio_iovcnt = 0;
	ret = 0;

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		for (cp = fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
			/* void */;
		if ((n = fmt - cp) != 0) {
			if ((unsigned)ret + n > INT_MAX) {
				ret = EOF;
				goto error;
			}
			PRINT(cp, n);
			ret += n;
		}
		if (ch == '\0')
			goto done;
#ifdef ALTIVEC
		pct = fmt;
#endif
		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';
#ifdef ALTIVEC
		vsep = 'X'; /* Illegal value, changed to defaults later. */
#endif

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
			/*
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '#':
			flags |= ALT;
			goto rflag;
#ifdef ALTIVEC
		case ',': case ';': case ':': case '_':
			vsep = ch;
			goto rflag;
#endif
		case '*':
			/*
			 * ``A negative field width argument is taken as a
			 * - flag followed by a positive field width.''
			 *	-- ANSI X3J11
			 * They don't exclude field widths read from args.
			 */
			GETASTER (width);
			if (width >= 0)
				goto rflag;
			width = -width;
			/* FALLTHROUGH */
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				GETASTER (n);
				prec = n < 0 ? -1 : n;
				goto rflag;
			}
			n = 0;
			while (is_digit(ch)) {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			}
			prec = n < 0 ? -1 : n;
			goto reswitch;
		case '0':
			/*
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			if (ch == '$') {
				nextarg = n;
                        	if (argtable == NULL) {
                                	argtable = statargtable;
                                	__find_arguments (fmt0, orgap,
						&argtable);
				}
				goto rflag;
                        }
			width = n;
			goto reswitch;
#ifdef FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
#ifdef ALTIVEC
		case 'v':
			flags |= VECTOR;
			goto rflag;
#endif
		case 'h':
			flags |= SHORTINT;
			goto rflag;
		case 'l':
			if (flags & LONGINT)
				flags |= QUADINT;
			else
				flags |= LONGINT;
			goto rflag;
		case 'q':
			flags |= QUADINT;
			goto rflag;
		case 'z':
			if (sizeof(size_t) == sizeof(long))
				flags |= LONGINT;
			if (sizeof(size_t) == sizeof(quad_t))
				flags |= QUADINT;
			goto rflag;
		case 'c':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			}
#endif
			*(cp = buf) = GETARG(int);
			size = 1;
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				break;
			} else
#endif
			if (flags & QUADINT) {
				uqval = GETARG(quad_t);
				if ((quad_t)uqval < 0) {
					uqval = -uqval;
					sign = '-';
				}
			} else {
				ulval = SARG();
				if ((long)ulval < 0) {
					ulval = -ulval;
					sign = '-';
				}
			}
			base = 10;
			goto number;
#ifdef FLOATING_POINT
		case 'e':
		case 'E':
		case 'f':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				flags |= FPT;
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			}
#endif
			goto fp_begin;
		case 'g':
		case 'G':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				flags |= FPT;
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			}
#endif
			if (prec == 0)
				prec = 1;
fp_begin:		if (prec == -1)
				prec = DEFPREC;
			if (flags & LONGDBL)
				/* XXX this loses precision. */
				_double = (double)GETARG(long double);
			else
				_double = GETARG(double);
			/* do this before tricky precision changes */
			if (isinf(_double)) {
				if (_double < 0)
					sign = '-';
				cp = "Inf";
				size = 3;
				break;
			}
			if (isnan(_double)) {
				cp = "NaN";
				size = 3;
				break;
			}
			flags |= FPT;
			if (dtoaresult != NULL) {
				free(dtoaresult);
				dtoaresult = NULL;
			}
			cp = cvt(_double, prec, flags, &softsign,
				&expt, ch, &ndig, &dtoaresult);
			if (ch == 'g' || ch == 'G') {
				if (expt <= -4 || expt > prec)
					ch = (ch == 'g') ? 'e' : 'E';
				else
					ch = 'g';
			}
			if (ch <= 'e') {	/* 'e' or 'E' fmt */
				--expt;
				expsize = exponent(expstr, expt, ch);
				size = expsize + ndig;
				if (ndig > 1 || flags & ALT)
					++size;
			} else if (ch == 'f') {		/* f fmt */
				if (expt > 0) {
					size = expt;
					if (prec || flags & ALT)
						size += prec + 1;
				} else	/* "0.X" */
					size = prec + 2;
			} else if (expt >= ndig) {	/* fixed g fmt */
				size = expt;
				if (flags & ALT)
					++size;
			} else
				size = ndig + (expt > 0 ?
					1 : 2 - expt);

			if (softsign)
				sign = '-';
			break;
#endif /* FLOATING_POINT */
		case 'n':
			if (flags & QUADINT)
				*GETARG(quad_t *) = ret;
			else if (flags & LONGINT)
				*GETARG(long *) = ret;
			else if (flags & SHORTINT)
				*GETARG(short *) = ret;
			else
				*GETARG(int *) = ret;
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			} else
#endif
			if (flags & QUADINT)
				uqval = GETARG(u_quad_t);
			else
				ulval = UARG();
			base = 8;
			goto nosign;
		case 'p':
			/*
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			}
#endif
			ulval = (u_long)GETARG(void *);
			base = 16;
			xdigs = "0123456789abcdef";
			flags = (flags & ~QUADINT) | HEXPREFIX;
			ch = 'x';
			goto nosign;
		case 's':
			if ((cp = GETARG(char *)) == NULL)
				cp = "(null)";
			if (prec >= 0) {
				/*
				 * can't use strlen; can only look for the
				 * NUL in the first `prec' characters, and
				 * strlen() will go further.
				 */
				char *p = memchr(cp, 0, (size_t)prec);

				if (p != NULL) {
					size = p - cp;
					if (size > prec)
						size = prec;
				} else
					size = prec;
			} else
				size = strlen(cp);
			sign = '\0';
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			} else
#endif
			if (flags & QUADINT)
				uqval = GETARG(u_quad_t);
			else
				ulval = UARG();
			base = 10;
			goto nosign;
		case 'X':
			xdigs = "0123456789ABCDEF";
			goto hex;
		case 'x':
			xdigs = "0123456789abcdef";
hex:
#ifdef ALTIVEC
			if (flags & VECTOR) {
				getvec(&vval, argtable, nextarg, ap);
				nextarg++;
				break;
			} else
#endif
			if (flags & QUADINT)
				uqval = GETARG(u_quad_t);
			else
				ulval = UARG();
			base = 16;
			/* leading 0x/X only if non-zero */
			if (flags & ALT &&
			    (flags & QUADINT ? uqval != 0 : ulval != 0))
				flags |= HEXPREFIX;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 */
			cp = buf + BUF;
			if (flags & QUADINT) {
				if (uqval != 0 || prec != 0)
					cp = __uqtoa(uqval, cp, base,
					    flags & ALT, xdigs);
			} else {
				if (ulval != 0 || prec != 0)
					cp = __ultoa(ulval, cp, base,
					    flags & ALT, xdigs);
			}
			size = buf + BUF - cp;
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

#ifdef ALTIVEC
		if (flags & VECTOR) {
			/*
			 * Do the minimum amount of work necessary to construct
			 * a format specifier that can be used to recursively
			 * call vfprintf() for each element in the vector.
			 */
			int i, j;	/* Counter. */
			int vcnt;	/* Number of elements in vector. */
			char *vfmt;	/* Pointer to format specifier. */
			char vfmt_buf[32]; /* Static buffer for format spec. */
			int vwidth = 0;	/* Width specified via '*'. */
			int vprec = 0;	/* Precision specified via '*'. */
			union {		/* Element. */
				int i;
				float f;
			} velm;
			char *vstr;	/* Used for asprintf(). */
			int vlen;	/* Length returned by asprintf(). */

			/*
			 * Set vfmt.  If vfmt_buf may not be big enough,
			 * malloc() space, taking care to free it later.
			 */
			if (&fmt[-1] - pct < sizeof(vfmt_buf))
				vfmt = vfmt_buf;
			else
				vfmt = (char *)malloc(&fmt[-1] - pct + 1);

			/* Set the separator character, if not specified. */
			if (vsep == 'X') {
				if (ch == 'c')
					vsep = '\0';
				else
					vsep = ' ';
			}

			/* Create the format specifier. */
			for (i = j = 0; i < &fmt[-1] - pct; i++) {
				switch (pct[i]) {
				case ',': case ';': case ':': case '_':
				case 'v': case 'h': case 'l':
					/* Ignore. */
					break;
				case '*':
					if (pct[i - 1] != '.')
						vwidth = 1;
					else
						vprec = 1;
					/* FALLTHROUGH */
				default:
					vfmt[j++] = pct[i];
				}
			}
		
			/*
			 * Determine the number of elements in the vector and
			 * finish up the format specifier.
			 */
			if (flags & SHORTINT) {
				vfmt[j++] = 'h';
				vcnt = 8;
			} else if (flags & LONGINT) {
				vfmt[j++] = 'l';
				vcnt = 4;
			} else {
				switch (ch) {
				case 'e':
				case 'E':
				case 'f':
				case 'g':
				case 'G':
					vcnt = 4;
					break;
				default:
					/*
					 * The default case should never
					 * happen.
					 */
				case 'c':
				case 'd':
				case 'i':
				case 'u':
				case 'o':
				case 'p':
				case 'x':
				case 'X':
					vcnt = 16;
				}
			}
			vfmt[j++] = ch;
			vfmt[j++] = '\0';

/* Get a vector element. */
#define VPRINT(cnt, ind, args...) do {					\
	if (flags & FPT) {						\
		velm.f = vval.vfloatarg[ind];				\
		vlen = asprintf(&vstr, vfmt , ## args, velm.f);		\
	} else {							\
		switch (cnt) {						\
		default:						\
		/* The default case should never happen. */		\
		case 4:							\
			velm.i = vval.vintarg[ind];			\
			break;						\
		case 8:							\
			velm.i = vval.vshortarg[ind];			\
			break;						\
		case 16:						\
			velm.i = vval.vchararg[ind];			\
			break;						\
		}							\
		vlen = asprintf(&vstr, vfmt , ## args, velm.i);		\
	}								\
	ret += vlen;							\
	PRINT(vstr, vlen);						\
	FLUSH();							\
	free(vstr);							\
} while (0)

			/* Actually print. */
			if (vwidth == 0) {
				if (vprec == 0) {
					/* First element. */
					VPRINT(vcnt, 0);
					for (i = 1; i < vcnt; i++) {
						/* Separator. */
						PRINT(&vsep, 1);

						/* Element. */
						VPRINT(vcnt, i);
					}
				} else {
					/* First element. */
					VPRINT(vcnt, 0, prec);
					for (i = 1; i < vcnt; i++) {
						/* Separator. */
						PRINT(&vsep, 1);

						/* Element. */
						VPRINT(vcnt, i, prec);
					}
				}
			} else {
				if (vprec == 0) {
					/* First element. */
					VPRINT(vcnt, 0, width);
					for (i = 1; i < vcnt; i++) {
						/* Separator. */
						PRINT(&vsep, 1);

						/* Element. */
						VPRINT(vcnt, i, width);
					}
				} else {
					/* First element. */
					VPRINT(vcnt, 0, width, prec);
					for (i = 1; i < vcnt; i++) {
						/* Separator. */
						PRINT(&vsep, 1);

						/* Element. */
						VPRINT(vcnt, i, width, prec);
					}
				}
			}
#undef VPRINT

			if (vfmt != vfmt_buf)
				free(vfmt);

			continue;
		}
#endif
		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		else if (flags & HEXPREFIX)
			realsz += 2;

		prsize = width > realsz ? width : realsz;
		if ((unsigned)ret + prsize > INT_MAX) {
			ret = EOF;
			goto error;
		}

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0)
			PAD(width - realsz, blanks);

		/* prefix */
		if (sign) {
			PRINT(&sign, 1);
		} else if (flags & HEXPREFIX) {
			ox[0] = '0';
			ox[1] = ch;
			PRINT(ox, 2);
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
			PAD(width - realsz, zeroes);

		/* leading zeroes from decimal precision */
		PAD(dprec - size, zeroes);

		/* the string or number proper */
#ifdef FLOATING_POINT
		if ((flags & FPT) == 0) {
			PRINT(cp, size);
		} else {	/* glue together f_p fragments */
			if (ch >= 'f') {	/* 'f' or 'g' */
				if (_double == 0) {
					/* kludge for __dtoa irregularity */
					if (expt >= ndig &&
					    (flags & ALT) == 0) {
						PRINT("0", 1);
					} else {
						PRINT("0.", 2);
						PAD(ndig - 1, zeroes);
					}
				} else if (expt <= 0) {
					PRINT("0.", 2);
					PAD(-expt, zeroes);
					PRINT(cp, ndig);
				} else if (expt >= ndig) {
					PRINT(cp, ndig);
					PAD(expt - ndig, zeroes);
					if (flags & ALT)
						PRINT(".", 1);
				} else {
					PRINT(cp, expt);
					cp += expt;
					PRINT(".", 1);
					PRINT(cp, ndig-expt);
				}
			} else {	/* 'e' or 'E' */
				if (ndig > 1 || flags & ALT) {
					ox[0] = *cp++;
					ox[1] = '.';
					PRINT(ox, 2);
					if (_double) {
						PRINT(cp, ndig-1);
					} else	/* 0.[0..] */
						/* __dtoa irregularity */
						PAD(ndig - 1, zeroes);
				} else	/* XeYYY */
					PRINT(cp, 1);
				PRINT(expstr, expsize);
			}
		}
#else
		PRINT(cp, size);
#endif

		/* left-adjusting padding (always blank) */
		if (flags & LADJUST)
			PAD(width - realsz, blanks);

		/* finally, adjust ret */
		ret += prsize;

		FLUSH();	/* copy out the I/O vectors */
	}
done:
	FLUSH();
error:
#ifdef FLOATING_POINT
	if (dtoaresult != NULL)
		free(dtoaresult);
#endif
	if (__sferror(fp))
		ret = EOF;
	/* FUNLOCKFILE(fp); */
        if ((argtable != NULL) && (argtable != statargtable))
                free (argtable);
	return (ret);
	/* NOTREACHED */
}

/*
 * Type ids for argument type table.
 */
#define T_UNUSED	0
#define T_SHORT		1
#define T_U_SHORT	2
#define TP_SHORT	3
#define T_INT		4
#define T_U_INT		5
#define TP_INT		6
#define T_LONG		7
#define T_U_LONG	8
#define TP_LONG		9
#define T_QUAD		10
#define T_U_QUAD	11
#define TP_QUAD		12
#define T_DOUBLE	13
#define T_LONG_DOUBLE	14
#define TP_CHAR		15
#define TP_VOID		16
#define T_VECTOR	17

/*
 * Find all arguments when a positional parameter is encountered.  Returns a
 * table, indexed by argument number, of pointers to each arguments.  The
 * initial argument table should be an array of STATIC_ARG_TBL_SIZE entries.
 * It will be replaces with a malloc-ed one if it overflows.
 */ 
static void
__find_arguments (fmt0, ap, argtable)
	const char *fmt0;
	va_list ap;
	union arg **argtable;
{
	register char *fmt;	/* format string */
	register int ch;	/* character from fmt */
	register int n, n2;	/* handy integer (short term usage) */
	register char *cp;	/* handy char pointer (short term usage) */
	register int flags;	/* flags as above */
	int width;		/* width from format (%8d), or 0 */
	unsigned char *typetable; /* table of types */
	unsigned char stattypetable [STATIC_ARG_TBL_SIZE];
	int tablesize;		/* current size of type table */
	int tablemax;		/* largest used index in table */
	int nextarg;		/* 1-based argument index */

	/*
	 * Add an argument type to the table, expanding if necessary.
	 */
#define ADDTYPE(type) \
	((nextarg >= tablesize) ? \
		__grow_type_table(nextarg, &typetable, &tablesize) : 0, \
	(nextarg > tablemax) ? tablemax = nextarg : 0, \
	typetable[nextarg++] = type)

#define	ADDSARG() \
	((flags&LONGINT) ? ADDTYPE(T_LONG) : \
		((flags&SHORTINT) ? ADDTYPE(T_SHORT) : ADDTYPE(T_INT)))

#define	ADDUARG() \
	((flags&LONGINT) ? ADDTYPE(T_U_LONG) : \
		((flags&SHORTINT) ? ADDTYPE(T_U_SHORT) : ADDTYPE(T_U_INT)))

	/*
	 * Add * arguments to the type array.
	 */
#define ADDASTER() \
	n2 = 0; \
	cp = fmt; \
	while (is_digit(*cp)) { \
		n2 = 10 * n2 + to_digit(*cp); \
		cp++; \
	} \
	if (*cp == '$') { \
		int hold = nextarg; \
		nextarg = n2; \
		ADDTYPE (T_INT); \
		nextarg = hold; \
		fmt = ++cp; \
	} else { \
		ADDTYPE (T_INT); \
	}
	fmt = (char *)fmt0;
	typetable = stattypetable;
	tablesize = STATIC_ARG_TBL_SIZE;
	tablemax = 0; 
	nextarg = 1;
	memset (typetable, T_UNUSED, STATIC_ARG_TBL_SIZE);

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		for (cp = fmt; (ch = *fmt) != '\0' && ch != '%'; fmt++)
			/* void */;
		if (ch == '\0')
			goto done;
		fmt++;		/* skip over '%' */

		flags = 0;
		width = 0;

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
		case '#':
			goto rflag;
		case '*':
			ADDASTER ();
			goto rflag;
		case '-':
		case '+':
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				ADDASTER ();
				goto rflag;
			}
			while (is_digit(ch)) {
				ch = *fmt++;
			}
			goto reswitch;
		case '0':
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			if (ch == '$') {
				nextarg = n;
				goto rflag;
			}
			width = n;
			goto reswitch;
#ifdef FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
		case 'h':
			flags |= SHORTINT;
			goto rflag;
		case 'l':
			if (flags & LONGINT)
				flags |= QUADINT;
			else
				flags |= LONGINT;
			goto rflag;
		case 'q':
			flags |= QUADINT;
			goto rflag;
		case 'c':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
				ADDTYPE(T_INT);
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
			if (flags & QUADINT) {
				ADDTYPE(T_QUAD);
			} else {
				ADDSARG();
			}
			break;
#ifdef FLOATING_POINT
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
			if (flags & LONGDBL)
				ADDTYPE(T_LONG_DOUBLE);
			else
				ADDTYPE(T_DOUBLE);
			break;
#endif /* FLOATING_POINT */
		case 'n':
			if (flags & QUADINT)
				ADDTYPE(TP_QUAD);
			else if (flags & LONGINT)
				ADDTYPE(TP_LONG);
			else if (flags & SHORTINT)
				ADDTYPE(TP_SHORT);
			else
				ADDTYPE(TP_INT);
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
			if (flags & QUADINT)
				ADDTYPE(T_U_QUAD);
			else
				ADDUARG();
			break;
		case 'p':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
				ADDTYPE(TP_VOID);
			break;
		case 's':
			ADDTYPE(TP_CHAR);
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
			if (flags & QUADINT)
				ADDTYPE(T_U_QUAD);
			else
				ADDUARG();
			break;
		case 'X':
		case 'x':
#ifdef ALTIVEC
			if (flags & VECTOR)
				ADDTYPE(T_VECTOR);
			else
#endif
			if (flags & QUADINT)
				ADDTYPE(T_U_QUAD);
			else
				ADDUARG();
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			break;
		}
	}
done:
	/*
	 * Build the argument table.
	 */
	if (tablemax >= STATIC_ARG_TBL_SIZE) {
		*argtable = (union arg *)
		    malloc (sizeof (union arg) * (tablemax + 1));
	}

	(*argtable) [0].intarg = NULL;
	for (n = 1; n <= tablemax; n++) {
		switch (typetable [n]) {
		    case T_UNUSED:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case T_SHORT:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case T_U_SHORT:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case TP_SHORT:
			(*argtable) [n].pshortarg = va_arg (ap, short *);
			break;
		    case T_INT:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case T_U_INT:
			(*argtable) [n].uintarg = va_arg (ap, unsigned int);
			break;
		    case TP_INT:
			(*argtable) [n].pintarg = va_arg (ap, int *);
			break;
		    case T_LONG:
			(*argtable) [n].longarg = va_arg (ap, long);
			break;
		    case T_U_LONG:
			(*argtable) [n].ulongarg = va_arg (ap, unsigned long);
			break;
		    case TP_LONG:
			(*argtable) [n].plongarg = va_arg (ap, long *);
			break;
		    case T_QUAD:
			(*argtable) [n].quadarg = va_arg (ap, quad_t);
			break;
		    case T_U_QUAD:
			(*argtable) [n].uquadarg = va_arg (ap, u_quad_t);
			break;
		    case TP_QUAD:
			(*argtable) [n].pquadarg = va_arg (ap, quad_t *);
			break;
#ifdef FLOATING_POINT
		    case T_DOUBLE:
			(*argtable) [n].doublearg = va_arg (ap, double);
			break;
		    case T_LONG_DOUBLE:
			(*argtable) [n].longdoublearg = va_arg (ap, long double);
			break;
#endif
#ifdef ALTIVEC
		    case T_VECTOR:
			{ int tmp = 0;
			getvec( &((*argtable) [n]), NULL, tmp, ap );
			}
#endif
		    case TP_CHAR:
			(*argtable) [n].pchararg = va_arg (ap, char *);
			break;
		    case TP_VOID:
			(*argtable) [n].pvoidarg = va_arg (ap, void *);
			break;
		}
	}

	if ((typetable != NULL) && (typetable != stattypetable))
		free (typetable);
}

/*
 * Increase the size of the type table.
 */
static void
__grow_type_table (nextarg, typetable, tablesize)
	int nextarg;
	unsigned char **typetable;
	int *tablesize;
{
	unsigned char *const oldtable = *typetable;
	const int oldsize = *tablesize;
	unsigned char *newtable;
	int newsize = oldsize * 2;

	if (newsize < nextarg + 1)
		newsize = nextarg + 1;
	if (oldsize == STATIC_ARG_TBL_SIZE) {
		if ((newtable = malloc (newsize)) == NULL)
			abort();	/* XXX handle better */
		bcopy (oldtable, newtable, oldsize);
	} else {
		if ((newtable = realloc (oldtable, newsize)) == NULL)
			abort();	/* XXX handle better */
	}
	memset (&newtable [oldsize], T_UNUSED, (newsize - oldsize));

	*typetable = newtable;
	*tablesize = newsize;
}


#ifdef FLOATING_POINT

extern char *__dtoa __P((double, int, int, int *, int *, char **, char **));

static char *
cvt(value, ndigits, flags, sign, decpt, ch, length, dtoaresultp)
	double value;
	int ndigits, flags, *decpt, ch, *length;
	char *sign;
	char **dtoaresultp;
{
	int mode, dsgn;
	char *digits, *bp, *rve;

	if (ch == 'f')
		mode = 3;		/* ndigits after the decimal point */
	else {
		/*
		 * To obtain ndigits after the decimal point for the 'e'
		 * and 'E' formats, round to ndigits + 1 significant
		 * figures.
		 */
		if (ch == 'e' || ch == 'E')
			ndigits++;
		mode = 2;		/* ndigits significant digits */
	}
	if (value < 0) {
		value = -value;
		*sign = '-';
	} else
		*sign = '\000';
	digits = __dtoa(value, mode, ndigits, decpt, &dsgn, &rve, dtoaresultp);
	if ((ch != 'g' && ch != 'G') || flags & ALT) {
		/* print trailing zeros */
		bp = digits + ndigits;
		if (ch == 'f') {
			if (*digits == '0' && value)
				*decpt = -ndigits + 1;
			bp += *decpt;
		}
		if (value == 0)	/* kludge for __dtoa irregularity */
			rve = bp;
		while (rve < bp)
			*rve++ = '0';
	}
	*length = rve - digits;
	return (digits);
}

static int
exponent(p0, exp, fmtch)
	char *p0;
	int exp, fmtch;
{
	register char *p, *t;
	char expbuf[MAXEXP];

	p = p0;
	*p++ = fmtch;
	if (exp < 0) {
		exp = -exp;
		*p++ = '-';
	}
	else
		*p++ = '+';
	t = expbuf + MAXEXP;
	if (exp > 9) {
		do {
			*--t = to_char(exp % 10);
		} while ((exp /= 10) > 9);
		*--t = to_char(exp);
		for (; t < expbuf + MAXEXP; *p++ = *t++);
	}
	else {
		*p++ = '0';
		*p++ = to_char(exp);
	}
	return (p - p0);
}
#endif /* FLOATING_POINT */
