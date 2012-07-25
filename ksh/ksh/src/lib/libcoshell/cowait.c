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
 * AT&T Research
 *
 * wait for and return status of job or the next coshell job that completes
 * job==co for non-blocking wait
 */

#include "colib.h"

#include <ctype.h>

/*
 * cat and remove fd {1,2} serialized output
 */

static void
cat(Cojob_t* job, char** path, Sfio_t* op)
{
	Sfio_t*		sp;

	if (sp = sfopen(NiL, *path, "r"))
	{
		sfmove(sp, op, SF_UNBOUND, -1);
		sfclose(sp);
	}
	else
		errormsg(state.lib, ERROR_LIBRARY|2, "%s: cannot open job %d serialized output", *path, job->id);
	remove(*path);
	free(*path);
	*path = 0;
}

/*
 * the number of running+zombie jobs
 * these would count against --jobs or NPROC
 */

int
cojobs(Coshell_t* co)
{
	int	any;
	int	n;

	if (co)
		any = 0;
	else if (!(co = state.coshells))
		return -1;
	else
		any = 1;
	n = 0;
	do
	{
		n += co->outstanding;
	} while (any && (co = co->next));
	return n;
}

/*
 * the number of pending cowait()'s
 */

int
copending(Coshell_t* co)
{
	int	any;
	int	n;

	if (co)
		any = 0;
	else if (!(co = state.coshells))
		return -1;
	else
		any = 1;
	n = 0;
	do
	{
		n += co->outstanding + co->svc_outstanding;
	} while (any && (co = co->next));
	return n;
}

/*
 * the number of completed jobs not cowait()'d for
 * cowait() always reaps the zombies first
 */

int
cozombie(Coshell_t* co)
{
	int	any;
	int	n;

	if (co)
		any = 0;
	else if (!(co = state.coshells))
		return -1;
	else
		any = 1;
	n = 0;
	do
	{
		n += (co->outstanding + co->svc_outstanding) - (co->running + co->svc_running);
	} while (any && (co = co->next));
	return n;
}

Cojob_t*
cowait(register Coshell_t* co, Cojob_t* job, int timeout)
{
	register char*		s;
	register Cojob_t*	cj;
	register Coservice_t*	cs;
	register ssize_t	n;
	char*			b;
	char*			e;
	unsigned long		user;
	unsigned long		sys;
	int			any;
	int			id;
	int			active;
	char			buf[32];

	static unsigned long	serial = 0;

	serial++;
	if (co || job && (co = job->coshell))
		any = 0;
	else if (!(co = state.coshells))
		goto echild;
	else
		any = 1;

	/*
	 * first drain the zombies
	 */

	active = 0;
 zombies:
	do
	{
#if 0
		errormsg(state.lib, 2, "coshell %d zombie wait %lu timeout=%d outstanding=<%d,%d> running=<%d,%d>", co->index, serial, timeout, co->outstanding, co->svc_outstanding, co->running, co->svc_running);
#endif
		if ((co->outstanding + co->svc_outstanding) > (co->running + co->svc_running))
			for (cj = co->jobs; cj; cj = cj->next)
				if (cj->pid == CO_PID_ZOMBIE && (!job || cj == job))
				{
					cj->pid = CO_PID_FREE;
					if (cj->service)
						co->svc_outstanding--;
					else
						co->outstanding--;
#if 0
					errormsg(state.lib, 2, "coshell %d zombie wait %lu timeout=%d outstanding=<%d,%d> running=<%d,%d> reap job %d", co->index, serial, timeout, co->outstanding, co->svc_outstanding, co->running, co->svc_running, cj->id);
#endif
					return cj;
				}
				else if (cj->service && !cj->service->pid)
				{
					cj->pid = CO_PID_ZOMBIE;
					cj->status = 2;
					cj->service = 0;
					co->svc_running--;
				}
		if (co->running > 0)
			active = 1;
		else if (co->svc_running > 0)
		{
			n = 0;
			for (cs = co->service; cs; cs = cs->next)
				if (cs->pid && kill(cs->pid, 0))
				{
					cs->pid = 0;
					close(cs->fd);
					cs->fd = -1;
					n = 1;
				}
			if (n)
				goto zombies;
			active = 1;
		}
	} while (any && (co = co->next));

	/*
	 * reap the active jobs
	 */

	if (!active)
		goto echild;
	if (any)
		co = state.coshells;
	do
	{
		while (co->running > 0 && (timeout < 0 || sfpoll(&co->msgfp, 1, timeout) == 1) && (s = b = sfgetr(co->msgfp, '\n', 1)))
		{
#if 0
			errormsg(state.lib, 2, "coshell %d active wait %lu timeout=%d outstanding=<%d,%d> running=<%d,%d>", co->index, serial, timeout, co->outstanding, co->svc_outstanding, co->running, co->svc_running);
#endif

			/*
			 * read and parse a coshell message packet of the form
			 *
			 *	<type> <id> <args> <newline>
			 *        %c    %d    %s      %c
			 */

			while (isspace(*s))
				s++;
			if (!(n = *s) || n != 'a' && n != 'j' && n != 'x')
				goto invalid;
			while (*++s && !isspace(*s));
			id = strtol(s, &e, 10);
			if (*e && !isspace(*e))
				goto invalid;
			for (s = e; isspace(*s); s++);

			/*
			 * locate id in the job list
			 */

			for (cj = co->jobs; cj; cj = cj->next)
				if (id == cj->id)
					break;
			if ((co->flags | (cj ? cj->flags : 0)) & CO_DEBUG)
				errormsg(state.lib, 2, "coshell %d message \"%c %d %s\"", co->index, n, id, s);
			if (!cj)
			{
				errormsg(state.lib, 2, "coshell %d job id %d not found [%s]", co->index, id, b);
				errno = ESRCH;
				return 0;
			}

			/*
			 * now interpret the message
			 */

			switch (n)
			{

			case 'a':
				/*
				 * coexec() ack
				 */

				if (cj == job)
					return cj;
				break;

			case 'j':
				/*
				 * <s> is the job pid
				 */

				n = cj->pid;
				cj->pid = strtol(s, NiL, 10);
				if (n == CO_PID_WARPED)
					goto nuke;
				break;

			case 'x':
				/*
				 * <s> is the job exit code and user,sys times
				 */

				cj->status = strtol(s, &e, 10);
				user = sys = 0;
				for (;;)
				{
					if (e <= s)
						break;
					for (s = e; isalpha(*s) || isspace(*s); s++);
					user += strelapsed(s, &e, CO_QUANT);
					if (e <= s)
						break;
					for (s = e; isalpha(*s) || isspace(*s); s++);
					sys += strelapsed(s, &e, CO_QUANT);
				}
				cj->user += user;
				cj->sys += sys;
				co->user += user;
				co->sys += sys;
				if (cj->out)
					cat(cj, &cj->out, sfstdout);
				if (cj->err)
					cat(cj, &cj->err, sfstderr);
				if (cj->pid > 0 || cj->service || (co->flags & (CO_INIT|CO_SERVER)))
				{
				nuke:
					if (cj->pid > 0)
					{
						/*
						 * nuke the zombies
						 */

						n = sfsprintf(buf, sizeof(buf), "wait %d\n", cj->pid);
						write(co->cmdfd, buf, n);
					}
					if (cj->service)
						co->svc_running--;
					else
						co->running--;
					if (!job || cj == job)
					{
						cj->pid = CO_PID_FREE;
						if (cj->service)
							co->svc_outstanding--;
						else
							co->outstanding--;
#if 0
						errormsg(state.lib, 2, "coshell %d active wait %lu timeout=%d outstanding=<%d,%d> running=<%d,%d> reap job %d", co->index, serial, timeout, co->outstanding, co->svc_outstanding, co->running, co->svc_running, cj->id);
#endif
						return cj;
					}
					cj->pid = CO_PID_ZOMBIE;
				}
				else
					cj->pid = CO_PID_WARPED;
				break;

			}
		}
	} while (any && (co = co->next));
	return 0;
 echild:
#if 0
	errormsg(state.lib, 2, "coshell wait ECHILD");
#endif
	errno = ECHILD;
	return 0;
 invalid:
	errormsg(state.lib, 2, "coshell %d invalid message \"%-.*s>>>%s<<<\"", co->index, s - b, b, s);
	errno = EINVAL;
	return 0;
}
