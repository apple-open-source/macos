/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * lists.h - the LIST structure and routines to manipulate them
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 08/23/94 (seiwald) - new list_append()
 */

/*
 * LIST - list of strings
 */

typedef struct _list LIST;

struct _list {
	LIST	*next;
	LIST	*tail;		/* only valid in head node */
	char	*string;	/* private copy */
} ;

typedef struct _lol LOL;

# define LOL_MAX 9

struct _lol {
	int	count;
	LIST	*list[ LOL_MAX ];
} ;

LIST	*list_append();
LIST 	*list_copy();
LIST 	*list_new();
void	list_free();
void	list_print();
LIST	*list_sublist();

# define list_next( l ) ((l)->next)

# define L0 ((LIST *)0)

void	lol_init();
void	lol_add();
void	lol_free();
LIST	*lol_get();
void	lol_print();
