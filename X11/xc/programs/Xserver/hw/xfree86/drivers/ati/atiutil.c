/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiutil.c,v 1.8 2003/01/01 19:16:34 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "atiutil.h"

/*
 * ATIReduceRatio --
 *
 * Reduce a fraction by factoring out the largest common divider of the
 * fraction's numerator and denominator.
 */
void
ATIReduceRatio
(
    int *Numerator,
    int *Denominator
)
{
    int Multiplier, Divider, Remainder;

    Multiplier = *Numerator;
    Divider = *Denominator;

    while ((Remainder = Multiplier % Divider))
    {
        Multiplier = Divider;
        Divider = Remainder;
    }

    *Numerator /= Divider;
    *Denominator /= Divider;
}

/*
 * ATIDivide --
 *
 * Using integer arithmetic and avoiding overflows, this function finds the
 * rounded integer that best approximates
 *
 *         Numerator      Shift
 *        ----------- * 2
 *        Denominator
 *
 * using the specified rounding (floor (<0), nearest (=0) or ceiling (>0)).
 */
int
ATIDivide
(
    int       Numerator,
    int       Denominator,
    int       Shift,
    const int RoundingKind
)
{
    int Rounding = 0;                           /* Default to floor */

#define MaxInt ((int)((unsigned int)(-1) >> 2))

    ATIReduceRatio(&Numerator, &Denominator);

    /* Deal with left shifts but try to keep the denominator even */
    if (Denominator & 1)
    {
        if (Denominator <= MaxInt)
        {
            Denominator <<= 1;
            Shift++;
        }
    }
    else while ((Shift > 0) && !(Denominator & 3))
    {
        Denominator >>= 1;
        Shift--;
    }

    /* Deal with right shifts */
    while (Shift < 0)
    {
        if ((Numerator & 1) && (Denominator <= MaxInt))
            Denominator <<= 1;
        else
            Numerator >>= 1;

        Shift++;
    }

    if (!RoundingKind)                          /* Nearest */
        Rounding = Denominator >> 1;
    else if (RoundingKind > 0)                  /* Ceiling */
        Rounding = Denominator - 1;

    return ((Numerator / Denominator) << Shift) +
            ((((Numerator % Denominator) << Shift) + Rounding) / Denominator);
}
