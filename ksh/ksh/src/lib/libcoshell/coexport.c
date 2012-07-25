/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1990-2011 AT&T Intellectual Property          *
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
*                                                                      *
***********************************************************************/
#pragma prototyped
/*
 * Glenn Fowler
 * at&t Research
 *
 * coshell export var set/unset
 */

#include "colib.h"

/*
 * set or unset coshell export variable
 */

int
coexport(Coshell_t* co, const char* name, const char* value)
{
	Coexport_t*	ex;
	char*		v;

	if (!co->export)
	{
		if (!(co->exdisc = vmnewof(co->vm, 0, Dtdisc_t, 1, 0)))
			return -1;
		co->exdisc->link = offsetof(Coexport_t, link);
		co->exdisc->key = offsetof(Coexport_t, name);
		co->exdisc->size = 0;
		if (!(co->export = dtnew(co->vm, co->exdisc, Dthash)))
		{
			vmfree(co->vm, co->exdisc);
			return -1;
		}
	}
	if (!(ex = (Coexport_t*)dtmatch(co->export, name)))
	{
		if (!value)
			return 0;
		if (!(ex = vmnewof(co->vm, 0, Coexport_t, 1, strlen(name))))
			return -1;
		strcpy(ex->name, name);
		dtinsert(co->export, ex);
	}
	if (ex->value)
	{
		vmfree(co->vm, ex->value);
		ex->value = 0;
	}
	if (value)
	{
		if (!(v = vmstrdup(co->vm, value)))
			return -1;
		ex->value = v;
	}
	else
	{
		dtdelete(co->export, ex);
		vmfree(co->vm, ex);
	}
	co->init.sync = 1;
	return 0;
}
