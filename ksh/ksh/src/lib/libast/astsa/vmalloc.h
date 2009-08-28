/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2007 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#pragma prototyped
/*
 * standalone mini vmalloc interface
 */

#ifndef _VMALLOC_H
#define _VMALLOC_H

#define vmalloc(v,n)		_vm_resize(v,(void*)0,n)
#define vmalign(v,n,a)		_vm_resize(v,(void*)0,n)
#define vmclose(v)		_vm_close(v)
#define vmfree(v,p)
#define vmnewof(v,o,t,n,x)	(t*)_vm_resize(v,(void*)o,sizeof(t)*(n)+(x))
#define vmopen(a,b,c)		_vm_open()

#define VM_CHUNK		(32*1024)
#define VM_ALIGN		16

typedef struct Vmchunk_s
{
	struct Vmchunk_s*	next;
	char			align[VM_ALIGN - sizeof(struct Vmchunk_s*)];
	char			data[VM_CHUNK - VM_ALIGN];
} Vmchunk_t;

typedef struct Vmalloc_s
{
	Vmchunk_t		base;		
	Vmchunk_t*		current;
	char*			data;
	long			size;
	long			last;
} Vmalloc_t;

extern Vmalloc_t*		Vmregion;

extern int			_vm_close(Vmalloc_t*);
extern Vmalloc_t*		_vm_open(void);
extern void*			_vm_resize(Vmalloc_t*, void*, unsigned long);

#endif
