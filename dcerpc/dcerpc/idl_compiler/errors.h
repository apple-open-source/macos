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
**      ERRORS.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**
**
**  VERSION: DCE 1.0
**
*/

#ifndef ERRORS_H
#define ERRORS_H

#include <errno.h>
#include <nidl.h>
#include <nametbl.h>

#define IDL_ERROR_LIST_SIZE 5

/* An opaque pointer. */
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

void error
(
	long msg_id,
	...
);

void warning
(
	long msg_id,
	...
);

void vlog_source_error
(
 STRTAB_str_t filename,
 int lineno,
 long msg_id,
 va_list ap
);

void log_source_error
(
	/* it is not a nonsense */
	STRTAB_str_t filename,
	int lineno,
	long msg_id,
	... /* 0..5 args terminated by NULL if less than five */
);

void vlog_source_warning
(
 STRTAB_str_t filename,
 int lineno,
 long msg_id,
 va_list ap
 );

void log_source_warning
(
	/* it is not a nonsense */
	STRTAB_str_t filename,
	int lineno,
	long msg_id,
	... /* 0..5 args terminated by NULL if less than five */
);

void vlog_error
(
 /* it is not a nonsense */
 int lineno, /* Source line number */
 long msg_id, /* Message ID */
 va_list ap
);

void log_error
(
	/* it is not a nonsense */
	int lineno, /* Source line number */
	long msg_id, /* Message ID */
	... /* 0..5 args terminated by NULL if less than five */
);

void vlog_warning
(
 /* it is not a nonsense */
 int lineno, /* Source line number */
 long msg_id, /* Message ID */
 va_list ap
);

void log_warning
(
	/* it is not a nonsense */
	int lineno, /* Source line number */
	long msg_id, /* Message ID */
	... /* 0..5 args terminated by NULL if less than five */
);

typedef struct {
    long msg_id;
    const void* arg[IDL_ERROR_LIST_SIZE];
} idl_error_list_t;

typedef idl_error_list_t *idl_error_list_p;

void error_list
(
    int vecsize,
    idl_error_list_p errvec,
    boolean exitflag
);

void inq_name_for_errors
(
    char *name,
	size_t name_len
);

void set_name_for_errors
(
    char const *name
);

boolean print_errors
(
    void
);

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
    int first_line;
    int first_column;
    int last_line;
    int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

struct parser_location_t;

void idl_yyerror
(
    const struct parser_location_t * location,
    char const * message
);

void yywhere
(
    const struct parser_location_t * location
);

/*
 * Error info to be fillin the fe_info nodes
 */
extern int          error_count;

/* XXX These globals are set and unset in the NIDL and ACF parsers in
 * xxx_input() and xxx_parser_destroy(). They should be more-or-less
 * accurate, but it would be better to continue plumbing the
 * parser_location_t down into error() and warning() so that we can
 * get rid of this ugliness.
 */

extern FILE    *yyin_p;           /* Points to yyin or acf_yyin */
extern unsigned*yylineno_p;       /* Points to yylineno or acf_yylineno */

#ifdef DUMPERS
#define INTERNAL_ERROR(string) {printf("Internal Error Diagnostic: %s\n",string);warning(NIDL_INTERNAL_ERROR,__FILE__,__LINE__);}
#else
#define INTERNAL_ERROR(string) {error(NIDL_INTERNAL_ERROR,__FILE__,__LINE__); printf(string);}
#endif
#endif
/* preserve coding style vim: set tw=78 sw=4 : */
