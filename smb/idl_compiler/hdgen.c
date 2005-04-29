/*
 * 
 * (c) Copyright 1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1993 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
**
**  NAME:
**
**      hdgen.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      BE_gen_c_header which emits the C header file for an interface.
**
**  VERSION: DCE 1.0
**
**
*/

#include <nidl.h>
#include <ast.h>
#include <cspell.h>
#include <cspeldcl.h>
#include <files.h>
#include <genpipes.h>
#include <hdgen.h>
#include <dutils.h>

extern int yylineno;

/*
 * mapchar
 *
 * Maps a single character into a string suitable for emission
 */
char *mapchar
(
    AST_constant_n_t *cp,   /* Constant node with kind == AST_char_const_k */
    boolean warning_flag    /* unused */
)
{
    char c = cp->value.char_val;
    static char buf[10];

    switch (c)
    {
        case AUDIBLE_BELL: return AUDIBLE_BELL_CSTR;
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\v': return "\\v";
        case '\\': return "\\\\";
        case '\'': return "\\\'";
        case '\"': return "\\\"";
        default:
            if (c >= ' ' && c <= '~')
                sprintf(buf, "%c", c);
            else
                sprintf(buf, "\\%03o", c);
            return buf;
    }
}

static void CSPELL_constant_def
(
    FILE *fid,
    AST_constant_n_t *cp,
    char *cast
)
{
    char *s;

    fprintf (fid, "#define ");
    spell_name (fid, cp->name);
    fprintf (fid, " %s", cast);
    if (cp->defined_as != NULL)
        spell_name (fid, cp->defined_as->name);
    else
        switch (cp->kind) {
            case AST_nil_const_k:
                fprintf (fid, "NULL");
                break;
            case AST_int_const_k:
                fprintf (fid, "(%ld)", cp->value.int_val);
                break;
            case AST_hyper_int_const_k:
                fprintf (fid, "{%ld,%lu}",
                        cp->value.hyper_int_val.high,
                        cp->value.hyper_int_val.low);
                break;
            case AST_char_const_k:
                fprintf (fid, "'%s'", mapchar(cp, TRUE));
                break;
            case AST_string_const_k:
                STRTAB_str_to_string (cp->value.string_val, &s);
                fprintf (fid, "\"%s\"", s);
                break;
            case AST_boolean_const_k:
                fprintf (fid, "%s",
                         cp->value.boolean_val ? "ndr_true" : "ndr_false");
                break;
        }
    fprintf (fid, "\n");
}


static void CSPELL_operation_def
(
    FILE *fid,
    AST_operation_n_t *op
)
{
    AST_type_n_t       func_type_node;

    func_type_node = *BE_function_p;
    func_type_node.type_structure.function = op;

    fprintf (fid, "extern ");
    CSPELL_typed_name (fid, &func_type_node, op->name, NULL, false, true,
                        (AST_ENCODE_SET(op) || AST_DECODE_SET(op)));
    fprintf (fid, ";\n");
}


void CSPELL_type_def
(
    FILE *fid,
    AST_type_n_t *tp,
    boolean spell_tag
)
{
    fprintf (fid, "typedef ");
    CSPELL_typed_name (fid, tp, tp->name, tp, false, spell_tag, false);
    fprintf (fid, ";\n");

    /* declare the "bind" and "unbind" routines as extern's for [handle] types */
    if (AST_HANDLE_SET(tp) && !((tp->kind == AST_handle_k)
                                && (tp->xmit_as_type == NULL)))
    {
        fprintf (fid, "handle_t ");
        spell_name (fid, tp->name);
        fprintf (fid, "_bind(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " h\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_unbind(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " uh,\nhandle_t h\nn);\n");
    }
}


static void CSPELL_exports
(
    FILE *fid,
    AST_export_n_t *ep
)
{
    for (; ep; ep = ep->next) {
        switch (ep->kind) {
            case AST_constant_k:
                CSPELL_constant_def (fid, ep->thing_p.exported_constant, "");
                break;
            case AST_operation_k:
                    CSPELL_operation_def (fid, ep->thing_p.exported_operation);
                break;
            case AST_type_k:
                CSPELL_type_def (fid, ep->thing_p.exported_type, true);
                break;
            default:
               INTERNAL_ERROR( "Unknown export type in CSPELL_exports" );
               break;
            }
        }
}

static void CSPELL_epv_field
(
    FILE *fid,
    AST_operation_n_t *op
)
{
    AST_type_n_t       type_node_a, type_node_b;
    AST_pointer_n_t    pointer_node;

    type_node_a = *BE_pointer_p;
    type_node_a.type_structure.pointer = &pointer_node;

    type_node_b = *BE_function_p;
    type_node_b.type_structure.function = op;

    pointer_node.fe_info = NULL;
    pointer_node.be_info.other = NULL;
    pointer_node.pointee_type = &type_node_b;

    CSPELL_typed_name (fid, &type_node_a, op->name, NULL, false, true,
                        (AST_ENCODE_SET(op) || AST_DECODE_SET(op)));
    fprintf (fid, ";\n");
}


static void CSPELL_epv_type_and_var
(
    FILE *fid,
    NAMETABLE_id_t if_name,
    unsigned int if_version,
    AST_export_n_t *ep,
    boolean declare_cepv
)
{

    /*  emit the declaration of the client/manager EPV type
        and, conditional on declare_cepv, an extern declaration
        for the client EPV
    */

    AST_operation_n_t *op;

    fprintf (fid, "typedef struct ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_epv_t {\n", (if_version%65536), (if_version/65536));
    for (; ep; ep = ep->next)
        if (ep->kind == AST_operation_k) {
            op = ep->thing_p.exported_operation;
            CSPELL_epv_field (fid, op);
        }
    fprintf (fid, "} ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_epv_t;\n", (if_version%65536), (if_version/65536));

    if (declare_cepv) {
        fprintf(fid, "extern ");
        spell_name(fid, if_name);
        fprintf(fid, "_v%d_%d_epv_t ", if_version%65536, if_version/65536);
        spell_name(fid, if_name);
        fprintf(fid, "_v%d_%d_c_epv;\n", if_version%65536, if_version/65536);
    }

}

static void CSPELL_if_spec_refs
(
    FILE *fid,
    NAMETABLE_id_t if_name,
    unsigned int if_version
)
{
    fprintf (fid, "extern rpc_if_handle_t ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_c_ifspec;\n",(if_version%65536),(if_version/65536));

    fprintf (fid, "extern rpc_if_handle_t ");
    spell_name (fid, if_name);
    fprintf (fid, "_v%d_%d_s_ifspec;\n",(if_version%65536),(if_version/65536));
}

static void CSPELL_user_prototypes
(
    FILE *fid,
    AST_interface_n_t *ifp
)
{
    AST_export_n_t *ep;
    AST_type_p_n_t *tpp;
    AST_type_n_t *tp;

    /*
     * declare context handle rundown routines
     */
    for (ep = ifp->exports; ep; ep = ep->next)
    {
        if (ep->kind != AST_type_k)
            continue;
        tp = ep->thing_p.exported_type;
        if (!AST_CONTEXT_RD_SET(tp))
            continue;

        fprintf(fid, "void ");
        spell_name(fid, tp->name);
        fprintf(fid, "_rundown(\n");
        fprintf(fid, "    rpc_ss_context_t context_handle\n);\n");
    }

    /*
     * declare the "from_xmit", "to_xmit", "free_xmit", and "free"
     * routines as extern's for types with the [transmit_as()] attribute
     */
    for (ep = ifp->exports; ep; ep = ep->next)
    {
        if (ep->kind != AST_type_k)
            continue;
        tp = ep->thing_p.exported_type;
        if (tp->xmit_as_type == NULL)
            continue;

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_from_xmit(\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " *xmit_object,\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_to_xmit(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object,\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " **xmit_object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_inst(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_xmit(\n");
        CSPELL_type_exp_simple (fid, tp->xmit_as_type);
        fprintf (fid, " *xmit_object\n);\n");
    }

    /*
     * declare the "from_local", "to_local", "free_local", and
     * "free" routines as extern's for types with the [represent_as()]
     * attribute
     */
    for (tpp = ifp->ra_types; tpp; tpp = tpp->next)
    {
        tp = tpp->type;

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_from_local(\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object,\n");
        spell_name (fid, tp->name);
        fprintf (fid, " **net_object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_to_local(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *net_object,\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_local(\n");
        spell_name (fid, tp->rep_as_type->type_name);
        fprintf (fid, " *local_object\n);\n");

        fprintf (fid, "void ");
        spell_name (fid, tp->name);
        fprintf (fid, "_free_inst(\n");
        spell_name (fid, tp->name);
        fprintf (fid, " *net_object\n);\n");
    }

    /*
     * Declare the binding handle callout routine specified with the
     * [binding_callout] interface attribute, if any.
     */
    if (ifp->binding_callout_name != NAMETABLE_NIL_ID)
    {
        char *callout_name;
        NAMETABLE_id_to_string(ifp->binding_callout_name, &callout_name);
        /* Don't emit proto for canned routines declared in stubbase.h */
        if (strncmp(callout_name, "rpc_ss_bind_", 12/*prefix len*/) != 0)
        {
            fprintf(fid, "void ");
            spell_name(fid, ifp->binding_callout_name);
            fprintf(fid, "(\n");
            fprintf(fid, "rpc_binding_handle_t *p_binding,\n");
            fprintf(fid, "rpc_if_handle_t interface_handle,\n");
            fprintf(fid, "error_status_t *p_st\n);\n");
        }

    }
}

/*
 *  Spell "extern" statements for user exceptions
 */
void BE_spell_extern_user_excs
(
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *ifp /* Ptr to AST interface node */
)
{
    AST_exception_n_t *p_exception;

    if (ifp->exceptions == NULL)
    {
        /* There are no user exceptions */
        return;
    }

    /* There is at least one user exception, so drag in some exception
        handling machinery */
#ifdef ultrix
    fprintf (fid, INCLUDE_TEMPLATE, "cma.h");
#endif
    fprintf (fid, INCLUDE_TEMPLATE, "rpcexc.h");

    for (p_exception = ifp->exceptions;
         p_exception != NULL;
         p_exception = p_exception->next)
    {
        fprintf(fid, "extern EXCEPTION ");
        spell_name(fid, p_exception->name);
        fprintf(fid, ";\n");
    }
}

/*
 *  Spell prototypes for I-char machinery
 */
static void BE_spell_ichar_prototypes
(
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *ifp  /* Ptr to AST interface node */
)
{
    AST_type_p_n_t *cstpp; /* Pointer to chain of [cs_char] types */
    AST_type_n_t *cstp;     /* Pointer to [cs_char] type */
    AST_name_n_t *rnp;      /* Pointer to [cs_tag_rtn] name */

    for (cstpp = ifp->cs_types; cstpp != NULL; cstpp = cstpp->next)
    {
        cstp = cstpp->type;

        fprintf(fid, "void ");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, "_net_size(\n");
        fprintf(fid, "rpc_binding_handle_t h,\n");
        fprintf(fid, "idl_ulong_int tag,\n");
        fprintf(fid, "idl_ulong_int l_storage_len,\n");
        fprintf(fid, "idl_cs_convert_t *p_convert_type,\n");
        fprintf(fid, "idl_ulong_int *p_w_storage_len,\n");
        fprintf(fid, "error_status_t *p_st\n);\n");

        fprintf(fid, "void ");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, "_to_netcs(\n");
        fprintf(fid, "rpc_binding_handle_t h,\n");
        fprintf(fid, "idl_ulong_int tag,\n");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, " *ldata,\n");
        fprintf(fid, "idl_ulong_int l_data_len,\n");
        spell_name(fid, cstp->name);
        fprintf(fid, " *wdata,\n");
        fprintf(fid, "idl_ulong_int *p_w_data_len,\n");
        fprintf(fid, "error_status_t *p_st\n);\n");

        fprintf(fid, "void ");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, "_local_size(\n");
        fprintf(fid, "rpc_binding_handle_t h,\n");
        fprintf(fid, "idl_ulong_int tag,\n");
        fprintf(fid, "idl_ulong_int w_storage_len,\n");
        fprintf(fid, "idl_cs_convert_t *p_convert_type,\n");
        fprintf(fid, "idl_ulong_int *p_l_storage_len,\n");
        fprintf(fid, "error_status_t *p_st\n);\n");

        fprintf(fid, "void ");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, "_from_netcs(\n");
        fprintf(fid, "rpc_binding_handle_t h,\n");
        fprintf(fid, "idl_ulong_int tag,\n");
        spell_name(fid, cstp->name);
        fprintf(fid, " *wdata,\n");
        fprintf(fid, "idl_ulong_int w_data_len,\n");
        fprintf(fid, "idl_ulong_int l_storage_len,\n");
        spell_name(fid, cstp->cs_char_type->type_name);
        fprintf(fid, " *ldata,\n");
        fprintf(fid, "idl_ulong_int *p_l_data_len,\n");
        fprintf(fid, "error_status_t *p_st\n);\n");
    }

    for (rnp = ifp->cs_tag_rtns; rnp != NULL; rnp = rnp->next)
    {
        fprintf(fid, "void ");
        spell_name(fid, rnp->name);
        fprintf(fid, "(\n");
        fprintf(fid, "rpc_binding_handle_t h,\n");
        fprintf(fid, "idl_boolean server_side,\n");
        fprintf(fid, "idl_ulong_int *p_stag,\n");
        fprintf(fid, "idl_ulong_int *p_drtag,\n");
        fprintf(fid, "idl_ulong_int *p_rtag,\n");
        fprintf(fid, "error_status_t *p_st\n);\n");
    }
}

/*
 *  Generate C header file
 */
void BE_gen_c_header
(
    FILE *fid,              /* Handle for emitted C text */
    AST_interface_n_t *ifp, /* Ptr to AST interface node */
    boolean bugs[],         /* List of backward compatibility "bugs" */
    boolean cepv_opt        /* -cepv option present */
)
{
    AST_import_n_t    *impp;
    AST_include_n_t   *incp;
    char        include_var_name[max_string_len];
    char        *fn_str, *if_name;

    NAMETABLE_id_to_string(ifp->name, &if_name);
    sprintf (include_var_name, "%s_v%d_%d_included", if_name,
                               (ifp->version%65536), (ifp->version/65536));
    fprintf (fid, "#ifndef %s\n#define %s\n",
             include_var_name, include_var_name);

    if (AST_DOUBLE_USED_SET(ifp) && !AST_LOCAL_SET(ifp))
        fprintf(fid, "#ifndef IDL_DOUBLE_USED\n#define IDL_DOUBLE_USED\n#endif\n");

    if (BE_bug_boolean_def)
        fprintf(fid, "#ifndef NIDL_bug_boolean_def\n#define NIDL_bug_boolean_def\n#endif\n");

#ifdef PERFMON
    /* Include the performance monitoring definitions. */
    fprintf (fid, "#ifdef PERFMON\n");
    fprintf (fid, INCLUDE_TEMPLATE, "idl_log.h");
    fprintf (fid, "#endif\n");
#endif

    fprintf (fid, "#ifndef IDLBASE_H\n");
    fprintf (fid, INCLUDE_TEMPLATE, "idlbase.h");
    fprintf (fid, "#endif\n");

    if (!AST_LOCAL_SET(ifp) && (ifp->op_count > 0))
        fprintf (fid, INCLUDE_TEMPLATE, "rpc.h");

    if (AST_HAS_ENCODE_OPS_SET(ifp))
        fprintf (fid, INCLUDE_TEMPLATE, "idl_es.h");

    for (incp = ifp->includes; incp; incp = incp->next) {
        STRTAB_str_to_string (incp->simple_file_name, &fn_str);
        fprintf (fid, USER_INCLUDE_H_TEMPLATE, fn_str);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    extern \"C\" {\n#endif\n\n");

    for (impp = ifp->imports; impp; impp=impp->next) {
        STRTAB_str_to_string (impp->file_name, &fn_str);
        FILE_form_filespec((char *)NULL, (char *)NULL,
                                                        HEADER_SUFFIX,
                             fn_str, include_var_name);
        if (impp->interface != NULL)
            fprintf (fid, "#ifndef %s_v%d_%d_included\n",
                    BE_get_name(impp->interface->name),
                    (impp->interface->version%65536),
                    (impp->interface->version/65536));
        fprintf (fid, USER_INCLUDE_TEMPLATE, include_var_name);
        if (impp->interface != NULL)
            fprintf (fid, "#endif\n");
    }

    BE_spell_extern_user_excs(fid, ifp);
    CSPELL_exports (fid, ifp->exports);
    BE_gen_pipe_routine_decls (fid, ifp);
    CSPELL_user_prototypes (fid, ifp);
    BE_spell_ichar_prototypes(fid, ifp);

    /* emit declarations of implicit handle variable and epv's */
    if (ifp->implicit_handle_name != NAMETABLE_NIL_ID) {
        fprintf (fid, "globalref ");
        if ( ! AST_IMPLICIT_HANDLE_G_SET(ifp) )
        {
            fprintf(fid, "handle_t");
        }
        else
        {
            spell_name (fid, ifp->implicit_handle_type_name);
        }
        fprintf(fid, " ");
        spell_name (fid, ifp->implicit_handle_name);
        fprintf (fid, ";\n");
        }

    if (!AST_LOCAL_SET(ifp) && (ifp->op_count > 0)) {
        CSPELL_epv_type_and_var(fid, ifp->name, ifp->version, ifp->exports,
            cepv_opt);
        CSPELL_if_spec_refs (fid, ifp->name, ifp->version);
    }

    fprintf (fid, "\n#ifdef __cplusplus\n    }\n#endif\n\n");

    fprintf (fid, "#endif\n");
}
