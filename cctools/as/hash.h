/* hash.h - for hash.c
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

struct hash_entry
{
  char *      hash_string;	/* points to where the symbol string is */
				/* NULL means slot is not used */
				/* DELETED means slot was deleted */
  char *      hash_value;	/* user's datum, associated with symbol */
};


#define HASH_STATLENGTH	(6)
struct hash_control
{
  struct hash_entry * hash_where; /* address of hash table */
  int         hash_sizelog;	/* Log of ( hash_mask + 1 ) */
  int         hash_mask;	/* masks a hash into index into table */
  int         hash_full;	/* when hash_stat[STAT_USED] exceeds this, */
				/* grow table */
  struct hash_entry * hash_wall; /* point just after last (usable) entry */
				/* here we have some statistics */
  int hash_stat[HASH_STATLENGTH]; /* lies & statistics */
				/* we need STAT_USED & STAT_SIZE */
};


/*						returns		  */
extern struct hash_control *hash_new(	/* [control block]	  */
    void);			
extern char *hash_insert(		/* error string           */
    struct hash_control *handle,
    char *string,
    char *value);
extern char *hash_apply(		/* 0 means OK             */
    struct hash_control *handle,
    char *(*function)(char *hash_string, char *hash_value));
extern char *hash_find(			/* value                  */
    struct hash_control *handle,
    char *string);
extern char *hash_jam(			/* error text (internal)  */
    struct hash_control *handle,
    char *string,
    char *value);
