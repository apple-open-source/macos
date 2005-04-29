%{

/*
 *  
 *  (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 *  (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 *  (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *                  permission to use, copy, modify, and distribute this
 *  file for any purpose is hereby granted without fee, provided that
 *  the above copyright notices and this notice appears in all source
 *  code copies, and that none of the names of Open Software
 *  Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 *  Corporation be used in advertising or publicity pertaining to
 *  distribution of the software without specific, written prior
 *  permission.  Neither Open Software Foundation, Inc., Hewlett-
 *  Packard Company, nor Digital Equipment Corporation makes any
 *  representations about the suitability of this software for any
 *  purpose.
 *  
 */

/*
 */

 /*
 **
 **  NAME:
 **
 **      acf.l
 **
 **  FACILITY:
 **
 **      Interface Definition Language (IDL) Compiler
 **
 **  ABSTRACT:
 **
 ** "lex" source file for Attribute Configuration File (ACF) parser.
 **
 **  VERSION: DCE 1.0
 **
 */

#define PROCESSING_LEX          /* Define before including nidl.h */
#define PROCESSING_ACF_LEX      /* Define before including nidl.h */
#include <nidl.h>               /* IDL common defs */
#include <acf_y_tab.h>          /* yacc include file */
#include <nametbl.h>            /* Nametable defs */
#include <nidlmsg.h>            /* Error message IDs */
#include <command.h>

#ifdef GNU_LEX_YACC
extern int yylineno;
#define YY_DECL int yylex(YYSTYPE *yylval)
#define YYLVAL(member)	((yylval)->member)
#else
#define YYLVAL(member)	yylval.member
#endif

extern void commenteof(         /* Called on EOF within comment */
    void
);

static void acf_yymark(         /* Handles # <lineno> left by cpp */
    void
);

static int acf_screen(          /* Screens a token to see if it is a keyword */
    char *token
#if defined (GNU_LEX_YACC)
    , YYSTYPE *yylval
#endif
);

static void acf_read_comment(   /* Reads and copies comment text */
    void
);

typedef struct                  /* Keyword and corresponding value */
{
    char    *keyword;
    int     token_value;
} keyword_entry;

static keyword_entry acf_keywords[] =   /* Table of keywords and values */
{
    {"auto_handle",     AUTO_HANDLE_KW},
    {"binding_callout", BINDING_CALLOUT_KW},
    {"code",            CODE_KW},
    {"comm_status",     COMM_STATUS_KW},
    {"cs_char",         CS_CHAR_KW},
    {"cs_tag_rtn",      CS_TAG_RTN_KW},
    {"enable_allocate", ENABLE_ALLOCATE_KW},
    {"explicit_handle", EXPLICIT_HANDLE_KW},
    {"extern_exceptions", EXTERN_EXCEPS_KW},
    {"fault_status",    FAULT_STATUS_KW},
    {"handle_t",        HANDLE_T_KW},
    {"heap",            HEAP_KW},
    {"implicit_handle", IMPLICIT_HANDLE_KW},
    {"include",         INCLUDE_KW},
    {"interface",       INTERFACE_KW},
    {"in_line",         IN_LINE_KW},
    {"nocode",          NOCODE_KW},
    {"out_of_line",     OUT_OF_LINE_KW},
    {"represent_as",    REPRESENT_AS_KW},
    {"typedef",         TYPEDEF_KW},
    {0,                 0}      /* Sentinel - Do not remove */
};

%}


delim           [ \t\n\f]
ws              {delim}+
letter          [A-Za-z_$]
digit           [0-9]
oct_digit       [0-7]
hex_digit       [0-9A-Fa-f]
id              {letter}({letter}|{digit})*
other           .

%%


{ws}        { /* White space: No action, and no return */ }

","         {return(COMMA);}
"{"         {return(LBRACE);}
"??<"       {return(LBRACE);}
"["         {return(LBRACKET);}
"("         {return(LPAREN);}
"}"         {return(RBRACE);}
"??>"       {return(RBRACE);}
"]"         {return(RBRACKET);}
")"         {return(RPAREN);}
";"         {return(SEMI);}

^"#".*\n    {acf_yymark();}

\"[^\"\n]*\" {  /* Quoted string: Strip off quotes, add to string table. */
                char    stripped_string[max_string_len];

                strcpy(stripped_string, (char *)&yytext[1]);
                stripped_string[strlen((char *)yytext)-2] = '\0';
                YYLVAL(y_string) = STRTAB_add_string(stripped_string);
                return(STRING);
            }

"/*"        {   /* Comment: Ignore through closing delimiter. */
                acf_read_comment();
            }

{id}        {
                 /* If id is too long, truncate it and issue a warning */
                 if (yyleng > MAX_ID) {
                    NAMETABLE_id_t id;
                    char *identifier;
                    id = NAMETABLE_add_id((char *)yytext);
                    NAMETABLE_id_to_string(id, &identifier);
                    log_warning(yylineno, NIDL_IDTOOLONG, identifier, MAX_ID);
                    /* Truncate the string */
                    yytext[MAX_ID] = '\0';
                    id = NAMETABLE_add_id((char *)yytext);
                    }

                /* Identifier: See if it is a valid keyword. */
#if defined (GNU_LEX_YACC)
                return acf_screen((char *)yytext, yylval);
#else
                return acf_screen((char *)yytext);
#endif
            }

{other}     {return(UNKNOWN);};
%%

/*
 *  a c f _ y y w r a p
 *
 *  Function:   Called by lexical analyzer when end-of-file reached.
 *
 *  Globals:    acf_yylineno
 *
 *  Returns:    0 => continue lexical analysis
 *              1 => wrap-up lexical analysis
 */

int acf_yywrap(void)

{
    /*
     * Note: Currently no support for one acf #including another.
     */
    return (1);
}

/*
**  a c f _ y y m a r k
**
**  Processes a "# lineno" line as left by cpp.
**
**  Implicit Outputs:
**      acf_yylineno is updated; If a filename was included in the source
**      line, set_name_for_errors is called to set up the source filename
**      for error messages.
*/

static void acf_yymark()
{
    char    *source ; /* Source file name in #line directive */
    int prev_lineno = acf_yylineno - 1;

    if (strncmp((char *)yytext, "#pragma", 7) == 0)
    {
        /* Just ignore #pragma. */
        return;
    }
    source = (char *) MALLOC(yyleng) ;
    if (sscanf((char *)yytext, "# line %d %s", &acf_yylineno, source) < 1)
    {
	if (sscanf((char *)yytext, "# %d %s", &acf_yylineno, source) < 1)
        {
            log_warning(prev_lineno, NIDL_CPPCMDOPT,
#ifdef VMS
                (CMD_DCL_interface) ? "/PREPROCESS" :
#endif
                "-cpp_cmd");
        }
    }

    /* If text included a source file name, set name for error reporting. */
    if (source[0] != '\0')
    {
        char    file_name[max_string_len];

        /* Strip the quotes. */
        strcpy(file_name, &source[1]);
        file_name[strlen(file_name)-1] = '\0';

        set_name_for_errors(file_name);
    }

    FREE(source);
}

/*
 *  a c f _ s c r e e n
 *
 *  Function:   Checks to see if a token is a keyword.
 *
 *  Inputs:     token to check
 *
 *  Returns:    specific keyword ID if token is a keyword;
 *              the value IDENTIFIER otherwise
 */

#if defined (GNU_LEX_YACC)
static int acf_screen(char *token, YYSTYPE *yylval)
#else
static int acf_screen(char *token)
#endif

{
    int i;

    /*
     * Scan the reserved word table.
     */
    for (i = 0; acf_keywords[i].keyword != 0; i++)
        if (strcmp(token, acf_keywords[i].keyword) == 0)
            return acf_keywords[i].token_value;     /* Return keyword ID */

    /*
     * Not a reserved word; therefore, an identifier, return nametable id.
     */
    YYLVAL(y_id) = NAMETABLE_add_id(token);
    return IDENTIFIER;                  /* Return IDENTIFER */
}

/*
 *  a c f _ r e a d _ c o m m e n t
 *
 *  Function:   Reads a comment in the source file.
 *
 */

static void acf_read_comment(void)

{
    int     c;

    while ( (c = input()) != 0 )
    {
        if (c == '*')
        {
            if ( (c = input()) == '/')
                break;
            else
                unput(c);
        }
        else if (c == 0)
            commenteof();
    }
}

/*
 *  a c f _ k e y w o r d _ l o o k u p
 *
 *  Looks up a keyword's text string given its numeric token value.
 */

char * acf_keyword_lookup
(
    int         token_value     /* Numeric value of keyword token */
)

{
    keyword_entry * entry;

    for (entry = acf_keywords ; entry->keyword != NULL ; entry++)
        if (entry->token_value == token_value)
            return entry->keyword;

    /* Not found, just return question mark. */
    return "?";
}


 /** End of "lex" source file. **/

