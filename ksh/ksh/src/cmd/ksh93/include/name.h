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
#ifndef _NV_PRIVATE
/*
 * This is the implementation header file for name-value pairs
 */

#define _NV_PRIVATE	\
	Namfun_t	*nvfun;		/* pointer to trap functions */ \
	union Value	nvalue; 	/* value field */ \
	char		*nvenv;		/* pointer to environment name */ 

#include	<ast.h>
#include	<cdt.h>
#include	"shtable.h"

/* Nodes can have all kinds of values */
union Value
{
	const char	*cp;
	int		*ip;
	char		c;
	int		i;
	unsigned	u;
	long		*lp;
	Sflong_t	*llp;	/* for long long arithmetic */
	short		s;
	double		*dp;	/* for floating point arithmetic */
	Sfdouble_t	*ldp;	/* for long floating point arithmetic */
	struct Namarray	*array;	/* for array node */
	struct Namval	*np;	/* for Namval_t node */
	union Value	*up;	/* for indirect node */
	struct Ufunction *rp;	/* shell user defined functions */
	struct Namfun	*funp;	/* discipline pointer */
	int (*bfp)(int,char*[],void*);/* builtin entry point function pointer */
};

#include	"nval.h"

/* used for arrays */

#define ARRAY_MAX 	(1L<<ARRAY_BITS) /* maximum number of elements in an array */
#define ARRAY_MASK	(ARRAY_MAX-1)	/* For index values */

#define ARRAY_INCR	32	/* number of elements to grow when array 
				   bound exceeded.  Must be a power of 2 */

/* These flags are used as options to array_get() */
#define ARRAY_ASSIGN	0
#define ARRAY_LOOKUP	1
#define ARRAY_DELETE	2


/* This describes a user shell function node */
struct Ufunction
{
	int	*ptree;			/* address of parse tree */
	int	lineno;			/* line number of function start */
	off_t	hoffset;		/* offset into source or history file */
	Namval_t *nspace;		/* pointer to name space */
	char	*fname;			/* file name where function defined */
};

/* attributes of Namval_t items */

/* The following attributes are for internal use */
#define NV_NOCHANGE	(NV_EXPORT|NV_IMPORT|NV_RDONLY|NV_TAGGED|NV_NOFREE)
#define NV_ATTRIBUTES	(~(NV_NOSCOPE|NV_ARRAY|NV_IDENT|NV_ASSIGN|NV_REF|NV_VARNAME))
#define NV_PARAM	NV_NODISC	/* expansion use positional params */

/* This following are for use with nodes which are not name-values */
#define NV_TYPE		0x1000000
#define NV_FUNCTION	(NV_RJUST|NV_FUNCT)	/* value is shell function */
#define NV_FPOSIX	NV_LJUST		/* posix function semantics */
#define NV_FTMP		NV_ZFILL		/* function source in tmpfile */

#define NV_NOPRINT	(NV_LTOU|NV_UTOL)	/* do not print */
#define NV_NOALIAS	(NV_NOPRINT|NV_IMPORT)
#define NV_NOEXPAND	NV_RJUST		/* do not expand alias */
#define NV_BLTIN	(NV_NOPRINT|NV_EXPORT)
#define BLT_ENV		(NV_RDONLY)		/* non-stoppable,
						 * can modify enviornment */
#define BLT_SPC		(NV_LJUST)		/* special built-ins */
#define BLT_EXIT	(NV_RJUST)		/* exit value can be > 255 */
#define BLT_DCL		(NV_TAGGED)		/* declaration command */
#define nv_isref(n)	(nv_isattr((n),NV_REF)==NV_REF)
#define nv_istable(n)	(nv_isattr((n),NV_TABLE|NV_LJUST|NV_RJUST)==NV_TABLE)
#define is_abuiltin(n)	(nv_isattr(n,NV_BLTIN)==NV_BLTIN)
#define is_afunction(n)	(nv_isattr(n,NV_FUNCTION)==NV_FUNCTION)
#define	nv_funtree(n)	((n)->nvalue.rp->ptree)
#define	funptr(n)	((n)->nvalue.bfp)

#define NV_SUBQUOTE	(NV_ADD<<1)	/* used with nv_endsubscript */

/* NAMNOD MACROS */
/* ... for attributes */

#define nv_setattr(n,f)	((n)->nvflag = (f))
#define nv_context(n)	((void*)(n)->nvfun)		/* for builtins */
#define nv_table(n)	((Namval_t*)((n)->nvfun))	/* for references */
#define nv_refnode(n)	((Namval_t*)((n)->nvalue.np))	/* for references */
#if SHOPT_OO
#   define nv_class(np)		(nv_isattr(np,NV_REF|NV_IMPORT)?0:(Namval_t*)((np)->nvenv))
#endif /* SHOPT_OO */

/* ... etc */

#define nv_setsize(n,s)	((n)->nvsize = (s))
#undef nv_size
#define nv_size(np)	((np)->nvsize)
#define nv_isnull(np)	(!(np)->nvalue.cp && !(np)->nvfun && !nv_isattr(np,NV_SHORT))

/* ...	for arrays */

#define array_elem(ap)	((ap)->nelem&ARRAY_MASK)
#define array_assoc(ap)	((ap)->fun)

extern int		array_maxindex(Namval_t*);
extern char 		*nv_endsubscript(Namval_t*, char*, int);
extern Namfun_t 	*nv_cover(Namval_t*);
struct argnod;		/* struct not declared yet */
extern Namarr_t 	*nv_arrayptr(Namval_t*);
extern int		nv_setnotify(Namval_t*,char **);
extern int		nv_unsetnotify(Namval_t*,char **);
extern void		nv_setlist(struct argnod*, int);
extern void 		nv_optimize(Namval_t*);
extern void		nv_outname(Sfio_t*,char*, int);
extern void 		nv_scope(struct argnod*);
extern void 		nv_unref(Namval_t*);
extern void		_nv_unset(Namval_t*,int);
extern int		nv_clone(Namval_t*, Namval_t*, int);
extern void		*nv_diropen(const char*);
extern char		*nv_dirnext(void*);
extern void		nv_dirclose(void*); 
extern char		*nv_getvtree(Namval_t*, Namfun_t*);
extern void		nv_attribute(Namval_t*, Sfio_t*, char*, int);
extern Namval_t		*nv_bfsearch(const char*, Dt_t*, Namval_t**, char**);
extern Namval_t		*nv_mkclone(Namval_t*);
extern char		*nv_getbuf(size_t);
extern Namval_t		*nv_mount(Namval_t*, const char *name, Dt_t*);

extern const Namdisc_t	RESTRICTED_disc;
extern char		nv_local;
extern Dtdisc_t		_Nvdisc;
extern const char	e_subscript[];
extern const char	e_nullset[];
extern const char	e_notset[];
extern const char	e_noparent[];
extern const char	e_readonly[];
extern const char	e_badfield[];
extern const char	e_restricted[];
extern const char	e_ident[];
extern const char	e_varname[];
extern const char	e_funname[];
extern const char	e_noalias[];
extern const char	e_aliname[];
extern const char	e_badexport[];
extern const char	e_badref[];
extern const char	e_noref[];
extern const char	e_selfref[];
extern const char	e_envmarker[];
extern const char	e_badlocale[];
extern const char	e_loop[];
#endif /* _NV_PRIVATE */
