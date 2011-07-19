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
**      ifspemts.c
**
**  FACILITY:
**
**      IDL Compiler Backend
**
**  ABSTRACT:
**
**  Routines for the emission of ifspecs, including MIA transfer syntaxes
**
**
*/

#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <command.h>
#include <ifspec.h>
#include <bedeck.h>
#include <dutils.h>
#include <mtsbacke.h>

/******************************************************************************/
/*                                                                            */
/*    Return a pointer to the name of the if_spec                             */
/*                                                                            */
/******************************************************************************/
static char *BE_ifspec_name
(
    AST_interface_n_t *ifp,
    BE_output_k_t kind
)
{
    static char retval[100];

    sprintf(retval, "%s_v%ld_%ld_%c_ifspec", BE_get_name(ifp->name),
            (ifp->version%65536), (ifp->version/65536),
            kind == BE_client_stub_k ? 'c' : 's');

    return retval;
}

/******************************************************************************/
/*                                                                            */
/*    Spell the manager epv to the output stream                              */
/*                                                                            */
/******************************************************************************/
void CSPELL_manager_epv
(
    FILE *fid,
    AST_interface_n_t *ifp
)
{
    AST_export_n_t *p_export;
    boolean first_op = true;

    fprintf( fid, "\nstatic %s_v%ld_%ld_epv_t IDL_manager_epv = {\n",
           BE_get_name(ifp->name), (ifp->version%65536), (ifp->version/65536) );

    for( p_export = ifp->exports; p_export != NULL; p_export = p_export->next )
    {
        if (p_export->kind == AST_operation_k)
        {
            if ( ! first_op ) fprintf( fid, "," );
            spell_name( fid, p_export->thing_p.exported_operation->name );
            fprintf( fid, "\n" );
            first_op = false;
        }
    }

    fprintf( fid,"};\n" );
}

/******************************************************************************/
/*                                                                            */
/*    Spell an interface definition and related material to the output stream */
/*                                                                            */
/******************************************************************************/
void CSPELL_interface_def
(
    FILE *fid,
    AST_interface_n_t *ifp,
    BE_output_k_t kind,
    boolean generate_mepv
)
{
    boolean     first;
    long        i, endpoints;
    char const *protseq, *portspec;

    if ((endpoints = ifp->number_of_ports) != 0)
    {
        first = true;
        fprintf(fid,
                "static rpc_endpoint_vector_elt_t IDL_endpoints[%ld] = \n{",
                endpoints);
        for (i = 0; i < endpoints; i++)
        {
            STRTAB_str_to_string(ifp->protocol[i], &protseq);
            STRTAB_str_to_string(ifp->endpoints[i], &portspec);
            if (!first) fprintf(fid, ",\n");
            fprintf(fid,
               "{(unsigned_char_p_t)\"%s\", (unsigned_char_p_t)\"%s\"}",
                   protseq, portspec);
            first = false;
        }
        fprintf(fid, "};\n");
    }

    /* Transfer syntaxes */
    fprintf( fid, "\nstatic rpc_syntax_id_t IDL_transfer_syntaxes[%d] = {\n{\n",
                                                                 1 );
        fprintf(fid,
"{0x8a885d04u, 0x1ceb, 0x11c9, 0x9f, 0xe8, {0x8, 0x0, 0x2b, 0x10, 0x48, 0x60}},");
        fprintf(fid, "\n2}");
    fprintf(fid, "};\n");

    fprintf(fid, "\nstatic rpc_if_rep_t IDL_ifspec = {\n");
    fprintf(fid, "  1, /* ifspec rep version */\n");
    fprintf(fid, "  %d, /* op count */\n", ifp->op_count);
    fprintf(fid, "  %ld, /* if version */\n", ifp->version);
    fprintf(fid, "  {");
    fprintf(fid, "0x%8.8lxu, ", ifp->uuid.time_low);
    fprintf(fid, "0x%4.4x, ", ifp->uuid.time_mid);
    fprintf(fid, "0x%4.4x, ", ifp->uuid.time_hi_and_version);
    fprintf(fid, "0x%2.2x, ", ifp->uuid.clock_seq_hi_and_reserved);
    fprintf(fid, "0x%2.2x, ", ifp->uuid.clock_seq_low);
    fprintf(fid, "{");
    first = true;
    for (i = 0; i < 6; i++)
    {
        if (first)
            first = false;
        else
            fprintf (fid, ", ");
        fprintf (fid, "0x%x", ifp->uuid.node[i]);
    }
    fprintf(fid, "}},\n");
    fprintf(fid, "  2, /* stub/rt if version */\n");
    fprintf(fid, "  {%ld, %s}, /* endpoint vector */\n", endpoints,
                 endpoints ? "IDL_endpoints" : "NULL");
    fprintf(fid, "  {%d, IDL_transfer_syntaxes} /* syntax vector */\n",
                                                                 1 );

    if (kind == BE_server_stub_k)
    {
        fprintf(fid, ",IDL_epva /* server_epv */\n");
        if (generate_mepv)
        {
            fprintf(fid,",(rpc_mgr_epv_t)&IDL_manager_epv /* manager epv */\n");
        }
        else
        {
            fprintf(fid,",NULL /* manager epv */\n");
        }
    }
    else
    {
        fprintf(fid,",NULL /* server epv */\n");
        fprintf(fid,",NULL /* manager epv */\n");
    }
    fprintf(fid, "};\n");
    fprintf(fid,
            "/* global */ rpc_if_handle_t %s = (rpc_if_handle_t)&IDL_ifspec;\n",
            BE_ifspec_name(ifp, kind));
}
