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
 * Shell arithmetic - uses streval library
 *   David Korn
 *   AT&T Labs
 */

#include	"defs.h"
#include	<ctype.h>
#include	"lexstates.h"
#include	"name.h"
#include	"streval.h"
#include	"variables.h"
#include	"FEATURE/locale"

static Namval_t *scope(register Namval_t *np,register struct lval *lvalue,int assign)
{
	register Namarr_t *ap;
	register int flag = lvalue->flag;
	register char *sub=0;
	if(lvalue->emode&ARITH_COMP)
	{
		char *cp = (char*)np;
		register Namval_t *mp;
		if(cp>=lvalue->expr &&  cp < lvalue->expr+lvalue->elen)
		{
			/* do bindiing to node now */
			int c = cp[flag];
			cp[flag] = 0;
			np = nv_open(cp,sh.var_tree,NV_NOASSIGN|NV_VARNAME);
			cp[flag] = c;
			if(cp[flag+1]=='[')
				flag++;
			else
				flag = 0;
		}
		else if(dtvnext(sh.var_tree) && (mp=nv_search((char*)np,sh.var_tree,HASH_NOSCOPE|HASH_SCOPE|HASH_BUCKET)))
		{
			while(nv_isref(mp))
			{
				sub = mp->nvenv;
				mp = nv_refnode(mp);
			}
			np = mp;
		}
	}
	if(flag || sub)
	{
		if(!sub)
			sub = (char*)&lvalue->expr[flag];
		if(((ap=nv_arrayptr(np)) && array_assoc(ap)) || (lvalue->emode&ARITH_COMP))
			nv_endsubscript(np,sub,NV_ADD|NV_SUBQUOTE);
		else
			nv_putsub(np, NIL(char*),flag);
	}
	return(np);
}

static Sfdouble_t arith(const char **ptr, struct lval *lvalue, int type, Sfdouble_t n)
{
	register Sfdouble_t r= 0;
	char *str = (char*)*ptr;
	switch(type)
	{
	    case ASSIGN:
	    {
		register Namval_t *np = (Namval_t*)(lvalue->value);
		np = scope(np,lvalue,1);
		nv_putval(np, (char*)&n, NV_INTEGER|NV_DOUBLE|NV_LONG);
		r=nv_getnum(np);
		break;
	    }
	    case LOOKUP:
	    {
		register int c = *str;
		lvalue->value = (char*)0;
		if(c=='.')
			c = str[1];
		if(isaletter(c))
		{
			register Namval_t *np;
			int dot=0;
			char *cp;
			while(1)
			{
				while(c= *++str, isaname(c));
				if(c!='.')
					break;
				dot=1;
				if((c = *++str) !='[')
					continue;
				str = nv_endsubscript((Namval_t*)0,cp=str,NV_SUBQUOTE)-1;
				if(sh_checkid(cp+1,(char*)0))
					str -=2;
			}
			if(c=='(')
			{
				int fsize = str- (char*)(*ptr);
				const struct mathtab *tp;
				c = **ptr;
				lvalue->fun = 0;
				if(fsize<=(sizeof(tp->fname)-2)) for(tp=shtab_math; *tp->fname; tp++)
				{
					if(*tp->fname > c)
						break;
					if(tp->fname[1]==c && tp->fname[fsize+1]==0 && strncmp(&tp->fname[1],*ptr,fsize)==0)
					{
						lvalue->fun = tp->fnptr;
						lvalue->nargs = *tp->fname;
						break;
					}
				}
				if(lvalue->fun)
					break;
				lvalue->value = (char*)ERROR_dictionary(e_function);
				return(r);
			}
			if((lvalue->emode&ARITH_COMP) && dot)
			{
				lvalue->value = (char*)*ptr;
				lvalue->flag =  str-lvalue->value;
				break;
			}
			*str = 0;
			if(sh_isoption(SH_NOEXEC))
				np = L_ARGNOD;
			else
			{
				Dt_t  *root = (lvalue->emode&ARITH_COMP)?sh.var_base:sh.var_tree;
				np = nv_open(*ptr,root,NV_NOASSIGN|NV_VARNAME);
			}
			*str = c;
			lvalue->value = (char*)np;
			if((lvalue->emode&ARITH_COMP) || (nv_isarray(np) && nv_aindex(np)<0))
			{
				/* bind subscript later */
				lvalue->flag = 0;
				if(c=='[')
				{
					lvalue->flag = (str-lvalue->expr);
					str = nv_endsubscript(np,str,0);
				}
				break;
			}
			if(c=='[')
				str = nv_endsubscript(np,str,NV_ADD|NV_SUBQUOTE);
			else if(nv_isarray(np))
				nv_putsub(np,NIL(char*),ARRAY_UNDEF);
			if(nv_isattr(np,NV_INTEGER|NV_DOUBLE)==(NV_INTEGER|NV_DOUBLE))
				lvalue->isfloat=1;
			lvalue->flag = nv_aindex(np);
		}
		else
		{
			char	lastbase=0, *val = str, oerrno = errno;
			errno = 0;
			r = strtonll(val,&str, &lastbase,-1);
			if(*str=='8' || *str=='9')
			{
				lastbase=10;
				errno = 0;
				r = strtonll(val,&str, &lastbase,-1);
			}
			if(lastbase<=1)
				lastbase=10;
			if(*val=='0')
			{
				while(*val=='0')
					val++;
				if(*val==0 || *val=='.')
					val--;
			}
			if(r==LONG_MAX && errno)
				c='e';
			else
				c = *str;
			if(c==GETDECIMAL(0) || c=='e' || c == 'E')
			{
				lvalue->isfloat=1;
				r = strtod(val,&str);
			}
			else if(lastbase==10 && val[1])
			{
				if(val[2]=='#')
					val += 3;
				if((str-val)>2*sizeof(Sflong_t))
				{
					Sfdouble_t rr;
					rr = strtold(val,&str);
					if(rr!=r)
					{
						r = rr;
						lvalue->isfloat=1;
					}
				}
			}
			errno = oerrno;
		}
		break;
	    }
	    case VALUE:
	    {
		register Namval_t *np = (Namval_t*)(lvalue->value);
		if(sh_isoption(SH_NOEXEC))
			return(0);
		np = scope(np,lvalue,0);
		if(((lvalue->emode&2) || lvalue->level>1 || sh_isoption(SH_NOUNSET)) && nv_isnull(np) && !nv_isattr(np,NV_INTEGER))
		{
			*ptr = nv_name(np);
			lvalue->value = (char*)ERROR_dictionary(e_notset);
			lvalue->emode |= 010;
			return(0);
		}
		r = nv_getnum(np);
		if(nv_isattr(np,NV_INTEGER|NV_BINARY)==(NV_INTEGER|NV_BINARY))
			lvalue->isfloat= (r!=(Sflong_t)r);
		else if(nv_isattr(np,NV_INTEGER|NV_DOUBLE)==(NV_INTEGER|NV_DOUBLE))
			lvalue->isfloat=1;
		return(r);
	    }

	    case ERRMSG:
		sfsync(NIL(Sfio_t*));
#if 0
		if(warn)
			errormsg(SH_DICT,ERROR_warn(0),lvalue->value,*ptr);
		else
#endif
			errormsg(SH_DICT,ERROR_exit((lvalue->emode&3)!=0),lvalue->value,*ptr);
	}
	*ptr = str;
	return(r);
}

/*
 * convert number defined by string to a Sfdouble_t
 * ptr is set to the last character processed
 * if mode>0, an error will be fatal with value <mode>
 */

Sfdouble_t sh_strnum(register const char *str, char** ptr, int mode)
{
	register Sfdouble_t d;
	char base=0, *last;
	if(*str==0)
		return(0);
	errno = 0;
	d = strtonll(str,&last,&base,-1);
	if(*last || errno)
	{
		d = strval(str,&last,arith,mode);
		if(!ptr && *last && mode>0)
			errormsg(SH_DICT,ERROR_exit(1),e_lexbadchar,*last,str);
	}
	if(ptr)
		*ptr = last;
	return(d);
}

Sfdouble_t sh_arith(register const char *str)
{
	return(sh_strnum(str, (char**)0, 1));
}

void	*sh_arithcomp(register char *str)
{
	const char *ptr = str;
	Arith_t *ep;
	ep = arith_compile(str,(char**)&ptr,arith,ARITH_COMP|1);
	if(*ptr)
		errormsg(SH_DICT,ERROR_exit(1),e_lexbadchar,*ptr,str);
	return((void*)ep);
}
