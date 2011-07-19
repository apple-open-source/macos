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
**      user_exc.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Code generation for user exceptions
**
**
*/

#include <ast.h>
#include <be_pvt.h>
#include <cspell.h>
#include <user_exc.h>

/******************************************************************************/
/*                                                                            */
/*  Declare user exceptions                                                   */
/*                                                                            */
/******************************************************************************/
static void DDBE_list_exceptions
(
    FILE *fid,                      /* [in] Handle for emitted C text */
    AST_interface_n_t *p_interface, /* [in] Pointer to AST interface node */
    int *p_num_declared_exceptions, /* [out] Number of declared exceptions */
    int *p_num_extern_exceptions    /* [out] Number of external exceptions */
)
{
    AST_exception_n_t *p_exception;

    *p_num_declared_exceptions = 0;
    *p_num_extern_exceptions = 0;

    for (p_exception = p_interface->exceptions;
         p_exception != NULL;
         p_exception = p_exception->next)
    {
        if (AST_EXTERN_SET(p_exception))
        {
            fprintf(fid, "extern ");
            (*p_num_extern_exceptions)++;
        }
        else
            (*p_num_declared_exceptions)++;
        fprintf(fid, "EXCEPTION ");
        spell_name(fid, p_exception->name);
        fprintf(fid, ";\n");
    }
}

/******************************************************************************/
/*                                                                            */
/*  Spell code to initialize declared exceptions                              */
/*                                                                            */
/******************************************************************************/
static void DDBE_init_exceptions
(
    FILE *fid,                      /* [in] Handle for emitted C text */
    AST_interface_n_t *p_interface  /* [in] Pointer to AST interface node */
)
{
    AST_exception_n_t *p_exception;

    fprintf(fid, "static void IDL_exceptions_init()\n{\n");
    for (p_exception = p_interface->exceptions;
         p_exception != NULL;
         p_exception = p_exception->next)
    {
        if ( ! AST_EXTERN_SET(p_exception))
        {
            fprintf(fid, "EXCEPTION_INIT(");
            spell_name(fid, p_exception->name);
            fprintf(fid, ");\n");
        }
    }
    fprintf(fid, "}\n");
    fprintf( fid,
"static RPC_SS_THREADS_ONCE_T IDL_exception_once = RPC_SS_THREADS_ONCE_INIT;\n"
             );
}

/******************************************************************************/
/*                                                                            */
/*  Spell an array of pointers to the user exceptions                         */
/*                                                                            */
/******************************************************************************/
static void DDBE_ref_exception_array
(
    FILE *fid,                      /* [in] Handle for emitted C text */
    AST_interface_n_t *p_interface  /* [in] Pointer to AST interface node */
)
{
    AST_exception_n_t *p_exception;
    boolean first = true;

    fprintf(fid, "static EXCEPTION *IDL_exception_addresses[] = {\n");
    for (p_exception = p_interface->exceptions;
         p_exception != NULL;
         p_exception = p_exception->next)
    {
        if (first)
            first = false;
        else
            fprintf(fid, ",\n");
        fprintf(fid, "&");
        spell_name(fid, p_exception->name);
    }
    fprintf(fid, "};\n");
}

/******************************************************************************/
/*                                                                            */
/*  Declare user exception machinery at start of stub                         */
/*                                                                            */
/******************************************************************************/
void DDBE_user_exceptions
(
    FILE *fid,                      /* [in] Handle for emitted C text */
    AST_interface_n_t *p_interface, /* [in] Pointer to AST interface node */
    int *p_num_declared_exceptions, /* [out] Number of declared exceptions */
    int *p_num_extern_exceptions    /* [out] Number of external exceptions */
)
{
    DDBE_list_exceptions(fid, p_interface, p_num_declared_exceptions,
                         p_num_extern_exceptions);
    if (*p_num_declared_exceptions != 0)
        DDBE_init_exceptions(fid, p_interface);
    else if (*p_num_extern_exceptions == 0)
    {
        /* No exception machinery to set up */
        return;
    }
    DDBE_ref_exception_array(fid, p_interface);
}
