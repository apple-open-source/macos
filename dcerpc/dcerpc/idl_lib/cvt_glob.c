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
**
**  NAME:
**
**      cvt_glob.c
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**  Global data used by floating point conversion routines.
**
**  VERSION: DCE 1.0
**
*/
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <cvt.h>
#include <cvt_pvt.h>

#if (NDR_LOCAL_INT_REP != ndr_c_int_big_endian)

idl_ulong_int vax_c[] = {

        0x00008000, 0x00000000, 0x00000000, 0x00000000,         /* ROPs */
        0x00000000, 0x00000000, 0x00000000, 0x00000000,         /* zeros */
        0xffff7fff, 0xffffffff, 0xffffffff, 0xffffffff,         /* +huge */
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,         /* -huge */

};

idl_ulong_int ieee_s[] = {

        0x7fbfffff,             /* little endian ieee s nan */
        0xffffbf7f,             /* big endian ieee s nan */
        0x00000000,             /* le ieee s +zero */
        0x00000000,             /* be ieee s +zero */
        0x80000000,             /* le ieee s -zero */
        0x00000080,             /* be ieee s -zero */
        0x7f7fffff,             /* le ieee s +huge */
        0xffff7f7f,             /* be ieee s +huge */
        0xff7fffff,             /* le ieee s -huge */
        0xffff7fff,             /* be ieee s -huge */
        0x7f800000,             /* le ieee s +infinity */
        0x0000807f,             /* be ieee s +infinity */
        0xff800000,             /* le ieee s -infinity */
        0x000080ff,             /* be ieee s -infinity */

};

idl_ulong_int ieee_t[] = {

        0xffffffff, 0x7ff7ffff,         /* le ieee t nan */
        0xfffff77f, 0xffffffff,         /* be ieee t nan */
        0x00000000, 0x00000000,         /* le ieee t +zero */
        0x00000000, 0x00000000,         /* be ieee t +zero */
        0x00000000, 0x80000000,         /* le ieee t -zero */
        0x00000080, 0x00000000,         /* be ieee t -zero */
        0xffffffff, 0x7fefffff,         /* le ieee s +huge */
        0xffffef7f, 0xffffffff,         /* be ieee s +huge */
        0xffffffff, 0xffefffff,         /* le ieee s -huge */
        0xffffefff, 0xffffffff,         /* be ieee s -huge */
        0x00000000, 0x7ff00000,         /* le ieee t +infinity */
        0x0000f07f, 0x00000000,         /* be ieee t +infinity */
        0x00000000, 0xfff00000,         /* le ieee t -infinity */
        0x0000f0ff, 0x00000000,         /* be ieee t -infinity */

};

idl_ulong_int ibm_s[] = {

   0x000000ff,          /* ibm s invalid */
   0x00000000,          /* ibm s +zero */
   0x00000080,          /* ibm s -zero */
   0xffffff7f,          /* ibm s +huge */
   0xffffffff,          /* ibm s -huge */
   0xffffff7f,          /* ibm s +infinity */
   0xffffffff,          /* ibm s -infinity */

};

idl_ulong_int ibm_l[] = {

   0x000000ff, 0x00000000,              /* ibm t invalid */
   0x00000000, 0x00000000,              /* ibm t +zero */
   0x00000080, 0x00000000,              /* ibm t -zero */
   0xffffff7f, 0xffffffff,              /* ibm t +huge */
   0xffffffff, 0xffffffff,              /* ibm t -huge */
   0xffffff7f, 0xffffffff,              /* ibm t +infinity */
   0xffffffff, 0xffffffff,              /* ibm t -infinity */

};

idl_ulong_int cray[] = {

        0x00000060, 0x00000000,         /* cray invalid */
        0x00000000, 0x00000000,         /* cray +zero */
        0x00000080, 0x00000000,         /* cray -zero */
        0xffffff5f, 0xffffffff,         /* cray +huge */
        0xffffffdf, 0xffffffff,         /* cray -huge */
        0x00000060, 0x00000000,         /* cray +infinity */
        0x000000e0, 0x00000000,         /* cray -infinity */

};

idl_ulong_int int_c[] = {

        0x00000000,             /* le int nan */
        0x00000000,             /* be int nan */
        0x00000000,             /* le int zero */
        0x00000000,             /* be int zero */
        0x7fffffff,             /* le int +huge */
        0xffffff7f,             /* be int +huge */
        0x80000000,             /* le int -huge */
        0x00000080,             /* be int -huge */
        0x7fffffff,             /* le int +infinity */
        0xffffff7f,             /* be int +infinity */
        0x80000000,             /* le int -infinity */
        0x00000080,             /* be int -infinity */

};

#else /* Big-endian Data */

idl_ulong_int vax_c[] = {

        0x00800000, 0x00000000, 0x00000000, 0x00000000,         /* ROPs */
        0x00000000, 0x00000000, 0x00000000, 0x00000000,         /* zeros */
        0xff7fffff, 0xffffffff, 0xffffffff, 0xffffffff,         /* +huge */
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,         /* -huge */

};

idl_ulong_int ieee_s[] = {

        0xffffbf7f,             /* little endian ieee s nan */
        0x7fbfffff,             /* big endian ieee s nan */
        0x00000000,             /* le ieee s +zero */
        0x00000000,             /* be ieee s +zero */
        0x00000080,             /* le ieee s -zero */
        0x80000000,             /* be ieee s -zero */
        0xffff7f7f,             /* le ieee s +huge */
        0x7f7fffff,             /* be ieee s +huge */
        0xffff7fff,             /* le ieee s -huge */
        0xff7fffff,             /* be ieee s -huge */
        0x0000807f,             /* le ieee s +infinity */
        0x7f800000,             /* be ieee s +infinity */
        0x000080ff,             /* le ieee s -infinity */
        0xff800000,             /* be ieee s -infinity */

};

idl_ulong_int ieee_t[] = {

        0xffffffff, 0xfffff77f,         /* le ieee t nan */
        0x7ff7ffff, 0xffffffff,         /* be ieee t nan */
        0x00000000, 0x00000000,         /* le ieee t +zero */
        0x00000000, 0x00000000,         /* be ieee t +zero */
        0x00000000, 0x00000080,         /* le ieee t -zero */
        0x80000000, 0x00000000,         /* be ieee t -zero */
        0xffffffff, 0xffffef7f,         /* le ieee s +huge */
        0x7fefffff, 0xffffffff,         /* be ieee s +huge */
        0xffffffff, 0xffffefff,         /* le ieee s -huge */
        0xffefffff, 0xffffffff,         /* be ieee s -huge */
        0x00000000, 0x0000f07f,         /* le ieee t +infinity */
        0x7ff00000, 0x00000000,         /* be ieee t +infinity */
        0x00000000, 0x0000f0ff,         /* le ieee t -infinity */
        0xfff00000, 0x00000000,         /* be ieee t -infinity */

};

idl_ulong_int ibm_s[] = {

   0xff000000,          /* ibm s invalid */
   0x00000000,          /* ibm s +zero */
   0x80000000,          /* ibm s -zero */
   0x7fffffff,          /* ibm s +huge */
   0xffffffff,          /* ibm s -huge */
   0x7fffffff,          /* ibm s +infinity */
   0xffffffff,          /* ibm s -infinity */

};

idl_ulong_int ibm_l[] = {

   0xff000000, 0x00000000,              /* ibm t invalid */
   0x00000000, 0x00000000,              /* ibm t +zero */
   0x80000000, 0x00000000,              /* ibm t -zero */
   0x7fffffff, 0xffffffff,              /* ibm t +huge */
   0xffffffff, 0xffffffff,              /* ibm t -huge */
   0x7fffffff, 0xffffffff,              /* ibm t +infinity */
   0xffffffff, 0xffffffff,              /* ibm t -infinity */

};

idl_ulong_int cray[] = {

        0x60000000, 0x00000000,         /* cray invalid */
        0x00000000, 0x00000000,         /* cray +zero */
        0x80000000, 0x00000000,         /* cray -zero */
        0x5fffffff, 0xffffffff,         /* cray +huge */
        0xdfffffff, 0xffffffff,         /* cray -huge */
        0x60000000, 0x00000000,         /* cray +infinity */
        0xe0000000, 0x00000000,         /* cray -infinity */

};

idl_ulong_int int_c[] = {

        0x00000000,             /* le int nan */
        0x00000000,             /* be int nan */
        0x00000000,             /* le int zero */
        0x00000000,             /* be int zero */
        0xffffff7f,             /* le int +huge */
        0x7fffffff,             /* be int +huge */
        0x00000080,             /* le int -huge */
        0x80000000,             /* be int -huge */
        0xffffff7f,             /* le int +infinity */
        0x7fffffff,             /* be int +infinity */
        0x00000080,             /* le int -infinity */
        0x80000000,             /* be int -infinity */

};

#endif /* Big-Endian Data */
