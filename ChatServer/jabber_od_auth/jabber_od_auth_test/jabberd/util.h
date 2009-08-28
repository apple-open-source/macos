/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#ifndef INCL_UTIL_H
#define INCL_UTIL_H

JABBERD2_API char *strescape(pool_t p, char *buf, int len); /* Escape <>&'" chars */

/* --------------------------------------------------------- */
/*                                                           */
/* String pools (spool) functions                            */
/*                                                           */
/* --------------------------------------------------------- */
struct spool_node
{
    char *c;
    struct spool_node *next;
};

typedef struct spool_struct
{
    pool_t p;
    int len;
    struct spool_node *last;
    struct spool_node *first;
} *spool;

JABBERD2_API spool spool_new(pool_t p); /* create a string pool */
JABBERD2_API void spooler(spool s, ...); /* append all the char * args to the pool, terminate args with s again */
JABBERD2_API char *spool_print(spool s); /* return a big string */
JABBERD2_API void spool_add(spool s, char *str); /* add a single string to the pool */
JABBERD2_API void spool_escape(spool s, char *raw, int len); /* add and xml escape a single string to the pool */
JABBERD2_API char *spools(pool_t p, ...); /* wrap all the spooler stuff in one function, the happy fun ball! */

#endif    /* INCL_UTIL_H */


