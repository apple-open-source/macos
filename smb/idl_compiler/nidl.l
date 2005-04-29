%{
 /*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
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
**  NAME:
**
**      IDL.L
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      This file defines the tokenizing rules for lexical analysis.
**
**  VERSION: DCE 1.0
**
*/



/*  Get definitions for token values    */
#define PROCESSING_LEX          /* Define before including nidl.h */
#define PROCESSING_NIDL_LEX     /* Define before including nidl.h */
#include <nidl.h>
#include <ctype.h>
#include <errors.h>
#include <nametbl.h>
#include <ast.h>
#include <astp.h>
#include <y_tab.h>
#include <driver.h>
#include <command.h>

#ifdef GNU_LEX_YACC
#define YY_DECL int yylex(YYSTYPE *yylval)
#define YYLVAL(member)	((yylval)->member)
#else
#define YYLVAL(member)	yylval.member
#endif

boolean search_attributes_table = false ;

#ifndef MAX_INCLUSION_DEPTH
#define MAX_INCLUSION_DEPTH 10
#endif



void commenteof (
    void
);

#ifndef YY_DECL
int yylex (
    void
);
#endif

int yylook (
    void
);

static void read_c_comment (
    void
);

static void yymark (
    void
);

#ifdef GNU_LEX_YACC
int yywrap (void);
#else
static int yywrap (
    void
);
#endif

static int yyuuid (
    char *s,
    nidl_uuid_t *uuid
);

static int yyolduuid (
    char *s,
    nidl_uuid_t *uuid
);



int KEYWORDS_screen (
    char *token,
    NAMETABLE_id_t *id
);

char *KEYWORDS_lookup_text (
    long token
);

int yyinput (
    void
);

#ifndef _AIX
int yyoutput (
    int c
);

#if !defined (GNU_LEX_YACC)
int yyunput (
    int c
);
#endif /* GNU_LEX_YACC */
#endif /* !_AIX */

int yyback (
    int *p,

    int m
);


%}


/* regular definitions  */

delim           [ \t\n\f]
opt_ws          {delim}*
ws              {delim}
letter          [A-Za-z_$]
digit           [0-9]
hex_digit       [0-9A-Fa-f]
id              {letter}({letter}|{digit})*
l_suffix        (l|L)
u_suffix        (u|U)
f_suffix        (u|U)
integer         -?{digit}+(({u_suffix}{l_suffix})|({l_suffix}{u_suffix})|{l_suffix}|{u_suffix})?
c_hex_integer   (0(x|X){hex_digit}*)(({l_suffix}{u_suffix}?)|({u_suffix}{l_suffix}?))?
float           {digit}+\.{digit}+({f_suffix}|{l_suffix})?
octet           {hex_digit}{hex_digit}
octet2          {octet}{octet}
octet_dot       {octet}\.
octet2_dash     {octet2}\-
olduuid         \({opt_ws}{octet2}{octet2}{octet}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet_dot}{octet}{opt_ws}\)
uuid            \({opt_ws}{octet2}{octet2_dash}{octet2_dash}{octet2_dash}{octet2_dash}{octet2}{octet2}{octet2}{opt_ws}\)
other           .

%%
{ws}            { /* No action, and no return */ }

":"             {return(COLON);}
","             {return(COMMA);}
".."            {return(DOTDOT);}
"="             {return(EQUAL);}
\[              {return(LBRACKET);}
"("             {return(LPAREN);}
\]              {return(RBRACKET);}
")"             {return(RPAREN);}
";"             {return(SEMI);}
"*"             {return(STAR);}
"{"             {return(LBRACE);}
"??<"           {return(LBRACE);}
"}"             {return(RBRACE);}
"??>"           {return(RBRACE);}
"?"             {return(QUESTION);}
"|"             {return(BAR);}
"||"            {return(BARBAR);}
"<"             {return(LANGLE);}
"<<"            {return(LANGLEANGLE);}
">"             {return(RANGLE);}
">>"            {return(RANGLEANGLE);}
"&"             {return(AMP);}
"&&"            {return(AMPAMP);}
"<="            {return(LESSEQUAL);}
">="            {return(GREATEREQUAL);}
"=="            {return(EQUALEQUAL);}
"^"             {return(CARET);}
"+"             {return(PLUS);}
"-"             {return(MINUS);}
"!"             {return(NOT);}
"!="            {return(NOTEQUAL);}
"/"             {return(SLASH);}
"%"             {return(PERCENT);}
"~"             {return(TILDE);}

^"#".*\n            { yymark() ;}
^"%"(c|C){ws}\n     { log_error(yylineno-1,NIDL_USETRANS); }
^"%pascal"{ws}\n    { log_error(yylineno-1,NIDL_USETRANS); }
^"%PASCAL"{ws}\n    { log_error(yylineno-1,NIDL_USETRANS); }

'\\''   {
    /* Simple escaped single quote character literal */
    YYLVAL(y_char) = '\'';
    return(CHAR);
}

'[^'\n\\]'  {
    /* Simple character constants */
    YYLVAL(y_char) = yytext [1];
    return(CHAR);
}

'\\[^'\n]*' {
    /* Character constants with an escape */
    if ((yyleng > 6) || (yyleng < 4)) {
          log_error(yylineno, NIDL_INVCHARLIT);
          return(UNKNOWN);
    }
    switch (yytext[2]) {
        case 'n':   YYLVAL(y_char) = '\n'; break;
        case 't':   YYLVAL(y_char) = '\t'; break;
        case 'v':   YYLVAL(y_char) = '\v'; break;
        case 'b':   YYLVAL(y_char) = '\b'; break;
        case 'r':   YYLVAL(y_char) = '\r'; break;
        case 'f':   YYLVAL(y_char) = '\f'; break;
        case 'a':   YYLVAL(y_char) = AUDIBLE_BELL; break;
        case '\\':  YYLVAL(y_char) = '\\'; break;
        case '?':   YYLVAL(y_char) = '\?'; break;
        case '\'':  YYLVAL(y_char) = '\''; break;
        case '\"':  YYLVAL(y_char) = '\"'; break;
        case 'x' :  {
            /* Hex literal value */
            int char_value;
            if (sscanf((char *)&yytext[3],"%x",&char_value) != 1)
                log_error(yylineno, NIDL_INVCHARLIT);
            else
            {
                YYLVAL(y_char) = (char )char_value;
                log_warning(yylineno, NIDL_NONPORTCHAR);
            }
            break;
        }
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '7':   {
            /* Octal literal value */
            int char_value;
            if (sscanf((char *)&yytext[2],"%o",&char_value) != 1)
                log_error(yylineno, NIDL_INVCHARLIT);
            else
            {
                YYLVAL(y_char) = (char )char_value;
                log_warning(yylineno, NIDL_NONPORTCHAR);
            }
            break;
        }
        default:
            /* all others are illegal */
            log_error(yylineno, NIDL_INVCHARLIT);
            return(UNKNOWN);
    }

    return (CHAR);
}

\"[^\"\n]*  {
    if (yytext[yyleng-1] == '\\')
        yymore(); /* Allow \" within strings, look for next " */
    else {
    YYLVAL(y_string) = STRTAB_add_string((char *)&yytext[1]);
        if (input() == '\n')
            log_error(yylineno, NIDL_STRUNTERM);
        return(STRING);
    }
}

"/*"    { read_c_comment(); }

{id}    {
    int token;
    NAMETABLE_id_t id;
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
    if ((token = KEYWORDS_screen((char *)yytext, &id))==IDENTIFIER) {
        YYLVAL(y_id) = id;
    }
    else {
        YYLVAL(y_id) = NAMETABLE_NIL_ID;
    }
    return token;
}


{integer}   {
    int unsigned_int = false;
#if defined(vax) && defined(ultrix)
    float fval;
    char *fval_fmt = "%f";
#define FLOAT float
#else
    double fval;
    char *fval_fmt = "%lf";
#define FLOAT double
#endif

    /*
    ** Remove suffix for long and/or unsigned, if present
    */
    if ((yytext[yyleng-1] == 'L') ||
        (yytext[yyleng-1] == 'l') ||
        (yytext[yyleng-1] == 'U') ||
        (yytext[yyleng-1] == 'u'))
    {
        if ((yytext[yyleng-1] == 'U') ||
            (yytext[yyleng-1] == 'u'))
                unsigned_int = true;
        yytext[yyleng-1] = '\0';
        if ((yytext[yyleng-2] == 'L') ||
            (yytext[yyleng-2] == 'l') ||
            (yytext[yyleng-2] == 'U') ||
            (yytext[yyleng-2] == 'u'))
        {
            if ((yytext[yyleng-2] == 'U') ||
                (yytext[yyleng-2] == 'u'))
                    unsigned_int = true;
            yytext[yyleng-2] = '\0';
        }
    }

    /*
    ** Convert to a float to get overflow detection.
    */
    sscanf((char *)yytext, fval_fmt, &fval);
    YYLVAL(y_ival) = 0;

    /*
    ** Throw out integers that are out of range.
    */
    if (unsigned_int &&
        ((strlen((char *)yytext) > 10)
        || (fval > (FLOAT)ASTP_C_ULONG_MAX)
        || (fval < (FLOAT)ASTP_C_ULONG_MIN)
        ))
        log_error(yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
    else if ((strlen((char *)yytext) > 11) ||
        (fval > (FLOAT)ASTP_C_LONG_MAX) ||
        (fval < (FLOAT)ASTP_C_LONG_MIN)
        )
        log_error(yylineno, NIDL_INTOVERFLOW, KEYWORDS_lookup_text(LONG_KW));
    else {
        sscanf((char *)yytext, "%i", &YYLVAL(y_ival));
    }


    if ((yytext[0] == '0') && (strlen((char *)yytext) != strspn((char *)yytext,"01234567"))) {
        char    *int_text;              /* Text of integer */
        STRTAB_str_t string_id;         /* Entry in string table of integer */
        string_id = STRTAB_add_string(yytext_p);
        STRTAB_str_to_string(string_id, &int_text);
        log_error(yylineno, NIDL_INVOCTDIGIT, int_text);
    }

    return(INTEGER_NUMERIC);
}

{c_hex_integer} {
    /*
    ** Remove suffix for long and/or unsigned, if present
    */
    if ((yytext[yyleng-1] == 'L') ||
        (yytext[yyleng-1] == 'l') ||
        (yytext[yyleng-1] == 'U') ||
        (yytext[yyleng-1] == 'u'))
    {
        yytext[yyleng-1] = '\0';
        if ((yytext[yyleng-2] == 'L') ||
            (yytext[yyleng-2] == 'l') ||
            (yytext[yyleng-2] == 'U') ||
            (yytext[yyleng-2] == 'u'))
        {
            yytext[yyleng-2] = '\0';
        }
    }

    /*
    ** Scan the hex integer and return the value as an integer
    */
    sscanf((char *)&yytext[2],"%x", &YYLVAL(y_ival));
    return(INTEGER_NUMERIC);
}

{float} {
    YYLVAL(y_float) = STRTAB_add_string((char *)yytext);
    return(FLOAT_NUMERIC);
}

{uuid}      {   return (yyuuid((char *)&yytext[1], &YYLVAL(y_uuid))); }
{olduuid}   {   return (yyolduuid((char *)&yytext[1], &YYLVAL(y_uuid))); }
{other}     {   return (UNKNOWN); }

%%

void commenteof()
{
    log_error (yylineno, NIDL_COMMENTEOF);
    nidl_terminate();
}

#ifdef GNU_LEX_YACC
int yywrap ()
#else
static int yywrap ()
#endif
{
    /*
     * With Flex, this causes infinite recursion between "yywrap()" and
     * "input()"; does it not do so with Lex?
     */
#ifndef GNU_LEX_YACC
    int    c ;
    if ((c = input()) != 0)
    {
        unput(c) ;
        return (0) ;
    }
#endif
    yylineno = 1 ;
        return (1);
}

static void yymark()
{
    char    *source ; /* Source file name in #line directive */
    int prev_lineno = yylineno - 1;

    if (strncmp((char *)yytext, "#pragma", 7) == 0)
    {
        /* Just ignore #pragma. */
        return;
    }
    source = (char *) MALLOC(yyleng) ;
    source[0] ='\0';
    /* If the format wasn't # line {int} {string}, then reparse without "line" identifier */
    if (sscanf((char *)yytext, "# line %d %s", &yylineno, source) < 1)
    {
	if (sscanf((char *)yytext, "# %d %s", &yylineno, source) < 1)
        {
            log_warning(prev_lineno, NIDL_CPPCMDOPT,
#ifdef VMS
                (CMD_DCL_interface) ? "/PREPROCESS" :
#endif
                "-cpp_cmd");
        }
    }

    /* If text included a source file name, set name for error reporting. */
    if (source[0] != '\0'
        &&  !(source[0] == '"' && source[1] == '"'))
    {
        char    file_name[max_string_len];

        /* Strip the quotes. */
        strcpy(file_name, &source[1]);
        file_name[strlen(file_name)-1] = '\0';

        set_name_for_errors(file_name);
    }

    FREE(source);
}

static void read_c_comment()
{
    register int c;

    /* While not EOF look for end of comment */
    while ((c = input()))
    {
        if (c == '*')
        {
            if ((c = input()) == '/')
                break ;
            else
                unput(c) ;
        }
    }

    /* Didn't find end comment before EOF, issue error */
    if (c == 0) commenteof();
}


static int yyolduuid(s, uuid)
char *s;
nidl_uuid_t *uuid;
{
    unsigned32 v1_time_high;
    unsigned32 v1_time_low;
    unsigned32 v1_reserved = 0;  /* v1 UUID always zero here */
    unsigned32 v1_family;
    unsigned32 v1_host[7];
    int        i;
    char *uuid_str;

    if (sscanf(s, "%8X%4X.%2X.%2X.%2X.%2X.%2X.%2X.%2X.%2X",
              &v1_time_high, &v1_time_low, &v1_family,
              &v1_host[0], &v1_host[1], &v1_host[2], &v1_host[3],
              &v1_host[4], &v1_host[5], &v1_host[6]) != 10)
        log_error(yylineno, NIDL_SYNTAXUUID);

    /* scanf only returns ints, so scan into ints and copy into smaller types */
    uuid->time_low                  = v1_time_high;
    uuid->time_mid                  = v1_time_low;
    uuid->time_hi_and_version       = v1_reserved;
    uuid->clock_seq_hi_and_reserved = v1_family;
    uuid->clock_seq_low             = v1_host[0];

    for (i=0; i < 6; i++)
        uuid->node[i] = v1_host[i+1];

    uuid_str = (char *)malloc(sizeof("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"));

    sprintf(uuid_str, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
              uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
              uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
              uuid->node[0], uuid->node[1], uuid->node[2],
              uuid->node[3], uuid->node[4], uuid->node[5]);

    log_warning(yylineno, NIDL_OLDUUID);
    log_warning(yylineno, NIDL_NEWUUID, uuid_str);
    return (UUID_REP) ;
}

static int yyuuid(s, uuid)
char *s;
nidl_uuid_t *uuid;
{
    int i;
    unsigned32 time_low, time_mid, time_hi_and_version,
              clock_seq_hi_and_reserved, clock_seq_low,
              node[6];

    if (sscanf(s, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
              &time_low, &time_mid, &time_hi_and_version,
              &clock_seq_hi_and_reserved, &clock_seq_low,
              &node[0], &node[1], &node[2],
              &node[3], &node[4], &node[5]) != 11)
        log_error(yylineno, NIDL_SYNTAXUUID);

    /* scanf only returns ints, so scan into ints and copy into smaller types */
    uuid->time_low = time_low;
    uuid->time_mid = time_mid;
    uuid->time_hi_and_version = time_hi_and_version;
    uuid->clock_seq_hi_and_reserved = clock_seq_hi_and_reserved;
    uuid->clock_seq_low = clock_seq_low;
    for (i=0; i < 6; i++)
        uuid->node[i] = node[i];
    return (UUID_REP);
}

/*
 * Define some entry points for frontend with GNU Bison & Flex.  These
 * save and restore the Flex state.  Flex doesn't provide this functionality
 * so I had to work it from the lex_yy.c source code.
 */

#ifdef GNU_LEX_YACC

#include "flex_state.h"

void	flex_SaveState (struct FlexState *Save)
{

/*
 * Save the state information
 */

    Save->yy_start		  = yy_start;
    Save->yy_init		  = yy_init;
    Save->yy_more_flag		  = yy_more_flag;
    Save->yy_more_len		  = yy_more_len;
    Save->yy_did_buffer_switch_on_eof = yy_did_buffer_switch_on_eof;
    Save->yy_last_accepting_state = yy_last_accepting_state;
    Save->yy_last_accepting_cpos  = yy_last_accepting_cpos;
    Save->yy_current_buffer	  = (void *)yy_current_buffer;

#if 1
/*
 * We need to save these in yy_current_buffer without doing a calling
 * yy_switch_to_buffer because we don't want the full switch buffer
 * functionality.  We just want to save the state, not initiate a
 * new buffer.
 */

    if (Save->yy_current_buffer != (void *)NULL)
    {
	*yy_c_buf_p = yy_hold_char;
	yy_current_buffer->yy_buf_pos = yy_c_buf_p;
	yy_current_buffer->yy_n_chars = yy_n_chars;
    }
#else
    Save->yy_hold_char            = yy_hold_char;
    Save->yy_n_chars              = yy_n_chars;
    Save->yy_c_buf_p              = yy_c_buf_p;
#endif

/* Force the next call yylex to initialise */

    yy_start = 0;
    yy_init = 1;
    yy_more_flag = 0;
    yy_more_len = 0;
    yy_did_buffer_switch_on_eof = 0;
    yy_last_accepting_state = 0;
    yy_last_accepting_cpos = NULL;
    yy_current_buffer = (YY_BUFFER_STATE)NULL;
    yy_hold_char = 0;
    yy_n_chars = 0;
    yy_c_buf_p = NULL;
}

/*
 * Restore the original state
 */

void	flex_RestoreState (struct FlexState *Save)
{
/*
 * throw away the input buffer structure from the last parse
 */

    if (yy_current_buffer != (YY_BUFFER_STATE)NULL)
	yy_delete_buffer (yy_current_buffer);

/*
 * Restore the values
 */

    yy_start		    = Save->yy_start;
    yy_init		    = Save->yy_init;
    yy_more_flag	    = Save->yy_more_flag;
    yy_more_len		    = Save->yy_more_len;
    yy_did_buffer_switch_on_eof = Save->yy_did_buffer_switch_on_eof;
    yy_last_accepting_state = Save->yy_last_accepting_state;
    yy_last_accepting_cpos  = Save->yy_last_accepting_cpos;

#if 1
/*
 * If the old input buffer structure existed, use it.  Otherwise,
 * reset to null
 */

    if (Save->yy_current_buffer != (void *)NULL)
	yy_switch_to_buffer ((YY_BUFFER_STATE)Save->yy_current_buffer);

    else
	yy_current_buffer = (YY_BUFFER_STATE)NULL;
#else
    yy_hold_char            = Save->yy_hold_char;
    yy_n_chars              = Save->yy_n_chars;
    yy_c_buf_p              = Save->yy_c_buf_p;
#endif
}
#endif
