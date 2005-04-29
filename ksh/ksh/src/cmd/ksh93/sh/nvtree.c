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
 * code for tree nodes and name walking
 *
 *   David Korn
 *   AT&T Labs
 *
 */

#include	"defs.h"
#include	"name.h"
#include	"argnod.h"

struct nvdir
{
	Dt_t		*root;
	Namval_t	*hp;
	Namval_t	*table;
	Namval_t	*(*nextnode)(Namval_t*,Dt_t*,Namfun_t*);
	Namfun_t	*fun;
	struct nvdir	*prev;
	int		len;
	int		offset;
	char		data[1];
};

char *nv_getvtree(Namval_t*, Namfun_t *);
static void put_tree(Namval_t*, const char*, int,Namfun_t*);

static Namval_t *create_tree(Namval_t *np,const char *name,int flag,Namfun_t *fp)
{
	NOT_USED(np);
	NOT_USED(name);
	NOT_USED(flag);
	NOT_USED(fp);
	return(0);
}

static const Namdisc_t treedisc =
{
	0,
	put_tree,
	nv_getvtree,
	0,
	0,
	create_tree
};

static char *nextdot(const char *str)
{
	char *cp;
	if(*str=='.')
		str++;
	if(*str=='[')
	{
		cp = nv_endsubscript((Namval_t*)0,(char*)str,0);
		return(*cp=='.'?cp:0);
	}
	else
		return(strchr(str,'.'));
}

static  Namfun_t *nextdisc(Namval_t *np)
{
	register Namfun_t *fp;
	if(nv_isref(np))
		return(0);
        for(fp=np->nvfun;fp;fp=fp->next)
	{
		if(fp && fp->disc && fp->disc->nextf)
			return(fp);
	}
	return(0);
}

void *nv_diropen(const char *name)
{
	char *next,*last;
	int len=strlen(name);
	struct nvdir *save, *dp = new_of(struct nvdir,len);
	Namval_t *np;
	Namfun_t *nfp;
	if(!dp)
		return(0);
	memset((void*)dp, 0, sizeof(*dp));
	last=dp->data;
	if(name[len-1]=='*' || name[len-1]=='@')
		len -= 1;
	memcpy(last,name,len);
	last[len] = 0;
	dp->len = len;
	dp->root = sh.var_tree;
	dp->table = sh.last_table;
	dp->hp = (Namval_t*)dtfirst(dp->root);
	while(next= nextdot(last))
	{
		*next = 0;
		np = nv_search(last,dp->root,0);
		*next = '.';
		if(np && ((nfp=nextdisc(np)) || nv_istable(np)))
		{
			if(!(save = new_of(struct nvdir,0)))
				return(0);
			*save = *dp;
			dp->prev = save;
			if(nv_istable(np))
				dp->root = nv_dict(np);
			else
				dp->root = (Dt_t*)dp;
			dp->offset = last-(char*)name;
			if(dp->offset<len)
				dp->len = len-dp->offset;
			else
				dp->len = 0;
			if(nfp)
			{
				dp->nextnode = nfp->disc->nextf;
				dp->table = np;
				dp->fun = nfp;
				dp->hp = (*dp->nextnode)(np,(Dt_t*)0,nfp);
			}
			else
				dp->nextnode = 0;
		}
		else
			break;
		last = next+1;
	}
	return((void*)dp);
}


static Namval_t *nextnode(struct nvdir *dp)
{
	if(dp->nextnode)
		return((*dp->nextnode)(dp->hp,dp->root,dp->fun));
	return((Namval_t*)dtnext(dp->root,dp->hp));
}

char *nv_dirnext(void *dir)
{
	register struct nvdir *save, *dp = (struct nvdir*)dir;
	register Namval_t *np, *last_table;
	register char *cp;
	Namfun_t *nfp;
	while(1)
	{
		while(np=dp->hp)
		{
			dp->hp = nextnode(dp);
			if(nv_isnull(np))
				continue;
			last_table = sh.last_table;
			sh.last_table = dp->table;
			cp = nv_name(np);
			sh.last_table = last_table;
			if(!dp->len || memcmp(cp+dp->offset,dp->data,dp->len)==0)
			{
				if((nfp=nextdisc(np)) || nv_istable(np))
				{
					Dt_t *root;
					if(nv_istable(np))
						root = nv_dict(np);
					else
						root = (Dt_t*)dp;
					/* check for recursive walk */
					for(save=dp; save;  save=save->prev) 
					{
						if(save->root==root)
							break;
					}
					if(save)
						continue;
					if(!(save = new_of(struct nvdir,0)))
						return(0);
					*save = *dp;
					dp->prev = save;
					dp->root = root;
					dp->len = 0;
					if(np->nvfun)
					{
						dp->nextnode = nfp->disc->nextf;
						dp->table = np;
						dp->fun = nfp;
						dp->hp = (*dp->nextnode)(np,(Dt_t*)0,nfp);
					}
					else
						dp->nextnode = 0;
				}
				return(cp);
			}
		}
		if(!(save=dp->prev))
			break;
#if 0
		sh.last_table = dp->table;
#endif
		*dp = *save;
		free((void*)save);
	}
	return(0);
}

void nv_dirclose(void *dir)
{
	struct nvdir *dp = (struct nvdir*)dir;
	if(dp->prev)
		nv_dirclose((void*)dp->prev);
	free(dir);
}

static void outtype(Namval_t *np, Namfun_t *fp, Sfio_t* out, const char *prefix)
{
	char *type;
	Namval_t *tp = fp->type;
	if(!tp && fp->disc && fp->disc->typef) 
		tp = (*fp->disc->typef)(np,fp);
	for(fp=fp->next;fp;fp=fp->next)
	{
		if(fp->type || (fp->disc && fp->disc->typef &&(*fp->disc->typef)(np,fp)))
		{
			outtype(np,fp,out,prefix);
			break;
		}
	}
	if(prefix && *prefix=='t')
		type = "-T";
	else if(!prefix)
		type = "type";
	if(type)
		sfprintf(out,"%s %s ",type,tp->nvname);
}

/*
 * print the attributes of name value pair give by <np>
 */
void nv_attribute(register Namval_t *np,Sfio_t *out,char *prefix,int noname)
{
	register const Shtable_t *tp;
	register char *cp;
	register unsigned val;
	register unsigned mask;
	register unsigned attr;
	Namfun_t *fp=0; 
	for(fp=np->nvfun;fp;fp=fp->next)
	{
		if(fp->type || (fp->disc && fp->disc->typef &&(*fp->disc->typef)(np,fp)))
			break;
	}
	if(!fp  && !nv_isattr(np,~NV_ARRAY))
	{
		if(!nv_isattr(np,NV_ARRAY)  || nv_aindex(np)>=0)
			return;
	}

	if ((attr=nv_isattr(np,~NV_NOFREE)) || fp)
	{
		if((attr&NV_NOPRINT)==NV_NOPRINT)
			attr &= ~NV_NOPRINT;
		if(!attr && !fp)
			return;
		if(prefix)
			sfputr(out,prefix,' ');
		for(tp = shtab_attributes; *tp->sh_name;tp++)
		{
			val = tp->sh_number;
			mask = val;
			if(fp && (val&NV_INTEGER))
				break;
			/*
			 * the following test is needed to prevent variables
			 * with E attribute from being given the F
			 * attribute as well
			*/
			if(val==(NV_INTEGER|NV_DOUBLE) && (attr&NV_EXPNOTE))
				continue;
			if(val&NV_INTEGER)
				mask |= NV_DOUBLE;
			else if(val&NV_HOST)
				mask = NV_HOST;
			if((attr&mask)==val)
			{
				if(val==NV_ARRAY)
				{
					Namarr_t *ap = nv_arrayptr(np);
					if(array_assoc(ap))
						cp = "associative";
					else
						cp = "indexed";
					if(!prefix)
						sfputr(out,cp,' ');
					else if(*cp=='i')
						continue;
				}
				if(prefix)
				{
					if(*tp->sh_name=='-')
						sfprintf(out,"%.2s ",tp->sh_name);
				}
				else
					sfputr(out,tp->sh_name+2,' ');
		                if ((val&(NV_LJUST|NV_RJUST|NV_ZFILL)) && !(val&NV_INTEGER) && val!=NV_HOST)
					sfprintf(out,"%d ",nv_size(np));
			}
		        if(val==NV_INTEGER && nv_isattr(np,NV_INTEGER))
			{
				if(nv_size(np) != 10)
				{
					if(nv_isattr(np, NV_DOUBLE))
						cp = "precision";
					else
						cp = "base";
					if(!prefix)
						sfputr(out,cp,' ');
					sfprintf(out,"%d ",nv_size(np));
				}
				break;
			}
		}
		if(fp)
			outtype(np,fp,out,prefix);
		if(noname)
			return;
		sfputr(out,nv_name(np),'\n');
	}
}

static void outval(char *name, char *vname, Sfio_t *outfile, int indent)
{
	register Namval_t *np;
        register Namfun_t *fp;
	int isarray, associative=0;
	if(!(np=nv_open(vname,sh.var_tree,NV_VARNAME|NV_NOADD|NV_NOASSIGN|NV_NOARRAY)))
		return;
        for(fp=np->nvfun;fp;fp=fp->next)
	{
		if(fp && fp->disc== &treedisc)
			return;
	}
	if(nv_isnull(np))
		return;
	isarray=0;
	if(nv_isattr(np,NV_ARRAY))
	{
		isarray=1;
		if(array_elem(nv_arrayptr(np))==0)
			isarray=2;
		else
			nv_putsub(np,NIL(char*),ARRAY_SCAN);
		associative= nv_aindex(np)<0;
	}
	if(!outfile)
	{
		_nv_unset(np,NV_RDONLY);
		nv_close(np);
		return;
	}
	sfnputc(outfile,'\t',indent);
	nv_attribute(np,outfile,"typeset",'=');
	nv_outname(outfile,name,-1);
	sfputc(outfile,(isarray==2?'\n':'='));

	if(isarray)
	{
		if(isarray==2)
			return;
		sfwrite(outfile,"(\n",2);
		sfnputc(outfile,'\t',++indent);
	}
	while(1)
	{
		char *fmtq,*ep;
		if(isarray && associative)
		{
			sfprintf(outfile,"[%s]",sh_fmtq(nv_getsub(np)));
			sfputc(outfile,'=');
		}
		if(!(fmtq = sh_fmtq(nv_getval(np))))
			fmtq = "";
		else if(!associative && (ep=strchr(fmtq,'=')))
		{
			char *qp = strchr(fmtq,'\'');
			if(!qp || qp>ep)
			{
				sfwrite(outfile,fmtq,ep-fmtq);
				sfputc(outfile,'\\');
				fmtq = ep;
			}
		}
		if(*name=='[' && !isarray)
			sfprintf(outfile,"(%s)\n",fmtq);
		else
			sfputr(outfile,fmtq,'\n');
		if(!nv_nextsub(np))
			break;
		sfnputc(outfile,'\t',indent);
	}
	if(isarray)
	{
		sfnputc(outfile,'\t',--indent);
		sfwrite(outfile,")\n",2);
	}
}

/*
 * format initialization list given a list of assignments <argp>
 */
static char **genvalue(char **argv, register Sfio_t *outfile, const char *prefix, int n, int indent)
{
	register char *cp,*nextcp,*arg;
	register int m;
	if(n==0)
		m = strlen(prefix);
	else
		m = nextdot(prefix)-prefix;
	m++;
	if(outfile)
	{
		sfwrite(outfile,"(\n",2);
		indent++;
	}
	for(; arg= *argv; argv++)
	{
		cp = arg + n;
		if(n==0 && cp[m-1]!='.')
			continue;
		if(n && cp[m-1]==0)
			break;
		if(n==0 || strncmp(arg,prefix-n,m+n)==0)
		{
			cp +=m;
			if(nextcp=nextdot(cp))
			{
				if(outfile)
				{
					sfnputc(outfile,'\t',indent);
					nv_outname(outfile,cp,nextcp-cp);
					sfputc(outfile,'=');
				}
				argv = genvalue(argv,outfile,cp,n+m ,indent);
				if(outfile)
					sfputc(outfile,'\n');
				if(*argv)
					continue;
				break;
			}
			else
				outval(cp,arg,outfile,indent);
		}
		else
			break;
	}
	if(outfile)
	{
		cp = (char*)prefix;
		cp[m-1] = 0;
		outval(".",cp-n,outfile,indent);
		cp[m-1] = '.';
		sfnputc(outfile,'\t',indent-1);
		sfputc(outfile,')');
	}
	return(--argv);
}

/*
 * walk the virtual tree and print or delete name-value pairs
 */
static char *walk_tree(register Namval_t *np, int dlete)
{
	static Sfio_t *out;
	Sfio_t *outfile;
	int savtop = staktell();
	char *savptr = stakfreeze(0);
	register struct argnod *ap; 
	struct argnod *arglist=0;
	char *name,*cp, **argv;
	char *subscript=0;
	void *dir;
	int n=0;
	stakputs(nv_name(np));
#if SHOPT_COMPOUND_ARRAY
	if(subscript = nv_getsub(np))
	{
		stakputc('[');
		stakputs(subscript);
		stakputc(']');
		stakputc('.');
	}
#endif /* SHOPT_COMPOUND_ARRAY */
	name = stakfreeze(1);
	dir = nv_diropen(name);
	while(cp = nv_dirnext(dir))
	{
		stakseek(ARGVAL);
		stakputs(cp);
		ap = (struct argnod*)stakfreeze(1);
		ap->argflag = ARG_RAW;
		ap->argchn.ap = arglist; 
		n++;
		arglist = ap;
	}
	argv = (char**)stakalloc((n+1)*sizeof(char*));
	argv += n;
	*argv = 0;
	for(; ap; ap=ap->argchn.ap)
		*--argv = ap->argval;
	strsort(argv,n,strcmp);
	nv_dirclose(dir);
	if(dlete)
		outfile = 0;
	else if(!(outfile=out))
		outfile = out =  sfnew((Sfio_t*)0,(char*)0,-1,-1,SF_WRITE|SF_STRING);
	else
		sfseek(outfile,0L,SEEK_SET);
	genvalue(argv,outfile,name,0,0);
	stakset(savptr,savtop);
	if(!outfile)
		return((char*)0);
	sfputc(out,0);
	return((char*)out->_data);
}

/*
 * get discipline for compound initializations
 */
char *nv_getvtree(register Namval_t *np, Namfun_t *fp)
{
	NOT_USED(fp);
	if(nv_isattr(np,NV_BINARY) &&  nv_isattr(np,NV_RAW))
		return(nv_getv(np,fp));
	return(walk_tree(np,0));
}

/*
 * put discipline for compound initializations
 */
static void put_tree(register Namval_t *np, const char *val, int flags,Namfun_t *fp)
{
	struct Namarray *ap;
	int nleft = 0;
	if(!nv_isattr(np,NV_INTEGER))
		walk_tree(np,1);
	nv_putv(np, val, flags,fp);
	if(nv_isattr(np,NV_INTEGER))
		return;
	if(ap= nv_arrayptr(np))
		nleft = array_elem(ap);
	if(nleft==0)
	{
		fp = nv_stack(np,fp);
		if(fp = nv_stack(np,NIL(Namfun_t*)))
		{
			free((void*)fp);
		}
	}
}

/*
 * Insert discipline to cause $x to print current tree
 */
void nv_setvtree(register Namval_t *np)
{
	register Namfun_t *nfp = newof(NIL(void*),Namfun_t,1,0);
	nfp->disc = &treedisc;
	nv_stack(np, nfp);
}

/*
 * the following three functions are for creating types
 */

int nv_settype(Namval_t* np, Namval_t *tp, int flags)
{
	int isnull = nv_isnull(np);
	char *val=0;
	if(isnull)
		flags &= ~NV_APPEND;
	else
	{
		val = strdup(nv_getval(np));
		if(!(flags&NV_APPEND))
			_nv_unset(np, NV_RDONLY);
	}
	if(!nv_clone(tp,np,flags|NV_NOFREE))
		return(0);
	if(val)
	{
		nv_putval(np,val,NV_RDONLY);
		free((void*)val);
	}
	return(0);
}

