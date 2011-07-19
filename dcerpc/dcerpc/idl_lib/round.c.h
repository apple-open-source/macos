/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

/*
**
**  NAME:
**
**      round.c.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      This module is an include file.
**
**	This module rounds CVT floating point data to any specified position.
**	Any of the following rounding modes can be applied:
**
**      Note: None of the following implementations ever perform true truncation
**            on their values.  Whenever truncation becomes necessary - either
**            by being specified directly or by being required indirectly
**            through rounding - values are actually left untouched.  Users
**            of this routine must zero out fractional fields themselves if
**            true truncation is needed.
**
**         VAX ROUNDING
**
**              Input data are rounded such that the representable value nearest
**              the infinitely precise result is delivered; if two representable
**              values are equally near, the one greatest in magnitude is
**              delivered.
**
**         ROUND TO NEAREST
**
**              Input data are rounded such that the representable value nearest
**              the infinitely precise result is delivered; if two representable
**              values are equally near, the one with its least significant bit
**              zero is delivered.
**
**         ROUND TO POSITIVE INFINITY
**
**              Input data are rounded such that the representable value closest
**              to and no less than the infinitely precise result is delivered.
**
**         ROUND TO NEGATIVE INFINITY
**
**              Input data are rounded such that the representable value closest
**              to and no greater than the infinitely precise result is
**              delivered.
**
**         TRUNCATION (ROUND TOWARDS ZERO)
**
**              True truncation is not implemented here.  Input values are
**              delivered in their original, untouched form.
**
**              A definition of "true" truncation follows:  Truncation, or
**              rounding towards zero, implies input data are rounded such
**              that the representable value closest to and no greater in
**              magnitude than the infinitely precise result is delivered.
**
**  VERSION: DCE 1.0
**
*/

/*
**
**  Implicit input/output:
**
**	r			On input, a valid CVT floating point number.
**				On output, a rounded representation of the
**				input.
**
**
**  Implicit input:
**
**	round_bit_position	An integer specifying the position to round to.
**				0 <= round_bit_position <= 127.
**
**				Note: Valid CVT mantissa bits are addressed as 1
**				through 128.  Accordingly, specifying 0 as a
**				position to round to implies an exponent
**				increase whenever rounding occurs.  As for
**				truncation: truncation allways leaves a CVT
**				number untouched.
**
**	options			A valid CVT options bit mask in which at least
**				one, and only one, CVT rounding mode is
**				specified.  If no rounding mode is specified,
**				results are unpredictable.  Rounding is
**				performed in accordance with this mask.
**
**	i			An uninitialized integer used for indexing.
**
**
**  Note: for efficiency this routine performs no explicit error checking.
**
*/

{
  int roundup, more_bits;
  unsigned32  bit_mask;

      /* Check TRUNCATE option */

  if ( ! (options & CVT_C_TRUNCATE) ) {

           /* Determine which word the round bit resides in */

      i = (round_bit_position >> 5) + 1;

           /* Create a mask isolating the round bit */

      bit_mask = 0x1 << (31 - (round_bit_position & 0x1FL));

           /* Check VAX ROUNDING option */

      if (options & CVT_C_VAX_ROUNDING)
          roundup = r[i] & bit_mask;

      else {
          roundup = 0;
          switch ( r[i] & bit_mask ) {

                /* If round bit is clear, and ROUND TO NEAREST option */
                /* is selected we truncate */

          case  0 : if (options & CVT_C_ROUND_TO_NEAREST)
                      break;

                /* Otherwise, make note of wheather there are any bits set */
                /* after the round bit, and then check the remaining cases */

          default : if ( ! (more_bits = r[i] & (bit_mask - 1)) )
                      switch ( i ) {
                        case  1 : more_bits = r[2];
                        case  2 : more_bits |= r[3];
                        case  3 : more_bits |= r[4];
                        default : break;
                      }

                /* Re-check ROUND TO NEAREST option.  NOTE: if we've reached  */
                /* this point and ROUND TO NEAREST has been selected, the     */
                /* round bit is set. */

                    if (options & CVT_C_ROUND_TO_NEAREST) {
                        if ( ! ( roundup = more_bits ) )
								{
                            if ( bit_mask << 1 )
                               roundup = r[i] & (bit_mask << 1);
                            else if (i != 1)
                               roundup = r[i-1] & 1;
								}

                /* Check ROUND TO POSITIVE INFINITY option */

                    } else if (options & CVT_C_ROUND_TO_POS) {
                        if ( !(r[U_R_FLAGS] & U_R_NEGATIVE) )
                          roundup = (r[i] & bit_mask) | more_bits;

                /* Check ROUND TO NEGITIVE INFINITY option */

                    } else if (r[U_R_FLAGS] & U_R_NEGATIVE)
                        roundup = (r[i] & bit_mask) | more_bits;
          }
      }

      if ( roundup ) {          /* Perform rounding if necessary */

               /* Add 1 at round position */

         bit_mask <<= 1;
         r[i] = (r[i] & ~(bit_mask - 1)) + bit_mask;

               /* Propagate any carry */

	 while ( ! r[i] )
           r[--i] += 1;

               /* If carry reaches exponent MSB gets zeroed and must be reset */

         if ( ! i )
           r[1] = 0x80000000L;
      }
  }
}
