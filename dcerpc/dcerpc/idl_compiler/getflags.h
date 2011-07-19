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
**  NAME
**
**      GETFLAGS.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Constants for command line parsing
**
**  VERSION: DCE 1.0
**
*/

#ifndef GETFLAGS_H
#define GETFLAGS_H

#include <nidl.h>

typedef char *FLAGDEST;
typedef struct options
        {
        const char *option;
        int ftype;
        FLAGDEST dest;
        } OPTIONS;

/*
 * Rico, 23-Mar-90: The format of the ftype field appears to be:
 *
 *      Bit 15:   (1 bit)  HIDARG flag - flags hidden args to printflags fn
 *      Bit 14:   (1 bit)  VARARG flag - flags a variable option (can repeat)
 *      Bit 13-8: (6 bits) maximum number of occurences of a variable option
 *      Bit  7-0: (8 bits) argument type
 *
 *       15  14  13              8   7                    0
 *      -----------------------------------------------------
 *      | H | V |  max_occurences  |     argument_type      |
 *      -----------------------------------------------------
 */

/* Argument types */
#define INTARG      0
#define STRARG      1
#define TOGGLEARG   2
#define CHRARG      3
#define FLTARG      4
#define LONGARG     5
#define ASSERTARG   6
#define DENYARG     7
#define OSTRARG     8           /* Optional string arg, added 23-Mar-90 */

#define HIDARG (128 << 8)       /* H bit */
#define VARARGFLAG 64           /* V bit - gets shifted 8 bits by macros */
#define MULTARGMASK 63          /* Mask to get max_occurences */

/* Macros for specifying ftype */
#define MULTARG(n, a) (((n) << 8) + a)
#define AINTARG(n) MULTARG(n,INTARG)
#define VINTARG(n) AINTARG(n|VARARGFLAG)
#define ASTRARG(n) MULTARG(n,STRARG)
#define VSTRARG(n) ASTRARG(n|VARARGFLAG)
#define ATOGGLEARG(n) MULTARG(n,TOGGLEARG)
#define AASSERTARG(n) MULTARG(n,ASSERTARG)
#define ADENYARG(n) MULTARG(n,DENYARG)
#define ACHRARG(n) MULTARG(n,CHRARG)
#define VCHRARG(n) ACHRARG(n|VARARGFLAG)
#define AFLTARG(n) MULTARG(n,FLTARG)
#define VFLTARG(n) AFLTARG(n|VARARGFLAG)
#define ALONGARG(n) MULTARG(n,LONGARG)
#define VLONGARG(n) AFLTARG(n|VARARGFLAG)

/* Macros for converting command line arguments */
#define GETINT(s) {s = atoi(*++av); ac--;}
#define GETSTR(s) {s = *++av;ac--;}
#define GETCH(s) {av++; s = av[0][0]; ac--;}
#define GETFLT(s) {s = atof(*++av); ac--;}
#define GETLONG(s) {s = atol(*++av); ac--;}

void printflags (
    const OPTIONS table[]
);

void getflags (
    int argc,
    char **argv,
    const OPTIONS table[]
);

void flags_incr_count (
    const OPTIONS table[],
    const char *option,
    int delta
);

int flags_option_count (
    const OPTIONS table[],
    const char *option
);

int flags_other_count (
    void
);

char *flags_other (
    int index
);

#endif
