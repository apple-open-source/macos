/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
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
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#if defined(_UWIN) && defined(_BLD_ast)

void _STUB_vmexit(){}

#else

#include	"vmhdr.h"

/*
**	Any required functions for process exiting.
**	Written by Kiem-Phong Vo, kpv@research.att.com (05/25/93).
*/
#if _PACKAGE_ast || _lib_atexit

void _STUB_vmexit(){}

#else

#if _lib_onexit

#if __STD_C
int atexit(void (*exitf)(void))
#else
int atexit(exitf)
void	(*exitf)();
#endif
{
	return onexit(exitf);
}

#else /*!_lib_onexit*/

typedef struct _exit_s
{	struct _exit_s*	next;
	void(*		exitf)_ARG_((void));
} Exit_t;
static Exit_t*	Exit;

#if __STD_C
atexit(void (*exitf)(void))
#else
atexit(exitf)
void	(*exitf)();
#endif
{	Exit_t*	e;

	if(!(e = (Exit_t*)malloc(sizeof(Exit_t))) )
		return -1;
	e->exitf = exitf;
	e->next = Exit;
	Exit = e;
	return 0;
}

#if __STD_C
void exit(int type)
#else
void exit(type)
int	type;
#endif
{
	Exit_t*	e;

	for(e = Exit; e; e = e->next)
		(*e->exitf)();

#if _exit_cleanup
	_cleanup();
#endif

	_exit(type);
	return type;
}

#endif	/* _lib_onexit || _lib_on_exit */

#endif /*!PACKAGE_ast*/

#endif
