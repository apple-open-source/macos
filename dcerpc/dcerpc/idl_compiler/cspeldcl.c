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
**  NAME:
**
**      cspeldcl.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines to spell declaration related material to C source
**
**  VERSION: DCE 1.0
*/

#include <nidl.h>
#include <ast.h>
#include <bedeck.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <hdgen.h>

/******************************************************************************/
/*                                                                            */
/*    Build text string for constant value                                    */
/*                                                                            */
/******************************************************************************/
void CSPELL_constant_val_to_string
(
    AST_constant_n_t *cp,
    char *str
)
{
    char const *str2;

    switch (cp->kind) {
        case AST_nil_const_k:
            sprintf (str, "NULL");
            break;
        case AST_boolean_const_k:
            if (cp->value.boolean_val)
                sprintf (str, "ndr_true");
            else
                sprintf (str, "ndr_false");
            break;
        case AST_int_const_k:
            sprintf (str, "%ld", cp->value.int_val);
            break;
        case AST_string_const_k:
            STRTAB_str_to_string (cp->value.string_val, &str2);
            sprintf (str, "\"%s\"", str2);
            break;
        case AST_char_const_k:
            sprintf (str, "'%s'", mapchar(cp, FALSE));
            break;
        default:
            INTERNAL_ERROR("Unsupported tag in CSPELL_constant_val_to_string");
            break;
        }
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell constant to C source                                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_constant_val
(
    FILE *fid,
    AST_constant_n_t *cp
)
{
    char str[max_string_len];

    CSPELL_constant_val_to_string (cp, str);
    fprintf (fid, "%s", str);
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell union case label comment to C source                   */
/*                                                                            */
/******************************************************************************/
void CSPELL_labels
(
    FILE *fid,
    AST_case_label_n_t *clp
)
{
    boolean first = true;

    fprintf (fid, "/* case(s): ");
    for (; clp; clp = clp->next) {
        if (first)
            first = false;
        else
            fprintf (fid, ", ");
        if (clp->default_label)
            fprintf (fid, "default");
        else
            CSPELL_constant_val (fid, clp->value);
        };
    fprintf (fid, " */\n");
}

/******************************************************************************/
/*                                                                            */
/*    Routine to spell function parameter list to C source                    */
/*                                                                            */
/******************************************************************************/
void CSPELL_parameter_list
(
    FILE *fid,
    AST_parameter_n_t *pp,
    boolean encoding_services   /* TRUE => [encode] or [decode] on operation */
)
{
    boolean            first = true;

    if (pp)
    {
        for (; pp; pp = pp->next)
        {
            if (AST_HIDDEN_SET(pp))
            {
                /* Parameter does not appear in signature delivered to user */
                continue;
            }
            if (first)
            {
                first = false;
                if (encoding_services)
                {
                    /* First parameter is a pickling handle */
                    fprintf(fid, "    /* [in] */ idl_es_handle_t ");
                    if (pp->type->kind == AST_pointer_k)
                    {
                        /* Passed by reference */
                        fprintf(fid, "*");
                    }
                    spell_name(fid, pp->name);
                    continue;
                }
            }
            else
                fprintf (fid, ",\n");
            fprintf (fid, "    /* [");
            if (AST_IN_SET(pp))
            {
                fprintf(fid, "in");
                if (AST_OUT_SET(pp)) fprintf (fid, ", out");
            }
            else fprintf  (fid, "out");
            fprintf (fid, "] */ ");
#ifndef MIA
            if (pp->be_info.param)
                CSPELL_typed_name (fid, pp->type,
                    pp->be_info.param->name, NULL, false, true, false);
            else
#endif
                 CSPELL_typed_name (fid, pp->type, pp->name, NULL, false, true,
                                    false);
        }
    }
    else
        fprintf (fid, "    void");
}

/******************************************************************************/
/*                                                                            */
/*    Spell new and old style parameter lists                                 */
/*                                                                            */
/******************************************************************************/
void CSPELL_finish_synopsis
(
    FILE *fid ATTRIBUTE_UNUSED,
    AST_parameter_n_t *paramlist ATTRIBUTE_UNUSED
)
{
    /* this function used to put in the function parameters
     when prototypes were NOT being used.  Since we now
     always use function prototypes, this function now does
     nothing */
}
