%START  MACRO_DEF BRACE_BAL
%a 14000
%p 12000
%e 2000
%n 1000
%o 20000



/* compiler/core/lex-asn1.l */
/* AUTHOR: Mike Sample */
/* DATE:   91/92 */

/* Copyright (C) 1991, 1992 Michael Sample */
/*               and the University of British Columbia */
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or */
/* (at your option) any later version. */

/* these comments must only be a single line each - lex blows it otherwise */
/* due to this claim, the rcs log is at the end of this file. */
/* $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/core/lex-asn1.l,v 1.1 2001/06/20 21:27:57 dmitch Exp $ */

/* This lex spec should compile under either lex or flex. */

/* There are three modes to the lexical analyzer, INITIAL, MACRO_DEF, */
/* and BRACE_BAL.  INITIAL is the normal mode. MACRO_DEF is used by   */
/* a lexical tie-in from the yacc code to eat a MACRO DEFINTION as a  */
/* single blk of text.  BRACE_BAL is used to by a lexical tie-in for  */
/* eating values inside { }'s.  */

/* if your ASN.1 source file has control characters that cause      */
/* snacc to choke, use a program like 'tr' to condition them first. */


WHITESPC  [ \t\n\r]


%{

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "exports.h"
#include "parse-asn1.h"  /* defines the returned token values */
#include "parser.h"
#include "lex-stuff.h"
#include "errno.h"

unsigned long int myLineNoG = 0;

%}

%%

<INITIAL>"[C]"       return BOXC_SYM;
<INITIAL>"[S]"       return BOXS_SYM;

<INITIAL>{WHITESPC}+ { COUNT_NEWLINES (myLineNoG, yytext);}
<INITIAL>"."      return DOT_SYM;
<INITIAL>","      return COMMA_SYM;
<INITIAL>"{"      return LEFTBRACE_SYM;
<INITIAL>"}"      return RIGHTBRACE_SYM;
<INITIAL>"("      return LEFTPAREN_SYM;
<INITIAL>")"      return RIGHTPAREN_SYM;
<INITIAL>"["      return LEFTBRACKET_SYM;
<INITIAL>"]"      return RIGHTBRACKET_SYM;
<INITIAL>"<"      return LESSTHAN_SYM;
<INITIAL>"-"      return MINUS_SYM;
<INITIAL>"::="    return GETS_SYM;
<INITIAL>"|"      return BAR_SYM;
<INITIAL>";"      return SEMI_COLON_SYM;
<INITIAL>TAGS     return TAGS_SYM;
<INITIAL>BOOLEAN  return BOOLEAN_SYM;
<INITIAL>INTEGER  return INTEGER_SYM;
<INITIAL>BIT      return BIT_SYM;
<INITIAL>STRING   return STRING_SYM;
<INITIAL>OCTET    return OCTET_SYM;
<INITIAL>NULL     return NULL_SYM;
<INITIAL>SEQUENCE return SEQUENCE_SYM;
<INITIAL>OF       return OF_SYM;
<INITIAL>SET      return SET_SYM;
<INITIAL>IMPLICIT return IMPLICIT_SYM;
<INITIAL>CHOICE   return CHOICE_SYM;
<INITIAL>ANY      return ANY_SYM;
<INITIAL>OBJECT{WHITESPC}*IDENTIFIER {
      COUNT_NEWLINES (myLineNoG, yytext);
      return OBJECT_IDENTIFIER_SYM;}
<INITIAL>OPTIONAL return OPTIONAL_SYM;
<INITIAL>DEFAULT  return DEFAULT_SYM;
<INITIAL>COMPONENTS return COMPONENTS_SYM;
<INITIAL>UNIVERSAL return UNIVERSAL_SYM;
<INITIAL>APPLICATION return APPLICATION_SYM;
<INITIAL>PRIVATE  return PRIVATE_SYM;
<INITIAL>TRUE     return TRUE_SYM;
<INITIAL>FALSE    return FALSE_SYM;
<INITIAL>BEGIN    return BEGIN_SYM;
<INITIAL>END      return END_SYM;
<INITIAL>DEFINITIONS return DEFINITIONS_SYM;
<INITIAL>EXPLICIT return EXPLICIT_SYM;
<INITIAL>ENUMERATED return ENUMERATED_SYM;
<INITIAL>EXPORTS  return EXPORTS_SYM;
<INITIAL>IMPORTS  return IMPORTS_SYM;
<INITIAL>REAL     return REAL_SYM;
<INITIAL>INCLUDES return INCLUDES_SYM;
<INITIAL>MIN      return MIN_SYM;
<INITIAL>MAX      return MAX_SYM;
<INITIAL>SIZE     return SIZE_SYM;
<INITIAL>FROM     return FROM_SYM;
<INITIAL>WITH     return WITH_SYM;
<INITIAL>COMPONENT return COMPONENT_SYM;
<INITIAL>PRESENT  return PRESENT_SYM;
<INITIAL>ABSENT   return ABSENT_SYM;
<INITIAL>DEFINED  return DEFINED_SYM;
<INITIAL>BY       return BY_SYM;
<INITIAL>PLUS-INFINITY return PLUS_INFINITY_SYM;
<INITIAL>MINUS-INFINITY return MINUS_INFINITY_SYM;



<MACRO_DEF>(.|\n) {
    int i;
    char *buf;
    int bufSize;
    int inComment;
    int inStr;
    unsigned int c, c1, c2;

    /*
     * matches any first char, then
     * copies everything until an uncommented,
     * unquoted END.  This Lex state is started
     * from the yacc src (lexical tie in)
     * from the MACRO_DEF production.
     *
     * if you don't like realloc and don't care about
     * macro defs just have this return a constant string
     * like "BEGIN <not parsed> END" after eating the definition
     */

    unput (yytext[0]);

    bufSize = 1024;
    buf = Malloc (1024);

    i = 0;          /* put BEGIN str at beginning */
    buf[i++] = 'B';
    buf[i++] = 'E';
    buf[i++] = 'G';
    buf[i++] = 'I';
    buf[i++] = 'N';
    buf[i++] = '\n';

    inStr = FALSE;
    inComment = FALSE;
    for ( ; ; i++)
    {
        c = input();

        if (i >= (bufSize - 4))
        {
            bufSize += 512;
            buf = (char*) Realloc (buf, bufSize);
        }

        buf[i] = c;

        if ((inComment) && (c == '\n'))
              inComment = FALSE;
        else if (!(inStr) &&  (c == '-'))
        {
            c = input();
            if (c  == '-')
            {
                buf[++i] = c;
                inComment = !inComment;
            }
            else
                unput (c);
        }
        else  if (inComment)
            continue;
        else if (c == '"')
              inStr = !inStr;
        else if (inStr)
            continue;
        else if (c == 'E')
        {
            c1 = input();
            c2 = input();
            if ((c1 == 'N') && (c2 == 'D'))
            {
                buf[++i] = 'N';
                buf[++i] = 'D';
                buf[++i] = '\0';
                yylval.charPtr = buf;
                COUNT_NEWLINES (myLineNoG, buf);
                myLineNoG -=1; /* take off 1 added after "BEGIN" */
                return MACRODEFBODY_SYM;
            }
            else
            {
                unput (c2);
                unput (c1);
            }
        }
    }
   /* not reached */
}



<INITIAL>[A-Z](-[A-Z0-9]|[A-Z0-9])*{WHITESPC}*MACRO {
         int i;
        /* copy and return the Macro's name only */
        /* doesn't handle comments between macro name and MACRO sym */
         for (i = 0; (yytext[i] != ' ') &&
                     (yytext[i] != '\t') &&
                     (yytext[i] != '\n') &&
                     (yytext[i] != '\r'); i++);
         yylval.charPtr = Malloc (i+1);
         strncpy (yylval.charPtr, yytext, i);
         yylval.charPtr[i] = '\0';
         return NAMEDMACRO_SYM; }


<INITIAL>OPERATION        return OPERATION_SYM;
<INITIAL>ARGUMENT         return ARGUMENT_SYM;
<INITIAL>RESULT           return RESULT_SYM;
<INITIAL>ERRORS           return ERRORS_SYM;
<INITIAL>LINKED           return LINKED_SYM;

<INITIAL>ERROR            return ERROR_SYM;
<INITIAL>PARAMETER        return PARAMETER_SYM;

<INITIAL>BIND             return BIND_SYM;
<INITIAL>BIND-ERROR       return BINDERROR_SYM;
<INITIAL>UNBIND           return UNBIND_SYM;
<INITIAL>UNBIND-ERROR     return UNBINDERROR_SYM;

<INITIAL>APPLICATION-CONTEXT          return AC_SYM;
<INITIAL>APPLICATION-SERVICE-ELEMENTS return ASES_SYM;
<INITIAL>REMOTE                       return REMOTE_SYM;
<INITIAL>INITIATOR                    return INITIATOR_SYM;
<INITIAL>RESPONDER                    return RESPONDER_SYM;
<INITIAL>ABSTRACT{WHITESPC}*SYNTAXES {
        COUNT_NEWLINES (myLineNoG, yytext);
        return ABSTRACTSYNTAXES_SYM;}

<INITIAL>APPLICATION-SERVICE-ELEMENT return ASE_SYM;
<INITIAL>OPERATIONS                  return OPERATIONS_SYM;
<INITIAL>CONSUMER{WHITESPC}*INVOKES {
        COUNT_NEWLINES (myLineNoG, yytext);
        return CONSUMERINVOKES_SYM;}

<INITIAL>SUPPLIER{WHITESPC}*INVOKES  {
        COUNT_NEWLINES (myLineNoG, yytext);
        return SUPPLIERINVOKES_SYM;}

<INITIAL>EXTENSION-ATTRIBUTE          return EXTENSIONATTRIBUTE_SYM;
<INITIAL>EXTENSIONS                   return EXTENSIONS_SYM;
<INITIAL>CHOSEN                       return CHOSEN_SYM;

<INITIAL>EXTENSION                   return EXTENSION_SYM;
<INITIAL>CRITICAL                    return CRITICAL_SYM;
<INITIAL>FOR                         return FOR_SYM;
<INITIAL>SUBMISSION                  return SUBMISSION_SYM;
<INITIAL>DELIVERY                    return DELIVERY_SYM;
<INITIAL>TRANSFER                    return TRANSFER_SYM;

<INITIAL>OBJECT                  return OBJECT_SYM;
<INITIAL>PORTS                   return PORTS_SYM;

<INITIAL>PORT                      return PORT_SYM;
<INITIAL>ABSTRACT{WHITESPC}*OPERATIONS  {
       COUNT_NEWLINES (myLineNoG, yytext);
       return ABSTRACTOPS_SYM;}


<INITIAL>REFINE                    return REFINE_SYM;
<INITIAL>AS                        return AS_SYM;
<INITIAL>RECURRING                 return RECURRING_SYM;
<INITIAL>VISIBLE                   return VISIBLE_SYM;
<INITIAL>PAIRED                    return PAIRED_SYM;

<INITIAL>ABSTRACT-BIND             return ABSTRACTBIND_SYM;
<INITIAL>TO                        return TO_SYM;

<INITIAL>ABSTRACT-UNBIND           return ABSTRACTUNBIND_SYM;

<INITIAL>ABSTRACT-ERROR            return ABSTRACTERROR_SYM;

<INITIAL>ABSTRACT-OPERATION        return ABSTRACTOPERATION_SYM;

<INITIAL>TOKEN                     return TOKEN_SYM;

<INITIAL>TOKEN-DATA                return TOKENDATA_SYM;

<INITIAL>SECURITY-CATEGORY         return SECURITYCATEGORY_SYM;

<INITIAL>ALGORITHM         return ALGORITHM_SYM;
<INITIAL>ENCRYPTED         return ENCRYPTED_SYM;
<INITIAL>SIGNED            return SIGNED_SYM;
<INITIAL>SIGNATURE         return SIGNATURE_SYM;
<INITIAL>PROTECTED         return PROTECTED_SYM;

<INITIAL>OBJECT-TYPE       return OBJECTTYPE_SYM;
<INITIAL>SYNTAX            return SYNTAX_SYM;
<INITIAL>ACCESS            return ACCESS_SYM;
<INITIAL>STATUS            return STATUS_SYM;
<INITIAL>DESCRIPTION       return DESCRIPTION_SYM;
<INITIAL>REFERENCE         return REFERENCE_SYM;
<INITIAL>INDEX             return INDEX_SYM;
<INITIAL>DEFVAL            return DEFVAL_SYM;



<BRACE_BAL>(.|\n) {
    int i;
    char *buf;
    int bufSize;
    int inComment;
    int inStr;
    int braceDepth;
    char c, c1, c2;

    /*
     * matches any first char, then
     * copies everything until an ending "}"
     * Assumes that initially parsed a "{"
     * and puts one at beg. of returned string
     */
    unput (yytext[0]);

    bufSize = 256;
    buf = Malloc (256);

    i = 0;          /* put openning brace at beginning */
    buf[i++] = '{';
    buf[i++] = ' ';

    inStr = FALSE;
    inComment = FALSE;
    braceDepth = 1;
    for ( ; ; i++)
    {
        c = input();

        if (i >= (bufSize - 2))
        {
            bufSize += 256;
            buf = (char*) Realloc (buf, bufSize);
        }

        buf[i] = c;

        if ((inComment) && (c == '\n'))
              inComment = FALSE;
        else if (!(inStr) &&  (c == '-'))
        {
            c = input();
            if (c  == '-')
            {
                buf[++i] = c;
                inComment = !inComment;
            }
            else
                unput (c);
        }
        else  if (inComment)
            continue;
        else if (c == '"')
              inStr = !inStr;
        else if (inStr)
            continue;
        else if (c == '{')
            braceDepth++;
        else if (c == '}')
        {
            braceDepth--;
            if (braceDepth == 0)
            {
                buf[++i] = '\0';
                yylval.charPtr = buf;
                COUNT_NEWLINES (myLineNoG, buf);
                return BRACEBAL_SYM;
            }
        }
    }
   /* not reached */
}



<INITIAL>\'[0-1]*\'B {
          COUNT_NEWLINES (myLineNoG, yytext);
          yylval.charPtr = (char*)Malloc (yyleng);
          strncpy (yylval.charPtr, yytext+1, yyleng -1);  /* strip "'"s */
          yylval.charPtr[yyleng-2] = '\0';
          return BSTRING_SYM;}

<INITIAL>\'[0-9A-Fa-f]*\'H {
          COUNT_NEWLINES (myLineNoG, yytext);
          yylval.charPtr = (char*)Malloc (yyleng);
          strncpy (yylval.charPtr, yytext+1, yyleng -1);   /* strip "'"s */
          yylval.charPtr[yyleng-2] = '\0';
          return HSTRING_SYM;}

<INITIAL>\"([^\"]|"\"\"")*\"  {
          COUNT_NEWLINES (myLineNoG, yytext);
          yylval.charPtr = (char*)Malloc (yyleng);
          strncpy (yylval.charPtr, yytext+1, yyleng -1);       /* strip '"'s */
          yylval.charPtr[yyleng-2] = '\0'; /* 2 quotes == quote in a quote */
          return CSTRING_SYM;}

<INITIAL>[A-Z](-[a-zA-Z0-9]|[a-zA-Z0-9])* {
          yylval.charPtr = (char*)Malloc (yyleng+1);
          strcpy (yylval.charPtr, yytext);
          yylval.charPtr[yyleng] = '\0';
          return UCASEFIRST_IDENT_SYM;}


<INITIAL>[a-z](-[a-zA-Z0-9]|[a-zA-Z0-9])* {
          yylval.charPtr = (char*)Malloc (yyleng+1);
          strcpy (yylval.charPtr, yytext);
          yylval.charPtr[yyleng] = '\0';
          return LCASEFIRST_IDENT_SYM;}

<INITIAL>[1-9][0-9]* { /*first digit cannot be zero on multi-digit #'s*/
	  errno = 0;
	  {
	      unsigned long ul = (unsigned long) strtol(yytext,NULL,10);
	      if (!errno && ul>(unsigned long)0xFFFFFFFF) {
		  errno = ERANGE;
	      }
	      if (!errno) {
		  yylval.uintVal = (unsigned int) ul;
		  return NUMBER_SYM;
	      }
	  }
          yylval.charPtr = (char*)Malloc (yyleng+1);
          strcpy (yylval.charPtr, yytext);
          yylval.charPtr[yyleng] = '\0';
          return NUMBER_ERANGE;}

<INITIAL>0 {  /*allow zero as first digit on single digit #'s*/
          yylval.uintVal = 0;
          return NUMBER_SYM;}


<INITIAL>"--snacc"(-[^-\n]|[^\-\n])*("--"|\n) {
         /* this must be before the normal comment eater so that snacc attribs
          * are not treated as normal comments
          */
        /* eat comments, update line no */
        int len;
        COUNT_NEWLINES (myLineNoG, yytext);
        yylval.charPtr = (char*)Malloc (yyleng-4);
        /* skip first "--snacc" in copy to ret val */
        strcpy (yylval.charPtr, yytext + 7);
        len = strlen (yylval.charPtr);
        /* strip off newline or -- terminator for comment */
        if (yylval.charPtr[len-1] == '\n')
           yylval.charPtr[len-1] = '\0';
        else
           yylval.charPtr[len-2] = '\0';
        return SNACC_ATTRIBUTES; }


<INITIAL>"--"(-[^\-\n]|[^\-\n])*("--"|\n|"-\n") {
        /* eat comments, update line no */
        COUNT_NEWLINES (myLineNoG, yytext);}


%%


/*
 * these "LexBegin..." routines are used by yacc for (ack!)
 * lexical tie ins
 */

int
LexBeginMacroDefContext()
{
    BEGIN (MACRO_DEF);
}

int
LexBeginBraceBalContext()
{
    BEGIN (BRACE_BAL);
}

int
LexBeginInitialContext()
{
    BEGIN (INITIAL);
}

/*
 * $Log: lex-asn1.l,v $
 * Revision 1.1  2001/06/20 21:27:57  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:49  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.5  1997/08/28 09:46:41  wan
 * Reworked number range checking, only gives warning now.
 *
 * Revision 1.4  1997/06/19 09:17:17  wan
 * Added isPdu flag to tables. Added value range checks during parsing.
 *
 * Revision 1.3  1995/07/25 19:41:30  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:37:12  rj
 * snacc_config.h removed.
 *
 * for a list of changes relative to the 1.1 distribution, please refer to the ChangeLog.
 */
