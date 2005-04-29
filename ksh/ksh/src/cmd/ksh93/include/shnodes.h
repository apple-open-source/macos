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
#ifndef _SHNODES_H
#define _SHNODES_H	1
/*
 *	UNIX shell
 *	Written by David Korn
 *
 */


#include	<ast.h>
#include	"argnod.h"

/* command tree for tretyp */
#define FINT		(02<<COMBITS)		/* non-interruptable */
#define FAMP		(04<<COMBITS)		/* background */
#define FPIN		(010<<COMBITS)		/* input is a pipe */
#define FPOU		(040<<COMBITS)		/* output is a pipe */
#define FPCL		(0100<<COMBITS)		/* close the pipe */
#define FCOOP		(0200<<COMBITS)		/* cooperating process */
#define FPOSIX		(02<<COMBITS)		/* posix semantics function */
#define FLINENO		(04<<COMBITS)		/* for/case has line number */

#define TNEGATE		(01<<COMBITS)		/* ! inside [[...]] */
#define TBINARY		(02<<COMBITS)		/* binary operator in [[...]] */
#define TUNARY		(04<<COMBITS)		/* unary operator in [[...]] */
#define TTEST		(010<<COMBITS)
#define TPAREN		(TBINARY|TUNARY)
#define TSHIFT		(COMBITS+4)
#define TNSPACE		(TFUN|COMSCAN)

#define TCOM	0
#define TPAR	1
#define TFIL	2
#define TLST	3
#define TIF	4
#define TWH	5
#define TUN	(TWH|COMSCAN)
#define TTST	6
#define TSW	7
#define TAND	8
#define TORF	9
#define TFORK	10
#define TFOR	11
#define TSELECT	(TFOR|COMSCAN)
#define TARITH	12
#define	TTIME	13
#define TSETIO	14
#define TFUN	15

/* this node is a proforma for those that follow */

struct trenod
{
	int		tretyp;
	struct ionod	*treio;
};


struct forknod
{
	int		forktyp;
	struct ionod	*forkio;
	union anynode	*forktre;
	int		forkline;
};


struct ifnod
{
	int		iftyp;
	union anynode	*iftre;
	union anynode	*thtre;
	union anynode	*eltre;
};

struct whnod
{
	int		whtyp;
	union anynode	*whtre;
	union anynode	*dotre;
	struct arithnod	*whinc;
};

struct fornod
{
	int		fortyp;
	char	 	*fornam;
	union anynode	*fortre;
	struct comnod	*forlst;
	int		forline;
};

struct swnod
{
	int		swtyp;
	struct argnod	*swarg;
	struct regnod	*swlst;
	struct ionod	*swio;
	int		swline;
};

struct regnod
{
	struct argnod	*regptr;
	union anynode	*regcom;
	struct regnod	*regnxt;
	char		regflag;
};

struct parnod
{
	int		partyp;
	union anynode	*partre;
};

struct lstnod
{
	int		lsttyp;
	union anynode	*lstlef;
	union anynode	*lstrit;
};

/* tst is same as lst, but with extra field for line number */
struct tstnod
{
	struct lstnod	tstlst;
	int		tstline;	
};

struct functnod
{
	int		functtyp;
	char		*functnam;
	union anynode	*functtre;
	int		functline;
	off_t		functloc;
	struct slnod	*functstak;
	struct comnod	*functargs;
};

struct arithnod
{
	int		artyp;
	int		arline;
	struct argnod	*arexpr;
	void		*arcomp;
};


/* types of ionodes stored in iofile  */
#define IOUFD	0x3f		/* file descriptor number mask */
#define IOPUT	0x40		/* > redirection operator */
#define IOAPP	0x80		/* >> redirection operator */
#define IODOC	0x100		/* << redirection operator */
#define IOMOV	0x200		/* <& or >& operators */
#define IOCLOB	0x400		/* noclobber bit */
#define IORDW	0x800		/* <> redirection operator */
#define IORAW	0x1000		/* no expansion needed for filename */
#define IOSTRG	0x2000		/* here-document stored as incore string */
#define IOSTRIP 0x4000		/* strip leading tabs for here-document */
#define IOQUOTE	0x8000		/* here-document delimiter was quoted */

union anynode
{
	struct argnod	arg;
	struct ionod	io;
	struct whnod	wh;
	struct swnod	sw;
	struct ifnod	if_;
	struct dolnod	dol;
	struct comnod	com;
	struct trenod	tre;
	struct forknod	fork;
	struct fornod	for_;
	struct regnod	reg;
	struct parnod	par;
	struct lstnod	lst;
	struct tstnod	tst;
	struct functnod	funct;
	struct arithnod	ar;
};

extern void			sh_freeup(void);
extern void			sh_funstaks(struct slnod*,int);
extern Sfio_t 			*sh_subshell(union anynode*, int, int);
extern int			sh_exec(const union anynode*,int);
#if defined(__EXPORT__) && defined(_BLD_DLL) && defined(_BLD_shell) 
   __EXPORT__
#endif
extern int			sh_tdump(Sfio_t*, const union anynode*);
extern union anynode		*sh_dolparen(void);
extern union anynode		*sh_trestore(Sfio_t*);
#if SHOPT_KIA
    extern int 			kiaclose(void);
    extern unsigned long 	kiaentity(const char*,int,int,int,int,unsigned long,int,int,const char*);
#endif /* SHOPT_KIA */

#endif /* _SHNODES_H */
