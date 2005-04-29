/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * string processing routines for Korn shell
 *
 */

#include	<ast.h>
#include	"defs.h"
#include	<stak.h>
#include	<ctype.h>
#include	<ccode.h>
#include	"shtable.h"
#include	"lexstates.h"
#include	"national.h"

#if !_lib_iswprint && !defined(iswprint)
#   define iswprint(c)		((c&~0377) || isprint(c))
#endif


/*
 *  Table lookup routine
 *  <table> is searched for string <sp> and corresponding value is returned
 *  This is only used for small tables and is used to save non-sharable memory 
 */

int sh_locate(register const char *sp,const Shtable_t *table,int size)
{
	register int first;
	register const Shtable_t	*tp;
	register int c;
	if(sp==0 || (first= *sp)==0)
		return(0);
	tp=table;
	while((c= *tp->sh_name) && (CC_NATIVE!=CC_ASCII || c <= first))
	{
		if(first == c && strcmp(sp,tp->sh_name)==0)
			return(tp->sh_number);
		tp = (Shtable_t*)((char*)tp+size);
	}
	return(0);
}

/*
 * look for the substring <oldsp> in <string> and replace with <newsp>
 * The new string is put on top of the stack
 */
char *sh_substitute(const char *string,const char *oldsp,char *newsp)
/*@
	assume string!=NULL && oldsp!=NULL && newsp!=NULL;
	return x satisfying x==NULL ||
		strlen(x)==(strlen(in string)+strlen(in newsp)-strlen(in oldsp));
@*/
{
	register const char *sp = string;
	register const char *cp;
	const char *savesp = 0;
	stakseek(0);
	if(*sp==0)
		return((char*)0);
	if(*(cp=oldsp) == 0)
		goto found;
#if SHOPT_MULTIBYTE
	mbinit();
#endif /* SHOPT_MULTIBYTE */
	do
	{
	/* skip to first character which matches start of oldsp */
		while(*sp && (savesp==sp || *sp != *cp))
		{
#if SHOPT_MULTIBYTE
			/* skip a whole character at a time */
			int c = mbsize(sp);
			if(c < 0)
				sp++;
			while(c-- > 0)
#endif /* SHOPT_MULTIBYTE */
			stakputc(*sp++);
		}
		if(*sp == 0)
			return((char*)0);
		savesp = sp;
	        for(;*cp;cp++)
		{
			if(*cp != *sp++)
				break;
		}
		if(*cp==0)
		/* match found */
			goto found;
		sp = savesp;
		cp = oldsp;
	}
	while(*sp);
	return((char*)0);

found:
	/* copy new */
	stakputs(newsp);
	/* copy rest of string */
	stakputs(sp);
	return(stakfreeze(1));
}

/*
 * TRIM(sp)
 * Remove escape characters from characters in <sp> and eliminate quoted nulls.
 */

void	sh_trim(register char *sp)
/*@
	assume sp!=NULL;
	promise  strlen(in sp) <= in strlen(sp);
@*/
{
	register char *dp;
	register int c;
	if(sp)
	{
		dp = sp;
		while(c= *sp++)
		{
			if(c == '\\')
				c = *sp++;
			if(c)
				*dp++ = c;
		}
		*dp = 0;
	}
}

/*
 * copy <str1> to <str2> changing upper case to lower case
 * <str2> must be big enough to hold <str1>
 * <str1> and <str2> may point to the same place.
 */

void sh_utol(register char const *str1,register char *str2)
/*@
	assume str1!=0 && str2!=0
	return x satisfying strlen(in str1)==strlen(in str2);
@*/ 
{
	register int c;
	for(; c= *((unsigned char*)str1); str1++,str2++)
	{
		if(isupper(c))
			*str2 = tolower(c);
		else
			*str2 = c;
	}
	*str2 = 0;
}

/*
 * print <str> qouting chars so that it can be read by the shell
 * puts null terminated result on stack, but doesn't freeze it
 */
char	*sh_fmtq(const char *string)
{
	register const char *cp = string;
	register int c, state;
	int offset;
	if(!cp)
		return((char*)0);
	offset = staktell();
#if SHOPT_MULTIBYTE
	state = ((c= mbchar(cp))==0);
#else
	state = ((c= *(unsigned char*)cp++)==0);
#endif
	if(isaletter(c))
	{
#if SHOPT_MULTIBYTE
		while((c=mbchar(cp)),isaname(c));
#else
		while((c = *(unsigned char*)cp++),isaname(c));
#endif
		if(c==0)
			return((char*)string);
		if(c=='=')
		{
			if(*cp==0)
				return((char*)string);
			c = cp - string;
			stakwrite(string,c);
			string = cp;
#if SHOPT_MULTIBYTE
			c = mbchar(cp);
#else
			c = *(unsigned char*)cp++;
#endif
		}
	}
	if(c==0 || c=='#' || c=='~')
		state = 1;
#if SHOPT_MULTIBYTE
	for(;c;c= mbchar(cp))
#else
	for(;c; c= *(unsigned char*)cp++)
#endif
	{
#if SHOPT_MULTIBYTE
		if(c>=0x200)
			continue;
		if(c=='\'' || !iswprint(c))
#else
		if(c=='\'' || !isprint(c))
#endif /* SHOPT_MULTIBYTE */
			state = 2;
		else if(c==']' || (c!=':' && (c=sh_lexstates[ST_NORM][c]) && c!=S_EPAT))
			state |=1;
	}
	if(state<2)
	{
		if(state==1)
			stakputc('\'');
		if(c = --cp - string)
			stakwrite(string,c);
		if(state==1)
			stakputc('\'');
	}
	else
	{
		stakwrite("$'",2);
		cp = string;
#if SHOPT_MULTIBYTE
		while(c= mbchar(cp))
#else
		while(c= *(unsigned char*)cp++)
#endif
		{
			state=1;
			switch(c)
			{
			    case ('a'==97?'\033':39):
				c = 'E';
				break;
			    case '\n':
				c = 'n';
				break;
			    case '\r':
				c = 'r';
				break;
			    case '\t':
				c = 't';
				break;
			    case '\f':
				c = 'f';
				break;
			    case '\b':
				c = 'b';
				break;
			    case '\a':
				c = 'a';
				break;
			    case '\\':	case '\'':
				break;
			    default:
#if SHOPT_MULTIBYTE
				if(!iswprint(c))
#else
				if(!isprint(c))
#endif
				{
					sfprintf(staksp,"\\%.3o",c);
					continue;
				}
				state=0;
				break;
			}
			if(state)
				stakputc('\\');
			stakputc(c);
		}
		stakputc('\'');
	}
	stakputc(0);
	return(stakptr(offset));
}

#if SHOPT_MULTIBYTE
	int sh_strchr(const char *string, register const char *dp)
	{
		wchar_t c, d;
		register const char *cp=string;
		mbinit();
		d = mbchar(dp); 
		mbinit();
		while(c = mbchar(cp))
		{
			if(c==d)
				return(cp-string);
		}
		if(d==0)
			return(cp-string);
		return(-1);
	}
#endif /* SHOPT_MULTIBYTE */

const char *_sh_translate(const char *message)
{
#if ERROR_VERSION >= 20000317L
	return(ERROR_translate(0,0,e_dict,message));
#else
#if ERROR_VERSION >= 20000101L
	return(ERROR_translate(e_dict,message));
#else
	return(ERROR_translate(message,1));
#endif
#endif
}

/*
 * change '['identifier']' to identifier
 * character before <str> must be a '['
 * returns pointer to last character
 */
char *sh_checkid(char *str, char *last)
{
	register unsigned char *cp = (unsigned char*)str;
	register unsigned char *v = cp;
	register int c;
	if(c= *cp++,isaletter(c))
		while(c= *cp++,isaname(c));
	if(c==']' && (!last || ((char*)cp==last)))
	{
		/* eliminate [ and ] */
		while(v < cp)
		{
			v[-1] = *v;
			v++;
		}
		if(last)
			last -=2;
		else
		{
			while(*v)
			{
				v[-2] = *v;
				v++;
			}
			v[-2] = 0;
			last = (char*)v;
		}
	}
	return(last);
}

#if	_AST_VERSION  <= 20000317L
char *fmtident(const char *string)
{
	return((char*)string);
}
#endif
