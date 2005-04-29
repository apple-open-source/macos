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
 * AT&T Labs
 *
 */

#include	"defs.h"
#include	<ctype.h>
#include	"variables.h"
#include	"path.h"
#include	"lexstates.h"
#include	"timeout.h"
#include	"FEATURE/locale"
#include	"FEATURE/externs"

#if !_lib_pathnative && _lib_uwin_path

#define _lib_pathnative		1

extern int	uwin_path(const char*, char*, int);

size_t
pathnative(const char* path, char* buf, size_t siz)
{
	return uwin_path(path, buf, siz);
}

#endif /* _lib_pathnative */

static void	attstore(Namval_t*,void*);
#ifndef _ENV_H
static void	pushnam(Namval_t*,void*);
static char	*staknam(Namval_t*, char*);
#endif
static void	ltou(const char*,char*);
static void	rightjust(char*, int, int);

struct adata
{
	char    **argnam;
	int     attsize;
	char    *attval;
};

char		nv_local = 0;
#ifndef _ENV_H
static void(*nullscan)(Namval_t*,void*);
#endif

#if ( SFIO_VERSION  <= 20010201L )
#   define _data        data
#endif

/* ========	name value pair routines	======== */

#include	"shnodes.h"
#include	"builtins.h"

static char *getbuf(size_t len)
{
	static char *buf;
	static size_t buflen;
	if(buflen < len)
	{
		if(buflen==0)
			buf = (char*)malloc(len);
		else
			buf = (char*)realloc(buf,len);
		buflen = len;
	}
	return(buf);
}

#ifdef _ENV_H
void sh_envput(Env_t* ep,Namval_t *np)
{
	int offset = staktell();
	Namarr_t *ap = nv_arrayptr(np);
	char *val;
	if(ap)
	{
		if(ap->nelem&ARRAY_UNDEF)
			nv_putsub(np,"0",0L);
		else if(!(val=nv_getsub(np)) || strcmp(val,"0"))
			return;
	}
	if(!(val = nv_getval(np)))
		return;
	stakputs(nv_name(np));
	stakputc('=');
	stakputs(val);
	stakseek(offset);
	env_add(ep,stakptr(offset),ENV_STRDUP);
}
#endif

/*
 * output variable name in format for re-input
 */
void nv_outname(Sfio_t *out, char *name, int len)
{
	const char *cp=name, *sp;
	int c, offset = staktell();
	while(sp= strchr(cp,'['))
	{
		if(len>0 && cp+len < sp)
			break;
		sfwrite(out,cp,++sp-cp);
		stakseek(offset);
		for(; c= *sp; sp++)
		{
			if(c==']')
				break;
			else if(c=='\\')
			{
				if(*sp=='[' || *sp==']' || *sp=='\\')
					c = *sp++;
			}
			stakputc(c);
		}
		stakputc(0);
		sfputr(out,sh_fmtq(stakptr(offset)),-1);
		if(len>0)
		{
			sfputc(out,']');
			return;
		}
		cp = sp;
	}
	if(*cp)
	{
		if(len>0)
			sfwrite(out,cp,len);
		else
			sfputr(out,cp,-1);
	}
	stakseek(offset);
}

/*
 * Perform parameter assignment for a linked list of parameters
 * <flags> contains attributes for the parameters
 */
void nv_setlist(register struct argnod *arg,register int flags)
{
	register char *cp;
	register Namval_t *np;
	char *trap=sh.st.trap[SH_DEBUGTRAP];
	int traceon = (sh_isoption(SH_XTRACE)!=0);
	if(sh_isoption(SH_ALLEXPORT))
		flags |= NV_EXPORT;
	if(sh.prefix)
	{
		flags &= ~(NV_IDENT|NV_EXPORT);
		flags |= NV_VARNAME;
	}
	for(;arg; arg=arg->argnxt.ap)
	{
		sh.used_pos = 0;
		if(arg->argflag&ARG_MAC)
			cp = sh_mactrim(arg->argval,-1);
		else
		{
			stakseek(0);
			if(*arg->argval==0 && arg->argchn.ap && !(arg->argflag&~(ARG_APPEND|ARG_QUOTED)))
			{
#if SHOPT_COMPOUND_ARRAY
				int flag = (NV_VARNAME|NV_ASSIGN);
#else
				int flag = (NV_VARNAME|NV_ARRAY|NV_ASSIGN);
#endif /* SHOPT_COMPOUND_ARRAY */
				struct fornod *fp=(struct fornod*)arg->argchn.ap;
				register union anynode *tp=fp->fortre;
				char *prefix = sh.prefix;
				if(arg->argflag&ARG_QUOTED)
					cp = sh_mactrim(fp->fornam,-1);
				else
					cp = fp->fornam;
				error_info.line = fp->fortyp-sh.st.firstline;
				if(sh.fn_depth && (Namval_t*)tp->com.comnamp==SYSTYPESET)
			                flag |= NV_NOSCOPE;
				if(prefix)
					*--cp = '.';
				np = nv_open(cp,sh.var_tree,flag);
				if(prefix)
					*cp++ = 0;
				/* check for array assignment */
				if(tp->tre.tretyp!=TLST && tp->com.comarg && !tp->com.comset)
				{
					int argc;
					char **argv = sh_argbuild(&argc,&tp->com,0);
					if(!(arg->argflag&ARG_APPEND))
						nv_unset(np);
					nv_setvec(np,(arg->argflag&ARG_APPEND),argc,argv);
					if(traceon || trap)
					{
						int n = -1;
						char *name = nv_name(np);
						if(arg->argflag&ARG_APPEND)
							n = '+';
						if(trap)
							sh_debug(trap,name,(char*)0,argv,(arg->argflag&ARG_APPEND)|ARG_ASSIGN);
						if(traceon)
						{
							sh_trace(NIL(char**),0);
							sfputr(sfstderr,name,n);
							sfwrite(sfstderr,"=( ",3);
							while(cp= *argv++)
								sfputr(sfstderr,sh_fmtq(cp),' ');
							sfwrite(sfstderr,")\n",2);
						}
					}
					continue;
				}
				if(tp->tre.tretyp==TLST || !tp->com.comset || tp->com.comset->argval[0]!='[')
				{
#if SHOPT_COMPOUND_ARRAY
					if(*cp!='[' && strchr(cp,'['))
					{
						nv_close(np);
						np = nv_open(cp,sh.var_tree,flag);
					}
#endif /* SHOPT_COMPOUND_ARRAY */
					if((arg->argflag&ARG_APPEND) && !nv_isarray(np))
						nv_unset(np);
				}
				else
				{
					if(sh_isoption(SH_BASH))
					{
						if(!(arg->argflag&ARG_APPEND))
							nv_unset(np);
					}
					else if((arg->argflag&ARG_APPEND) && (!nv_isarray(np) || (nv_aindex(np)>=0)))
					{
						nv_unset(np);
						nv_setarray(np,nv_associative);
					}
					else
						nv_setarray(np,nv_associative);
				}
				if(prefix)
				{
					int offset=0;
					stakputs(prefix);
					stakputc('.');
					if(*cp=='[')
						offset = staktell()+1;
					stakputs(cp);
					if(offset && sh_checkid(stakptr(offset),(char*)0))
						stakseek(staktell()-2);
					cp = stakfreeze(1);
				}
				sh.prefix = cp;
				sh_exec(tp,sh_isstate(SH_ERREXIT));
				sh.prefix = prefix;
				if(nv_isnull(np))
					nv_setvtree(np);
				continue;
			}
			cp = arg->argval;
		}
		if(sh.prefix && *cp=='.' && cp[1]=='=')
			cp++;
		np = nv_open(cp,sh.var_tree,flags);
		if(!np->nvfun)
		{
			if(sh.used_pos)
				nv_onattr(np,NV_PARAM);
			else
				nv_offattr(np,NV_PARAM);
		}
		if(traceon || trap)
		{
			register char *sp=cp;
			char *name=nv_name(np);
			char *sub=0;
			int append = 0;
			if(nv_isarray(np))
				sub = nv_getsub(np);
			if(cp=strchr(sp,'='))
			{
				if(cp[-1]=='+')
					append = ARG_APPEND;
				cp++;
			}
			if(traceon)
			{
				sh_trace(NIL(char**),0);
				nv_outname(sfstderr,name,-1);
				if(sub)
					sfprintf(sfstderr,"[%s]",sh_fmtq(sub));
				if(cp)
				{
					if(append)
						sfputc(sfstderr,'+');
					sfprintf(sfstderr,"=%s\n",sh_fmtq(cp));
				}
			}
			if(trap)
			{
					char *av[2];
					av[0] = cp;
					av[1] = 0;
					sh_debug(trap,name,sub,av,append);
			}
		}
	}
}

/*
 * copy the subscript onto the stack
 */
static void stak_subscript(const char *sub, int last)
{
	register int c;
	stakputc('[');
	while(c= *sub++)
	{
		if(c=='[' || c==']' || c=='\\')
			stakputc('\\');
		stakputc(c);
	}
	stakputc(last);
}

/*
 * construct a new name from a prefix and base name on the stack
 */
static char *newname(register const char *prefix, register const char *name, const char *sub, char **lastptr)
{
	register int last=0,offset = staktell();
	if(prefix)
	{
		stakputs(prefix);
		if(*name=='.' && name[1]=='[')
			last = staktell()+2;
		if(*name!='[' && *name!='.' && *name!='=')
			stakputc('.');
	}
	stakputs(name);
	if(last && sh_checkid(stakptr(last),(char*)0))
		stakseek(staktell()-2);
	if(sub)
		stak_subscript(sub,']');
	if(lastptr)
	{
		char *cp = *lastptr;
		last = staktell();
		if(*cp)
			stakputs(cp);
	}
	stakputc(0);
	if(lastptr)
		*lastptr = stakptr(last);
	return(stakptr(offset));
}

/*
 * Put <arg> into associative memory.
 * If <flags> & NV_ARRAY then subscript is not allowed
 * If <flags> & NV_NOSCOPE then use the current scope only
 * If <flags> & NV_ASSIGN then assignment is allowed
 * If <flags> & NV_IDENT then name must be an identifier
 * If <flags> & NV_VARNAME then name must be a valid variable name
 * If <flags> & NV_NOADD then node will not be added if not found
 * If <flags> & NV_NOREF then don't follow reference
 * If <flags> & NV_NOFAIL then don't generate an error message on failure
 * SH_INIT is only set while initializing the environment
 */
Namval_t	*nv_open(const char *name,Dt_t *root,int flags)
{
	register char *cp = (char*)name;
	register Namval_t	*np=0;
	register int sep = *cp;
	register char *lastdot = 0;
	register long mode = ((flags&NV_NOADD)?0:NV_ADD);
	char *sub, *ptr;
	int offset = -1, fun=0;
#if SHOPT_APPEND
	int append=0;
#endif /* SHOPT_APPEND */

	if(root==sh.alias_tree)
	{
		while((sep= *(unsigned char*)cp++) && (sep!='=') && (sep!='/') && 
			(sep>=0x200 || !(sep=sh_lexstates[ST_NORM][sep]) || sep==S_EPAT));
		cp--;
	}
	else
	{
		sh.last_table = 0;
		if(!root)
			root = sh.var_tree;
		else if(root==sh_subfuntree(1))
		{
			fun=1;
			if(sh.namespace && !strchr(name,'.'))
			{
				name = cp = newname(nv_name(sh.namespace),name,(char*)0,(char**)0);
				flags &=  ~NV_IDENT;
			}
		}
		if(root==sh.var_tree && *name=='.' && name[1])
			root = sh.var_base;
		else
			sh.last_table = sh.namespace;
		if(!(flags&(NV_IDENT|NV_VARNAME|NV_ASSIGN)))
		{
			if(flags&NV_NOSCOPE)
				mode |= HASH_SCOPE|HASH_NOSCOPE;
			np = nv_search(name,root,mode);
			if(np && !(flags&NV_NOREF))
			{
				while(nv_isref(np))
				{
					sh.last_table = nv_table(np);
					np = nv_refnode(np);
				}
			}
			return(np);
		}
		if(sh.prefix && (flags&NV_ASSIGN))
		{
			offset = staktell();
			if(sh.prefix && *name=='.' && name[1]==0)
				cp = sh.prefix;
			else
				cp = newname(sh.prefix,name,(char*)0,(char**)0);
			name = cp;
			if(*cp=='.' && root==sh.var_tree)
				root = sh.var_base;
			sep = *(unsigned char*)cp;
		}
		/* first skip over alpha-numeric */
		while(1)
		{
			if(sep=='.')
			{
				if(flags&NV_IDENT)
					goto failed;
				if(root==sh.var_tree)
					flags &= ~(NV_NOSCOPE|NV_EXPORT);
				if(!lastdot && cp!=name && (flags&NV_VARNAME))
				{
					/* see whether first component is ref */
					*cp = 0;
					np = nv_search(name,sh.var_tree,0);
					*cp = '.';
					if(np && nv_isref(np))
					{
						/* don't optimize */
						sh.argaddr = 0; 
						/* substitute ref name */
						while(nv_isref(np))
						{
							sub = np->nvenv;
							sh.last_table = nv_table(np);
							np = nv_refnode(np);
						}
						ptr = cp;
						name=newname((char*)0,nv_name(np),sub,&ptr);
						cp = ptr;
					}
				}
				lastdot = cp++;
			}
			if(sep= *(unsigned char*)cp, !isaletter(sep))
			{
#if SHOPT_COMPOUND_ARRAY
				if(sep!='[' || !(flags&NV_VARNAME) || fun)
					break;
				if(cp==lastdot+1)
					np = 0;
				ptr = nv_endsubscript(np,cp,0);
				if(cp==lastdot+1)
				{
					cp = lastdot = ptr;
					sep = *cp;
					continue;
				}
				if(!ptr || *ptr!='.')
					break;
				if((ptr-cp)==3 && cp[1]=='0')
				{
					strcpy(cp,ptr);
					ptr = cp;
				}
				*ptr = 0;
				np = nv_search(name,root,0);
				*ptr = '.';
				if(np)
					lastdot = ptr;
				else
				{
					*cp = 0;
					np = nv_search(name,root,0);
					*cp = '[';
					if(np)
						lastdot = cp;
				}
				if(!lastdot)
					lastdot = (char*)name;
				cp = ptr+1;
				if(sep= *(unsigned char*)cp, !isaletter(sep))
					goto failed;
#else
				break;
#endif /* SHOPT_COMPOUND_ARRAY */
			}
			while(sep= *(unsigned char*)(++cp),isaname(sep));
		}
		if(offset>=0)
			stakseek(offset);
#if SHOPT_COMPOUND_ARRAY
	retry:
#endif /* SHOPT_COMPOUND_ARRAY */
		/* if name doesn't have to be an varname or ident skip to '=' */
#if SHOPT_APPEND
		if(sep && sep!='=' && sep!='[' && sep!='+')
#else
		if(sep && sep!='=' && sep!='[')
#endif /* SHOPT_APPEND */
		{
			if(flags&NV_IDENT)
				goto failed;
			else if(flags&NV_VARNAME)
			{
				if(flags&NV_NOFAIL)
					return(0);
				errormsg(SH_DICT,ERROR_exit(1),(root==sh.var_tree?e_varname:e_funname),name);
			}
			while((sep= *++cp) && sep!='=');
		}
		if(lastdot==name)
			lastdot = 0;
		if(lastdot)
		{
			if(sep)
				*cp = 0;
			np = nv_search(name,root,0);
			if(sep)
				*cp = sep;
			if(!np && sep=='[')
				ptr = nv_endsubscript(np,cp,0);
			else
				ptr = cp;
		}
		while(!np && !fun && lastdot && lastdot>name)
		{
			int c = *lastdot;
			*lastdot=0;
			np = nv_search(name,fun?sh.var_tree:root,0);
			*lastdot=c;
			if(c=='[')
				lastdot = nv_endsubscript(np,lastdot,0);
			if(np)
			{
				char *sp;
				Namval_t *nq;
				Namfun_t *fp=0;
				if(!nv_isref(np) && (fp=np->nvfun))
				{
					for(; fp; fp=fp->next)
					{
						if(fp->disc && fp->disc->createf)
							break;
					}
				}
				if(fp)
				{
					c = *ptr;
					*ptr = 0;
					np = nv_create(nq=np,lastdot+1,flags&NV_NOADD,(Namfun_t*)0);
					*ptr = c;
					if(np==nq)
					{
						if(flags&NV_NOFAIL)
							return(0);
						errormsg(SH_DICT,ERROR_exit(1),e_varname,name);
					}
				}
				else if((sp=strchr(lastdot+1,'.')) && sp<cp)
				{
					if(flags&NV_NOFAIL)
						return(0);
					errormsg(SH_DICT,ERROR_exit(1),e_noparent,name);
				}
				else
					np = 0;
				break;
			}
			while(--lastdot>name && *lastdot!='.');
			if(lastdot==name)
			{
				if(flags&NV_NOFAIL)
					return(0);
				errormsg(SH_DICT,ERROR_exit(1),e_noparent,name);
			}
		}
	}
	if(cp!=name)
	{
		if(sep && sh.subshell && root==sh.alias_tree)
			root = sh_subaliastree(1);
		if(sep)
			*cp = 0;
		if((flags&NV_NOSCOPE) && dtvnext(root) && root==sh.var_tree 
			&& (np=nv_search(name,sh.var_base,0)))
		{
			Namfun_t *disc = nv_cover(np);
			char *name = nv_name(np);
			if(np=nv_search((char*)np,root,mode|HASH_NOSCOPE|HASH_SCOPE|HASH_BUCKET))
			{
				np->nvfun = disc;
				np->nvname = name;
			}
		}
		if(!np)
		{
			if(flags&NV_NOSCOPE)
				mode |= HASH_SCOPE|HASH_NOSCOPE;
			np = nv_search(name,root,mode);
		}
#if SHOPT_NAMESPACE
		if(*name!='.' && sh.namespace && np && root==sh.var_tree && dtsearch(sh.var_base,np)==np)
			np = nv_mkclone(np);
#endif /* SHOPT_NAMESPACE */
		if(sep)
			*cp = sep;
		if(((flags&NV_NOREF) && sep==0) || (!np && (flags&NV_NOADD)))
			return(np);
		sub = 0;
		if(flags&NV_NOREF)
			nv_unref(np);
		else while(nv_isref(np))
		{
			sub = np->nvenv;
			sh.last_table = nv_table(np);
			np = nv_refnode(np);
		}
		if(sub)	/* nameref to a subscript */
		{
			int offset = staktell();
			stak_subscript(sub,']');
			stakputc(0);
			nv_endsubscript(np,stakptr(offset),NV_ADD);
			stakseek(offset);
		}
		/* check for subscript*/
		else if(sep=='[' && !(flags&NV_ARRAY))
		{
			sep = (flags&NV_ASSIGN?NV_ADD:0);
#if SHOPT_COMPOUND_ARRAY
			lastdot = nv_endsubscript(np,cp,NV_ADD);
			if((sep = *lastdot)=='.')
			{
				sub = nv_getsub(np);
				ptr = lastdot;
				name = newname((char*)0,nv_name(np),sub?sub:"0",&ptr);
				cp= ptr;
				lastdot = 0;
				np = 0;
				while(1)
				{
					while(sep= *(unsigned char*)(++cp),isaname(sep));
					if(sep!='.')
						break;
					lastdot = cp;
				}
				goto retry;
			}
			cp = lastdot;
#else
			cp = nv_endsubscript(np,cp,NV_ADD);
			sep = *cp;
#endif /* SHOPT_COMPOUND_ARRAY */
		}
		else if(nv_isarray(np))
			nv_putsub(np,NIL(char*),ARRAY_UNDEF);
#if SHOPT_APPEND
		if(sep=='+')
		{
			append = NV_APPEND;
			sep = *++cp;
		}
#endif /* SHOPT_APPEND */
		if(sep && ((sep!='=')||!(flags&NV_ASSIGN)))
		{
			if(sh_isstate(SH_INIT))
				return(0);
			goto failed;
		}
		if(sep == '=')
		{
			cp++;
			if(sh_isstate(SH_INIT))
			{
				nv_putval(np, cp, NV_RDONLY);
				if(np==PWDNOD)
					nv_onattr(np,NV_TAGGED);
			}
			else
			{
				sep = (root==sh.alias_tree?0:flags&NV_EXPORT);
#if SHOPT_APPEND
				nv_putval(np, cp, append|sep);
#else
				nv_putval(np, cp, sep);
#endif /*SHOPT_APPEND */
#if SHOPT_BSH
				if(flags&NV_EXPORT)
					nv_offattr(np,NV_IMPORT);
#endif /* SHOPT_BSH */
			}
			nv_onattr(np, flags&NV_ATTRIBUTES);
		}
		return(np);
	}
failed:
	if(!sh_isstate(SH_INIT) && !(flags&NV_NOFAIL))
		errormsg(SH_DICT,ERROR_exit(1),(root==sh.alias_tree?e_aliname:e_ident),name);
	return(0);
}

#if SHOPT_MULTIBYTE
    static char *savep;
    static char savechars[8+1];
    static int ja_size(char*, int, int);
    static void ja_restore(void);
#endif /* SHOPT_MULTIBYTE */

/*
 * put value <string> into name-value node <np>.
 * If <np> is an array, then the element given by the
 *   current index is assigned to.
 * If <flags> contains NV_RDONLY, readonly attribute is ignored
 * If <flags> contains NV_INTEGER, string is a pointer to a number
 * If <flags> contains NV_NOFREE, previous value is freed, and <string>
 * becomes value of node and <flags> becomes attributes
 */
void nv_putval(register Namval_t *np, const char *string, int flags)
{
	register const char *sp=string;
	register union Value *up;
	register char *cp;
	register int size = 0;
	register int dot;
	int	was_local = nv_local;
	if(!(flags&NV_RDONLY) && nv_isattr (np, NV_RDONLY))
		errormsg(SH_DICT,ERROR_exit(1),e_readonly, nv_name(np));
	/* The following could cause the shell to fork if assignment
	 * would cause a side effect
	 */
	sh.argaddr = 0;
	if(sh.subshell && !nv_local)
		np = sh_assignok(np,1);
	if(np->nvfun && !nv_isattr(np,NV_REF))
	{
		/* This function contains disc */
		if(!nv_local)
		{
			nv_local=1;
			nv_putv(np,sp,flags,np->nvfun);
#ifdef _ENV_H
			if((flags&NV_EXPORT) || nv_isattr(np,NV_EXPORT))
				sh_envput(sh.env,np);
#endif
			return;
		}
		/* called from disc, assign the actual value */
	}
	flags &= ~NV_NODISC;
	if(flags&(NV_NOREF|NV_NOFREE))
	{
		if(!nv_isnull(np))
			nv_unset(np);
		nv_local=0;
		np->nvalue.cp = (char*)sp;
		nv_setattr(np,(flags&~NV_RDONLY)|NV_NOFREE);
		return;
	}
	nv_local=0;
	up= &np->nvalue;
#if !SHOPT_BSH
	if(nv_isattr(np,NV_EXPORT))
		nv_offattr(np,NV_IMPORT);
	if(!nv_isattr(np,NV_MINIMAL))
		np->nvenv = 0;
#endif /* SHOPT_BSH */
	if(nv_isattr (np, NV_INTEGER))
	{
		if(nv_isattr(np, NV_DOUBLE))
		{
			if(nv_isattr(np, NV_LONG) && sizeof(double)<sizeof(Sfdouble_t))
			{
				Sfdouble_t ld, old=0;
				if(flags&NV_INTEGER)
				{
					if(flags&NV_LONG)
						ld = *((Sfdouble_t*)sp);
					else if(flags&NV_SHORT)
						ld = *((float*)sp);
					else
						ld = *((double*)sp);
				}
				else
					ld = sh_arith(sp);
				if(!up->ldp)
					up->ldp = new_of(Sfdouble_t,0);
				else if(flags&NV_APPEND)
					old = *(up->ldp);
				*(up->ldp) = ld+old;
			}
			else
			{
				double d,od=0;
				if(flags&NV_INTEGER)
				{
					if(flags&NV_LONG)
						d = (double)(*(Sfdouble_t*)sp);
					else if(flags&NV_SHORT)
						d = (double)(*(float*)sp);
					else
						d = *(double*)sp;
				}
				else
					d = sh_arith(sp);
				if(!up->dp)
					up->dp = new_of(double,0);
				else if(flags&NV_APPEND)
					od = *(up->dp);
				*(up->dp) = d+od;
			}
		}
		else
		{
			if(nv_isattr(np, NV_LONG) && sizeof(long)<sizeof(Sflong_t))
			{
				Sflong_t ll=0,oll=0;
				if(flags&NV_INTEGER)
				{
					if(flags&NV_DOUBLE)
					{
						if(flags&NV_LONG)
							ll = *((Sfdouble_t*)sp);
						else if(flags&NV_SHORT)
							ll = *((float*)sp);
						else
							ll = *((double*)sp);
					}
					else
					{
						if(flags&NV_LONG)
							ll = *((Sflong_t*)sp);
						else if(flags&NV_SHORT)
							ll = *((short*)sp);
						else
							ll = *((long*)sp);
					}
				}
				else if(sp)
					ll = (Sflong_t)sh_arith(sp);
				if(!up->llp)
					up->llp = new_of(Sflong_t,0);
				else if(flags&NV_APPEND)
					oll = *(up->llp);
				*(up->llp) = ll+oll;
			}
			else
			{
				long l=0,ol=0;
				if(flags&NV_INTEGER)
				{
					if(flags&NV_DOUBLE)
					{
						Sflong_t ll;
						if(flags&NV_LONG)
							ll = *((Sfdouble_t*)sp);
						else if(flags&NV_SHORT)
							ll = *((float*)sp);
						else
							ll = *((double*)sp);
						l = (long)ll;
					}
					else
					{
						if(flags&NV_LONG)
							l = *((Sflong_t*)sp);
						else if(flags&NV_SHORT)
							l = *((short*)sp);
						else
							l = *(long*)sp;
					}
				}
				else if(sp)
				{
					Sfdouble_t ld = sh_arith(sp);
					if(ld<0)
						l = (long)ld;
					else
						l = (unsigned long)ld;
				}
				if(nv_size(np) <= 1)
					nv_setsize(np,10);
				if(nv_isattr (np, NV_SHORT))
				{
					short s=0;
					if(flags&NV_APPEND)
						s = up->s;
					up->s = s+(short)l;
					nv_onattr(np,NV_NOFREE);
				}
				else
				{
					if(!up->lp)
						up->lp = new_of(long,0);
					else if(flags&NV_APPEND)	
						ol =  *(up->lp);
					*(up->lp) = l+ol;
					if(l && *sp++ == '0')
						nv_onattr(np,NV_UNSIGN);
				}
			}
		}
	}
	else
	{
		const char *tofree=0;
		char numbuf[40];
#if SHOPT_APPEND
		int offset;
#endif /* SHOPT_APPEND */
#if _lib_pathnative
		char buff[PATH_MAX];
#endif /* _lib_pathnative */
		if(flags&NV_INTEGER)
		{
			if(flags&NV_DOUBLE)
			{
				if(flags&NV_LONG)
					sfsprintf(numbuf,sizeof(numbuf),"%.*Lg\0",12,*((Sfdouble_t*)sp));
				else
					sfsprintf(numbuf,sizeof(numbuf),"%.*g\0",12,*((double*)sp));
			}
			else if(flags&NV_LONG)
				sfsprintf(numbuf,sizeof(numbuf),"%lld\0",*((Sflong_t*)sp));
			else
				sfsprintf(numbuf,sizeof(numbuf),"%ld\0",*((long*)sp));
			sp = numbuf;
		}
		if(nv_isattr(np, NV_HOST)==NV_HOST && sp)
		{
#ifdef _lib_pathnative
			/*
			 * return the host file name given the UNIX name
			 */
			pathnative(sp,buff,sizeof(buff));
			if(buff[1]==':' && buff[2]=='/')
			{
				buff[2] = '\\';
				if(*buff>='A' &&  *buff<='Z')
					*buff += 'a'-'A';
			}
			sp = buff;
#else
			;
#endif /* _lib_pathnative */
		}
		else if((nv_isattr(np, NV_RJUST|NV_ZFILL|NV_LJUST)) && sp)
		{
			for(;*sp == ' '|| *sp=='\t';sp++);
	        	if((nv_isattr(np,NV_ZFILL)) && (nv_isattr(np,NV_LJUST)))
				for(;*sp=='0';sp++);
			size = nv_size(np);
#if SHOPT_MULTIBYTE
			if(size)
				size = ja_size((char*)sp,size,nv_isattr(np,NV_RJUST|NV_ZFILL));
#endif /* SHOPT_MULTIBYTE */
		}
#if SHOPT_APPEND
		if(!up->cp)
			flags &= ~NV_APPEND;
		if((flags&NV_APPEND) && !nv_isattr(np,NV_BINARY))
		{
			offset = staktell();
			stakputs(up->cp);
			stakputs(sp);
			stakputc(0);
			sp = stakptr(offset);
		}
#endif /*SHOPT_APPEND */
		if(!nv_isattr(np, NV_NOFREE))
		{
			/* delay free in case <sp> points into free region */
			tofree = up->cp;
		}
		nv_offattr(np,NV_NOFREE);
       	 	if (sp)
		{
			dot = strlen(sp);
#if (_AST_VERSION>=20030127L)
			if(nv_isattr(np,NV_BINARY))
			{
				int oldsize = (flags&NV_APPEND)?nv_size(np):0;
				if(flags&NV_RAW)
				{
					up->cp = sp;
					return;
				}
				size = 0;
				if(nv_isattr(np,NV_ZFILL))
					size = nv_size(np);
				if(size==0)
					size = oldsize + (3*dot/4);
				cp = (char*)malloc(size+1);
				if(oldsize)
					memcpy((void*)cp,(void*)up->cp,oldsize);
				up->cp = cp;
				if(size <= oldsize)
					return;
				dot = base64decode(sp,dot, (void**)0, cp+oldsize, size-oldsize,(void**)0);
				dot += oldsize;
				if(!nv_isattr(np,NV_ZFILL) || nv_size(np)==0)
					nv_setsize(np,dot);
				else if(nv_isattr(np,NV_ZFILL) && (size>dot))
					memset((void*)&cp[dot],0,size-dot);
				return;
			}
			else
#endif
			if(size==0 && nv_isattr(np,NV_LJUST|NV_RJUST|NV_ZFILL))
				nv_setsize(np,size=dot);
			else if(size > dot)
				dot = size;
			cp = (char*)malloc(((unsigned)dot+1));
		}
		else
			cp = 0;
		up->cp = cp;
		if(sp)
		{
			if(nv_isattr(np, NV_LTOU))
				ltou(sp,cp);
			else if(nv_isattr (np, NV_UTOL))
				sh_utol(sp,cp);
			else
       			 	strcpy(cp, sp);
			if(nv_isattr(np, NV_RJUST) && nv_isattr(np, NV_ZFILL))
				rightjust(cp,size,'0');
			else if(nv_isattr(np, NV_RJUST))
				rightjust(cp,size,' ');
			else if(nv_isattr(np, NV_LJUST))
			{
				register char *dp;
				dp = strlen (cp) + cp;
				*(cp = (cp + size)) = 0;
				for (; dp < cp; *dp++ = ' ');
			 }
#if SHOPT_MULTIBYTE
			/* restore original string */
			if(savep)
				ja_restore();
#endif /* SHOPT_MULTIBYTE */
		}
#if SHOPT_APPEND
		if(flags&NV_APPEND)
			stakseek(offset);
#endif /* SHOPT_APPEND */
		if(tofree)
			free((void*)tofree);
	}
#ifdef _ENV_H
	if(!was_local && ((flags&NV_EXPORT) || nv_isattr(np,NV_EXPORT)))
		sh_envput(sh.env,np);
#endif
	return;
}

/*
 *
 *   Right-justify <str> so that it contains no more than
 *   <size> characters.  If <str> contains fewer than <size>
 *   characters, left-pad with <fill>.  Trailing blanks
 *   in <str> will be ignored.
 *
 *   If the leftmost digit in <str> is not a digit, <fill>
 *   will default to a blank.
 */
static void rightjust(char *str, int size, int fill)
{
	register int n;
	register char *cp,*sp;
	n = strlen(str);

	/* ignore trailing blanks */
	for(cp=str+n;n && *--cp == ' ';n--);
	if (n == size)
		return;
	if(n > size)
        {
        	*(str+n) = 0;
        	for (sp = str, cp = str+n-size; sp <= str+size; *sp++ = *cp++);
        	return;
        }
	else *(sp = str+size) = 0;
	if (n == 0)  
        {
        	while (sp > str)
               		*--sp = ' ';
        	return;
        }
	while(n--)
	{
		sp--;
		*sp = *cp--;
	}
	if(!isdigit(*str))
		fill = ' ';
	while(sp>str)
		*--sp = fill;
	return;
}

#if SHOPT_MULTIBYTE
    /*
     * handle left and right justified fields for multi-byte chars
     * given physical size, return a logical size which reflects the
     * screen width of multi-byte characters
     * Multi-width characters replaced by spaces if they cross the boundary
     * <type> is non-zero for right justified  fields
     */

    static int ja_size(char *str,int size,int type)
    {
	register char *cp = str;
	register int c, n=size;
	register int outsize;
	register char *oldcp=cp;
	int oldn;
	wchar_t w;
	while(*cp)
	{
		oldn = n;
		w = mbchar(cp);
		outsize = mbwidth(w);
		size -= outsize;
		c = cp-oldcp;
		n += (c-outsize);
		oldcp = cp;
		if(size<=0 && type==0)
			break;
	}
	/* check for right justified fields that need truncating */
	if(size <0)
	{
		if(type==0)
		{
			/* left justified and character crosses field boundary */
			n = oldn;
			/* save boundary char and replace with spaces */
			size = c;
			savechars[size] = 0;
			while(size--)
			{
				savechars[size] = cp[size];
				cp[size] = ' ';
			}
			savep = cp;
		}
		size = -size;
		if(type)
			n -= (ja_size(str,size,0)-size);
	}
	return(n);
    }

    static void ja_restore(void)
    {
	register char *cp = savechars;
	while(*cp)
		*savep++ = *cp++;
	savep = 0;
    }
#endif /* SHOPT_MULTIBYTE */

#ifndef _ENV_H
static char *staknam(register Namval_t *np, char *value)
{
	register char *p,*q;
	q = stakalloc(strlen(nv_name(np))+(value?strlen(value):0)+2);
	p=strcopy(q,nv_name(np));
	if(value)
	{
		*p++ = '=';
		strcpy(p,value);
	}
	return(q);
}
#endif

/*
 * put the name and attribute into value of attributes variable
 */
#ifdef _ENV_H
static void attstore(register Namval_t *np, void *data)
{
	register int flag, c = ' ';
	NOT_USED(data);
	if(!(nv_isattr(np,NV_EXPORT)))
		return;
	flag = nv_isattr(np,NV_RDONLY|NV_UTOL|NV_LTOU|NV_RJUST|NV_LJUST|NV_ZFILL|NV_INTEGER);
	stakputc('=');
	if((flag&NV_DOUBLE) && (flag&NV_INTEGER))
	{
		/* export doubles as integers for ksh88 compatibility */
		stakputc(c+(flag&~(NV_DOUBLE|NV_EXPNOTE)));
	}
	else
	{
		stakputc(c+flag);
		if(flag&NV_INTEGER)
			c +=  nv_size(np);
	}
	stakputc(c);
	stakputs(nv_name(np));
}
#else
static void attstore(register Namval_t *np, void *data)
{
	register int flag = np->nvflag;
	register struct adata *ap = (struct adata*)data;
	if(!(flag&NV_EXPORT) || (flag&NV_FUNCT))
		return;
	flag &= (NV_RDONLY|NV_UTOL|NV_LTOU|NV_RJUST|NV_LJUST|NV_ZFILL|NV_INTEGER);
	*ap->attval++ = '=';
	if((flag&NV_DOUBLE) && (flag&NV_INTEGER))
	{
		/* export doubles as integers for ksh88 compatibility */
		*ap->attval++ = ' '+(flag&~(NV_DOUBLE|NV_EXPNOTE));
		*ap->attval = ' ';
	}
	else
	{
		*ap->attval++ = ' '+flag;
		if(flag&NV_INTEGER)
			*ap->attval = ' ' + nv_size(np);
		else
			*ap->attval = ' ';
	}
	ap->attval = strcopy(++ap->attval,nv_name(np));
}
#endif

#ifndef _ENV_H
static void pushnam(Namval_t *np, void *data)
{
	register char *value;
	register struct adata *ap = (struct adata*)data;
	if(nv_isattr(np,NV_IMPORT))
	{
		if(np->nvenv)
			*ap->argnam++ = np->nvenv;
	}
	else if(value=nv_getval(np))
		*ap->argnam++ = staknam(np,value);
	if(nv_isattr(np,NV_RDONLY|NV_UTOL|NV_LTOU|NV_RJUST|NV_LJUST|NV_ZFILL|NV_INTEGER))
		ap->attsize += (strlen(nv_name(np))+4);
}
#endif

/*
 * Generate the environment list for the child.
 */

#ifdef _ENV_H
char **sh_envgen(void)
{
	int offset,tell;
	register char **er;
	env_delete(sh.env,"_");
	er = env_get(sh.env);
	offset = staktell();
	stakputs(e_envmarker);
	tell = staktell();
	nv_scan(sh.var_tree, attstore,(void*)0,0,(NV_RDONLY|NV_UTOL|NV_LTOU|NV_RJUST|NV_LJUST|NV_ZFILL|NV_INTEGER));
	if(tell ==staktell())
		stakseek(offset);
	else
		*--er = stakfreeze(1)+offset;
	return(er);
}
#else
char **sh_envgen(void)
{
	register char **er;
	register int namec;
	register char *cp;
	struct adata data;
	/* L_ARGNOD gets generated automatically as full path name of command */
	nv_offattr(L_ARGNOD,NV_EXPORT);
	data.attsize = 6;
	namec = nv_scan(sh.var_tree,nullscan,(void*)0,NV_EXPORT,NV_EXPORT);
	er = (char**)stakalloc((namec+4)*sizeof(char*));
	data.argnam = (er+=2);
	nv_scan(sh.var_tree, pushnam,&data,NV_EXPORT, NV_EXPORT);
	*data.argnam = (char*)stakalloc(data.attsize);
	cp = data.attval = strcopy(*data.argnam,e_envmarker);
	nv_scan(sh.var_tree, attstore,&data,0,(NV_RDONLY|NV_UTOL|NV_LTOU|NV_RJUST|NV_LJUST|NV_ZFILL|NV_INTEGER));
	*data.attval = 0;
	if(cp!=data.attval)
		data.argnam++;
	*data.argnam = 0;
	return(er);
}
#endif

struct scan
{
	void    (*scanfn)(Namval_t*, void*);
	int     scanmask;
	int     scanflags;
	int     scancount;
	void    *scandata;
};


static int scanfilter(Dt_t *dict, void *arg, void *data)
{
	register Namval_t *np = (Namval_t*)arg;
	register int k=np->nvflag;
	register struct scan *sp = (struct scan*)data;
	NOT_USED(dict);
	if(sp->scanmask?(k&sp->scanmask)==sp->scanflags:(!sp->scanflags || (k&sp->scanflags)))
	{
		if(!np->nvalue.cp && !nv_isattr(np,~NV_DEFAULT))
			return(0);
		if(sp->scanfn)
		{
			if(nv_isarray(np))
				nv_putsub(np,NIL(char*),0L);
			(*sp->scanfn)(np,sp->scandata);
		}
		sp->scancount++;
	}
	return(0);
}

/*
 * Walk through the name-value pairs
 * if <mask> is non-zero, then only nodes with (nvflags&mask)==flags
 *	are visited
 * If <mask> is zero, and <flags> non-zero, then nodes with one or
 *	more of <flags> is visited
 * If <mask> and <flags> are zero, then all nodes are visted
 */
int nv_scan(Dt_t *root, void (*fn)(Namval_t*,void*), void *data,int mask, int flags)
{
	Dt_t *base=0;
	struct scan sdata;
	int (*hashfn)(Dt_t*, void*, void*);
	sdata.scanmask = mask;
	sdata.scanflags = flags&~NV_NOSCOPE;
	sdata.scanfn = fn;
	sdata.scancount = 0;
	sdata.scandata = data;
	hashfn = scanfilter;
	if(flags&NV_NOSCOPE)
		base = dtview((Dt_t*)root,0);
	dtwalk(root, hashfn,&sdata);
	if(base)
		 dtview((Dt_t*)root,base);
	return(sdata.scancount);
}

/*
 * create a new environment scope
 */
void nv_scope(struct argnod *envlist)
{
	register Dt_t *newscope;
	newscope = dtopen(&_Nvdisc,Dtbag);
	dtview(newscope,(Dt_t*)sh.var_tree);
	sh.var_tree = (Dt_t*)newscope;
	nv_setlist(envlist,NV_EXPORT|NV_NOSCOPE|NV_IDENT|NV_ASSIGN);
}

/* 
 * Remove freeable local space associated with the nvalue field
 * of nnod. This includes any strings representing the value(s) of the
 * node, as well as its dope vector, if it is an array.
 */

void	sh_envnolocal (register Namval_t *np, void *data)
{
	char *cp=0;
	NOT_USED(data);
	if(nv_isattr(np,NV_EXPORT) && nv_isarray(np))
	{
		nv_putsub(np,NIL(char*),0);
		if(cp = nv_getval(np))
			cp = strdup(cp);
	}
	if(nv_isattr(np,NV_EXPORT|NV_NOFREE))
	{
		if(nv_isref(np))
		{
			nv_offattr(np,NV_NOFREE|NV_REF);
			np->nvalue.cp = 0;
			np->nvfun = 0;
		}
		if(!cp)
			return;
	}
	if(nv_isarray(np))
		nv_putsub(np,NIL(char*),ARRAY_SCAN);
	_nv_unset(np,NV_RDONLY);
	nv_setattr(np,0);
	if(cp)
	{
		nv_putval(np,cp,0);
		free((void*)cp);
	}
}

/*
 * Currently this is a dummy, but someday will be needed
 * for reference counting
 */
void	nv_close(Namval_t *np)
{
	NOT_USED(np);
}

static void table_unset(register Dt_t *root, int flags, Dt_t *oroot)
{
	register Namval_t *np,*nq;
	for(np=(Namval_t*)dtfirst(root);np;np=nq)
	{
		_nv_unset(np,flags);
#ifdef _ENV_H
		if(oroot && (nq=nv_search(nv_name(np),oroot,0)) && nv_isattr(nq,NV_EXPORT))
			sh_envput(sh.env,nq);
#endif
		nq = (Namval_t*)dtnext(root,np);
		dtdelete(root,np);
		free((void*)np);
	}
}

/*
 *
 *   Set the value of <np> to 0, and nullify any attributes
 *   that <np> may have had.  Free any freeable space occupied
 *   by the value of <np>.  If <np> denotes an array member, it
 *   will retain its attributes.
 *   <flags> can contain NV_RDONLY to override the readonly attribute
 *	being cleared.
 */
void	_nv_unset(register Namval_t *np,int flags)
{
	register union Value *up;
	if(!(flags&NV_RDONLY) && nv_isattr (np,NV_RDONLY))
		errormsg(SH_DICT,ERROR_exit(1),e_readonly, nv_name(np));
	if(is_afunction(np) && np->nvalue.ip)
	{
		register struct slnod *slp = (struct slnod*)(np->nvenv);
		if(slp)
		{
			/* free function definition */
			register char *name=nv_name(np),*cp= strrchr(name,'.');
			if(cp)
			{
				Namval_t *npv;
				*cp = 0;
				 npv = nv_open(name,sh.var_tree,NV_ARRAY|NV_VARNAME|NV_NOADD);
				*cp++ = '.';
				if(npv)
					nv_setdisc(npv,cp,NIL(Namval_t*),(Namfun_t*)npv);
			}
			stakdelete(slp->slptr);
			np->nvalue.ip = 0;
		}
		goto done;
	}
	if(sh.subshell && !nv_isnull(np))
		np = sh_assignok(np,0);
	nv_offattr(np,NV_NODISC);
	if(np->nvfun && !nv_isref(np))
	{
		/* This function contains disc */
		if(!nv_local)
		{
			nv_local=1;
			nv_putv(np,NIL(char*),flags,np->nvfun);
			return;
		}
		/* called from disc, assign the actual value */
		nv_local=0;
	}
	up = &np->nvalue;
	if(up->cp)
	{
		if(!nv_isattr (np, NV_NOFREE))
			free((void*)up->cp);
		up->cp = 0;
	}
done:
#ifdef _ENV_H
	if(nv_isattr(np,NV_EXPORT))
	{
		char *sub;
		if(!nv_isarray(np) || (sub=nv_getsub(np)) && strcmp(sub,"0")==0)
			env_delete(sh.env,nv_name(np));
	}
#endif
	if(!nv_isarray(np) || !nv_arrayptr(np))
	{
		if(nv_isref(np))
		{
			if(np->nvenv)
				free((void*)np->nvenv);
			np->nvfun = 0;
		}
		nv_setsize(np,0);
		if(!nv_isattr(np,NV_MINIMAL) || nv_isattr(np,NV_EXPORT))
		{
			np->nvenv = 0;
			nv_setattr(np,0);
		}
		else
			nv_setattr(np,NV_MINIMAL);
	}
}

void	nv_unset(register Namval_t *np)
{
	_nv_unset(np,0);
}

/*
 * return the node pointer in the highest level scope
 */
Namval_t *nv_scoped(register Namval_t *np)
{
	if(!dtvnext(sh.var_tree))
		return(np);
	return(dtsearch(sh.var_tree,np));
}

#if 1
/*
 * return space separated list of names of variables in given tree
 */
static char *tableval(Dt_t *root)
{
	static Sfio_t *out;
	register Namval_t *np;
	register int first=1;
	register Dt_t *base = dtview(root,0);
        if(out)
                sfseek(out,(Sfoff_t)0,SEEK_SET);
        else
                out =  sfnew((Sfio_t*)0,(char*)0,-1,-1,SF_WRITE|SF_STRING);
	for(np=(Namval_t*)dtfirst(root);np;np=(Namval_t*)dtnext(root,np))
	{
                if(!nv_isnull(np) || np->nvfun || nv_isattr(np,~NV_NOFREE))
		{
			if(!first)
				sfputc(out,' ');
			else
				first = 0;
			sfputr(out,np->nvname,-1);
		}
	}
	sfputc(out,0);
	if(base)
		dtview(root,base);
	return((char*)out->_data);
}
#endif

#if SHOPT_OPTIMIZE
struct optimize
{
	Namfun_t	hdr;
	char		**ptr;
	struct optimize	*next;
	Namval_t	*np;
};

static struct optimize *opt_free;

static void optimize_clear(Namval_t* np, Namfun_t *fp)
{
	struct optimize *op = (struct optimize*)fp;
	nv_stack(np,fp);
	nv_stack(np,(Namfun_t*)0);
	for(;op && op->np==np; op=op->next)
	{
		if(op->ptr)
		{
			*op->ptr = 0;
			op->ptr = 0;
		}
	}
}

static void put_optimize(Namval_t* np,const char *val,int flags,Namfun_t *fp)
{
	nv_putv(np,val,flags,fp);
	optimize_clear(np,fp);
}

static const Namdisc_t optimize_disc  = {  0, put_optimize };

void nv_optimize(Namval_t *np)
{
	register Namfun_t *fp;
	register struct optimize *op, *xp;
	if(sh.argaddr)
	{
		for(fp=np->nvfun; fp; fp = fp->next)
		{
			if(fp->disc->getnum || fp->disc->getval)
			{
				sh.argaddr = 0;
				return;
			}
			if(fp->disc== &optimize_disc)
				break;
		}
		if((xp= (struct optimize*)fp) && xp->ptr==sh.argaddr)
			return;
		if(op = opt_free)
			opt_free = op->next;
		else
			op=(struct optimize*)calloc(1,sizeof(struct optimize));
		op->ptr = sh.argaddr;
		op->np = np;
		if(xp)
		{
			op->hdr.disc = 0;
			op->next = xp->next;
			xp->next = op;
		}
		else
		{
			op->hdr.disc = &optimize_disc;
			op->next = (struct optimize*)sh.optlist;
			sh.optlist = (void*)op;
			nv_stack(np,&op->hdr);
		}
	}
}

void sh_optclear(Shell_t *shp, void *old)
{
	register struct optimize *op,*opnext;
	for(op=(struct optimize*)shp->optlist; op; op = opnext)
	{
		opnext = op->next;
		if(op->ptr && op->hdr.disc)
		{
			nv_stack(op->np,&op->hdr);
			nv_stack(op->np,(Namfun_t*)0);
		}
		op->next = opt_free;
		opt_free = op;
	}
	shp->optlist = old;
}

#else
#   define	optimize_clear(np,fp)
#endif /* SHOPT_OPTIMIZE */

/*
 *   Return a pointer to a character string that denotes the value
 *   of <np>.  If <np> refers to an array,  return a pointer to
 *   the value associated with the current index.
 *
 *   If the value of <np> is an integer, the string returned will
 *   be overwritten by the next call to nv_getval.
 *
 *   If <np> has no value, 0 is returned.
 */

char *nv_getval(register Namval_t *np)
{
	register union Value *up= &np->nvalue;
	register int numeric;
#if SHOPT_OPTIMIZE
	if(!nv_local && sh.argaddr)
		nv_optimize(np);
#endif /* SHOPT_OPTIMIZE */
	if(!np->nvfun && !nv_isattr(np,NV_ARRAY|NV_INTEGER|NV_FUNCT|NV_REF|NV_TABLE))
		goto done;
	if(nv_isref(np))
	{
		sh.last_table = nv_table(np);
		return(nv_name(up->np));
	}
	if(np->nvfun)
	{
		if(!nv_local)
		{
			nv_local=1;
			return(nv_getv(np, np->nvfun));
		}
		nv_local=0;
	}
	numeric = ((nv_isattr (np, NV_INTEGER)) != 0);
	if(numeric)
	{
		Sflong_t  ll;
		if(!up->cp)
			return("0");
		if(nv_isattr (np,NV_DOUBLE))
		{
			Sfdouble_t ld;
			double d;
			char *curbuf, *format;
			long l = nv_size(np)+8;
			curbuf = getbuf(l);
			if(nv_isattr(np,NV_LONG))
			{
				ld = *up->ldp;
				if(nv_isattr (np,NV_EXPNOTE))
					format = "%.*Lg\0";
				else
					format = "%.*Lf\0";
				sfsprintf(curbuf,l,format,nv_size(np),ld);
			}
			else
			{
				d = *up->dp;
				if(nv_isattr (np,NV_EXPNOTE))
					format = "%.*g\0";
				else
					format = "%.*f\0";
				sfsprintf(curbuf,l,format,nv_size(np),d);
			}
			return(curbuf);
		}
        	else if(nv_isattr (np,NV_LONG))
			ll = *up->llp;
        	else if(nv_isattr (np,NV_SHORT))
			ll = up->s;
		else if(nv_isattr(np,NV_UNSIGN))
			ll = (unsigned long)*(up->lp);
        	else
			ll = *(up->lp);
		if((numeric=nv_size(np))==10)
		{
			if(nv_isattr(np,NV_UNSIGN))
			{
				char *cp=fmtbuf(36);
				sfsprintf(cp,36,"%I*u",sizeof(ll),ll);
				return(cp);
			}
			numeric = 0;
		}
		return(fmtbasell(ll,numeric, numeric&&numeric!=10));
	}
done:
#if (_AST_VERSION>=20030127L)
	/*
	 * if NV_RAW flag is on, return pointer to binary data 
	 * otherwise, base64 encode the data and return this string
	 */
	if(up->cp && nv_isattr(np,NV_BINARY) && !nv_isattr(np,NV_RAW))
	{
		char *cp;
		int size= nv_size(np), insize=(4*size)/3+size/45+8;
		base64encode(up->cp, size, (void**)0, cp=getbuf(insize), insize, (void**)0); 
		return(cp);
	}
#endif
	return ((char*)up->cp);
}

Sfdouble_t nv_getnum(register Namval_t *np)
{
	register union Value *up;
	register Sfdouble_t r=0;
	register char *str;
#if SHOPT_OPTIMIZE
	if(!nv_local && sh.argaddr)
		nv_optimize(np);
#endif /* SHOPT_OPTIMIZE */
	if(nv_istable(np))
		errormsg(SH_DICT,ERROR_exit(1),e_number,nv_name(np));
     	if(np->nvfun)
	{
		if(!nv_local)
		{
			nv_local=1;
			return(nv_getn(np, np->nvfun));
		}
		nv_local=0;
	}
     	if(nv_isattr (np, NV_INTEGER))
	{
		up= &np->nvalue;
		if(!up->lp)
			r = 0;
		else if(nv_isattr(np, NV_DOUBLE))
		{
			if(nv_isattr(np, NV_LONG))
	                       	r = *up->ldp;
			else
       	                	r = *up->dp;
		}
		else if(nv_isattr(np, NV_UNSIGN))
		{
			if(nv_isattr(np, NV_LONG))
				r = (Sflong_t)*((Sfulong_t*)up->llp);
			else if(nv_isattr(np, NV_SHORT))
				r = (Sflong_t)((unsigned short)up->s);
			else
				r = *((unsigned long*)up->lp);
		}
		else
		{
			if(nv_isattr(np, NV_LONG))
				r = *up->llp;
			else if(nv_isattr(np, NV_SHORT))
				r = up->s;
			else
				r = *up->lp;
		}
	}
	else if((str=nv_getval(np)) && *str!=0)
	{
		while(*str=='0')
			str++;
		r = sh_arith(str);
	}
	return(r);
}
/*
 *   Give <np> the attributes <newatts,> and change its current
 *   value to conform to <newatts>.  The <size> of left and right
 *   justified fields may be given.
 */
void nv_newattr (register Namval_t *np, unsigned newatts, int size)
{
	register char *sp;
	register char *cp = 0;
	register unsigned int n;
	Namarr_t *ap = 0;
	int oldsize,oldatts;

	/* check for restrictions */
	if(sh_isoption(SH_RESTRICTED) && ((sp=nv_name(np))==nv_name(PATHNOD) || sp==nv_name(SHELLNOD) || sp==nv_name(ENVNOD) ))
		errormsg(SH_DICT,ERROR_exit(1),e_restricted,nv_name(np));
	/* handle attributes that do not change data separately */
	n = np->nvflag;
#if SHOPT_BSH
	if(newatts&NV_EXPORT)
		nv_offattr(np,NV_IMPORT);
#endif /* SHOPT_BSH */
#ifdef _ENV_H
	if(((n^newatts)&NV_EXPORT))
	/* record changes to the environment */
	{
		if(n&NV_EXPORT)
			env_delete(sh.env,nv_name(np));
		else
			sh_envput(sh.env,np);
	}
#endif
	if((size==0||(n&NV_INTEGER)) && ((n^newatts)&~NV_NOCHANGE)==0)
	{
		if(size)
			nv_setsize(np,size);
		nv_offattr(np, ~NV_NOFREE);
		nv_onattr(np, newatts);
		return;
	}
	/* for an array, change all the elements */
	if((ap=nv_arrayptr(np)) && ap->nelem>0)
		nv_putsub(np,NIL(char*),ARRAY_SCAN);
	oldsize = nv_size(np);
	oldatts = np->nvflag;
	if(ap) /* add element to prevent array deletion */
		ap->nelem++;
	do
	{
		nv_setsize(np,oldsize);
		np->nvflag = oldatts;
		if (sp = nv_getval(np))
 		{
			if(nv_isattr(np,NV_ZFILL))
				while(*sp=='0') sp++;
			cp = (char*)malloc((n=strlen (sp)) + 1);
			strcpy(cp, sp);
			if(ap)
			{
				Namval_t *mp;
				ap->nelem &= ~ARRAY_SCAN;
				if(mp=nv_opensub(np))
					nv_onattr(mp,NV_NOFREE);
			}
			nv_unset(np);
			if(ap)
				ap->nelem |= ARRAY_SCAN;
			if(size==0 && (newatts&(NV_LJUST|NV_RJUST|NV_ZFILL)))
				size = n;
		}
		else
			nv_unset(np);
		nv_setsize(np,size);
		np->nvflag &= NV_ARRAY;
		np->nvflag |= newatts;
		if (cp)
		{
			nv_putval (np, cp, NV_RDONLY);
			free(cp);
		}
	}
	while(ap && nv_nextsub(np));
	if(ap)
		ap->nelem--;
	return;
}

#ifndef _NEXT_SOURCE
static char *oldgetenv(const char *string)
{
	register char c0,c1;
	register const char *cp, *sp;
	register char **av = environ;
	if(!string || (c0= *string)==0)
		return(0);
	if((c1=*++string)==0)
		c1= '=';
	while(cp = *av++)
	{
		if(cp[0]!=c0 || cp[1]!=c1) 
			continue;
		sp = string;
		while(*sp && *sp++ == *++cp);
		if(*sp==0 && *++cp=='=')
			return((char*)(cp+1));
	}
	return(0);
}

/*
 * This version of getenv the hash storage to access environment values
 */
char *getenv(const char *name)
/*@
	assume name!=0;
@*/ 
{
	register Namval_t *np;
	if(!sh.var_tree)
		return(oldgetenv(name));
	if((np = nv_search(name,sh.var_tree,0)) && nv_isattr(np,NV_EXPORT))
		return(nv_getval(np));
	return(0);
}
#endif /* _NEXT_SOURCE */

#undef putenv
/*
 * This version of putenv uses the hash storage to assign environment values
 */
int putenv(const char *name)
{
	register Namval_t *np;
	if(name)
	{
		np = nv_open(name,sh.var_tree,NV_EXPORT|NV_IDENT|NV_NOARRAY|NV_ASSIGN);
		if(!strchr(name,'='))
			nv_unset(np);
	}
	return(0);
}


/*
 * Override libast setenv()
 */
char* setenviron(const char *name)
{
	register Namval_t *np;
	if(name)
	{
		np = nv_open(name,sh.var_tree,NV_EXPORT|NV_IDENT|NV_NOARRAY|NV_ASSIGN);
		if(strchr(name,'='))
			return(nv_getval(np));
		nv_unset(np);
	}
	return("");
}

/*
 * copy <str1> to <str2> changing lower case to upper case
 * <str2> must be big enough to hold <str1>
 * <str1> and <str2> may point to the same place.
 */

static void ltou(register char const *str1,register char *str2)
/*@
	assume str1!=0 && str2!=0;
	return x satisfying strlen(in str1)==strlen(in str2);
@*/ 
{
	register int c;
	for(; c= *((unsigned char*)str1); str1++,str2++)
	{
		if(islower(c))
			*str2 = toupper(c);
		else
			*str2 = c;
	}
	*str2 = 0;
}

/*
 * normalize <cp> and return pointer to subscript if any
 */
static char *lastdot(register char *cp)
{
	register char *dp=cp, *ep=0;
	register int c;
	while(c= *cp++)
	{
		*dp++ = c;
		if(c=='[')
			ep = cp;
		else if(c=='.')
		{
			if(*cp=='[')
			{
				ep = nv_endsubscript((Namval_t*)0,cp,0);
				c = ep-cp;
				memcpy(dp,cp,c);
				dp = sh_checkid(dp+1,dp+c);
				cp = ep;
			}
			ep = 0;
		}
	}
	*dp = 0;
	return(ep);
}

/*
 * Create a reference node from <np>
 */
void nv_setref(register Namval_t *np)
{
	register Namval_t *nq, *nr;
	register char *ep,*cp;
	Dt_t *hp=sh.var_tree;
	if(nv_isref(np))
		return;
	if(nv_isarray(np))
		errormsg(SH_DICT,ERROR_exit(1),e_badref,nv_name(np));
	if(!(cp=nv_getval(np)))
		errormsg(SH_DICT,ERROR_exit(1),e_noref,nv_name(np));
	ep = lastdot(cp);
	if(nv_isattr(np,NV_PARAM))
	{
		if(sh.st.prevst && !(hp=(Dt_t*)sh.st.prevst->save_tree))
		{
			if(!(hp=dtvnext(sh.var_tree)))
				hp = sh.var_tree;
		}
	}
	nr= nq = nv_open(cp, hp, NV_NOARRAY|NV_VARNAME|NV_NOREF);
	while(nv_isref(nr))
	{
		sh.last_table = nv_table(nr);
		nr = nv_refnode(nr);
	}
	if(nr==np) 
	{
		if(sh.namespace && nv_dict(sh.namespace)==hp)
			errormsg(SH_DICT,ERROR_exit(1),e_selfref,nv_name(np));
		/* bind to earlier scope, or add to global scope */
		if(!(hp=dtvnext(hp)) || (nq=nv_search((char*)np,hp,NV_ADD|HASH_BUCKET))==np)
			errormsg(SH_DICT,ERROR_exit(1),e_selfref,nv_name(np));
	}
	if(ep)
	{
		/* cause subscript evaluation and return result */
		nv_endsubscript(nq,ep,NV_ADD);
		ep = nv_getsub(nq);
	}
	nv_unset(np);
	np->nvalue.np = nq;
	if(ep)
		np->nvenv = strdup(ep);
	np->nvfun = (Namfun_t*)sh.last_table;
	nv_onattr(np,NV_REF|NV_NOFREE);
}

/*
 * get the scope corresponding to <index>
 * whence uses the same values as lseeek()
 */
Shscope_t *sh_getscope(int index, int whence)
{
	register struct sh_scoped *sp, *topmost;
	if(whence==SEEK_CUR)
		sp = &sh.st;
	else
	{
		if ((struct sh_scoped*)sh.topscope != sh.st.self)
			topmost = (struct sh_scoped*)sh.topscope;
		else
			topmost = &(sh.st);
		sp = topmost;
		if(whence==SEEK_SET)
		{
			int n =0;
			while(sp = sp->prevst)
				n++;
			index = n - index;
			sp = topmost;
		}
	}
	if(index < 0)
		return((Shscope_t*)0);
	while(index-- && (sp = sp->prevst));
	return((Shscope_t*)sp);
}

/*
 * make <scoped> the top scope and return previous scope
 */
Shscope_t *sh_setscope(Shscope_t *scope)
{
	Shscope_t *old = (Shscope_t*)sh.st.self;
	*sh.st.self = sh.st;
	sh.st = *((struct sh_scoped*)scope);
	sh.var_tree = scope->var_tree;
	return(old);
}

void nv_unscope(void)
{
	register Dt_t *root = sh.var_tree;
	register Dt_t *dp = dtview(root,(Dt_t*)0);
	sh.var_tree=dp;
	table_unset(root,NV_RDONLY|NV_NOSCOPE,dp);
	dtclose(root);
}

/*
 * The inverse of creating a reference node
 */
void nv_unref(register Namval_t *np)
{
	Namval_t *nq = nv_refnode(np);
	if(!nv_isref(np))
		return;
	nv_offattr(np,NV_NOFREE|NV_REF);
	np->nvalue.cp = strdup(nv_name(nq=nv_refnode(np)));
	np->nvfun = 0;
#if SHOPT_OPTIMIZE
	{
		Namfun_t *fp;
		for(fp=nq->nvfun; fp; fp = fp->next)
		{
			if(fp->disc== &optimize_disc)
			{
				optimize_clear(nq,fp);
				return;
			}
		}
	}
#endif
}

/*
 * These following are for binary compatibility with the old hash library
 * They will be removed someday
 */

#if defined(__IMPORT__) && defined(__EXPORT__)
#   define extern __EXPORT__
#endif

#undef	hashscope

extern Dt_t *hashscope(Dt_t *root)
{
	return(dtvnext(root));
}

#undef	hashfree

extern Dt_t	*hashfree(Dt_t *root)
{
	Dt_t *dp = dtvnext(root);
	dtclose(root);
	return(dp);
}

#undef	hashname

extern char	*hashname(void *obj)
{
	Namval_t *np = (Namval_t*)obj;
	return(np->nvname);
}

#undef	hashlook

extern void *hashlook(Dt_t *root, const char *name, int mode,int size)
{
	NOT_USED(size);
	return((void*)nv_search(name,root,mode));
}

char *nv_name(register Namval_t *np)
{
	register int len;
	register Namval_t *table;
	register Namfun_t *fp;
	char *curbuf, *cp;
	int inbuff;
	if(is_abuiltin(np) || is_afunction(np))
		return(np->nvname);
	if(nv_istable(np))
#if 0
		sh.last_table = (Namval_t*)nv_context(np);
#else
		sh.last_table = nv_create(np,0, NV_LAST,(Namfun_t*)0);
#endif
	else if(!nv_isref(np))
	{
		for(fp= np->nvfun ; fp; fp=fp->next)
		if(fp->disc && fp->disc->namef)
		{
			if(np==sh.last_table)
				sh.last_table = 0;
			return((*fp->disc->namef)(np,fp));
		}
	}
	if(!(table=sh.last_table) || *np->nvname=='.' || table==sh.namespace || np==table)
		return(np->nvname);
	cp = nv_name(table);
	inbuff = (cp==getbuf(0));
	len = strlen(cp) + strlen(np->nvname) + 2;
	curbuf = getbuf(len);
	if(inbuff)
		cp = curbuf;
	sfsprintf(curbuf,len,"%s.%s\0",cp,np->nvname);
	return(curbuf);
}

Namval_t *nv_lastdict(void)
{
	return(sh.last_table);
}

#undef nv_context
/*
 * returns the data context for a builtin
 */
void *nv_context(Namval_t *np)
{
	return((void*)np->nvfun);
}

#define DISABLE /* proto workaround */

int nv_isnull DISABLE (register Namval_t *np)
{
	return(nv_isnull(np));
}

#undef nv_setsize
int nv_setsize(register Namval_t *np, int size)
{
	int oldsize = nv_size(np);
	if(size>=0)
		np->nvsize = size;
	return(oldsize);
}
