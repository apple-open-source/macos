/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#ifndef FLOAT_TYPE
#define FLOAT_TYPE double
#define FUNC_PREFIX
#define FLOAT_FMT_FLAG
#define FLOAT_NAME_EXT
#endif

#ifndef MAXDIG
#define MAXDIG 120
#endif

#ifndef APPEND
#define APPEND(a, b) APPEND2 (a, b)
#endif

#ifndef APPEND2
#define APPEND2(a, b) a##b
#endif

#ifndef FLOOR
#define FLOOR APPEND(floor, FLOAT_NAME_EXT)
#endif
#ifndef FABS
#define FABS APPEND(fabs, FLOAT_NAME_EXT)
#endif
#ifndef LOG10
#define LOG10 APPEND(log10, FLOAT_NAME_EXT)
#endif
#ifndef EXP
#define EXP APPEND(exp, FLOAT_NAME_EXT)
#endif
#ifndef ISINF
#define ISINF APPEND(isinf, FLOAT_NAME_EXT)
#endif
#ifndef ISNAN
#define ISNAN APPEND(isnan, FLOAT_NAME_EXT)
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef __set_errno
#define __set_errno(x) errno = (x)
#endif

#ifndef HAVE_FCVT
int
APPEND (FUNC_PREFIX, fcvt_r) (FLOAT_TYPE value, 
                              int ndigit, 
                              int *decpt, 
                              int *sign, 
                              char *buf, 
                              size_t len)
{
   int n, i;
   int left;

   if (buf == NULL) {
      __set_errno (EINVAL);
      return -1;
   }

   left = 0;
   if (!ISINF (value) && !ISNAN (value)) {
      /* OK, the following is not entirely correct.  -0.0 is not handled
       * correctly but glibc 2.0 does not have a portable function to
       * implement this test.  
       */
       *sign = value < 0.0;
       if (*sign)
          value = -value;

       if (ndigit < 0) {
          /* Rounding to the left of the decimal point.  */
          while (ndigit < 0) {
             FLOAT_TYPE new_value = value * 0.1;

             if (new_value < 1.0) {
                ndigit = 0;
                break;
             }

             value = new_value;
             ++left;
             ++ndigit;
          }
       }
    } else {
       /* Value is Inf or NaN.  */
       *sign = 0;
    }

    n = strx_nprint (buf, len, "%.*" FLOAT_FMT_FLAG "f", ndigit, value);
    if (n < 0)
       return -1;

    i = 0;
    while (i < n && isdigit (buf[i]))
       ++i;
    *decpt = i;

    if (i == 0)
       /* Value is Inf or NaN.  */
       return 0;

    if (i < n) {
       do
       ++i;
       while (i < n && !isdigit (buf[i]));

       if (*decpt == 1 && buf[0] == '0' && value != 0.0) {
          /* We must not have leading zeroes.  Strip them all out and
           * adjust *DECPT if necessary.  */
          --*decpt;
          while (i < n && buf[i] == '0')
          {
             --*decpt;
             ++i;
          }
       }

       memmove (&buf[MAX (*decpt, 0)], &buf[i], n - i);
       buf[n - (i - MAX (*decpt, 0))] = '\0';
    }

    if (left) {
       *decpt += left;
       if (--len > n) {
          while (left-- > 0 && n < len)
             buf[n++] = '0';
          buf[n] = '\0';
       }
    }

    return 0;
}
#endif /* HAVE_FCVT */

#define weak_extern2(name) 
weak_extern2 (FLOOR) weak_extern2 (LOG10) weak_extern2 (FABS)
weak_extern2 (EXP)

#ifndef HAVE_ECVT
int
APPEND (FUNC_PREFIX, ecvt_r) (FLOAT_TYPE value, 
                              int ndigit, 
                              int *decpt, 
                              int *sign, 
                              char *buf, 
                              size_t len)
{
  int exponent = 0;

  if (!ISNAN (value) && !ISINF (value) && value != 0.0) {
      FLOAT_TYPE (*log10_function) (FLOAT_TYPE) = &LOG10;

      if (log10_function) {
         /* Use the reasonable code if -lm is included.  */
         FLOAT_TYPE dexponent;
         dexponent = FLOOR (LOG10 (FABS (value)));
         value *= EXP (dexponent * -M_LN10);
         exponent = (int) dexponent;
      } else {
         /* Slow code that doesn't require -lm functions.  */
         FLOAT_TYPE d;
         if (value < 0.0)
            d = -value;
         else
            d = value;
         if (d < 1.0) {
            do {
               d *= 10.0;
               --exponent;
            } while (d < 1.0);
         } else if (d >= 10.0) {
            do {
               d *= 0.1;
               ++exponent;
            } while (d >= 10.0);
         }
         if (value < 0.0)
            value = -d;
         else
            value = d;
       }
    } else if (value == 0.0)
       /* SUSv2 leaves it unspecified whether *DECPT is 0 or 1 for 0.0.
        * This could be changed to -1 if we want to return 0.  */
        exponent = 0;

    if (ndigit <= 0 && len > 0) {
       buf[0] = '\0';
       *decpt = 1;
       if (!ISINF (value) && !ISNAN (value))
          *sign = value < 0.0;
       else
          *sign = 0;
    } else
       if (APPEND (FUNC_PREFIX, fcvt_r) (value, ndigit - 1, decpt, sign,
                      buf, len))
          return -1;

    *decpt += exponent;
    return 0;
}
#endif /* HAVE_ECVT */

#ifndef HAVE_FCVT
char *
APPEND (FUNC_PREFIX, fcvt) (FLOAT_TYPE value, 
                            int ndigit, 
                            int *decpt, 
                            int *sign)
{
  static char buf[MAXDIG];

  (void) APPEND (FUNC_PREFIX, fcvt_r) (value, ndigit, decpt, sign,
                       buf, sizeof buf);

  return buf;
}
#endif /* HAVE_FCVT */

#ifndef HAVE_ECVT
char *
APPEND (FUNC_PREFIX, ecvt) (FLOAT_TYPE value, 
                            int ndigit, 
                            int *decpt, 
                            int *sign)
{
  static char buf[MAXDIG];

  (void) APPEND (FUNC_PREFIX, ecvt_r) (value, ndigit, decpt, sign,
                       buf, sizeof buf);

  return buf;
}
#endif /* HAVE_ECVT */

#ifndef HAVE_GCVT
char *
APPEND (FUNC_PREFIX, gcvt) (FLOAT_TYPE value, 
                            int ndigit, 
                            char *buf)
{
  sprintf (buf, "%.*" FLOAT_FMT_FLAG "g", ndigit, value);
  return buf;
}
#endif /* HAVE_GCVT */
