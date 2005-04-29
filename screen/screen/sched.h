/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 * $Id: sched.h,v 1.1.1.1 1993/06/16 23:51:13 jnweiger Exp $ FAU
 */

struct event
{
  struct event *next;
  void (*handler) __P((struct event *, char *));
  char *data;
  int fd;
  int type;
  int pri;
  struct timeval timeout;
  int queued;		/* in evs queue */
  int active;		/* in fdset */
  int *condpos;		/* only active if condpos - condneg > 0 */
  int *condneg;
};

#define EV_TIMEOUT	0
#define EV_READ		1
#define EV_WRITE	2
#define EV_ALWAYS	3
