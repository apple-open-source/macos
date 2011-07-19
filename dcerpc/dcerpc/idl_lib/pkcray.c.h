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

/*
**
**  NAME:
**
**      pkcray.c.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**      This module contains code to extract information from an
**      UNPACKED_REAL structure and to create a CRAY floating number
**      with those bits.
**
**              This module is meant to be used as an include file.
**
**  VERSION: DCE 1.0
**
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

/*
**++
**  Functional Description:
**
**  This module contains code to extract information from an
**  UNPACKED_REAL structure and to create a CRAY floating number
**  with those bits.
**
**  See the header files for a description of the UNPACKED_REAL
**  structure.
**
**  A normalized CRAY floating number looks like:
**
**      [0]: Sign bit, 15 exp bits (bias 16384), 16 fraction bits
**      [1]: 32 low order fraction bits
**
**      0.5 <= fraction < 1.0, MSB explicit
**      Since CRAY has no hidden bits the MSB must always be set.
**
**  Some of the CRAY exponent range is not used.
**  Exponents < 0x2000 and >= 0x6000 are invalid.
**
**
**  Implicit parameters:
**
**      options: a word of flags, see include files.
**
**      output_value: a pointer to the input parameter.
**
**      r: an UNPACKED_REAL structure.
**
**--
*/

if (r[U_R_FLAGS] & U_R_UNUSUAL) {

        if (r[U_R_FLAGS] & U_R_ZERO)

                if (r[U_R_FLAGS] & U_R_NEGATIVE)
                        memcpy(output_value, CRAY_NEG_ZERO, 8);
                else
                        memcpy(output_value, CRAY_POS_ZERO, 8);

        else if (r[U_R_FLAGS] & U_R_INFINITY) {

                if (r[U_R_FLAGS] & U_R_NEGATIVE) {
                        memcpy(output_value, CRAY_NEG_INFINITY, 8);
                        DCETHREAD_RAISE(dcethread_aritherr_e);    /* Negative infinity */
                } else {
                        memcpy(output_value, CRAY_POS_INFINITY, 8);
                        DCETHREAD_RAISE(dcethread_aritherr_e);    /* Positive infinity */
                }

        } else if (r[U_R_FLAGS] & U_R_INVALID) {

                memcpy(output_value, CRAY_INVALID, 8);
                DCETHREAD_RAISE(dcethread_aritherr_e);    /* Invalid value */

        }

} else {

        round_bit_position = 48;

#include "round.c.h"

        if (r[U_R_EXP] < (U_R_BIAS - 8192)) {

                /* Underflow */

                if (r[U_R_FLAGS] & U_R_NEGATIVE)
                        memcpy(output_value, CRAY_NEG_ZERO, 8);
                else
                        memcpy(output_value, CRAY_POS_ZERO, 8);
                if (options & CVT_C_ERR_UNDERFLOW) {
                        DCETHREAD_RAISE(dcethread_fltund_e);  /* Underflow */
                }

        } else if (r[U_R_EXP] > (U_R_BIAS + 8191)) {

                /* Overflow */

                if (options & CVT_C_TRUNCATE) {

                        if (r[U_R_FLAGS] & U_R_NEGATIVE)
                                memcpy(output_value, CRAY_NEG_HUGE, 8);
                        else
                                memcpy(output_value, CRAY_POS_HUGE, 8);

                } else if ((options & CVT_C_ROUND_TO_POS)
                                        && (r[U_R_FLAGS] & U_R_NEGATIVE)) {

                                memcpy(output_value, CRAY_NEG_HUGE, 8);

                } else if ((options & CVT_C_ROUND_TO_NEG)
                                        && !(r[U_R_FLAGS] & U_R_NEGATIVE)) {

                                memcpy(output_value, CRAY_POS_HUGE, 8);

                } else {

                        memcpy(output_value, CRAY_INVALID, 8);

                }

                DCETHREAD_RAISE(dcethread_fltovf_e);  /* Overflow */

        } else {

                /* Adjust bias of exponent */

                r[U_R_EXP] -= (U_R_BIAS - 16384);

                /* Make room for exponent and sign bit */

                r[2] >>= 16;
                r[2] |= (r[1] << 16);
                r[1] >>= 16;

                /* OR in exponent and sign bit */

                r[1] |= (r[U_R_EXP] << 16);
                r[1] |= (r[U_R_FLAGS] << 31);

                /* Shuffle bytes to big endian format */
#if (NDR_LOCAL_INT_REP == ndr_c_int_big_endian)
		if (options & CVT_C_BIG_ENDIAN) {

			r[0] = r[1];
			r[1] = r[2];

		} else {

	                r[0]  = ((r[1] << 24) | (r[1] >> 24));
		        r[0] |= ((r[1] << 8) & 0x00FF0000L);
			r[0] |= ((r[1] >> 8) & 0x0000FF00L);
	                r[1]  = ((r[2] << 24) | (r[2] >> 24));
		        r[1] |= ((r[2] << 8) & 0x00FF0000L);
			r[1] |= ((r[2] >> 8) & 0x0000FF00L);

		}
#else

                r[0]  = ((r[1] << 24) | (r[1] >> 24));
                r[0] |= ((r[1] << 8) & 0x00FF0000L);
                r[0] |= ((r[1] >> 8) & 0x0000FF00L);
                r[1]  = ((r[2] << 24) | (r[2] >> 24));
                r[1] |= ((r[2] << 8) & 0x00FF0000L);
                r[1] |= ((r[2] >> 8) & 0x0000FF00L);
#endif
                memcpy(output_value, r, 8);

        }

}
