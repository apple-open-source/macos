%k 10000
%n 5000
%a 20000
%e 10000
%p 25000

Ident		([A-Za-z_][A-Za-z_0-9]*)
Number		([0-9]+)
String		([-/._$A-Za-z0-9]+)
QString		(\"[^"\n]*\")
AString		(\<[^>\n]*\>)
FileName	({QString}|{AString})

%{
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <mach/message.h>
#include <string.h>
#include <type.h>
#include <statement.h>
#include <global.h>
#include <parser.h>
#include <lexxer.h>
#include <cross64.h>

#ifdef	LDEBUG
#define RETURN(sym)							\
{									\
    printf("yylex: returning '%s' (%d)\n", #sym, (sym));		\
    return (sym);							\
}
#else	LDEBUG
#define RETURN(sym)	return (sym)
#endif	LDEBUG

#define	TPRETURN(intype, outtype, tsize)				\
{									\
    yylval.symtype.innumber = (intype);					\
    yylval.symtype.instr = #intype;					\
    yylval.symtype.outnumber = (outtype);				\
    yylval.symtype.outstr = #outtype;					\
    yylval.symtype.size = (tsize);					\
    RETURN(sySymbolicType);						\
}

#define	TRETURN(type, tsize)	TPRETURN(type,type,tsize)

#define	FRETURN(val)							\
{									\
    yylval.flag = (val);						\
    RETURN(syIPCFlag);							\
}

static int oldYYBegin;
#define yybgin yy_start

static void doSharp(const char *body); /* process body of # directives */
%}

%Start	Normal String FileName QString SkipToEOL

%%

<Normal>[Rr][Oo][Uu][Tt][Ii][Nn][Ee]		RETURN(syRoutine);
<Normal>[Ff][Uu][Nn][Cc][Tt][Ii][Oo][Nn]	RETURN(syFunction);
<Normal>[Pp][Rr][Oo][Cc][Ee][Dd][Uu][Rr][Ee]	RETURN(syProcedure);
<Normal>[Ss][Ii][Mm][Pp][Ll][Ee][Pp][Rr][Oo][Cc][Ee][Dd][Uu][Rr][Ee] RETURN(sySimpleProcedure);
<Normal>[Ss][Ii][Mm][Pp][Ll][Ee][Rr][Oo][Uu][Tt][Ii][Nn][Ee] RETURN(sySimpleRoutine);
<Normal>[Ss][Uu][Bb][Ss][Yy][Ss][Tt][Ee][Mm]	RETURN(sySubsystem);
<Normal>[Mm][Ss][Gg][Oo][Pp][Tt][Ii][Oo][Nn]	RETURN(syMsgOption);
<Normal>[Mm][Ss][Gg][Ss][Ee][Qq][Nn][Oo]	RETURN(syMsgSeqno);
<Normal>[Ww][Aa][Ii][Tt][Tt][Ii][Mm][Ee]	RETURN(syWaitTime);
<Normal>[Nn][Oo][Ww][Aa][Ii][Tt][Tt][Ii][Mm][Ee]	RETURN(syNoWaitTime);
<Normal>[Ii][Nn]				RETURN(syIn);
<Normal>[Oo][Uu][Tt]				RETURN(syOut);
<Normal>[Ii][Nn][Oo][Uu][Tt]			RETURN(syInOut);
<Normal>[Rr][Ee][Qq][Uu][Ee][Ss][Tt][Pp][Oo][Rr][Tt]	RETURN(syRequestPort);
<Normal>[Rr][Ee][Pp][Ll][Yy][Pp][Oo][Rr][Tt]		RETURN(syReplyPort);
<Normal>[Uu][Rr][Ee][Pp][Ll][Yy][Pp][Oo][Rr][Tt]	RETURN(syUReplyPort);
<Normal>[Ss][Rr][Ee][Pp][Ll][Yy][Pp][Oo][Rr][Tt]	RETURN(sySReplyPort);
<Normal>[Aa][Rr][Rr][Aa][Yy]			RETURN(syArray);
<Normal>[Oo][Ff]				RETURN(syOf);
<Normal>[Ee][Rr][Rr][Oo][Rr]			RETURN(syErrorProc);
<Normal>[Ss][Ee][Rr][Vv][Ee][Rr][Pp][Rr][Ee][Ff][Ii][Xx] RETURN(syServerPrefix);
<Normal>[Uu][Ss][Ee][Rr][Pp][Rr][Ee][Ff][Ii][Xx]	RETURN(syUserPrefix);
<Normal>[Ss][Ee][Rr][Vv][Ee][Rr][Dd][Ee][Mm][Uu][Xx]	RETURN(syServerDemux);
<Normal>[Rr][Cc][Ss][Ii][Dd]			RETURN(syRCSId);
<Normal>[Ii][Mm][Pp][Oo][Rr][Tt]		RETURN(syImport);
<Normal>[Uu][Ii][Mm][Pp][Oo][Rr][Tt]		RETURN(syUImport);
<Normal>[Ss][Ii][Mm][Pp][Oo][Rr][Tt]		RETURN(sySImport);
<Normal>[Tt][Yy][Pp][Ee]			RETURN(syType);
<Normal>[Kk][Ee][Rr][Nn][Ee][Ll][Ss][Ee][Rr][Vv][Ee][Rr] RETURN(syKernelServer);
<Normal>[Kk][Ee][Rr][Nn][Ee][Ll][Uu][Ss][Ee][Rr]	RETURN(syKernelUser);
<Normal>[Ss][Kk][Ii][Pp]			RETURN(sySkip);
<Normal>[Ss][Tt][Rr][Uu][Cc][Tt]		RETURN(syStruct);
<Normal>[Ii][Nn][Tt][Rr][Aa][Nn]		RETURN(syInTran);
<Normal>[Oo][Uu][Tt][Tt][Rr][Aa][Nn]		RETURN(syOutTran);
<Normal>[Dd][Ee][Ss][Tt][Rr][Uu][Cc][Tt][Oo][Rr]	RETURN(syDestructor);
<Normal>[Cc][Tt][Yy][Pp][Ee]				RETURN(syCType);
<Normal>[Cc][Uu][Ss][Ee][Rr][Tt][Yy][Pp][Ee]		RETURN(syCUserType);
<Normal>[Cc][Ss][Ee][Rr][Vv][Ee][Rr][Tt][Yy][Pp][Ee]	RETURN(syCServerType);
<Normal>[Cc]_[Ss][Tt][Rr][Ii][Nn][Gg]			RETURN(syCString);

<Normal>[Ii][Ss][Ll][Oo][Nn][Gg]		FRETURN(flLong);
<Normal>[Ii][Ss][Nn][Oo][Tt][Ll][Oo][Nn][Gg]	FRETURN(flNotLong);
<Normal>[Dd][Ee][Aa][Ll][Ll][Oo][Cc]		FRETURN(flDealloc);
<Normal>[Nn][Oo][Tt][Dd][Ee][Aa][Ll][Ll][Oo][Cc] FRETURN(flNotDealloc);
<Normal>[Ss][Ee][Rr][Vv][Ee][Rr][Cc][Oo][Pp][Yy] FRETURN(flServerCopy);
<Normal>[Cc][Oo][Uu][Nn][Tt][Ii][Nn][Oo][Uu][Tt] FRETURN(flCountInOut);

<Normal>[Pp][Oo][Ll][Yy][Mm][Oo][Rr][Pp][Hh][Ii][Cc]	TRETURN(MACH_MSG_TYPE_POLYMORPHIC,word_size_in_bits);

<Normal>"MACH_MSG_TYPE_UNSTRUCTURED"	TRETURN(MACH_MSG_TYPE_UNSTRUCTURED,0);
<Normal>"MACH_MSG_TYPE_BIT"		TRETURN(MACH_MSG_TYPE_BIT,1);
<Normal>"MACH_MSG_TYPE_BOOLEAN"		TRETURN(MACH_MSG_TYPE_BOOLEAN,32);
<Normal>"MACH_MSG_TYPE_INTEGER_16"	TRETURN(MACH_MSG_TYPE_INTEGER_16,16);
<Normal>"MACH_MSG_TYPE_INTEGER_32"	TRETURN(MACH_MSG_TYPE_INTEGER_32,32);
<Normal>"MACH_MSG_TYPE_INTEGER_64"	TRETURN(MACH_MSG_TYPE_INTEGER_64,64);
<Normal>"MACH_MSG_TYPE_CHAR"		TRETURN(MACH_MSG_TYPE_CHAR,8);
<Normal>"MACH_MSG_TYPE_BYTE"		TRETURN(MACH_MSG_TYPE_BYTE,8);
<Normal>"MACH_MSG_TYPE_INTEGER_8"	TRETURN(MACH_MSG_TYPE_INTEGER_8,8);
<Normal>"MACH_MSG_TYPE_REAL"		TRETURN(MACH_MSG_TYPE_REAL,0);
<Normal>"MACH_MSG_TYPE_STRING"		TRETURN(MACH_MSG_TYPE_STRING,0);
<Normal>"MACH_MSG_TYPE_STRING_C"	TRETURN(MACH_MSG_TYPE_STRING_C,0);

<Normal>"MACH_MSG_TYPE_MOVE_RECEIVE"	TPRETURN(MACH_MSG_TYPE_MOVE_RECEIVE,MACH_MSG_TYPE_PORT_RECEIVE,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_COPY_SEND"	TPRETURN(MACH_MSG_TYPE_COPY_SEND,MACH_MSG_TYPE_PORT_SEND,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_MAKE_SEND"	TPRETURN(MACH_MSG_TYPE_MAKE_SEND,MACH_MSG_TYPE_PORT_SEND,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_MOVE_SEND"	TPRETURN(MACH_MSG_TYPE_MOVE_SEND,MACH_MSG_TYPE_PORT_SEND,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_MAKE_SEND_ONCE"	TPRETURN(MACH_MSG_TYPE_MAKE_SEND_ONCE,MACH_MSG_TYPE_PORT_SEND_ONCE,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_MOVE_SEND_ONCE"	TPRETURN(MACH_MSG_TYPE_MOVE_SEND_ONCE,MACH_MSG_TYPE_PORT_SEND_ONCE,word_size_in_bits);

<Normal>"MACH_MSG_TYPE_PORT_NAME"	TRETURN(MACH_MSG_TYPE_PORT_NAME,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_PORT_RECEIVE"	TPRETURN(MACH_MSG_TYPE_POLYMORPHIC,MACH_MSG_TYPE_PORT_RECEIVE,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_PORT_SEND"	TPRETURN(MACH_MSG_TYPE_POLYMORPHIC,MACH_MSG_TYPE_PORT_SEND,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_PORT_SEND_ONCE"	TPRETURN(MACH_MSG_TYPE_POLYMORPHIC,MACH_MSG_TYPE_PORT_SEND_ONCE,word_size_in_bits);
<Normal>"MACH_MSG_TYPE_POLYMORPHIC"	TRETURN(MACH_MSG_TYPE_POLYMORPHIC,0);

<Normal>":"		RETURN(syColon);
<Normal>";"		RETURN(sySemi);
<Normal>","		RETURN(syComma);
<Normal>"+"		RETURN(syPlus);
<Normal>"-"		RETURN(syMinus);
<Normal>"*"		RETURN(syStar);
<Normal>"/"		RETURN(syDiv);
<Normal>"("		RETURN(syLParen);
<Normal>")"		RETURN(syRParen);
<Normal>"="		RETURN(syEqual);
<Normal>"^"		RETURN(syCaret);
<Normal>"~"		RETURN(syTilde);
<Normal>"<"		RETURN(syLAngle);
<Normal>">"		RETURN(syRAngle);
<Normal>"["		RETURN(syLBrack);
<Normal>"]"		RETURN(syRBrack);
<Normal>"|"		RETURN(syBar);

<Normal>{Ident}		{ yylval.identifier = strmake(yytext);
			  RETURN(syIdentifier); }
<Normal>{Number}	{ yylval.number = atoi(yytext); RETURN(syNumber); }

<String>{String}	{ yylval.string = strmake(yytext);
			  BEGIN Normal; RETURN(syString); }
<FileName>{FileName}	{ yylval.string = strmake(yytext);
			  BEGIN Normal; RETURN(syFileName); }
<QString>{QString}	{ yylval.string = strmake(yytext);
			  BEGIN Normal; RETURN(syQString); }

^\#[ \t]*{Number}[ \t]*\"[^"]*\"	{ doSharp(yytext+1);
					  oldYYBegin = yybgin;
					  BEGIN SkipToEOL; }
^\#\ *{Number}				{ doSharp(yytext+1);
					  oldYYBegin = yybgin;
					  BEGIN SkipToEOL; }
^\#					{ yyerror("illegal # directive");
					  oldYYBegin = yybgin;
					  BEGIN SkipToEOL; }

<SkipToEOL>\n		yybgin = oldYYBegin;
<SkipToEOL>.		;

[ \t\n]			;
.			{ BEGIN Normal; RETURN(syError); }

%%

int yylineno;

extern void
LookNormal(void)
{
    BEGIN Normal;
}

extern void
LookString(void)
{
    BEGIN String;
}

extern void
LookQString(void)
{
    BEGIN QString;
}

extern void
LookFileName(void)
{
    BEGIN FileName;
}

static void
doSharp(const char *body)
{
    register const char *startName;

#ifndef	__386BSD__
    yylineno = atoi(body);
    startName = index(body, '"');
    if (startName != NULL)
    {
	char	*c;

	c = rindex(body, '"');
	*c = '\0';
	strfree(yyinname);
	yyinname = strmake(startName+1);
    }
#endif
}

int
yywrap()
{
	return 1;
}

