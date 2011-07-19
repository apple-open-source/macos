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
**      acf.y
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  ACF Parser Grammar and parser helper functions
**
**  VERSION: DCE 1.0
**
*/

/*******************************
 *  parser declarations section  *
 *******************************/

%{

  /* Tank Trap to stop older yacc parsers */
  /* Bison defines the macro YYBISON      */

#ifndef YYBISON
This grammar file needs to be built with GNU Bison V1.25 or later.
  GNU Bison can be be obtained from ftp://prep.ai.mit.edu:/pub/gnu
#endif

/* Declarations in this section are copied from yacc source to y_tab.c. */
#include <stdarg.h>

#include <nidl.h>               /* IDL compiler-wide defs */

#include <ast.h>                /* Abstract Syntax Tree defs */
#include <astp.h>               /* Import AST processing routine defs */
#include <command.h>            /* Command line defs */
#include <message.h>            /* Error message defs */
#include <nidlmsg.h>            /* Error message IDs */
#include <files.h>
#include <propagat.h>
#include <checker.h>

#define YYDEBUG 1

extern AST_interface_n_t *the_interface;    /* Ptr to AST interface node */
extern boolean ASTP_parsing_main_idl;       /* True when parsing main IDL */

typedef union                   /* Attributes bitmask */
{
    struct
    {
        unsigned auto_handle    : 1;
        unsigned binding_callout: 1;
        unsigned code           : 1;
        unsigned comm_status    : 1;
        unsigned cs_char        : 1;
        unsigned cs_drtag       : 1;
        unsigned cs_rtag        : 1;
        unsigned cs_stag        : 1;
        unsigned cs_tag_rtn     : 1;
        unsigned decode         : 1;
        unsigned enable_allocate: 1;
        unsigned encode         : 1;
        unsigned explicit_handle: 1;
        unsigned extern_exceps  : 1;
        unsigned fault_status   : 1;
        unsigned heap           : 1;
        unsigned implicit_handle: 1;
        unsigned in_line        : 1;
        unsigned nocode         : 1;
        unsigned out_of_line    : 1;
        unsigned represent_as   : 1;
        unsigned nocancel       : 1;
    }   bit;
    long    mask;
}   acf_attrib_t;

typedef struct acf_param_t      /* ACF parameter info structure */
{
    struct acf_param_t *next;                   /* Forward link */
    acf_attrib_t    parameter_attr;             /* Parameter attributes */
    NAMETABLE_id_t  param_id;                   /* Parameter name */
}   acf_param_t;

/* An opaque pointer. */
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

typedef struct acf_parser_state_t
{
    yyscan_t	acf_yyscanner;
    unsigned	acf_yynerrs;
    parser_location_t acf_location;

    boolean	acf_dumpers;

    acf_attrib_t acf_interface_attr;	/* Interface attributes */
    acf_attrib_t acf_type_attr;		/* Type attributes */
    acf_attrib_t acf_operation_attr;	/* Operation attributes */
    acf_attrib_t acf_parameter_attr;	/* Parameter attributes */

    const char * acf_interface_name;        /* Interface name */
    const char * acf_impl_name;             /* Implicit handle name */
    const char * acf_type_name;             /* Current type name */
    const char * acf_repr_type_name;        /* Current represent_as type */
    const char * acf_cs_char_type_name;     /* Current cs_char type */
    const char * acf_operation_name;        /* Current operation name */
    const char * acf_cs_tag_rtn_name;       /* Current cs_tag_rtn name */
    const char * acf_binding_callout_name;  /* Current binding_callout name */
    boolean	 acf_named_type;    /* True if parsed type is named type */

    AST_include_n_t * acf_include_list;	    /* List of AST include nodes */
    AST_include_n_t * acf_include_p;        /* Ptr to a created include node */

    acf_param_t * acf_parameter_list;	    /* Param list for curr. operation */
    acf_param_t * acf_parameter_free_list;  /* True if param attrs specified */
    boolean       acf_parameter_attr_list;  /* True if param attrs specified */
} acf_parser_state_t;

/*
 * Forward declarations to shut up the compiler
 */

static boolean lookup_exception(acf_parser_state_t *, NAMETABLE_id_t,
	boolean, AST_exception_n_t **);
static boolean lookup_type(acf_parser_state_t *, char const *, boolean,
	NAMETABLE_id_t *, AST_type_n_t **);
static boolean lookup_operation(acf_parser_state_t *, char const *,
	boolean, NAMETABLE_id_t *, AST_operation_n_t **);
static boolean lookup_parameter(acf_parser_state_t *, AST_operation_n_t *,
	char const *, boolean, NAMETABLE_id_t *, AST_parameter_n_t **);
static boolean lookup_rep_as_name(AST_type_p_n_t *, NAMETABLE_id_t, AST_type_n_t **, char const **);
static boolean lookup_cs_char_name(AST_type_p_n_t *, NAMETABLE_id_t, AST_type_n_t **, char const * *);
static acf_param_t * alloc_param(acf_parser_state_t *);
static void free_param(acf_parser_state_t *, acf_param_t *);
static void free_param_list(acf_parser_state_t *, acf_param_t **);
void add_param_to_list(acf_param_t *, acf_param_t **);
static void append_parameter(acf_parser_state_t *, AST_operation_n_t *,
	char const *, acf_attrib_t *);
static void process_rep_as_type(acf_parser_state_t *, AST_interface_n_t *,
	AST_type_n_t *, char const *);
static void process_cs_char_type(acf_parser_state_t *, AST_interface_n_t *,
	AST_type_n_t *, char const *);
static void dump_attributes(acf_parser_state_t *, const char *, const char *, acf_attrib_t *);

/*
 * Warning and Error stuff
 */

static void acf_yyerror ( YYLTYPE *, acf_parser_p, char const *);

/*
**  a c f _ e r r o r
**
**  Issues an error message, and bumps the error count.
**
*/
static void acf_error
(
    acf_parser_state_t * acf,
    long msgid,
    ...
)
{
    va_list ap;

    va_start(ap, msgid);
    vlog_error(acf_yylineno(acf), msgid, ap);
    va_end(ap);

    acf->acf_yynerrs++;
}

/*
**  a c f _ w a r n i n g
**
**  Issues a warning message.
**
*/
static void acf_warning
(
    acf_parser_state_t * acf,
    long msgid,
    ...
)
{
    va_list ap;

    va_start(ap, msgid);

    vlog_warning(acf_yylineno(acf), msgid, ap);

    va_end(ap);
}

%}

%locations
%defines
%error-verbose
%pure-parser
%name-prefix="acf_yy"

/* Tell Bison that the Flexer takes a yyscan_t parameter. */
%lex-param { void * lexxer }
/* Tell Bison that we will pass the yyscan_t scanner into yyparse. */
%parse-param { acf_parser_state_t * acf }

/* Tell Bison how to get the lexxer argument from the parser state. */
%{
#define lexxer acf->acf_yyscanner
%}

/*------------------------------------*
 *  yylval and yyval type definition  *
 *------------------------------------*/

/*
 * Union declaration defines the possible datatypes of the external variables
 * yylval and yyval.
 */

%union
{
    NAMETABLE_id_t  y_id;       /* Identifier */
    STRTAB_str_t    y_string;   /* Text string */
}

%{
#include <acf_l.h>
%}

/*-----------------------------*
 *  Tokens used by the parser  *
 *-----------------------------*/

/* Keywords */

%token AUTO_HANDLE_KW
%token BINDING_CALLOUT_KW
%token CODE_KW
%token COMM_STATUS_KW
%token CS_CHAR_KW
%token CS_TAG_RTN_KW
%token ENABLE_ALLOCATE_KW
%token EXPLICIT_HANDLE_KW
%token EXTERN_EXCEPS_KW
%token FAULT_STATUS_KW
%token HANDLE_T_KW
%token HEAP_KW
%token IMPLICIT_HANDLE_KW
%token INCLUDE_KW
%token INTERFACE_KW
%token IN_LINE_KW
%token NOCODE_KW
%token NOCANCEL_KW
%token OUT_OF_LINE_KW
%token REPRESENT_AS_KW
%token TYPEDEF_KW

/* Punctuation */

%token COMMA
%token LBRACE
%token LBRACKET
%token LPAREN
%token RBRACE
%token RBRACKET
%token RPAREN
%token SEMI
%token TILDE
%token UNKNOWN  /* Unrecognized by LEX */

/* Tokens setting yylval */

%token <y_id>       IDENTIFIER
%token <y_string>   STRING

/*-----------------------------*
 *  Starting state for parser  *
 *-----------------------------*/

%start acf_interface

%%

/****************************
 *  parser grammar section  *
 ****************************/

acf_interface:
        acf_interface_header acf_interface_body
    ;

acf_interface_header:
        acf_interface_attr_list INTERFACE_KW acf_interface_name
    {
        char const      *ast_int_name;  /* Interface name in AST node */
        NAMETABLE_id_t  impl_name_id;   /* Nametable id of impl_handle var */

        if (acf->acf_dumpers)
	{
            dump_attributes(acf, "ACF interface", acf->acf_interface_name,
		    &acf->acf_interface_attr);
	}

        /* Store source information. */
        if (the_interface->fe_info != NULL)
        {
            the_interface->fe_info->acf_file = acf->acf_location.fileid;
            the_interface->fe_info->acf_source_line = acf_yylineno(acf);
        }

        /*
         *  Interface attributes are saved for main and imported interfaces.
         *  the_interface = pointer to main or imported interface node
         *
         *  Make sure that the interface name in the ACF agrees with the
         *  interface name in the main IDL file.  Then set the parsed
         *  attributes in the interface node.
         *
         *  interface_attr = bitmask of interface attributes parsed.
         *  interface_name = ACF interface name parsed.
         */

        NAMETABLE_id_to_string(the_interface->name, &ast_int_name);

        if (strcmp(acf->acf_interface_name, ast_int_name) != 0)
        {
            char const *acf_int_name;   /* Ptr to permanent copy */
            NAMETABLE_id_t name_id;     /* Handle on permanent copy */
            char const *file_name;      /* Related file name */

            name_id = NAMETABLE_add_id(acf->acf_interface_name);
            NAMETABLE_id_to_string(name_id, &acf_int_name);

            STRTAB_str_to_string(the_interface->fe_info->file, &file_name);

            acf_error(acf, NIDL_INTNAMDIF, acf_int_name, ast_int_name);
            acf_error(acf, NIDL_NAMEDECLAT, ast_int_name, file_name,
                      the_interface->fe_info->source_line);
        }
        else
        {
            if (acf->acf_interface_attr.bit.code)
                AST_SET_CODE(the_interface);
            if (acf->acf_interface_attr.bit.nocode)
                AST_SET_NO_CODE(the_interface);
            if (acf->acf_interface_attr.bit.decode)
                AST_SET_DECODE(the_interface);
            if (acf->acf_interface_attr.bit.encode)
                AST_SET_ENCODE(the_interface);
            if (acf->acf_interface_attr.bit.explicit_handle)
                AST_SET_EXPLICIT_HANDLE(the_interface);
            if (acf->acf_interface_attr.bit.in_line)
                AST_SET_IN_LINE(the_interface);
            if (acf->acf_interface_attr.bit.out_of_line)
                AST_SET_OUT_OF_LINE(the_interface);
            if (acf->acf_interface_attr.bit.auto_handle)
                AST_SET_AUTO_HANDLE(the_interface);
            if (acf->acf_interface_attr.bit.nocancel)
                AST_SET_NO_CANCEL(the_interface);

            if (acf->acf_interface_attr.bit.cs_tag_rtn)
                the_interface->cs_tag_rtn_name = NAMETABLE_add_id(acf->acf_cs_tag_rtn_name);
            if (acf->acf_interface_attr.bit.binding_callout)
	    {
                the_interface->binding_callout_name =
		    NAMETABLE_add_id(acf->acf_binding_callout_name);
	    }

            if (acf->acf_interface_attr.bit.implicit_handle)
            {
                /* Store the [implicit_handle] variable name in nametbl. */
                impl_name_id = NAMETABLE_add_id(acf->acf_impl_name);

	        ASTP_set_implicit_handle(the_interface,
		    acf->acf_named_type ? NAMETABLE_add_id(acf->acf_type_name)
			       : NAMETABLE_NIL_ID,
		    impl_name_id);

            }
        }

        acf->acf_interface_name = NULL;
        acf->acf_type_name = NULL;
        acf->acf_impl_name = NULL;
        acf->acf_binding_callout_name = NULL;
        acf->acf_cs_tag_rtn_name = NULL;
        acf->acf_interface_attr.mask = 0;        /* Reset attribute mask */
    }
    ;

acf_interface_attr_list:
        LBRACKET acf_interface_attrs RBRACKET
    |   /* Nothing */
    ;

acf_interface_attrs:
        acf_interface_attr
    |   acf_interface_attrs COMMA acf_interface_attr
    ;

acf_interface_attr:
        acf_code_attr
    {
        if (acf->acf_interface_attr.bit.code)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.code = TRUE;
    }
    |   acf_nocode_attr
    {
        if (acf->acf_interface_attr.bit.nocode)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.nocode = TRUE;
    }
    |   acf_binding_callout_attr
    {
        if (acf->acf_interface_attr.bit.binding_callout)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_interface_attr.bit.binding_callout = TRUE;
    }
    |   acf_cs_tag_rtn_attr
    {
        if (acf->acf_interface_attr.bit.cs_tag_rtn)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_interface_attr.bit.cs_tag_rtn = TRUE;
    }
    |   acf_explicit_handle_attr
    {
        if (acf->acf_interface_attr.bit.explicit_handle)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.explicit_handle = TRUE;
    }
    |   acf_nocancel_attr
    {
        if (acf->acf_interface_attr.bit.nocancel)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.nocancel = TRUE;
    }
    |   acf_inline_attr
    {
        if (acf->acf_interface_attr.bit.in_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (acf->acf_interface_attr.bit.out_of_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.out_of_line = TRUE;
    }
    |   acf_implicit_handle_attr
    {
        if (acf->acf_interface_attr.bit.implicit_handle)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_interface_attr.bit.implicit_handle = TRUE;
    }
    |   acf_auto_handle_attr
    {
        if (acf->acf_interface_attr.bit.auto_handle)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.auto_handle = TRUE;
    }
    |   acf_extern_exceps_attr
    {
        if (acf->acf_interface_attr.bit.extern_exceps)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_interface_attr.bit.extern_exceps = TRUE;
    }
    |   IDENTIFIER
    {
        if (NAMETABLE_add_id("decode") == $<y_id>1)
        {
            if (acf->acf_interface_attr.bit.decode)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_interface_attr.bit.decode = TRUE;
        }
        else if (NAMETABLE_add_id("encode") == $<y_id>1)
        {
            if (acf->acf_interface_attr.bit.encode)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_interface_attr.bit.encode = TRUE;
        }
        else
            log_error(acf_yylineno(acf), NIDL_ERRINATTR, NULL);
    }
    ;

acf_implicit_handle_attr:
        IMPLICIT_HANDLE_KW LPAREN acf_implicit_handle RPAREN
    ;

acf_implicit_handle:
        acf_impl_type acf_impl_name
    ;

acf_impl_type:
        acf_handle_type
    {
        acf->acf_named_type = FALSE;
    }
    |   IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_type_name);
        acf->acf_named_type = TRUE;
    }
    ;

acf_handle_type:
        HANDLE_T_KW
    ;

acf_impl_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_impl_name);
    }
    ;

acf_extern_exceps_attr:
        EXTERN_EXCEPS_KW LPAREN acf_ext_excep_list RPAREN
    |   EXTERN_EXCEPS_KW
    {
        if (ASTP_parsing_main_idl)
        {
            AST_exception_n_t *excep_p;
            for (excep_p = the_interface->exceptions;
                 excep_p != NULL;
                 excep_p = excep_p->next)
            {
                AST_SET_EXTERN(excep_p);
            }
        }
    }
    ;

acf_ext_excep_list:
        acf_ext_excep
    |   acf_ext_excep_list COMMA acf_ext_excep
    ;

acf_ext_excep:
        IDENTIFIER
    {
        AST_exception_n_t *excep_p;
        if (ASTP_parsing_main_idl)
            if (lookup_exception(acf, $<y_id>1, TRUE, &excep_p))
                AST_SET_EXTERN(excep_p);
    }
    ;

acf_interface_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_interface_name);
    }
    ;

acf_interface_body:
        LBRACE acf_body_elements RBRACE
    |   LBRACE RBRACE
    |   error
        { log_error(acf_yylineno(acf), NIDL_SYNTAXERR, NULL); }
    |   error RBRACE
        { log_error(acf_yylineno(acf), NIDL_SYNTAXERR, NULL); }
    ;

acf_body_elements:
        acf_body_element
    |   acf_body_elements acf_body_element
    ;

acf_body_element:
        acf_include SEMI
    |   acf_type_declaration SEMI
    |   acf_operation_declaration SEMI
    |   error SEMI
        {
            log_error(acf_yylineno(acf), NIDL_SYNTAXERR, NULL);
            /* Re-initialize attr masks to avoid sticky attributes */
            acf->acf_interface_attr.mask = 0;
            acf->acf_type_attr.mask      = 0;
            acf->acf_operation_attr.mask = 0;
            acf->acf_parameter_attr.mask = 0;
        }
    ;

acf_include:
        INCLUDE_KW acf_include_list
        {
        if (ASTP_parsing_main_idl)
	{
            the_interface->includes = (AST_include_n_t *)
                AST_concat_element((ASTP_node_t *)the_interface->includes,
                                   (ASTP_node_t *)acf->acf_include_list);
	}
        acf->acf_include_list = NULL;
        }
    |   INCLUDE_KW error
        { log_error(acf_yylineno(acf), NIDL_SYNTAXERR, NULL); }
    ;

acf_include_list:
        acf_include_name
    |   acf_include_list COMMA acf_include_name
    ;

acf_include_name:
        STRING
    {
        if (ASTP_parsing_main_idl)
        {
            char const      *parsed_include_file;
            char            include_type[PATH_MAX];
            char            include_file[PATH_MAX];
            STRTAB_str_t    include_file_id;

            STRTAB_str_to_string($<y_string>1, &parsed_include_file);

            /*
             * Log warning if include name contains a file extension.
             * Tack on the correct extension based on the -lang option.
             */
            FILE_parse(parsed_include_file, (char *)NULL, 0, (char *)NULL, 0,
                       include_type, sizeof (include_type));
            if (include_type[0] != '\0')
                acf_warning(acf, NIDL_INCLUDEXT);

            FILE_form_filespec(parsed_include_file, (char *)NULL,
							   ".h",
                               (char *)NULL, include_file, sizeof(include_file));

            /* Create an include node. */
            include_file_id = STRTAB_add_string(include_file);
            acf->acf_include_p = AST_include_node(acf_location(acf),
		    include_file_id, $<y_string>1);

            /* Store source information. */
            if (acf->acf_include_p->fe_info != NULL)
            {
                acf->acf_include_p->fe_info->acf_file = acf->acf_location.fileid;
                acf->acf_include_p->fe_info->acf_source_line = acf_yylineno(acf);
            }

            acf->acf_include_list = (AST_include_n_t *)
                AST_concat_element((ASTP_node_t *)acf->acf_include_list,
                                   (ASTP_node_t *)acf->acf_include_p);
        }
    }
    ;

acf_type_declaration:
        TYPEDEF_KW error
        { log_error(acf_yylineno(acf), NIDL_SYNTAXERR, NULL); }
    |   TYPEDEF_KW acf_type_attr_list acf_named_type_list
    {
        acf->acf_type_attr.mask = 0;    /* Reset attribute mask */
        acf->acf_repr_type_name = NULL; /* Reset represent_as type name */
        acf->acf_cs_char_type_name = NULL;       /* Reset cs_char type name */
    }
    ;

acf_named_type_list:
        acf_named_type
    {
        acf->acf_type_name = NULL;               /* Reset type name */
    }
    |   acf_named_type_list COMMA acf_named_type
    {
        acf->acf_type_name = NULL;               /* Reset type name */
    }
    ;

acf_named_type:
        IDENTIFIER
    {
        NAMETABLE_id_t  type_id;        /* Nametable id of type name */
        AST_type_n_t    *type_p;        /* Ptr to AST type node */

        NAMETABLE_id_to_string($<y_id>1, &acf->acf_type_name);

        if (acf->acf_dumpers)
	{
            dump_attributes(acf, "ACF type",
		    acf->acf_type_name, &acf->acf_type_attr);
	}

        /*
         *  Lookup the type_name parsed and verify that it is a valid type
         *  node.  Then set the parsed attributes in the type node.
         *
         *  type_attr = bitmask of type attributes parsed.
         *  type_name = name of type_t node to look up.
         *  [repr_type_name] = name of represent_as type.
         *  [cs_char_type_name] = name of cs_char type.
         */

        if (lookup_type(acf, acf->acf_type_name, TRUE, &type_id, &type_p))
        {
            /* Store source information. */
            if (type_p->fe_info != NULL)
            {
                type_p->fe_info->acf_file = acf->acf_location.fileid;
                type_p->fe_info->acf_source_line = acf_yylineno(acf);
            }

            if (acf->acf_type_attr.bit.heap
                && type_p->kind != AST_pipe_k
                && !AST_CONTEXT_RD_SET(type_p))
                PROP_set_type_attr(type_p,AST_HEAP);
            if (acf->acf_type_attr.bit.in_line)
                PROP_set_type_attr(type_p,AST_IN_LINE);
            if ((acf->acf_type_attr.bit.out_of_line) &&
                (type_p->kind != AST_pointer_k) &&
                (type_p->xmit_as_type == NULL))
                PROP_set_type_attr(type_p,AST_OUT_OF_LINE);
            if (acf->acf_type_attr.bit.represent_as)
                process_rep_as_type(acf, the_interface, type_p, acf->acf_repr_type_name);
            if (acf->acf_type_attr.bit.cs_char)
                process_cs_char_type(acf, the_interface, type_p, acf->acf_cs_char_type_name);
        }
    }
    ;

acf_type_attr_list:
        LBRACKET acf_rest_of_attr_list
    |   /* Nothing */
    ;

acf_rest_of_attr_list:
        acf_type_attrs RBRACKET
    |   error SEMI
        {
        log_error(acf_yylineno(acf), NIDL_MISSONATTR, NULL);
        }
    |   error RBRACKET
        {
        log_error(acf_yylineno(acf), NIDL_ERRINATTR, NULL);
        }
    ;

acf_type_attrs:
        acf_type_attr
    |   acf_type_attrs COMMA acf_type_attr
    ;

acf_type_attr:
        acf_represent_attr
    {
        if (acf->acf_type_attr.bit.represent_as)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_type_attr.bit.represent_as = TRUE;
    }
    |   acf_cs_char_attr
    {
        if (acf->acf_type_attr.bit.cs_char)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_type_attr.bit.cs_char = TRUE;
    }
    |   acf_heap_attr
    {
        if (acf->acf_type_attr.bit.heap)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_type_attr.bit.heap = TRUE;
    }
    |   acf_inline_attr
    {
        if (acf->acf_type_attr.bit.in_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_type_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (acf->acf_type_attr.bit.out_of_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_type_attr.bit.out_of_line = TRUE;
    }
    ;

acf_represent_attr:
        REPRESENT_AS_KW LPAREN acf_repr_type RPAREN
    ;

acf_repr_type:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_repr_type_name);
    }
    ;

acf_cs_char_attr:
        CS_CHAR_KW LPAREN acf_cs_char_type RPAREN
    ;

acf_cs_char_type:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_cs_char_type_name);
    }
    ;

acf_operation_declaration:
        acf_op_attr_list acf_operations
    {
        acf->acf_operation_attr.mask = 0;        /* Reset attribute mask */
        acf->acf_cs_tag_rtn_name     = NULL;     /* Reset cs_tag_rtn name */
    }
    ;

acf_operations:
        acf_operation
    |   acf_operations COMMA acf_operation
    ;

acf_operation:
        IDENTIFIER LPAREN acf_parameter_list RPAREN
    {
        acf_param_t         *p;         /* Ptr to local parameter structure */
        NAMETABLE_id_t      op_id;      /* Nametable id of operation name */
        NAMETABLE_id_t      param_id;   /* Nametable id of parameter name */
        AST_operation_n_t   *op_p;      /* Ptr to AST operation node */
        AST_parameter_n_t   *param_p;   /* Ptr to AST parameter node */
        boolean             log_error;  /* TRUE => error if name not found */
        char const          *param_name;/* character string of param id */

        NAMETABLE_id_to_string($<y_id>1, &acf->acf_operation_name);

        if (acf->acf_dumpers)
	{
            dump_attributes(acf, "ACF operation",
		    acf->acf_operation_name, &acf->acf_operation_attr);
	}

        /*
         *  Operation and parameter attributes are ignored for imported
         *  interfaces.  Operations and parameters within imported interfaces
         *  are not put in the AST.
         */
        if (ASTP_parsing_main_idl)
        {
            /*
             *  Lookup the operation_name parsed and verify that it is a valid
             *  operation node.  Then set the parsed attributes in the operation
             *  node.  For each parameter_name that was parsed for this
             *  operation, chase the parameter list off the AST operation node
             *  to verify that it is a valid parameter for that operation.
             *  Then set the parsed attributes for that parameter into the
             *  relevant parameter node.
             *
             *  operation_attr = bitmask of operation attributes parsed.
             *  operation_name = name of routine_t node to look up.
             *  [cs_tag_rtn_name] = cs_tag_rtn name.
             *  parameter_list = linked list of parameter information.
             */

            if (lookup_operation(acf, acf->acf_operation_name, TRUE, &op_id, &op_p))
            {
                /* Store source information. */
                if (op_p->fe_info != NULL)
                {
                    op_p->fe_info->acf_file = acf->acf_location.fileid;
                    op_p->fe_info->acf_source_line = acf_yylineno(acf);
                }

                if (acf->acf_operation_attr.bit.comm_status)
                {
                    /*
                     * Assume the AST Builder always builds a result param,
                     * even for void operations.
                     */
                    AST_SET_COMM_STATUS(op_p->result);
                }
                if (acf->acf_operation_attr.bit.fault_status)
                    AST_SET_FAULT_STATUS(op_p->result);

                if (acf->acf_operation_attr.bit.code)
                    AST_SET_CODE(op_p);
                if (acf->acf_operation_attr.bit.nocode)
                    AST_SET_NO_CODE(op_p);
                if (acf->acf_operation_attr.bit.decode)
                    AST_SET_DECODE(op_p);
                if (acf->acf_operation_attr.bit.encode)
                    AST_SET_ENCODE(op_p);
                if (acf->acf_operation_attr.bit.enable_allocate)
                    AST_SET_ENABLE_ALLOCATE(op_p);
                if (acf->acf_operation_attr.bit.explicit_handle)
                    AST_SET_EXPLICIT_HANDLE(op_p);
                if (acf->acf_operation_attr.bit.nocancel)
                    AST_SET_NO_CANCEL(op_p);
                if (acf->acf_operation_attr.bit.cs_tag_rtn)
                    op_p->cs_tag_rtn_name = NAMETABLE_add_id(acf->acf_cs_tag_rtn_name);

                for (p = acf->acf_parameter_list ; p != NULL ; p = p->next)
                {
                    /*
                     * Most parameter attributes, if present, require that the
                     * referenced parameter be defined in the IDL.  If only
                     * [comm_status] and/or [fault_status] is present, the
                     * parameter  needn't be IDL-defined.
                     */
                    if (!p->parameter_attr.bit.heap
                        &&  !p->parameter_attr.bit.in_line
                        &&  !p->parameter_attr.bit.out_of_line
                        &&  !p->parameter_attr.bit.cs_stag
                        &&  !p->parameter_attr.bit.cs_drtag
                        &&  !p->parameter_attr.bit.cs_rtag
                        &&  (p->parameter_attr.bit.comm_status
                             || p->parameter_attr.bit.fault_status))
                        log_error = FALSE;
                    else
                        log_error = TRUE;

                    NAMETABLE_id_to_string(p->param_id, &param_name);
                    if (lookup_parameter(acf, op_p, param_name, log_error,
                                         &param_id, &param_p))
                    {
                        /* Store source information. */
                        if (param_p->fe_info != NULL)
                        {
                            param_p->fe_info->acf_file = acf->acf_location.fileid;
                            param_p->fe_info->acf_source_line = acf_yylineno(acf);
                        }

                        if (p->parameter_attr.bit.comm_status)
                            AST_SET_COMM_STATUS(param_p);
                        if (p->parameter_attr.bit.fault_status)
                            AST_SET_FAULT_STATUS(param_p);
                        if (p->parameter_attr.bit.heap)
                        {
                            AST_type_n_t *ref_type_p;
                            ref_type_p = param_follow_ref_ptr(param_p,
                                                              CHK_follow_ref);
                            if (ref_type_p->kind != AST_pipe_k
                                && !AST_CONTEXT_SET(param_p)
                                && !AST_CONTEXT_RD_SET(ref_type_p)
                                && !type_is_scalar(ref_type_p))
                                AST_SET_HEAP(param_p);
                        }
                        if (p->parameter_attr.bit.in_line)
                            AST_SET_IN_LINE(param_p);
                        /*
                         * We parse the [out_of_line] parameter attribute,
                         * but disallow it.
                         */
                        if (p->parameter_attr.bit.out_of_line)
                            acf_error(acf, NIDL_INVOOLPRM);
                        if (p->parameter_attr.bit.cs_stag)
                            AST_SET_CS_STAG(param_p);
                        if (p->parameter_attr.bit.cs_drtag)
                            AST_SET_CS_DRTAG(param_p);
                        if (p->parameter_attr.bit.cs_rtag)
                            AST_SET_CS_RTAG(param_p);
                    }
                    else if (log_error == FALSE)
                    {
                        /*
                         * Lookup failed, but OK since the parameter only has
                         * attribute(s) that specify an additional parameter.
                         * Append a parameter to the operation parameter list.
                         */
                        NAMETABLE_id_to_string(p->param_id, &param_name);
                        append_parameter(acf, op_p, param_name, &p->parameter_attr);
                    }
                }
            }
        }

        free_param_list(acf, &acf->acf_parameter_list);       /* Free parameter list */

        acf->acf_operation_name = NULL;
    }
    ;

acf_op_attr_list:
        LBRACKET acf_op_attrs RBRACKET
    |   /* Nothing */
    ;

acf_op_attrs:
        acf_op_attr
    |   acf_op_attrs COMMA acf_op_attr
    ;

acf_op_attr:
        acf_commstat_attr
    {
        if (acf->acf_operation_attr.bit.comm_status)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.comm_status = TRUE;
    }
    |   acf_code_attr
    {
        if (acf->acf_operation_attr.bit.code)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.code = TRUE;
    }
    |   acf_nocode_attr
    {
        if (acf->acf_operation_attr.bit.nocode)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.nocode = TRUE;
    }
    |   acf_cs_tag_rtn_attr
    {
        if (acf->acf_operation_attr.bit.cs_tag_rtn)
            log_error(acf_yylineno(acf), NIDL_ATTRUSEMULT, NULL);
        acf->acf_operation_attr.bit.cs_tag_rtn = TRUE;
    }
    |   acf_enable_allocate_attr
    {
        if (acf->acf_operation_attr.bit.enable_allocate)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.enable_allocate = TRUE;
    }
    |   acf_explicit_handle_attr
    {
        if (acf->acf_operation_attr.bit.explicit_handle)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.explicit_handle = TRUE;
    }
    |   acf_nocancel_attr
    {
        if (acf->acf_operation_attr.bit.nocancel)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.nocancel = TRUE;
    }
    |   acf_faultstat_attr
    {
        if (acf->acf_operation_attr.bit.fault_status)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_operation_attr.bit.fault_status = TRUE;
    }
    |   IDENTIFIER
    {
        if (NAMETABLE_add_id("decode") == $<y_id>1)
        {
            if (acf->acf_operation_attr.bit.decode)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_operation_attr.bit.decode = TRUE;
        }
        else if (NAMETABLE_add_id("encode") == $<y_id>1)
        {
            if (acf->acf_operation_attr.bit.encode)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_operation_attr.bit.encode = TRUE;
        }
        else
            log_error(acf_yylineno(acf), NIDL_ERRINATTR, NULL);
    }
    ;

acf_binding_callout_attr:
        BINDING_CALLOUT_KW LPAREN acf_binding_callout_name RPAREN
    ;

acf_binding_callout_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_binding_callout_name);
    }
    ;

acf_cs_tag_rtn_attr:
        CS_TAG_RTN_KW LPAREN acf_cs_tag_rtn_name RPAREN
    ;

acf_cs_tag_rtn_name:
        IDENTIFIER
    {
        NAMETABLE_id_to_string($<y_id>1, &acf->acf_cs_tag_rtn_name);
    }
    ;

acf_parameter_list:
        acf_parameters
    |   /* Nothing */
    ;

acf_parameters:
        acf_parameter
    |   acf_parameters COMMA acf_parameter
    ;

acf_parameter:
        acf_param_attr_list IDENTIFIER
    {
        if (acf->acf_dumpers)
        {
            char const *param_name;
            NAMETABLE_id_to_string($<y_id>2, &param_name);
            dump_attributes(acf, "ACF parameter",
		    param_name, &acf->acf_parameter_attr);
        }

        if (acf->acf_parameter_attr_list) /* If there were param attributes: */
        {
            acf_param_t *p;             /* Pointer to parameter record */

            /*
             * Allocate and initialize a parameter record.
             */
            p = alloc_param(acf);
            p->parameter_attr = acf->acf_parameter_attr;
            p->param_id = $<y_id>2;

            /*
             * Add to end of parameter list.
             */
            add_param_to_list(p, &acf->acf_parameter_list);

            acf->acf_parameter_attr.mask = 0;
        }
    }
    ;

acf_param_attr_list:
        LBRACKET acf_param_attrs RBRACKET
    {
        acf->acf_parameter_attr_list = TRUE;     /* Flag that we have param attributes */
    }
    |   /* Nothing */
    {
        acf->acf_parameter_attr_list = FALSE;
    }
    ;

acf_param_attrs:
        acf_param_attr
    |   acf_param_attrs COMMA acf_param_attr
    ;

acf_param_attr:
        acf_commstat_attr
    {
        if (acf->acf_parameter_attr.bit.comm_status)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_parameter_attr.bit.comm_status = TRUE;
    }
    |   acf_faultstat_attr
    {
        if (acf->acf_parameter_attr.bit.fault_status)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_parameter_attr.bit.fault_status = TRUE;
    }
    |   acf_heap_attr
    {
        if (acf->acf_parameter_attr.bit.heap)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_parameter_attr.bit.heap = TRUE;
    }
    |   acf_inline_attr
    {
        if (acf->acf_parameter_attr.bit.in_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_parameter_attr.bit.in_line = TRUE;
    }
    |   acf_outofline_attr
    {
        if (acf->acf_parameter_attr.bit.out_of_line)
            log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
        acf->acf_parameter_attr.bit.out_of_line = TRUE;
    }
    |   IDENTIFIER
    {
        if (NAMETABLE_add_id("cs_stag") == $<y_id>1)
        {
            if (acf->acf_parameter_attr.bit.cs_stag)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_parameter_attr.bit.cs_stag = TRUE;
        }
        else if (NAMETABLE_add_id("cs_drtag") == $<y_id>1)
        {
            if (acf->acf_parameter_attr.bit.cs_drtag)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_parameter_attr.bit.cs_drtag = TRUE;
        }
        else if (NAMETABLE_add_id("cs_rtag") == $<y_id>1)
        {
            if (acf->acf_parameter_attr.bit.cs_rtag)
                log_warning(acf_yylineno(acf), NIDL_MULATTRDEF, NULL);
            acf->acf_parameter_attr.bit.cs_rtag = TRUE;
        }
        else
            log_error(acf_yylineno(acf), NIDL_ERRINATTR, NULL);
    }
    ;

acf_auto_handle_attr:   AUTO_HANDLE_KW;
acf_code_attr:          CODE_KW;
acf_nocode_attr:        NOCODE_KW;
acf_enable_allocate_attr: ENABLE_ALLOCATE_KW;
acf_explicit_handle_attr: EXPLICIT_HANDLE_KW;
acf_nocancel_attr:      NOCANCEL_KW;
acf_heap_attr:          HEAP_KW;
acf_inline_attr:        IN_LINE_KW;
acf_outofline_attr:     OUT_OF_LINE_KW;
acf_commstat_attr:      COMM_STATUS_KW;
acf_faultstat_attr:     FAULT_STATUS_KW;

%%

/***************************
 *  yacc programs section  *
 ***************************/

/*
 *  a c f _ p a r s e r _ a l l o c
 *
 *  Function:   Called to create an new ACF parser object
 *
 */

acf_parser_p acf_parser_alloc
(
    boolean     *cmd_opt_arr,   /* [in] Array of command option flags */
    void        **cmd_val_arr,  /* [in] Array of command option values */
    char        *acf_file       /* [in] ACF file name */
)
{
    acf_parser_state_t * acf;

    acf = NEW(acf_parser_state_t);

    acf->acf_dumpers = FALSE;

#ifdef DUMPERS
    if (cmd_opt_arr[opt_dump_acf])
	acf->acf_dumpers = TRUE;
#else
    (void)cmd_opt_arr;
#endif

    /* Set global (STRTAB_str_t error_file_name_id) for error processing. */
    set_name_for_errors(acf_file);
    acf->acf_location.fileid = STRTAB_add_string(acf_file);

   // XXX save file name ID in parser state

    acf->acf_interface_attr.mask = 0;
    acf->acf_type_attr.mask      = 0;
    acf->acf_operation_attr.mask = 0;
    acf->acf_parameter_attr.mask = 0;

    acf->acf_interface_name      = NULL;
    acf->acf_type_name           = NULL;
    acf->acf_repr_type_name      = NULL;
    acf->acf_cs_char_type_name   = NULL;
    acf->acf_operation_name      = NULL;
    acf->acf_binding_callout_name= NULL;
    acf->acf_cs_tag_rtn_name     = NULL;

    acf->acf_include_list        = NULL;

    acf->acf_parameter_list      = NULL;
    acf->acf_parameter_free_list = NULL;

    return acf;
}

/*
 *  a c f _ p a r s e r _ d e s t r o y
 *
 *  Function:   Called after ACF parsing to free allocated memory.
 *
 */

void acf_parser_destroy
(
    acf_parser_p acf
)
{
    acf_param_t *p, *q;     /* Ptrs to parameter record */

    p = acf->acf_parameter_free_list;

    while (p != NULL)
    {
        q = p;
        p = p->next;
        FREE(q);
    }

    if (acf->acf_yyscanner)
    {
	acf_yylex_destroy(acf->acf_yyscanner);
    }

    FREE(acf);
    yyin_p = NULL;
    yylineno_p = NULL;
}

void acf_parser_input
(
    acf_parser_p acf,
    FILE * in
)
{

    assert(acf->acf_yyscanner == NULL);

    acf_yylex_init(&acf->acf_yyscanner);
    acf_yyset_in(in, acf->acf_yyscanner);

    yyin_p = in;
    yylineno_p = &acf->acf_location.lineno;
}

unsigned acf_yylineno
(
   acf_parser_p acf
)
{
   return acf_yyget_lineno(acf->acf_yyscanner);
}

const parser_location_t * acf_location
(
   acf_parser_p acf
)
{
    /* Update the current location before handing it back ... */
    acf->acf_location.lineno = acf_yylineno(acf);
    acf->acf_location.location = *acf_yyget_lloc(acf->acf_yyscanner);
    acf->acf_location.text = acf_yyget_text(acf->acf_yyscanner);

    return &acf->acf_location;
}

unsigned acf_errcount
(
   acf_parser_p acf
)
{
   return acf->acf_yynerrs;
}

static void acf_yyerror
(
    YYLTYPE * yylloc ATTRIBUTE_UNUSED,
    acf_parser_p acf,
    char const * message
)
{
    const struct parser_location_t * loc;
    loc = acf_location(acf);
    idl_yyerror(loc, message);
}

/*
**  l o o k u p _ e x c e p t i o n
**
**  Looks up a name in the nametable, and if it is bound to a valid exception
**  node, returns the address of the exception node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_exception
(
    acf_parser_state_t *acf,
    NAMETABLE_id_t  excep_id,     /* [in] Nametable id of exception name */
    boolean         log_error,    /* [in] TRUE => log error if name not found */
    AST_exception_n_t **excep_ptr /*[out] Ptr to AST exception node */
)
{
    AST_exception_n_t *excep_p;     /* Ptr to node bound to looked up name */
    char const      *perm_excep_name;   /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id ATTRIBUTE_UNUSED;            /* Handle on permanent copy */

    if (excep_id != NAMETABLE_NIL_ID)
    {
        excep_p = (AST_exception_n_t *)NAMETABLE_lookup_binding(excep_id);

        if (excep_p != NULL && excep_p->fe_info->node_kind == fe_exception_n_k)
        {
            *excep_ptr = excep_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        NAMETABLE_id_to_string(excep_id, &perm_excep_name);
        acf_error(acf, NIDL_EXCNOTDEF, perm_excep_name);
    }

    *excep_ptr = NULL;
    return FALSE;
}

/*
**  l o o k u p _ t y p e
**
**  Looks up a name in the nametable, and if it is bound to a valid type
**  node, returns the address of the type node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_type
(
    acf_parser_state_t *acf,
    char const      *type_name, /* [in] Name to look up */
    boolean         log_error,  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *type_id,   /*[out] Nametable id of type name */
    AST_type_n_t    **type_ptr  /*[out] Ptr to AST type node */
)
{
    AST_type_n_t    *type_p;    /* Ptr to node bound to looked up name */
    char const      *perm_type_name;    /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    *type_id = NAMETABLE_lookup_id(type_name);

    if (*type_id != NAMETABLE_NIL_ID)
    {
        type_p = (AST_type_n_t *)NAMETABLE_lookup_binding(*type_id);

        if (type_p != NULL && type_p->fe_info->node_kind == fe_type_n_k)
        {
            *type_ptr = type_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        name_id = NAMETABLE_add_id(type_name);
        NAMETABLE_id_to_string(name_id, &perm_type_name);
        acf_error(acf, NIDL_TYPNOTDEF, perm_type_name);
    }

    *type_ptr = NULL;
    return FALSE;
}

/*
**  l o o k u p _ o p e r a t i o n
**
**  Looks up a name in the nametable, and if it is bound to a valid operation
**  node, returns the address of the operation node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_operation
(
    acf_parser_state_t *acf,
    char const      *op_name,   /* [in] Name to look up */
    boolean         log_error,  /* [in] TRUE => log error if name not found */
    NAMETABLE_id_t  *op_id,     /*[out] Nametable id of operation name */
    AST_operation_n_t **op_ptr  /*[out] Ptr to AST operation node */
)
{
    AST_operation_n_t   *op_p;  /* Ptr to node bound to looked up name */
    char const      *perm_op_name;      /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    *op_id = NAMETABLE_lookup_id(op_name);

    if (*op_id != NAMETABLE_NIL_ID)
    {
        op_p = (AST_operation_n_t *)NAMETABLE_lookup_binding(*op_id);

        if (op_p != NULL && op_p->fe_info->node_kind == fe_operation_n_k)
        {
            *op_ptr = op_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        name_id = NAMETABLE_add_id(op_name);
        NAMETABLE_id_to_string(name_id, &perm_op_name);
        acf_error(acf, NIDL_OPNOTDEF, perm_op_name);
    }

    *op_ptr = NULL;
    return FALSE;
}

/*
**  l o o k u p _ p a r a m e t e r
**
**  Searches an operation node's parameter list for the parameter name passed.
**  If found, returns the address of the parameter node.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_parameter
(
    acf_parser_state_t  *acf,
    AST_operation_n_t   *op_p,          /* [in] Ptr to AST operation node */
    char const          *param_name,    /* [in] Parameter name to look up */
    boolean             log_error,      /* [in] TRUE=> log error if not found */
    NAMETABLE_id_t      *param_id,      /*[out] Nametable id of param name */
    AST_parameter_n_t   **param_ptr     /*[out] Ptr to AST parameter node */
)
{
    AST_parameter_n_t   *param_p;       /* Ptr to operation parameter node */
    char const          *op_param_name; /* Name of an operation parameter */
    char const          *op_name;       /* Operation name */
    char const      *perm_param_name;   /* Ptr to permanent copy */
    NAMETABLE_id_t  name_id;            /* Handle on permanent copy */

    for (param_p = op_p->parameters ; param_p != NULL ; param_p = param_p->next)
    {
        NAMETABLE_id_to_string(param_p->name, &op_param_name);

        if (strcmp(param_name, op_param_name) == 0)
        {
            *param_id   = param_p->name;
            *param_ptr  = param_p;
            return TRUE;
        }
    }

    if (log_error)
    {
        char const *file_name;     /* Related file name */

        NAMETABLE_id_to_string(op_p->name, &op_name);
        name_id = NAMETABLE_add_id(param_name);
        NAMETABLE_id_to_string(name_id, &perm_param_name);

        STRTAB_str_to_string(op_p->fe_info->file, &file_name);

        acf_error(acf, NIDL_PRMNOTDEF, perm_param_name, op_name);
        acf_error(acf, NIDL_NAMEDECLAT, op_name, file_name,
                  op_p->fe_info->source_line);
    }

    return FALSE;
}

/*
**  l o o k u p _ r e p _ a s _ n a m e
**
**  Scans a list of type nodes that have represent_as types for a match with
**  the type name given by the parameter repr_name_id.  If so, returns the
**  address of the found type node and a pointer to the associated
**  represent_as type name.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_rep_as_name
(
    AST_type_p_n_t  *typep_p,           /* [in] Listhead of type ptr nodes */
    NAMETABLE_id_t  repr_name_id,       /* [in] represent_as name to look up */
    AST_type_n_t    **ret_type_p,       /*[out] Type node if found */
    char const      **ret_type_name     /*[out] Type name if found */
)
{
    AST_type_n_t    *type_p;            /* Ptr to a type node */

    for ( ; typep_p != NULL ; typep_p = typep_p->next )
    {
        type_p = typep_p->type;
        if (type_p->name == repr_name_id)
        {
            *ret_type_p = type_p;
            NAMETABLE_id_to_string(type_p->rep_as_type->type_name,
                                   ret_type_name);
            return TRUE;
        }
    }

    return FALSE;
}

/*
**  l o o k u p _ c s _ c h a r _ n a m e
**
**  Scans a list of type nodes that have cs_char types for a match with
**  the type name given by the parameter cs_char_name_id.  If so, returns the
**  address of the found type node and a pointer to the associated
**  cs_char type name.
**
**  Returns:    TRUE if lookup succeeds, FALSE otherwise.
*/

static boolean lookup_cs_char_name
(
    AST_type_p_n_t  *typep_p,           /* [in] Listhead of type ptr nodes */
    NAMETABLE_id_t  cs_char_name_id,    /* [in] cs_char name to look up */
    AST_type_n_t    **ret_type_p,       /*[out] Type node if found */
    char const      **ret_type_name     /*[out] Type name if found */
)
{
    AST_type_n_t    *type_p;            /* Ptr to a type node */

    for ( ; typep_p != NULL ; typep_p = typep_p->next )
    {
        type_p = typep_p->type;
        if (type_p->name == cs_char_name_id)
        {
            *ret_type_p = type_p;
            NAMETABLE_id_to_string(type_p->cs_char_type->type_name,
                                   ret_type_name);
            return TRUE;
        }
    }

    return FALSE;
}

/*
 *  a c f _ a l l o c _ p a r a m
 *
 *  Function:   Allocates an acf_param_t, either from the free list or heap.
 *
 *  Returns:    Address of acf_param_t
 *
 *  Globals:    parameter_free_list - listhead for free list
 *
 *  Side Effects:   Exits program if unable to allocate memory.
 */

static acf_param_t *alloc_param
(
    acf_parser_state_t * acf
)
{
    acf_param_t *p;     /* Ptr to parameter record */

    if (acf->acf_parameter_free_list != NULL)
    {
        p = acf->acf_parameter_free_list;
        acf->acf_parameter_free_list = acf->acf_parameter_free_list->next;
    }
    else
    {
        p = NEW (acf_param_t);
        p->next                 = NULL;
        p->parameter_attr.mask  = 0;
        p->param_id             = NAMETABLE_NIL_ID;
    }

    return p;
}

/*
 *  a c f _ f r e e _ p a r a m
 *
 *  Function:   Frees an acf_param_t by reinitilizing it and returning it to
 *              the head of the free list.
 *
 *  Input:      p - Pointer to acf_param_t record
 *
 *  Globals:    parameter_free_list - listhead for free list
 */

static void free_param
(
    acf_parser_state_t * acf,
    acf_param_t *p              /* [in] Pointer to acf_param_t record */
)
{
    p->parameter_attr.mask  = 0;
    p->param_id             = NAMETABLE_NIL_ID;

    p->next                 = acf->acf_parameter_free_list;
    acf->acf_parameter_free_list     = p;
}

/*
 *  a c f _ f r e e _ p a r a m _ l i s t
 *
 *  Function:   Frees a list of acf_param_t records.
 *
 *  Input:      list - Address of list pointer
 *
 *  Output:     list pointer = NULL
 */

static void free_param_list
(
    acf_parser_state_t * acf,
    acf_param_t **list          /* [in] Address of list pointer */
)
{
    acf_param_t *p, *q;     /* Ptrs to parameter record */

    p = *list;

    while (p != NULL)
    {
        q = p;
        p = p->next;
        free_param(acf, q);
    }

    *list = NULL;            /* List now empty */
}

/*
 *  a d d _ p a r a m _ t o _ l i s t
 *
 *  Function:   Add a acf_param_t record to the end of a list.
 *
 *  Inputs:     p - Pointer to parameter record
 *              list - Address of list pointer
 *
 *  Outputs:    List is modified.
 */

void add_param_to_list
(
    acf_param_t *p,             /* [in] Pointer to parameter record */
    acf_param_t **list          /* [in] Address of list pointer */
)
{
    acf_param_t *q;         /* Ptr to parameter record */

    if (*list == NULL)      /* If list empty */
        *list = p;          /* then list now points at param */
    else
    {
        for (q = *list ; q->next != NULL ; q = q->next)
            ;
        q->next = p;        /* else last record in list now points at param */
    }

    p->next = NULL;         /* Param is now last in list */
}

/*
**  a p p e n d _ p a r a m e t e r
**
**  Appends a parameter to an operation's parameter list.
*/

static void append_parameter
(
    acf_parser_state_t  *acf,
    AST_operation_n_t   *op_p,          /* [in] Ptr to AST operation node */
    char const          *param_name,    /* [in] Parameter name */
    acf_attrib_t        *param_attr     /* [in] Parameter attributes */
)
{
    NAMETABLE_id_t      new_param_id;   /* Nametable id of new parameter name */
    AST_parameter_n_t   *new_param_p;   /* Ptr to new parameter node */
    AST_type_n_t        *new_type_p;    /* Ptr to new parameter type node */
    AST_pointer_n_t     *new_ptr_p;     /* Ptr to new pointer node */
    NAMETABLE_id_t      status_id;      /* Nametable id of status_t */
    AST_type_n_t        *status_type_p; /* Type node bound to status_t name */
    AST_parameter_n_t   *param_p;       /* Ptr to operation parameter node */

    /* Look up error_status_t type. */
    status_id = NAMETABLE_add_id("error_status_t");
    status_type_p = (AST_type_n_t *)NAMETABLE_lookup_binding(status_id);
    if (status_type_p == NULL)
    {
        acf_error(acf, NIDL_ERRSTATDEF, "error_status_t", "nbase.idl");
        return;
    }

    /*
     * Have to create an '[out] error_status_t *param_name' parameter
     * that has the specified parameter attributes.
     */
    new_param_id = NAMETABLE_add_id(param_name);
    new_param_p = AST_parameter_node(acf_location(acf), new_param_id);
    new_type_p  = AST_type_node(acf_location(acf), AST_pointer_k);
    new_ptr_p   = AST_pointer_node(acf_location(acf), status_type_p);

    new_type_p->type_structure.pointer = new_ptr_p;
    AST_SET_REF(new_type_p);

    new_param_p->name = new_param_id;
    new_param_p->type = new_type_p;
    new_param_p->uplink = op_p;
    if (param_attr->bit.comm_status)
        AST_SET_ADD_COMM_STATUS(new_param_p);
    if (param_attr->bit.fault_status)
        AST_SET_ADD_FAULT_STATUS(new_param_p);
    AST_SET_OUT(new_param_p);
    AST_SET_REF(new_param_p);

    param_p = op_p->parameters;
    if (param_p == NULL)
    {
        /* Was null param list, now has one param. */
        op_p->parameters = new_param_p;
    }
    else if (param_p->last == NULL)
    {
        /* Was one param, now have two params. */
        param_p->next = new_param_p;
        param_p->last = new_param_p;
    }
    else
    {
        /* Was more than one param, now have one more. */
        param_p->last->next = new_param_p;
        param_p->last = new_param_p;
    }
}

/*
**  p r o c e s s _ r e p _ a s _ t y p e
**
**  Processes a [represent_as] clause applied to a type.  Validates that
**  [represent_as] types are not nested.  Adds the type to a list of types
**  that have the [represent_as] attribute.
*/

static void process_rep_as_type
(
    acf_parser_state_t  *acf,
    AST_interface_n_t   *int_p,     /* [in] Ptr to AST interface node */
    AST_type_n_t        *type_p,    /* [in] Ptr to AST type node */
    char const      *ref_type_name  /* [in] Name in represent_as() clause */
)
{
    NAMETABLE_id_t  ref_type_id;    /* Nametable id of referenced name */
    char const      *file_name;     /* Related file name */
    char const      *perm_name;     /* Permanent copy of referenced name */
    AST_type_n_t    *parent_type_p; /* Parent type with same attribute */
    char const      *parent_name;   /* Name of parent type */

    ref_type_id = NAMETABLE_add_id(ref_type_name);

    /*
     * Report error if the type name referenced in the attribute is an AST
     * type which also has the same attribute, i.e. types with this attribute
     * cannot nest.
     */
    if (lookup_rep_as_name(int_p->ra_types, ref_type_id, &parent_type_p,
                           &perm_name))
    {
        NAMETABLE_id_to_string(parent_type_p->name, &parent_name);
        STRTAB_str_to_string(parent_type_p->fe_info->acf_file, &file_name);

        acf_error(acf, NIDL_REPASNEST);
        acf_error(acf, NIDL_TYPEREPAS, parent_name, perm_name);
        acf_error(acf, NIDL_NAMEDECLAT, parent_name, file_name,
                  parent_type_p->fe_info->acf_source_line);
    }

    /*
     * If the type node already has a type name for this attribute,
     * this one must duplicate that same name.
     */
    if (type_p->rep_as_type != NULL)
    {
        NAMETABLE_id_to_string(type_p->rep_as_type->type_name, &perm_name);

        if (strcmp(perm_name, ref_type_name) != 0)
        {
            char const *new_ref_type_name; /* Ptr to permanent copy */
            NAMETABLE_id_t  name_id;       /* Handle on perm copy */

            name_id = NAMETABLE_add_id(ref_type_name);
            NAMETABLE_id_to_string(name_id, &new_ref_type_name);

            STRTAB_str_to_string(
                            type_p->rep_as_type->fe_info->acf_file, &file_name);

            acf_error(acf, NIDL_CONFREPRTYPE, new_ref_type_name, perm_name);
            acf_error(acf, NIDL_NAMEDECLAT, perm_name, file_name,
                      type_p->rep_as_type->fe_info->acf_source_line);
        }
    }
    else
    {
        /*
         * Process valid [represent_as] clause.
         */
        AST_type_p_n_t  *typep_p;       /* Used to link type nodes */
        AST_rep_as_n_t  *repas_p;       /* Ptr to represent_as node */

        /* Add represent_as type name and build rep_as AST node. */

        repas_p = type_p->rep_as_type =
	    AST_represent_as_node(acf_location(acf),ref_type_id);
        /* Store source information. */
        if (repas_p->fe_info != NULL)
        {
            repas_p->fe_info->acf_file = acf->acf_location.fileid;
            repas_p->fe_info->acf_source_line = acf_yylineno(acf);
        }

        /* Check for associated def-as-tag node. */

        if (type_p->fe_info->tag_ptr != NULL)
            type_p->fe_info->tag_ptr->rep_as_type = type_p->rep_as_type;

        /* Link type node into list of represent_as types. */

        typep_p = AST_type_ptr_node(acf_location(acf));
        typep_p->type = type_p;

        int_p->ra_types = (AST_type_p_n_t *)AST_concat_element(
                                                (ASTP_node_t *)int_p->ra_types,
                                                (ASTP_node_t *)typep_p);
    }
}

/*
**  p r o c e s s _ c s _ c h a r _ t y p e
**
**  Processes a [cs_char] clause applied to a type.  Validates that
**  [cs_char] types are not nested.  Adds the type to a list of types
**  that have the [cs_char] attribute.
*/

static void process_cs_char_type
(
    acf_parser_state_t  *acf,
    AST_interface_n_t   *int_p,     /* [in] Ptr to AST interface node */
    AST_type_n_t        *type_p,    /* [in] Ptr to AST type node */
    char const      *ref_type_name  /* [in] Name in cs_char() clause */
)
{
    NAMETABLE_id_t  ref_type_id;    /* Nametable id of referenced name */
    char const      *file_name;     /* Related file name */
    char const      *perm_name;     /* Permanent copy of referenced name */
    AST_type_n_t    *parent_type_p; /* Parent type with same attribute */
    char const      *parent_name;   /* Name of parent type */

    ref_type_id = NAMETABLE_add_id(ref_type_name);

    /*
     * Report error if the type name referenced in the attribute is an AST
     * type which also has the same attribute, i.e. types with this attribute
     * cannot nest.
     */
    if (lookup_cs_char_name(int_p->cs_types, ref_type_id, &parent_type_p,
                            &perm_name))
    {
        NAMETABLE_id_to_string(parent_type_p->name, &parent_name);
        STRTAB_str_to_string(parent_type_p->fe_info->acf_file, &file_name);

        /*** This needs updating ***/
        acf_error(acf, NIDL_REPASNEST);
        acf_error(acf, NIDL_TYPEREPAS, parent_name, perm_name);
        acf_error(acf, NIDL_NAMEDECLAT, parent_name, file_name,
                  parent_type_p->fe_info->acf_source_line);
    }

    /*
     * If the type node already has a type name for this attribute,
     * this one must duplicate that same name.
     */
    if (type_p->cs_char_type != NULL)
    {
        NAMETABLE_id_to_string(type_p->cs_char_type->type_name, &perm_name);

        if (strcmp(perm_name, ref_type_name) != 0)
        {
            char const *new_ref_type_name; /* Ptr to permanent copy */
            NAMETABLE_id_t  name_id;    /* Handle on perm copy */

            name_id = NAMETABLE_add_id(ref_type_name);
            NAMETABLE_id_to_string(name_id, &new_ref_type_name);

            STRTAB_str_to_string(
                        type_p->cs_char_type->fe_info->acf_file, &file_name);

            /*** This needs updating ***/
            acf_error(acf, NIDL_CONFREPRTYPE, new_ref_type_name, perm_name);
            acf_error(acf, NIDL_NAMEDECLAT, perm_name, file_name,
                      type_p->cs_char_type->fe_info->acf_source_line);
        }
    }
    else
    {
        /*
         * Process valid [cs_char] clause.
         */
        AST_type_p_n_t  *typep_p;       /* Used to link type nodes */
        AST_cs_char_n_t *cschar_p;      /* Ptr to cs_char node */

        /* Add cs_char type name and build cs_char AST node. */

        cschar_p = type_p->cs_char_type = AST_cs_char_node(
				acf_location(acf), ref_type_id);
        /* Store source information. */
        if (cschar_p->fe_info != NULL)
        {
            cschar_p->fe_info->acf_file = acf->acf_location.fileid;
            cschar_p->fe_info->acf_source_line = acf_yylineno(acf);
        }

        /* Check for associated def-as-tag node. */

        if (type_p->fe_info->tag_ptr != NULL)
            type_p->fe_info->tag_ptr->cs_char_type = type_p->cs_char_type;

        /* Link type node into list of cs_char types. */

        typep_p = AST_type_ptr_node(acf_location(acf));
        typep_p->type = type_p;

        int_p->cs_types = (AST_type_p_n_t *)AST_concat_element(
                                                (ASTP_node_t *)int_p->cs_types,
                                                (ASTP_node_t *)typep_p);
    }
}

/*
 *  d u m p _ a t t r i b u t e s
 *
 *  Function:   Prints list of attributes parsed for a particular node type
 *
 *  Inputs:     header_text - Initial text before node name and attributes
 *              node_name   - Name of interface, type, operation, or parameter
 *              node_attr_p - Address of node attributes structure
 *
 *  Globals:    repr_type_name  - represent_as type name, used if bit is set
 *              cs_char_type_name - cs_char type name, used if bit is set
 *              cs_tag_rtn_name - cs_tag_rtn name, used if bit is set
 *              binding_callout_name - binding_callout name, used if bit is set
 */

static void dump_attributes
(
    acf_parser_state_t *acf,
    const char	    *header_text,       /* [in] Initial output text */
    char const      *node_name,         /* [in] Name of tree node */
    acf_attrib_t    *node_attr_p        /* [in] Node attributes ptr */
)
#define MAX_ATTR_TEXT   1024    /* Big enough for lots of extern_exceptions */
{
    char            attr_text[MAX_ATTR_TEXT];   /* Buf for formatting attrs */
    int             pos;                /* Position in buffer */
    acf_attrib_t    node_attr;          /* Node attributes */

    node_attr = *node_attr_p;

    printf("%s %s", header_text, node_name);

    if (node_attr.mask == 0)
    {
        printf("\n");
    }
    else
    {
        printf(" attributes: ");
        strlcpy(attr_text, "[", sizeof (attr_text));

        if (node_attr.bit.auto_handle)
            strlcat(attr_text, "auto_handle, ", sizeof(attr_text));
        if (node_attr.bit.code)
            strlcat(attr_text, "code, ", sizeof(attr_text));
        if (node_attr.bit.nocode)
            strlcat(attr_text, "nocode, ", sizeof(attr_text));
        if (node_attr.bit.comm_status)
            strlcat(attr_text, "comm_status, ", sizeof(attr_text));
        if (node_attr.bit.decode)
            strlcat(attr_text, "decode, ", sizeof(attr_text));
        if (node_attr.bit.enable_allocate)
            strlcat(attr_text, "enable_allocate, ", sizeof(attr_text));
        if (node_attr.bit.encode)
            strlcat(attr_text, "encode, ", sizeof(attr_text));
        if (node_attr.bit.explicit_handle)
            strlcat(attr_text, "explicit_handle, ", sizeof(attr_text));
        if (node_attr.bit.nocancel)
            strlcat(attr_text, "nocancel, ", sizeof(attr_text));
        if (node_attr.bit.extern_exceps && ASTP_parsing_main_idl)
        {
            AST_exception_n_t   *excep_p;
            char const               *name;
            strlcat(attr_text, "extern_exceptions(", sizeof(attr_text));
            for (excep_p = the_interface->exceptions;
                 excep_p != NULL;
                 excep_p = excep_p->next)
            {
                if (AST_EXTERN_SET(excep_p))
                {
                    NAMETABLE_id_to_string(excep_p->name, &name);
                    strlcat(attr_text, name, sizeof(attr_text));
                    strlcat(attr_text, ",", sizeof(attr_text));
                }
            }
            attr_text[strlen(attr_text)-1] = '\0';  /* overwrite trailing ',' */
            strlcat(attr_text, "), ", sizeof(attr_text));
        }
        if (node_attr.bit.fault_status)
            strlcat(attr_text, "fault_status, ", sizeof(attr_text));
        if (node_attr.bit.heap)
            strlcat(attr_text, "heap, ", sizeof(attr_text));
        if (node_attr.bit.implicit_handle)
            strlcat(attr_text, "implicit_handle, ", sizeof(attr_text));
        if (node_attr.bit.in_line)
            strlcat(attr_text, "in_line, ", sizeof(attr_text));
        if (node_attr.bit.out_of_line)
            strlcat(attr_text, "out_of_line, ", sizeof(attr_text));
        if (node_attr.bit.cs_stag)
            strlcat(attr_text, "cs_stag, ", sizeof(attr_text));
        if (node_attr.bit.cs_drtag)
            strlcat(attr_text, "cs_drtag, ", sizeof(attr_text));
        if (node_attr.bit.cs_rtag)
            strlcat(attr_text, "cs_rtag, ", sizeof(attr_text));
        if (node_attr.bit.represent_as)
        {
            strlcat(attr_text, "represent_as(", sizeof(attr_text));
            strlcat(attr_text, acf->acf_repr_type_name, sizeof(attr_text));
            strlcat(attr_text, "), ", sizeof(attr_text));
        }
        if (node_attr.bit.cs_char)
        {
            strlcat(attr_text, "cs_char(", sizeof(attr_text));
            strlcat(attr_text, acf->acf_cs_char_type_name, sizeof(attr_text));
            strlcat(attr_text, "), ", sizeof(attr_text));
        }
        if (node_attr.bit.cs_tag_rtn)
        {
            strlcat(attr_text, "cs_tag_rtn(", sizeof(attr_text));
            strlcat(attr_text, acf->acf_cs_tag_rtn_name, sizeof(attr_text));
            strlcat(attr_text, "), ", sizeof(attr_text));
        }
        if (node_attr.bit.binding_callout)
        {
            strlcat(attr_text, "binding_callout(", sizeof(attr_text));
            strlcat(attr_text, acf->acf_binding_callout_name, sizeof(attr_text));
            strlcat(attr_text, "), ", sizeof(attr_text));
        }

        /* Overwrite trailing ", " with "]" */

        pos = strlen(attr_text) - strlen(", ");
        attr_text[pos] = ']';
        attr_text[pos+1] = '\0';

        printf("%s\n", attr_text);
    }
}

/* preserve coding style vim: set tw=78 sw=4 : */
