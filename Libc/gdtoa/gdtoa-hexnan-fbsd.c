/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 2000 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

#include "gdtoaimp.h"
#include <fpmath.h>

 static void
#ifdef KR_headers
L_shift(x, x1, i) ULong *x; ULong *x1; int i;
#else
L_shift(ULong *x, ULong *x1, int i)
#endif
{
	int j;

	i = 8 - i;
	i <<= 2;
	j = ULbits - i;
	do {
		*x |= x[1] << j;
		x[1] >>= i;
		} while(++x < x1);
	}

 int
#ifdef KR_headers
hexnan(sp, fpi, x0)
	CONST char **sp; FPI *fpi; ULong *x0;
#else
hexnan( CONST char **sp, FPI *fpi, ULong *x0)
#endif
{
	int nbits, len;
	char *cp;
	CONST char *s;

	if (sp == NULL || *sp == NULL || **sp != '(')
		return STRTOG_NaN;
	s = *sp;
	if ((cp = strchr(s + 1, ')')) == NULL) {
		*sp += strlen(s);
		cp = s + 1;
		}
	else {
		len = cp - (s + 1);
		cp = alloca(len + 1);
		if (!cp)
			return STRTOG_NaN;
		strlcpy(cp, s + 1, len + 1);
		*sp += len + 2;
		}
	nbits = fpi->nbits;
	/* a hack */
	if (nbits == 52) {	/* double */
		union IEEEd2bits u;
		u.d = nan(cp);
		x0[1] = u.bits.manh;
		x0[0] = u.bits.manl;
		}
	else if (nbits < 52) {	/* float */
		union IEEEf2bits u;
		u.f = nanf(cp);
		x0[0] = u.bits.man;
		}
	else {			/* long double */
		union IEEEl2bits u;
		u.e = nanl(cp);
#if defined(__ppc__) || defined(__ppc64__)
		x0[3] = (ULong)(u.bits.manh >> 44);
		x0[2] = (ULong)(u.bits.manh >> 12);
		x0[1] = ((ULong)u.bits.manh & 0xfff) << 20 | (ULong)(u.bits.manl >> 32);
		x0[0] = (ULong)u.bits.manl;
#elif defined(__i386__) || defined(__x86_64__) || defined(__arm__)
		x0[1] = (ULong)u.bits.manh;
		x0[0] = (ULong)u.bits.manl;
#else
#error unsupported architecture
#endif
		}

	return STRTOG_NaNbits;
	}
