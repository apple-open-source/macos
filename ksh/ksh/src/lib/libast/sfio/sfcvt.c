/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#include	"sfhdr.h"

/*	Convert a floating point value to ASCII.
**
**	Written by Kiem-Phong Vo and Glenn Fowler (SFFMT_AFORMAT)
*/

static char		*Inf = "Inf", *Zero = "0";
#define SF_INTPART	(SF_IDIGITS/2)
#define SF_INFINITE	((_Sfi = 3), Inf)
#define SF_ZERO		((_Sfi = 1), Zero)

#if __STD_C
char* _sfcvt(Sfdouble_t dv, char* buf, size_t size, int n_digit,
		int* decpt, int* sign, int* len, int format)
#else
char* _sfcvt(dv,buf,size,n_digit,decpt,sign,len,format)
Sfdouble_t	dv;		/* value to convert		*/
char*		buf;		/* conversion goes here		*/
size_t		size;		/* size of buf			*/
int		n_digit;	/* number of digits wanted	*/
int*		decpt;		/* to return decimal point	*/
int*		sign;		/* to return sign		*/
int*		len;		/* return string length		*/
int		format;		/* conversion format		*/
#endif
{
	reg char		*sp;
	reg long		n, v;
	reg char		*ep, *b, *endsp;
	_ast_flt_unsigned_max_t	m;

	static char		lx[] = "0123456789abcdef";
	static char		ux[] = "0123456789ABCDEF";

	*sign = *decpt = 0;

#if !_ast_fltmax_double
	if(format&SFFMT_LDOUBLE)
	{	Sfdouble_t	f = dv;

		if(f == 0.)
			return SF_ZERO;
		else if((*sign = (f < 0.)) )	/* assignment = */
			f = -f;
		if(f < LDBL_MIN)
			return SF_ZERO;
		else if(f > LDBL_MAX)
			return SF_INFINITE;

		if(format & SFFMT_AFORMAT)
		{	Sfdouble_t	g;
			int		x;
			b = sp = buf;
			ep = (format & SFFMT_UPPER) ? ux : lx;
			if(n_digit <= 0 || n_digit >= (size - 9))
				n_digit = size - 9;
			endsp = sp + n_digit + 1;

			g = frexpl(f, &x);
			*decpt = x;
			f = ldexpl(g, 8 * sizeof(m) - 3);

			for (;;)
			{	m = f;
				x = 8 * sizeof(m);
				while ((x -= 4) >= 0)
				{	*sp++ = ep[(m >> x) & 0xf];
					if (sp >= endsp)
					{	ep = sp + 1;
						goto done;
					}
				}
				f -= m;
				f = ldexpl(f, 8 * sizeof(m));
			}
		}

		n = 0;
		if(f >= (Sfdouble_t)SF_MAXLONG)
		{	/* scale to a small enough number to fit an int */
			v = SF_MAXEXP10-1;
			do
			{	if(f < _Sfpos10[v])
					v -= 1;
				else
				{
					f *= _Sfneg10[v];
					if((n += (1<<v)) >= SF_IDIGITS)
						return SF_INFINITE;
				}
			} while(f >= (Sfdouble_t)SF_MAXLONG);
		}
		*decpt = (int)n;

		b = sp = buf + SF_INTPART;
		if((v = (int)f) != 0)
		{	/* translate the integer part */
			f -= (Sfdouble_t)v;

			sfucvt(v,sp,n,ep,long,ulong);

			n = b-sp;
			if((*decpt += (int)n) >= SF_IDIGITS)
				return SF_INFINITE;
			b = sp;
			sp = buf + SF_INTPART;
		}
		else	n = 0;

		/* remaining number of digits to compute; add 1 for later rounding */
		n = (((format&SFFMT_EFORMAT) || *decpt <= 0) ? 1 : *decpt+1) - n;
		if(n_digit > 0)
		{	if(n_digit > LDBL_DIG)
				n_digit = LDBL_DIG;
			n += n_digit;
		}

		if((ep = (sp+n)) > (endsp = buf+(size-2)))
			ep = endsp; 
		if(sp > ep)
			sp = ep;
		else
		{
			if((format&SFFMT_EFORMAT) && *decpt == 0 && f > 0.)
			{	Sfdouble_t	d;
				while((int)(d = f*10.) == 0)
				{	f = d;
					*decpt -= 1;
				}
			}

			while(sp < ep)
			{	/* generate fractional digits */
				if(f <= 0.)
				{	/* fill with 0's */
					do { *sp++ = '0'; } while(sp < ep);
					goto done;
				}
				else if((n = (long)(f *= 10.)) < 10)
				{	*sp++ = '0' + n;
					f -= n;
				}
				else /* n == 10 */
				{	do { *sp++ = '9'; } while(sp < ep);
				}
			}
		}
	} else
#endif
	{	double	f = (double)dv;

		if(f == 0.)
			return SF_ZERO;
		else if((*sign = (f < 0.)) )	/* assignment = */
			f = -f;
		if(f < DBL_MIN)
			return SF_ZERO;
		else if(f > DBL_MAX)
			return SF_INFINITE;

		if(format & SFFMT_AFORMAT)
		{	double	g;
			int	x;
			b = sp = buf;
			ep = (format & SFFMT_UPPER) ? ux : lx;
			if(n_digit <= 0 || n_digit >= (size - 9))
				n_digit = size - 9;
			endsp = sp + n_digit;

			g = frexp(f, &x);
			*decpt = x;
			f = ldexp(g, 8 * sizeof(m) - 3);

			for (;;)
			{	m = f;
				x = 8 * sizeof(m);
				while ((x -= 4) >= 0)
				{	*sp++ = ep[(m >> x) & 0xf];
					if (sp >= endsp)
					{	ep = sp + 1;
						goto done;
					}
				}
				f -= m;
				f = ldexp(f, 8 * sizeof(m));
			}
		}
		n = 0;
		if(f >= (double)SF_MAXLONG)
		{	/* scale to a small enough number to fit an int */
			v = SF_MAXEXP10-1;
			do
			{	if(f < _Sfpos10[v])
					v -= 1;
				else
				{	f *= _Sfneg10[v];
					if((n += (1<<v)) >= SF_IDIGITS)
						return SF_INFINITE;
				}
			} while(f >= (double)SF_MAXLONG);
		}
		*decpt = (int)n;

		b = sp = buf + SF_INTPART;
		if((v = (int)f) != 0)
		{	/* translate the integer part */
			f -= (double)v;

			sfucvt(v,sp,n,ep,long,ulong);

			n = b-sp;
			if((*decpt += (int)n) >= SF_IDIGITS)
				return SF_INFINITE;
			b = sp;
			sp = buf + SF_INTPART;
		}
		else	n = 0;

		/* remaining number of digits to compute; add 1 for later rounding */
		n = (((format&SFFMT_EFORMAT) || *decpt <= 0) ? 1 : *decpt+1) - n;
		if(n_digit > 0)
		{	if(n_digit > DBL_DIG)
				n_digit = DBL_DIG;
			n += n_digit;
		}

		if((ep = (sp+n)) > (endsp = buf+(size-2)))
			ep = endsp; 
		if(sp > ep)
			sp = ep;
		else
		{
			if((format&SFFMT_EFORMAT) && *decpt == 0 && f > 0.)
			{	reg double	d;
				while((int)(d = f*10.) == 0)
				{	f = d;
					*decpt -= 1;
				}
			}

			while(sp < ep)
			{	/* generate fractional digits */
				if(f <= 0.)
				{	/* fill with 0's */
					do { *sp++ = '0'; } while(sp < ep);
					goto done;
				}
				else if((n = (int)(f *= 10.)) < 10)
				{	*sp++ = (char)('0' + n);
					f -= n;
				}
				else /* n == 10 */
				{	do { *sp++ = '9'; } while(sp < ep);
				}
			}
		}
	}

	if(ep <= b)
		ep = b+1;
	else if(ep < endsp)
	{	/* round the last digit */
		*--sp += 5;
		while(*sp > '9')
		{	*sp = '0';
			if(sp > b)
				*--sp += 1;
			else
			{	/* next power of 10 */
				*sp = '1';
				*decpt += 1;
				if(!(format&SFFMT_EFORMAT))
				{	/* add one more 0 for %f precision */
					ep[-1] = '0';
					ep += 1;
				}
			}
		}
	}

done:
	*--ep = '\0';
	if(len)
		*len = ep-b;
	return b;
}
