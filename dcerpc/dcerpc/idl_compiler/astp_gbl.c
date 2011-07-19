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
**      ASTP_BLD_GLOBALS.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Defines global variables used by the parser and abstract
**      sytax tree (AST) builder modules.
**
**  VERSION: DCE 1.0
**
*/

#include <nidl.h>
#include <astp.h>

/*
 * External variables defined here, exported in ASTP.H
 * Theses externals are shared between the AST builder modules
 */

/*
 *  Interface Attributes
 */

/*
 *  Operation, Parameter, Type Attributes
 */

AST_type_n_t        *ASTP_transmit_as_type = NULL;
AST_type_n_t        *ASTP_switch_type = NULL;
AST_case_label_n_t  *ASTP_case = NULL;

/*
 *  Interface just parsed
 */
AST_interface_n_t *the_interface = NULL;

//centeris wfu
AST_cpp_quote_n_t *global_cppquotes = NULL;

AST_cpp_quote_n_t *global_cppquotes_post = NULL;

AST_import_n_t *global_imports = NULL;

/*
 * List head for saved context for field
 * attributes forward referenced parameters.
 */
ASTP_field_ref_ctx_t *ASTP_field_ref_ctx_list = NULL;

/*
 * List head for referenced struct/union tags.
 */
ASTP_tag_ref_n_t *ASTP_tag_ref_list = NULL;

/*
 *  Control for parser
 */
boolean ASTP_parsing_main_idl = TRUE;

/*
 *  Builtin in constants
 */

AST_constant_n_t    *zero_constant_p = NULL;

/*
 * Builtin base types
 */
AST_type_n_t    *ASTP_char_ptr = NULL,
                *ASTP_boolean_ptr = NULL,
                *ASTP_byte_ptr = NULL,
                *ASTP_void_ptr = NULL,
                *ASTP_handle_ptr = NULL,
                *ASTP_short_float_ptr = NULL,
                *ASTP_long_float_ptr = NULL,
                *ASTP_small_int_ptr = NULL,
                *ASTP_short_int_ptr = NULL,
                *ASTP_long_int_ptr = NULL,
                *ASTP_hyper_int_ptr = NULL,
                *ASTP_small_unsigned_ptr = NULL,
                *ASTP_short_unsigned_ptr = NULL,
                *ASTP_long_unsigned_ptr = NULL,
                *ASTP_hyper_unsigned_ptr = NULL;

/* Default tag for union */
NAMETABLE_id_t  ASTP_tagged_union_id;
