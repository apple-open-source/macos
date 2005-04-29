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
 */
/*
**
**  NAME:
**
**      acf.h
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  Include file for ACF parser.
**
**  VERSION: DCE 1.0
**
*/

/*
 *  The macros below redefine global names that are referenced by lex and/or
 *  yacc.  This allows multiple lex-and-yacc-based parsers to be present
 *  within the same program without multiply defined global names.
 *
 *  **NOTE**:  For portability, the list below must contain ALL lex/yacc
 *  function and variable names that are global in ONE OR MORE of the
 *  implementations of lex/yacc that are supported to build the IDL compiler.
 */
#define yyact       acf_yyact
#define yyback      acf_yyback
#define yybgin      acf_yybgin
#define yychar      acf_yychar
#define yychk       acf_yychk
#define yycrank     acf_yycrank
#define yydebug     acf_yydebug
#define yydef       acf_yydef
#define yyerrflag   acf_yyerrflag
#define yyestate    acf_yyestate
#define yyexca      acf_yyexca
#define yyextra     acf_yyextra
#define yyfnd       acf_yyfnd
#define yyin        acf_yyin
#define yyinput     acf_yyinput
#define yyleng      acf_yyleng
#define yylex       acf_yylex
#define yylineno    acf_yylineno
#define yylook      acf_yylook
#define yylsp       acf_yylsp
#define yylstate    acf_yylstate
#define yylval      acf_yylval
#define yymatch     acf_yymatch
#define yymorfg     acf_yymorfg
#define yynerrs     acf_yynerrs
#define yyolsp      acf_yyolsp
#define yyout       acf_yyout
#define yyoutput    acf_yyoutput
#define yypact      acf_yypact
#define yyparse     acf_yyparse
#define yypgo       acf_yypgo
#define yyprevious  acf_yyprevious
#define yyps        acf_yyps
#define yypv        acf_yypv
#define yypvt       acf_yypvt
#define yyr1        acf_yyr1
#define yyr2        acf_yyr2
#define yyreds      acf_yyreds
#define yys         acf_yys
#define yysbuf      acf_yysbuf
#define yysptr      acf_yysptr
#define yystate     acf_yystate
#define yysvec      acf_yysvec
#define yytchar     acf_yytchar
#define yytext      acf_yytext
#define yytmp       acf_yytmp
#define yytoks      acf_yytoks
#define yytop       acf_yytop
#define yyunput     acf_yyunput
#define yyv         acf_yyv
#define yyval       acf_yyval
#define yyvstop     acf_yyvstop
#define yywrap      acf_yywrap
#define yymaxdepth  acf_yymaxdepth
#define yyposix_point	acf_yyposix_point
#define yynls16		acf_yynls16
#define yynls_wchar	acf_yynls_wchar
#define yylocale	acf_yylocale

/*
 * Added for AIX 4.1
 */

#define __once_yylex	acf___once_yylex

/*
 * Added for Flex
 */

#define yy_create_buffer	acf_yy_create_buffer
#define yy_delete_buffer	acf_yy_delete_buffer
#define yy_flush_buffer		acf_yy_flush_buffer
#define yy_init_buffer		acf_yy_init_buffer
#define yy_load_buffer_state	acf_yy_load_buffer_state
#define yy_scan_buffer		acf_yy_scan_buffer
#define yy_scan_bytes		acf_yy_scan_bytes
#define yy_scan_string		acf_yy_scan_string
#define yy_switch_to_buffer	acf_yy_switch_to_buffer
#define yyrestart		acf_yyrestart
