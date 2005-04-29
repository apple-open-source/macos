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
 *	File name expansion
 *
 *	David Korn
 *	AT&T Labs
 *
 */

#if KSHELL
#   include	"defs.h"
#   include	"variables.h"
#   include	"test.h"
#else
#   include	<ast.h>
#   include	<setjmp.h>
#endif /* KSHELL */
#include	<glob.h>
#include	<ls.h>
#include	<stak.h>
#include	<ast_dir.h>
#include	"io.h"
#include	"path.h"

#if !SHOPT_BRACEPAT
#   define SHOPT_BRACEPAT	0
#endif

#if KSHELL
#   define argbegin	argnxt.cp
    static	const char	*sufstr;
    static	int		suflen;
    static int scantree(Dt_t*,const char*, struct argnod**);
#else
#   define sh_sigcheck()	(0)
#   define sh_access		access
#   define suflen		0
#endif /* KSHELL */


/*
 * This routine builds a list of files that match a given pathname
 * Uses external routine strgrpmatch() to match each component
 * A leading . must match explicitly
 *
 */

#ifndef GLOB_AUGMENTED
#   define GLOB_AUGMENTED	0
#endif

#define GLOB_RESCAN 1
#define globptr()	((struct glob*)membase)

static struct glob	 *membase;

#if GLOB_VERSION >= 20010916L
static char *nextdir(glob_t *gp, char *dir)
{
	Pathcomp_t *pp = (Pathcomp_t*)gp->gl_handle;
	if(!dir)
		pp = path_get("");
	else
		pp = pp->next;
	gp->gl_handle = (void*)pp;
	if(pp)
		return(pp->name);
	return(0);
}
#endif

int path_expand(const char *pattern, struct argnod **arghead)
{
	glob_t gdata;
	register struct argnod *ap;
	register glob_t *gp= &gdata;
	register int flags,extra=0;
#if SHOPT_BASH
	register int off;
	register char *sp, *cp, *cp2;
#endif
	memset(gp,0,sizeof(gdata));
	flags = GLOB_AUGMENTED|GLOB_NOCHECK|GLOB_NOSORT|GLOB_STACK|GLOB_LIST|GLOB_DISC;
	if(sh_isoption(SH_MARKDIRS))
		flags |= GLOB_MARK;
	if(sh_isoption(SH_GLOBSTARS))
		flags |= GLOB_STARSTAR;
#if SHOPT_BASH
#if 0
	if(sh_isoption(SH_BASH) && !sh_isoption(SH_EXTGLOB))
		flags &= ~GLOB_AUGMENTED;
#endif
	if(sh_isoption(SH_NULLGLOB))
		flags &= ~GLOB_NOCHECK;
	if(sh_isoption(SH_NOCASEGLOB))
		flags |= GLOB_ICASE;
#endif
	if(sh_isstate(SH_COMPLETE))
	{
#if KSHELL
		extra += scantree(sh.alias_tree,pattern,arghead); 
		extra += scantree(sh.fun_tree,pattern,arghead); 
#   if GLOB_VERSION >= 20010916L
		gp->gl_nextdir = nextdir;
#   endif
#endif /* KSHELL */
		flags |= GLOB_COMPLETE;
	}
#if SHOPT_BASH
	if(off = staktell())
		sp = stakfreeze(0);
	if(sh_isoption(SH_BASH))
	{
		/*
		 * For bash, FIGNORE is a colon separated list of suffixes to
		 * ignore when doing filename/command completion.
		 * GLOBIGNORE is similar to ksh FIGNORE, but colon separated
		 * instead of being an augmented shell pattern.
		 * Generate shell patterns out of those here.
		 */
		if(sh_isstate(SH_FCOMPLETE))
			cp=nv_getval(nv_scoped(FIGNORENOD));
		else
		{
			static Namval_t *GLOBIGNORENOD;
			if(!GLOBIGNORENOD)
				GLOBIGNORENOD = nv_open("GLOBIGNORE",sh.var_tree,0);
			cp=nv_getval(nv_scoped(GLOBIGNORENOD));
		}
		if(cp)
		{
			flags |= GLOB_AUGMENTED;
			stakputs("@(");
			if(!sh_isstate(SH_FCOMPLETE))
			{
				stakputs(cp);
				for(cp=stakptr(off); *cp; cp++)
					if(*cp == ':')
						*cp='|';
			}
			else
			{
				cp2 = strtok(cp, ":");
				if(!cp2)
					cp2=cp;
				do
				{
					stakputc('*');
					stakputs(cp2);
					if(cp2 = strtok(NULL, ":"))
					{
						*(cp2-1)=':';
						stakputc('|');
					}
				} while(cp2);
			}
			stakputc(')');
			gp->gl_fignore = stakfreeze(1);
		}
		else if(!sh_isstate(SH_FCOMPLETE) && sh_isoption(SH_DOTGLOB))
			gp->gl_fignore = "";
	}
	else
#endif
	gp->gl_fignore = nv_getval(nv_scoped(FIGNORENOD));
	if(suflen)
		gp->gl_suffix = sufstr;
	gp->gl_intr = &sh.trapnote; 
	suflen = 0;
	glob(pattern, flags, 0, gp);
#if SHOPT_BASH
	if(off)
		stakset(sp,off);
	else
		stakseek(0);
#endif
	sh_sigcheck();
	for(ap= (struct argnod*)gp->gl_list; ap; ap = ap->argnxt.ap)
	{
		ap->argchn.ap = ap->argnxt.ap;
		if(!ap->argnxt.ap)
			ap->argchn.ap = *arghead;
	}
	*arghead = (struct argnod*)gp->gl_list;
	if(gp->gl_list)
		*arghead = (struct argnod*)gp->gl_list;
	return(gp->gl_pathc+extra);
}

#if KSHELL

/*
 * scan tree and add each name that matches the given pattern
 */
static int scantree(Dt_t *tree, const char *pattern, struct argnod **arghead)
{
	register Namval_t *np;
	register struct argnod *ap;
	register int nmatch=0;
	register char *cp;
	np = (Namval_t*)dtfirst(tree);
	for(;np && !nv_isnull(np);(np = (Namval_t*)dtnext(tree,np)))
	{
		if(strmatch(cp=nv_name(np),pattern))
		{
			ap = (struct argnod*)stakseek(ARGVAL);
			stakputs(cp);
			ap = (struct argnod*)stakfreeze(1);
			ap->argbegin = NIL(char*);
			ap->argchn.ap = *arghead;
			ap->argflag = ARG_RAW|ARG_MAKE;
			*arghead = ap;
			nmatch++;
		}
	}
	return(nmatch);
}

/*
 * file name completion
 * generate the list of files found by adding an suffix to end of name
 * The number of matches is returned
 */

int path_complete(const char *name,register const char *suffix, struct argnod **arghead)
{
	sufstr = suffix;
	suflen = strlen(suffix);
	return(path_expand(name,arghead));
}

#endif

#if SHOPT_BRACEPAT
int path_generate(struct argnod *todo, struct argnod **arghead)
/*@
	assume todo!=0;
	return count satisfying count>=1;
@*/
{
	register char *cp;
	register int brace;
	register struct argnod *ap;
	struct argnod *top = 0;
	struct argnod *apin;
	char *pat, *rescan, *bracep;
	char *sp;
	char comma;
	int count = 0;
	todo->argchn.ap = 0;
again:
	apin = ap = todo;
	todo = ap->argchn.ap;
	cp = ap->argval;
	comma = brace = 0;
	/* first search for {...,...} */
	while(1) switch(*cp++)
	{
		case '{':
			if(brace++==0)
				pat = cp;
			break;
		case '}':
			if(--brace>0)
				break;
			if(brace==0 && comma && *cp!='(')
				goto endloop1;
			comma = brace = 0;
			break;
		case ',':
			if(brace==1)
				comma = 1;
			break;
		case '\\':
			cp++;
			break;
		case 0:
			/* insert on stack */
			ap->argchn.ap = top;
			top = ap;
			if(todo)
				goto again;
			for(; ap; ap=apin)
			{
				apin = ap->argchn.ap;
				if((brace = path_expand(ap->argval,arghead)))
					count += brace;
				else
				{
					ap->argchn.ap = *arghead;
					*arghead = ap;
					count++;
				}
				(*arghead)->argflag |= ARG_MAKE;
			}
			return(count);
	}
endloop1:
	rescan = cp;
	bracep = cp = pat-1;
	*cp = 0;
	while(1)
	{
		brace = 0;
		/* generate each pattern and put on the todo list */
		while(1) switch(*++cp)
		{
			case '\\':
				cp++;
				break;
			case '{':
				brace++;
				break;
			case ',':
				if(brace==0)
					goto endloop2;
				break;
			case '}':
				if(--brace<0)
					goto endloop2;
		}
	endloop2:
		/* check for match of '{' */
		brace = *cp;
		*cp = 0;
		if(brace == '}')
		{
			apin->argchn.ap = todo;
			todo = apin;
			sp = strcopy(bracep,pat);
			sp = strcopy(sp,rescan);
			break;
		}
		ap = (struct argnod*)stakseek(ARGVAL);
		ap->argflag = ARG_RAW;
		ap->argchn.ap = todo;
		stakputs(apin->argval);
		stakputs(pat);
		stakputs(rescan);
		todo = ap = (struct argnod*)stakfreeze(1);
		pat = cp+1;
	}
	goto again;
}
#endif /* SHOPT_BRACEPAT */
